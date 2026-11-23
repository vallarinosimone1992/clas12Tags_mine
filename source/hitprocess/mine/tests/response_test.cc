#include "mine_response.h"
#include "mine_fiber_geometry.h"
#include "mine_sipm_pde.h"
#include "Randomize.hh"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
bool close(double a, double b, double tolerance = 1e-10) {
    return std::abs(a-b) <= tolerance*std::max(1.0, std::abs(b));
}
template<class F> void rejects(F f) {
    bool failed=false;
    try { f(); } catch(const std::invalid_argument&) { failed=true; }
    assert(failed);
}
}
int main() {
    using namespace mine;
    using namespace CLHEP;
    HepRandom::setTheSeed(12345);
    MINEParameters p; p.useOpticalTable=false;
    assert(close(p.scintillator.lightYield,8000/MeV));
    assert(close(p.scintillator.decayTime,2.5*ns));
    assert(close(p.scintillator.bulkAttenuationLength,1000*mm));
    assert(close(p.scintillator.propagationVelocity,170*mm/ns));
    assert(!p.sipm.ideal_SiPM && p.sipm.pixels==1600);
    assert(close(p.sipm.darkCountRate,0.0005/ns));
    assert(close(p.sipm.crossTalkProbability,0.07));
    const double edep=2*MeV, length=10*mm;
    const double visible=edep/(1+p.scintillator.birks*edep/length);
    assert(close(birksVisibleEnergy(edep,length,-1,p.scintillator.birks),visible));
    assert(close(birksVisibleEnergy(edep,length,0,p.scintillator.birks),edep));
    assert(close(birksVisibleEnergy(edep,0,-1,p.scintillator.birks),edep));
    HitInfo hit;
    hit.readoutLocal={500*mm,0,0};
    hit.steps.push_back({edep,length,3*ns,-1,13,{0,0,0},{0,0,0}});
    const auto scint=scintillate(hit,p.scintillator);
    assert(close(scint.evis,visible));
    assert(close(scint.photons,visible*p.scintillator.lightYield));
    // Legacy point collection has an exactly calculable 500 mm bulk path.
    const auto point=collectLight(scint,hit.readoutLocal,p.scintillator,p.collection);
    assert(close(point.distance,500*mm));
    assert(close(point.photons,scint.photons*p.collection.efficiency*std::exp(-0.5)));
    assert(close(point.packets.front().time,3*ns+500*mm/p.scintillator.propagationVelocity));
    // Geometry determines capture point and arc length, independently of old anchor.
    const auto paths=fiberPaths(p.fiber);
    assert(paths.size()==5);
    const auto near0=nearestFiberRoute({0,20*mm,0},paths,0);
    const auto near1=nearestFiberRoute({0,20*mm,0},paths,1);
    assert(close(near0.bulkDistance,0.5*mm));
    assert(close(near0.distanceToEnd,510*mm));
    assert(close(near1.distanceToEnd,470*mm));
    assert(close(near0.distanceToEnd+near1.distanceToEnd,near0.totalLength));
    assert(close(near0.sensor.y(),-490*mm));
    const auto collected=collectLight(scint,hit.readoutLocal,p.scintillator,p.collection,&p.fiber);
    assert(close(collected.distance,0.5*mm));
    assert(close(collected.packets.front().fiberDistance,490*mm));
    FiberParameters f=p.fiber;
    const auto direct=transportDirectFiber(collected,f), wls=transportWLS(collected,f);
    assert(close(direct.photons,collected.photons*std::exp(-490*mm/f.attenuationLength)));
    assert(close(wls.photons,direct.photons*0.031));
    assert(close(wls.packets.front().wavelengthNm,476));
    assert(close(wls.packets.front().transportDecay,6.9*ns));
    assert(close(direct.packets.front().time,collected.packets.front().time+490*mm/f.propagationVelocity));
    f.mode=ReadoutMode::Reference;
    const auto referenceCollection=collectLight(scint,hit.readoutLocal,p.scintillator,p.collection,&f);
    assert(close(referenceCollection.photons,collected.photons));
    assert(close(referenceCollection.distance,collected.distance));
    assert(close(transportLight(collected,f).photons,collected.photons));
    f=p.fiber; f.layout=2;
    const auto serp=fiberPaths(f);
    assert(serp.size()==2);
    const auto serp0=nearestFiberRoute({-250,-100,0.5},serp,0);
    const auto serp1=nearestFiberRoute({-250,-100,0.5},serp,1);
    assert(close(serp0.distanceToEnd+serp1.distanceToEnd,serp0.totalLength));
    assert(serp0.totalLength>2000*mm);
    f.minBendRadius=1000*mm;
    rejects([&]{fiberPaths(f);});
    // New layouts: corner fillets, same-edge triple readout, panel-driven routing.
    auto corner=parametersFromMap({{"mine_fiber_layout",3}});
    corner.useOpticalTable=false;
    const auto cornerPaths=fiberPaths(corner.fiber);
    assert(cornerPaths.size()==14);
    assert(close(cornerPaths.front().front().x(),-500) && close(cornerPaths.front().front().y(),-500));
    assert(close(cornerPaths.back().back().x(),500) && close(cornerPaths.back().back().y(),500));
    const auto corner0=nearestFiberRoute({0,0,0},cornerPaths,0), corner1=nearestFiberRoute({0,0,0},cornerPaths,1);
    assert(close(corner0.distanceToEnd+corner1.distanceToEnd,corner0.totalLength));
    assert(corner0.totalLength>1400 && corner0.totalLength<2000);
    auto triple=parametersFromMap({{"mine_fiber_layout",4}});
    const auto triplePaths=fiberPaths(triple.fiber);
    assert(triplePaths.size()==3 && close(triple.fiber.pitch,80));
    for(const auto& path:triplePaths) assert(close(path.front().y(),-500) && close(path.back().y(),-500));
    auto panel=parametersFromMap({{"mine_fiber_layout",5}});
    const auto panelPaths=fiberPaths(panel.fiber);
    assert(panelPaths.size()==2 && panelPaths.front().size()>10);
    rejects([]{parametersFromMap({{"mine_fiber_layout",3},{"mine_corner_fibers",13}});});
    rejects([]{parametersFromMap({{"mine_corner_diagonal",2}});});
    rejects([&]{auto invalid=corner.fiber; invalid.pitch=200; fiberPaths(invalid);});
    rejects([&]{auto invalid=corner.fiber; invalid.panelHalfY=400; fiberPaths(invalid);});
    rejects([&]{auto invalid=triple.fiber; invalid.pitch=100; fiberPaths(invalid);});
    rejects([&]{auto invalid=panel.fiber; invalid.edgeMargin=500; fiberPaths(invalid);});
    auto smallHit=hit; smallHit.panelHalfSize={400,400,5};
    auto straight=parametersFromMap({{"mine_fiber_layout",6}});
    straight.useOpticalTable=false;
    const auto smallResponse=simulate(smallHit,straight);
    assert(close(smallResponse.transport.distance,400));
    rejects([&]{auto invalid=smallHit; invalid.panelHalfSize.setZ(0.1); simulate(invalid,corner);});
    // Profile selection precedes overrides, regardless of alphabetical key order.
    auto ys=parametersFromMap({{"mine_fiber_model",1}});
    assert(close(ys.fiber.wlsDecayTime,1.47*ns));
    assert(close(ys.fiber.attenuationLength,3000*mm));
    assert(close(ys.fiber.emissionWavelengthNm,462));
    ys=parametersFromMap({{"mine_fiber_model",1},{"mine_fiber_attenuation",4100},{"mine_wls_decay_time",2}});
    assert(close(ys.fiber.attenuationLength,4100*mm) && close(ys.fiber.wlsDecayTime,2*ns));
    assert(!parametersFromMap({{"mine_pde",0.2}}).sipm.useSpectralPDE);
    rejects([]{parametersFromMap({{"ideal_SiPM",0.5}});});
    rejects([]{parametersFromMap({{"mine_wls_efficiency",0.5}});});
    rejects([]{parametersFromMap({{"mine_readout_end",2}});});
    rejects([]{parametersFromMap({{"mine_wls_trapping_efficiency",0.6}});});
    assert(close(spectralPDE(450),0.497));
    assert(close(spectralPDE(476),0.49536));
    assert(close(spectralPDE(462.5),0.4965));
    assert(spectralPDE(260)==0 && spectralPDE(910)==0);
    rejects([]{spectralPDE(0);});
    TransportResponse fixed;
    fixed.photons=200;
    fixed.packets.push_back({200,3*ns,0,0,{0,0,0},0,476});
    const auto sensor=respondSiPM(fixed,3*ns,p.sipm,p.electronics);
    assert(close(sensor.meanNpe,200*spectralPDE(476)));
    // Ideal mode overrides ALL configured losses, finite pixels and secondary noise.
    auto ideal=p.sipm;
    ideal.ideal_SiPM=true; ideal.pde=0; ideal.pixels=1;
    ideal.crossTalkProbability=1; ideal.afterpulseProbability=1; ideal.darkCountRate=1;
    const auto idealResponse=respondSiPM(fixed,3*ns,ideal,p.electronics);
    assert(close(idealResponse.meanNpe,200));
    assert(idealResponse.fired==idealResponse.npe);
    assert(idealResponse.dark==0 && idealResponse.crossTalk==0 && idealResponse.afterpulses==0);
    // Meaningful statistical check of primary PE mean/variance, not a single draw.
    auto simple=p.sipm;
    simple.useSpectralPDE=false; simple.pde=0.25; simple.pixels=0;
    simple.crossTalkProbability=0; simple.darkCountRate=0;
    constexpr int trials=10000;
    double sum=0, squares=0;
    for(int i=0;i<trials;++i) {
        const auto r=respondSiPM(fixed,3*ns,simple,p.electronics);
        sum+=r.npe; squares+=r.npe*r.npe;
    }
    const double mean=sum/trials, variance=squares/trials-mean*mean;
    assert(std::abs(mean-50)<0.6 && std::abs(variance-50)<3);
    // Check nominal real-sensor cross-talk and dark rate in the linear limit.
    simple.crossTalkProbability=p.sipm.crossTalkProbability;
    simple.darkCountRate=p.sipm.darkCountRate;
    double primaries=0, cross=0, dark=0;
    for(int i=0;i<2000;++i) {
        const auto r=respondSiPM(fixed,3*ns,simple,p.electronics);
        primaries+=r.npe; cross+=r.crossTalk; dark+=r.dark;
    }
    assert(std::abs(cross/primaries-0.07)<0.01);
    assert(std::abs(dark/2000-0.0525)<0.02);
    simple.crossTalkProbability=0; simple.darkCountRate=0;
    // Saturation is active in real mode and bypassed in ideal mode.
    simple.pixels=1;
    const auto saturated=respondSiPM(fixed,3*ns,simple,p.electronics);
    assert(saturated.fired<=1 && saturated.npe>1);
    // Full chain remains stable for zero-PE/subthreshold events. Do not require
    // every 2 MeV hit to cross threshold with the uncalibrated capture efficiency.
    const auto response=simulate(hit,p);
    assert(response.transport.photons>0 && response.collection.distance<1*mm);
    assert(close(response.transport.distance,490*mm));
    assert(response.digitized.adc>=0);
    assert(response.waveform.time.size()==response.waveform.amplitude.size());
    auto bright=p; bright.collection.efficiency=1;
    const auto crossing=simulate(hit,bright);
    assert(crossing.digitized.adc>0 && crossing.digitized.timingValid && crossing.digitized.tdc>=3*ns);
    std::cout << "MINE response tests passed; Poisson mean=" << mean << ", variance=" << variance << '\n';
}
