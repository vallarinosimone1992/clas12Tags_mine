#pragma once
// Portable measured mini-panel geometry. No Geant4/ROOT or external MC linkage.
// Geometry source: MC_MINE_LightTrapping/src/MiniPanelSpiralGeometry.cpp,
// inspected 2026-11-22. The original MC source remains unchanged.
#include "mine_optical_geometry_model.h"
#include <array>
#include <vector>
namespace mine {
enum class MiniSpiralSegmentKind { Line, Arc };
enum class MiniSpiralSection { Spiral, S };
struct MiniSpiralSegment {
    MiniSpiralSegmentKind kind=MiniSpiralSegmentKind::Line;
    MiniSpiralSection section=MiniSpiralSection::Spiral;
    OpticalPoint start,end,center,planeU{1,0,0},planeV{0,1,0};
    double radiusMm=0,sweepRad=0;
    // Exact S parameter interval represented by this biarc; spiral uses -1.
    double sourceU0=-1,sourceU1=-1;
    double LengthMm() const;
    OpticalPoint PointAtFraction(double fraction) const;
    OpticalPoint TangentAtFraction(double fraction) const;
    double DistanceTo(const OpticalPoint&) const;
};
struct MiniSpiralConfig {
    double widthMm=300,heightMm=500,thicknessMm=8;
    double diameterMm=1,grooveWidthMm=1,grooveDepthMm=2;
    std::array<double,5> leftXmm{{20,45,70,95,120}};
    std::array<double,5> topInsetMm{{10,55,100,145,175}};
    std::array<double,4> bottomInsetMm{{10,55,100,145}};
    double sStartYmm=230,sReadoutXmm=250;
    double sInflectionDistanceMm=230,sShapeParameter=1;
    double liftEndFraction=.9,surfaceGapMm=0;
    double minBendRadiusMm=30;
    // Source MC crossing calibration uses its original 1-mm sampling.
    double sourceCurveStepMm=1;
    // Adaptive G1 biarcs, with an independent dense numerical check.
    double approximationToleranceMm=.0002;
    // Upper fiber gets safety*(sqrt(1+(dz/dsXY)^2)-1)*D during the lift.
    // Vanishes smoothly at either end; default max adjustment is ~3.6 um.
    double liftSpacingSafety=3;
    double readoutWidthMm=3,readoutHeightMm=3;
};
struct MiniSpiralFiber {
    std::vector<MiniSpiralSegment> segments;
    std::vector<OpticalPoint> samples;
    double LengthMm() const;
    OpticalPoint PointAt(double distanceMm) const;
};
struct MiniSpiralModel {
    MiniSpiralConfig config;
    std::array<MiniSpiralFiber,2> fibers;
    std::array<OpticalReadoutPlane,2> readouts;
    double sShapeParameter=1,inflectionU=0,firstCrossingU=0,liftEndU=0;
    OpticalPoint inflectionLowerLeftMm;
    double spiralLengthMm=0,sourceSampledSpiralLengthMm=0;
    double mcToLocalZMm=-4;
    double maxLiftSpacingCorrectionMm=0,maxVerifiedApproximationErrorMm=0;
    // All returned S positions/tangents use centered GEMC panel coordinates.
    OpticalPoint SPoint(double u,int fiber=0) const;
    OpticalPoint STangent(double u,int fiber=0) const;
};
MiniSpiralModel BuildMiniSpiralModel(const MiniSpiralConfig& config={});
} // namespace mine
