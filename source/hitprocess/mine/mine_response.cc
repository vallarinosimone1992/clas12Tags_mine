#include "mine_response.h"
#include "mine_fiber_geometry.h"
#include "mine_sipm_pde.h"
#include "mine_optical_response.h"
#include "G4Poisson.hh"
#include "Randomize.hh"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace mine {
namespace {
double attenuation(double distance, double length) { return length > 0 ? std::exp(-distance/length) : 1; }
double delay(double distance, double velocity) { return velocity > 0 ? distance/velocity : 0; }
double exponential(double mean) {
    return mean > 0 ? -mean*std::log(std::max(G4UniformRand(), 1e-300)) : 0;
}
// Protect against accidental pathological settings without silently clipping physics.
constexpr std::size_t maxPulses = 1000000;
long poisson(double mean) {
    if (!std::isfinite(mean) || mean < 0 || mean > maxPulses)
        throw std::runtime_error("MINE PE/dark mean outside supported [0,1e6] range");
    return mean > 0 ? G4Poisson(mean) : 0;
}
}
double birksVisibleEnergy(double edep, double length, int charge, double birks) {
    if (edep <= 0) return 0;
    // Match BDX's helper for neutral tracks and zero-length steps, without its
    // subsequent Edep_B=Edep overwrite. Zero-length charged steps are counted.
    if (charge == 0 || length <= 0 || birks == 0) return edep;
    return edep/(1+birks*edep/length);
}
ScintillationResponse scintillate(const HitInfo& hit, const ScintillatorParameters& p) {
    ScintillationResponse out;
    bool first = true;
    for (const auto& step : hit.steps) {
        if (!std::isfinite(step.edep) || !std::isfinite(step.length) || !std::isfinite(step.time) ||
            step.edep < 0 || step.length < 0)
            throw std::invalid_argument("MINE received invalid step energy/length/time");
        if (step.edep == 0) continue;
        if (first || step.time < out.firstTime) out.firstTime = step.time;
        first = false;
        const double visible = birksVisibleEnergy(step.edep, step.length, step.charge, p.birks);
        out.edep += step.edep; out.evis += visible;
        const double photons = visible*p.lightYield;
        out.photons += photons;
        if (step.charge != 0 && step.length == 0) ++out.unquenchedSteps;
        out.packets.push_back({photons, step.time, p.decayTime, 0, step.local});
    }
    return out;
}
CollectionResponse collectLight(const ScintillationResponse& light, const G4ThreeVector& readout,
                                const ScintillatorParameters& s, const CollectionParameters& c, const FiberParameters* fiber) {
    CollectionResponse out;
    const bool useRoute = fiber && fiber->layout != 0;
    const auto paths = useRoute ? fiberPaths(*fiber) : std::vector<FiberPath>{};
    for (auto packet : light.packets) {
        double distance = (packet.local-readout).mag();
        if (useRoute) {
            const auto route = nearestFiberRoute(packet.local, paths, fiber->readoutEnd);
            distance = route.bulkDistance;
            packet.fiberDistance = route.distanceToEnd + fiber->length;
        }
        if (!std::isfinite(distance)) throw std::invalid_argument("MINE invalid local position");
        out.distance += packet.photons*distance;
        packet.photons *= c.efficiency*attenuation(distance, s.bulkAttenuationLength);
        packet.time += delay(distance, s.propagationVelocity);
        out.photons += packet.photons;
        out.packets.push_back(packet);
    }
    if (light.photons > 0) out.distance /= light.photons;
    return out;
}
TransportResponse transportDirectFiber(const CollectionResponse& light, const FiberParameters& p) {
    TransportResponse out;
    out.mode = ReadoutMode::DirectFiber;
    double weight = 0;
    for (auto packet : light.packets) {
        const double distance = packet.fiberDistance >= 0 ? packet.fiberDistance : p.length;
        out.distance += packet.photons*distance;
        weight += packet.photons;
        packet.photons *= p.couplingEfficiency*attenuation(distance, p.attenuationLength);
        packet.time += delay(distance, p.propagationVelocity);
        out.photons += packet.photons;
        out.packets.push_back(packet);
    }
    if (weight > 0) out.distance /= weight;
    return out;
}
TransportResponse transportWLS(const CollectionResponse& light, const FiberParameters& p) {
    auto out = transportDirectFiber(light, p);
    out.mode = ReadoutMode::WLS;
    out.photons *= p.trappingEfficiency;
    for (auto& packet : out.packets) {
        packet.photons *= p.trappingEfficiency;
        packet.transportDecay = p.wlsDecayTime;
        packet.wavelengthNm = p.emissionWavelengthNm;
    }
    return out;
}
TransportResponse transportLight(const CollectionResponse& light, const FiberParameters& p) {
    if (p.mode == ReadoutMode::WLS) return transportWLS(light, p);
    if (p.mode == ReadoutMode::DirectFiber) return transportDirectFiber(light, p);
    // Reference mode deliberately ignores all fiber parameters.
    TransportResponse out;
    out.photons = light.photons; out.packets = light.packets;
    return out;
}
SiPMResponse respondSiPM(const TransportResponse& light, double origin, const SiPMParameters& p,
                        const ElectronicsParameters& e) {
    SiPMResponse out;
    struct Candidate { double time; bool dark; };
    std::vector<Candidate> candidates;
    for (const auto& packet : light.packets) {
        const double wavelength = packet.wavelengthNm > 0 ? packet.wavelengthNm : p.wavelengthNm;
        const double pde = p.ideal_SiPM ? 1 : (p.useSpectralPDE ? spectralPDE(wavelength) : p.pde);
        const double mean = packet.photons*pde;
        out.meanNpe += mean;
        long count = 0;
        if (packet.resolvedArrival) {
            // Geant4 has already drawn the photon population and its complete
            // history. A second Poisson draw would change both the efficiency
            // variance and ideal-SiPM photon counts. Apply one Bernoulli PDE.
            if (packet.photons != 1 || packet.emissionDecay != 0 || packet.transportDecay != 0 || packet.opticalTable)
                throw std::invalid_argument("MINE explicit arrival has an effective optical response attached");
            auto& arrival = *packet.resolvedArrival;
            if (!arrival.pdeApplied) {
                arrival.detected = pde >= 1 || (pde > 0 && G4UniformRand() < pde);
                arrival.pdeApplied = true;
            }
            count = arrival.detected ? 1 : 0;
        } else count = poisson(mean);
        if (candidates.size()+count > maxPulses) throw std::runtime_error("MINE too many PE pulses");
        out.npe += count; // Primary detected PE before pixel losses; not dark/crosstalk.
        for (long i = 0; i < count; ++i) {
            // Sample independently for each PE, never once for an entire energy step.
            const double opticalDelay = packet.opticalTable ? packet.opticalTable->SampleTime(
                packet.local, packet.sensorId, G4UniformRand(), G4UniformRand()) : 0;
            candidates.push_back({packet.time+exponential(packet.emissionDecay)+
                                  opticalDelay+exponential(packet.transportDecay), false});
        }
    }
    const long dark = p.ideal_SiPM ? 0 : poisson(p.darkCountRate*(e.windowEnd-e.windowStart));
    if (candidates.size()+dark > maxPulses) throw std::runtime_error("MINE too many dark pulses");
    for (long i = 0; i < dark; ++i)
        candidates.push_back({origin+e.windowStart+G4UniformRand()*(e.windowEnd-e.windowStart), true});
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.time < b.time; });
    const int pixels = p.ideal_SiPM ? 0 : p.pixels;
    std::vector<bool> occupied(pixels, false);
    auto fire = [&](double time) {
        if (pixels > 0) {
            const int pixel = std::min(pixels-1, static_cast<int>(G4UniformRand()*pixels));
            if (occupied[pixel]) return false;
            occupied[pixel] = true; // Toy gate occupancy: no recovery within this hit.
        }
        out.pulses.push_back({time, p.gain}); ++out.fired;
        return true;
    };
    for (const auto& candidate : candidates) {
        if (!fire(candidate.time)) continue;
        if (candidate.dark) ++out.dark;
        // One-generation prompt cross-talk and optional afterpulses; not a cascade.
        if (!p.ideal_SiPM && p.crossTalkProbability > 0 && G4UniformRand() < p.crossTalkProbability && fire(candidate.time))
            ++out.crossTalk;
        if (!p.ideal_SiPM && p.afterpulseProbability > 0 && G4UniformRand() < p.afterpulseProbability) {
            // Full-amplitude delayed afterpulse assumes recovery; does not consume
            // another prompt pixel. No partial recovery or trap model is claimed.
            out.pulses.push_back({candidate.time+exponential(p.afterpulseTime), p.gain});
            ++out.afterpulses;
        }
    }
    std::sort(out.pulses.begin(), out.pulses.end(), [](const Avalanche& a, const Avalanche& b) { return a.time < b.time; });
    return out;
}
Response simulateTabulated(const HitInfo& hit, const MINEParameters& p, const TabulatedOptics& optics) {
    validate(p);
    optics.ValidatePanel(hit.panelHalfSize);
    Response out;
    out.tabulated = true;
    out.fiberCount = optics.fiberCount; out.fiberModel = optics.model; out.fiberLayout = optics.layout;
    out.opticalFingerprint = optics.fingerprint;
    out.scintillator = scintillate(hit, p.scintillator);
    for (int s=0; s<2; ++s) {
        auto& channel = out.sensors[s];
        // Conditional transport includes collection, trapping and any exported
        // attenuation. There is no nearest-fiber route or second optical loss.
        channel.collection.distance = channel.transport.distance = -1;
        channel.transport.mode = optics.mode;
        for (auto packet : out.scintillator.packets) {
            packet.photons *= optics.Efficiency(packet.local, s);
            channel.collection.photons += packet.photons;
            channel.collection.packets.push_back(packet);
            packet.photons *= p.fiber.couplingEfficiency;
            packet.transportDecay = optics.additionalWlsDecay;
            packet.wavelengthNm = optics.wavelengthNm;
            packet.opticalTable = &optics; packet.sensorId = s;
            channel.transport.photons += packet.photons;
            channel.transport.packets.push_back(packet);
        }
        channel.sipm = respondSiPM(channel.transport, out.scintillator.firstTime, p.sipm, p.electronics);
        channel.waveform = makeWaveform(channel.sipm.pulses, out.scintillator.firstTime, p.electronics);
        channel.digitized = digitize(channel.waveform, p.electronics);
    }
    out.bothTimingValid = out.sensors[0].digitized.timingValid && out.sensors[1].digitized.timingValid;
    if (out.bothTimingValid) {
        const double t0=out.sensors[0].digitized.tdc, t1=out.sensors[1].digitized.tdc;
        out.meanTdc=(t0+t1)/2; out.differenceTdc=t1-t0;
    }
    const auto& selected=out.sensors.at(p.fiber.readoutEnd);
    out.collection=selected.collection; out.transport=selected.transport;
    out.sipm=selected.sipm; out.waveform=selected.waveform; out.digitized=selected.digitized;
    return out;
}
Response simulate(const HitInfo& hit, const MINEParameters& input) {
    if (input.useGeant4Optics)
        throw std::invalid_argument("MINE explicit optical response requires panel-associated arrivals: use simulateGeant4");
    if (input.useOpticalTable) return simulateTabulated(hit, input, ActiveOpticalResponse());
    auto p = input;
    p.fiber.panelHalfX = hit.panelHalfSize.x();
    p.fiber.panelHalfY = hit.panelHalfSize.y();
    if (!std::isfinite(hit.panelHalfSize.z()) || hit.panelHalfSize.z() <= 0 ||
        (p.fiber.layout != 0 && std::abs(p.fiber.z) > hit.panelHalfSize.z()))
        throw std::invalid_argument("MINE fiber Z lies outside the scintillator Box");
    validate(p);
    Response out;
    out.fiberModel=p.fiber.model; out.fiberLayout=p.fiber.layout;
    out.scintillator = scintillate(hit, p.scintillator);
    out.collection = collectLight(out.scintillator, hit.readoutLocal, p.scintillator, p.collection, &p.fiber);
    out.transport = transportLight(out.collection, p.fiber);
    out.sipm = respondSiPM(out.transport, out.scintillator.firstTime, p.sipm, p.electronics);
    out.waveform = makeWaveform(out.sipm.pulses, out.scintillator.firstTime, p.electronics);
    out.digitized = digitize(out.waveform, p.electronics);
    return out;
}
Response simulateGeant4(const HitInfo& hit, const MINEParameters& p, int panel,
                        std::vector<OpticalArrival>& arrivals, long generatedScintillation,
                        int fiberCount, std::uint64_t opticalFingerprint, int fiberModel, int fiberLayout) {
    validate(p);
    if (generatedScintillation < 0) throw std::invalid_argument("MINE negative generated scintillation count");
    Response out;
    out.geant4Optics = true;
    // Explicit geometry/material profiles; effective routing is never called.
    if(fiberModel<0 || fiberModel>2) throw std::invalid_argument("MINE invalid explicit fiber profile");
    if (fiberCount < 1) throw std::invalid_argument("MINE invalid explicit fiber count");
    if(fiberLayout!=3 && fiberLayout!=7) throw std::invalid_argument("MINE unsupported explicit fiber layout");
    if(fiberLayout==7 && fiberCount!=2) throw std::invalid_argument("MINE mini_panel_spiral requires two fibers");
    out.fiberCount = fiberCount; out.fiberModel = fiberModel; out.fiberLayout = fiberLayout;
    out.opticalFingerprint = opticalFingerprint;
    out.generatedScintillation = generatedScintillation;
    out.scintillator = scintillate(hit, p.scintillator); // Deposition truth/expected mean only.
    out.scintillator.packets.clear(); // No second scintillation generation or timing draw.
    for (const auto& arrival : arrivals)
        if (arrival.panel == panel && (arrival.sensor < 0 || arrival.sensor > 1))
            throw std::invalid_argument("MINE tracked optical arrival has invalid sensor identifier");
    for (int s = 0; s < 2; ++s) {
        auto& channel = out.sensors[s];
        channel.collection.distance = channel.transport.distance = -1;
        channel.transport.mode = ReadoutMode::WLS;
        for (auto& arrival : arrivals) {
            if (arrival.panel != panel || arrival.sensor != s) continue;
            if (!std::isfinite(arrival.time) || !std::isfinite(arrival.wavelengthNm) || arrival.wavelengthNm <= 0 ||
                !std::isfinite(arrival.pathLength) || arrival.pathLength < 0)
                throw std::invalid_argument("MINE invalid tracked optical arrival");
            LightPacket packet;
            packet.photons = 1; packet.time = arrival.time;
            packet.wavelengthNm = arrival.wavelengthNm; packet.sensorId = s;
            packet.resolvedArrival = &arrival;
            channel.collection.photons += 1;
            channel.transport.photons += 1;
            channel.transport.packets.push_back(packet);
        }
        channel.sipm = respondSiPM(channel.transport, out.scintillator.firstTime, p.sipm, p.electronics);
        channel.waveform = makeWaveform(channel.sipm.pulses, out.scintillator.firstTime, p.electronics);
        channel.digitized = digitize(channel.waveform, p.electronics);
    }
    out.bothTimingValid = out.sensors[0].digitized.timingValid && out.sensors[1].digitized.timingValid;
    if (out.bothTimingValid) {
        const double t0 = out.sensors[0].digitized.tdc, t1 = out.sensors[1].digitized.tdc;
        out.meanTdc = (t0+t1)/2; out.differenceTdc = t1-t0;
    }
    const auto& selected = out.sensors.at(p.fiber.readoutEnd);
    out.collection = selected.collection; out.transport = selected.transport;
    out.sipm = selected.sipm; out.waveform = selected.waveform; out.digitized = selected.digitized;
    return out;
}
}
