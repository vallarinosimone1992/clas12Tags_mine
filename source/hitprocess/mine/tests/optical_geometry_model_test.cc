#include "mine_optical_geometry_model.h"
#include "mine_groove_materials.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <stdexcept>

namespace {
double angle(const mine::OpticalPoint& a,const mine::OpticalPoint& b) {
    return std::acos(std::clamp(a.unit().dot(b.unit()),-1.,1.));
}
void near(double a,double b,double tolerance=1e-8) {assert(std::abs(a-b)<tolerance);}
}
int main(int argc,char** argv) {
    mine::Geant4OpticsConfig c;c.wlsAbsorptionLengthMm=0; // geometry-only test, deliberately no WLS
    auto m=mine::BuildCornerFanOpticalModel(c,500,500,5);
    assert(m.fibers.size()==15);near(m.mcToLocalZMm,-5);
    near(m.effectivePitchMm,94.11214325830903,1e-8);
    assert(m.effectiveInsetMm>10 && m.effectiveInsetMm<11);
    assert(m.readouts[0].widthMm>20 && m.readouts[0].widthMm<21);
    assert(m.fibers[7].straight);near(m.fibers[7].start.z,-4.5);
    for(std::size_t f=0;f<m.fibers.size();++f) {
        const auto& route=m.fibers[f];
        near(route.LengthMm(),m.fibers[m.fibers.size()-f-1].LengthMm());
        near((route.PointAt(0)-route.start).norm(),0);
        near((route.PointAt(route.LengthMm())-route.end).norm(),0);
        for(int s=0;s<2;++s) {
            const auto& plane=m.readouts[s];const auto point=s?route.end:route.start;
            near((point-plane.center).dot(plane.normal),0);
        }
        if(!route.straight) {
            near(route.radiusMm,101);
            const auto radial0=(route.tangent0-route.center).unit();
            const auto radial1=(route.tangent1-route.center).unit();
            near((route.tangent0-route.start).unit().dot(radial0),0);
            near((route.end-route.tangent1).unit().dot(radial1),0);
            near(angle(radial0,radial1),std::abs(route.deltaAngleRad));
        }
        for(int i=0;i<=2000;++i) {
            const auto p=route.PointAt(route.LengthMm()*i/2000.);
            assert(std::abs(p.x)<=499.999999 && std::abs(p.y)<=499.999999);
            assert(p.z>=-4.500000001 && p.z<=-4.499999999);
        }
    }
    c.diagonal="tl_br";auto other=mine::BuildCornerFanOpticalModel(c,500,500,5);
    for(std::size_t f=0;f<m.fibers.size();++f) near(m.fibers[f].LengthMm(),other.fibers[f].LengthMm());
    c.fiberCount=14;c.fiberZOffsetsMm.assign(14,.1);
    auto shifted=mine::BuildCornerFanOpticalModel(c,500,500,5);
    for(const auto& route:shifted.fibers) near(route.start.z,-4.4);
    c.fiberZOffsetsMm[0]=1;bool failed=false;
    try {mine::BuildCornerFanOpticalModel(c,500,500,5);} catch(const std::invalid_argument&) {failed=true;}
    assert(failed);
    mine::Geant4OpticsConfig missing;failed=false;
    try {missing.Validate();} catch(const std::invalid_argument&) {failed=true;}assert(failed);
    if(argc>1) {
        const auto diagnostic=mine::Geant4OpticsConfig::Load(argv[1]);
        assert(diagnostic.wlsAbsorptionLengthMm==0);
        assert(mine::BuildCornerFanOpticalModel(diagnostic,500,500,5).fibers.size()==15);
        assert(diagnostic.fingerprint!=0);
    }
    if(argc>2) {
        failed=false;
        try {mine::Geant4OpticsConfig::Load(argv[2]);} catch(const std::invalid_argument&) {failed=true;}
        assert(failed); // A shipped REQUIRED placeholder must never silently run.
    }
    mine::Geant4OpticsConfig g;g.wlsAbsorptionLengthMm=0;g.grooveModel="square";g.zMcMm=1;
    const auto grooves=mine::BuildCornerFanOpticalModel(g,500,500,5);
    for(const auto& route:grooves.fibers) near(route.start.z,-4);
    near(mine::grooveRefractiveIndex("air",425),1.000277);
    near(mine::grooveRefractiveIndex("gel",425),1.4338+10520/(425.*425.));
    near(mine::grooveRefractiveIndex("glue",425),1.57);
    for(int invalid=0;invalid<5;++invalid) {
        auto bad=g;
        if(invalid==0)bad.grooveMaterial="unknown";
        if(invalid==1)bad.grooveWidthMm=3;
        if(invalid==2)bad.zMcMm=.5;
        if(invalid==3)bad.grooveDepthMm=bad.grooveWidthMm=20;
        if(invalid==4)bad.fiberZOffsetsMm.assign(15,.1);
        failed=false;try {mine::BuildCornerFanOpticalModel(bad,500,500,5);} catch(const std::invalid_argument&) {failed=true;}
        assert(failed);
    }
    std::cout<<std::setprecision(12)<<"corner_fan model: fibers="<<m.fibers.size()<<" pitch_mm="<<m.effectivePitchMm
             <<" inset_mm="<<m.effectiveInsetMm<<" readout_width_mm="<<m.readouts[0].widthMm
             <<" lengths_mm="<<m.fibers[7].LengthMm()<<","<<m.fibers.front().LengthMm()<<"; PASS\n";
}
