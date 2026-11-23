#pragma once
#include "mine_optical_geometry_model.h"
#include "mine_wls_spectrum.h"
#include "mine_kuraray_transmission_data.h"
namespace mine {
inline constexpr double fiberHostAnchorNm=520.;
inline constexpr double fiberBlueJoinNm=480.;
inline constexpr double fiberLossReferenceConcentrationPpm=200.;
inline constexpr double dbPerKmToRatePerMm=2.302585092994045684/1e7;
inline bool usesKurarayFiberLoss(const Geant4OpticsConfig& c) {return c.fiberLossModel=="kuraray_spectral";}
template<std::size_t N> double clampedLoss(const std::array<std::pair<double,double>,N>& a,double lambda) {
    return spectralOrdinate(a,std::clamp(lambda,a.front().first,a.back().first));
}
inline double fiberLossSupportMinNm(const std::string& profile) {
    const int id=wlsProfileId(profile);
    if(id==1)return transmissiondata::ys6.front().first;
    if(id==2)return transmissiondata::ys4.front().first;
    return std::max(transmissiondata::ys4.front().first,transmissiondata::ys6.front().first);
}
inline double fiberLossSupportMaxNm(const std::string& profile) {
    const int id=wlsProfileId(profile);
    if(id==1)return transmissiondata::ys6.back().first;
    if(id==2)return transmissiondata::ys4.back().first;
    return std::min(transmissiondata::ys4.back().first,transmissiondata::ys6.back().first);
}
inline double fiberReferenceLossDbPerKm(const std::string& profile,double lambda) {
    if(!std::isfinite(lambda)||lambda<=0)throw std::invalid_argument("Invalid fiber-loss wavelength");
    lambda=std::clamp(lambda,fiberLossSupportMinNm(profile),fiberLossSupportMaxNm(profile));
    switch(wlsProfileId(profile)) {
    case 1:return clampedLoss(transmissiondata::ys6,lambda);
    case 2:return clampedLoss(transmissiondata::ys4,lambda);
    default:return .5*(clampedLoss(transmissiondata::ys4,lambda)+clampedLoss(transmissiondata::ys6,lambda));
    }
}
inline double fiberOrdinaryRatePerMm(const std::string& profile,double lambda) {
    // Red host curve is common to core/cladding. The blue excess belongs to
    // dye reabsorption, not a duplicate irreversible absorption term.
    return dbPerKmToRatePerMm*fiberReferenceLossDbPerKm(profile,std::max(lambda,fiberHostAnchorNm));
}
inline double fiberWlsRatePerMm(const Geant4OpticsConfig& c,double lambda) {
    const double original=1/wlsAbsorptionLengthMm(c.fiberModel,lambda,c.wlsDyeConcentrationPpm);
    if(!usesKurarayFiberLoss(c)||lambda<=fiberBlueJoinNm)return original;
    if(lambda>=fiberHostAnchorNm)return 1/unresolvedWlsLengthMm;
    const double first=fiberLossSupportMinNm(c.fiberModel);
    const auto excess=[&](double wave) {
        return std::max(0.,dbPerKmToRatePerMm*fiberReferenceLossDbPerKm(c.fiberModel,wave)-fiberOrdinaryRatePerMm(c.fiberModel,wave))
              *c.wlsDyeConcentrationPpm/fiberLossReferenceConcentrationPpm;
    };
    if(lambda>=first)return std::max(1/unresolvedWlsLengthMm,excess(lambda));
    const double blue=1/wlsAbsorptionLengthMm(c.fiberModel,fiberBlueJoinNm,c.wlsDyeConcentrationPpm);
    const double red=std::max(1/unresolvedWlsLengthMm,excess(first));
    const double u=(lambda-fiberBlueJoinNm)/(first-fiberBlueJoinNm);
    return std::exp((1-u)*std::log(blue)+u*std::log(red));
}
inline std::vector<double> fiberSpectralEnergyGridEv() {
    std::vector<double> e{1,6};
    for(double lambda=299;lambda<=701;lambda+=.25)e.push_back(opticalHcEvNm/lambda);
    for(const auto& p:transmissiondata::ys4)e.push_back(opticalHcEvNm/p.first);
    for(const auto& p:transmissiondata::ys6)e.push_back(opticalHcEvNm/p.first);
    for(const auto& p:y11data::absorption)e.push_back(opticalHcEvNm/p.first);
    for(const auto& p:y11data::emission)e.push_back(opticalHcEvNm/p.first);
    std::sort(e.begin(),e.end());
    e.erase(std::unique(e.begin(),e.end(),[](double a,double b){return std::abs(a-b)<1e-12;}),e.end());return e;
}
inline Y11PropertyVectors fiberWlsPropertyVectors(const Geant4OpticsConfig& c) {
    if(!usesKurarayFiberLoss(c))return wlsPropertyVectors(c.fiberModel,c.wlsDyeConcentrationPpm,c.wlsEmissionConvention=="power_per_nm");
    Y11PropertyVectors v;v.energyEv=fiberSpectralEnergyGridEv();
    for(double energy:v.energyEv) {
        const double lambda=opticalHcEvNm/energy,ratio=lambda/476.;
        v.absorptionLengthMm.push_back(1/fiberWlsRatePerMm(c,lambda));
        v.emissionPerEV.push_back(wlsRelativeEmission(c.fiberModel,lambda)*ratio*ratio*(c.wlsEmissionConvention=="power_per_nm"?ratio:1));
    }return v;
}
inline void fingerprintFiberLoss(std::uint64_t& hash,const Geant4OpticsConfig& c) {
    if(!usesKurarayFiberLoss(c))return;
    std::ostringstream s;s<<std::setprecision(17)<<transmissiondata::version
      <<";host_anchor_nm="<<fiberHostAnchorNm<<";blue_join_nm="<<fiberBlueJoinNm
      <<";reference_ppm="<<fiberLossReferenceConcentrationPpm<<";Y11=mean_rates;near_axis_reference;constant_host_endpoints;log_alpha_join;";
    for(const auto& p:transmissiondata::ys4)s<<p.first<<','<<p.second<<';';
    for(const auto& p:transmissiondata::ys6)s<<p.first<<','<<p.second<<';';
    for(unsigned char ch:s.str()){hash^=ch;hash*=1099511628211ULL;}
}
} // namespace mine
