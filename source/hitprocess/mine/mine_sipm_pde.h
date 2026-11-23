#ifndef MINE_SIPM_PDE_H
#define MINE_SIPM_PDE_H
#include <array>
#include <cmath>
#include <stdexcept>
namespace mine {
// Approximate digitization of KAPDB0325EB, red S13360-**75CS curve, p.4.
// s13360_series_kapd1052e.pdf, 25 C, nominal +3 V. Absolute uncertainty ~0.01.
// 270 nm is a zero boundary anchor; measured trace starts near 280 nm.
// No extrapolation beyond the documented 270-900 nm response range.
struct PDEPoint { double wavelengthNm, probability; };
constexpr std::array<PDEPoint, 28> s13360_3075cs_pde{{
    {270, 0.000},
    {280, 0.030},
    {290, 0.152},
    {300, 0.235},
    {325, 0.318},
    {350, 0.355},
    {375, 0.375},
    {400, 0.438},
    {425, 0.478},
    {450, 0.497},
    {475, 0.496},
    {500, 0.480},
    {525, 0.442},
    {550, 0.410},
    {575, 0.367},
    {600, 0.325},
    {625, 0.284},
    {650, 0.242},
    {675, 0.209},
    {700, 0.181},
    {725, 0.156},
    {750, 0.133},
    {775, 0.114},
    {800, 0.094},
    {825, 0.077},
    {850, 0.061},
    {875, 0.048},
    {900, 0.039},
}};
inline double spectralPDE(double wavelengthNm) {
    if (!std::isfinite(wavelengthNm) || wavelengthNm <= 0)
        throw std::invalid_argument("MINE invalid photon wavelength");
    if (wavelengthNm < s13360_3075cs_pde.front().wavelengthNm ||
        wavelengthNm > s13360_3075cs_pde.back().wavelengthNm) return 0;
    for (std::size_t i = 1; i < s13360_3075cs_pde.size(); ++i) {
        const auto a = s13360_3075cs_pde[i-1], b = s13360_3075cs_pde[i];
        if (wavelengthNm <= b.wavelengthNm) {
            const double t = (wavelengthNm-a.wavelengthNm)/(b.wavelengthNm-a.wavelengthNm);
            return a.probability+t*(b.probability-a.probability);
        }
    }
    return s13360_3075cs_pde.back().probability;
}
}
#endif
