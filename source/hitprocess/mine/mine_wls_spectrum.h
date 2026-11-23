#pragma once
#include "mine_y11_spectral_data.h"
#include "mine_kuraray_wls_data.h"
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace mine {
inline constexpr double opticalHcEvNm=1239.841984332;
inline constexpr double unresolvedWlsLengthMm=1e15;
template<std::size_t N> double spectralOrdinate(
        const std::array<std::pair<double,double>,N>& points,double wavelengthNm) {
    if(!std::isfinite(wavelengthNm) || wavelengthNm<=0)
        throw std::invalid_argument("Invalid spectral wavelength");
    if(wavelengthNm<points.front().first || wavelengthNm>points.back().first) return 0;
    const auto upper=std::lower_bound(points.begin(),points.end(),wavelengthNm,
        [](const auto& point,double value){return point.first<value;});
    if(upper==points.begin()) return upper->second;
    if(upper==points.end()) return points.back().second;
    const auto& low=*(upper-1);
    return low.second+(upper->second-low.second)*(wavelengthNm-low.first)/(upper->first-low.first);
}
inline double y11AbsorptionLengthMm(double wavelengthNm,double concentrationPpm) {
    if(!std::isfinite(concentrationPpm) || concentrationPpm<=0 || concentrationPpm>1e6)
        throw std::invalid_argument("Invalid Y11 dye concentration in ppm");
    const double coefficient=.0147*concentrationPpm*spectralOrdinate(y11data::absorption,wavelengthNm);
    return coefficient>0 ? std::min(unresolvedWlsLengthMm,1/coefficient) : unresolvedWlsLengthMm;
}
inline double y11EmissionPerEV(double wavelengthNm,bool powerPerNm=false) {
    // Convert density per wavelength to density per energy. Common factors
    // cancel when Geant4 normalizes. For radiant power per nm also divide by
    // photon energy before applying |d lambda/d E|.
    const double ratio=wavelengthNm/476.;
    return spectralOrdinate(y11data::emission,wavelengthNm)*ratio*ratio*(powerPerNm?ratio:1.);
}
struct Y11PropertyVectors {
    std::vector<double> energyEv,absorptionLengthMm,emissionPerEV;
};
inline Y11PropertyVectors y11PropertyVectors(double concentrationPpm,bool powerPerNm=false,double stepNm=.25) {
    if(!std::isfinite(stepNm) || stepNm<.01 || stepNm>1)
        throw std::invalid_argument("Y11 property grid step must be 0.01..1 nm");
    std::vector<double> energies{1,6};
    for(double lambda=349;lambda<=641;lambda+=stepNm) energies.push_back(opticalHcEvNm/lambda);
    for(const auto& p:y11data::absorption) energies.push_back(opticalHcEvNm/p.first);
    for(const auto& p:y11data::emission) energies.push_back(opticalHcEvNm/p.first);
    // Preserve support boundaries without extending measured curves.
    for(double lambda:{y11data::absorption.front().first,y11data::absorption.back().first,
                       y11data::emission.front().first,y11data::emission.back().first}) {
        energies.push_back(opticalHcEvNm/(lambda-1e-5));
        energies.push_back(opticalHcEvNm/(lambda+1e-5));
    }
    std::sort(energies.begin(),energies.end());
    energies.erase(std::unique(energies.begin(),energies.end(),[](double a,double b){return std::abs(a-b)<1e-12;}),energies.end());
    Y11PropertyVectors result;result.energyEv=energies;
    for(double energy:energies) {
        const double lambda=opticalHcEvNm/energy;
        result.absorptionLengthMm.push_back(y11AbsorptionLengthMm(lambda,concentrationPpm));
        result.emissionPerEV.push_back(y11EmissionPerEV(lambda,powerPerNm));
    }
    return result;
}
inline void fingerprintY11Spectra(std::uint64_t& hash) {
    std::ostringstream text;text<<std::setprecision(17)<<y11data::version<<";linear_lambda;energy_grid_0.25nm;unresolved_1e15mm;k430=0.0147;";
    for(const auto& point:y11data::absorption) text<<point.first<<','<<point.second<<';';
    text<<"emission:";
    for(const auto& point:y11data::emission) text<<point.first<<','<<point.second<<';';
    for(unsigned char ch:text.str()) {hash^=ch;hash*=1099511628211ULL;}
}
// PS/PMMA construction and the peak absorption normalization are shared working
// assumptions. Only Y11 has an absolute coefficient measurement; YS uses an
// effective concentration, not a claimed dye formulation. See reference/wls_fibers.md.
inline bool isSpectralWlsModel(const std::string& model) {
    return model=="spectral_y11" || model=="spectral_wls";
}
inline int wlsProfileId(const std::string& fiber) {
    if(fiber=="Y-11" || fiber=="Y11") return 0;
    if(fiber=="YS-6" || fiber=="YS6") return 1;
    if(fiber=="YS-4" || fiber=="YS4") return 2;
    throw std::invalid_argument("MINE fiberModel must be Y-11, YS-6 or YS-4");
}
inline double wlsProfileDecayNs(const std::string& fiber) {
    return std::array<double,3>{{6.9,1.3,1.4}}.at(wlsProfileId(fiber));
}
inline double wlsProfilePeakNm(const std::string& fiber) {
    return std::array<double,3>{{476,462,470}}.at(wlsProfileId(fiber));
}
inline double wlsRelativeAbsorption(const std::string& fiber,double lambda) {
    switch(wlsProfileId(fiber)) {
    case 0:return spectralOrdinate(y11data::absorption,lambda);
    case 1:return spectralOrdinate(kuraraydata::ys6_absorption,lambda);
    default:return spectralOrdinate(kuraraydata::ys4_absorption,lambda);
    }
}
inline double wlsRelativeEmission(const std::string& fiber,double lambda) {
    switch(wlsProfileId(fiber)) {
    case 0:return spectralOrdinate(y11data::emission,lambda);
    case 1:return spectralOrdinate(kuraraydata::ys6_emission,lambda);
    default:return spectralOrdinate(kuraraydata::ys4_emission,lambda);
    }
}
inline double wlsAbsorptionLengthMm(const std::string& fiber,double lambda,double concentrationPpm) {
    if(!std::isfinite(concentrationPpm) || concentrationPpm<=0 || concentrationPpm>1e6)
        throw std::invalid_argument("Invalid effective WLS dye concentration");
    const double rate=.0147*concentrationPpm*wlsRelativeAbsorption(fiber,lambda);
    return rate>0?std::min(unresolvedWlsLengthMm,1/rate):unresolvedWlsLengthMm;
}
inline Y11PropertyVectors wlsPropertyVectors(const std::string& fiber,double concentrationPpm,bool power=false) {
    if(wlsProfileId(fiber)==0) return y11PropertyVectors(concentrationPpm,power);
    // Include all 2-nm raster ordinates plus a finer interpolation grid. Spectra
    // end at zero, so interpolation to the 1..6 eV guard points is also zero.
    Y11PropertyVectors v;v.energyEv={1,6};
    for(double lambda=299;lambda<=701;lambda+=.25) v.energyEv.push_back(opticalHcEvNm/lambda);
    std::sort(v.energyEv.begin(),v.energyEv.end());
    for(double e:v.energyEv) {
        const double lambda=opticalHcEvNm/e,ratio=lambda/476.;
        v.absorptionLengthMm.push_back(wlsAbsorptionLengthMm(fiber,lambda,concentrationPpm));
        v.emissionPerEV.push_back(wlsRelativeEmission(fiber,lambda)*ratio*ratio*(power?ratio:1));
    }
    return v;
}
inline void fingerprintWlsSpectra(std::uint64_t& hash,const std::string& fiber) {
    if(wlsProfileId(fiber)==0) {fingerprintY11Spectra(hash);return;}
    std::ostringstream text;text<<std::setprecision(17)<<kuraraydata::version<<fiber
        <<";linear_lambda;energy_grid_0.25nm;shared_peak_k=0.0147;unresolved_1e15mm;";
    const auto& absorption=wlsProfileId(fiber)==1?kuraraydata::ys6_absorption:kuraraydata::ys4_absorption;
    const auto& emission=wlsProfileId(fiber)==1?kuraraydata::ys6_emission:kuraraydata::ys4_emission;
    for(const auto& p:absorption)text<<p.first<<','<<p.second<<';';
    for(const auto& p:emission)text<<p.first<<','<<p.second<<';';
    for(unsigned char ch:text.str()){hash^=ch;hash*=1099511628211ULL;}
}
} // namespace mine
