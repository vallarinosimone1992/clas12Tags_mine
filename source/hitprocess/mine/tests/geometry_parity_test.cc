// Optional cross-project parity test. Link the three original geometry .cpp
// files and Simulation.cpp from MC_MINE_LightTrapping; see README instructions.
#include "mine_fiber_geometry.h"
#include "LightTrapping/FiberGeometry.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>

std::unique_ptr<LightTrapping::FiberGeometry> reference(const mine::FiberParameters& p) {
    using namespace LightTrapping;
    if (p.layout == 1) return std::make_unique<ParallelFanInGeometry>(p.parallelFibers,p.pitch,p.centerX,p.z,
        p.straightYMin,p.straightYMax,p.sensor0X,p.sensor0Y,p.sensor1X,p.sensor1Y,p.sensorPitch,p.minBendRadius,p.curveStep);
    if (p.layout == 2) return std::make_unique<DoubleSerpentineGeometry>(p.lanesPerFiber,p.pitch,p.centerX,p.z,
        p.straightYMin,p.straightYMax,p.sensor0X,p.sensor0Y,p.sensor1X,p.sensor1Y,p.sensorPitch,p.minBendRadius,p.curveStep);
    if (p.layout == 3) return std::make_unique<DiagonalCornerFanGeometry>(p.cornerFibers,p.pitch,p.z,
        p.panelHalfX,p.panelHalfY,p.diagonal==0 ? "bl_tr" : "tl_br",p.minBendRadius,p.curveStep);
    if (p.layout == 4) return std::make_unique<TripleSerpentineGeometry>(p.pitch,p.z,p.panelHalfX,p.panelHalfY,
        p.edgeMargin,p.sensorPitch,p.minBendRadius,p.curveStep);
    if (p.layout == 5) return std::make_unique<PanelDoubleSerpentineGeometry>(p.pitch,p.z,p.panelHalfX,p.panelHalfY,
        p.orientation==0 ? "vertical" : "horizontal",p.autoSensorPlacement,p.edgeMargin,
        p.sensor0X,p.sensor0Y,p.sensor1X,p.sensor1Y,p.sensorPitch,p.minBendRadius,p.curveStep);
    return std::make_unique<StraightFiberGeometry>(p.centerX,p.z,-p.panelHalfY,p.panelHalfY);
}
std::size_t compare(const std::string& name, const mine::FiberParameters& p) {
    const auto ref=reference(p);
    const auto paths=mine::fiberPaths(p);
    assert(paths.size()==ref->FiberCount());
    std::size_t n=0;
    for(std::size_t f=0;f<paths.size();++f) {
        const auto points=ref->CenterlinePoints(f);
        assert(paths[f].size()==points.size());
        double length=0;
        for(std::size_t i=0;i<points.size();++i) {
            const auto& a=paths[f][i]; const auto& b=points[i];
            assert(std::abs(a.x()-b.x)<1e-9 && std::abs(a.y()-b.y)<1e-9 && std::abs(a.z()-b.z)<1e-9);
            if(i) length+=(a-paths[f][i-1]).mag();
            ++n;
        }
        assert(std::abs(length-ref->LengthMm(f))<1e-8);
        for(int end=0;end<2;++end) {
            const auto& actual=end==0 ? paths[f].front() : paths[f].back();
            const auto expected=ref->End(f,end);
            assert(expected.sensorId==end);
            assert(std::abs(actual.x()-expected.position.x)<1e-9);
            assert(std::abs(actual.y()-expected.position.y)<1e-9);
        }
    }
    for (const auto& source : {G4ThreeVector{0,0,0},G4ThreeVector{123,-234,0.5},G4ThreeVector{-333,222,-2}}) {
        const auto a=mine::nearestFiberRoute(source,paths,0), b=mine::nearestFiberRoute(source,paths,1);
        assert(a.fiberId==b.fiberId);
        assert(std::abs(a.distanceToEnd+b.distanceToEnd-a.totalLength)<1e-8);
    }
    std::cout<<name<<": "<<paths.size()<<" fibers, "<<n<<" matching vertices\n";
    return n;
}
int main() {
    std::size_t vertices=0;
    for(int layout=1;layout<=6;++layout) {
        auto p=mine::parametersFromMap({{"mine_fiber_layout",layout}}).fiber;
        vertices+=compare("layout "+std::to_string(layout),p);
    }
    auto p=mine::parametersFromMap({{"mine_fiber_layout",3},{"mine_corner_diagonal",1}}).fiber;
    vertices+=compare("corner tl_br",p);
    p=mine::parametersFromMap({{"mine_fiber_layout",3},{"mine_corner_fibers",6},{"mine_fiber_pitch",80},
        {"mine_min_bend_radius",50},{"mine_curve_step",3}}).fiber;
    vertices+=compare("corner custom",p);
    p=mine::parametersFromMap({{"mine_fiber_layout",5},{"mine_serpentine_orientation",1}}).fiber;
    vertices+=compare("panel double horizontal",p);
    p.panelHalfX=600; p.panelHalfY=450;
    vertices+=compare("panel double rectangular",p);
    p=mine::parametersFromMap({{"mine_fiber_layout",5},{"mine_auto_sensor_placement",0},
        {"mine_sensor0_x",-400},{"mine_sensor0_y",-500},{"mine_sensor1_x",400},{"mine_sensor1_y",500}}).fiber;
    vertices+=compare("panel double manual",p);
    p=mine::parametersFromMap({{"mine_fiber_layout",1},{"mine_parallel_fibers",10},
        {"mine_straight_ymin",-250},{"mine_straight_ymax",250},{"mine_sensor0_y",-500},{"mine_sensor1_y",500}}).fiber;
    vertices+=compare("quick_parallel.conf",p);
    std::cout<<"Geometry parity passed: "<<vertices<<" vertices compared against original MC code\n";
}
