#ifndef MINE_PARAMETERS_H
#define MINE_PARAMETERS_H

#include "G4ThreeVector.hh"
#include "G4SystemOfUnits.hh"
#include <map>
#include <string>

namespace mine {
enum class ReadoutMode { Reference = 0, WLS = 1, DirectFiber = 2 };

struct ScintillatorParameters {
    // BDX BirksAttenuation formula; this number is from GEMC ScintillatorB,
    // NOT a measured MINE coefficient or a number specified by veto_hitprocess.
    double birks = 0.126 * CLHEP::mm / CLHEP::MeV;
    double lightYield = 8000 / CLHEP::MeV; // MINE extruded-scintillator working value.
    double decayTime = 2.5 * CLHEP::ns; // MINE extruded-scintillator working value.
    double bulkAttenuationLength = 1000 * CLHEP::mm; // 100 cm; effective bulk loss.
    double propagationVelocity = 170 * CLHEP::mm / CLHEP::ns; // 17 cm/ns.
};
struct CollectionParameters {
    // PRE-TRAPPING capture and re-emission probability. Still provisional!
    // Replace with the optical MC capture map; do not put 6.2% here.
    double efficiency = 0.01;
    // Legacy layout 0 only: fraction of Box half-dimensions, default +X edge.
    // Routed layouts derive capture and sensor positions from the fiber centerlines.
    G4ThreeVector readoutFraction{1, 0, 0};
};
struct FiberParameters {
    ReadoutMode mode = ReadoutMode::WLS;
    int model = 0; // 0 Y-11(200) single clad; 1 future YS-6 single-clad hypothesis.
    int layout = 1; // 0 anchor; 1 compact fan-in; 2 legacy double; 3 corner fan;
                    // 4 triple serpentine; 5 panel double serpentine; 6 straight.
    int cornerFibers = 14, diagonal = 0; // 0 bl_tr; 1 tl_br.
    int orientation = 0; // Panel double: 0 vertical; 1 horizontal.
    bool autoSensorPlacement = true;
    double edgeMargin = 100 * CLHEP::mm;
    // Replaced by actual Geant4 Box dimensions by simulate(); defaults for helper callers.
    double panelHalfX = 500 * CLHEP::mm, panelHalfY = 500 * CLHEP::mm;
    int readoutEnd = 0; // One physical sensor per response/run, never sum both SiPMs.
    // Routing snapshot from MC_MINE_LightTrapping/config/default.conf (2026-10-15).
    int parallelFibers = 5, lanesPerFiber = 3;
    double pitch = 100 * CLHEP::mm, centerX = 0, z = 0.5 * CLHEP::mm;
    double straightYMin = -100 * CLHEP::mm, straightYMax = 100 * CLHEP::mm;
    double sensor0X = 0, sensor0Y = -490 * CLHEP::mm;
    double sensor1X = 0, sensor1Y = 490 * CLHEP::mm;
    double sensorPitch = 1.2 * CLHEP::mm, curveStep = 2 * CLHEP::mm;
    double minBendRadius = 100 * CLHEP::mm; // Non-S, 1 mm diameter.
    double length = 0; // Extra tail AFTER the geometrical route; sole path in layout 0.
    double attenuationLength = 3500 * CLHEP::mm; // Conservative proxy for catalog >3.5 m.
    // Derived from n_core=1.59, n_clad=1.49 and uniform guided cos(theta):
    // v_eff=(c/n_core)*(1-r)/log(1/r), r=n_clad/n_core. Not measured group velocity.
    double propagationVelocity = 182.56 * CLHEP::mm / CLHEP::ns;
    double couplingEfficiency = 1;
    double trappingEfficiency = 0.031; // Per end, conditional on isotropic re-emission.
    double wlsDecayTime = 6.9 * CLHEP::ns; // Kuraray Y-11 working value.
    double emissionWavelengthNm = 476; // Peak approximation, not full spectral folding.
};
struct SiPMParameters {
    bool ideal_SiPM = false;
    bool useSpectralPDE = true; // S13360-3075CS typical curve, 25 C, +3 V overvoltage.
    double wavelengthNm = 425; // Unshifted scintillator peak proxy; WLS uses fiber peak.
    double pde = 0.25; // Optional constant override: mine_pde disables the curve.
    double gain = 1; // Normalized charge per avalanche; hardware gain is 4e6 electrons.
    int pixels = 1600; // 3x3 mm, 75 um pitch, 82% fill factor already included in PDE.
    double crossTalkProbability = 0.07;
    // Datasheet does NOT specify an afterpulse probability/time; leave disabled.
    double afterpulseProbability = 0;
    double afterpulseTime = 20 * CLHEP::ns; // Uncalibrated, inactive by default.
    double darkCountRate = 0.0005 / CLHEP::ns; // 500 kcps per device at 25 C.
};
struct ElectronicsParameters {
    // Unit-area difference of exponentials. Amplitude unit is charge/ns.
    double riseTime = 1 * CLHEP::ns;
    double fallTime = 5 * CLHEP::ns;
    double samplingInterval = 0.25 * CLHEP::ns;
    double windowStart = -5 * CLHEP::ns; // Relative to earliest depositing step.
    double windowEnd = 100 * CLHEP::ns;
    double adcConversion = 1; // Counts per normalized integrated charge unit.
    double threshold = 0.05 / CLHEP::ns; // Toy leading-edge threshold.
    double noiseSigma = 0 / CLHEP::ns;  // Independent sample noise, disabled.
    double tdcBin = 0; // 0: continuous interpolated crossing in ns; >0: floor to bin.
};
struct MINEParameters {
    bool useOpticalTable = true; // Default: self-contained optical_response.dat, both sensors.
    bool useGeant4Optics = false; // Explicit MINE_RESPONSE_MODE=geant4; no response table.
    ScintillatorParameters scintillator;
    CollectionParameters collection;
    FiberParameters fiber;
    SiPMParameters sipm;
    ElectronicsParameters electronics;
};

// Optional GEMC TEXT parameters override defaults in Geant4 internal units.
// Unknown mine_* names and invalid values are errors, not silently ignored.
// The explicit material config selects ideal vs measured SiPM; legacy callers
// keep the historical ideal explicit profile through the default argument.
MINEParameters parametersFromMap(const std::map<std::string, double>&, bool explicitIdealSiPM = true);
void validate(const MINEParameters&);
}
#endif
