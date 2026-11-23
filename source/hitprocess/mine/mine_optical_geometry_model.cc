#include "mine_optical_geometry_model.h"
#include "mine_wls_spectrum.h"
#include "mine_fiber_loss.h"
#include "mine_scintillation_spectrum.h"
#include "mine_coating_spectrum.h"
#include "mine_groove_materials.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>

namespace mine {
namespace {
constexpr double pi=3.14159265358979323846;
constexpr double tolerance=1e-6;
std::string trim(const std::string& s) {
    const auto first=s.find_first_not_of(" \t\r\n");
    return first==std::string::npos ? "" : s.substr(first,s.find_last_not_of(" \t\r\n")-first+1);
}
double number(const std::string& key,const std::string& value) {
    std::size_t n=0;double result;
    try {result=std::stod(value,&n);} catch(...) {throw std::invalid_argument("MINE invalid optical config value: "+key);}
    if(n!=value.size() || !std::isfinite(result)) throw std::invalid_argument("MINE invalid optical config value: "+key);
    return result;
}
}
Geant4OpticsConfig Geant4OpticsConfig::Load(const std::string& path) {
    std::ifstream input(path);
    if(!input) throw std::runtime_error("MINE cannot open Geant4 optical configuration: "+path);
    Geant4OpticsConfig c;c.sourcePath=path;
    std::map<std::string,double*> numeric={
        {"grooveWidthMm",&c.grooveWidthMm},{"grooveDepthMm",&c.grooveDepthMm},
        {"sensorGelGapMm",&c.sensorGelGapMm},{"tentAngleDeg",&c.tentAngleDeg},
        {"tentClearanceMm",&c.tentClearanceMm},{"miniCurveToleranceMm",&c.miniCurveToleranceMm},
        {"scintillationRiseNs",&c.scintillationRiseNs},{"scintillatorDensityGcm3",&c.scintillatorDensityGcm3},
        {"wlsDyeConcentrationPpm",&c.wlsDyeConcentrationPpm},{"pitchMm",&c.pitchMm},{"minBendRadiusMm",&c.minBendRadiusMm},{"zMcMm",&c.zMcMm},
        {"fiberDiameterMm",&c.fiberDiameterMm},{"claddingFraction",&c.claddingFraction},
        {"readoutInsetMm",&c.readoutInsetMm},{"readoutWidthMm",&c.readoutWidthMm},{"readoutHeightMm",&c.readoutHeightMm},
        {"coatingReferenceAlbedo",&c.coatingReferenceAlbedo},{"nScintillator",&c.nScintillator},{"nCore",&c.nCore},{"nCladding",&c.nCladding},{"reflectivity",&c.reflectivity},
        {"scintillationWavelengthNm",&c.scintillationWavelengthNm},{"wlsWavelengthNm",&c.wlsWavelengthNm},{"wlsDecayNs",&c.wlsDecayNs},
        {"wlsAbsorptionLengthMm",&c.wlsAbsorptionLengthMm},{"wlsQuantumYield",&c.wlsQuantumYield},
        {"bulkAbsorptionLengthMm",&c.bulkAbsorptionLengthMm},{"coreAbsorptionLengthMm",&c.coreAbsorptionLengthMm},
        {"claddingAbsorptionLengthMm",&c.claddingAbsorptionLengthMm},{"effectiveFiberAttenuationMm",&c.effectiveFiberAttenuationMm},
        {"birksMmPerMeV",&c.birksMmPerMeV},{"lightYieldPerMeV",&c.lightYieldPerMeV},{"scintillationDecayNs",&c.scintillationDecayNs}};
    std::map<std::string,bool> seen;std::string line;std::uint64_t hash=14695981039346656037ULL;
    while(std::getline(input,line)) {
        for(unsigned char ch:line) {hash^=ch;hash*=1099511628211ULL;}hash^='\n';hash*=1099511628211ULL;
        line=trim(line.substr(0,line.find('#')));if(line.empty()) continue;
        const auto equal=line.find('=');
        if(equal==std::string::npos) throw std::invalid_argument("MINE optical config expects key=value");
        const auto key=trim(line.substr(0,equal)),value=trim(line.substr(equal+1));
        if(seen[key]) throw std::invalid_argument("MINE duplicate optical config key: "+key);
        seen[key]=true;
        if(numeric.count(key)) *numeric.at(key)=number(key,value);
        else if(key=="model") c.model=value;
        else if(key=="geometry") c.geometry=value;
        else if(key=="sipmModel") c.sipmModel=value;
        else if(key=="fiberModel") c.fiberModel=value;
        else if(key=="fiberLossModel") c.fiberLossModel=value;
        else if(key=="wlsEmissionConvention") c.wlsEmissionConvention=value;
        else if(key=="scintillationSpectrum") c.scintillationSpectrum=value;
        else if(key=="scintillationEmissionConvention") c.scintillationEmissionConvention=value;
        else if(key=="grooveModel") c.grooveModel=value;
        else if(key=="grooveMaterial") c.grooveMaterial=value;
        else if(key=="coatingModel") c.coatingModel=value;
        else if(key=="coatingMaterial") c.coatingMaterial=value;
        else if(key=="diagonal") c.diagonal=value;
        else if(key=="fiberCount" || key=="sensorCount") {
            const double n=number(key,value);
            if(n<1 || n>1000 || std::floor(n)!=n) throw std::invalid_argument("MINE invalid optical config integer: "+key);
            (key=="fiberCount"?c.fiberCount:c.sensorCount)=static_cast<int>(n);
        } else if(key=="autoFit" || key=="checkOverlaps") {
            if(value!="0" && value!="1") throw std::invalid_argument("MINE optical config flags require 0 or 1: "+key);
            (key=="autoFit"?c.autoFit:c.checkOverlaps)=value=="1";
        } else if(key=="fiberZOffsetsMm") {
            std::istringstream offsets(value);std::string element;
            while(std::getline(offsets,element,',')) c.fiberZOffsetsMm.push_back(number(key,trim(element)));
            if(c.fiberZOffsetsMm.empty()) throw std::invalid_argument("MINE empty fiberZOffsetsMm");
        } else throw std::invalid_argument("MINE unknown optical config key: "+key);
    }
    if(!input.eof()) throw std::runtime_error("MINE error reading optical config");
    if(c.coatingMaterial!="constant" && seen["reflectivity"])
        throw std::invalid_argument("MINE spectral coatingMaterial supplies R(lambda); remove the scalar reflectivity key (or select coatingMaterial=constant)");
    // Named fiber profiles supply their decay/peak unless explicitly overridden.
    if(!seen["wlsDecayNs"]) c.wlsDecayNs=wlsProfileDecayNs(c.fiberModel);
    if(!seen["wlsWavelengthNm"]) c.wlsWavelengthNm=wlsProfilePeakNm(c.fiberModel);
    c.Validate();
    if(isSpectralWlsModel(c.model)) fingerprintWlsSpectra(hash,c.fiberModel);
    if(c.geometry=="mini_panel_spiral") {
        const std::string version="mini_spiral_native_v1;300x500x8;left20,45,70,95,120;top10,55,100,145,175;bottom10,55,100,145;S230_250_inflection230;lift0.9_safety3;G1biarcs;nearstraight50nm_or_quartertol;CSGjoin1um_zeroTorusStart;canopy1200_tangent_roof;exit_clearance0.5um;SiPM3x3;";
        for(unsigned char ch:version){hash^=ch;hash*=1099511628211ULL;}
    }
    fingerprintFiberLoss(hash,c);
    fingerprintScintillationSpectrum(hash,c.scintillationSpectrum);
    // Version the new angular law while preserving legacy hashes when the key
    // is absent (the historical specular boundary is unchanged).
    if(c.coatingModel=="diffuse")
        for(unsigned char ch:std::string("opaque_unified_Lambertian_v1")) {hash^=ch;hash*=1099511628211ULL;}
    fingerprintCoatingSpectrum(hash,c.coatingMaterial);
    if(c.grooveModel!="none") fingerprintGrooveMaterial(hash,c.grooveMaterial);
    c.fingerprint=hash&((1ULL<<52)-1);return c;
}
void Geant4OpticsConfig::Validate() const {
    const int profile=wlsProfileId(fiberModel);
    if(geometry!="corner_fan" && geometry!="mini_panel_spiral")
        throw std::invalid_argument("MINE explicit geometry must be corner_fan or mini_panel_spiral");
    if(sipmModel!="ideal" && sipmModel!="hamamatsu")
        throw std::invalid_argument("MINE sipmModel must be ideal or hamamatsu");
    for(double value:{sensorGelGapMm,tentClearanceMm,scintillationRiseNs,scintillatorDensityGcm3})
        if(!std::isfinite(value) || value<0)throw std::invalid_argument("MINE gap, clearance, rise and density must be finite and nonnegative");
    if(!std::isfinite(tentAngleDeg) || tentAngleDeg<=0 || tentAngleDeg>=90 ||
       !std::isfinite(miniCurveToleranceMm) || miniCurveToleranceMm<=0 || miniCurveToleranceMm>.1)
        throw std::invalid_argument("MINE tentAngleDeg must be in (0,90), miniCurveToleranceMm in (0,0.1]");
    if(geometry=="mini_panel_spiral" && (fiberCount!=2 || grooveModel!="rectangular" ||
       std::abs(grooveWidthMm-1)>1e-9 || std::abs(grooveDepthMm-2)>1e-9 ||
       std::abs(fiberDiameterMm-1)>1e-9 || std::abs(zMcMm-6.5)>1e-9))
        throw std::invalid_argument("MINE mini_panel_spiral requires two 1-mm fibers, rectangular 1x2-mm groove and zMcMm=6.5");
    if(geometry=="mini_panel_spiral" && (sensorGelGapMm<=0 || std::abs(readoutWidthMm-3)>1e-9 || std::abs(readoutHeightMm-3)>1e-9))
        throw std::invalid_argument("MINE mini_panel_spiral requires a positive gel gap and 3x3-mm SiPMs");
    if(fiberLossModel!="none" && fiberLossModel!="kuraray_spectral")
        throw std::invalid_argument("MINE fiberLossModel must be none or kuraray_spectral");
    if(usesKurarayFiberLoss(*this) && (!isSpectralWlsModel(model) ||
       effectiveFiberAttenuationMm!=0 || coreAbsorptionLengthMm!=0 || claddingAbsorptionLengthMm!=0))
        throw std::invalid_argument("MINE kuraray_spectral requires spectral WLS and zero scalar fiber losses to avoid double counting");
    if((model!="monochromatic_effective" && !isSpectralWlsModel(model)) || sensorCount!=2 ||
       (model=="monochromatic_effective" && profile!=0))
        throw std::invalid_argument("MINE explicit optics supports spectral_wls (Y-11/YS-6/YS-4), legacy spectral_y11, or monochromatic_effective Y-11; single cladding and two sensors");
    if(!isScintillationSpectrum(scintillationSpectrum))
        throw std::invalid_argument("MINE scintillationSpectrum must be monochromatic, fnal_extruded or ej200");
    if(scintillationEmissionConvention!="photon_per_nm" && scintillationEmissionConvention!="power_per_nm")
        throw std::invalid_argument("MINE scintillationEmissionConvention must be photon_per_nm or power_per_nm");
    if(scintillationSpectrum!="monochromatic" && !isSpectralWlsModel(model))
        throw std::invalid_argument("MINE broadband scintillation requires spectral_wls or spectral_y11");
    if(coatingModel!="specular" && coatingModel!="diffuse")
        throw std::invalid_argument("MINE coatingModel must be specular or diffuse");
    if(!isCoatingMaterial(coatingMaterial))
        throw std::invalid_argument("MINE coatingMaterial must be constant, fnal_tio2, tyvek or aluminized_mylar");
    if((coatingMaterial=="fnal_tio2" || coatingMaterial=="tyvek") && coatingModel!="diffuse")
        throw std::invalid_argument("MINE fnal_tio2 and tyvek require coatingModel=diffuse");
    if(coatingMaterial=="aluminized_mylar" && coatingModel!="specular")
        throw std::invalid_argument("MINE aluminized_mylar requires coatingModel=specular");
    if(!std::isfinite(coatingReferenceAlbedo) || coatingReferenceAlbedo<0 || coatingReferenceAlbedo>1)
        throw std::invalid_argument("MINE coatingReferenceAlbedo must be in [0,1]");
    if(coatingMaterial!="tyvek" && coatingMaterial!="aluminized_mylar" && coatingReferenceAlbedo!=1)
        throw std::invalid_argument("MINE coatingReferenceAlbedo applies only to Yang Tyvek/Mylar ratios");
    if((coatingMaterial=="tyvek" || coatingMaterial=="aluminized_mylar") && scintillationSpectrum=="monochromatic" &&
       (scintillationWavelengthNm<250 || scintillationWavelengthNm>800))
        throw std::invalid_argument("MINE monochromatic source is outside the 250-800 nm Yang coating support");
    if(grooveModel!="none" && grooveModel!="square" && grooveModel!="rectangular")
        throw std::invalid_argument("MINE grooveModel must be none, square or rectangular");
    if(geometry=="corner_fan" && grooveModel=="rectangular")
        throw std::invalid_argument("MINE rectangular groove currently requires mini_panel_spiral");
    if(!isGrooveMaterial(grooveMaterial))
        throw std::invalid_argument("MINE grooveMaterial must be air, gel or glue");
    if(grooveModel=="square") {
        if(!std::isfinite(grooveWidthMm) || !std::isfinite(grooveDepthMm) ||
           grooveWidthMm<=fiberDiameterMm || grooveDepthMm<=fiberDiameterMm ||
           std::abs(grooveWidthMm-grooveDepthMm)>1e-9 || std::abs(zMcMm-grooveDepthMm/2)>1e-9)
            throw std::invalid_argument("MINE square open groove requires width=depth > fiber diameter and zMcMm=depth/2");
        for(double dz:fiberZOffsetsMm) if(std::abs(dz)>1e-9)
            throw std::invalid_argument("MINE open grooves on one face require zero fiberZOffsetsMm");
    }
    if(diagonal!="bl_tr" && diagonal!="tl_br") throw std::invalid_argument("MINE invalid corner-fan diagonal");
    for(double value:{pitchMm,minBendRadiusMm,fiberDiameterMm,claddingFraction,readoutInsetMm,readoutWidthMm,readoutHeightMm,
                      nScintillator,nCore,nCladding,scintillationWavelengthNm,wlsWavelengthNm,wlsDecayNs,lightYieldPerMeV,scintillationDecayNs})
        if(!std::isfinite(value) || value<=0) throw std::invalid_argument("MINE optical dimensions, indices and emission parameters must be finite and positive");
    for(double value:{bulkAbsorptionLengthMm,coreAbsorptionLengthMm,claddingAbsorptionLengthMm,
                      effectiveFiberAttenuationMm,birksMmPerMeV})
        if(!std::isfinite(value) || value<0) throw std::invalid_argument("MINE optical attenuation lengths must be explicitly nonnegative; wlsAbsorptionLengthMm has no physical default");
    if(model=="monochromatic_effective" && (!std::isfinite(wlsAbsorptionLengthMm) || wlsAbsorptionLengthMm<0))
        throw std::invalid_argument("MINE monochromatic model requires wlsAbsorptionLengthMm >= 0");
    if(isSpectralWlsModel(model) && wlsAbsorptionLengthMm!=-1)
        throw std::invalid_argument("MINE spectral WLS derives absorption from concentration; remove legacy wlsAbsorptionLengthMm");
    if(wlsEmissionConvention!="photon_per_nm" && wlsEmissionConvention!="power_per_nm")
        throw std::invalid_argument("MINE wlsEmissionConvention must be photon_per_nm or power_per_nm");
    if(!std::isfinite(wlsDyeConcentrationPpm) || wlsDyeConcentrationPpm<=0 || wlsDyeConcentrationPpm>1e6)
        throw std::invalid_argument("MINE invalid wlsDyeConcentrationPpm");
    if(isSpectralWlsModel(model) && scintillationSpectrum=="monochromatic" && wlsRelativeAbsorption(fiberModel,scintillationWavelengthNm)<=0)
        throw std::invalid_argument("MINE spectral blue proxy is outside the selected WLS absorption support");
    if(fiberCount<1 || fiberCount>1000 || claddingFraction>=.5 || !std::isfinite(zMcMm) ||
       reflectivity<0 || reflectivity>1 || !std::isfinite(reflectivity) || wlsQuantumYield!=1 ||
       (model=="monochromatic_effective" && wlsWavelengthNm<=scintillationWavelengthNm) || nCore<=nCladding || nCladding<1 || nScintillator<1 ||
       scintillationWavelengthNm<=1239.841984332/6*(1+1e-8) || wlsWavelengthNm>=1239.841984332*(1-1e-8))
        throw std::invalid_argument("MINE invalid optical configuration; phase 1 requires unity WLS quantum yield");
    if(!fiberZOffsetsMm.empty() && fiberZOffsetsMm.size()!=static_cast<std::size_t>(fiberCount))
        throw std::invalid_argument("MINE fiberZOffsetsMm must contain one offset per fiber");
    for(double value:fiberZOffsetsMm) if(!std::isfinite(value)) throw std::invalid_argument("MINE non-finite vertical offset");
}
const Geant4OpticsConfig& ActiveGeant4OpticsConfig() {
    static const Geant4OpticsConfig c=[] {
        const char* path=std::getenv("MINE_GEANT4_CONFIG");
        if(!path || !*path) throw std::invalid_argument("MINE_GEANT4_CONFIG must name the explicit Geant4 optical assumptions");
        return Geant4OpticsConfig::Load(path);
    }();return c;
}
double OpticalFiberRoute::LengthMm() const {
    return straight ? (end-start).norm() : (tangent0-start).norm()+radiusMm*std::abs(deltaAngleRad)+(end-tangent1).norm();
}
OpticalPoint OpticalFiberRoute::PointAt(double s) const {
    if(s<0 || s>LengthMm()+tolerance) throw std::invalid_argument("MINE fiber coordinate outside route");
    if(straight) return start+(end-start).unit()*s;
    const double first=(tangent0-start).norm(),arc=radiusMm*std::abs(deltaAngleRad);
    if(s<=first) return start+(tangent0-start).unit()*s;
    if(s<=first+arc) {
        const double a=startAngleRad+deltaAngleRad*(s-first)/arc;
        return center+OpticalPoint{radiusMm*std::cos(a),radiusMm*std::sin(a),0};
    }
    return tangent1+(end-tangent1).unit()*(s-first-arc);
}
CornerFanOpticalModel BuildCornerFanOpticalModel(const Geant4OpticsConfig& c,double halfX,double halfY,double halfZ) {
    c.Validate();
    if(halfX<=0 || halfY<=0 || halfZ<=0 || std::abs(halfX-halfY)>tolerance)
        throw std::invalid_argument("MINE corner_fan optical model requires a square panel");
    if(c.grooveModel=="square" && c.grooveDepthMm>=2*halfZ)
        throw std::invalid_argument("MINE groove depth must be smaller than panel thickness");
    // Exact formulas from MC_MINE_LightTrapping/src/AlternativeGeometries.cpp,
    // ResolveCornerFanSizing + BuildDiagonalCornerFanPaths, snapshot 2026-11-02.
    // Keep the analytic circle instead of turning its sampled points into facets.
    const double axialHalf=std::sqrt(2.)*halfX,r=c.fiberDiameterMm/2;
    double pitch=c.pitchMm,inset=c.readoutInsetMm,width=c.readoutWidthMm;
    if(inset>=axialHalf || c.fiberCount*c.fiberDiameterMm>2*axialHalf)
        throw std::invalid_argument("MINE corner fan count/diameter or inset cannot fit panel");
    if(c.autoFit && c.fiberCount>1) {
        const double depth=std::max(inset,c.fiberDiameterMm),d2=depth*depth,r2=r*r;
        const double maxSlope=(d2-r2)/(d2+r*std::sqrt(2*d2-r2))*(1-1e-9);
        const double halfIndex=.5*(c.fiberCount-1);
        pitch=std::min(pitch,axialHalf*maxSlope/halfIndex);
        const double outerOffset=halfIndex*pitch,cosOuter=axialHalf/std::hypot(axialHalf,outerOffset);
        inset=std::max(depth,c.fiberDiameterMm*(1+1e-6)*axialHalf/(pitch*cosOuter));
    }
    const double outerOffset=.5*(c.fiberCount-1)*pitch,slope=outerOffset/axialHalf;
    if(c.autoFit) width=std::max(width,2*(inset*slope+r*std::hypot(1.,slope))+2*tolerance);
    if(inset>=axialHalf || outerOffset>=axialHalf-tolerance)
        throw std::invalid_argument("MINE corner fan cannot fit in panel");
    const double z=c.zMcMm-halfZ,inv=1/std::sqrt(2.);
    const OpticalPoint corner0{-halfX,c.diagonal=="bl_tr"?-halfY:halfY,z};
    const OpticalPoint corner1{halfX,-corner0.y,z},axis=(corner1-corner0).unit();
    const OpticalPoint widthAxis{-axis.y,axis.x,0},vertexDirection=c.diagonal=="bl_tr"?OpticalPoint{-inv,inv,0}:OpticalPoint{inv,inv,0};
    CornerFanOpticalModel result;result.effectivePitchMm=pitch;result.effectiveInsetMm=inset;result.mcToLocalZMm=-halfZ;
    result.readouts={OpticalReadoutPlane{corner0+axis*inset,axis*(-1),widthAxis,{0,0,1},width,c.readoutHeightMm},
                     OpticalReadoutPlane{corner1-axis*inset,axis,widthAxis,{0,0,1},width,c.readoutHeightMm}};
    for(int f=0;f<c.fiberCount;++f) {
        const double dz=c.fiberZOffsetsMm.empty()?0:c.fiberZOffsetsMm.at(f);
        if(z+dz-r< -halfZ-tolerance || z+dz+r>halfZ+tolerance ||
           std::abs(dz)+r>c.readoutHeightMm/2+tolerance)
            throw std::invalid_argument("MINE corner-fan vertical offset leaves panel or common readout");
        const auto shift=OpticalPoint{0,0,dz},s0=corner0+shift,s1=corner1+shift;
        const double offset=(f-.5*(c.fiberCount-1))*pitch;
        OpticalFiberRoute route;
        if(std::abs(offset)<tolerance) {
            route.start=s0+axis*inset;route.end=s1-axis*inset;route.straight=true;
        } else {
            const auto vertex=vertexDirection*offset+OpticalPoint{0,0,z+dz};
            const auto ray0=(s0-vertex).unit(),ray1=(s1-vertex).unit();
            const double phi=std::acos(std::clamp(ray0.dot(ray1),-1.,1.));
            route.radiusMm=1.01*c.minBendRadiusMm;
            const double tangentDistance=route.radiusMm/std::tan(.5*phi);
            if(tangentDistance>=(s0-vertex).norm()-tolerance || tangentDistance>=(s1-vertex).norm()-tolerance)
                throw std::invalid_argument("MINE corner-fan fillet does not fit");
            route.tangent0=vertex+ray0*tangentDistance;route.tangent1=vertex+ray1*tangentDistance;
            route.center=vertex+(ray0+ray1).unit()*(route.radiusMm/std::sin(.5*phi));
            const auto r0=route.tangent0-route.center,r1=route.tangent1-route.center;
            route.startAngleRad=std::atan2(r0.y,r0.x);
            double delta=std::atan2(r1.y,r1.x)-route.startAngleRad;
            while(delta<=-pi) delta+=2*pi;
            while(delta>pi) delta-=2*pi;
            const auto incoming=(route.tangent0-s0).unit(),outgoing=(s1-route.tangent1).unit();
            const double turn=incoming.x*outgoing.y-incoming.y*outgoing.x;
            if(turn>0 && delta<0) delta+=2*pi;
            if(turn<0 && delta>0) delta-=2*pi;
            if(std::abs(delta)>pi+tolerance) throw std::logic_error("MINE corner-fan wrong circular arc");
            route.deltaAngleRad=delta;
            const double first=(route.tangent0-s0).dot(axis),last=(s1-route.tangent1).dot(axis);
            if(inset>=first-tolerance || inset>=last-tolerance) throw std::invalid_argument("MINE readout plane cuts curved section");
            route.start=s0+(route.tangent0-s0)*(inset/first);
            route.end=s1+(route.tangent1-s1)*(inset/last);
        }
        // Full elliptical end footprint, not just centerline coverage.
        for(int end=0;end<2;++end) {
            const auto& plane=result.readouts[end];const auto p=end?route.end:route.start;
            const auto tangent=route.straight?(route.end-route.start).unit():
                (end?(route.end-route.tangent1).unit():(route.tangent0-route.start).unit());
            const double ellipse=r/std::abs(tangent.dot(plane.normal));
            if(std::abs((p-plane.center).dot(widthAxis))+ellipse>width/2+tolerance)
                throw std::invalid_argument("MINE full oblique fiber footprint exceeds sensor width");
        }
        result.fibers.push_back(route);
    }
    return result;
}
} // namespace mine
