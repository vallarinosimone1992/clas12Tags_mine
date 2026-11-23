#include "mine_geant4_optics.h"
#include "mine_mini_spiral_model.h"
#include "mine_geant4_tracking.h"
#include "mine_optical_event.h"
#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4Material.hh"
#include "G4MaterialPropertiesTable.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4RunManager.hh"
#include "G4EmStandardPhysics.hh"
#include "G4OpticalPhysics.hh"
#include "G4OpticalParameters.hh"
#include "G4OpticalPhoton.hh"
#include "G4ParticleGun.hh"
#include "G4Event.hh"
#include "G4Step.hh"
#include "G4PhysicalConstants.hh"
#include "G4SystemOfUnits.hh"
#include "G4UserSteppingAction.hh"
#include "G4UserEventAction.hh"
#include "G4VUserDetectorConstruction.hh"
#include "G4VUserPrimaryGeneratorAction.hh"
#include "G4VModularPhysicsList.hh"
#include "G4TransportationManager.hh"
#include "G4Navigator.hh"
#include "Randomize.hh"
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
void require(bool b,const char* msg){if(!b)throw std::runtime_error(msg);}
class Detector:public G4VUserDetectorConstruction {public:mine::Geant4OpticsConfig c;const mine::Geant4OpticsGeometry* geometry=nullptr;explicit Detector(mine::Geant4OpticsConfig x):c(x){}
 G4VPhysicalVolume* Construct()override{auto*n=G4NistManager::Instance();auto*w=new G4LogicalVolume(new G4Box("world",1000*mm,1000*mm,1000*mm),n->FindOrBuildMaterial("G4_AIR"),"world");auto*wp=new G4PVPlacement(nullptr,{},w,"world",nullptr,false,0);auto*p=new G4LogicalVolume(new G4Box("panel",150*mm,250*mm,4*mm),n->FindOrBuildMaterial("G4_PLASTIC_SC_VINYLTOLUENE"),"panel");auto*pp=new G4PVPlacement(nullptr,{},p,"panel",w,false,0);geometry=&mine::BuildGeant4Optics(pp,wp,1,c);auto* t=p->GetMaterial()->GetMaterialPropertiesTable();require(std::abs(p->GetMaterial()->GetDensity()/(g/cm3)-1.023)<1e-9,"Wrong EJ200 density");require(std::abs(t->GetConstProperty("FASTSCINTILLATIONRISETIME")/ns-.9)<1e-12,"Missing rise MPT");return wp;}
};
class Physics:public G4VModularPhysicsList{public:Physics(){RegisterPhysics(new G4EmStandardPhysics);auto*p=G4OpticalParameters::Instance();p->SetScintFiniteRiseTime(true);p->SetWLSTimeProfile("exponential");p->SetProcessActivation("Cerenkov",false);p->SetProcessActivation("OpRayleigh",false);p->SetProcessActivation("OpMieHG",false);p->SetProcessActivation("OpWLS2",false);RegisterPhysics(new G4OpticalPhysics);}void SetCuts()override{SetCutsWithDefault();}};
class Source:public G4VUserPrimaryGeneratorAction{G4ParticleGun gun{1};public:Source(){gun.SetParticleDefinition(G4OpticalPhoton::Definition());gun.SetParticleEnergy(h_Planck*c_light/(550*nm));gun.SetParticleMomentumDirection({0,-1,0});gun.SetParticlePolarization({1,0,0});}void GeneratePrimaries(G4Event* e)override{int k=e->GetEventID()%5;const double x=k<2?130:100,z=k<2?2.5+k:4.5+(k-2);gun.SetParticlePosition(k==4?G4ThreeVector(130.8*mm,-249.8*mm,2*mm):G4ThreeVector(x*mm,-249.8*mm,z*mm));gun.GeneratePrimaryVertex(e);}};
class Steps:public G4UserSteppingAction{public:void UserSteppingAction(const G4Step*s)override{mine::RecordGeant4OpticalStep(s);
if(G4RunManager::GetRunManager()->GetCurrentEvent()->GetEventID()<5)std::cout<<"TRACE "<<G4RunManager::GetRunManager()->GetCurrentEvent()->GetEventID()<<" step="<<s->GetTrack()->GetCurrentStepNumber()<<" pre="<<(s->GetPreStepPoint()->GetPhysicalVolume()?s->GetPreStepPoint()->GetPhysicalVolume()->GetName():"null")<<" post="<<(s->GetPostStepPoint()->GetPhysicalVolume()?s->GetPostStepPoint()->GetPhysicalVolume()->GetName():"null")<<" len="<<s->GetStepLength()/mm<<" time="<<s->GetPostStepPoint()->GetGlobalTime()/ns<<std::endl;}};
class Events:public G4UserEventAction{public:std::array<int,5> good{},total{};void BeginOfEventAction(const G4Event*)override{mine::ResetGeant4OpticalTracking();mine::OpticalEvent().clear();}void EndOfEventAction(const G4Event* e)override{int k=e->GetEventID()%5;++total[k];for(auto&a:mine::OpticalEvent().arrivals){int sensor=k<2||k==4?0:1,fiber=k==4?-1:k%2;if(e->GetEventID()<5)std::cout<<"ARR "<<k<<" sensor="<<a.sensor<<" fiber="<<a.entryFiber<<" time="<<a.time/ns<<std::endl;if(a.sensor==sensor && a.entryFiber==fiber && a.time<.1*ns)++good[k];}}};
int main(int argc,char**argv){try{require(argc==2,"Usage mini-test CONFIG");setenv("MINE_RESPONSE_MODE","geant4",1);G4Random::setTheSeed(20261123);auto rm=std::make_unique<G4RunManager>();auto*d=new Detector(mine::Geant4OpticsConfig::Load(argv[1]));rm->SetUserInitialization(d);rm->SetUserInitialization(new Physics);rm->SetUserAction(new Source);rm->SetUserAction(new Steps);auto* events=new Events;rm->SetUserAction(events);rm->Initialize();
 auto* nav=G4TransportationManager::GetTransportationManager()->GetNavigatorForTracking();int points=0;for(int f=0;f<2;++f)for(const auto&s:d->geometry->mini->fibers[f].segments){auto p=s.PointAtFraction(.5);auto*pv=nav->LocateGlobalPointAndSetup({p.x*mm,p.y*mm,p.z*mm});const auto*info=mine::FindOpticalVolume(pv);if(!info || info->role!=mine::OpticalVolumeRole::Core || info->fiberId!=f){std::cerr<<"Bad point "<<p.x<<','<<p.y<<','<<p.z<<" volume="<<(pv?pv->GetName():"none")<<std::endl;throw std::runtime_error("Centerline material navigation failure");}++points;}
 for(int f=0;f<2;++f){auto& ss=d->geometry->mini->fibers[f].segments;auto& last=ss.back();std::cout<<"LAST fiber="<<f<<" R="<<last.radiusMm<<" sweep="<<last.sweepRad<<" start="<<last.start.x<<","<<last.start.y<<","<<last.start.z<<" end="<<last.end.x<<","<<last.end.y<<","<<last.end.z<<std::endl;G4ThreeVector p(100*mm,-249.8*mm,(4.5+f)*mm);std::cout<<"INSIDE core="<<d->geometry->cores[f]->GetLogicalVolume()->GetSolid()->Inside(p)<<" clad="<<d->geometry->claddings[f]->GetLogicalVolume()->GetSolid()->Inside(p)<<std::endl;}std::cout<<"MINI_NAVIGATION_PASS points="<<points<<std::endl;rm->BeamOn(100);for(int k=0;k<5;++k){std::cout<<"MINI_COUPLING case="<<k<<" good="<<events->good[k]<<'/'<<events->total[k]<<std::endl;require(events->good[k]>=18,"Gel/sensor transport or last-fiber attribution failed");}std::cout<<"MINI_NATIVE_PASS"<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"ERROR: "<<e.what()<<std::endl;return 1;}}
