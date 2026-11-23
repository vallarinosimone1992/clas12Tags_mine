// Generation-only probe: a single muon deposits energy in the actual optical
// panel builder. Optical children are counted, then deliberately discarded by
// this diagnostic's stack (there is NO collection/timing prediction here).
#include "mine_geant4_optics.h"
#include "mine_scintillation_spectrum.h"
#include <algorithm>
#include <vector>
#include "G4Box.hh"
#include "G4EmSaturation.hh"
#include "G4EmStandardPhysics.hh"
#include "G4Event.hh"
#include "G4LogicalVolume.hh"
#include "G4Material.hh"
#include "G4MaterialPropertiesTable.hh"
#include "G4LossTableManager.hh"
#include "G4MuonMinus.hh"
#include "G4NistManager.hh"
#include "G4OpticalParameters.hh"
#include "G4OpticalPhoton.hh"
#include "G4OpticalPhysics.hh"
#include "G4ParticleGun.hh"
#include "G4PVPlacement.hh"
#include "G4ProcessManager.hh"
#include "G4ProcessVector.hh"
#include "G4PhysicsTable.hh"
#include "G4RunManager.hh"
#include "G4Step.hh"
#include "G4Scintillation.hh"
#include "G4SystemOfUnits.hh"
#include "G4Track.hh"
#include "G4UserStackingAction.hh"
#include "G4UserSteppingAction.hh"
#include "G4Version.hh"
#include "G4VModularPhysicsList.hh"
#include "G4VUserDetectorConstruction.hh"
#include "G4VUserPrimaryGeneratorAction.hh"
#include "G4VProcess.hh"
#include "Randomize.hh"
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
namespace {
class Detector final:public G4VUserDetectorConstruction {
    mine::Geant4OpticsConfig config;
public:
    explicit Detector(mine::Geant4OpticsConfig c):config(std::move(c)) {config.checkOverlaps=false;}
    G4VPhysicalVolume* Construct() override {
        auto* nist=G4NistManager::Instance();
        auto* worldLV=new G4LogicalVolume(new G4Box("world",1000*mm,1000*mm,1000*mm),nist->FindOrBuildMaterial("G4_AIR"),"world");
        auto* world=new G4PVPlacement(nullptr,{},worldLV,"world",nullptr,false,0);
        auto* panelLV=new G4LogicalVolume(new G4Box("panel",500*mm,500*mm,5*mm),nist->FindOrBuildMaterial("G4_PLASTIC_SC_VINYLTOLUENE"),"panel");
        auto* panel=new G4PVPlacement(nullptr,{},panelLV,"panel",worldLV,false,0);
        mine::BuildGeant4Optics(panel,world,1,config);return world;
    }
};
class Physics final:public G4VModularPhysicsList {
public:
    Physics() {
        RegisterPhysics(new G4EmStandardPhysics);RegisterPhysics(new G4OpticalPhysics);
        auto* parameters=G4OpticalParameters::Instance();
        parameters->SetProcessActivation("Cerenkov",false);parameters->SetScintByParticleType(false);
        parameters->SetScintTrackSecondariesFirst(false);parameters->SetScintStackPhotons(true);
#if G4VERSION_NUMBER < 1100
        parameters->SetScintEnhancedTimeConstants(false);parameters->SetScintYieldFactor(1);
#endif
    }
    void SetCuts() override {SetCutsWithDefault();}
};
class Source final:public G4VUserPrimaryGeneratorAction {
    G4ParticleGun gun{1};
public:
    Source() {
        auto* muon=G4MuonMinus::Definition();gun.SetParticleDefinition(muon);
        const double momentum=1*GeV,mass=muon->GetPDGMass();
        gun.SetParticleEnergy(std::sqrt(momentum*momentum+mass*mass)-mass);
        gun.SetParticlePosition({0,20*mm,-100*mm});gun.SetParticleMomentumDirection({0,0,1});
    }
    void GeneratePrimaries(G4Event* event) override {gun.GeneratePrimaryVertex(event);}
};
class Stack final:public G4UserStackingAction {
public:
    G4ClassificationOfNewTrack ClassifyNewTrack(const G4Track* track) override {
        return track->GetDefinition()==G4OpticalPhoton::Definition()?fKill:fUrgent;
    }
};
class Stepping final:public G4UserSteppingAction {
public:
    long scintillation=0;double deposited=0,visible=0;
    std::vector<double> wavelengths;
    std::map<std::string,long> opticalCreators;
    void UserSteppingAction(const G4Step* step) override {
        const auto* region=mine::FindOpticalVolume(step->GetPreStepPoint()->GetPhysicalVolume());
        if(region && region->role==mine::OpticalVolumeRole::Panel) {
            deposited+=step->GetTotalEnergyDeposit();
            visible+=G4LossTableManager::Instance()->EmSaturation()->VisibleEnergyDepositionAtAStep(step);
        }
        for(const auto* child:*step->GetSecondaryInCurrentStep()) {
            if(child->GetDefinition()!=G4OpticalPhoton::Definition()) continue;
            const auto* creator=child->GetCreatorProcess();const auto name=creator?creator->GetProcessName():"none";
            ++opticalCreators[name];if(name=="Scintillation") {
                ++scintillation;wavelengths.push_back(mine::opticalHcEvNm/(child->GetKineticEnergy()/eV));
            }
        }
    }
};
}
int main(int argc,char** argv) {
    if(argc<2 || argc>3) throw std::invalid_argument("usage: scintillation-smoke explicit-model.conf [events]");
    const int events=argc==3?std::stoi(argv[2]):1;
    if(events<1 || events>10) throw std::invalid_argument("events must be 1..10");
    auto config=mine::Geant4OpticsConfig::Load(argv[1]);G4Random::setTheSeed(571203);
    auto run=std::make_unique<G4RunManager>();run->SetUserInitialization(new Detector(config));run->SetUserInitialization(new Physics);
    run->SetUserAction(new Source);run->SetUserAction(new Stack);auto* stepping=new Stepping;run->SetUserAction(stepping);
    run->Initialize();
    G4OpticalParameters::Instance()->Dump();
    auto* processes=G4MuonMinus::Definition()->GetProcessManager()->GetProcessList();
    for(std::size_t i=0;i<processes->size();++i) {
        std::cout<<"MU_PROCESS "<<(*processes)[i]->GetProcessName()<<std::endl;
        if(auto* scint=dynamic_cast<G4Scintillation*>((*processes)[i])) {
            std::cout<<"SCINT_PROCESS stack="<<scint->GetStackPhotons()<<" yield="<<scint->GetScintillationYieldFactor()
                     <<" enhanced="<<scint->GetEnhancedTimeConstants()<<std::endl;
            auto* table=scint->GetFastIntegralTable();
            std::cout<<"FAST_INTEGRAL_TABLE entries="<<(table?table->size():0)<<std::endl;
        }
    }
    for(const auto* material:*G4Material::GetMaterialTable()) {
        if(material->GetName().find("MINE_optical_")!=0) continue;
        auto* properties=material->GetMaterialPropertiesTable();
        std::cout<<"OPTICAL_MATERIAL "<<material->GetName()<<" index="<<material->GetIndex()<<" table="<<properties<<std::endl;
        if(properties && config.scintillationSpectrum=="monochromatic") properties->DumpTable();
    }
    run->BeamOn(events);
    std::cout<<"GENERATION_ONLY_SMOKE Edep_MeV="<<stepping->deposited/MeV<<" Evis_MeV="<<stepping->visible/MeV
             <<" expected="<<stepping->visible/MeV*config.lightYieldPerMeV<<" generated="<<stepping->scintillation<<std::endl;
    for(const auto& entry:stepping->opticalCreators) std::cout<<entry.first<<"="<<entry.second<<std::endl;
    if(config.scintillationSpectrum!="monochromatic") {
        // Compare actual native Scintillation photons with an independent
        // wavelength-domain CDF, not the energy table passed to Geant4.
        std::vector<double> cumulative;double sum=0;
        constexpr double step=.01;
        for(double lambda=300+step/2;lambda<600;lambda+=step) {
            sum+=mine::scintillationPerNm(config.scintillationSpectrum,lambda)
                *(config.scintillationEmissionConvention=="power_per_nm"?lambda/425.:1.)*step;
            cumulative.push_back(sum);
        }
        std::sort(stepping->wavelengths.begin(),stepping->wavelengths.end());
        double ks=0,mean=0;const auto count=stepping->wavelengths.size();
        for(std::size_t i=0;i<count;++i) {
            const double lambda=stepping->wavelengths[i];mean+=lambda;
            const auto bin=static_cast<long>((lambda-300)/step);
            const double cdf=bin<0?0:bin>=static_cast<long>(cumulative.size())?1:cumulative[bin]/sum;
            ks=std::max(ks,std::max(std::abs(cdf-double(i)/count),std::abs(cdf-double(i+1)/count)));
        }
        std::cout<<"SCINT_NATIVE profile="<<config.scintillationSpectrum<<" photons="<<count
                 <<" mean_nm="<<mean/count<<" KS="<<ks<<std::endl;
        if(count<1000 || ks>5/std::sqrt(double(count))) throw std::runtime_error("Native scintillation spectrum differs from wavelength CDF");
    }
    if(stepping->visible<=0 || stepping->scintillation<=0) throw std::runtime_error("scintillation generation failed");
}
