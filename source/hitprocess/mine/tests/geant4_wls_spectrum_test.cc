// Isolate the production WLS properties in a matched-index 0.5 mm slab.
// Record/kill secondaries only in this test: no boundaries or trapping in the
// absorption denominator; production transport is unaffected.
#include "mine_geant4_optics.h"
#include "mine_wls_spectrum.h"
#include "mine_fiber_loss.h"
#include "G4Box.hh"
#include "G4EmStandardPhysics.hh"
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
#include "G4UserSteppingAction.hh"
#include "G4VModularPhysicsList.hh"
#include "G4VUserDetectorConstruction.hh"
#include "G4VUserPrimaryGeneratorAction.hh"
#include "Randomize.hh"
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
namespace {
void require(bool pass,const char* message) {if(!pass) throw std::runtime_error(message);}
class Detector final:public G4VUserDetectorConstruction {
public:
    mine::Geant4OpticsConfig config;
    G4VPhysicalVolume* slab=nullptr;
    G4MaterialPropertiesTable* properties=nullptr;
    explicit Detector(mine::Geant4OpticsConfig c):config(std::move(c)) {}
    G4VPhysicalVolume* Construct() override {
        auto* base=G4NistManager::Instance()->FindOrBuildMaterial("G4_POLYSTYRENE");
        auto make=[&](const char* name,bool wls) {
            auto* material=new G4Material(name,base->GetDensity(),base);
            auto* table=new G4MaterialPropertiesTable;
            G4double energies[]={1*eV,6*eV},indices[]={1.59,1.59};
            table->AddProperty("RINDEX",energies,indices,2);
            if(wls) {mine::ConfigureGeant4WLS(table,config);properties=table;}
            material->SetMaterialPropertiesTable(table);return material;
        };
        auto* worldLV=new G4LogicalVolume(new G4Box("world",10*mm,10*mm,10*mm),make("external",false),"world");
        auto* world=new G4PVPlacement(nullptr,{},worldLV,"world",nullptr,false,0);
        auto* slabLV=new G4LogicalVolume(new G4Box("slab",5*mm,5*mm,.25*mm),make("Y11",true),"slab");
        slab=new G4PVPlacement(nullptr,{},slabLV,"slab",worldLV,false,0);
        return world;
    }
};
class Physics final:public G4VModularPhysicsList {
public:
    Physics() {
        RegisterPhysics(new G4EmStandardPhysics);
        auto* parameters=G4OpticalParameters::Instance();
        parameters->SetWLSTimeProfile("exponential");
        parameters->SetProcessActivation("Scintillation",false);
        parameters->SetProcessActivation("Cerenkov",false);
        parameters->SetProcessActivation("OpWLS2",false);
        RegisterPhysics(new G4OpticalPhysics);
    }
};
class Source final:public G4VUserPrimaryGeneratorAction {
    G4ParticleGun gun{1};
public:
    double wavelength=430;
    Source() {gun.SetParticleDefinition(G4OpticalPhoton::Definition());}
    void GeneratePrimaries(G4Event* event) override {
        gun.SetParticleEnergy(h_Planck*c_light/(wavelength*nm));
        gun.SetParticlePosition({0,0,(-.25+1e-6)*mm});
        gun.SetParticleMomentumDirection({0,0,1});gun.SetParticlePolarization({1,0,0});
        gun.GeneratePrimaryVertex(event);
    }
};
class Steps final:public G4UserSteppingAction {
    Detector& detector;
public:
    int absorbed=0,children=0;double sumLambda=0,sumDelay=0,minLambda=1e9,maxLambda=0;
    double inputLambda=430;
    explicit Steps(Detector& d):detector(d) {}
    void reset(double lambda) {absorbed=children=0;sumLambda=sumDelay=maxLambda=0;minLambda=1e9;inputLambda=lambda;}
    void UserSteppingAction(const G4Step* step) override {
        auto* track=step->GetTrack();
        if(track->GetParentID()!=0) {track->SetTrackStatus(fStopAndKill);return;}
        const auto* process=step->GetPostStepPoint()->GetProcessDefinedStep();
        if(process && process->GetProcessName()=="OpWLS") {
            ++absorbed;
            for(const auto* child:*step->GetSecondaryInCurrentStep()) {
                const double lambda=h_Planck*c_light/child->GetKineticEnergy()/nm;
                require(lambda>=inputLambda-1e-6,"Anti-Stokes secondary in WLS test");
                ++children;sumLambda+=lambda;minLambda=std::min(minLambda,lambda);maxLambda=std::max(maxLambda,lambda);
                sumDelay+=(child->GetGlobalTime()-step->GetPostStepPoint()->GetGlobalTime())/ns;
            }
        }
        if(step->GetPostStepPoint()->GetPhysicalVolume()!=detector.slab) track->SetTrackStatus(fStopAndKill);
    }
};
std::pair<double,double> expectedLambdaMoments(const std::string& fiber,double lower,bool power) {
    double norm=0,moment=0,second=0;
    constexpr double dx=.01;
    for(double lambda=std::max(lower,300.)+dx/2;
        lambda<700;lambda+=dx) {
        double density=mine::wlsRelativeEmission(fiber,lambda);
        if(power) density*=lambda;
        norm+=density;moment+=density*lambda;second+=density*lambda*lambda;
    }
    const double mean=moment/norm;return {mean,second/norm-mean*mean};
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"usage: geant4-wls-spectrum-test config.conf");
        auto config=mine::Geant4OpticsConfig::Load(argv[1]);
        constexpr int photons=8000;
        auto run=std::make_unique<G4RunManager>();G4Random::setTheSeed(314159);
        auto* detector=new Detector(config);run->SetUserInitialization(detector);run->SetUserInitialization(new Physics);
        auto* source=new Source;auto* steps=new Steps(*detector);
        run->SetUserAction(source);run->SetUserAction(steps);run->Initialize();
        for(double lambda:{430.,480.,500.}) {
            auto* absorption=detector->properties->GetProperty("WLSABSLENGTH");
            const double length=absorption->Value(h_Planck*c_light/(lambda*nm))/mm;
            if(mine::isSpectralWlsModel(config.model)) {
                const double reference=1/mine::fiberWlsRatePerMm(config,lambda);
                require(std::abs(length/reference-1)<.005,"Native material absorption does not match spectral source");
            }
            const double p=-std::expm1(-(.5-1e-6)/length);
            source->wavelength=lambda;steps->reset(lambda);run->BeamOn(photons);
            const double observed=double(steps->absorbed)/photons;
            require(std::abs(observed-p)<6*std::sqrt(p*(1-p)/photons)+.001,"Absorption differs from Beer-Lambert prediction");
            require(steps->children==steps->absorbed,"WLS must emit one secondary at unity QY");
            std::cout<<"SPECTRAL_SLAB lambda_nm="<<lambda<<" L_mm="<<length<<" expectedP="<<p<<" observedP="<<observed;
            if(steps->children) {
                const double meanLambda=steps->sumLambda/steps->children;
                const double meanDelay=steps->sumDelay/steps->children;
                if(mine::isSpectralWlsModel(config.model)) {
                    require(steps->minLambda>=std::max(lambda,300.)-1e-4 &&
                            steps->maxLambda<=700+1e-4,"Emission outside source support");
                    const auto moments=expectedLambdaMoments(config.fiberModel,lambda,config.wlsEmissionConvention=="power_per_nm");
                    require(std::abs(meanLambda-moments.first)<6*std::sqrt(moments.second/steps->children)+.1,"Incorrect emission density/Jacobian");
                } else require(std::abs(meanLambda-config.wlsWavelengthNm)<1e-4,"Legacy emission changed");
                require(std::abs(meanDelay-config.wlsDecayNs)<6*config.wlsDecayNs/std::sqrt(steps->children)+.02,"Incorrect WLS exponential delay");
                std::cout<<" meanLambda_nm="<<meanLambda<<" range_nm="<<steps->minLambda<<','<<steps->maxLambda<<" delay_ns="<<meanDelay;
            }
            std::cout<<'\n';
        }
        std::cout<<"Native Geant4 WLS spectrum checks passed: "<<config.model<<'\n';
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
