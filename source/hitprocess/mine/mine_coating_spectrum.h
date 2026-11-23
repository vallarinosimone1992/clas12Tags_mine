#pragma once
#include "mine_coating_spectral_data.h"
#include "mine_optical_geometry_model.h"
#include "mine_wls_spectrum.h"
#include <string>

namespace mine {
inline bool isCoatingMaterial(const std::string& material) {
    return material=="constant" || material=="fnal_tio2" || material=="tyvek" || material=="aluminized_mylar";
}
template<std::size_t N> double coatingInterpolated(
    const std::array<std::pair<double,double>,N>& points,double coordinate) {
    if(!std::isfinite(coordinate) || coordinate<=0) throw std::invalid_argument("Invalid coating spectral coordinate");
    // Explicit endpoint assumption outside source support; no translated tail.
    return spectralOrdinate(points,std::clamp(coordinate,points.front().first,points.back().first));
}
inline double coatingReflectivity(const Geant4OpticsConfig& c,double wavelengthNm) {
    if(!std::isfinite(wavelengthNm) || wavelengthNm<=0) throw std::invalid_argument("Invalid coating wavelength");
    if(c.coatingMaterial=="constant") return c.reflectivity;
    if(c.coatingMaterial=="fnal_tio2")
        return coatingInterpolated(coatingdata::fnal_tio2_energy,opticalHcEvNm/wavelengthNm);
    if(c.coatingMaterial=="tyvek")
        return c.coatingReferenceAlbedo*coatingInterpolated(coatingdata::tyvek,wavelengthNm);
    if(c.coatingMaterial=="aluminized_mylar")
        return c.coatingReferenceAlbedo*coatingInterpolated(coatingdata::aluminized_mylar,wavelengthNm);
    throw std::invalid_argument("Unknown coating material");
}
struct CoatingPropertyVector {std::vector<double> energyEv,reflectivity;};
inline CoatingPropertyVector coatingPropertyVector(const Geant4OpticsConfig& c) {
    CoatingPropertyVector result;result.energyEv={1,6};
    if(c.coatingMaterial=="fnal_tio2") {
        // Retain the source's linear-energy interpolation exactly.
        for(const auto& p:coatingdata::fnal_tio2_energy) result.energyEv.push_back(p.first);
    } else if(c.coatingMaterial!="constant") {
        for(double lambda=250;lambda<=800;lambda+=.5) result.energyEv.push_back(opticalHcEvNm/lambda);
        const auto append=[&](const auto& points) {for(const auto& p:points) result.energyEv.push_back(opticalHcEvNm/p.first);};
        if(c.coatingMaterial=="tyvek") append(coatingdata::tyvek);
        else if(c.coatingMaterial=="aluminized_mylar") append(coatingdata::aluminized_mylar);
        else throw std::invalid_argument("Unknown coating material");
    }
    auto& e=result.energyEv;std::sort(e.begin(),e.end());
    e.erase(std::unique(e.begin(),e.end(),[](double a,double b){return std::abs(a-b)<1e-12;}),e.end());
    // Reflectivity is a probability, NOT dN/dlambda: no Jacobian applies.
    for(double energy:e) result.reflectivity.push_back(coatingReflectivity(c,opticalHcEvNm/energy));
    return result;
}
inline void fingerprintCoatingSpectrum(std::uint64_t& hash,const std::string& material) {
    if(material=="constant") return;
    std::ostringstream text;text<<std::setprecision(17)<<coatingdata::version<<';'<<material
        <<";no_peak_shift;endpoint_clamp;raw_FNAL_linear_E;Yang_linear_lambda_grid_0.5nm;";
    const auto append=[&](const auto& points){for(const auto& p:points) text<<p.first<<','<<p.second<<';';};
    if(material=="fnal_tio2") append(coatingdata::fnal_tio2_energy);
    else if(material=="tyvek") append(coatingdata::tyvek);
    else if(material=="aluminized_mylar") append(coatingdata::aluminized_mylar);
    else throw std::invalid_argument("Unknown coating material");
    for(unsigned char ch:text.str()){hash^=ch;hash*=1099511628211ULL;}
}
} // namespace mine
