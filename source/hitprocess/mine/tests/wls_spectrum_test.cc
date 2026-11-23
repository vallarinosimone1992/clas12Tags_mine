#include "mine_wls_spectrum.h"
#include "mine_optical_geometry_model.h"
#include <cassert>
#include <iostream>
int main(int argc,char** argv) {
    using namespace mine;
    assert(std::abs(y11AbsorptionLengthMm(430,200)-1/(.0147*200))<1e-10);
    assert(std::abs(y11AbsorptionLengthMm(425,100)/y11AbsorptionLengthMm(425,200)-2)<1e-12);
    assert(y11AbsorptionLengthMm(480,200)<100); // Resolved green reabsorption.
    assert(y11AbsorptionLengthMm(500,200)==unresolvedWlsLengthMm);
    assert(y11EmissionPerEV(476)>0 && y11EmissionPerEV(700)==0);
    assert(std::abs(y11EmissionPerEV(520)/spectralOrdinate(y11data::emission,520)-std::pow(520./476,2))<1e-12);
    assert(std::abs(y11EmissionPerEV(520,true)/y11EmissionPerEV(520)-520./476)<1e-12);
    const auto vectors=y11PropertyVectors(200);
    for(std::size_t i=0;i<vectors.energyEv.size();++i) {
        assert(i==0 || vectors.energyEv[i]>vectors.energyEv[i-1]);
        assert(std::isfinite(vectors.absorptionLengthMm[i]) && vectors.absorptionLengthMm[i]>0);
        assert(std::isfinite(vectors.emissionPerEV[i]) && vectors.emissionPerEV[i]>=0);
    }
    for(const auto& fiber:{"Y-11","YS-6","YS-4"}) {
        const auto v=wlsPropertyVectors(fiber,200);
        for(std::size_t j=0;j<v.energyEv.size();++j) {
            assert(j==0 || v.energyEv[j]>v.energyEv[j-1]);
            assert(v.absorptionLengthMm[j]>0 && std::isfinite(v.absorptionLengthMm[j]));
            assert(v.emissionPerEV[j]>=0 && std::isfinite(v.emissionPerEV[j]));
        }
        assert(wlsAbsorptionLengthMm(fiber,600,200)==unresolvedWlsLengthMm);
    }
    assert(wlsProfileId("YS6")==1 && wlsProfileId("YS4")==2);
    assert(wlsProfileDecayNs("YS-6")==1.3 && wlsProfileDecayNs("YS-4")==1.4);
    assert(wlsRelativeEmission("YS-6",462)>wlsRelativeEmission("YS-6",476));
    assert(wlsRelativeEmission("YS-4",470)>wlsRelativeEmission("YS-4",490));
    bool rejected=false;try { y11AbsorptionLengthMm(430,0); } catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected);
    if(argc>1) {
        auto config=Geant4OpticsConfig::Load(argv[1]);assert(config.model=="spectral_y11");
        const auto hash=config.fingerprint;assert(hash>0);
        config.wlsDyeConcentrationPpm=0;rejected=false;
        try {config.Validate();} catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
    }
    if(argc>2) assert(Geant4OpticsConfig::Load(argv[2]).model=="monochromatic_effective");
    std::uint64_t previous=0;
    for(int i=3;i<argc;++i) {
        const auto c=Geant4OpticsConfig::Load(argv[i]);
        assert(isSpectralWlsModel(c.model));
        assert(c.effectiveFiberAttenuationMm==0 && c.fiberLossModel=="kuraray_spectral");
        assert(c.fingerprint>0 && c.fingerprint!=previous);previous=c.fingerprint;
        assert(c.wlsDecayNs==wlsProfileDecayNs(c.fiberModel));
        auto invalid=c;invalid.fiberModel="YS-3";bool bad=false;
        try{invalid.Validate();}catch(const std::invalid_argument&){bad=true;}assert(bad);
    }
    std::cout<<"Y11 spectral unit checks passed; L430="<<y11AbsorptionLengthMm(430,200)
             <<" mm; L425="<<y11AbsorptionLengthMm(425,200)<<" mm; L480="<<y11AbsorptionLengthMm(480,200)<<" mm\n";
}
