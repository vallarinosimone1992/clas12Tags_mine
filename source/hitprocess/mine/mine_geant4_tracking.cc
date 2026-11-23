#include "mine_geant4_tracking.h"
#include "mine_geant4_optics.h"
#include "mine_optical_event.h"
#include "G4EmSaturation.hh"
#include "G4LossTableManager.hh"
#include "G4OpticalPhoton.hh"
#include "G4OpBoundaryProcess.hh"
#include "G4ProcessManager.hh"
#include "G4ProcessVector.hh"
#include "G4PhysicalConstants.hh"
#include "G4Step.hh"
#include "G4SystemOfUnits.hh"
#include "G4Track.hh"
#include "G4VProcess.hh"
#include "G4UserTrackingAction.hh"
#include "G4TrackingManager.hh"
#include "G4Trajectory.hh"
#include <cstdlib>
#include <map>
#include <sstream>
#include <stdexcept>

namespace mine {
namespace {
struct Ancestry { int panel=-1; double pathBefore=0; int firstWlsFiber=-1,lastWlsFiber=-1,nWls=0,lastExitFiber=-1; };
thread_local std::map<const G4Track*,Ancestry> pending;
thread_local std::map<int,Ancestry> tracks;
thread_local int drawnOptical=0;
class DisplayTrajectory final : public G4Trajectory {
public:
    explicit DisplayTrajectory(const G4Track* track):G4Trajectory(track) {}
    void AppendStep(const G4Step* step) override {
        // Bound display memory even for very long skew rays. No tracking cut.
        if(GetPointEntries()<2000) G4Trajectory::AppendStep(step);
    }
};
class OpticalDisplayTracking final : public G4UserTrackingAction {
public:
    void PreUserTrackingAction(const G4Track* track) override {
        const bool optical=track->GetDefinition()==G4OpticalPhoton::Definition();
        const bool display=!optical || drawnOptical++<50;
        fpTrackingManager->SetStoreTrajectory(display ? 1 : 0);
        if(display) fpTrackingManager->SetTrajectory(new DisplayTrajectory(track));
    }
};
}
void ResetGeant4OpticalTracking() { pending.clear();tracks.clear();drawnOptical=0; }
G4UserTrackingAction* MakeOpticalVisualizationTracking() { return new OpticalDisplayTracking; }
bool RecordGeant4OpticalStep(const G4Step* step) {
    if(!geant4OpticalMode()) return false;
    auto* track=step->GetTrack();
    const bool optical=track->GetDefinition()==G4OpticalPhoton::Definition();
    const auto* pre=step->GetPreStepPoint();const auto* post=step->GetPostStepPoint();
    const auto* region=FindOpticalVolume(pre->GetPhysicalVolume());
    const auto* target=FindOpticalVolume(post->GetPhysicalVolume());
    Ancestry ancestry;
    if(optical) {
        if(track->GetCurrentStepNumber()==1) {
            const auto child=pending.find(track);
            if(child!=pending.end()) {ancestry=child->second;pending.erase(child);}
            else if(region) ancestry.panel=region->panelId; // Optical test primary.
            tracks[track->GetTrackID()]=ancestry;
        } else ancestry=tracks.at(track->GetTrackID());
    } else if(region) ancestry.panel=region->panelId;
    if(optical && region && region->fiberId>=0 &&
       (region->role==OpticalVolumeRole::Core || region->role==OpticalVolumeRole::Cladding) &&
       (!target || target->fiberId!=region->fiberId ||
        (target->role!=OpticalVolumeRole::Core && target->role!=OpticalVolumeRole::Cladding)))
        ancestry.lastExitFiber=region->fiberId;
    if(optical && target && target->role==OpticalVolumeRole::Panel)
        ancestry.lastExitFiber=-1;
    if(optical) tracks[track->GetTrackID()]=ancestry;

    if(!optical && region && region->role==OpticalVolumeRole::Panel && step->GetTotalEnergyDeposit()>0) {
        // Match Geant4's scintillation quenching, including nonionizing losses.
        const double visible=G4LossTableManager::Instance()->EmSaturation()->VisibleEnergyDepositionAtAStep(step);
        auto& event=OpticalEvent();event.visibleScintillation[region->panelId]+=visible;
        event.expectedScintillation[region->panelId]+=visible/MeV*ActiveGeant4OpticsConfig().lightYieldPerMeV;
    }
    const auto* definingProcess=post->GetProcessDefinedStep();
    const bool wlsAbsorbed=optical && definingProcess && definingProcess->GetProcessName()=="OpWLS";
    int wlsSecondaries=0;
    if(wlsAbsorbed) {
        if(!region || region->role!=OpticalVolumeRole::Core || region->fiberId<0)
            throw std::runtime_error("MINE WLS absorption outside an identified fiber core");
        if(ancestry.firstWlsFiber<0) ancestry.firstWlsFiber=region->fiberId;
        ancestry.lastWlsFiber=region->fiberId;++ancestry.nWls;
        for(const auto* child:*step->GetSecondaryInCurrentStep())
            if(child->GetDefinition()==G4OpticalPhoton::Definition()) ++wlsSecondaries;
        const auto point=post->GetPosition();
        OpticalEvent().wlsAbsorptions.push_back({region->panelId,region->fiberId,track->GetTrackID(),
            track->GetParentID(),wlsSecondaries,post->GetGlobalTime(),
            h_Planck*c_light/pre->GetKineticEnergy()/nm,point.x(),point.y(),point.z()});
    }
    for(const auto* secondary:*step->GetSecondaryInCurrentStep()) {
        if(secondary->GetDefinition()!=G4OpticalPhoton::Definition()) continue;
        const auto* creator=secondary->GetCreatorProcess();
        // Child IDs are assigned by the stack later; pointer is stable until
        // tracking begins. A WLS child's origin/path inherits its full lineage.
        auto child=ancestry;
        if(optical) child.pathBefore+=track->GetTrackLength();
        pending[secondary]=child;
        if(creator && creator->GetProcessName()=="Scintillation" && child.panel>=0)
            ++OpticalEvent().generatedScintillation[child.panel];
    }
    if(!optical) return false;
    bool physicalBoundaryAbsorption=false;
    if(post->GetStepStatus()==fGeomBoundary) {
        auto* processes=track->GetDefinition()->GetProcessManager()->GetProcessList();
        for(std::size_t i=0;i<processes->size();++i) {
            auto* boundary=dynamic_cast<G4OpBoundaryProcess*>((*processes)[i]);
            if(boundary && boundary->GetStatus()==NoRINDEX) {
                std::ostringstream message;
                message << "MINE optical boundary has no RINDEX: unsupported material, not a physical loss; pre="
                        << (pre->GetPhysicalVolume()?pre->GetPhysicalVolume()->GetName():"outside")
                        << "; post=" << (post->GetPhysicalVolume()?post->GetPhysicalVolume()->GetName():"outside")
                        << "; position_mm=" << post->GetPosition()/mm
                        << "; track=" << track->GetTrackID() << "; step=" << track->GetCurrentStepNumber();
                throw std::runtime_error(message.str());
            }
            if(boundary && (boundary->GetStatus()==Absorption || boundary->GetStatus()==Detection))
                physicalBoundaryAbsorption=true;
        }
    }
    if(target && target->role==OpticalVolumeRole::Sensor &&
       (!region || region->role!=OpticalVolumeRole::Sensor)) {
        const double energy=pre->GetKineticEnergy();
        if(energy<=0) throw std::runtime_error("MINE optical sensor arrival with nonpositive photon energy");
        OpticalEvent().arrivals.push_back({target->panelId,target->sensorId,track->GetTrackID(),
            track->GetParentID(),post->GetGlobalTime(),h_Planck*c_light/energy/nm,
            ancestry.pathBefore+track->GetTrackLength(),false,false});
        auto& arrival=OpticalEvent().arrivals.back();
        arrival.entryRole=region?static_cast<int>(region->role):-1;
        arrival.entryFiber=region && region->fiberId>=0?region->fiberId:ancestry.lastExitFiber;
        arrival.fiber=ancestry.lastWlsFiber;arrival.firstWlsFiber=ancestry.firstWlsFiber;
        arrival.nWls=ancestry.nWls;
        // Ideal sensor entrance absorbs once. PDE is applied later exactly
        // once to this recorded photon by the existing response pipeline.
        track->SetTrackStatus(fStopAndKill);
    }
    if(track->GetTrackStatus()==fStopAndKill) {
        const auto* process=post->GetProcessDefinedStep();
        if(process && process->GetProcessName()=="Transportation" && post->GetStepStatus()!=fWorldBoundary &&
           !physicalBoundaryAbsorption &&
           (!target || target->role!=OpticalVolumeRole::Sensor))
            throw std::runtime_error("MINE Geant4 Transportation killed an optical track inside the world: numerical failure, not attenuation");
        tracks.erase(track->GetTrackID());
    }
    SynchronizeOpticalVelocity(step);
    return true;
}
}
