#include "mine_hitprocess.h"
#include "G4Exception.hh"
#include "mine_optical_response.h"
#include "mine_optical_geometry_model.h"
#include "mine_wls_spectrum.h"
#include <stdexcept>
#include <mutex>

namespace {
mine::HitInfo readHit(MHit* hit, const mine::CollectionParameters& parameters) {
    mine::HitInfo out;
    const auto edep = hit->GetEdep(), length = hit->GetDx(), time = hit->GetTime();
    const auto local = hit->GetLPos(), global = hit->GetPos();
    const auto charge = hit->GetCharges(), pid = hit->GetPIDs();
    const auto n = edep.size();
    if (length.size()!=n || time.size()!=n || local.size()!=n || global.size()!=n || charge.size()!=n || pid.size()!=n)
        throw std::runtime_error("MINE MHit step-vector sizes differ");
    const auto detectors = hit->GetDetectors();
    if (detectors.empty() || detectors.front().type != "Box" || detectors.front().dimensions.size() < 3)
        throw std::runtime_error("MINE effective response requires a Box detector");
    for (int i = 0; i < 3; ++i) out.panelHalfSize[i] = detectors.front().dimensions[i];
    for (int i = 0; i < 3; ++i) out.readoutLocal[i] = parameters.readoutFraction[i]*detectors.front().dimensions[i];
    for (std::size_t i = 0; i < n; ++i)
        out.steps.push_back({edep[i], length[i], time[i], charge[i], pid[i], local[i], global[i]});
    return out;
}
}
void mine_HitProcess::initWithRunNumber(int) {
    try {
        const auto p = mine::parametersFromMap(gpars, !mine::geant4OpticalMode() || mine::ActiveGeant4OpticsConfig().sipmModel=="ideal");
        if (p.useGeant4Optics) {
            const auto& optics = mine::ActiveGeant4OpticsConfig();
            if (std::abs(p.scintillator.birks/(CLHEP::mm/CLHEP::MeV)-optics.birksMmPerMeV)>1e-12 ||
                std::abs(p.scintillator.lightYield*CLHEP::MeV-optics.lightYieldPerMeV)>1e-9 ||
                std::abs(p.scintillator.decayTime/CLHEP::ns-optics.scintillationDecayNs)>1e-12)
                throw std::invalid_argument("MINE Geant4 material config Birks/yield/decay must match the TEXT scintillator parameters");
            static std::once_flag responseNotice;
            std::call_once(responseNotice, [&optics] {
                std::cout << "MINE explicit optical response: geometry=" << optics.geometry
                          << "; sipmModel=" << optics.sipmModel << "; additional timing jitter=0; "
                             "tracked arrivals use full Geant4 times/wavelengths, without effective optical factors."
                          << std::endl;
            });
        } else if (p.useOpticalTable) (void)mine::ActiveOpticalResponse();
    } catch (const std::exception& error) {
        G4Exception("mine_HitProcess::initWithRunNumber", "MINE_OPTICAL_RESPONSE", FatalException, error.what());
    }
}
mine::Response mine_HitProcess::response(MHit* hit) {
    auto parameters = mine::parametersFromMap(gpars, !mine::geant4OpticalMode() || mine::ActiveGeant4OpticsConfig().sipmModel=="ideal");
    if (parameters.useGeant4Optics) {
        const auto identity = hit->GetId();
        if (identity.empty()) throw std::runtime_error("MINE optical hit without panel identifier");
        auto& event = mine::OpticalEvent();
        const int panel = identity.front().id;
        const auto& optics = mine::ActiveGeant4OpticsConfig();
        // The material configuration is authoritative in explicit mode, so
        // reported visible energy/expected yield use the same Birks/yield.
        parameters.scintillator.birks = optics.birksMmPerMeV*CLHEP::mm/CLHEP::MeV;
        parameters.scintillator.lightYield = optics.lightYieldPerMeV/CLHEP::MeV;
        parameters.scintillator.decayTime = optics.scintillationDecayNs*CLHEP::ns;
        auto out = mine::simulateGeant4(readHit(hit, parameters.collection), parameters, panel,
                                       event.arrivals, event.generatedScintillation[panel],
                                       optics.fiberCount, optics.fingerprint, mine::wlsProfileId(optics.fiberModel), optics.geometry=="mini_panel_spiral"?7:3);
        // Core/cladding particle deposits belong to Edep and t0, but do not
        // scintillate. The stepping hook accumulates these quantities only in
        // the panel scintillator, using its actual Geant4 material properties.
        out.scintillator.evis = event.visibleScintillation[panel];
        out.scintillator.photons = event.expectedScintillation[panel];
        return out;
    }
    return mine::simulate(readHit(hit, parameters.collection), parameters);
}
map<string, double> mine_HitProcess::integrateDgt(MHit* hit, int hitn) {
    writeHit = false; // Framework requires a fresh decision for each hit.
    if (hit->isBackgroundHit || hit->isElectronicNoise) return {}; // No step truth in these constructors.
    const auto identity = hit->GetId();
    if (identity.empty()) throw std::runtime_error("MINE hit without panel identifier");
    // The response helper exposes each stage, including the actual sampled waveform.
    const auto r = response(hit);
    if (r.scintillator.edep <= 0) return {};
    writeHit = true; // Keep below-threshold hits for validation; timingValid distinguishes them.
    using namespace CLHEP;
    const auto parameters = mine::parametersFromMap(gpars, !mine::geant4OpticalMode() || mine::ActiveGeant4OpticsConfig().sipmModel=="ideal");
    map<string,double> values = {{"hitn", hitn}, {"id", identity.front().id},
            {"Edep", r.scintillator.edep/MeV}, {"Evis", r.scintillator.evis/MeV},
            {"Nphotons", r.scintillator.photons}, {"Ncollected", r.collection.photons},
            {"Ntransported", r.transport.photons}, {"meanNpe", r.sipm.meanNpe},
            {"Npe", r.sipm.npe}, {"Nfired", r.sipm.fired},
            {"Ndark", r.sipm.dark}, {"NcrossTalk", r.sipm.crossTalk}, {"Nafterpulse", r.sipm.afterpulses},
            {"ADC", r.digitized.adc}, {"TDC", r.digitized.timingValid ? r.digitized.tdc/ns : -1},
            {"timingValid", r.digitized.timingValid ? 1 : 0},
            {"t0", r.scintillator.firstTime/ns}, {"readoutDistance", r.collection.distance/mm},
            {"fiberDistance", r.transport.distance/mm}, {"readoutMode", static_cast<int>(r.transport.mode)},
            {"unquenchedSteps", r.scintillator.unquenchedSteps},
            {"readoutEnd", parameters.fiber.readoutEnd},
            {"fiberModel", r.fiberModel},
            {"fiberLayout", r.fiberLayout},
            {"idealSiPM", parameters.sipm.ideal_SiPM ? 1 : 0},
            {"effectivePDE", r.transport.photons > 0 ? r.sipm.meanNpe/r.transport.photons : 0},
            {"opticalTable", r.tabulated ? 1 : 0}, {"opticalHash", static_cast<double>(r.opticalFingerprint)},
            {"responseMode", r.geant4Optics ? 2 : (r.tabulated ? 1 : 0)},
            {"Nscintillation", r.geant4Optics ? static_cast<double>(r.generatedScintillation) : -1},
            {"fiberCount", r.fiberCount}, {"bothTimingValid", r.bothTimingValid ? 1 : 0},
            {"TDCmean", r.bothTimingValid ? r.meanTdc/ns : -1},
            {"TDCdifference", r.bothTimingValid ? r.differenceTdc/ns : -1}};
    for (int s=0; s<2; ++s) {
        const auto& channel=r.sensors[s];const auto suffix=std::to_string(s);
        values["Ncollected"+suffix]=channel.collection.photons;
        values["Ntransported"+suffix]=channel.transport.photons;
        values["meanNpe"+suffix]=channel.sipm.meanNpe;
        values["Npe"+suffix]=channel.sipm.npe;
        values["Nfired"+suffix]=channel.sipm.fired;
        values["Ndark"+suffix]=channel.sipm.dark;
        values["NcrossTalk"+suffix]=channel.sipm.crossTalk;
        values["Nafterpulse"+suffix]=channel.sipm.afterpulses;
        values["ADC"+suffix]=channel.digitized.adc;
        values["TDC"+suffix]=channel.digitized.timingValid ? channel.digitized.tdc/ns : -1;
        values["timingValid"+suffix]=channel.digitized.timingValid ? 1 : 0;
        values["effectivePDE"+suffix]=channel.transport.photons>0 ? channel.sipm.meanNpe/channel.transport.photons : 0;
    }
    return values;
}
vector<identifier> mine_HitProcess::processID(vector<identifier> id, G4Step*, detector) {
    if (!id.empty()) id.back().id_sharing = 1; // One depositing-panel MHit; both sensor responses share its bank row.
    return id;
}
map<int, vector<double>> mine_HitProcess::chargeTime(MHit*, int) {
    // SIGNALVT unconditionally indexes hardware[0..4] and uses a different
    // HitProcess instance from integrateDgt. Empty data would crash; inventing
    // addresses or resampling the response would be misleading. Fail explicitly.
    G4Exception("mine_HitProcess::chargeTime", "MINE_NO_HARDWARE_MAP", FatalException,
                "MINE SIGNALVT is not supported: use mine::Response.waveform for timing studies.");
    return {};
}
double mine_HitProcess::voltage(double charge, double time, double forTime) {
    return charge*mine::singlePixelPulse(forTime-time, mine::parametersFromMap(gpars).electronics);
}
