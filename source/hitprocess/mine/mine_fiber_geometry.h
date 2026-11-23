#ifndef MINE_FIBER_GEOMETRY_H
#define MINE_FIBER_GEOMETRY_H
#include "mine_parameters.h"
#include "mine_fiber_layouts.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>
namespace mine {
// Effective centerlines only: NO Geant4 optical volumes or ray tracing.
// Layouts 1,3,4,5,6 mirror MC_MINE_LightTrapping (2026-10-19); 2 keeps the legacy layout.
// Nearest-point capture is a placeholder for the MC's distribution of capture sites.
using FiberPath = std::vector<G4ThreeVector>;
namespace routing {
constexpr double pi = 3.14159265358979323846;
inline void append(FiberPath& path, const G4ThreeVector& point) {
    if (path.empty() || (point-path.back()).mag() > 1e-7) path.push_back(point);
    if (path.size() > 100000) throw std::invalid_argument("MINE fiber routing exceeds 100000 points");
}
inline void transition(FiberPath& path, G4ThreeVector a, G4ThreeVector b, double step) {
    if (b.y() <= a.y()) throw std::invalid_argument("MINE fan-in requires increasing Y");
    append(path, a);
    const double estimate = (b-a).mag()/step;
    if (!std::isfinite(estimate) || estimate > 100000) throw std::invalid_argument("MINE curve_step too small");
    const int n = std::max(2, static_cast<int>(std::ceil(estimate)));
    for (int i=1; i<=n; ++i) {
        const double u = static_cast<double>(i)/n, smooth = 0.5*(1-std::cos(pi*u));
        append(path, {a.x()+(b.x()-a.x())*smooth, a.y()+(b.y()-a.y())*u, a.z()+(b.z()-a.z())*u});
    }
}
inline void semicircle(FiberPath& path, double left, double right, double y, double z, bool up, double step) {
    const double radius = (right-left)/2;
    const double estimate = pi*radius/step;
    if (!std::isfinite(estimate) || estimate > 100000) throw std::invalid_argument("MINE curve_step too small");
    const int n = std::max(4, static_cast<int>(std::ceil(estimate)));
    for (int i=1; i<=n; ++i) {
        const double u=static_cast<double>(i)/n, angle=up ? pi*(1-u) : pi*(1+u);
        append(path, {(left+right)/2+radius*std::cos(angle), y+radius*std::sin(angle), z});
    }
}
inline void checkBends(const FiberPath& path, double minimum) {
    for (std::size_t i=1; i+1<path.size(); ++i) {
        const auto a=path[i]-path[i-1], b=path[i+1]-path[i];
        const double twiceArea=a.cross(b).mag();
        if (twiceArea < 1e-12) continue;
        const double radius=a.mag()*b.mag()*(a+b).mag()/(2*twiceArea);
        if (radius+1e-6 < minimum) throw std::invalid_argument("MINE fiber route violates minimum bend radius");
    }
}
}
inline std::vector<FiberPath> fiberPaths(const FiberParameters& p) {
    std::vector<FiberPath> paths;
    if (p.layout == 0) return paths;
    if (p.layout == 1) {
        paths = updated::fan::BuildParallelFanInPaths(
            p.parallelFibers, p.pitch, p.centerX, p.z, p.straightYMin, p.straightYMax,
            p.sensor0X, p.sensor0Y, p.sensor1X, p.sensor1Y, p.sensorPitch, p.minBendRadius, p.curveStep);
    } else if (p.layout == 2) {
        const double left=p.centerX-0.5*(2*p.lanesPerFiber-1)*p.pitch;
        for (int fiber=0; fiber<2; ++fiber) {
            const double offset=(fiber-0.5)*p.sensorPitch, firstX=left+fiber*p.pitch;
            FiberPath path;
            routing::transition(path, {p.sensor0X+offset,p.sensor0Y,p.z}, {firstX,p.straightYMin,p.z}, p.curveStep);
            bool up=true;
            for (int lane=0; lane<p.lanesPerFiber; ++lane) {
                const double x=firstX+2*lane*p.pitch, y=up ? p.straightYMax : p.straightYMin;
                routing::append(path, {x,y,p.z});
                if (lane+1<p.lanesPerFiber) {
                    routing::semicircle(path,x,x+2*p.pitch,y,p.z,up,p.curveStep);
                    up=!up;
                }
            }
            // Copy the start: vector append may invalidate references to path.back().
            const auto last=path.back();
            routing::transition(path,last,{p.sensor1X+offset,p.sensor1Y,p.z},p.curveStep);
            paths.push_back(std::move(path));
        }
    } else if (p.layout == 3) {
        paths = updated::alternative::BuildDiagonalCornerFanPaths(
            p.cornerFibers,p.pitch,p.z,p.panelHalfX,p.panelHalfY,
            p.diagonal == 0 ? "bl_tr" : "tl_br",p.minBendRadius,p.curveStep);
    } else if (p.layout == 4) {
        paths = updated::alternative::BuildTripleSerpentinePaths(
            p.pitch,p.z,p.panelHalfX,p.panelHalfY,p.edgeMargin,p.sensorPitch,p.minBendRadius,p.curveStep);
    } else if (p.layout == 5) {
        paths = updated::panel::BuildPanelPathsOnly(
            p.pitch,p.z,p.panelHalfX,p.panelHalfY,p.orientation == 0 ? "vertical" : "horizontal",
            p.autoSensorPlacement,p.edgeMargin,p.sensor0X,p.sensor0Y,p.sensor1X,p.sensor1Y,
            p.sensorPitch,p.minBendRadius,p.curveStep);
    } else if (p.layout == 6) {
        paths.push_back({{p.centerX,-p.panelHalfY,p.z},{p.centerX,p.panelHalfY,p.z}});
    } else throw std::invalid_argument("MINE unknown fiber layout");
    for (const auto& path : paths) {
        routing::checkBends(path,p.minBendRadius);
        for (const auto& point : path)
            if (!std::isfinite(point.x()) || !std::isfinite(point.y()) || !std::isfinite(point.z()) ||
                std::abs(point.x()) > p.panelHalfX+1e-6 || std::abs(point.y()) > p.panelHalfY+1e-6)
                throw std::invalid_argument("MINE fiber centerline leaves the panel");
    }
    return paths;
}
struct FiberRoute {
    double bulkDistance=0, distanceToEnd=0, coordinate=0, totalLength=0;
    std::size_t fiberId=0;
    G4ThreeVector capturePoint, sensor;
};
inline FiberRoute nearestFiberRoute(const G4ThreeVector& source, const std::vector<FiberPath>& paths, int end) {
    if (paths.empty() || (end!=0 && end!=1)) throw std::invalid_argument("MINE invalid effective fiber route");
    FiberRoute best;
    best.bulkDistance=std::numeric_limits<double>::infinity();
    for (std::size_t f=0; f<paths.size(); ++f) {
        const auto& path=paths[f];
        double total=0;
        for (std::size_t i=1; i<path.size(); ++i) total+=(path[i]-path[i-1]).mag();
        double start=0;
        for (std::size_t i=1; i<path.size(); ++i) {
            const auto axis=path[i]-path[i-1];
            const double length=axis.mag();
            const double t=std::max(0.0,std::min(1.0,(source-path[i-1]).dot(axis)/(length*length)));
            const auto point=path[i-1]+t*axis;
            const double distance=(source-point).mag();
            if (distance<best.bulkDistance) {
                best.bulkDistance=distance; best.coordinate=start+t*length;
                best.totalLength=total; best.distanceToEnd=end==0 ? best.coordinate : total-best.coordinate;
                best.capturePoint=point; best.sensor=end==0 ? path.front() : path.back(); best.fiberId=f;
            }
            start+=length;
        }
    }
    if (!std::isfinite(best.bulkDistance)) throw std::invalid_argument("MINE non-finite capture position");
    return best;
}
}
#endif
