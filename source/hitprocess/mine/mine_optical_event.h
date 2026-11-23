#pragma once

#include <cstdlib>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace mine {
// Explicitly selected by the launcher; leaving it unset preserves older direct
// launches and the standalone effective-response helpers.
inline bool geant4OpticalMode() {
    const char* raw = std::getenv("MINE_RESPONSE_MODE");
    if (!raw || std::string(raw) == "tabulated") return false;
    if (std::string(raw) == "geant4") return true;
    throw std::invalid_argument("MINE_RESPONSE_MODE must be tabulated or geant4");
}

struct OpticalArrival {
    int panel = 0, sensor = 0, track = 0, parent = 0;
    // Geant4 internal units for time/path; wavelength is explicitly in nm.
    // pathLength includes the complete ancestral path through WLS generations.
    double time = 0, wavelengthNm = 0, pathLength = 0;
    bool detected = false; // PDE decision, filled once by simulateGeant4().
    bool pdeApplied = false;
    int fiber=-1,firstWlsFiber=-1,nWls=0,entryFiber=-1;
    int entryRole = -1; // Sensor-entry medium: panel=0, core=1, clad=2, sensor=3, filling=4.
};

struct OpticalWlsAbsorption {
    int panel=-1,fiber=-1,track=0,parent=0,secondaryCount=0;
    double time=0,wavelengthNm=0,x=0,y=0,z=0;
};
struct OpticalEventStore {
    std::vector<OpticalArrival> arrivals;
    std::vector<OpticalWlsAbsorption> wlsAbsorptions;
    std::map<int, long> generatedScintillation;
    std::map<int, double> expectedScintillation, visibleScintillation;
    std::uint64_t configFingerprint = 0;
    void clear() {
        arrivals.clear(); wlsAbsorptions.clear(); generatedScintillation.clear();
        expectedScintillation.clear(); visibleScintillation.clear();
        configFingerprint = 0;
    }
};

// Sequential GEMC today; thread-local also prevents accidental event mixing in
// future worker configurations. Clear at BeginOfEvent, consume after tracking.
inline OpticalEventStore& OpticalEvent() {
    static thread_local OpticalEventStore event;
    return event;
}
}
