// Small deterministic, already-emitted-photon benchmark. This executable is
// independent of GEMC/HIPO and uses the same smooth solids as the panel.
// Usage: benchmark straight|bend N seed output.csv [radius_mm=100 angle_deg=90]
// Straight reference has the same TOTAL centerline length and source position
// (input-lead midpoint) as the bend. No WLS delay, PDE, absorption or imposed
// trapping. Side escape is terminated explicitly, matching the isolated MC.
#include "mine_geant4_optics.h"
#include "G4Box.hh"
#include "G4Event.hh"
#include "G4EmStandardPhysics.hh"
#include "G4LogicalSkinSurface.hh"
#include "G4LogicalVolume.hh"
#include "G4Material.hh"
#include "G4MaterialPropertiesTable.hh"
#include "G4NistManager.hh"
#include "G4OpticalPhoton.hh"
#include "G4OpticalPhysics.hh"
#include "G4OpticalSurface.hh"
#include "G4ParticleGun.hh"
#include "G4PhysicalConstants.hh"
#include "G4PVPlacement.hh"
#include "G4RunManager.hh"
#include "G4RotationMatrix.hh"
#include "G4Step.hh"
#include "G4StepPoint.hh"
#include "G4SystemOfUnits.hh"
#include "G4Track.hh"
#include "G4Transform3D.hh"
#include "G4UserSteppingAction.hh"
#include "G4VModularPhysicsList.hh"
#include "G4VUserDetectorConstruction.hh"
#include "G4VUserPrimaryGeneratorAction.hh"
#include "Randomize.hh"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
constexpr double kPi=3.14159265358979323846;
G4ThreeVector point(const mine::OpticalPoint& p) {return {p.x*mm,p.y*mm,p.z*mm};}
G4Material* material(const std::string& name,const char* nist,double index) {
    auto* source=G4NistManager::Instance()->FindOrBuildMaterial(nist);
    auto* result=new G4Material(name,source->GetDensity(),source);
    auto* table=new G4MaterialPropertiesTable;
    G4double energy[]={1*eV,6*eV},values[]={index,index};table->AddProperty("RINDEX",energy,values,2);
    result->SetMaterialPropertiesTable(table);return result;
}
class Detector final:public G4VUserDetectorConstruction {
public:
    mine::OpticalFiberRoute route;
    std::array<mine::OpticalReadoutPlane,2> planes;
    G4VPhysicalVolume* core=nullptr;G4VPhysicalVolume* cladding=nullptr;G4VPhysicalVolume* outside=nullptr;
    std::array<G4VPhysicalVolume*,2> sensor{{nullptr,nullptr}};
    Detector(bool bend,double radius,double angle) {
        const double arc=radius*angle;
        route.start={0,0,0};
        if(bend) {
            route.tangent0={0,50,0};route.center={radius,50,0};
            route.tangent1={radius-radius*std::cos(angle),50+radius*std::sin(angle),0};
            route.end=route.tangent1+mine::OpticalPoint{std::sin(angle),std::cos(angle),0}*50;
            route.radiusMm=radius;route.startAngleRad=kPi;route.deltaAngleRad=-angle;
        } else {route.end={0,100+arc,0};route.straight=true;}
        const auto endAxis=bend?(route.end-route.tangent1).unit():mine::OpticalPoint{0,1,0};
        planes={mine::OpticalReadoutPlane{route.start,{0,-1,0},{1,0,0},{0,0,1},2,2},
                mine::OpticalReadoutPlane{route.end,endAxis,{-endAxis.y,endAxis.x,0},{0,0,1},2,2}};
    }
    G4VPhysicalVolume* Construct() override {
        auto* outerMaterial=material("benchmark_external","G4_PLASTIC_SC_VINYLTOLUENE",1.58);
        auto* coreMaterial=material("benchmark_core","G4_POLYSTYRENE",1.59);
        auto* cladMaterial=material("benchmark_clad","G4_PLEXIGLASS",1.49);
        const double extent=std::max(1000.,3*route.LengthMm());
        auto* world=new G4LogicalVolume(new G4Box("world",extent*mm,extent*mm,extent*mm),outerMaterial,"world");
        outside=new G4PVPlacement(nullptr,{},world,"world",nullptr,false,0);
        auto* claddingLV=new G4LogicalVolume(mine::BuildOpticalFiberSolid("cladding",route,.5,planes),cladMaterial,"cladding");
        auto* coreLV=new G4LogicalVolume(mine::BuildOpticalFiberSolid("core",route,.48,planes),coreMaterial,"core");
        cladding=new G4PVPlacement(nullptr,{},claddingLV,"cladding",world,false,0);
        core=new G4PVPlacement(nullptr,{},coreLV,"core",claddingLV,false,0);
        for(int s=0;s<2;++s) {
            const auto normal=point(planes[s].normal),width=point(planes[s].widthAxis);
            G4RotationMatrix basis(width,normal.cross(width),normal);
            auto* shape=new G4Box("sensor",1*mm,1*mm,.005*mm);
            auto* lv=new G4LogicalVolume(shape,outerMaterial,"sensor");
            sensor[s]=new G4PVPlacement(G4Transform3D(basis,point(planes[s].center+planes[s].normal*.005)),lv,"sensor",world,false,s);
            auto* opticalSurface=new G4OpticalSurface("absorber",unified,polished,dielectric_metal);
            auto* properties=new G4MaterialPropertiesTable;
            G4double energies[]={1*eV,6*eV},zero[]={0,0};
            properties->AddProperty("REFLECTIVITY",energies,zero,2);properties->AddProperty("EFFICIENCY",energies,zero,2);
            opticalSurface->SetMaterialPropertiesTable(properties);new G4LogicalSkinSurface("sensor",lv,opticalSurface);
        }
        bool bad=cladding->CheckOverlaps(5000,0,true,5)||core->CheckOverlaps(5000,0,true,5);
        for(auto* pv:sensor) bad=pv->CheckOverlaps(5000,0,true,5)||bad;
        if(bad) throw std::runtime_error("benchmark geometry overlaps");
        return outside;
    }
};
class Physics final:public G4VModularPhysicsList {
public:
    Physics() {RegisterPhysics(new G4EmStandardPhysics);RegisterPhysics(new G4OpticalPhysics);}
    void SetCuts() override {SetCutsWithDefault();}
};
class Source final:public G4VUserPrimaryGeneratorAction {
    G4ParticleGun gun{1};std::mt19937_64 rng;std::uniform_real_distribution<double> uniform{0,1};
    struct Launch {G4ThreeVector position,direction,polarization;};
    std::vector<Launch> replay;
public:
    explicit Source(unsigned long seed,int photons):rng(seed) {
        gun.SetParticleDefinition(G4OpticalPhoton::Definition());
        gun.SetParticleEnergy(h_Planck*c_light/(476*nm));gun.SetParticleTime(0);
        if(const char* path=std::getenv("MINE_FIBER_REPLAY")) {
            std::ifstream input(path);std::string line;
            if(!input || !std::getline(input,line) || line!="x_mm,y_mm,z_mm,dx,dy,dz,px,py,pz")
                throw std::runtime_error("invalid replay source header");
            while(std::getline(input,line)) {
                if(line.empty()) continue;
                std::replace(line.begin(),line.end(),',',' ');std::istringstream row(line);
                std::array<double,9> values{};
                for(double& value:values) if(!(row>>value) || !std::isfinite(value))
                    throw std::runtime_error("invalid replay source value");
                row>>std::ws;if(!row.eof()) throw std::runtime_error("extra replay source columns");
                Launch launch{{values[0]*mm,values[1]*mm,values[2]*mm},
                              {values[3],values[4],values[5]},{values[6],values[7],values[8]}};
                if(std::abs(launch.direction.mag()-1)>1e-9 || std::abs(launch.polarization.mag()-1)>1e-9 ||
                   std::abs(launch.direction.dot(launch.polarization))>1e-9)
                    throw std::runtime_error("replay direction/polarization must be unit and transverse");
                replay.push_back(launch);
            }
            if(replay.size()!=static_cast<std::size_t>(photons)) throw std::runtime_error("replay row count must equal requested photons");
            std::cout<<"Replaying explicit launch states from "<<path<<std::endl;
        }
    }
    void GeneratePrimaries(G4Event* event) override {
        if(!replay.empty()) {
            const auto& launch=replay.at(event->GetEventID());
            gun.SetParticlePosition(launch.position);gun.SetParticleMomentumDirection(launch.direction);
            gun.SetParticlePolarization(launch.polarization);gun.GeneratePrimaryVertex(event);return;
        }
        const double r=.48*std::sqrt(uniform(rng)),phi=2*kPi*uniform(rng);
        gun.SetParticlePosition({r*std::cos(phi)*mm,25*mm,r*std::sin(phi)*mm});
        const double cosine=2*uniform(rng)-1,azimuth=2*kPi*uniform(rng),sine=std::sqrt(1-cosine*cosine);
        const G4ThreeVector direction{sine*std::cos(azimuth),cosine,sine*std::sin(azimuth)};
        gun.SetParticleMomentumDirection(direction);
        const auto perpendicular=direction.orthogonal().unit();const double alpha=2*kPi*uniform(rng);
        gun.SetParticlePolarization(perpendicular*std::cos(alpha)+direction.cross(perpendicular)*std::sin(alpha));
        gun.GeneratePrimaryVertex(event);
    }
};
class Stepping final:public G4UserSteppingAction {
    Detector& detector;std::ostream& output;
    double opticalTime=0;int previousEvent=-1;
public:
    std::array<long,2> counts{{0,0}};long side=0,miss=0;double maxTimeError=0;
    unsigned long long totalSteps=0,maxStepsPerPhoton=0;
    Stepping(Detector& d,std::ostream& o):detector(d),output(o) {output<<"event,sensor,path_mm,time_ns,wavelength_nm,core_path_mm,cladding_path_mm\n";}
    double corePath=0,cladPath=0;
    void UserSteppingAction(const G4Step* step) override {
        const int event=G4RunManager::GetRunManager()->GetCurrentEvent()->GetEventID();
        if(event!=previousEvent) {opticalTime=0;corePath=0;cladPath=0;previousEvent=event;}
        auto* track=step->GetTrack();auto* before=step->GetPreStepPoint()->GetPhysicalVolume();auto* after=step->GetPostStepPoint()->GetPhysicalVolume();
        ++totalSteps;maxStepsPerPhoton=std::max(maxStepsPerPhoton,static_cast<unsigned long long>(track->GetCurrentStepNumber()));
        mine::SynchronizeOpticalVelocity(step);
        const double length=step->GetStepLength();
        static const int traceEvent=[] {
            const char* value=std::getenv("MINE_FIBER_TRACE_EVENT");return value?std::stoi(value):-1;
        }();
        if(event==traceEvent) {
            const auto* pre=step->GetPreStepPoint();const auto* post=step->GetPostStepPoint();
            std::cerr<<std::setprecision(17)<<"TRACE event="<<event<<" step="<<track->GetCurrentStepNumber()
                     <<" pre="<<(before?before->GetName():"null")<<" post="<<(after?after->GetName():"null")
                     <<" material="<<pre->GetMaterial()->GetName()<<" length_mm="<<length/mm
                     <<" pre_time="<<pre->GetGlobalTime()/ns<<" post_time="<<post->GetGlobalTime()/ns
                     <<" pre_v_c="<<pre->GetVelocity()/c_light<<" post_v_c="<<post->GetVelocity()/c_light
                     <<" track_v_c="<<track->GetVelocity()/c_light<<'\n';
        }
        if(before==detector.core) {corePath+=length;opticalTime+=1.59*length/c_light;}
        else if(before==detector.cladding) {cladPath+=length;opticalTime+=1.49*length/c_light;}
        for(int s=0;s<2;++s) if(after==detector.sensor[s]) {
            if(before==detector.core) {
                ++counts[s];maxTimeError=std::max(maxTimeError,std::abs(track->GetGlobalTime()-opticalTime));
                output<<event<<','<<s<<','<<track->GetTrackLength()/mm<<','<<track->GetGlobalTime()/ns<<','
                      <<h_Planck*c_light/step->GetPreStepPoint()->GetKineticEnergy()/nm<<','<<corePath/mm<<','<<cladPath/mm<<'\n';
            } else ++miss;
            track->SetTrackStatus(fStopAndKill);return;
        }
        if(after==detector.outside || !after) {++side;track->SetTrackStatus(fStopAndKill);}
    }
};
}
int main(int argc,char** argv) {
    if(argc<5 || argc>7) {std::cerr<<"usage: benchmark straight|bend N seed output.csv [radius_mm angle_deg]\n";return 2;}
    const std::string mode=argv[1];const int photons=std::stoi(argv[2]);const auto seed=std::stoul(argv[3]);
    const double radius=argc>5?std::stod(argv[5]):100,angle=(argc>6?std::stod(argv[6]):90)*kPi/180;
    if((mode!="straight" && mode!="bend") || photons<1 || radius<100 || angle<=0 || angle>kPi)
        throw std::invalid_argument("invalid benchmark parameters");
    // Refuse to overwrite previous validation data.
    {std::ifstream existing(argv[4]);if(existing) throw std::runtime_error("benchmark output already exists");}
    std::ofstream output(argv[4]);if(!output) throw std::runtime_error("cannot create benchmark output");
    output<<std::setprecision(17);G4Random::setTheSeed(seed);
    auto manager=std::make_unique<G4RunManager>();auto* detector=new Detector(mode=="bend",radius,angle);
    manager->SetUserInitialization(detector);manager->SetUserInitialization(new Physics);
    manager->SetUserAction(new Source(seed,photons));auto* stepping=new Stepping(*detector,output);manager->SetUserAction(stepping);
    manager->Initialize();manager->BeamOn(photons);
    std::cout<<"benchmark "<<mode<<" generated="<<photons<<" length_mm="<<detector->route.LengthMm()
             <<" sensor0="<<stepping->counts[0]<<" sensor1="<<stepping->counts[1]
             <<" side="<<stepping->side<<" readout_miss="<<stepping->miss
             <<" steps="<<stepping->totalSteps<<" max_steps_per_photon="<<stepping->maxStepsPerPhoton
             <<" max_time_minus_integral_n_ds_c_ns="<<stepping->maxTimeError/ns<<std::endl;
    if(stepping->counts[0]+stepping->counts[1]+stepping->side+stepping->miss!=photons || stepping->maxTimeError>1e-8*ns)
        throw std::runtime_error("benchmark accounting or propagation-time failure");
}
