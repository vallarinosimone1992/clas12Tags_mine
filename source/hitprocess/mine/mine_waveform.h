#ifndef MINE_WAVEFORM_H
#define MINE_WAVEFORM_H
#include "mine_parameters.h"
#include <vector>
namespace mine {
struct Avalanche { double time = 0, charge = 0; };
struct Waveform { std::vector<double> time, amplitude; };
struct DigitizedResponse {
    double charge = 0; // Integral within sampled window, normalized charge units.
    double adc = 0;    // Continuous ADC-like counts (no ADC bit depth assumed).
    double tdc = -1;   // ns internally; -1 sentinel when timingValid is false.
    bool timingValid = false;
};
double singlePixelPulse(double elapsed, const ElectronicsParameters&);
Waveform makeWaveform(const std::vector<Avalanche>&, double origin, const ElectronicsParameters&);
DigitizedResponse digitize(const Waveform&, const ElectronicsParameters&);
}
#endif
