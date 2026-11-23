#include "mine_geant4_optics.h"
#include "mine_wls_spectrum.h"
#include "mine_fiber_loss.h"
#include "mine_scintillation_spectrum.h"
#include "mine_coating_spectrum.h"
#include "mine_groove_materials.h"
#include "G4Box.hh"
#include "G4IntersectionSolid.hh"
#include "G4Tubs.hh"
#include "G4CutTubs.hh"
#include "G4DisplacedSolid.hh"
#include "G4ExtrudedSolid.hh"
#include "G4GeometryTolerance.hh"
#include "G4IonisParamMat.hh"
#include "G4LogicalBorderSurface.hh"
#include "G4LogicalSkinSurface.hh"
#include "G4LogicalVolume.hh"
#include "G4Material.hh"
#include "G4MaterialPropertiesTable.hh"
#include "G4NistManager.hh"
#include "G4OpticalSurface.hh"
#include "G4OpBoundaryProcess.hh"
#include "G4ProcessManager.hh"
#include "G4ProcessVector.hh"
#include "G4PhysicalConstants.hh"
#include "G4PVPlacement.hh"
#include "G4RotationMatrix.hh"
#include "G4Step.hh"
#include "G4StepPoint.hh"
#include "G4SystemOfUnits.hh"
#include "G4Torus.hh"
#include "G4Track.hh"
#include "G4Transform3D.hh"
#include "G4TwoVector.hh"
#include "G4UnionSolid.hh"
#include "G4Version.hh"
#include "G4VisAttributes.hh"
#include "G4VisManager.hh"
#include "G4VUserVisAction.hh"
#include "G4Polyline.hh"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>

