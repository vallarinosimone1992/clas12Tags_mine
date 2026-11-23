#include "mine_waveform.h"
#include "Randomize.hh"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace mine {
double singlePixelPulse(double t, const ElectronicsParameters& p) {
    if (t < 0) return 0;
    return (std::exp(-t/p.fallTime) - std::exp(-t/p.riseTime))/(p.fallTime-p.riseTime);
}
Waveform makeWaveform(const std::vector<Avalanche>& pulses, double origin, const ElectronicsParameters& p) {
    Waveform w;
    const auto intervals = static_cast<std::size_t>(std::ceil((p.windowEnd-p.windowStart)/p.samplingInterval));
    w.time.reserve(intervals+1); w.amplitude.reserve(intervals+1);
    for (std::size_t i = 0; i <= intervals; ++i) {
        const double t = origin + std::min(p.windowStart+i*p.samplingInterval, p.windowEnd);
        double v = 0;
        for (const auto& pulse : pulses) v += pulse.charge * singlePixelPulse(t-pulse.time, p);
        if (p.noiseSigma > 0) v += G4RandGauss::shoot(0, p.noiseSigma);
        w.time.push_back(t); w.amplitude.push_back(v);
    }
    return w;
}
DigitizedResponse digitize(const Waveform& w, const ElectronicsParameters& p) {
    if (w.time.size() != w.amplitude.size()) throw std::invalid_argument("MINE waveform size mismatch");
    DigitizedResponse d;
    // A waveform already above threshold at the first sample has no measured
    // rising edge inside the gate: do not fabricate a TDC at the gate boundary.
    for (std::size_t i = 1; i < w.time.size(); ++i) {
        const double dt = w.time[i]-w.time[i-1];
        if (dt <= 0) throw std::invalid_argument("MINE waveform times not increasing");
        d.charge += dt*(w.amplitude[i-1]+w.amplitude[i])/2;
        if (!d.timingValid && w.amplitude[i-1] < p.threshold && w.amplitude[i] >= p.threshold) {
            d.tdc = w.time[i-1] + dt*(p.threshold-w.amplitude[i-1])/(w.amplitude[i]-w.amplitude[i-1]);
            if (p.tdcBin > 0) d.tdc = std::floor(d.tdc/p.tdcBin)*p.tdcBin;
            d.timingValid = true;
        }
    }
    d.adc = std::max(0.0, d.charge*p.adcConversion);
    return d;
}
}
