#include "mine_parameters.h"
#include "mine_optical_event.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace mine {
namespace {
void require(bool ok, const std::string& message) {
    if (!ok) throw std::invalid_argument("MINE parameters: " + message);
}
void nonnegative(double x, const char* name) {
    require(std::isfinite(x) && x >= 0, std::string(name) + " must be finite and >= 0");
}
void probability(double x, const char* name) {
    nonnegative(x, name);
    require(x <= 1, std::string(name) + " must be <= 1");
}
}
MINEParameters parametersFromMap(const std::map<std::string, double>& values, bool explicitIdealSiPM) {
    MINEParameters p;
    auto& s = p.scintillator; auto& c = p.collection;
    auto& f = p.fiber; auto& sipm = p.sipm; auto& e = p.electronics;
    // Profile selection must happen before explicit overrides, independent of map order.
    auto profile = values.find("mine_fiber_model");
    if (profile != values.end()) {
        require(profile->second == 0 || profile->second == 1, "fiber_model must be 0 (Y-11) or 1 (YS-6)");
        f.model = static_cast<int>(profile->second);
        if (f.model == 1) {
            f.attenuationLength = 3000 * CLHEP::mm; // Catalog >3 m, conservative proxy.
            f.wlsDecayTime = 1.47 * CLHEP::ns; // Kodama et al. 2024, NOT a catalog guarantee.
            f.emissionWavelengthNm = 462;
        }
    }
    // Geometry-specific defaults are selected BEFORE explicit parameter overrides.
    const auto layoutKey = values.find("mine_fiber_layout");
    if (layoutKey != values.end() && layoutKey->second == 4) {
        f.pitch = 80 * CLHEP::mm;
        f.edgeMargin = 80 * CLHEP::mm;
    }
    if (values.count("mine_pde")) sipm.useSpectralPDE = false;
    double mode = static_cast<int>(f.mode), pixels = sipm.pixels;
    double ideal = sipm.ideal_SiPM, spectral = sipm.useSpectralPDE;
    double tabulated = p.useOpticalTable;
    double model = f.model, layout = f.layout, end = f.readoutEnd;
    double parallel = f.parallelFibers, lanes = f.lanesPerFiber;
    double corner = f.cornerFibers, diagonal = f.diagonal, orientation = f.orientation;
    double autoSensor = f.autoSensorPlacement;
    double rx = c.readoutFraction.x(), ry = c.readoutFraction.y(), rz = c.readoutFraction.z();
    std::map<std::string, double*> fields = {
        {"mine_use_optical_table", &tabulated}, {"mine_birks", &s.birks}, {"mine_light_yield", &s.lightYield},
        {"mine_decay_time", &s.decayTime}, {"mine_bulk_attenuation", &s.bulkAttenuationLength},
        {"mine_bulk_velocity", &s.propagationVelocity}, {"mine_collection_efficiency", &c.efficiency},
        {"mine_readout_x", &rx}, {"mine_readout_y", &ry}, {"mine_readout_z", &rz},
        {"mine_readout_mode", &mode}, {"mine_fiber_length", &f.length},
        {"mine_fiber_attenuation", &f.attenuationLength}, {"mine_fiber_velocity", &f.propagationVelocity},
        {"mine_coupling_efficiency", &f.couplingEfficiency},
        {"mine_wls_trapping_efficiency", &f.trappingEfficiency},
        {"mine_fiber_model", &model}, {"mine_fiber_layout", &layout}, {"mine_readout_end", &end},
        {"mine_corner_fibers", &corner}, {"mine_corner_diagonal", &diagonal},
        {"mine_serpentine_orientation", &orientation}, {"mine_auto_sensor_placement", &autoSensor},
        {"mine_edge_margin", &f.edgeMargin},
        {"mine_parallel_fibers", &parallel}, {"mine_lanes_per_fiber", &lanes},
        {"mine_fiber_pitch", &f.pitch}, {"mine_fiber_center_x", &f.centerX}, {"mine_fiber_z", &f.z},
        {"mine_straight_ymin", &f.straightYMin}, {"mine_straight_ymax", &f.straightYMax},
        {"mine_sensor0_x", &f.sensor0X}, {"mine_sensor0_y", &f.sensor0Y},
        {"mine_sensor1_x", &f.sensor1X}, {"mine_sensor1_y", &f.sensor1Y},
        {"mine_sensor_pitch", &f.sensorPitch}, {"mine_curve_step", &f.curveStep},
        {"mine_min_bend_radius", &f.minBendRadius},
        {"mine_wls_wavelength_nm", &f.emissionWavelengthNm},
        {"ideal_SiPM", &ideal}, {"mine_spectral_pde", &spectral},
        {"mine_photon_wavelength_nm", &sipm.wavelengthNm},
        {"mine_wls_decay_time", &f.wlsDecayTime}, {"mine_pde", &sipm.pde},
        {"mine_gain", &sipm.gain}, {"mine_pixels", &pixels},
        {"mine_crosstalk", &sipm.crossTalkProbability}, {"mine_afterpulse", &sipm.afterpulseProbability},
        {"mine_afterpulse_time", &sipm.afterpulseTime}, {"mine_dark_rate", &sipm.darkCountRate},
        {"mine_pulse_rise", &e.riseTime}, {"mine_pulse_fall", &e.fallTime},
        {"mine_sampling_interval", &e.samplingInterval}, {"mine_window_start", &e.windowStart},
        {"mine_window_end", &e.windowEnd}, {"mine_adc_conversion", &e.adcConversion},
        {"mine_threshold", &e.threshold}, {"mine_noise_sigma", &e.noiseSigma}, {"mine_tdc_bin", &e.tdcBin}
    };
    for (const auto& value : values) {
        if (value.first.rfind("mine_", 0) != 0 && value.first != "ideal_SiPM") continue;
        require(value.first != "mine_wls_efficiency",
                "mine_wls_efficiency removed: capture/re-emission belongs in mine_collection_efficiency; "
                "use mine_wls_trapping_efficiency for per-end guided fraction");
        auto field = fields.find(value.first);
        require(field != fields.end(), "unknown key " + value.first);
        require(std::isfinite(value.second), value.first + " is not finite");
        *field->second = value.second;
    }
    require(mode == 0 || mode == 1 || mode == 2, "readout_mode must be 0, 1 or 2");
    require(pixels >= 0 && pixels <= 10000000 && std::floor(pixels) == pixels,
            "pixels must be an integer in [0,10000000]");
    require((ideal == 0 || ideal == 1) && (spectral == 0 || spectral == 1), "SiPM flags must be 0 or 1");
    require(model == 0 || model == 1, "invalid fiber_model");
    require(layout >= 0 && layout <= 7 && std::floor(layout) == layout, "fiber_layout must be an integer in [0,7]");
    require(corner >= 2 && corner <= 1000 && std::floor(corner) == corner && static_cast<int>(corner)%2 == 0,
            "corner_fibers must be even and in [2,1000]");
    require(diagonal == 0 || diagonal == 1, "corner_diagonal must be 0 (bl_tr) or 1 (tl_br)");
    require(orientation == 0 || orientation == 1, "serpentine_orientation must be 0 or 1");
    require(autoSensor == 0 || autoSensor == 1, "auto_sensor_placement must be 0 or 1");
    require(end == 0 || end == 1, "readout_end must be 0 or 1");
    require(parallel >= 1 && parallel <= 1000 && std::floor(parallel) == parallel, "invalid parallel_fibers");
    require(lanes >= 3 && lanes <= 999 && std::floor(lanes) == lanes && static_cast<int>(lanes)%2 == 1,
            "lanes_per_fiber must be odd and in [3,999]");
    require(tabulated == 0 || tabulated == 1, "use_optical_table must be 0 or 1");
    p.useOpticalTable = tabulated;
    p.useGeant4Optics = geant4OpticalMode();
    if (p.useGeant4Optics) p.useOpticalTable = false;
    else if (std::getenv("MINE_RESPONSE_MODE"))
        require(p.useOpticalTable, "explicit tabulated mode requires mine_use_optical_table=1");
    sipm.ideal_SiPM = ideal; sipm.useSpectralPDE = spectral;
    // Config-selected explicit operating point; no second PDE draw is added.
    // The default argument preserves historical ideal explicit callers.
    if (p.useGeant4Optics) sipm.ideal_SiPM = explicitIdealSiPM;
    f.model = static_cast<int>(model); f.layout = static_cast<int>(layout); f.readoutEnd = static_cast<int>(end);
    f.cornerFibers = static_cast<int>(corner); f.diagonal = static_cast<int>(diagonal);
    f.orientation = static_cast<int>(orientation); f.autoSensorPlacement = autoSensor;
    f.parallelFibers = static_cast<int>(parallel); f.lanesPerFiber = static_cast<int>(lanes);
    f.mode = static_cast<ReadoutMode>(static_cast<int>(mode));
    sipm.pixels = static_cast<int>(pixels);
    c.readoutFraction = {rx, ry, rz};
    validate(p);
    return p;
}
void validate(const MINEParameters& p) {
    const auto& s = p.scintillator; const auto& f = p.fiber;
    const auto& q = p.sipm; const auto& e = p.electronics;
    nonnegative(s.birks, "birks"); nonnegative(s.lightYield, "light_yield");
    nonnegative(s.decayTime, "decay_time"); nonnegative(s.bulkAttenuationLength, "bulk_attenuation");
    nonnegative(s.propagationVelocity, "bulk_velocity");
    probability(p.collection.efficiency, "collection_efficiency");
    for (int i = 0; i < 3; ++i)
        require(std::isfinite(p.collection.readoutFraction[i]) && std::abs(p.collection.readoutFraction[i]) <= 1,
                "readout coordinates must be finite fractions in [-1,1]");
    require(f.mode == ReadoutMode::Reference || f.mode == ReadoutMode::WLS || f.mode == ReadoutMode::DirectFiber,
            "invalid readout mode");
    nonnegative(f.length, "fiber_length"); nonnegative(f.attenuationLength, "fiber_attenuation");
    nonnegative(f.propagationVelocity, "fiber_velocity"); probability(f.couplingEfficiency, "coupling_efficiency");
    probability(f.trappingEfficiency, "wls_trapping_efficiency");
    require(f.trappingEfficiency <= 0.5, "per-end trapping cannot exceed 0.5");
    require(f.model == 0 || f.model == 1, "invalid fiber_model");
    require(f.layout >= 0 && f.layout <= 7, "invalid fiber_layout");
    require(f.layout!=7 || p.useGeant4Optics, "mini_panel_spiral layout is implemented only for explicit Geant4 transport");
    require(f.cornerFibers >= 2 && f.cornerFibers <= 1000 && f.cornerFibers%2 == 0, "invalid corner_fibers");
    require(f.diagonal == 0 || f.diagonal == 1, "invalid corner_diagonal");
    require(f.orientation == 0 || f.orientation == 1, "invalid serpentine_orientation");
    nonnegative(f.edgeMargin, "edge_margin");
    require(std::isfinite(f.panelHalfX) && f.panelHalfX > 0 && std::isfinite(f.panelHalfY) && f.panelHalfY > 0,
            "invalid panel half dimensions");
    require(2*std::max(f.panelHalfX,f.panelHalfY)/f.pitch <= 10000, "fiber_pitch too small for panel");
    require(f.readoutEnd == 0 || f.readoutEnd == 1, "invalid readout_end");
    require(f.parallelFibers >= 1 && f.parallelFibers <= 1000, "invalid parallel_fibers");
    require(f.lanesPerFiber >= 3 && f.lanesPerFiber <= 999 && f.lanesPerFiber%2 == 1, "invalid lanes_per_fiber");
    require(std::isfinite(f.pitch) && f.pitch > 0, "fiber_pitch must be positive");
    require(std::isfinite(f.curveStep) && f.curveStep > 0, "curve_step must be positive");
    require(std::isfinite(f.minBendRadius) && f.minBendRadius > 0, "minimum bend radius must be positive");
    nonnegative(f.sensorPitch, "sensor_pitch");
    for (double coordinate : {f.centerX, f.z, f.straightYMin, f.straightYMax, f.sensor0X, f.sensor0Y, f.sensor1X, f.sensor1Y})
        require(std::isfinite(coordinate), "non-finite fiber geometry coordinate");
    if (f.layout == 1 || f.layout == 2 || f.layout == 6)
        require(f.sensor0Y < f.straightYMin && f.straightYMin < f.straightYMax && f.straightYMax < f.sensor1Y,
                "require sensor0_y < straight_ymin < straight_ymax < sensor1_y");
    require(std::isfinite(f.emissionWavelengthNm) && f.emissionWavelengthNm > 0, "invalid WLS wavelength");
    require(std::isfinite(q.wavelengthNm) && q.wavelengthNm > 0, "invalid photon wavelength");
    nonnegative(f.wlsDecayTime, "wls_decay_time");
    probability(q.pde, "pde"); nonnegative(q.gain, "gain");
    require(q.pixels >= 0 && q.pixels <= 10000000, "invalid pixels");
    probability(q.crossTalkProbability, "crosstalk"); probability(q.afterpulseProbability, "afterpulse");
    nonnegative(q.afterpulseTime, "afterpulse_time"); nonnegative(q.darkCountRate, "dark_rate");
    require(std::isfinite(e.riseTime) && std::isfinite(e.fallTime) && e.riseTime > 0 && e.fallTime > e.riseTime,
            "pulse requires 0 < rise < fall");
    require(std::isfinite(e.samplingInterval) && e.samplingInterval > 0, "sampling_interval must be positive");
    require(std::isfinite(e.windowStart) && std::isfinite(e.windowEnd) && e.windowEnd > e.windowStart,
            "invalid waveform window");
    require((e.windowEnd - e.windowStart)/e.samplingInterval <= 100000,
            "waveform exceeds 100000 sampling intervals");
    nonnegative(e.adcConversion, "adc_conversion"); nonnegative(e.threshold, "threshold");
    require(e.threshold > 0, "threshold must be positive");
    nonnegative(e.noiseSigma, "noise_sigma"); nonnegative(e.tdcBin, "tdc_bin");
}
}
