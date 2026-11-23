#include "mine_mini_spiral_model.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <fstream>
#include <iomanip>
#include <stdexcept>
namespace {
void need(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
void near(double x,double y,double tolerance,const char* text){need(std::isfinite(x)&&std::abs(x-y)<=tolerance,text);}
template<class F>void rejects(F f,const char* text){bool bad=false;try{f();}catch(const std::invalid_argument&){bad=true;}need(bad,text);}
}
int main(int argc,char** argv){try{
    mine::MiniSpiralConfig c;const auto m=mine::BuildMiniSpiralModel(c);
    if(argc==2){std::ofstream csv(argv[1]);need(bool(csv),"Cannot write geometry CSV");csv<<std::setprecision(15)<<"fiber,x_mm,y_mm,z_mm\n";for(int f=0;f<2;++f)for(const auto&p:m.fibers[f].samples)csv<<f<<','<<p.x<<','<<p.y<<','<<p.z<<'\n';}
    near(m.sShapeParameter,.6271471366392918,1e-11,"Source S fit");
    near(m.firstCrossingU,.3172511686051585,1e-10,"Source crossing convention");
    near(m.sourceSampledSpiralLengthMm,4001.92866478863,1e-7,"Original sampled spiral length");
    near(m.spiralLengthMm,4001.94671058465,1e-7,"Analytic semicircle spiral length");
    near(std::hypot(m.inflectionLowerLeftMm.x,m.inflectionLowerLeftMm.y),230,1e-8,"Radial inflection measurement");
    need(m.maxVerifiedApproximationErrorMm<c.approximationToleranceMm,"Bounded dense numerical S error");
    need(m.maxLiftSpacingCorrectionMm>0 && m.maxLiftSpacingCorrectionMm<.004,"Only micron-level stack correction");
    for(int f=0;f<2;++f){
        const auto& fiber=m.fibers[f];const auto a=fiber.segments.front().start,b=fiber.segments.back().end;
        near(a.x,130,1e-10,"Outer readout X");near(a.y,-250,1e-10,"Outer readout Y");near(a.z,2.5+f,1e-10,"Groove stack relative to panel");
        near(b.x,100,1e-10,"Inner readout X");near(b.y,-250,1e-10,"Inner readout Y");near(b.z,4.5+f,1e-10,"Lifted stack relative to panel");
        need(fiber.LengthMm()>4278 && fiber.LengthMm()<4281,"Full transport length");
        near((m.SPoint(0,f)-mine::OpticalPoint{-30,-20,2.5+f}).norm(),0,1e-10,"S departure");
        near(m.SPoint(m.liftEndU,f).z,4.5+f,1e-10,"Lift ends before crossing");
        double minRadius=std::numeric_limits<double>::infinity();
        for(std::size_t i=0;i<fiber.segments.size();++i){
            const auto& s=fiber.segments[i];if(s.kind==mine::MiniSpiralSegmentKind::Arc){
                minRadius=std::min(minRadius,s.radiusMm);
                near(s.planeU.dot(s.planeV),0,1e-9,"Arc plane orthogonality");
                near(s.planeU.norm(),1,1e-9,"Arc plane U unit");near(s.planeV.norm(),1,1e-9,"Arc plane V unit");
                near((s.PointAtFraction(.499999)-s.PointAtFraction(.500001)).norm()/(.000002*s.LengthMm()),1,1e-6,"Arc metric");
            }
            if(i){near((s.start-fiber.segments[i-1].end).norm(),0,1e-7,"G0 centerline joins");near(s.TangentAtFraction(0).dot(fiber.segments[i-1].TangentAtFraction(1)),1,1e-9,"G1 centerline joins");}
        }
        near(minRadius,30,1e-4,"Measured inner radius");
    }
    near(m.readouts[0].center.z,3,1e-12,"Outer end-plane center");near(m.readouts[1].center.z,5,1e-12,"Inner end-plane center");
    // Check a dense lower-S set against the physical upper arc tubes. Candidate
    // spans overlap in parameter locally because their nearest points have
    // slightly different u; no same-u-only spacing assumption is used.
    double minDistance=std::numeric_limits<double>::infinity();
    for(const auto& low:m.fibers[0].segments)if(low.section==mine::MiniSpiralSection::S)for(int j=0;j<=20;++j){
        const auto p=low.PointAtFraction(j/20.);double distance=std::numeric_limits<double>::infinity();
        for(const auto& up:m.fibers[1].segments)if(up.section==mine::MiniSpiralSection::S && up.sourceU1>=low.sourceU0-.01 && up.sourceU0<=low.sourceU1+.01)distance=std::min(distance,up.DistanceTo(p));
        minDistance=std::min(minDistance,distance);
    }
    need(minDistance>=c.diameterMm-2e-7,"Stacked native-arc centerlines overlap");
    rejects([&]{auto bad=c;bad.minBendRadiusMm=50;mine::BuildMiniSpiralModel(bad);},"Reject radius incompatible with measured spiral");
    rejects([&]{auto bad=c;bad.grooveDepthMm=1.5;mine::BuildMiniSpiralModel(bad);},"Reject undersized groove");
    rejects([&]{auto bad=c;bad.sInflectionDistanceMm=1;mine::BuildMiniSpiralModel(bad);},"Reject impossible inflection quote");
    auto symmetric=c;symmetric.sInflectionDistanceMm=0;symmetric.sShapeParameter=1;const auto sm=mine::BuildMiniSpiralModel(symmetric);
    near(sm.inflectionLowerLeftMm.x,185,1e-8,"Symmetric S X");near(sm.inflectionLowerLeftMm.y,115,1e-8,"Symmetric S Y");
    std::cout<<"MINI_SPIRAL_MODEL_PASS lengths_mm="<<m.fibers[0].LengthMm()<<','<<m.fibers[1].LengthMm()
             <<" segments="<<m.fibers[0].segments.size()<<','<<m.fibers[1].segments.size()
             <<" max_approx_error_mm="<<m.maxVerifiedApproximationErrorMm
             <<" max_spacing_correction_um="<<1000*m.maxLiftSpacingCorrectionMm
             <<" min_interfiber_center_distance_mm="<<minDistance<<'\n';
    return 0;
}catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}}
