#pragma once
class G4Step;
class G4UserTrackingAction;
namespace mine {
void ResetGeant4OpticalTracking();
G4UserTrackingAction* MakeOpticalVisualizationTracking();
// Called before GEMC's generic track cuts. Returns true for an explicit-mode
// optical track, whose physics termination is handled by Geant4 alone.
bool RecordGeant4OpticalStep(const G4Step*);
}
