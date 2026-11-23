#include "mine_response.h"
#include "mine_optical_response.h"
#include "Randomize.hh"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>

namespace {
using namespace mine;
LightTrapping::OpticalResponseTable Fixture() {
    LightTrapping::OpticalResponseTable t;
    t.xMm={-250,250};t.yMm={-250,250};t.xMinMm=t.yMinMm=-500;t.xMaxMm=t.yMaxMm=500;
    t.metadata={{"gemc_contract_version","1"},{"sensor_count","2"},{"fiber_type","wls"},{"fiber_model","Y-11"},
      {"fiber_geometry","diagonal_corner_fan"},{"number_of_fibers","15"},
      {"timing_reference","arrivalTimeNs - productionTimeNs; ns; full optical transport, no direct-flight subtraction"},
      {"scintillation_decay_included","false"},{"sipm_and_electronics_included","false"},
      {"scintillator_half_size_x_mm","500"},{"scintillator_half_size_y_mm","500"},{"scintillator_thickness_mm","10"},
      {"wls_decay_included","true"},{"wls_decay_ns","6.9"},{"additional_wls_decay_ns","0"},
      {"readout_wavelength_nm","476"},{"bulk_attenuation_mm","1000"},{"fiber_attenuation_mm","3500"},
      {"wall_limit_photons","0"},{"fiber_limit_photons","0"}};
    t.cells.resize(4);
    for(auto& cell:t.cells) {
        cell.generated=1000;
        for(int sensor=0;sensor<2;++sensor) {
            auto& r=cell.sensors[sensor];r.arrived=100*(sensor+1);r.sumWeight=r.sumWeightSquared=r.arrived;
            r.timeNs={2.,120.};r.cdf={.5,1};
        }
    }
    return t;
}
bool Near(double a,double b,double eps=1e-10) {return std::abs(a-b)<eps;}
template<class F> void Reject(F fn) {bool yes=false;try{fn();}catch(const std::exception&){yes=true;}assert(yes);}
}
int main(int argc,char** argv) {
    using namespace mine;using namespace CLHEP;
    CLHEP::HepRandom::setTheSeed(93821);
    auto raw=Fixture();TabulatedOptics optics(raw);
    MINEParameters p;p.sipm.ideal_SiPM=true;p.scintillator.birks=0;p.scintillator.lightYield=1000/MeV;
    p.scintillator.decayTime=0;
    HitInfo hit;hit.steps.push_back({1*MeV,1*mm,3*ns,1,13,{0,0,0},{0,0,0}});
    // Deliberately extreme legacy collection/trapping/distance/velocity settings
    // must not affect tabulated photons or transport delays.
    p.collection.efficiency=0;p.fiber.trappingEfficiency=0;p.scintillator.bulkAttenuationLength=.001*mm;
    p.fiber.attenuationLength=.001*mm;p.scintillator.propagationVelocity=.001*mm/ns;
    p.fiber.propagationVelocity=.001*mm/ns;p.fiber.wlsDecayTime=900*ns;p.fiber.emissionWavelengthNm=900;
    const auto response=simulateTabulated(hit,p,optics);
    assert(response.tabulated && response.fiberCount==15 && response.fiberLayout==3 && response.fiberModel==0);
    assert(Near(response.sensors[0].transport.photons,100) && Near(response.sensors[1].transport.photons,200));
    for(int sensor=0;sensor<2;++sensor) {
        const auto& channel=response.sensors[sensor];bool early=false,late=false;
        assert(Near(channel.sipm.meanNpe,100*(sensor+1)));
        assert(channel.sipm.fired==channel.sipm.npe && channel.sipm.dark==0);
        for(const auto& pulse:channel.sipm.pulses) {
            assert(Near(pulse.time,5*ns) || Near(pulse.time,123*ns));
            early|=Near(pulse.time,5*ns);late|=Near(pulse.time,123*ns);
        }
        assert(early && late); // Full tail retained; independent draw within same step.
    }
    assert(response.bothTimingValid);
    assert(Near(response.meanTdc,(response.sensors[0].digitized.tdc+response.sensors[1].digitized.tdc)/2));
    assert(Near(response.differenceTdc,response.sensors[1].digitized.tdc-response.sensors[0].digitized.tdc));
    assert(Near(response.sipm.meanNpe,response.sensors[0].sipm.meanNpe));
    // One missing sensor produces no fabricated combined time.
    auto one=raw;for(auto& c:one.cells)c.sensors[1]={};TabulatedOptics oneSensor(one);
    const auto single=simulateTabulated(hit,p,oneSensor);
    assert(!single.bothTimingValid && single.meanTdc==-1 && !single.sensors[1].digitized.timingValid);
    // Missing WLS decay is supplied by table metadata, not stale GEMC overrides.
    auto missing=raw;missing.metadata["wls_decay_included"]="false";missing.metadata["wls_decay_ns"]="0";
    missing.metadata["additional_wls_decay_ns"]="6.9";
    for(auto& c:missing.cells)for(auto& r:c.sensors){r.timeNs={2};r.cdf={1};}
    TabulatedOptics extra(missing);
    double delay=0;std::size_t n=0;
    for(int i=0;i<100;++i) {
        const auto r=simulateTabulated(hit,p,extra);
        for(const auto& pulse:r.sensors[0].sipm.pulses){assert(pulse.time>=5*ns);delay+=pulse.time-5*ns;++n;}
    }
    assert(std::abs(delay/n/ns-6.9)<.3);
    // Clear/YS-6 and geometry changes need only another table.
    auto clear=raw;clear.metadata["fiber_type"]="clear";clear.metadata["fiber_model"]="clear";
    clear.metadata["wls_decay_included"]="false";clear.metadata["wls_decay_ns"]="0";
    clear.metadata["readout_wavelength_nm"]="425";clear.metadata["fiber_geometry"]="triple_serpentine";
    TabulatedOptics clearOptics(clear);assert(clearOptics.mode==ReadoutMode::DirectFiber && clearOptics.model==-1 && clearOptics.layout==4);
    auto ys=missing;ys.metadata["fiber_model"]="YS-6";ys.metadata["additional_wls_decay_ns"]="1.47";
    ys.metadata["readout_wavelength_nm"]="462";TabulatedOptics ysOptics(ys);
    assert(ysOptics.model==1 && Near(ysOptics.additionalWlsDecay,1.47*ns));
    auto incompatible=hit;incompatible.panelHalfSize.setZ(6);Reject([&]{simulateTabulated(incompatible,p,optics);});
    incompatible=hit;incompatible.steps[0].local.setX(500.1);Reject([&]{simulateTabulated(incompatible,p,optics);});
    auto bad=raw;bad.metadata.erase("gemc_contract_version");Reject([&]{TabulatedOptics t(bad);});
    bad=raw;bad.metadata["additional_wls_decay_ns"]="6.9";Reject([&]{TabulatedOptics t(bad);});
    bad=raw;bad.metadata["wall_limit_photons"]="1";Reject([&]{TabulatedOptics t(bad);});
    bad=raw;bad.xMinMm=-450;bad.xMaxMm=450;Reject([&]{TabulatedOptics t(bad);});
    bad=raw;bad.cells[0]={};Reject([&]{TabulatedOptics t(bad);});
    // Actual independent-sensor Poisson splitting preserves each mean and variance.
    double sums[2]={},squares[2]={},cross=0;constexpr int trials=2000;
    for(int i=0;i<trials;++i) {
        const auto r=simulateTabulated(hit,p,optics);double a=r.sensors[0].sipm.npe,b=r.sensors[1].sipm.npe;
        sums[0]+=a;sums[1]+=b;squares[0]+=a*a;squares[1]+=b*b;cross+=a*b;
    }
    for(int sensor=0;sensor<2;++sensor){double mean=sums[sensor]/trials,var=squares[sensor]/trials-mean*mean;assert(std::abs(mean-100*(sensor+1))<1.5);assert(std::abs(var-100*(sensor+1))<18);}
    assert(std::abs(cross/trials-sums[0]*sums[1]/(trials*trials))<12);
    if(argc>1) {
        const auto production=TabulatedOptics::Load(argv[1]);production.ValidatePanel(hit.panelHalfSize);
        p.scintillator.decayTime=2.5*ns;
        const auto r=simulateTabulated(hit,p,production);
        for(int sensor=0;sensor<2;++sensor) assert(Near(r.sensors[sensor].transport.photons,1000*production.Efficiency({0,0,0},sensor)));
        std::cout<<"Production table verified: "<<production.Text("fiber_geometry")<<", fibers="<<production.fiberCount
                 <<", fingerprint="<<production.fingerprint<<'\n';
    }
    std::cout<<"Tabulated response checks passed: photon means/statistics, independent full-tail timing, two sensors, WLS bookkeeping and compatibility rejection\n";
}
