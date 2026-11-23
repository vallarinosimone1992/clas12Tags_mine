// Native straight-fiber transmission benchmark, using production material helpers.
// A single photon/WLS lineage per event; virtual planes count its FIRST forward
// fiber crossing, exactly as separate absorbing detectors would. No timing cut.
#include "mine_geant4_optics.h"
#include "mine_wls_spectrum.h"
#include "mine_fiber_loss.h"
#include "G4Box.hh"
#include "G4Tubs.hh"
#include "G4EmStandardPhysics.hh"
#include "G4Event.hh"
#include "G4LogicalVolume.hh"
#include "G4Material.hh"
#include "G4MaterialPropertiesTable.hh"
#include "G4NistManager.hh"
#include "G4OpticalPhoton.hh"
#include "G4OpticalPhysics.hh"
#include "G4OpticalParameters.hh"
#include "G4ParticleGun.hh"
#include "G4PhysicalConstants.hh"
#include "G4PVPlacement.hh"
#include "G4RunManager.hh"
#include "G4Step.hh"
#include "G4SystemOfUnits.hh"
#include "G4UserEventAction.hh"
#include "G4UserSteppingAction.hh"
#include "G4VModularPhysicsList.hh"
#include "G4VUserDetectorConstruction.hh"
#include "G4VUserPrimaryGeneratorAction.hh"
#include "Randomize.hh"
#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
namespace {
constexpr double pi=3.14159265358979323846;
constexpr std::array<double,10> planes{{100,250,500,1000,1500,2000,3000,4000,5000,6000}};
class Detector final:public G4VUserDetectorConstruction {
public:
 mine::Geant4OpticsConfig config;G4VPhysicalVolume *core=nullptr,*clad=nullptr,*world=nullptr;
 explicit Detector(mine::Geant4OpticsConfig c):config(std::move(c)){}
 G4VPhysicalVolume* Construct() override {
  auto material=[&](const char* name,const char* nist,double index,int kind){
   auto* base=G4NistManager::Instance()->FindOrBuildMaterial(nist);
   auto* m=new G4Material(name,base->GetDensity(),base);auto* t=new G4MaterialPropertiesTable;
   G4double es[]={1*eV,6*eV},ns_[]={index,index};t->AddProperty("RINDEX",es,ns_,2);
   if(kind>=0){mine::ConfigureGeant4FiberAbsorption(t,config,kind==1);if(kind==1)mine::ConfigureGeant4WLS(t,config);}
   m->SetMaterialPropertiesTable(t);return m;
  };
  auto* air=material("bench_air","G4_AIR",1.000277,-1);
  auto* cm=material("bench_core","G4_POLYSTYRENE",config.nCore,1);
  auto* cl=material("bench_clad","G4_PLEXIGLASS",config.nCladding,0);
  const double outer=config.fiberDiameterMm/2,inner=outer-config.claddingFraction*config.fiberDiameterMm;
  auto* w=new G4LogicalVolume(new G4Box("world",10*mm,10*mm,6100*mm),air,"world");
  world=new G4PVPlacement(nullptr,{},w,"world",nullptr,false,0);
  auto* l=new G4LogicalVolume(new G4Tubs("clad",0,outer*mm,3001*mm,0,2*pi),cl,"clad");
  auto* c=new G4LogicalVolume(new G4Tubs("core",0,inner*mm,3001*mm,0,2*pi),cm,"core");
  clad=new G4PVPlacement(nullptr,{0,0,3001*mm},l,"clad",w,false,0);
  core=new G4PVPlacement(nullptr,{},c,"core",l,false,0);
  if(clad->CheckOverlaps(1000,0,true,3)||core->CheckOverlaps(1000,0,true,3))throw std::runtime_error("Straight-fiber overlap");
  // Verify exactly the same ordinary length in both production material tables.
  for(auto* m:{cm,cl}) {
   auto* a=m->GetMaterialPropertiesTable()->GetProperty("ABSLENGTH");
   if((config.effectiveFiberAttenuationMm>0 || mine::usesKurarayFiberLoss(config)) && (!a || a->Value(2.5*eV)<=0))throw std::runtime_error("Missing finite ordinary absorption");
  }
  if(mine::usesKurarayFiberLoss(config)) {
   auto* cp=cm->GetMaterialPropertiesTable();auto* clp=cl->GetMaterialPropertiesTable();
   for(double lambda:{500.,505.125,510.,520.,550.,600.,610.,650.,700.}) {
    const double energy=h_Planck*c_light/(lambda*nm);
    const double coreLength=cp->GetProperty("ABSLENGTH")->Value(energy)/mm;
    const double cladLength=clp->GetProperty("ABSLENGTH")->Value(energy)/mm;
    if(std::abs(coreLength/cladLength-1)>1e-12)throw std::runtime_error("Core/cladding loss mismatch");
    const double rate=1/coreLength+mm/cp->GetProperty("WLSABSLENGTH")->Value(energy);
    const double target=mine::dbPerKmToRatePerMm*mine::fiberReferenceLossDbPerKm(config.fiberModel,lambda);
    if(std::abs(rate/target-1)>.002)throw std::runtime_error("Native WLS+ordinary budget mismatch");
   }
   std::cout<<"NATIVE_LOSS_BUDGET_PASS identical_core_cladding=1 curves_counted_once=1"<<std::endl;
  }
  return world;
 }
};
class Physics final:public G4VModularPhysicsList {
public:
 Physics(){RegisterPhysics(new G4EmStandardPhysics);auto* p=G4OpticalParameters::Instance();p->SetWLSTimeProfile("exponential");
 p->SetProcessActivation("Scintillation",false);p->SetProcessActivation("Cerenkov",false);p->SetProcessActivation("OpWLS2",false);
 p->SetProcessActivation("OpRayleigh",false);p->SetProcessActivation("OpMieHG",false);RegisterPhysics(new G4OpticalPhysics);}
 void SetCuts()override{SetCutsWithDefault();}
};
class Source final:public G4VUserPrimaryGeneratorAction {
 Detector& d;G4ParticleGun gun{1};double mono;std::string launch;std::vector<double> wavelengths,cdf;
public:
 Source(Detector& det,double monochromatic,std::string pattern):d(det),mono(monochromatic),launch(std::move(pattern)){
  gun.SetParticleDefinition(G4OpticalPhoton::Definition());
  double sum=0;
  for(double lambda=300.025;lambda<700;lambda+=.05){double y=mine::wlsRelativeEmission(d.config.fiberModel,lambda);
   if(d.config.wlsEmissionConvention=="power_per_nm")y*=lambda;sum+=y;wavelengths.push_back(lambda);cdf.push_back(sum);}
  for(double& v:cdf)v/=sum;
 }
 void GeneratePrimaries(G4Event* e)override{
  double lambda=mono;
  if(lambda<=0){auto j=std::lower_bound(cdf.begin(),cdf.end(),G4UniformRand())-cdf.begin();lambda=wavelengths.at(j);}
  gun.SetParticleEnergy(h_Planck*c_light/(lambda*nm));
  const double inner=d.config.fiberDiameterMm*(.5-d.config.claddingFraction),outer=d.config.fiberDiameterMm*.5;
  const double r=launch=="cladding_axial"?std::sqrt(inner*inner+(outer*outer-inner*inner)*G4UniformRand()):inner*std::sqrt(G4UniformRand()),phi=2*pi*G4UniformRand();
  gun.SetParticlePosition({r*std::cos(phi)*mm,r*std::sin(phi)*mm,.01*mm});
  // Isotropic emission conditioned on the forward meridional guided cone.
  // Selection changes normalization, not a posteriori trapping efficiency.
  const double muMin=d.config.nCladding/d.config.nCore,mu=muMin+(1-muMin)*G4UniformRand(),az=2*pi*G4UniformRand();
  const G4ThreeVector dir=launch=="cone"?G4ThreeVector{std::sqrt(1-mu*mu)*std::cos(az),std::sqrt(1-mu*mu)*std::sin(az),mu}:G4ThreeVector{0,0,1};
  const double alpha=2*pi*G4UniformRand();const auto q=dir.orthogonal().unit();
  gun.SetParticleMomentumDirection(dir);gun.SetParticlePolarization(q*std::cos(alpha)+dir.cross(q)*std::sin(alpha));
  gun.SetParticleTime(0);gun.GeneratePrimaryVertex(e);
 }
};
struct Counts {std::array<long,10> reached{},directReached{};std::array<bool,10> seen{},directSeen{};long ordinary=0,wls=0,escaped=0;unsigned long long steps=0,maxSteps=0;};
class Events final:public G4UserEventAction {
 Counts& c;
public:explicit Events(Counts& c_):c(c_){}
 void BeginOfEventAction(const G4Event*)override{c.seen.fill(false);c.directSeen.fill(false);}
 void EndOfEventAction(const G4Event* e)override{if((e->GetEventID()+1)%1000==0)std::cout<<"ATTENUATION_PROGRESS events="<<e->GetEventID()+1<<std::endl;}
};
class Steps final:public G4UserSteppingAction {
 Detector& d;Counts& c;
public:Steps(Detector& det,Counts& count):d(det),c(count){}
 void UserSteppingAction(const G4Step* s)override{
  auto* t=s->GetTrack();mine::SynchronizeOpticalVelocity(s);++c.steps;c.maxSteps=std::max(c.maxSteps,static_cast<unsigned long long>(t->GetCurrentStepNumber()));
  if(t->GetCurrentStepNumber()>100000000)throw std::runtime_error("Numerical watchdog: step count exceeded; benchmark FAILS, photon is not silently censored");
  auto* pre=s->GetPreStepPoint();auto* post=s->GetPostStepPoint();
  if(pre->GetPhysicalVolume()==d.core || pre->GetPhysicalVolume()==d.clad){
   const double z0=pre->GetPosition().z()/mm,z1=post->GetPosition().z()/mm;
   for(std::size_t i=0;i<planes.size();++i)if(z0<planes[i] && z1>=planes[i]) {
    if(!c.seen[i]){c.seen[i]=true;++c.reached[i];}
    if(t->GetParentID()==0 && !c.directSeen[i]){c.directSeen[i]=true;++c.directReached[i];}
   }
  }
  const auto* proc=post->GetProcessDefinedStep();
  if(proc && proc->GetProcessName()=="OpAbsorption")++c.ordinary;
  if(proc && proc->GetProcessName()=="OpWLS")++c.wls;
  if(post->GetPhysicalVolume()==d.world || !post->GetPhysicalVolume()){++c.escaped;t->SetTrackStatus(fStopAndKill);}
 }
};
}
int main(int argc,char** argv){try{
 if(argc<5||argc>7)throw std::invalid_argument("Usage: straight-attenuation CONFIG N SEED OUTPUT.csv [monochromatic_nm=0 [cone|axial|cladding_axial]]");
 auto config=mine::Geant4OpticsConfig::Load(argv[1]);const int n=std::stoi(argv[2]);if(n<100)throw std::invalid_argument("N must be >=100");
 if(std::ifstream(argv[4]).good())throw std::runtime_error("Refusing to overwrite benchmark");
 const double mono=argc>=6?std::stod(argv[5]):0;
 const std::string launch=argc>=7?argv[6]:"cone";
 if(launch!="cone" && launch!="axial" && launch!="cladding_axial")throw std::invalid_argument("Invalid launch pattern");
 G4Random::setTheSeed(std::stol(argv[3]));
 auto rm=std::make_unique<G4RunManager>();auto* d=new Detector(config);Counts c;
 rm->SetUserInitialization(d);rm->SetUserInitialization(new Physics);rm->SetUserAction(new Source(*d,mono,launch));rm->SetUserAction(new Steps(*d,c));rm->SetUserAction(new Events(c));rm->Initialize();rm->BeamOn(n);
 if(c.ordinary+c.escaped!=n)throw std::runtime_error("Photon lineage accounting failure");
 std::ofstream f(argv[4]);f<<"distance_mm,generated,first_fiber_crossings,unshifted_primary_crossings\n";for(std::size_t i=0;i<planes.size();++i)f<<planes[i]<<','<<n<<','<<c.reached[i]<<','<<c.directReached[i]<<'\n';
 if(!f)throw std::runtime_error("Cannot save benchmark CSV");
 std::cout<<"ATTENUATION_DONE fiber="<<config.fiberModel<<" launch="<<launch<<" wavelength_nm="<<mono<<" fiber_loss_model="<<config.fiberLossModel<<" ordinary_path_mm="<<config.effectiveFiberAttenuationMm<<" n="<<n<<" wls="<<c.wls<<" ordinary_absorbed="<<c.ordinary<<" escaped="<<c.escaped<<" steps="<<c.steps<<" max_steps="<<c.maxSteps<<std::endl;
 return 0;
}catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<std::endl;return 1;}}
