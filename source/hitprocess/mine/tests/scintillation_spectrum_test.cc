#include "mine_scintillation_spectrum.h"
#include "mine_optical_geometry_model.h"
#include <cassert>
#include <iostream>

int main(int argc,char** argv) {
    using namespace mine;
    for(const std::string profile:{"fnal_extruded","ej200"}) {
        for(bool power:{false,true}) {
            const auto table=scintillationPropertyVector(profile,power);
            double areaE=0,momentE=0;
            for(std::size_t i=0;i<table.energyEv.size();++i) {
                assert(std::isfinite(table.emissionPerEV[i]) && table.emissionPerEV[i]>=0);
                if(i==0) continue;
                const double a=table.energyEv[i-1],b=table.energyEv[i],fa=table.emissionPerEV[i-1],fb=table.emissionPerEV[i];
                assert(b>a);
                areaE+=(b-a)*(fa+fb)/2;
                momentE+=(b-a)*(fa*opticalHcEvNm/a+fb*opticalHcEvNm/b)/2;
            }
            double area=0,moment=0;
            const double step=.001;
            for(double lambda=300+step/2;lambda<600;lambda+=step) {
                const double value=scintillationPerNm(profile,lambda)*(power?lambda/425.:1.);
                area+=value*step;moment+=value*lambda*step;
            }
            assert(area>0 && areaE>0);
            // Independent wavelength integral detects an omitted/reversed Jacobian.
            assert(std::abs(moment/area-momentE/areaE)<.003);
            const double mean=moment/area;
            assert(profile=="ej200" ? (mean>440 && mean<443) : (mean>402 && mean<408));
            std::cout<<"SCINT_SPECTRUM "<<profile<<" power="<<power<<" mean_nm="<<mean<<'\n';
        }
        assert(scintillationPerNm(profile,250)==0 && scintillationPerNm(profile,600)==0);
    }
    assert(scintillationPerNm("ej200",425)>.98);
    assert(scintillationPerNm("fnal_extruded",365)>.7); // Retain the measured PPO band.
    Geant4OpticsConfig c;c.model="spectral_y11";
    for(const std::string invalid:{"typo","EJ-200"}) {
        c.scintillationSpectrum=invalid;bool rejected=false;
        try {c.Validate();} catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
    }
    c.scintillationSpectrum="ej200";c.Validate();c.model="monochromatic_effective";c.wlsAbsorptionLengthMm=.34;
    bool rejected=false;try {c.Validate();} catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
    std::uint64_t hashA=10,hashB=10;
    fingerprintScintillationSpectrum(hashA,"fnal_extruded");fingerprintScintillationSpectrum(hashB,"ej200");assert(hashA!=hashB);
    if(argc==4) {
        const auto mono=Geant4OpticsConfig::Load(argv[1]),fnal=Geant4OpticsConfig::Load(argv[2]),ej=Geant4OpticsConfig::Load(argv[3]);
        assert(mono.scintillationSpectrum=="monochromatic" && fnal.scintillationSpectrum=="fnal_extruded" && ej.scintillationSpectrum=="ej200");
        assert(mono.fingerprint!=fnal.fingerprint && fnal.fingerprint!=ej.fingerprint);
        assert(fnal.lightYieldPerMeV==mono.lightYieldPerMeV && ej.scintillationDecayNs==mono.scintillationDecayNs);
    }
    std::cout<<"Scintillation spectral checks passed\n";
}
