#ifndef MINE_FIBER_LAYOUTS_H
#define MINE_FIBER_LAYOUTS_H
#include "mine_parameters.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>
namespace mine { namespace updated {
// Centerline builder snapshot from MC_MINE_LightTrapping, 2026-10-19.
// Adaptation: Vec3 -> G4ThreeVector; inline linkage; maximum point-count guard.
// No ray tracing, optical efficiencies or ROOT dependency is imported.
using Vec3 = G4ThreeVector;
inline double Norm(const Vec3& v) { return v.mag(); }
inline double Dot(const Vec3& a,const Vec3& b) { return a.dot(b); }
inline Vec3 Unit(const Vec3& v) {
    if (v.mag() <= 0) throw std::invalid_argument("MINE cannot normalize zero vector");
    return v*(1.0/v.mag());
}
namespace fan {
constexpr double kPi = 3.14159265358979323846;
constexpr double kEpsilon = 1e-7;
inline void AppendPoint(std::vector<Vec3>& path, const Vec3& point)
{
    if (!path.empty() && Norm(path.back() - point) < kEpsilon) {
        return;
    }
    if (path.size() >= 100000) throw std::invalid_argument("MINE route exceeds 100000 points");
    path.push_back(point);
}

inline void AppendLine(std::vector<Vec3>& path, const Vec3& end)
{
    AppendPoint(path, end);
}

inline double AppendCompactSBend(std::vector<Vec3>& path,
                          double xStart,
                          double yStart,
                          double xEnd,
                          double z,
                          double minBendRadiusMm,
                          double stepMm)
{
    if (minBendRadiusMm <= 0.0 || stepMm <= 0.0) {
        throw std::invalid_argument("Compact fan-in bend requires positive radius and step");
    }

    AppendPoint(path, {xStart, yStart, z});
    const double dx = xEnd - xStart;
    const double d = std::abs(dx);
    if (d < kEpsilon) return yStart;

    const double sign = dx > 0.0 ? 1.0 : -1.0;
    const double radius = minBendRadiusMm;
    const double theta = d >= 2.0 * radius
        ? 0.5 * kPi
        : std::acos(std::clamp(1.0 - d / (2.0 * radius), -1.0, 1.0));
    const double arcLength = radius * theta;
    const std::size_t nArc = std::max<std::size_t>(
        3, static_cast<std::size_t>(std::ceil(arcLength / stepMm)));

    // First arc: +Y -> oblique/horizontal.
    for (std::size_t i = 1; i <= nArc; ++i) {
        const double a = theta * static_cast<double>(i) / static_cast<double>(nArc);
        AppendPoint(path, {
            xStart + sign * radius * (1.0 - std::cos(a)),
            yStart + radius * std::sin(a),
            z
        });
    }

    Vec3 current = path.back();
    const double arcShift = 2.0 * radius * (1.0 - std::cos(theta));
    const double straightLength = d > arcShift
        ? (d - arcShift) / std::sin(theta)
        : 0.0;

    if (straightLength > kEpsilon) {
        current = {
            current.x() + sign * straightLength * std::sin(theta),
            current.y() + straightLength * std::cos(theta),
            z
        };
        AppendPoint(path, current);
    }

    const Vec3 secondStart = path.back();
    for (std::size_t i = 1; i <= nArc; ++i) {
        const double q = theta * static_cast<double>(i) / static_cast<double>(nArc);
        AppendPoint(path, {
            secondStart.x() + sign * radius *
                (std::cos(theta - q) - std::cos(theta)),
            secondStart.y() + radius *
                (std::sin(theta) - std::sin(theta - q)),
            z
        });
    }

    path.back().setX(xEnd);
    return path.back().y();
}

inline std::vector<std::vector<Vec3>> BuildParallelFanInPaths(
    std::size_t nFibers,
    double pitchMm,
    double centerX,
    double zMm,
    double straightYMinMm,
    double straightYMaxMm,
    double sensor0Xmm,
    double sensor0Ymm,
    double sensor1Xmm,
    double sensor1Ymm,
    double sensorOutputPitchMm,
    double minBendRadiusMm,
    double curveStepMm)
{
    if (nFibers == 0) {
        throw std::invalid_argument("Parallel fan-in requires at least one fiber");
    }
    if (pitchMm <= 0.0 || sensorOutputPitchMm < 0.0) {
        throw std::invalid_argument("Parallel fan-in pitches are invalid");
    }
    if (!(sensor0Ymm < sensor1Ymm)) {
        throw std::invalid_argument("Parallel fan-in requires sensor0Y < sensor1Y");
    }

    std::vector<std::vector<Vec3>> fibers;
    fibers.reserve(nFibers);
    const double mid = 0.5 * static_cast<double>(nFibers - 1);

    for (std::size_t i = 0; i < nFibers; ++i) {
        const double offset = static_cast<double>(i) - mid;
        const double xStraight = centerX + offset * pitchMm;
        const double xSensor0 = sensor0Xmm + offset * sensorOutputPitchMm;
        const double xSensor1 = sensor1Xmm + offset * sensorOutputPitchMm;

        std::vector<Vec3> path;
        const double lowerJoinY = AppendCompactSBend(
            path,
            xSensor0,
            sensor0Ymm,
            xStraight,
            zMm,
            minBendRadiusMm,
            curveStepMm);

        std::vector<Vec3> upperTransition;
        const double upperTransitionHeight = AppendCompactSBend(
            upperTransition,
            xStraight,
            0.0,
            xSensor1,
            zMm,
            minBendRadiusMm,
            curveStepMm);
        const double upperJoinY = sensor1Ymm - upperTransitionHeight;

        // The legacy StraightYMin/Max values are interpreted only as a minimum
        // common straight section requested by old configuration files. The
        // actual straight section is maximised automatically and normally much
        // longer than this interval.
        if (lowerJoinY > straightYMinMm + 1.0e-6 ||
            upperJoinY < straightYMaxMm - 1.0e-6) {
            throw std::invalid_argument(
                "Parallel fan-in cannot provide the requested minimum straight section "
                "with the selected sensor positions and bend radius");
        }
        if (upperJoinY <= lowerJoinY) {
            throw std::invalid_argument(
                "Parallel fan-in has no room for a straight section between the two compact bends");
        }

        AppendLine(path, {xStraight, upperJoinY, zMm});
        for (std::size_t p = 1; p < upperTransition.size(); ++p) {
            Vec3 point = upperTransition[p];
            point.setY(point.y() + upperJoinY);
            AppendPoint(path, point);
        }
        fibers.push_back(std::move(path));
    }
    return fibers;
}


}
namespace panel {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTolerance = 1.0e-6;

enum class Orientation {
    Vertical,
    Horizontal
};

inline Orientation ParseOrientation(const std::string& value)
{
    if (value == "vertical") return Orientation::Vertical;
    if (value == "horizontal") return Orientation::Horizontal;
    throw std::invalid_argument(
        "Geometry.DoubleSerpentine.Orientation must be 'vertical' or 'horizontal'");
}

inline Vec3 LocalToGlobal(double u, double v, double z, Orientation orientation)
{
    if (orientation == Orientation::Vertical) return {u, v, z};
    return {v, u, z};
}

inline std::array<double, 2> GlobalToLocal(double x, double y, Orientation orientation)
{
    if (orientation == Orientation::Vertical) return {x, y};
    return {y, x};
}

inline void AppendPoint(std::vector<Vec3>& path, const Vec3& point)
{
    if (!path.empty() && Norm(path.back() - point) < 1.0e-7) return;
    if (path.size() >= 100000) throw std::invalid_argument("MINE route exceeds 100000 points");
    path.push_back(point);
}

inline void AppendLineLocal(std::vector<Vec3>& path,
                     double u,
                     double v,
                     double z,
                     Orientation orientation)
{
    AppendPoint(path, LocalToGlobal(u, v, z, orientation));
}

inline void AppendCosineTransitionLocal(std::vector<Vec3>& path,
                                 double startU,
                                 double startV,
                                 double endU,
                                 double endV,
                                 double z,
                                 Orientation orientation,
                                 double stepMm)
{
    if (endV <= startV) {
        throw std::invalid_argument(
            "Panel double-serpentine transition requires increasing local longitudinal coordinate");
    }
    if (stepMm <= 0.0) {
        throw std::invalid_argument("Geometry curve step must be positive");
    }

    AppendPoint(path, LocalToGlobal(startU, startV, z, orientation));
    const double du = endU - startU;
    const double dv = endV - startV;
    const double estimate = std::hypot(du, dv);
    const std::size_t n = std::max<std::size_t>(
        2, static_cast<std::size_t>(std::ceil(estimate / stepMm)));

    for (std::size_t i = 1; i <= n; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(n);
        const double smooth = 0.5 * (1.0 - std::cos(kPi * t));
        AppendPoint(
            path,
            LocalToGlobal(
                startU + du * smooth,
                startV + dv * t,
                z,
                orientation));
    }
}

inline void AppendSemicircleLocal(std::vector<Vec3>& path,
                           double uLeft,
                           double uRight,
                           double vBase,
                           double z,
                           bool bulgePositiveV,
                           Orientation orientation,
                           double stepMm)
{
    const double radius = 0.5 * (uRight - uLeft);
    if (radius <= 0.0) {
        throw std::invalid_argument("Double-serpentine semicircle requires uRight > uLeft");
    }

    const double centerU = 0.5 * (uLeft + uRight);
    const std::size_t n = std::max<std::size_t>(
        4, static_cast<std::size_t>(std::ceil(kPi * radius / stepMm)));

    for (std::size_t i = 1; i <= n; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(n);
        const double theta = bulgePositiveV
            ? kPi * (1.0 - t)
            : kPi * (1.0 + t);
        const double u = centerU + radius * std::cos(theta);
        const double v = vBase + radius * std::sin(theta);
        AppendPoint(path, LocalToGlobal(u, v, z, orientation));
    }
}

inline std::size_t AutoLanesPerFiber(double halfTransverseMm, double pitchMm)
{
    const double usableWidth = 2.0 * halfTransverseMm;
    if (usableWidth <= 0.0 || pitchMm <= 0.0) {
        throw std::invalid_argument("Invalid panel width or serpentine pitch");
    }

    const std::size_t maxTotalLanes =
        static_cast<std::size_t>(std::floor(usableWidth / pitchMm)) + 1;

    // Two interleaved fibers use the same odd number of lanes so both fibers
    // start on the negative readout edge and end on the positive readout edge.
    std::size_t lanesPerFiber = maxTotalLanes / 2;
    if (lanesPerFiber % 2 == 0 && lanesPerFiber > 0) --lanesPerFiber;
    if (lanesPerFiber < 3) {
        throw std::invalid_argument(
            "Panel is too narrow for a double serpentine with the requested pitch");
    }
    return lanesPerFiber;
}

struct PanelSerpentineLayout {
    std::vector<std::vector<Vec3>> fibers;
    std::size_t lanesPerFiber{0};
};

inline PanelSerpentineLayout BuildPanelDoubleSerpentinePaths(
    double pitchMm,
    double zMm,
    double halfSizeXmm,
    double halfSizeYmm,
    const std::string& orientationText,
    bool autoSensorPlacement,
    double edgeMarginMm,
    double sensor0Xmm,
    double sensor0Ymm,
    double sensor1Xmm,
    double sensor1Ymm,
    double sensorOutputPitchMm,
    double minBendRadiusMm,
    double curveStepMm)
{
    if (pitchMm <= 0.0) {
        throw std::invalid_argument("Double-serpentine pitch must be positive");
    }
    if (pitchMm + kTolerance < minBendRadiusMm) {
        throw std::invalid_argument(
            "Double-serpentine pitch is smaller than Fiber.MinBendRadius_mm");
    }
    if (edgeMarginMm < 0.0 || sensorOutputPitchMm < 0.0) {
        throw std::invalid_argument("Double-serpentine edge/output margins must be non-negative");
    }
    if (curveStepMm <= 0.0) {
        throw std::invalid_argument("Geometry curve step must be positive");
    }

    const Orientation orientation = ParseOrientation(orientationText);
    const double panelHalfTransverse =
        orientation == Orientation::Vertical ? halfSizeXmm : halfSizeYmm;
    const double panelHalfLongitudinal =
        orientation == Orientation::Vertical ? halfSizeYmm : halfSizeXmm;

    // The regular lane lattice is allowed to approach the side edges to about
    // half a pitch. This is the natural continuation of an equally spaced
    // lattice and prevents the discrete odd-lane requirement from collapsing
    // a 1 m panel to a ~0.5 m span.
    const double laneHalfSpanLimit = panelHalfTransverse - 0.5 * pitchMm;
    if (laneHalfSpanLimit <= 0.0) {
        throw std::invalid_argument("Panel is too narrow for the requested serpentine pitch");
    }

    const std::size_t lanesPerFiber = AutoLanesPerFiber(laneHalfSpanLimit, pitchMm);
    const std::size_t totalLanes = 2 * lanesPerFiber;
    const double firstLaneStartU =
        -0.5 * static_cast<double>(totalLanes - 1) * pitchMm;

    const double firstFiber0U = firstLaneStartU;
    const double firstFiber1U = firstLaneStartU + pitchMm;
    const double lastFiber0U =
        firstFiber0U + 2.0 * static_cast<double>(lanesPerFiber - 1) * pitchMm;
    const double lastFiber1U =
        firstFiber1U + 2.0 * static_cast<double>(lanesPerFiber - 1) * pitchMm;

    const std::array<double, 2> firstLaneU{{firstFiber0U, firstFiber1U}};
    const std::array<double, 2> lastLaneU{{lastFiber0U, lastFiber1U}};

    // The serpentine itself is laid out independently of the sensor fan-in.
    // The furthest point of each U-turn sits edgeMarginMm from the physical
    // panel edge. Since same-fiber lanes are 2*pitch apart, each U-turn radius
    // is pitchMm and the straight runs end one radius before that envelope.
    const double turnExtremeV = panelHalfLongitudinal - edgeMarginMm;
    if (turnExtremeV <= pitchMm) {
        throw std::invalid_argument(
            "DoubleSerpentine.EdgeMargin_mm leaves insufficient room for the U-turn radius");
    }
    const double straightVMin = -turnExtremeV + pitchMm;
    const double straightVMax = turnExtremeV - pitchMm;
    if (straightVMax <= straightVMin) {
        throw std::invalid_argument("No longitudinal room remains for the serpentine straight runs");
    }

    double sensor0CenterU = 0.0;
    double sensor1CenterU = 0.0;

    if (autoSensorPlacement) {
        // Put the common readout points near the first and last lane pairs,
        // instead of forcing them to the middle of the panel edge.
        sensor0CenterU = 0.5 * (firstFiber0U + firstFiber1U);
        sensor1CenterU = 0.5 * (lastFiber0U + lastFiber1U);
    } else {
        const auto sensor0Local = GlobalToLocal(sensor0Xmm, sensor0Ymm, orientation);
        const auto sensor1Local = GlobalToLocal(sensor1Xmm, sensor1Ymm, orientation);
        sensor0CenterU = sensor0Local[0];
        sensor1CenterU = sensor1Local[0];

        const double edgeTolerance = std::max(1.0e-3, 0.5 * curveStepMm);
        if (std::abs(sensor0Local[1] + panelHalfLongitudinal) > edgeTolerance ||
            std::abs(sensor1Local[1] - panelHalfLongitudinal) > edgeTolerance) {
            throw std::invalid_argument(
                "Manual double-serpentine sensors must lie on the physical opposite panel edges");
        }
        if (std::abs(sensor0CenterU) > panelHalfTransverse + edgeTolerance ||
            std::abs(sensor1CenterU) > panelHalfTransverse + edgeTolerance) {
            throw std::invalid_argument(
                "Manual double-serpentine sensor offset lies outside the panel edge");
        }
    }

    PanelSerpentineLayout layout;
    layout.lanesPerFiber = lanesPerFiber;
    layout.fibers.reserve(2);

    for (std::size_t fiber = 0; fiber < 2; ++fiber) {
        const double endOffset =
            (static_cast<double>(fiber) - 0.5) * sensorOutputPitchMm;
        const double sensor0U = sensor0CenterU + endOffset;
        const double sensor1U = sensor1CenterU + endOffset;
        const double firstU = firstLaneU[fiber];

        std::vector<Vec3> path;
        AppendCosineTransitionLocal(
            path,
            sensor0U,
            -panelHalfLongitudinal,
            firstU,
            straightVMin,
            zMm,
            orientation,
            curveStepMm);

        bool goingPositiveV = true;
        for (std::size_t lane = 0; lane < lanesPerFiber; ++lane) {
            const double u = firstU + 2.0 * static_cast<double>(lane) * pitchMm;
            const double endV = goingPositiveV ? straightVMax : straightVMin;
            AppendLineLocal(path, u, endV, zMm, orientation);

            if (lane + 1 < lanesPerFiber) {
                const double nextU = u + 2.0 * pitchMm;
                AppendSemicircleLocal(
                    path,
                    u,
                    nextU,
                    endV,
                    zMm,
                    goingPositiveV,
                    orientation,
                    curveStepMm);
                goingPositiveV = !goingPositiveV;
            }
        }

        AppendCosineTransitionLocal(
            path,
            lastLaneU[fiber],
            straightVMax,
            sensor1U,
            panelHalfLongitudinal,
            zMm,
            orientation,
            curveStepMm);

        layout.fibers.push_back(std::move(path));
    }

    return layout;
}

inline std::vector<std::vector<Vec3>> BuildPanelPathsOnly(
    double pitchMm,
    double zMm,
    double halfSizeXmm,
    double halfSizeYmm,
    const std::string& orientation,
    bool autoSensorPlacement,
    double edgeMarginMm,
    double sensor0Xmm,
    double sensor0Ymm,
    double sensor1Xmm,
    double sensor1Ymm,
    double sensorOutputPitchMm,
    double minBendRadiusMm,
    double curveStepMm)
{
    return BuildPanelDoubleSerpentinePaths(
        pitchMm,
        zMm,
        halfSizeXmm,
        halfSizeYmm,
        orientation,
        autoSensorPlacement,
        edgeMarginMm,
        sensor0Xmm,
        sensor0Ymm,
        sensor1Xmm,
        sensor1Ymm,
        sensorOutputPitchMm,
        minBendRadiusMm,
        curveStepMm).fibers;
}

}
namespace alternative {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTolerance = 1.0e-6;

inline void AppendPoint(std::vector<Vec3>& path, const Vec3& point)
{
    if (!path.empty() && Norm(path.back() - point) < 1.0e-7) return;
    if (path.size() >= 100000) throw std::invalid_argument("MINE route exceeds 100000 points");
    path.push_back(point);
}

inline void AppendSmoothTransition(std::vector<Vec3>& path,
                            const Vec3& start,
                            const Vec3& end,
                            double stepMm)
{
    if (stepMm <= 0.0) {
        throw std::invalid_argument("Geometry.CurveStep_mm must be positive");
    }

    AppendPoint(path, start);
    const Vec3 delta = end - start;
    const double estimate = Norm(delta);
    const std::size_t n = std::max<std::size_t>(
        2, static_cast<std::size_t>(std::ceil(estimate / stepMm)));

    for (std::size_t i = 1; i <= n; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(n);
        const double smooth = 0.5 * (1.0 - std::cos(kPi * t));
        AppendPoint(path, {
            start.x() + delta.x() * smooth,
            start.y() + delta.y() * t,
            start.z() + delta.z() * t
        });
    }
}

inline void AppendVerticalSerpentineTurn(std::vector<Vec3>& path,
                                  double xLeft,
                                  double xRight,
                                  double yBase,
                                  double zMm,
                                  bool bulgePositiveY,
                                  double stepMm)
{
    const double radius = 0.5 * (xRight - xLeft);
    if (radius <= 0.0) {
        throw std::invalid_argument("Serpentine turn requires xRight > xLeft");
    }

    const double centerX = 0.5 * (xLeft + xRight);
    const std::size_t n = std::max<std::size_t>(
        4, static_cast<std::size_t>(std::ceil(kPi * radius / stepMm)));

    for (std::size_t i = 1; i <= n; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(n);
        const double theta = bulgePositiveY
            ? kPi * (1.0 - t)
            : kPi * (1.0 + t);
        AppendPoint(path, {
            centerX + radius * std::cos(theta),
            yBase + radius * std::sin(theta),
            zMm
        });
    }
}

inline double RequiredCosineRunMm(double transverseShiftMm, double minBendRadiusMm)
{
    if (std::abs(transverseShiftMm) < kTolerance) return 0.0;
    return 1.05 * kPi *
        std::sqrt(0.5 * minBendRadiusMm * std::abs(transverseShiftMm));
}

inline std::vector<std::vector<Vec3>> BuildTripleSerpentinePaths(
    double pitchMm,
    double zMm,
    double halfSizeXmm,
    double halfSizeYmm,
    double edgeMarginMm,
    double sensorOutputPitchMm,
    double minBendRadiusMm,
    double curveStepMm)
{
    constexpr std::size_t nFibers = 3;
    constexpr std::size_t lanesPerFiber = 4;
    constexpr std::size_t totalLanes = nFibers * lanesPerFiber;

    if (pitchMm <= 0.0 || edgeMarginMm < 0.0 || sensorOutputPitchMm < 0.0) {
        throw std::invalid_argument("Invalid triple-serpentine pitch or margin");
    }
    if (curveStepMm <= 0.0) {
        throw std::invalid_argument("Geometry.CurveStep_mm must be positive");
    }

    const double laneSpan = static_cast<double>(totalLanes - 1) * pitchMm;
    if (laneSpan > 2.0 * halfSizeXmm + kTolerance) {
        throw std::invalid_argument(
            "Twelve triple-serpentine lanes do not fit the panel at the requested pitch");
    }

    const double firstLaneX = -0.5 * laneSpan;
    const double sameFiberSpacing = static_cast<double>(nFibers) * pitchMm;
    const double turnRadius = 0.5 * sameFiberSpacing;
    if (turnRadius + kTolerance < minBendRadiusMm) {
        throw std::invalid_argument(
            "Triple-serpentine natural U-turn radius is below Fiber.MinBendRadius_mm");
    }

    const double yTurnTop = halfSizeYmm - edgeMarginMm - turnRadius;
    double yTurnBottom = -halfSizeYmm + edgeMarginMm + turnRadius;

    const double sensor0CenterX = firstLaneX + pitchMm;
    const double sensor1CenterX = -sensor0CenterX;

    double requiredBottomRun = 0.0;
    for (std::size_t fiber = 0; fiber < nFibers; ++fiber) {
        const double bundleOffset =
            (static_cast<double>(fiber) - 1.0) * sensorOutputPitchMm;
        const double startLaneX = firstLaneX + static_cast<double>(fiber) * pitchMm;
        const double endLaneX =
            startLaneX + static_cast<double>(lanesPerFiber - 1) * sameFiberSpacing;
        requiredBottomRun = std::max(
            requiredBottomRun,
            RequiredCosineRunMm(startLaneX - (sensor0CenterX + bundleOffset), minBendRadiusMm));
        requiredBottomRun = std::max(
            requiredBottomRun,
            RequiredCosineRunMm((sensor1CenterX + bundleOffset) - endLaneX, minBendRadiusMm));
    }
    yTurnBottom = std::max(yTurnBottom, -halfSizeYmm + requiredBottomRun);

    if (yTurnTop <= yTurnBottom) {
        throw std::invalid_argument(
            "Triple serpentine has no longitudinal room after bend and sensor constraints");
    }

    std::vector<std::vector<Vec3>> fibers;
    fibers.reserve(nFibers);

    for (std::size_t fiber = 0; fiber < nFibers; ++fiber) {
        const double bundleOffset =
            (static_cast<double>(fiber) - 1.0) * sensorOutputPitchMm;
        const double firstX = firstLaneX + static_cast<double>(fiber) * pitchMm;
        const double lastX =
            firstX + static_cast<double>(lanesPerFiber - 1) * sameFiberSpacing;

        std::vector<Vec3> path;
        AppendSmoothTransition(
            path,
            {sensor0CenterX + bundleOffset, -halfSizeYmm, zMm},
            {firstX, yTurnBottom, zMm},
            curveStepMm);

        bool goingUp = true;
        for (std::size_t lane = 0; lane < lanesPerFiber; ++lane) {
            const double x = firstX + static_cast<double>(lane) * sameFiberSpacing;
            const double endY = goingUp ? yTurnTop : yTurnBottom;
            AppendPoint(path, {x, endY, zMm});

            if (lane + 1 < lanesPerFiber) {
                const double nextX = x + sameFiberSpacing;
                AppendVerticalSerpentineTurn(
                    path, x, nextX, endY, zMm, goingUp, curveStepMm);
                goingUp = !goingUp;
            }
        }

        if (goingUp) {
            throw std::logic_error("Triple-serpentine four-lane parity error");
        }

        AppendSmoothTransition(
            path,
            {lastX, yTurnBottom, zMm},
            {sensor1CenterX + bundleOffset, -halfSizeYmm, zMm},
            curveStepMm);
        fibers.push_back(std::move(path));
    }

    return fibers;
}

inline std::vector<std::vector<Vec3>> BuildDiagonalCornerFanPaths(
    std::size_t nFibers,
    double centrePitchMm,
    double zMm,
    double halfSizeXmm,
    double halfSizeYmm,
    const std::string& diagonal,
    double minBendRadiusMm,
    double curveStepMm)
{
    if (nFibers == 0 || nFibers % 2 != 0) {
        throw std::invalid_argument(
            "Diagonal corner fan requires an even NFibers > 0 (no central fiber)");
    }
    if (centrePitchMm <= 0.0 || minBendRadiusMm <= 0.0 || curveStepMm <= 0.0) {
        throw std::invalid_argument("Diagonal corner fan pitch/radius/step must be positive");
    }
    if (std::abs(halfSizeXmm - halfSizeYmm) > kTolerance) {
        throw std::invalid_argument(
            "Diagonal corner fan currently requires a square scintillator panel");
    }

    const double invSqrt2 = 1.0 / std::sqrt(2.0);
    const double halfSize = halfSizeXmm;
    const double transverseHalfWidth = std::sqrt(2.0) * halfSize;
    const double halfIndex = 0.5 * static_cast<double>(nFibers - 1);
    const double outerVirtualOffset = halfIndex * centrePitchMm;
    if (outerVirtualOffset >= transverseHalfWidth - kTolerance) {
        throw std::invalid_argument(
            "Diagonal corner fan pitch is too large for the requested number of fibers");
    }

    Vec3 sensor0;
    Vec3 sensor1;
    Vec3 vertexDirection;
    if (diagonal == "bl_tr") {
        sensor0 = {-halfSizeXmm, -halfSizeYmm, zMm};
        sensor1 = { halfSizeXmm,  halfSizeYmm, zMm};
        vertexDirection = {-invSqrt2, invSqrt2, 0.0};
    } else if (diagonal == "tl_br") {
        sensor0 = {-halfSizeXmm,  halfSizeYmm, zMm};
        sensor1 = { halfSizeXmm, -halfSizeYmm, zMm};
        vertexDirection = {invSqrt2, invSqrt2, 0.0};
    } else {
        throw std::invalid_argument(
            "Geometry.DiagonalCornerFan.Diagonal must be 'bl_tr' or 'tl_br'");
    }

    // Each fiber is defined first as two straight segments meeting at a virtual
    // vertex on the opposite panel diagonal.  The 14 vertices are spaced by
    // CentrePitch_mm and there is no central one.  The sharp vertex is then
    // replaced by one circular fillet tangent to both straight segments.
    // Therefore every physical fiber is exactly: straight + one arc + straight.
    // A 1% radius margin avoids a false minimum-radius violation from polyline
    // discretisation while remaining effectively at the requested Rmin.
    const double filletRadius = 1.01 * minBendRadiusMm;

    std::vector<std::vector<Vec3>> fibers;
    fibers.reserve(nFibers);

    for (std::size_t fiber = 0; fiber < nFibers; ++fiber) {
        const double offset =
            (static_cast<double>(fiber) - halfIndex) * centrePitchMm;
        const Vec3 vertex{
            vertexDirection.x() * offset,
            vertexDirection.y() * offset,
            zMm
        };

        const Vec3 toSensor0 = sensor0 - vertex;
        const Vec3 toSensor1 = sensor1 - vertex;
        const double length0 = Norm(toSensor0);
        const double length1 = Norm(toSensor1);
        if (length0 <= kTolerance || length1 <= kTolerance) {
            throw std::invalid_argument("Corner-fan virtual vertex coincides with a sensor");
        }

        const Vec3 ray0 = toSensor0 * (1.0 / length0);
        const Vec3 ray1 = toSensor1 * (1.0 / length1);
        const double cosPhi = std::clamp(Dot(ray0, ray1), -1.0, 1.0);
        const double phi = std::acos(cosPhi);
        if (phi <= 1.0e-6 || phi >= kPi - 1.0e-6) {
            throw std::invalid_argument("Corner-fan virtual bend angle is degenerate");
        }

        const double tangentDistance = filletRadius / std::tan(0.5 * phi);
        if (tangentDistance >= length0 - kTolerance ||
            tangentDistance >= length1 - kTolerance) {
            throw std::invalid_argument(
                "Corner-fan fillet does not fit between the sensor and virtual vertex");
        }

        const Vec3 tangent0 = vertex + ray0 * tangentDistance;
        const Vec3 tangent1 = vertex + ray1 * tangentDistance;
        const Vec3 bisectorVector = ray0 + ray1;
        const double bisectorNorm = Norm(bisectorVector);
        if (bisectorNorm <= kTolerance) {
            throw std::invalid_argument("Corner-fan fillet bisector is degenerate");
        }
        const Vec3 bisector = bisectorVector * (1.0 / bisectorNorm);
        const double centerDistance = filletRadius / std::sin(0.5 * phi);
        const Vec3 center = vertex + bisector * centerDistance;

        const Vec3 r0 = tangent0 - center;
        const Vec3 r1 = tangent1 - center;
        double theta0 = std::atan2(r0.y(), r0.x());
        double theta1 = std::atan2(r1.y(), r1.x());

        const Vec3 incoming = Unit(tangent0 - sensor0);
        const Vec3 outgoing = Unit(sensor1 - tangent1);
        const double turnCross = incoming.x() * outgoing.y() - incoming.y() * outgoing.x();
        double delta = theta1 - theta0;
        while (delta <= -kPi) delta += 2.0 * kPi;
        while (delta > kPi) delta -= 2.0 * kPi;
        if (turnCross > 0.0 && delta < 0.0) delta += 2.0 * kPi;
        if (turnCross < 0.0 && delta > 0.0) delta -= 2.0 * kPi;
        if (std::abs(delta) > kPi + 1.0e-6) {
            throw std::logic_error("Corner-fan fillet selected the long circular arc");
        }

        std::vector<Vec3> path;
        AppendPoint(path, sensor0);
        AppendPoint(path, tangent0);

        const std::size_t nArc = std::max<std::size_t>(
            4,
            static_cast<std::size_t>(
                std::ceil(std::abs(delta) * filletRadius / curveStepMm)));
        for (std::size_t i = 1; i <= nArc; ++i) {
            const double t = static_cast<double>(i) / static_cast<double>(nArc);
            const double theta = theta0 + delta * t;
            AppendPoint(path, {
                center.x() + filletRadius * std::cos(theta),
                center.y() + filletRadius * std::sin(theta),
                zMm
            });
        }
        AppendPoint(path, sensor1);

        for (const Vec3& point : path) {
            if (point.x() < -halfSizeXmm - kTolerance ||
                point.x() >  halfSizeXmm + kTolerance ||
                point.y() < -halfSizeYmm - kTolerance ||
                point.y() >  halfSizeYmm + kTolerance) {
                throw std::invalid_argument(
                    "Diagonal corner fan leaves the scintillator panel");
            }
        }

        fibers.push_back(std::move(path));
    }

    return fibers;
}

}
}}
#endif
