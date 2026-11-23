#ifndef MINE_GEANT4_OPTICS_H
#define MINE_GEANT4_OPTICS_H
#include "mine_optical_geometry_model.h"
#include <vector>
#include <memory>
class G4VPhysicalVolume;
class G4LogicalVolume;
class G4Material;
class G4VSolid;
class G4Step;
class G4MaterialPropertiesTable;
class G4OpticalSurface;
class G4VisManager;
namespace mine {
struct MiniSpiralModel;
enum class OpticalVolumeRole { Panel,Core,Cladding,Sensor,Filler };
struct OpticalVolumeInfo {
    OpticalVolumeRole role=OpticalVolumeRole::Panel;
    int panelId=-1,fiberId=-1,sensorId=-1;
};
struct Geant4OpticsGeometry {
    G4VPhysicalVolume* panel=nullptr;
    CornerFanOpticalModel model;
    std::vector<G4VPhysicalVolume*> cores,claddings,sensors;
    G4VPhysicalVolume* filler=nullptr;
    std::vector<G4VPhysicalVolume*> fillers;
    std::vector<std::vector<OpticalPoint>> fiberPaths;
    std::shared_ptr<MiniSpiralModel> mini;
};
// Geometry stores own their Geant4 objects. Registry entries remain valid for
// this detector construction; no PDE or response-table dependency occurs here.
const Geant4OpticsGeometry& BuildGeant4Optics(G4VPhysicalVolume* panel,
    G4VPhysicalVolume* environment,int panelId,const Geant4OpticsConfig&);
// Shared factories and registry for the independent mini-panel builder.
G4Material* CreateGeant4ScintillatorMaterial(const std::string&,G4Material*,const Geant4OpticsConfig&);
G4Material* CreateGeant4OpticalMaterial(const std::string&,G4Material*,double index,double absorptionMm=0);
G4Material* CreateGeant4GrooveMaterial(const std::string&,const std::string& material);
void RegisterGeant4OpticalVolume(G4VPhysicalVolume*,const OpticalVolumeInfo&);
const Geant4OpticsGeometry& StoreGeant4OpticalGeometry(std::unique_ptr<Geant4OpticsGeometry>);
const Geant4OpticsGeometry& BuildMiniGeant4Optics(G4VPhysicalVolume*,G4VPhysicalVolume*,int,const Geant4OpticsConfig&);
// Same effective opaque coating used on panel/world and exterior cladding contacts.
// Exposed for native boundary tests; Geant4 owns the returned surface.
G4OpticalSurface* BuildGeant4CoatingSurface(const std::string& name,const Geant4OpticsConfig&);
G4VSolid* BuildOpticalGrooveSolid(const std::string& name,const CornerFanOpticalModel&,
    double widthMm,double depthMm);
// Shared actual-path loss, including optional independent core/cladding losses.
void ConfigureGeant4FiberAbsorption(G4MaterialPropertiesTable*,const Geant4OpticsConfig&,bool core);
void ConfigureGeant4WLS(G4MaterialPropertiesTable*,const Geant4OpticsConfig&);
const OpticalVolumeInfo* FindOpticalVolume(const G4VPhysicalVolume*);
// Call once before reconstructing the detector, after the previous event ends.
// Geant4 stores own/delete solids, materials and volumes, not this registry.
void ResetGeant4OpticsRegistry();
// Display sampled centerlines instead of Boolean-solid polyhedra; transport stays exact.
void RegisterGeant4OpticsVisualization(G4VisManager*);
// Geant4 10.7.4 zero-length boundary relocation can leave the velocity copied
// from its temporary incident medium. Restore the actual destination GROUPVEL
// before the next finite flight; no path, delay or effective speed is added.
bool SynchronizeOpticalVelocity(const G4Step*);
// Public to let isolated straight/bend tests use exactly the panel's solid
// builder. Faces are normal to supplied common readout planes, including cuts
// oblique to the fiber tangent. All joins are inside a single Boolean solid.
G4VSolid* BuildOpticalFiberSolid(const std::string& name,const OpticalFiberRoute&,
    double radiusMm,const std::array<OpticalReadoutPlane,2>&);
} // namespace mine
#endif