namespace mine {
namespace {
constexpr double pi=3.14159265358979323846;
std::map<const G4VPhysicalVolume*,OpticalVolumeInfo>& registry() {
    static std::map<const G4VPhysicalVolume*,OpticalVolumeInfo> entries;return entries;
}
std::vector<std::unique_ptr<Geant4OpticsGeometry>>& geometries() {
    static std::vector<std::unique_ptr<Geant4OpticsGeometry>> entries;return entries;
}
G4ThreeVector vec(const OpticalPoint& p) {return {p.x*mm,p.y*mm,p.z*mm};}
G4RotationMatrix axisRotation(const OpticalPoint& axis) {
    // Active local-to-panel transform, local cylinder z -> requested axis.
    G4RotationMatrix rotation;rotation.rotateY(std::acos(std::clamp(axis.z,-1.,1.)));
    rotation.rotateZ(std::atan2(axis.y,axis.x));return rotation;
}
G4VSolid* cutLine(const std::string& name,const OpticalPoint& a,const OpticalPoint& b,
                 double radius,const OpticalPoint& lowNormal,const OpticalPoint& highNormal) {
    const auto direction=(b-a).unit();const auto rotation=axisRotation(direction);
    const auto inverse=rotation.inverse();
    auto* cylinder=new G4CutTubs(name+"_primitive",0,radius*mm,(b-a).norm()*mm/2,0,2*pi,
                               inverse*vec(lowNormal),inverse*vec(highNormal));
    return new G4DisplacedSolid(name,cylinder,G4Transform3D(rotation,vec((a+b)*.5)));
}
G4MaterialPropertiesTable* dielectric(double index,double absorptionMm) {
    auto* table=new G4MaterialPropertiesTable;
    G4double energies[]={1*eV,6*eV},indices[]={index,index};
    table->AddProperty("RINDEX",energies,indices,2);
    // No attenuation property means explicitly disabled, never a zero-length
    // absorber. The 1000 mm bulk convention is intensity survival exp(-s/L)
    // for the actual optical ray length s in this material.
    if(absorptionMm>0) {
        G4double length[]={absorptionMm*mm,absorptionMm*mm};
        table->AddProperty("ABSLENGTH",energies,length,2);
    }
    return table;
}
G4Material* grooveMaterial(const std::string& name,const std::string& material) {
    auto* nist=G4NistManager::Instance();
    // A fresh dispersive MPT is essential: replacing a constant RINDEX while
    // leaving its generated GROUPVEL cached would retain the wrong speed.
    auto* properties=material=="gel"?new G4MaterialPropertiesTable:dielectric(grooveRefractiveIndex(material,425),0);
    if(material=="gel") {
        // Dense wavelength sampling yields the physical dispersion/group speed.
        // Constant air/glue indices are explicit source-limited approximations.
        std::vector<G4double> energy,index;
        for(int i=0;i<=4000;++i) {
            const double e=1.+5.*i/4000.;
            energy.push_back(e*eV);index.push_back(grooveRefractiveIndex(material,opticalHcEvNm/e));
        }
        properties->AddProperty("RINDEX",energy.data(),index.data(),energy.size());
    }
    if(material=="air") {
        // Independent optical air; never mutate the global G4_AIR material.
        auto* result=new G4Material(name,1.20479*mg/cm3,3,kStateGas,293.15*kelvin,1*atmosphere);
        result->AddElement(nist->FindOrBuildElement("N"),.755267);
        result->AddElement(nist->FindOrBuildElement("O"),.231781);
        result->AddElement(nist->FindOrBuildElement("Ar"),.012952);
        result->SetMaterialPropertiesTable(properties);return result;
    }
    // Optical index and density are source based; commercial compositions are
    // proprietary. Charged-particle transport uses declared proxies: PDMS for
    // the gel, DGEBA epoxy resin for cured EJ-500, not a chemical certification.
    auto* result=new G4Material(name,(material=="gel"?1.06:1.17)*g/cm3,material=="gel"?4:3);
    if(material=="gel") {
        result->AddElement(nist->FindOrBuildElement("C"),2);
        result->AddElement(nist->FindOrBuildElement("H"),6);
        result->AddElement(nist->FindOrBuildElement("O"),1);
        result->AddElement(nist->FindOrBuildElement("Si"),1);
    } else {
        result->AddElement(nist->FindOrBuildElement("C"),21);
        result->AddElement(nist->FindOrBuildElement("H"),24);
        result->AddElement(nist->FindOrBuildElement("O"),4);
    }
    result->SetMaterialPropertiesTable(properties);return result;
}
void emissionProxy(G4MaterialPropertiesTable* table,const char* key,double wavelengthNm) {
    const double energy=h_Planck*c_light/(wavelengthNm*nm);
    // Geant4 needs strictly increasing energies. This numerical delta has a
    // relative half-width 1e-8, not an invented physical emission spectrum.
    G4double energies[]={energy*(1-1e-8),energy,energy*(1+1e-8)},values[]={0,1,0};
    table->AddProperty(key,energies,values,3);
}
G4Material* copyMaterial(const std::string& name,G4Material* source,G4MaterialPropertiesTable* properties,double densityGcm3=0) {
    // The density-derived G4Material(name,density,base) constructor shares the
    // base MPT pointer. SetMaterialPropertiesTable deletes an existing MPT in
    // 10.7.4, so using that constructor can destroy the scintillator's table
    // while creating an independent non-scintillating readout material.
    // Copy composition/state instead; every optical table has one owner.
    const auto elements=source->GetNumberOfElements();
    if(elements==0) throw std::invalid_argument("MINE cannot clone a material without elements");
    auto* result=new G4Material(name,densityGcm3>0?densityGcm3*g/cm3:source->GetDensity(),static_cast<int>(elements),
                                source->GetState(),source->GetTemperature(),source->GetPressure());
    const auto* fractions=source->GetFractionVector();
    if(!fractions && elements!=1) throw std::invalid_argument("MINE source material lacks mass fractions");
    for(std::size_t i=0;i<elements;++i)
        result->AddElement((*source->GetElementVector())[i],fractions?fractions[i]:1.);
    result->GetIonisation()->SetMeanExcitationEnergy(source->GetIonisation()->GetMeanExcitationEnergy());
    result->GetIonisation()->SetBirksConstant(source->GetIonisation()->GetBirksConstant());
    result->SetMaterialPropertiesTable(properties);return result;
}
G4OpticalSurface* absorber(const std::string& name,double reflectivity,bool diffuse=false) {
    // dielectric_metal is the effective opaque boundary model, not a claim
    // that TiO2 is a metal: R survives, 1-R is lost, no transmission/Fresnel
    // correction on top of the configured probability. The angular law is
    // delegated to the native UNIFIED boundary process.
    auto* surface=new G4OpticalSurface(name,unified,diffuse?ground:polished,dielectric_metal);
    auto* table=new G4MaterialPropertiesTable;
    G4double energies[]={1*eV,6*eV},reflection[]={reflectivity,reflectivity},efficiency[]={0,0};
    table->AddProperty("REFLECTIVITY",energies,reflection,2);
    table->AddProperty("EFFICIENCY",energies,efficiency,2);
    if(diffuse) {
        G4double zero[]={0,0};
        table->AddProperty("SPECULARLOBECONSTANT",energies,zero,2);
        table->AddProperty("SPECULARSPIKECONSTANT",energies,zero,2);
        table->AddProperty("BACKSCATTERCONSTANT",energies,zero,2);
        // Their complement is 100% Lambertian conditional on reflection:
        // p(Omega)=cos(theta)/pi, NOT uniform solid angle. No WLS or delay.
        surface->SetSigmaAlpha(0);
    }
    surface->SetMaterialPropertiesTable(table);return surface;
}
double combineAbsorption(double microscopic,double effective) {
    const double inverse=(microscopic>0?1/microscopic:0)+(effective>0?1/effective:0);
    return inverse>0?1/inverse:0;
}
std::vector<OpticalPoint> clipReadoutPolygon(std::vector<OpticalPoint> polygon,double halfX,double halfY) {
    // Analytic Sutherland--Hodgman clipping, avoiding rejection sampling of a
    // tiny sensor intersected with the million-times-larger panel Box.
    for(int boundary=0;boundary<4;++boundary) {
        const auto distance=[&](const OpticalPoint& p) {
            return boundary==0?halfX-p.x:boundary==1?halfX+p.x:boundary==2?halfY-p.y:halfY+p.y;
        };
        std::vector<OpticalPoint> clipped;
        for(std::size_t i=0;i<polygon.size();++i) {
            const auto& a=polygon[i];const auto& b=polygon[(i+1)%polygon.size()];
            const double da=distance(a),db=distance(b);
            if(da>=0) clipped.push_back(a);
            if((da>=0)!=(db>=0)) clipped.push_back(a+(b-a)*(da/(da-db)));
        }
        polygon=std::move(clipped);
        if(polygon.empty()) break;
    }
    return polygon;
}
}
G4OpticalSurface* BuildGeant4CoatingSurface(const std::string& name,const Geant4OpticsConfig& config) {
    config.Validate();
    auto* surface=absorber(name,config.reflectivity,config.coatingModel=="diffuse");
    if(config.coatingMaterial!="constant") {
        auto spectrum=coatingPropertyVector(config);
        for(auto& energy:spectrum.energyEv) energy*=eV;
        auto* table=surface->GetMaterialPropertiesTable();
        table->RemoveProperty("REFLECTIVITY");
        table->AddProperty("REFLECTIVITY",spectrum.energyEv.data(),spectrum.reflectivity.data(),spectrum.energyEv.size());
    }
    return surface;
}
const OpticalVolumeInfo* FindOpticalVolume(const G4VPhysicalVolume* volume) {
    const auto found=registry().find(volume);return found==registry().end()?nullptr:&found->second;
}
void ResetGeant4OpticsRegistry() { registry().clear();geometries().clear(); }
bool SynchronizeOpticalVelocity(const G4Step* step) {
#if G4VERSION_NUMBER == 1074
    auto* track=step->GetTrack();auto* post=step->GetPostStepPoint();
    if(track->GetTrackStatus()!=fAlive || post->GetStepStatus()!=fGeomBoundary || !post->GetPhysicalVolume()) return false;
    auto* processes=track->GetDefinition()->GetProcessManager()->GetProcessList();
    bool relocation=false;
    for(std::size_t i=0;i<processes->size();++i) {
        const auto* boundary=dynamic_cast<G4OpBoundaryProcess*>((*processes)[i]);
        if(boundary && boundary->GetStatus()==StepTooSmall) {relocation=true;break;}
    }
    if(!relocation) return false;
    // At the zero relocation step the destination volume is the physical
    // medium for the next flight. Unlike a reflected finite boundary step,
    // this step performs no additional Fresnel interaction.
    const auto* material=post->GetPhysicalVolume()->GetLogicalVolume()->GetMaterial();
    auto* properties=material->GetMaterialPropertiesTable();
    auto* group=properties?properties->GetProperty("GROUPVEL"):nullptr;
    if(!group) throw std::runtime_error("MINE optical relocation destination lacks GROUPVEL");
    const double velocity=group->Value(step->GetPreStepPoint()->GetKineticEnergy());
    if(!std::isfinite(velocity) || velocity<=0 || velocity>c_light)
        throw std::runtime_error("MINE optical relocation destination has invalid GROUPVEL");
    post->SetVelocity(velocity);track->SetVelocity(velocity);track->UseGivenVelocity(true);
    return true;
#else
    (void)step;return false;
#endif
}
G4VSolid* BuildOpticalFiberSolid(const std::string& name,const OpticalFiberRoute& route,
    double radius,const std::array<OpticalReadoutPlane,2>& readouts) {
    if(radius<=0) throw std::invalid_argument("MINE fiber radius must be positive");
    if(route.straight) return cutLine(name,route.start,route.end,radius,readouts[0].normal,readouts[1].normal);
    const auto direction0=(route.tangent0-route.start).unit(),direction1=(route.end-route.tangent1).unit();
    // Boolean weld: extending the straight into the exact tangent torus avoids
    // coincident operand caps. 1e-4 mm changes the outer envelope by at most
    // weld^2/(2R), far below G4's 1e-9 mm surface tolerance for R>=100 mm.
    // These are overlapping operands of ONE solid, never overlapping physical
    // volumes or an optical boundary at the join.
    const double weld=1e-4;
    if(weld*weld/(2*(route.radiusMm-radius))*mm>G4GeometryTolerance::GetInstance()->GetSurfaceTolerance())
        throw std::invalid_argument("MINE Boolean weld exceeds geometry surface tolerance");
    auto* first=cutLine(name+"_input",route.start,route.tangent0+direction0*weld,radius,readouts[0].normal,direction0);
    auto* last=cutLine(name+"_output",route.tangent1-direction1*weld,route.end,radius,direction1*(-1),readouts[1].normal);
    const double begin=route.deltaAngleRad>0?route.startAngleRad:route.startAngleRad+route.deltaAngleRad;
    double start=std::fmod(begin+4*pi,2*pi);
    auto* arcPrimitive=new G4Torus(name+"_torus",0,radius*mm,route.radiusMm*mm,start,std::abs(route.deltaAngleRad));
    auto* arc=new G4DisplacedSolid(name+"_arc",arcPrimitive,G4Transform3D(G4RotationMatrix(),vec(route.center)));
    auto* joined=new G4UnionSolid(name+"_line_arc",first,arc);
    return new G4UnionSolid(name,joined,last);
}
G4VSolid* BuildOpticalGrooveSolid(const std::string& name,const CornerFanOpticalModel& model,
    double width,double depth) {
    if(width<=0 || depth<=0 || model.fibers.empty()) throw std::invalid_argument("MINE invalid groove dimensions");
    // Balanced ordinary unions retain correct DistanceToOut normals in the
    // installed Geant4 10.7.4. MultiUnion's exit-normal path gave zero normals
    // when nested in the panel clipping intersection; never suppress that error.
    std::vector<G4VSolid*> pieces;
    const auto add=[&](G4VSolid* shape,const G4Transform3D& transform) {
        pieces.push_back(new G4DisplacedSolid(name+"_piece_"+std::to_string(pieces.size()),shape,transform));
    };
    const double half=width/2,weld=1e-4;
    const auto line=[&](const std::string& key,const OpticalPoint& a,const OpticalPoint& b,
                        const OpticalPoint& lowNormal,const OpticalPoint& highNormal) {
        const auto direction=(b-a).unit(),normal=OpticalPoint{-direction.y,direction.x,0};
        std::vector<OpticalPoint> polygon;
        for(const auto& cap:{std::pair<OpticalPoint,OpticalPoint>{a,lowNormal},{b,highNormal}})
            for(double sign:{-1.,1.}) {
                const auto transverse=normal*(sign*half);
                polygon.push_back(cap.first+transverse-direction*(transverse.dot(cap.second)/direction.dot(cap.second)));
            }
        // Order around the rectangle (including oblique endpoint faces).
        std::swap(polygon[2],polygon[3]);
        double area=0;
        for(std::size_t i=0;i<polygon.size();++i) area+=polygon[i].x*polygon[(i+1)%polygon.size()].y-polygon[(i+1)%polygon.size()].x*polygon[i].y;
        if(area>0) std::reverse(polygon.begin(),polygon.end());
        std::vector<G4TwoVector> vertices;
        for(const auto& p:polygon) vertices.emplace_back(p.x*mm,p.y*mm);
        auto* shape=new G4ExtrudedSolid(key,vertices,depth*mm/2,G4TwoVector(),1,G4TwoVector(),1);
        G4Transform3D transform(G4RotationMatrix(),G4ThreeVector(0,0,a.z*mm));
        add(shape,transform);
    };
    for(std::size_t f=0;f<model.fibers.size();++f) {
        const auto key=name+"_"+std::to_string(f);const auto& route=model.fibers[f];
        if(route.straight) {line(key,route.start,route.end,model.readouts[0].normal,model.readouts[1].normal);continue;}
        if(route.radiusMm<=half || weld*weld/(2*(route.radiusMm-half))*mm>G4GeometryTolerance::GetInstance()->GetSurfaceTolerance())
            throw std::invalid_argument("MINE groove bend radius is too small");
        const auto direction0=(route.tangent0-route.start).unit(),direction1=(route.end-route.tangent1).unit();
        line(key+"_input",route.start,route.tangent0+direction0*weld,model.readouts[0].normal,direction0);
        line(key+"_output",route.tangent1-direction1*weld,route.end,direction1*(-1),model.readouts[1].normal);
        const double begin=route.deltaAngleRad>0?route.startAngleRad:route.startAngleRad+route.deltaAngleRad;
        const double start=std::fmod(begin+4*pi,2*pi);
        auto* arc=new G4Tubs(key+"_arc",(route.radiusMm-half)*mm,(route.radiusMm+half)*mm,depth*mm/2,start,std::abs(route.deltaAngleRad));
        G4Transform3D transform(G4RotationMatrix(),vec(route.center));add(arc,transform);
    }
    // Merging real touching/crossing grooves avoids overlapping sibling volumes
    // or spurious dielectric interfaces between equal filling materials.
    int generation=0;
    while(pieces.size()>1) {
        std::vector<G4VSolid*> combined;
        for(std::size_t i=0;i<pieces.size();i+=2)
            combined.push_back(i+1==pieces.size()?pieces[i]:new G4UnionSolid(
                name+"_join_"+std::to_string(generation)+"_"+std::to_string(i),pieces[i],pieces[i+1]));
        pieces=std::move(combined);++generation;
    }
    return pieces.front();
}
void ConfigureGeant4FiberAbsorption(G4MaterialPropertiesTable* table,const Geant4OpticsConfig& c,bool core) {
    c.Validate();
    if(table->GetProperty("ABSLENGTH"))table->RemoveProperty("ABSLENGTH");
    if(usesKurarayFiberLoss(c)) {
        auto energy=fiberSpectralEnergyGridEv();std::vector<G4double> lengths;
        for(double e:energy)lengths.push_back(mm/fiberOrdinaryRatePerMm(c.fiberModel,opticalHcEvNm/e));
        for(double& e:energy)e*=eV;
        table->AddProperty("ABSLENGTH",energy.data(),lengths.data(),energy.size());return;
    }
    const double length=combineAbsorption(core?c.coreAbsorptionLengthMm:c.claddingAbsorptionLengthMm,c.effectiveFiberAttenuationMm);
    if(length>0) {
        G4double energies[]={1*eV,6*eV},lengths[]={length*mm,length*mm};
        table->AddProperty("ABSLENGTH",energies,lengths,2);
    }
}
void ConfigureGeant4WLS(G4MaterialPropertiesTable* coreProperties,const Geant4OpticsConfig& c) {
    c.Validate();
    if(isSpectralWlsModel(c.model)) {
        const auto spectral=fiberWlsPropertyVectors(c);
        std::vector<G4double> energies,lengths;
        for(double energy:spectral.energyEv) energies.push_back(energy*eV);
        for(double length:spectral.absorptionLengthMm) lengths.push_back(length*mm);
        auto emission=spectral.emissionPerEV;
        coreProperties->AddProperty("WLSABSLENGTH",energies.data(),lengths.data(),energies.size());
        coreProperties->AddProperty("WLSCOMPONENT",energies.data(),emission.data(),energies.size());
        coreProperties->AddConstProperty("WLSTIMECONSTANT",c.wlsDecayNs*ns);
        // Unity quantum yield is unchanged. Each reabsorption is a real WLS
        // interaction, with one secondary and its own exponential delay.
    } else if(c.wlsAbsorptionLengthMm>0) {
        // Two photon energies only: no measured absorption spectrum is implied.
        // Re-emitted photons lie below the blue band and are not absorbed a
        // second time. The sharp edge is an explicit effective assumption.
        const double blue=h_Planck*c_light/(c.scintillationWavelengthNm*nm);
        const double green=h_Planck*c_light/(c.wlsWavelengthNm*nm);
        const double edge=(blue+green)/2;
        G4double energies[]={1*eV,edge*(1-1e-8),edge*(1+1e-8),6*eV};
        G4double lengths[]={DBL_MAX,DBL_MAX,c.wlsAbsorptionLengthMm*mm,c.wlsAbsorptionLengthMm*mm};
        coreProperties->AddProperty("WLSABSLENGTH",energies,lengths,4);
        emissionProxy(coreProperties,"WLSCOMPONENT",c.wlsWavelengthNm);
        coreProperties->AddConstProperty("WLSTIMECONSTANT",c.wlsDecayNs*ns);
        // Omit WLSMEANNUMBERPHOTONS: default is exactly one photon. Setting it
        // to 1 would instead introduce a Poisson multiplicity in Geant4.
    }
}
G4Material* CreateGeant4ScintillatorMaterial(const std::string& name,G4Material* source,const Geant4OpticsConfig& c) {
    auto* scintProperties=dielectric(c.nScintillator,c.bulkAbsorptionLengthMm);
#if G4VERSION_NUMBER >= 1100
    const char* scintillationKey="SCINTILLATIONCOMPONENT1";
    scintProperties->AddConstProperty("SCINTILLATIONTIMECONSTANT1",c.scintillationDecayNs*ns);
    scintProperties->AddConstProperty("SCINTILLATIONYIELD1",1.);
    if(c.scintillationRiseNs>0)scintProperties->AddConstProperty("SCINTILLATIONRISETIME1",c.scintillationRiseNs*ns);
#else
    const char* scintillationKey="FASTCOMPONENT";
    scintProperties->AddConstProperty("FASTTIMECONSTANT",c.scintillationDecayNs*ns);
    scintProperties->AddConstProperty("YIELDRATIO",1.);
    if(c.scintillationRiseNs>0)scintProperties->AddConstProperty("FASTSCINTILLATIONRISETIME",c.scintillationRiseNs*ns);
#endif
    if(c.scintillationSpectrum=="monochromatic") {
        emissionProxy(scintProperties,scintillationKey,c.scintillationWavelengthNm);
    } else {
        auto spectrum=scintillationPropertyVector(c.scintillationSpectrum,c.scintillationEmissionConvention=="power_per_nm");
        for(auto& energy:spectrum.energyEv) energy*=eV;
        scintProperties->AddProperty(scintillationKey,spectrum.energyEv.data(),spectrum.emissionPerEV.data(),spectrum.energyEv.size());
    }
    scintProperties->AddConstProperty("SCINTILLATIONYIELD",c.lightYieldPerMeV/MeV);
    scintProperties->AddConstProperty("RESOLUTIONSCALE",1.);
    auto* scint=copyMaterial(name,source,scintProperties,c.scintillatorDensityGcm3);
    scint->GetIonisation()->SetBirksConstant(c.birksMmPerMeV*mm/MeV);return scint;
}
G4Material* CreateGeant4OpticalMaterial(const std::string& name,G4Material* source,double index,double absorptionMm) {
    return copyMaterial(name,source,dielectric(index,absorptionMm));
}
G4Material* CreateGeant4GrooveMaterial(const std::string& name,const std::string& material) {return grooveMaterial(name,material);}
void RegisterGeant4OpticalVolume(G4VPhysicalVolume* p,const OpticalVolumeInfo& info) {registry()[p]=info;}
const Geant4OpticsGeometry& StoreGeant4OpticalGeometry(std::unique_ptr<Geant4OpticsGeometry> g) {
    geometries().push_back(std::move(g));return *geometries().back();
}
const Geant4OpticsGeometry& BuildGeant4Optics(G4VPhysicalVolume* panel,G4VPhysicalVolume* environment,
    int panelId,const Geant4OpticsConfig& c) {
    c.Validate();
    if(c.geometry=="mini_panel_spiral")return BuildMiniGeant4Optics(panel,environment,panelId,c);
    if(!panel || !environment || panel==environment) throw std::invalid_argument("MINE optics needs panel and containing environment");
    if(FindOpticalVolume(panel)) throw std::invalid_argument("MINE optical panel already constructed");
    auto* panelLV=panel->GetLogicalVolume();auto* box=dynamic_cast<G4Box*>(panelLV->GetSolid());
    if(!box) throw std::invalid_argument("MINE explicit optical phase 1 requires a Box panel");
    auto geometry=std::make_unique<Geant4OpticsGeometry>();geometry->panel=panel;
    geometry->model=BuildCornerFanOpticalModel(c,box->GetXHalfLength()/mm,box->GetYHalfLength()/mm,box->GetZHalfLength()/mm);
    const std::string prefix="MINE_optical_"+std::to_string(panelId);
    auto* nist=G4NistManager::Instance();
    auto* scint=CreateGeant4ScintillatorMaterial(prefix+"_scintillator",panelLV->GetMaterial(),c);
    panelLV->SetMaterial(scint);
    auto* coreProperties=dielectric(c.nCore,0);
    ConfigureGeant4FiberAbsorption(coreProperties,c,true);
    ConfigureGeant4WLS(coreProperties,c);
    auto* core=copyMaterial(prefix+"_core",nist->FindOrBuildMaterial("G4_POLYSTYRENE"),coreProperties);
    auto* cladProperties=dielectric(c.nCladding,0);
    ConfigureGeant4FiberAbsorption(cladProperties,c,false);
    auto* clad=copyMaterial(prefix+"_cladding",nist->FindOrBuildMaterial("G4_PLEXIGLASS"),cladProperties);
    // The readout slab is an ideal absorber, not a scintillating detector
    // material. Keep its particle-transport composition identical to displaced
    // plastic; omit every emission property so deposits there do not create a
    // second light source inside the black optical readout.
    auto* readoutMaterial=copyMaterial(prefix+"_readout",scint,dielectric(c.nScintillator,0));
    registry()[panel]={OpticalVolumeRole::Panel,panelId,-1,-1};
    // Border only: a panel skin would coat all fiber interfaces as well.
    auto* outerCoating=BuildGeant4CoatingSurface(prefix+"_reflector",c);
    new G4LogicalBorderSurface(prefix+"_outer_coating",panel,environment,outerCoating);
    G4LogicalVolume* fiberMother=panelLV;
    if(c.grooveModel=="square") {
        auto* channels=BuildOpticalGrooveSolid(prefix+"_groove_network",geometry->model,c.grooveWidthMm,c.grooveDepthMm);
        // At oblique readouts some square-groove corners reach a panel edge;
        // clip only the filling envelope, retaining every physical fiber.
        auto* envelope=new G4IntersectionSolid(prefix+"_grooves_inside_panel",channels,box);
        auto* lv=new G4LogicalVolume(envelope,grooveMaterial(prefix+"_"+c.grooveMaterial,c.grooveMaterial),prefix+"_grooves");
        auto* pv=new G4PVPlacement(nullptr,{},lv,prefix+"_grooves",panelLV,false,0,false);
        registry()[pv]={OpticalVolumeRole::Filler,panelId,-1,-1};
        geometry->filler=pv;fiberMother=lv;
        // The existing external wrapping covers the flush-filled openings too;
        // all plastic/filling/cladding interfaces remain bare dielectrics.
        new G4LogicalBorderSurface(prefix+"_groove_opening_coating",pv,environment,outerCoating);
        lv->SetVisAttributes(G4VisAttributes::GetInvisible());
    }
    const double outerRadius=c.fiberDiameterMm/2,coreRadius=outerRadius-c.claddingFraction*c.fiberDiameterMm;
    for(std::size_t f=0;f<geometry->model.fibers.size();++f) {
        const auto name=prefix+"_fiber_"+std::to_string(f);
        auto* claddingLV=new G4LogicalVolume(BuildOpticalFiberSolid(name+"_outer",geometry->model.fibers[f],outerRadius,geometry->model.readouts),clad,name+"_cladding");
        auto* coreLV=new G4LogicalVolume(BuildOpticalFiberSolid(name+"_inner",geometry->model.fibers[f],coreRadius,geometry->model.readouts),core,name+"_core");
        auto* claddingPV=new G4PVPlacement(nullptr,{},claddingLV,name+"_cladding",fiberMother,false,static_cast<int>(f),false);
        auto* corePV=new G4PVPlacement(nullptr,{},coreLV,name+"_core",claddingLV,false,static_cast<int>(f),false);
        // At z_MC = outer radius the cladding touches the outside bottom
        // face. Geant4 can navigate directly from that daughter to the world.
        // The same exterior wrap covers that contact, while every internal
        // cladding/scintillator/core boundary remains an uncoated dielectric.
        new G4LogicalBorderSurface(name+"_exterior_coating",claddingPV,environment,outerCoating);
        registry()[claddingPV]={OpticalVolumeRole::Cladding,panelId,static_cast<int>(f),-1};
        registry()[corePV]={OpticalVolumeRole::Core,panelId,static_cast<int>(f),-1};
        geometry->cores.push_back(corePV);geometry->claddings.push_back(claddingPV);
        // Boolean polyhedron generation stalls in HepPolyhedron for thin,
        // large-radius torus unions. Hide only their graphical representation.
        claddingLV->SetVisAttributes(G4VisAttributes::GetInvisible());
        coreLV->SetVisAttributes(G4VisAttributes::GetInvisible());
    }
    // Ideal common finite readouts, absorbing direct plastic illumination too.
    // Clip their mechanical proxy to the panel: MC planes may extend beyond the
    // plastic in z, whereas a Geant4 daughter must remain inside its mother.
    for(int s=0;s<2;++s) {
        const auto& plane=geometry->model.readouts[s];const auto name=prefix+"_sensor_"+std::to_string(s);
        constexpr double thickness=.01; // numerical absorbing-volume thickness, not optical dead layer
        // rotation's local x is not guaranteed equal to widthAxis; construct
        // basis explicitly so the 20 mm dimension lies in the panel plane.
        const auto normal=vec(plane.normal),widthAxis=vec(plane.widthAxis);
        G4RotationMatrix basis(widthAxis,normal.cross(widthAxis),normal);
        const auto center=plane.center+plane.normal*(thickness/2);
        const double halfZ=box->GetZHalfLength()/mm;
        const double zMin=std::max(-halfZ,plane.center.z-plane.heightMm/2);
        const double zMax=std::min(halfZ,plane.center.z+plane.heightMm/2);
        if(zMax<=zMin) throw std::invalid_argument("MINE readout has no height inside scintillator");
        const double zCenter=(zMax+zMin)/2;
        std::vector<OpticalPoint> polygon;
        for(const auto& uv:{std::pair<double,double>{-1,-1},{-1,1},{1,1},{1,-1}})
            polygon.push_back(center+plane.widthAxis*(uv.first*plane.widthMm/2)+plane.normal*(uv.second*thickness/2));
        const double halfX=box->GetXHalfLength()/mm,halfY=box->GetYHalfLength()/mm;
        bool fits=true;
        for(const auto& point:polygon)
            for(double z:{zMin,zMax})
                fits=fits && std::abs(point.x)<=halfX && std::abs(point.y)<=halfY && std::abs(z)<=halfZ;
        G4VSolid* solid=nullptr;G4Transform3D placement;
        if(fits) {
            solid=new G4Box(name+"_shape",plane.widthMm*mm/2,(zMax-zMin)*mm/2,thickness*mm/2);
            placement=G4Transform3D(basis,G4ThreeVector(center.x*mm,center.y*mm,zCenter*mm));
        } else {
            polygon=clipReadoutPolygon(std::move(polygon),halfX,halfY);
            if(polygon.size()<3) throw std::invalid_argument("MINE readout polygon is outside scintillator");
            double area=0;
            for(std::size_t i=0;i<polygon.size();++i) {
                const auto& a=polygon[i];const auto& b=polygon[(i+1)%polygon.size()];area+=a.x*b.y-b.x*a.y;
            }
            if(area>0) std::reverse(polygon.begin(),polygon.end()); // G4 polygon convention: clockwise
            std::vector<G4TwoVector> vertices;
            for(const auto& p:polygon) vertices.emplace_back(p.x*mm,p.y*mm);
            solid=new G4ExtrudedSolid(name+"_shape",vertices,(zMax-zMin)*mm/2,G4TwoVector(),1,G4TwoVector(),1);
            placement=G4Transform3D(G4RotationMatrix(),G4ThreeVector(0,0,zCenter*mm));
        }
        auto* lv=new G4LogicalVolume(solid,readoutMaterial,name);
        auto* pv=new G4PVPlacement(placement,lv,name,panelLV,false,s,false);
        new G4LogicalSkinSurface(name+"_absorption",lv,absorber(name+"_ideal",0));
        registry()[pv]={OpticalVolumeRole::Sensor,panelId,-1,s};geometry->sensors.push_back(pv);
        lv->SetVisAttributes(new G4VisAttributes(G4Colour(1.,.2,.2)));
    }
    if(c.checkOverlaps) {
        bool overlap=false;
        if(geometry->filler) overlap=geometry->filler->CheckOverlaps(4000,0,true,5)||overlap;
        for(auto* p:geometry->claddings) overlap=p->CheckOverlaps(2000,0,true,5)||overlap;
        for(auto* p:geometry->cores) overlap=p->CheckOverlaps(2000,0,true,5)||overlap;
        for(auto* p:geometry->sensors) overlap=p->CheckOverlaps(2000,0,true,5)||overlap;
        if(overlap) throw std::runtime_error("MINE explicit optical geometry overlap detected");
    }
    std::cout<<"MINE Geant4 optics "<<G4Version<<"; panel="<<panelId<<"; fibers="<<c.fiberCount
             <<"; fiber_model="<<c.fiberModel<<"; PS/PMMA single cladding radial fraction="<<c.claddingFraction
             <<"; pitch_mm="<<geometry->model.effectivePitchMm<<"; inset_mm="<<geometry->model.effectiveInsetMm
             <<"; sensor_width_mm="<<geometry->model.readouts[0].widthMm
             <<"; z_MC_to_local_mm="<<geometry->model.mcToLocalZMm
             <<"; groove_model="<<c.grooveModel<<"; groove_material="<<c.grooveMaterial
             <<"; groove_width_mm="<<c.grooveWidthMm<<"; groove_depth_mm="<<c.grooveDepthMm
             <<"; groove_n425="<<grooveRefractiveIndex(c.grooveMaterial,425)
             <<"; coating_model="<<c.coatingModel<<"; coating_material="<<c.coatingMaterial
             <<"; coating_R425="<<coatingReflectivity(c,425)<<"; coating_reference_albedo="<<c.coatingReferenceAlbedo
             <<"; bulk_exp_ray_length_mm="<<c.bulkAbsorptionLengthMm
             <<"; model="<<c.model
             <<"; scintillation_spectrum="<<c.scintillationSpectrum
             <<"; scintillation_convention="<<c.scintillationEmissionConvention
             <<"; WLS_L430_mm="<<(isSpectralWlsModel(c.model)?1/fiberWlsRatePerMm(c,430):c.wlsAbsorptionLengthMm)
             <<"; dye_ppm="<<c.wlsDyeConcentrationPpm
             <<"; emission_convention="<<c.wlsEmissionConvention
             <<"; fiber_loss_model="<<c.fiberLossModel
             <<"; effective_fiber_path_proxy_mm="<<c.effectiveFiberAttenuationMm
             <<"; config_hash="<<c.fingerprint<<std::endl;
    if(c.grooveModel=="square")
        std::cout<<"MINE NOTE: open grooves are flush-filled with "<<c.grooveMaterial
                 <<"; no absorption/scattering/emission in filling. Gel dispersion follows Thorlabs G608N3;"
                 <<" below 402 nm is unvalidated Cauchy extrapolation. Glue n=1.57 and air n=1.000277 are constant approximations;"
                 <<" gel/epoxy charged-particle compositions are proxies. See reference/grooves.md."<<std::endl;
    if(c.coatingMaterial=="tyvek" || c.coatingMaterial=="aluminized_mylar")
        std::cout<<"MINE NOTE: Yang2021 coating curve is relative to BaSO4, multiplied by coatingReferenceAlbedo="
                 <<c.coatingReferenceAlbedo<<" (unity approximates an ideal reference, not an absolute MINE contact calibration)."<<std::endl;
    if(c.coatingMaterial=="tyvek")
        std::cout<<"MINE NOTE: Tyvek uses an effective Lambertian angular law; its measured BRDF need not be purely Lambertian."<<std::endl;
    if(c.scintillationSpectrum=="fnal_extruded")
        std::cout<<"MINE NOTE: FNAL spectrum is a 310-nm-excited fluorescence proxy (PPO + POPOP), not a measured MIP spectrum. "
                 <<"Plastic self-absorption/re-emission and Y11 absorption below measured support are unresolved; see optics/scintillators/README.md."<<std::endl;
    if(c.scintillationSpectrum!="monochromatic")
        std::cout<<"MINE NOTE: Spectral source only; yield, decay, composition and constant bulk absorption remain independently configured."<<std::endl;
    geometries().push_back(std::move(geometry));return *geometries().back();
}
namespace {
class FiberCenterlineView final : public G4VUserVisAction {
public:
    void Draw() override {
        auto* vis=G4VVisManager::GetConcreteInstance();
        if(!vis) return;
        for(const auto& geometry:geometries()) {
            const auto rotation=geometry->panel->GetObjectRotationValue();
            const auto offset=geometry->panel->GetObjectTranslation();
            if(!geometry->fiberPaths.empty()) {
                for(double z:{-4.,4.}) {
                    G4Polyline line;
                    for(const auto& p:std::vector<OpticalPoint>{{-150,-250,z},{150,-250,z},{150,250,z},{-150,250,z},{-150,-250,z}})line.push_back(rotation*vec(p)+offset);
                    G4VisAttributes style(G4Colour(.35,.65,1.));line.SetVisAttributes(style);vis->Draw(line);
                }
            }
            for(const auto& points:geometry->fiberPaths) {
                G4Polyline line;for(const auto& p:points)line.push_back(rotation*vec(p)+offset);
                G4VisAttributes style(G4Colour(0.,.9,.4));style.SetLineWidth(2.);line.SetVisAttributes(style);vis->Draw(line);
            }
            for(const auto& fiber:geometry->model.fibers) {
                G4Polyline line;
                const auto length=fiber.LengthMm();
                const int count=std::max(2,static_cast<int>(std::ceil(length/2.)));
                for(int i=0;i<=count;++i)
                    line.push_back(rotation*vec(fiber.PointAt(length*i/count))+offset);
                G4VisAttributes style(G4Colour(0.,.9,.4));style.SetLineWidth(2.);
                line.SetVisAttributes(style);vis->Draw(line);
            }
        }
    }
};
}
void RegisterGeant4OpticsVisualization(G4VisManager* vis) {
    if(vis && !geometries().empty()) {
        for(const auto& g:geometries()) {
            auto* attributes=new G4VisAttributes(G4Colour(.35,.65,1.,.15));
            attributes->SetForceWireframe(true);
            g->panel->GetLogicalVolume()->SetVisAttributes(g->fiberPaths.empty()?attributes:G4VisAttributes::GetInvisible());
        }
        vis->RegisterRunDurationUserVisAction("MINE_fibers",new FiberCenterlineView);
    }
}
} // namespace mine
