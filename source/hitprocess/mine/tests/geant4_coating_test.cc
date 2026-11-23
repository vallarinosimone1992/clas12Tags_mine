// Native single-boundary experiment using the production coating builder.
// Check absolute survival, specular law vs Lambert cosine law, wavelength and
// time conservation. Photons are stopped after the first coated encounter only
// in this diagnostic; no production history or efficiency is truncated.
#include "mine_geant4_optics.h"
#include "mine_coating_spectrum.h"
#include "G4Box.hh"
#include "G4EmStandardPhysics.hh"
#include "G4Event.hh"
#include "G4LogicalBorderSurface.hh"
#include "G4LogicalVolume.hh"
#include "G4Material.hh"
#include "G4MaterialPropertiesTable.hh"
#include "G4NistManager.hh"
#include "G4OpBoundaryProcess.hh"
#include "G4OpticalParameters.hh"
#include "G4OpticalPhoton.hh"
#include "G4OpticalPhysics.hh"
#include "G4OpticalSurface.hh"
#include "G4ParticleGun.hh"
#include "G4PhysicalConstants.hh"
#include "G4ProcessManager.hh"
#include "G4ProcessVector.hh"
#include "G4PVPlacement.hh"
#include "G4RunManager.hh"
#include "G4Step.hh"
#include "G4SystemOfUnits.hh"
#include "G4UserSteppingAction.hh"
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
void require(bool condition,const char* message) {if(!condition) throw std::runtime_error(message);}
class Detector final:public G4VUserDetectorConstruction {
    mine::Geant4OpticsConfig config;
public:
    explicit Detector(mine::Geant4OpticsConfig c):config(std::move(c)) {}
    G4VPhysicalVolume* Construct() override {
        auto* nist=G4NistManager::Instance();
        auto material=[&](const char* name,double n) {
            auto* base=nist->FindOrBuildMaterial(name);
            auto* properties=new G4MaterialPropertiesTable;
            G4double energies[]={1*eV,6*eV},indices[]={n,n};
            properties->AddProperty("RINDEX",energies,indices,2);
            base->SetMaterialPropertiesTable(properties);return base;
        };
        auto* worldLV=new G4LogicalVolume(new G4Box("world",10*mm,10*mm,10*mm),material("G4_AIR",1),"world");
        auto* world=new G4PVPlacement(nullptr,{},worldLV,"world",nullptr,false,0);
        auto* slabLV=new G4LogicalVolume(new G4Box("slab",5*mm,5*mm,.5*mm),material("G4_POLYSTYRENE",config.nScintillator),"slab");
        auto* slab=new G4PVPlacement(nullptr,{},slabLV,"slab",worldLV,false,0);
        new G4LogicalBorderSurface("test_coating",slab,world,mine::BuildGeant4CoatingSurface("production_coating",config));
        return world;
    }
};
class Physics final:public G4VModularPhysicsList {
public:
    Physics() {
        RegisterPhysics(new G4EmStandardPhysics);
        auto* p=G4OpticalParameters::Instance();
        for(const char* name:{"Scintillation","Cerenkov","OpWLS","OpWLS2","OpAbsorption","OpRayleigh","OpMieHG"})
            p->SetProcessActivation(name,false);
        p->SetProcessActivation("OpBoundary",true);
        RegisterPhysics(new G4OpticalPhysics);
    }
};
class Source final:public G4VUserPrimaryGeneratorAction {
    G4ParticleGun gun{1};
public:
    double angle=0,wavelength=425;
    Source() {gun.SetParticleDefinition(G4OpticalPhoton::Definition());}
    void GeneratePrimaries(G4Event* event) override {
        gun.SetParticlePosition({0,0,0});
        gun.SetParticleEnergy(h_Planck*c_light/(wavelength*nm));
        gun.SetParticleMomentumDirection({std::sin(angle),0,std::cos(angle)});
        gun.SetParticlePolarization({0,1,0});gun.GeneratePrimaryVertex(event);
    }
};
class Steps final:public G4UserSteppingAction {
    const mine::Geant4OpticsConfig& config;
    G4OpBoundaryProcess* boundary=nullptr;
public:
    long reflected=0,absorbed=0;
    double sumX=0,sumY=0,sumMu=0,maxDirectionError=0,maxTimeError=0;
    std::vector<double> cosines;
    explicit Steps(const mine::Geant4OpticsConfig& c):config(c) {}
    void reset() {reflected=absorbed=0;sumX=sumY=sumMu=maxDirectionError=maxTimeError=0;cosines.clear();}
    void UserSteppingAction(const G4Step* step) override {
        auto* track=step->GetTrack();
        require(track->GetParentID()==0 && step->GetNumberOfSecondariesInCurrentStep()==0,"Coating produced a secondary photon");
        auto* post=step->GetPostStepPoint();auto* pre=step->GetPreStepPoint();
        require(post->GetStepStatus()==fGeomBoundary,"Unexpected non-boundary optical step");
        require(std::abs(post->GetPosition().z()/mm-.5)<1e-6,"Photon hit the wrong face");
        if(!boundary) {
            auto* list=track->GetDefinition()->GetProcessManager()->GetProcessList();
            for(std::size_t i=0;i<list->size();++i)
                if(auto* process=dynamic_cast<G4OpBoundaryProcess*>((*list)[i])) boundary=process;
        }
        require(boundary!=nullptr,"Native boundary process missing");
        const auto status=boundary->GetStatus();
        if(status==Absorption) {
            ++absorbed;require(track->GetTrackStatus()==fStopAndKill,"Absorbed photon still alive");
        } else {
            ++reflected;
            const auto direction=post->GetMomentumDirection();
            require(direction.z()<0,"Coating transmitted the photon");
            require(std::abs(post->GetKineticEnergy()/pre->GetKineticEnergy()-1)<1e-12,"Elastic reflection changed wavelength");
            const double mu=-direction.z();cosines.push_back(mu);sumMu+=mu;sumX+=direction.x();sumY+=direction.y();
            const auto incident=pre->GetMomentumDirection();
            const G4ThreeVector expected{incident.x(),incident.y(),-incident.z()};
            maxDirectionError=std::max(maxDirectionError,(direction-expected).mag());
            if(config.coatingModel=="diffuse") require(status==LambertianReflection,"Diffuse coating is not native LambertianReflection");
        }
        const double flight=step->GetStepLength()/(c_light/config.nScintillator);
        maxTimeError=std::max(maxTimeError,std::abs((post->GetGlobalTime()-pre->GetGlobalTime()-flight)/ns));
        track->SetTrackStatus(fStopAndKill);
    }
    void check(int photons,double wavelength) {
        require(reflected+absorbed==photons,"Boundary accounting lost a photon");
        const double r=mine::coatingReflectivity(config,wavelength);
        require(std::abs(double(reflected)/photons-r)<6*std::sqrt(r*(1-r)/photons)+.001,"Coating survival differs from configured R");
        require(maxTimeError<1e-10,"Reflection added an intrinsic delay");
        double ks=0;
        if(reflected && config.coatingModel=="diffuse") {
            std::sort(cosines.begin(),cosines.end());
            for(std::size_t i=0;i<cosines.size();++i) {
                const double cdf=cosines[i]*cosines[i]; // Lambert: F(mu)=mu^2.
                ks=std::max(ks,std::max(std::abs(cdf-double(i)/reflected),std::abs(cdf-double(i+1)/reflected)));
            }
            require(ks<6/std::sqrt(double(reflected)),"Diffuse angles fail Lambert cosine law");
            require(std::abs(sumMu/reflected-2./3)<6/std::sqrt(18.*reflected),"Incorrect mean Lambert cosine");
            require(std::abs(sumX/reflected)<3/std::sqrt(double(reflected)) && std::abs(sumY/reflected)<3/std::sqrt(double(reflected)),"Diffuse azimuth is not symmetric");
        } else if(reflected) require(maxDirectionError<1e-12,"Historical specular law changed");
        std::cout<<" expected_R="<<r<<" reflected="<<reflected<<" absorbed="<<absorbed<<" R="<<double(reflected)/photons
                 <<" mean_cos="<<(reflected?sumMu/reflected:0)<<" KS="<<ks<<" max_time_error_ns="<<maxTimeError<<'\n';
    }
};
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"usage: geant4-coating-test optical-config.conf");
        const auto config=mine::Geant4OpticsConfig::Load(argv[1]);
        G4Random::setTheSeed(931827);
        auto run=std::make_unique<G4RunManager>();run->SetUserInitialization(new Detector(config));run->SetUserInitialization(new Physics);
        auto* source=new Source;auto* steps=new Steps(config);
        run->SetUserAction(source);run->SetUserAction(steps);run->Initialize();
        constexpr int photons=5000;
        for(const auto& probe:{std::pair<double,double>{0,425},{45,425},{75,425},{45,350},{45,400},{45,550},{45,700}}) {
            source->angle=probe.first*deg;source->wavelength=probe.second;steps->reset();run->BeamOn(photons);
            std::cout<<"COATING_NATIVE material="<<config.coatingMaterial<<" model="<<config.coatingModel<<" angle_deg="<<probe.first<<" lambda_nm="<<probe.second;
            steps->check(photons,source->wavelength);
        }
        std::cout<<"Native Geant4 coating checks passed: "<<config.coatingModel<<'\n';
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
