#include "mine_response.h"
#include "mine_sipm_pde.h"
#include "Randomize.hh"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>

int main() {
    using namespace mine;
    using namespace CLHEP;
    HepRandom::setTheSeed(913721);
    setenv("MINE_RESPONSE_MODE", "geant4", 1);
    const auto selected = parametersFromMap({{"ideal_SiPM",0}});
    assert(selected.useGeant4Optics && !selected.useOpticalTable && selected.sipm.ideal_SiPM);
    const auto measured = parametersFromMap({{"ideal_SiPM",1}}, false);
    assert(measured.useGeant4Optics && !measured.sipm.ideal_SiPM && measured.sipm.useSpectralPDE);
    const auto miniSelected = parametersFromMap({{"mine_fiber_layout",7}}, true);
    assert(miniSelected.fiber.layout==7 && miniSelected.sipm.ideal_SiPM);
    setenv("MINE_RESPONSE_MODE", "tabulated", 1);
    bool incompatibleMode = false;
    try { parametersFromMap({{"mine_use_optical_table",0}}); }
    catch (const std::invalid_argument&) { incompatibleMode = true; }
    assert(incompatibleMode);
    bool miniLegacyRejected = false;
    try { parametersFromMap({{"mine_fiber_layout",7}}); }
    catch (const std::invalid_argument&) { miniLegacyRejected = true; }
    assert(miniLegacyRejected);
    unsetenv("MINE_RESPONSE_MODE");
    MINEParameters p;
    p.useGeant4Optics = true; p.useOpticalTable = false;
    p.sipm.ideal_SiPM = true;
    HitInfo hit;
    hit.steps = {{0, 0, 0, 1, 13, {}, {}}, {2*MeV, 10*mm, 3*ns, -1, 13, {}, {}}};
    std::vector<OpticalArrival> arrivals = {
        {1,0,11,2,5*ns,476,300*mm}, {1,0,12,2,123*ns,476,500*mm},
        {1,1,13,2,8*ns,425,400*mm}, {2,0,14,2,9*ns,476,500*mm}};
    // Effective transport settings must have no influence on tracked photons.
    p.scintillator.propagationVelocity = .001*mm/ns;
    p.scintillator.bulkAttenuationLength = .001*mm;
    p.scintillator.decayTime = 900*ns;
    p.fiber.propagationVelocity = .001*mm/ns;
    p.fiber.attenuationLength = .001*mm;
    p.fiber.trappingEfficiency = 0; p.fiber.couplingEfficiency = 0;
    p.fiber.wlsDecayTime = 900*ns;
    const auto r = simulateGeant4(hit, p, 1, arrivals, 12345);
    const auto mini = simulateGeant4(hit,p,1,arrivals,12345,2,456,0,7);
    assert(mini.geant4Optics && mini.fiberLayout==7 && mini.fiberCount==2 && mini.opticalFingerprint==456);
    bool miniCountRejected=false;
    try { simulateGeant4(hit,p,1,arrivals,12345,15,456,0,7); }
    catch(const std::invalid_argument&) { miniCountRejected=true; }
    assert(miniCountRejected);
    for(int profile:{1,2}) {
        const auto ys=simulateGeant4(hit,p,1,arrivals,12345,15,123,profile);
        assert(ys.fiberModel==profile && ys.opticalFingerprint==123);
    }
    assert(r.geant4Optics && !r.tabulated && r.generatedScintillation == 12345);
    assert(r.scintillator.edep == 2*MeV && r.scintillator.firstTime == 3*ns);
    assert(r.sensors[0].transport.photons == 2 && r.sensors[1].transport.photons == 1);
    assert(r.sensors[0].sipm.npe == 2 && r.sensors[1].sipm.npe == 1);
    assert(r.sensors[0].sipm.pulses[0].time == 5*ns);
    assert(r.sensors[0].sipm.pulses[1].time == 123*ns); // Tail retained beyond electronics gate.
    assert(r.sensors[1].sipm.pulses[0].time == 8*ns);
    assert(arrivals[0].pdeApplied && arrivals[0].detected && !arrivals[3].pdeApplied);
    assert(r.sensors[0].sipm.fired == 2 && r.sensors[0].sipm.dark == 0);
    assert(r.bothTimingValid && r.sipm.npe == r.sensors[0].sipm.npe);

    // A fixed number of tracked photons must have binomial PE variance, rather
    // than the Poisson variance used for unresolved expected photon packets.
    p.sipm.ideal_SiPM = false; p.sipm.useSpectralPDE = false; p.sipm.pde = .25;
    p.sipm.pixels = 0; p.sipm.darkCountRate = 0; p.sipm.crossTalkProbability = 0;
    constexpr int photons = 200, trials = 5000;
    double sum = 0, squares = 0;
    for (int trial = 0; trial < trials; ++trial) {
        std::vector<OpticalArrival> actual(photons);
        TransportResponse light;
        light.photons = photons;
        for (auto& a : actual) {
            a.time = 5*ns; a.wavelengthNm = 476;
            LightPacket packet;
            packet.photons = 1; packet.time = a.time; packet.wavelengthNm = a.wavelengthNm;
            packet.resolvedArrival = &a;
            light.packets.push_back(packet);
        }
        const auto sensor = respondSiPM(light, 3*ns, p.sipm, p.electronics);
        assert(sensor.meanNpe == photons*.25);
        sum += sensor.npe; squares += sensor.npe*sensor.npe;
        // The event can be inspected twice without a second PDE decision.
        const auto again = respondSiPM(light, 3*ns, p.sipm, p.electronics);
        assert(again.npe == sensor.npe);
    }
    const double mean = sum/trials, variance = squares/trials-mean*mean;
    assert(std::abs(mean-50) < .5 && std::abs(variance-37.5) < 2);
    // Spectral PDE is evaluated at the measured wavelength of each photon.
    p.sipm.useSpectralPDE = true;
    arrivals = {{1,0,1,0,5*ns,425,1*mm}, {1,0,2,0,5*ns,476,1*mm}};
    const auto spectral = simulateGeant4(hit, p, 1, arrivals, 2);
    assert(std::abs(spectral.sensors[0].sipm.meanNpe-spectralPDE(425)-spectralPDE(476)) < 1e-12);
    assert(spectral.sensors[1].sipm.npe == 0 && !spectral.bothTimingValid);
    bool rejected = false;
    arrivals.push_back({1,2,3,0,5*ns,476,1*mm});
    try { simulateGeant4(hit, p, 1, arrivals, 2); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    std::cout << "Explicit optical response passed: exact ideal arrivals/times, panel association, single PDE; binomial mean="
              << mean << ", variance=" << variance << " (expected 50, 37.5)\n";
}
