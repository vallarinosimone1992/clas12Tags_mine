#ifndef MINE_OPTICAL_GEOMETRY_MODEL_H
#define MINE_OPTICAL_GEOMETRY_MODEL_H
// No Geant4/ROOT dependency: the exact centerline and configuration can be
// checked independently of the transport executable.
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace mine {
struct OpticalPoint {
    double x=0,y=0,z=0;
    OpticalPoint operator+(const OpticalPoint& b) const { return {x+b.x,y+b.y,z+b.z}; }
    OpticalPoint operator-(const OpticalPoint& b) const { return {x-b.x,y-b.y,z-b.z}; }
    OpticalPoint operator*(double a) const { return {a*x,a*y,a*z}; }
    double dot(const OpticalPoint& b) const { return x*b.x+y*b.y+z*b.z; }
    double norm() const { return std::sqrt(dot(*this)); }
    OpticalPoint unit() const { return (*this)*(1/norm()); }
};
struct Geant4OpticsConfig {
    std::string model="monochromatic_effective", fiberModel="Y-11", diagonal="bl_tr";
    // Explicit transport layouts are separate from the historical effective layouts.
    std::string geometry="corner_fan",sipmModel="ideal";
    double sensorGelGapMm=1,tentAngleDeg=30,tentClearanceMm=0,miniCurveToleranceMm=.0002;
    // Zero density inherits the base Geant4 material; zero rise keeps historical emission.
    double scintillationRiseNs=0,scintillatorDensityGcm3=0;
    // spectral_wls selects Y-11/YS-6/YS-4; spectral_y11 remains a compatible alias.
    double wlsDyeConcentrationPpm=200;
    std::string wlsEmissionConvention="photon_per_nm";
    // Independent of the fiber model; only the photon-energy distribution changes.
    std::string scintillationSpectrum="monochromatic",scintillationEmissionConvention="photon_per_nm";
    int fiberCount=15,sensorCount=2;
    double pitchMm=100,minBendRadiusMm=100,zMcMm=.5;
    double fiberDiameterMm=1,claddingFraction=.02;
    double readoutInsetMm=10,readoutWidthMm=20,readoutHeightMm=1.5;
    // Open groove on the z_MC=0 face; centered fiber at depth/2.
    std::string grooveModel="none",grooveMaterial="gel";
    double grooveWidthMm=2,grooveDepthMm=2;
    bool autoFit=true,checkOverlaps=true;
    std::vector<double> fiberZOffsetsMm;
    // Opaque effective outer coating: elastic specular or Lambertian reflection.
    std::string coatingModel="specular",coatingMaterial="constant";
    // Yang curves are ratios to BaSO4: unity is an explicit ideal-reference
    // approximation. Does not scale the untuned FNAL table or constant mode.
    double coatingReferenceAlbedo=1;
    double nScintillator=1.58,nCore=1.59,nCladding=1.49,reflectivity=.98;
    double scintillationWavelengthNm=425,wlsWavelengthNm=476,wlsDecayNs=6.9;
    // -1 is deliberately invalid: no microscopic WLS length is available in
    // the MC/catalogue. A run must explicitly select this effective hypothesis.
    double wlsAbsorptionLengthMm=-1,wlsQuantumYield=1;
    double bulkAbsorptionLengthMm=1000,coreAbsorptionLengthMm=0,claddingAbsorptionLengthMm=0;
    // Shared ordinary loss along the actual photon path (finite in active configs).
    // It is never inferred from the catalogue's axial attenuation length.
    double effectiveFiberAttenuationMm=0;
    // kuraray_spectral: common host loss + separately budgeted weak WLS tail.
    std::string fiberLossModel="none";
    double birksMmPerMeV=.126,lightYieldPerMeV=8000,scintillationDecayNs=2.5;
    std::uint64_t fingerprint=0;
    std::string sourcePath;
    static Geant4OpticsConfig Load(const std::string& path);
    void Validate() const;
};
const Geant4OpticsConfig& ActiveGeant4OpticsConfig();

struct OpticalFiberRoute {
    OpticalPoint start,tangent0,center,tangent1,end;
    double radiusMm=0,startAngleRad=0,deltaAngleRad=0;
    bool straight=false;
    double LengthMm() const;
    OpticalPoint PointAt(double sMm) const;
};
struct OpticalReadoutPlane {
    OpticalPoint center,normal,widthAxis,heightAxis{0,0,1};
    double widthMm=0,heightMm=0;
};
struct CornerFanOpticalModel {
    std::vector<OpticalFiberRoute> fibers;
    std::array<OpticalReadoutPlane,2> readouts;
    double effectivePitchMm=0,effectiveInsetMm=0;
    // MC uses z in [0,thickness]; GEMC panel coordinates use [-halfZ,+halfZ].
    double mcToLocalZMm=0;
};
CornerFanOpticalModel BuildCornerFanOpticalModel(const Geant4OpticsConfig&,
                                                double halfXmm,double halfYmm,double halfZmm);
} // namespace mine
#endif
