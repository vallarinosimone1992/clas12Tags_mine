// Optional read-only comparison against the current reference MC sources.
// Include path and MC object/library paths are supplied at build time; GEMC
// does not depend on that project at runtime.
#include "mine_optical_geometry_model.h"
#include "LightTrapping/FiberGeometry.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    double maxPositionError=0,maxLengthDifference=0;
    for(const auto diagonal:{"bl_tr","tl_br"}) for(int count:{1,7,14,15}) {
        mine::Geant4OpticsConfig c;c.wlsAbsorptionLengthMm=0;c.diagonal=diagonal;c.fiberCount=count;
        const auto analytic=mine::BuildCornerFanOpticalModel(c,500,500,5);
        const LightTrapping::DiagonalCornerFanGeometry mc(count,100,.5,500,500,diagonal,100,2,10,20,1.5,1,true);
        assert(mc.FiberCount()==analytic.fibers.size());
        assert(std::abs(mc.CentrePitchMm()-analytic.effectivePitchMm)<1e-10);
        assert(std::abs(mc.ReadoutInsetMm()-analytic.effectiveInsetMm)<1e-10);
        for(std::size_t f=0;f<mc.FiberCount();++f) {
            const auto& route=analytic.fibers[f];
            maxLengthDifference=std::max(maxLengthDifference,std::abs(route.LengthMm()-mc.LengthMm(f)));
            for(const auto& p:mc.CenterlinePoints(f)) {
                const mine::OpticalPoint point{p.x,p.y,p.z-5};
                auto lineDistance=[&](mine::OpticalPoint a,mine::OpticalPoint b) {
                    const auto d=b-a;
                    return (point-a-d*std::clamp((point-a).dot(d)/d.dot(d),0.,1.)).norm();
                };
                double distance;
                if(route.straight) distance=lineDistance(route.start,route.end);
                else {
                    constexpr double twoPi=6.2831853071795864769;
                    const auto radial=point-route.center;
                    double delta=std::atan2(radial.y,radial.x)-route.startAngleRad;
                    if(route.deltaAngleRad>0) {while(delta<0) delta+=twoPi;while(delta>=twoPi) delta-=twoPi;}
                    else {while(delta>0) delta-=twoPi;while(delta<=-twoPi) delta+=twoPi;}
                    double arcDistance=std::min((point-route.tangent0).norm(),(point-route.tangent1).norm());
                    if(std::abs(delta)<=std::abs(route.deltaAngleRad)+1e-12)
                        arcDistance=std::abs(radial.norm()-route.radiusMm);
                    distance=std::min({lineDistance(route.start,route.tangent0),lineDistance(route.tangent1,route.end),arcDistance});
                }
                maxPositionError=std::max(maxPositionError,distance);
            }
            for(int end=0;end<2;++end) {
                const auto expected=mc.End(f,end).position;
                const auto actual=end?route.end:route.start;
                assert((actual-mine::OpticalPoint{expected.x,expected.y,expected.z-5}).norm()<1e-9);
            }
        }
    }
    assert(maxPositionError<1e-9);
    // MC uses 2 mm chords; the analytic toroidal path is slightly longer.
    assert(maxLengthDifference<.003);
    std::cout<<"MC/Geant4 analytic corner_fan: maximum position error_mm="<<maxPositionError
             <<" maximum arc/chord length difference_mm="<<maxLengthDifference<<"; PASS\n";
}
