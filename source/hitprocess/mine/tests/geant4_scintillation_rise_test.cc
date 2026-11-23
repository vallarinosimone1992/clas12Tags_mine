// Generation-only native Geant4 timing-law test. Optical children are inspected
// at creation and discarded before transport. It does not predict collection.
#include "mine_optical_geometry_model.h"
#include "mine_scintillation_spectrum.h"
#include "G4Box.hh"
#include "G4EmStandardPhysics.hh"
#include "G4IonisParamMat.hh"
#include "G4LogicalVolume.hh"
#include "G4Material.hh"
#include "G4MaterialPropertiesTable.hh"
#include "G4MuonMinus.hh"
#include "G4NistManager.hh"
#include "G4OpticalParameters.hh"
#include "G4OpticalPhoton.hh"
#include "G4OpticalPhysics.hh"
#include "G4ParticleGun.hh"
#include "G4PVPlacement.hh"
#include "G4RunManager.hh"
#include "G4Step.hh"
#include "G4SystemOfUnits.hh"
#include "G4Track.hh"
#include "G4VProcess.hh"
#include "G4UserStackingAction.hh"
#include "G4UserSteppingAction.hh"
#include "G4Version.hh"
#include "G4VModularPhysicsList.hh"
#include "G4VUserDetectorConstruction.hh"
#include "G4VUserPrimaryGeneratorAction.hh"
#include "Randomize.hh"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
namespace {
class Detector final:public G4VUserDetectorConstruction {
    mine::Geant4OpticsConfig c;
public:
    explicit Detector(mine::Geant4OpticsConfig config):c(std::move(config)){}
    G4VPhysicalVolume* Construct() override {
        auto* nist=G4NistManager::Instance();
        auto* air=nist->FindOrBuildMaterial("G4_AIR");
        auto* worldLV=new G4LogicalVolume(new G4Box("world",100*mm,100*mm,100*mm),air,"world");
        auto* world=new G4PVPlacement(nullptr,{},worldLV,"world",nullptr,false,0);
        auto* base=nist->FindOrBuildMaterial("G4_PLASTIC_SC_VINYLTOLUENE");
        auto* plastic=new G4Material("EJ200_generation_test",1.023*g/cm3,base);
        auto* table=new G4MaterialPropertiesTable;
        G4double energies[]={1*eV,6*eV},indices[]={c.nScintillator,c.nScintillator};
        table->AddProperty("RINDEX",energies,indices,2);
        auto spectrum=mine::scintillationPropertyVector(c.scintillationSpectrum,c.scintillationEmissionConvention=="power_per_nm");
        for(auto& e:spectrum.energyEv)e*=eV;
#if G4VERSION_NUMBER >= 1100
        table->AddProperty("SCINTILLATIONCOMPONENT1",spectrum.energyEv.data(),spectrum.emissionPerEV.data(),spectrum.energyEv.size());
        table->AddConstProperty("SCINTILLATIONTIMECONSTANT1",c.scintillationDecayNs*ns);
        table->AddConstProperty("SCINTILLATIONRISETIME1",c.scintillationRiseNs*ns);
        table->AddConstProperty("SCINTILLATIONYIELD1",1.);
#else
        table->AddProperty("FASTCOMPONENT",spectrum.energyEv.data(),spectrum.emissionPerEV.data(),spectrum.energyEv.size());
        table->AddConstProperty("FASTTIMECONSTANT",c.scintillationDecayNs*ns);
        table->AddConstProperty("FASTSCINTILLATIONRISETIME",c.scintillationRiseNs*ns);
        table->AddConstProperty("YIELDRATIO",1.);
#endif
        table->AddConstProperty("SCINTILLATIONYIELD",c.lightYieldPerMeV/MeV);
        table->AddConstProperty("RESOLUTIONSCALE",1.);
        plastic->SetMaterialPropertiesTable(table);plastic->GetIonisation()->SetBirksConstant(c.birksMmPerMeV*mm/MeV);
        auto* lv=new G4LogicalVolume(new G4Box("scintillator",50*mm,50*mm,4*mm),plastic,"scintillator");
        new G4PVPlacement(nullptr,{},lv,"scintillator",worldLV,false,0);return world;
    }
};
class Physics final:public G4VModularPhysicsList {
public:
    explicit Physics(double rise) {
        RegisterPhysics(new G4EmStandardPhysics);auto* p=G4OpticalParameters::Instance();
        p->SetProcessActivation("Cerenkov",false);p->SetScintByParticleType(false);
        p->SetScintTrackSecondariesFirst(false);p->SetScintStackPhotons(true);
        p->SetScintFiniteRiseTime(rise>0);
#if G4VERSION_NUMBER < 1100
        p->SetScintEnhancedTimeConstants(false);p->SetScintYieldFactor(1);
#endif
        RegisterPhysics(new G4OpticalPhysics);
    }
    void SetCuts() override {SetCutsWithDefault();}
};
class Source final:public G4VUserPrimaryGeneratorAction {
    G4ParticleGun gun{1};
public:
    Source(){auto* mu=G4MuonMinus::Definition();gun.SetParticleDefinition(mu);const double m=mu->GetPDGMass();gun.SetParticleEnergy(std::sqrt(GeV*GeV+m*m)-m);gun.SetParticlePosition({0,0,-20*mm});gun.SetParticleMomentumDirection({0,0,1});}
    void GeneratePrimaries(G4Event* e)override{gun.GeneratePrimaryVertex(e);}
};
class Stack final:public G4UserStackingAction {
public:G4ClassificationOfNewTrack ClassifyNewTrack(const G4Track* t)override{return t->GetDefinition()==G4OpticalPhoton::Definition()?fKill:fUrgent;}
};
class Steps final:public G4UserSteppingAction {
public:
    std::vector<double> delays;
    void UserSteppingAction(const G4Step* s)override {
        const auto* t=s->GetTrack();
        // Restrict to energetic primary muon. This makes within-step transit
        // reconstruction unambiguous and excludes slow stopping secondaries.
        if(t->GetDefinition()!=G4MuonMinus::Definition()||t->GetParentID()!=0)return;
        const auto* pre=s->GetPreStepPoint();const auto* post=s->GetPostStepPoint();const auto dx=post->GetPosition()-pre->GetPosition();
        for(const auto* child:*s->GetSecondaryInCurrentStep()) {
            if(child->GetDefinition()!=G4OpticalPhoton::Definition()||!child->GetCreatorProcess()||child->GetCreatorProcess()->GetProcessName()!="Scintillation")continue;
            const double u=dx.mag2()>0?std::clamp((child->GetPosition()-pre->GetPosition()).dot(dx)/dx.mag2(),0.,1.):0.;
            // Native 10.7 charged-step creation time uses a linear velocity
            // estimate. Reconstruct its transit term from the birth position.
            const double velocity=pre->GetVelocity()+u*(post->GetVelocity()-pre->GetVelocity())*.5;
            const double transit=velocity>0?u*s->GetStepLength()/velocity:0;
            const double delay=(child->GetGlobalTime()-pre->GetGlobalTime()-transit)/ns;
            if(delay< -1e-8)throw std::runtime_error("Negative scintillation emission delay");
            delays.push_back(std::max(0.,delay));
        }
    }
};
}
int main(int argc,char** argv){try{
    if(argc<2||argc>3)throw std::invalid_argument("Usage: scintillation-rise-test CONFIG [events=5]");
    auto c=mine::Geant4OpticsConfig::Load(argv[1]);if(c.scintillationSpectrum=="monochromatic")throw std::invalid_argument("Use a broadband scintillator profile for this test");
    const int events=argc>2?std::stoi(argv[2]):5;if(events<1||events>20)throw std::invalid_argument("Use 1..20 events");
    G4Random::setTheSeed(20261122);auto run=std::make_unique<G4RunManager>();run->SetUserInitialization(new Detector(c));run->SetUserInitialization(new Physics(c.scintillationRiseNs));
    run->SetUserAction(new Source);run->SetUserAction(new Stack);auto* steps=new Steps;run->SetUserAction(steps);run->Initialize();run->BeamOn(events);
    auto& times=steps->delays;std::sort(times.begin(),times.end());const auto N=times.size();if(N<1000)throw std::runtime_error("Insufficient primary scintillation photons");
    const double td=c.scintillationDecayNs,tr=c.scintillationRiseNs,te=tr>0?tr*td/(tr+td):0;
    const auto cdf=[&](double t){return te>0?1-(td*std::exp(-t/td)-te*std::exp(-t/te))/(td-te):1-std::exp(-t/td);};
    double sum=0,ks=0;for(std::size_t i=0;i<N;++i){sum+=times[i];const double f=cdf(times[i]);ks=std::max(ks,std::max(std::abs(f-double(i)/N),std::abs(f-double(i+1)/N)));}
    const double mean=sum/N,expected=td+te,variance=td*td+te*te;
    if(std::abs(mean-expected)>6*std::sqrt(variance/N)+.002||ks>6/std::sqrt(double(N)))throw std::runtime_error("Native emission time law mismatch");
    std::cout<<"SCINTILLATION_RISE_PASS photons="<<N<<" rise_ns="<<tr<<" decay_ns="<<td<<" mean_ns="<<mean<<" expected_mean_ns="<<expected<<" KS="<<ks<<std::endl;return 0;
}catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<std::endl;return 1;}}
