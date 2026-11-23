#include "mine_mini_spiral_model.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
namespace mine {
namespace {
constexpr double pi=3.1415926535897932384626433832795;
OpticalPoint cross(const OpticalPoint& a,const OpticalPoint& b) {
    return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}
void require(bool ok,const char* text) {if(!ok)throw std::invalid_argument(std::string("MINE mini spiral: ")+text);}
double smooth(double x) {return x*x*x*(10+x*(-15+6*x));}
double smoothD(double x) {return 30*x*x*(1-x)*(1-x);}
double smoothDD(double x) {return 60*x*(1-x)*(1-2*x);}
struct SShape {
    double x0,y0,x1,shape;
    double V(double u) const {return u/(shape+(1-shape)*u);}
    double X(double u) const {return x0+(x1-x0)*smooth(V(u));}
    double DX(double u) const {
        const double denominator=shape+(1-shape)*u;
        return (x1-x0)*smoothD(V(u))*shape/(denominator*denominator);
    }
    double DDX(double u) const {
        const double d=shape+(1-shape)*u,v=V(u),dv=shape/(d*d),dd=-2*shape*(1-shape)/(d*d*d);
        return (x1-x0)*(smoothDD(v)*dv*dv+smoothD(v)*dd);
    }
    double Inflection() const {
        double a=1e-9,b=1-1e-9;
        for(int i=0;i<70;++i){const double m=(a+b)/2;if(DDX(m)>0)a=m;else b=m;}
        return (a+b)/2;
    }
    OpticalPoint XY(double u) const {return {X(u),y0*(1-u),0};}
};
double fitShape(const MiniSpiralConfig& c) {
    if(c.sInflectionDistanceMm==0)return c.sShapeParameter;
    auto residual=[&](double shape){SShape s{c.leftXmm.back(),c.sStartYmm,c.sReadoutXmm,shape};const auto p=s.XY(s.Inflection());return std::hypot(p.x,p.y)-c.sInflectionDistanceMm;};
    double a=.05,fa=residual(a),best=0,score=std::numeric_limits<double>::infinity();
    for(int i=1;i<=200;++i) {
        const double b=.05*std::pow(400.,i/200.),fb=residual(b);
        if(fa*fb<=0) {
            double lo=a,hi=b,flo=fa;
            for(int j=0;j<70;++j){const double m=(lo+hi)/2,fm=residual(m);if(flo*fm<=0)hi=m;else{lo=m;flo=fm;}}
            const double root=(lo+hi)/2,cost=std::abs(std::log(root));
            if(cost<score){score=cost;best=root;}
        }
        a=b;fa=fb;
    }
    require(best>0,"S inflection-distance constraint cannot be fitted");return best;
}
MiniSpiralSegment line(const OpticalPoint& a,const OpticalPoint& b,MiniSpiralSection section=MiniSpiralSection::Spiral) {
    MiniSpiralSegment out;out.kind=MiniSpiralSegmentKind::Line;out.section=section;out.start=a;out.end=b;return out;
}
MiniSpiralSegment circle(const OpticalPoint& center,double radius,double startAngle,double sweep) {
    MiniSpiralSegment out;out.kind=MiniSpiralSegmentKind::Arc;out.center=center;out.radiusMm=radius;out.sweepRad=sweep;
    out.planeU={std::cos(startAngle),std::sin(startAngle),0};out.planeV={-std::sin(startAngle),std::cos(startAngle),0};
    out.start=center+out.planeU*radius;
    out.end=out.start+out.planeU*(-2*radius*std::pow(std::sin(sweep/2),2))+out.planeV*(radius*std::sin(sweep));return out;
}
MiniSpiralSegment fromTangent(const OpticalPoint& a,const OpticalPoint& tangent,const OpticalPoint& b) {
    const auto v=b-a;const double along=v.dot(tangent);const auto perpendicular=v-tangent*along;
    const double height=perpendicular.norm();
    if(height<1e-12*v.norm())return line(a,b,MiniSpiralSection::S);
    MiniSpiralSegment out;out.kind=MiniSpiralSegmentKind::Arc;out.section=MiniSpiralSection::S;
    out.start=a;out.end=b;out.radiusMm=v.dot(v)/(2*height);out.planeU=perpendicular*(-1/height);out.planeV=tangent;
    out.center=a-out.planeU*out.radiusMm;out.sweepRad=2*std::atan2(height,along);
    require(out.radiusMm>0 && out.sweepRad>0 && out.sweepRad<pi,"Biarc needs adaptive subdivision");return out;
}
MiniSpiralSegment reverse(const MiniSpiralSegment& a) {
    if(a.kind==MiniSpiralSegmentKind::Line)return line(a.end,a.start,a.section);
    auto out=a;out.start=a.end;out.end=a.start;
    out.planeU=a.planeU*std::cos(a.sweepRad)+a.planeV*std::sin(a.sweepRad);
    out.planeV=a.planeU*std::sin(a.sweepRad)-a.planeV*std::cos(a.sweepRad);return out;
}
std::array<MiniSpiralSegment,2> biarc(const OpticalPoint& p0,const OpticalPoint& t0,const OpticalPoint& p1,const OpticalPoint& t1) {
    const auto v=p1-p0;const double dot=std::clamp(t0.dot(t1),-1.,1.),b=v.dot(t0+t1);
    const double denominator=std::sqrt(b*b+2*(1-dot)*v.dot(v))+b;
    require(denominator>0,"Biarc is not forward monotonic");const double d=v.dot(v)/denominator;
    const auto join=(p0+p1+(t0-t1)*d)*.5;
    auto first=fromTangent(p0,t0,join),second=reverse(fromTangent(p1,t1*(-1),join));
    require(first.TangentAtFraction(1).dot(second.TangentAtFraction(0))>1-1e-10,"Biarc tangent discontinuity");return {first,second};
}
std::vector<MiniSpiralSegment> spiral(const MiniSpiralConfig& c,int layer) {
    const double z=c.thicknessMm-c.grooveDepthMm+c.diameterMm/2+layer*c.diameterMm-c.thicknessMm/2;
    auto local=[&](double x,double y){return OpticalPoint{x-c.widthMm/2,y-c.heightMm/2,z};};
    std::vector<MiniSpiralSegment> result;
    auto last=local(c.widthMm-c.leftXmm[0],0);
    auto addLine=[&](const OpticalPoint& p){if((p-last).norm()>1e-10)result.push_back(line(last,p));last=p;};
    for(std::size_t i=0;i<5;++i) {
        const double radius=c.widthMm/2-c.leftXmm[i],cy=c.heightMm-c.topInsetMm[i]-radius;
        addLine(local(c.widthMm-c.leftXmm[i],cy));
        auto arc=circle(local(c.widthMm/2,cy),radius,0,pi);result.push_back(arc);last=arc.end;
        if(i<4) {
            const double lowerRadius=(c.widthMm-c.leftXmm[i+1]-c.leftXmm[i])/2,lowerY=c.bottomInsetMm[i]+lowerRadius;
            addLine(local(c.leftXmm[i],lowerY));
            arc=circle(local((c.leftXmm[i]+c.widthMm-c.leftXmm[i+1])/2,lowerY),lowerRadius,pi,pi);
            result.push_back(arc);last=arc.end;
        } else addLine(local(c.leftXmm.back(),c.sStartYmm));
    }
    return result;
}
std::vector<OpticalPoint> sampledSpiral(const std::vector<MiniSpiralSegment>& segments,double maxStep) {
    std::vector<OpticalPoint> points;points.push_back(segments.front().start);
    for(const auto& s:segments) {
        const int n=s.kind==MiniSpiralSegmentKind::Arc?std::max(8,static_cast<int>(std::ceil(s.LengthMm()/maxStep))):1;
        for(int j=1;j<=n;++j)points.push_back(s.PointAtFraction(j/static_cast<double>(n)));
    }return points;
}
double firstCrossing(const MiniSpiralConfig& c,const SShape& s,const std::vector<OpticalPoint>& points) {
    const int samples=std::max(128,static_cast<int>(std::ceil(2*(c.sStartYmm+2*(c.sReadoutXmm-c.leftXmm.back()))/c.sourceCurveStepMm)));
    double first=1;auto a=s.XY(0);
    auto cross2=[](const OpticalPoint& x,const OpticalPoint& y){return x.x*y.y-x.y*y.x;};
    // Convert the source-calibration samples back to lower-left XY.
    for(int i=1;i<=samples;++i) {
        const auto b=s.XY(i/static_cast<double>(samples)),v=b-a;
        for(std::size_t j=1;j<points.size();++j) {
            const auto p=points[j-1]+OpticalPoint{c.widthMm/2,c.heightMm/2,0};
            const auto q=p-a,w=points[j]-points[j-1];const double det=cross2(v,w);
            if(std::abs(det)<1e-12)continue;
            const double t=cross2(q,w)/det,r=cross2(q,v)/det;
            if(t< -1e-9||t>1+1e-9||r< -1e-9||r>1+1e-9)continue;
            const double hit=(i-1+std::clamp(t,0.,1.))/samples;if(hit>1e-5)first=std::min(first,hit);
        }a=b;
    }
    require(first<1,"S does not cross the spiral");return first;
}
void approximateS(MiniSpiralModel& model,double lo,double hi,int depth) {
    std::array<std::array<MiniSpiralSegment,2>,2> arcs;
    bool valid=true;
    try {
        for(int f=0;f<2;++f)arcs[f]=biarc(model.SPoint(lo,f),model.STangent(lo,f),model.SPoint(hi,f),model.STangent(hi,f));
    } catch(const std::invalid_argument&) {valid=false;}
    double error=0;
    if(valid)for(int f=0;f<2;++f)for(int j=1;j<32;++j) {
        const auto p=model.SPoint(lo+(hi-lo)*j/32.,f);
        error=std::max(error,std::min(arcs[f][0].DistanceTo(p),arcs[f][1].DistanceTo(p)));
    }
    // A conservative margin relative to the requested numerical tolerance;
    // the accepted model is independently checked on a much denser grid.
    if(valid && error<=model.config.approximationToleranceMm*.2) {
        for(int f=0;f<2;++f)for(auto arc:arcs[f]) {
            arc.sourceU0=lo;arc.sourceU1=hi;model.fibers[f].segments.push_back(arc);
        }return;
    }
    require(depth<24,"S biarc subdivision failed to converge");
    const double mid=(lo+hi)/2;approximateS(model,lo,mid,depth+1);approximateS(model,mid,hi,depth+1);
}
void validate(const MiniSpiralConfig& c) {
    for(double value:{c.widthMm,c.heightMm,c.thicknessMm,c.diameterMm,c.grooveWidthMm,c.grooveDepthMm,c.minBendRadiusMm,c.sourceCurveStepMm,c.approximationToleranceMm,c.readoutWidthMm,c.readoutHeightMm})
        require(std::isfinite(value)&&value>0,"Dimensions/tolerances must be positive finite");
    require(c.grooveWidthMm>=c.diameterMm-1e-8&&c.grooveDepthMm>=2*c.diameterMm-1e-8&&c.grooveDepthMm<c.thicknessMm,"Two fibers must fit the groove");
    require(c.liftEndFraction>0&&c.liftEndFraction<=1&&std::isfinite(c.liftEndFraction),"Invalid lift-end fraction");
    require(c.surfaceGapMm>=0&&std::isfinite(c.surfaceGapMm),"Invalid surface gap");
    require(c.sShapeParameter>=.05&&c.sShapeParameter<=20&&std::isfinite(c.sShapeParameter),"Invalid S shape parameter");
    require(c.sInflectionDistanceMm>=0&&std::isfinite(c.sInflectionDistanceMm),"Invalid S inflection distance");
    require(c.liftSpacingSafety>=1&&c.liftSpacingSafety<=20&&std::isfinite(c.liftSpacingSafety),"Invalid lift-spacing safety factor");
    require(c.sStartYmm>0&&std::isfinite(c.sStartYmm)&&c.sReadoutXmm>c.leftXmm.back()&&c.sReadoutXmm<c.widthMm-c.diameterMm/2,"Invalid S endpoints");
    for(std::size_t i=0;i<5;++i) {
        require(std::isfinite(c.leftXmm[i])&&c.leftXmm[i]>=c.diameterMm/2&&c.leftXmm[i]<c.widthMm/2&&(i==0||c.leftXmm[i]-c.leftXmm[i-1]>=c.diameterMm),"Invalid left-leg X positions");
        require(std::isfinite(c.topInsetMm[i])&&c.topInsetMm[i]>=c.diameterMm/2&&(i==0||c.topInsetMm[i]>c.topInsetMm[i-1]),"Invalid upper insets");
        const double upper=c.heightMm-c.topInsetMm[i]-(c.widthMm/2-c.leftXmm[i]);
        require(upper>0,"Upper semicircle leaves panel");
        if(i<4) {
            require(std::isfinite(c.bottomInsetMm[i])&&c.bottomInsetMm[i]>=c.diameterMm/2&&(i==0||c.bottomInsetMm[i]>c.bottomInsetMm[i-1]),"Invalid lower insets");
            const double lower=c.bottomInsetMm[i]+(c.widthMm-c.leftXmm[i+1]-c.leftXmm[i])/2;
            const double nextUpper=c.heightMm-c.topInsetMm[i+1]-(c.widthMm/2-c.leftXmm[i+1]);
            require(upper>lower&&nextUpper>lower,"Reversed vertical tangencies");
        }
    }
    const double lastUpper=c.heightMm-c.topInsetMm.back()-(c.widthMm/2-c.leftXmm.back());
    const double lastLower=c.bottomInsetMm.back()+(c.widthMm-c.leftXmm[4]-c.leftXmm[3])/2;
    require(c.sStartYmm<lastUpper&&c.sStartYmm>lastLower,"S must depart within the inner left leg");
    require(c.readoutWidthMm>=c.diameterMm&&c.readoutHeightMm>=2*c.diameterMm,"Readout too small for paired ends");
}
} // namespace

double MiniSpiralSegment::LengthMm() const {return kind==MiniSpiralSegmentKind::Line?(end-start).norm():radiusMm*sweepRad;}
OpticalPoint MiniSpiralSegment::PointAtFraction(double f) const {
    require(std::isfinite(f)&&f>=-1e-12&&f<=1+1e-12,"Segment fraction outside [0,1]");f=std::clamp(f,0.,1.);
    if(f==0)return start;if(f==1)return end;
    if(kind==MiniSpiralSegmentKind::Line)return start+(end-start)*f;
    const double a=sweepRad*f;
    return start+planeU*(-2*radiusMm*std::pow(std::sin(a/2),2))+planeV*(radiusMm*std::sin(a));
}
OpticalPoint MiniSpiralSegment::TangentAtFraction(double f) const {
    if(kind==MiniSpiralSegmentKind::Line)return (end-start).unit();
    return planeU*(-std::sin(sweepRad*f))+planeV*std::cos(sweepRad*f);
}
double MiniSpiralSegment::DistanceTo(const OpticalPoint& p) const {
    if(kind==MiniSpiralSegmentKind::Line) {
        const auto v=end-start;const double t=std::clamp((p-start).dot(v)/v.dot(v),0.,1.);return (p-start-v*t).norm();
    }
    const auto d=p-start;double angle=std::atan2(d.dot(planeV),radiusMm+d.dot(planeU));
    if(angle<0)angle+=2*pi;
    if(angle>sweepRad)return std::min((p-start).norm(),(p-end).norm());
    return (p-PointAtFraction(angle/sweepRad)).norm();
}
double MiniSpiralFiber::LengthMm() const {double length=0;for(const auto& s:segments)length+=s.LengthMm();return length;}
OpticalPoint MiniSpiralFiber::PointAt(double s) const {
    require(std::isfinite(s)&&s>=0&&s<=LengthMm()+1e-8,"Fiber distance outside route");
    for(const auto& seg:segments){if(s<=seg.LengthMm())return seg.PointAtFraction(s/seg.LengthMm());s-=seg.LengthMm();}return segments.back().end;
}
OpticalPoint MiniSpiralModel::SPoint(double u,int fiber) const {
    require(std::isfinite(u)&&u>=0&&u<=1&&(fiber==0||fiber==1),"Invalid S point request");
    const SShape s{config.leftXmm.back(),config.sStartYmm,config.sReadoutXmm,sShapeParameter};
    const double h=config.grooveDepthMm+config.surfaceGapMm,w=std::min(1.,u/liftEndU);
    double correction=0;
    if(fiber==1&&u<liftEndU) {
        const double dz=h*smoothD(w)/liftEndU,slope=dz/std::hypot(s.DX(u),config.sStartYmm);
        correction=config.diameterMm*config.liftSpacingSafety*(std::sqrt(1+slope*slope)-1);
    }
    return {s.X(u)-config.widthMm/2,config.sStartYmm*(1-u)-config.heightMm/2,
            config.thicknessMm-config.grooveDepthMm+config.diameterMm/2+fiber*config.diameterMm+h*smooth(w)+correction-config.thicknessMm/2};
}
OpticalPoint MiniSpiralModel::STangent(double u,int fiber) const {
    require(std::isfinite(u)&&u>=0&&u<=1&&(fiber==0||fiber==1),"Invalid S tangent request");
    const SShape s{config.leftXmm.back(),config.sStartYmm,config.sReadoutXmm,sShapeParameter};
    const double dx=s.DX(u),ddx=s.DDX(u);double dz=0;
    if(u<liftEndU) {
        const double w=u/liftEndU,h=config.grooveDepthMm+config.surfaceGapMm;
        dz=h*smoothD(w)/liftEndU;
        if(fiber==1) {
            const double speed=std::hypot(dx,config.sStartYmm),ddz=h*smoothDD(w)/(liftEndU*liftEndU);
            const double slope=dz/speed,dslope=ddz/speed-dz*dx*ddx/(speed*speed*speed);
            dz+=config.diameterMm*config.liftSpacingSafety*slope*dslope/std::sqrt(1+slope*slope);
        }
    }
    return OpticalPoint{dx,-config.sStartYmm,dz}.unit();
}
MiniSpiralModel BuildMiniSpiralModel(const MiniSpiralConfig& c) {
    validate(c);MiniSpiralModel model;model.config=c;model.mcToLocalZMm=-c.thicknessMm/2;
    model.sShapeParameter=fitShape(c);const SShape s{c.leftXmm.back(),c.sStartYmm,c.sReadoutXmm,model.sShapeParameter};
    model.inflectionU=s.Inflection();model.inflectionLowerLeftMm=s.XY(model.inflectionU);
    for(int f=0;f<2;++f)model.fibers[f].segments=spiral(c,f);
    const auto originalSamples=sampledSpiral(model.fibers[0].segments,c.sourceCurveStepMm);
    for(std::size_t i=1;i<originalSamples.size();++i)model.sourceSampledSpiralLengthMm+=(originalSamples[i]-originalSamples[i-1]).norm();
    model.spiralLengthMm=model.fibers[0].LengthMm();model.firstCrossingU=firstCrossing(c,s,originalSamples);model.liftEndU=c.liftEndFraction*model.firstCrossingU;
    std::vector<double> nodes{0,model.liftEndU,model.firstCrossingU,model.inflectionU,1};std::sort(nodes.begin(),nodes.end());
    nodes.erase(std::unique(nodes.begin(),nodes.end()),nodes.end());
    for(std::size_t i=1;i<nodes.size();++i)approximateS(model,nodes[i-1],nodes[i],0);
    for(int f=0;f<2;++f) {
        auto& fiber=model.fibers[f];fiber.samples.push_back(fiber.segments.front().start);
        for(std::size_t i=0;i<fiber.segments.size();++i) {
            const auto& seg=fiber.segments[i];
            if(seg.kind==MiniSpiralSegmentKind::Arc)require(seg.radiusMm>=c.minBendRadiusMm-1e-4,"Measured route violates requested minimum radius");
            if(i>0) {
                require((seg.start-fiber.segments[i-1].end).norm()<1e-7,"Disconnected fiber pieces");
                require(seg.TangentAtFraction(0).dot(fiber.segments[i-1].TangentAtFraction(1))>1-1e-9,"Fiber has a tangent discontinuity");
            }
            const int count=std::max(1,static_cast<int>(std::ceil(seg.LengthMm()/c.sourceCurveStepMm)));
            for(int j=1;j<=count;++j)fiber.samples.push_back(seg.PointAtFraction(j/static_cast<double>(count)));
        }
    }
    // Independent dense geometric check. This is a numerically verified error,
    // not a symbolic continuum proof for the rational S and the spacing correction.
    for(int f=0;f<2;++f) {
        std::vector<const MiniSpiralSegment*> pieces;for(const auto& seg:model.fibers[f].segments)if(seg.section==MiniSpiralSection::S)pieces.push_back(&seg);
        for(int j=0;j<=20000;++j) {
            const double u=j/20000.;const auto p=model.SPoint(u,f);double distance=std::numeric_limits<double>::infinity();
            for(const auto* seg:pieces)if(u>=seg->sourceU0-1e-12&&u<=seg->sourceU1+1e-12)distance=std::min(distance,seg->DistanceTo(p));
            require(std::isfinite(distance),"S approximation has a parameter hole");
            model.maxVerifiedApproximationErrorMm=std::max(model.maxVerifiedApproximationErrorMm,distance);
            if(f==1)model.maxLiftSpacingCorrectionMm=std::max(model.maxLiftSpacingCorrectionMm,p.z-model.SPoint(u,0).z-c.diameterMm);
        }
    }
    require(model.maxVerifiedApproximationErrorMm<=c.approximationToleranceMm,"Dense S error check exceeds requested tolerance");
    for(int e=0;e<2;++e) {
        const auto a=e==0?model.fibers[0].segments.front().start:model.fibers[0].segments.back().end;
        const auto b=e==0?model.fibers[1].segments.front().start:model.fibers[1].segments.back().end;
        model.readouts[e]={ (a+b)*.5,{0,-1,0},{1,0,0},{0,0,1},c.readoutWidthMm,c.readoutHeightMm};
    }return model;
}
} // namespace mine
