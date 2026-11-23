#ifndef MINE_RESPONSE_H
#define MINE_RESPONSE_H
#include "mine_waveform.h"
#include "mine_optical_event.h"
#include <array>
#include <cstdint>

namespace mine {
struct TabulatedOptics;
// Adapter boundary: no dependency on MHit beyond this point.
struct StepInfo {
    double edep = 0, length = 0, time = 0;
    int charge = 0, pid = 0;
    G4ThreeVector local, global;
};
struct HitInfo {
    std::vector<StepInfo> steps;
    G4ThreeVector readoutLocal;
    G4ThreeVector panelHalfSize{500*CLHEP::mm,500*CLHEP::mm,5*CLHEP::mm};
};
struct LightPacket {
    double photons = 0, time = 0, emissionDecay = 0, transportDecay = 0;
    G4ThreeVector local;
    double fiberDistance = -1; // Per-packet geometrical distance to selected end; -1 legacy.
    double wavelengthNm = 0; // 0 uses the unshifted scintillator wavelength proxy.
    const TabulatedOptics* opticalTable = nullptr;
    int sensorId = 0;
    OpticalArrival* resolvedArrival = nullptr; // One already tracked photon; Bernoulli PDE.
};
struct ScintillationResponse {
    double edep = 0, evis = 0, photons = 0, firstTime = 0;
    int unquenchedSteps = 0; // Charged depositing steps with zero length.
    std::vector<LightPacket> packets;
};
struct CollectionResponse {
    double photons = 0, distance = 0; // Produced-photon-weighted bulk distance.
    std::vector<LightPacket> packets;
};
struct TransportResponse {
    ReadoutMode mode = ReadoutMode::Reference;
    double photons = 0, distance = 0; // Separate fiber path after collection.
    std::vector<LightPacket> packets;
};
struct SiPMResponse {
    double meanNpe = 0;
    long npe = 0, fired = 0, dark = 0, crossTalk = 0, afterpulses = 0;
    std::vector<Avalanche> pulses;
};
struct SensorResponse {
    CollectionResponse collection;
    TransportResponse transport;
    SiPMResponse sipm;
    Waveform waveform;
    DigitizedResponse digitized;
};
struct Response {
    std::array<SensorResponse,2> sensors;
    bool tabulated = false, geant4Optics = false, bothTimingValid = false;
    long generatedScintillation = 0; // Actual Geant4 scintillation photons, optical mode only.
    double meanTdc = -1, differenceTdc = -1;
    int fiberCount = 0, fiberModel = 0, fiberLayout = 0;
    std::uint64_t opticalFingerprint = 0;
    // Existing scalar fields alias the selected sensor for bank compatibility.
    ScintillationResponse scintillator;
    CollectionResponse collection;
    TransportResponse transport;
    SiPMResponse sipm;
    Waveform waveform;
    DigitizedResponse digitized;
};
double birksVisibleEnergy(double edep, double length, int charge, double birks);
ScintillationResponse scintillate(const HitInfo&, const ScintillatorParameters&);
CollectionResponse collectLight(const ScintillationResponse&, const G4ThreeVector& readout,
                                const ScintillatorParameters&, const CollectionParameters&,
                                const FiberParameters* fiber = nullptr);
TransportResponse transportWLS(const CollectionResponse&, const FiberParameters&);
TransportResponse transportDirectFiber(const CollectionResponse&, const FiberParameters&);
TransportResponse transportLight(const CollectionResponse&, const FiberParameters&);
SiPMResponse respondSiPM(const TransportResponse&, double origin, const SiPMParameters&,
                        const ElectronicsParameters&);
Response simulateTabulated(const HitInfo&, const MINEParameters&, const TabulatedOptics&);
Response simulateGeant4(const HitInfo&, const MINEParameters&, int panel,
                        std::vector<OpticalArrival>&, long generatedScintillation,
                        int fiberCount = 15, std::uint64_t opticalFingerprint = 0, int fiberModel = 0, int fiberLayout = 3);
Response simulate(const HitInfo&, const MINEParameters&);
}
#endif
