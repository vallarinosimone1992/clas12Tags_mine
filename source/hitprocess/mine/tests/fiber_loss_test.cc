#include "mine_fiber_loss.h"
#include <cassert>
#include <fstream>
#include <iomanip>
#include <iostream>
int main(int argc,char** argv) {
    assert(argc==5);
    std::ofstream output(argv[4]);output<<std::setprecision(17)<<"profile,wavelength_nm,reference_db_per_km,ordinary_rate_per_mm,wls_rate_per_mm\n";
    for(int i=1;i<=3;++i) {
        auto c=mine::Geant4OpticsConfig::Load(argv[i]);assert(mine::usesKurarayFiberLoss(c));
        const double first=mine::fiberLossSupportMinNm(c.fiberModel),last=mine::fiberLossSupportMaxNm(c.fiberModel);
        for(double lambda=first;lambda<=last;lambda+=.125) {
            const double ordinary=mine::fiberOrdinaryRatePerMm(c.fiberModel,lambda),wls=mine::fiberWlsRatePerMm(c,lambda);
            const double target=mine::dbPerKmToRatePerMm*mine::fiberReferenceLossDbPerKm(c.fiberModel,lambda);
            assert(ordinary>0 && wls>0);
            assert(std::abs((ordinary+wls)/target-1)<1e-8);
        }
        for(double lambda:{350.,400.,425.,470.,480.}) {
            assert(std::abs(mine::fiberWlsRatePerMm(c,lambda)*mine::wlsAbsorptionLengthMm(c.fiberModel,lambda,200)-1)<1e-12);
        }
        auto invalid=c;invalid.effectiveFiberAttenuationMm=4000;bool rejected=false;
        try{invalid.Validate();}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
        invalid=c;invalid.claddingAbsorptionLengthMm=10000;rejected=false;
        try{invalid.Validate();}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
        for(double lambda=300;lambda<=900;lambda+=.5)
            output<<c.fiberModel<<','<<lambda<<','<<mine::fiberReferenceLossDbPerKm(c.fiberModel,lambda)<<','<<mine::fiberOrdinaryRatePerMm(c.fiberModel,lambda)<<','<<mine::fiberWlsRatePerMm(c,lambda)<<'\n';
        std::cout<<"SPECTRAL_LOSS_UNIT_PASS profile="<<c.fiberModel<<" support_nm="<<first<<','<<last<<" host_L550_m="<<.001/mine::fiberOrdinaryRatePerMm(c.fiberModel,550)<<'\n';
    }
    for(double lambda:{500.,520.,550.,610.,650.,699.}) {
        const double mean=.5*(mine::fiberReferenceLossDbPerKm("YS-4",lambda)+mine::fiberReferenceLossDbPerKm("YS-6",lambda));
        assert(std::abs(mine::fiberReferenceLossDbPerKm("Y-11",lambda)-mean)<1e-10);
    }
    assert(output.good());
}
