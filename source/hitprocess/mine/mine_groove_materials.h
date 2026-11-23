#pragma once
// See reference/grooves.md. Phase index, not an attenuation or trapping factor.
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
namespace mine {
inline bool isGrooveMaterial(const std::string& material) {
    return material=="air" || material=="gel" || material=="glue";
}
inline double grooveRefractiveIndex(const std::string& material,double wavelengthNm) {
    if(!std::isfinite(wavelengthNm) || wavelengthNm<=0)
        throw std::invalid_argument("Invalid groove wavelength");
    if(material=="air") return 1.000277; // NIST ambient-air reference approximation.
    // Thorlabs G608N3 manufacturer Cauchy fit (lambda in nm), anchored by
    // published values from 402 to 1550 nm. UV continuation is an explicit
    // unvalidated extrapolation, not a measured FNAL-UV calibration.
    if(material=="gel") return 1.4338+10520/(wavelengthNm*wavelengthNm);
    if(material=="glue") return 1.57; // Eljen EJ-500; no dispersion supplied.
    throw std::invalid_argument("Unknown groove material");
}
inline void fingerprintGrooveMaterial(std::uint64_t& hash,const std::string& material) {
    const std::string model="open_square_merged_network_v1;"+material+
        ";air_n1.000277;gel_G608N3_n1.4338+10520/lambda_nm^2;glue_EJ500_n1.57;no_absorption;";
    for(unsigned char ch:model) {hash^=ch;hash*=1099511628211ULL;}
}
}
