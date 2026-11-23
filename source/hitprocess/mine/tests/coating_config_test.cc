#include "mine_optical_geometry_model.h"
#include "mine_coating_spectrum.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
int main(int argc,char** argv) {
    using namespace mine;
    assert(argc==5);
    auto specular=Geant4OpticsConfig::Load(argv[1]),diffuse=Geant4OpticsConfig::Load(argv[2]),tyvek=Geant4OpticsConfig::Load(argv[3]);
    assert(specular.coatingModel=="specular" && specular.coatingMaterial=="aluminized_mylar");
    assert(diffuse.coatingModel=="diffuse" && diffuse.coatingMaterial=="fnal_tio2");
    assert(tyvek.coatingModel=="diffuse" && tyvek.coatingMaterial=="tyvek");
    assert(specular.fingerprint!=diffuse.fingerprint && diffuse.fingerprint!=tyvek.fingerprint);
    assert(specular.scintillationSpectrum==diffuse.scintillationSpectrum);
    assert(std::abs(coatingReflectivity(diffuse,opticalHcEvNm/2.95)-.85)<1e-12); // Untuned source, not R'=1-.75(1-R).
    assert(std::abs(coatingReflectivity(diffuse,600)-.91)<1e-12);
    assert(coatingReflectivity(diffuse,350)==.07);
    assert(coatingReflectivity(specular,425)>.74 && coatingReflectivity(specular,425)<.77);
    assert(coatingReflectivity(tyvek,425)>.96 && coatingReflectivity(tyvek,425)<.98);
    auto scaled=tyvek;scaled.coatingReferenceAlbedo=.98;
    assert(std::abs(coatingReflectivity(scaled,425)/coatingReflectivity(tyvek,425)-.98)<1e-12);
    for(const auto& c:{specular,diffuse,tyvek}) {
        const auto v=coatingPropertyVector(c);
        for(std::size_t i=0;i<v.energyEv.size();++i) {
            assert(i==0 || v.energyEv[i]>v.energyEv[i-1]);
            assert(std::isfinite(v.reflectivity[i]) && v.reflectivity[i]>=0 && v.reflectivity[i]<=1);
        }
        for(double lambda:{350.,420.,425.,450.,550.,700.}) {
            const double energy=opticalHcEvNm/lambda;
            const auto upper=std::upper_bound(v.energyEv.begin(),v.energyEv.end(),energy);
            const auto j=std::size_t(upper-v.energyEv.begin());
            const double f=(energy-v.energyEv[j-1])/(v.energyEv[j]-v.energyEv[j-1]);
            const double interpolated=v.reflectivity[j-1]+f*(v.reflectivity[j]-v.reflectivity[j-1]);
            assert(std::abs(interpolated-coatingReflectivity(c,lambda))<1e-4);
        }
    }
    auto invalid=diffuse;invalid.coatingModel="specular";bool rejected=false;
    try {invalid.Validate();} catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
    const std::string root=argv[4];
    {std::ofstream out(root+"/coating-legacy.conf");out<<"model=monochromatic_effective\nwlsAbsorptionLengthMm=0\n";}
    auto legacy=Geant4OpticsConfig::Load(root+"/coating-legacy.conf");
    assert(legacy.coatingModel=="specular" && legacy.coatingMaterial=="constant" && legacy.reflectivity==.98);
    for(const std::string setting:{"coatingModel=typo","coatingModel=diffuse\ncoatingMaterial=typo",
        "coatingModel=diffuse\ncoatingMaterial=fnal_tio2\nreflectivity=0.98"}) {
        {std::ofstream out(root+"/coating-invalid.conf");out<<"model=monochromatic_effective\nwlsAbsorptionLengthMm=0\n"<<setting<<'\n';}
        rejected=false;try {Geant4OpticsConfig::Load(root+"/coating-invalid.conf");} catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
    }
    for(double r:{0.,1.}) {auto endpoint=legacy;endpoint.reflectivity=r;endpoint.Validate();assert(coatingReflectivity(endpoint,425)==r);}
    std::uint64_t h1=10,h2=10;fingerprintCoatingSpectrum(h1,"tyvek");fingerprintCoatingSpectrum(h2,"aluminized_mylar");assert(h1!=h2);
    std::cout<<"Coating checks passed: source spectra, untuned FNAL, probability interpolation, calibration, validation, legacy and hashes\n";
}
