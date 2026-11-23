#pragma once
#include "mine_scintillation_spectral_data.h"
#include "mine_wls_spectrum.h"
#include <string>

namespace mine {
inline bool isScintillationSpectrum(const std::string& profile) {
    return profile=="monochromatic" || profile=="fnal_extruded" || profile=="ej200";
}
inline double scintillationPerNm(const std::string& profile,double lambda) {
    if(profile=="fnal_extruded") return spectralOrdinate(scintdata::fnal_extruded,lambda);
    if(profile=="ej200") return spectralOrdinate(scintdata::ej200,lambda);
    throw std::invalid_argument("Expected a tabulated scintillation spectrum");
}
struct ScintillationPropertyVector {
    std::vector<double> energyEv,emissionPerEV;
};
inline ScintillationPropertyVector scintillationPropertyVector(const std::string& profile,bool powerPerNm=false) {
    if(profile!="fnal_extruded" && profile!="ej200")
        throw std::invalid_argument("Expected fnal_extruded or ej200 spectrum");
    std::vector<double> wavelengths;
    const auto append=[&](const auto& points) {
        for(const auto& p:points) wavelengths.push_back(p.first);
        // The plots end with no measured tail beyond their support. Tiny
        // guard intervals prevent Geant4 from interpolating to 1 or 6 eV.
        for(double edge:{points.front().first,points.back().first}) {
            wavelengths.push_back(edge-1e-5);wavelengths.push_back(edge+1e-5);
        }
        for(double lambda=points.front().first;lambda<=points.back().first;lambda+=.25)
            wavelengths.push_back(lambda);
    };
    if(profile=="fnal_extruded") append(scintdata::fnal_extruded);
    else append(scintdata::ej200);
    ScintillationPropertyVector result;result.energyEv={1,6};
    for(double lambda:wavelengths) result.energyEv.push_back(opticalHcEvNm/lambda);
    std::sort(result.energyEv.begin(),result.energyEv.end());
    auto& energies=result.energyEv;
    energies.erase(std::unique(energies.begin(),energies.end(),[](double a,double b){return std::abs(a-b)<1e-12;}),energies.end());
    for(double energy:energies) {
        const double lambda=opticalHcEvNm/energy,ratio=lambda/425.;
        // Photon density dN/dlambda -> dN/dE: multiply by lambda^2/hc.
        // For power/nm, first divide by photon energy (another lambda/hc).
        result.emissionPerEV.push_back(scintillationPerNm(profile,lambda)*ratio*ratio*(powerPerNm?ratio:1.));
    }
    return result;
}
inline void fingerprintScintillationSpectrum(std::uint64_t& hash,const std::string& profile) {
    if(profile=="monochromatic") return; // Preserve hashes of historical config files.
    if(!isScintillationSpectrum(profile)) throw std::invalid_argument("Invalid scintillation spectrum");
    std::ostringstream text;text<<std::setprecision(17)<<scintdata::version<<';'<<profile
        <<";linear_lambda;energy_grid_0.25nm;zero_outside_support;";
    const auto append=[&](const auto& points) {for(const auto& p:points) text<<p.first<<','<<p.second<<';';};
    if(profile=="fnal_extruded") append(scintdata::fnal_extruded);else append(scintdata::ej200);
    for(unsigned char ch:text.str()) {hash^=ch;hash*=1099511628211ULL;}
}
} // namespace mine
