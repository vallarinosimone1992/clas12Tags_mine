#include "mine_geant4_optics.h"
#include "mine_mini_spiral_model.h"
#include "mine_groove_materials.h"
#include "G4Box.hh"
#include "G4Torus.hh"
#include "G4Tubs.hh"
#include "G4DisplacedSolid.hh"
#include "G4UnionSolid.hh"
#include "G4IntersectionSolid.hh"
#include "G4SubtractionSolid.hh"
#include "G4TessellatedSolid.hh"
#include "G4TriangularFacet.hh"
#include "G4LogicalVolume.hh"
#include "G4LogicalBorderSurface.hh"
#include "G4LogicalSkinSurface.hh"
#include "G4Material.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4OpticalSurface.hh"
#include "G4MaterialPropertiesTable.hh"
#include "G4VisAttributes.hh"
#include "G4SystemOfUnits.hh"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <iomanip>
namespace mine { namespace {
constexpr double pi=3.14159265358979323846;
G4ThreeVector v(const OpticalPoint& p){return {p.x*mm,p.y*mm,p.z*mm};}
OpticalPoint cross(const OpticalPoint& a,const OpticalPoint& b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
G4RotationMatrix basis(const OpticalPoint& x,const OpticalPoint& y,const OpticalPoint& z){return G4RotationMatrix(v(x),v(y),v(z));}
G4VSolid* moved(const std::string& n,G4VSolid* s,const G4RotationMatrix& rot,const OpticalPoint& pos){return new G4DisplacedSolid(n,s,G4Transform3D(rot,v(pos)));}
G4VSolid* join(const std::string& name,std::vector<G4VSolid*> s){
 if(s.empty())throw std::runtime_error("Empty mini solid union");int level=0;
 while(s.size()>1){std::vector<G4VSolid*> next;for(std::size_t i=0;i<s.size();i+=2)next.push_back(i+1<s.size()?new G4UnionSolid(name+"_"+std::to_string(level)+"_"+std::to_string(i),s[i],s[i+1]):s[i]);s=std::move(next);++level;}return s[0];
}
G4VSolid* fiber(const std::string& name,const MiniSpiralFiber& f,double radius,double tolerance){
 std::vector<G4VSolid*> parts;constexpr double extension=1e-3;
 for(std::size_t i=0;i<f.segments.size();++i){const auto& s=f.segments[i];const auto n=name+"_"+std::to_string(i);const double before=i?extension:0,after=i+1<f.segments.size()?extension:0;
  if(s.kind==MiniSpiralSegmentKind::Line || s.radiusMm*2*std::pow(std::sin(s.sweepRad/4),2)<std::min(.00005,tolerance*.25)){auto t=(s.end-s.start).unit(),x=std::abs(t.z)<.8?cross(t,{0,0,1}).unit():cross(t,{1,0,0}).unit();auto y=cross(t,x);auto a=s.start-t*before,b=s.end+t*after;
   parts.push_back(moved(n,new G4Tubs(n+"_tube",0,radius*mm,(b-a).norm()*mm/2,0,2*pi),basis(x,y,t),(a+b)*.5));
  }else{
   // Geant4 10.7.4 misclassifies near-wall points for negative torus
   // start phi. Rotate the basis instead; keep identical physical end planes.
   const double a=before/s.radiusMm;
   auto u=s.planeU*std::cos(a)-s.planeV*std::sin(a);
   auto w=s.planeU*std::sin(a)+s.planeV*std::cos(a);
   auto z=cross(u,w).unit();
   parts.push_back(moved(n,new G4Torus(n+"_torus",0,radius*mm,s.radiusMm*mm,0,s.sweepRad+(before+after)/s.radiusMm),basis(u,w,z),s.center));
  }
 }return join(name+"_joined",std::move(parts));
}
// Closed loft used only for the rectangular S exit groove and Mylar air cavity.
// Fiber transport uses analytic circular tubes, not these mesh facets.
G4VSolid* loft(const std::string& name,const std::vector<std::vector<OpticalPoint>>& rings){
 auto* solid=new G4TessellatedSolid(name);const auto center=[&](const std::vector<OpticalPoint>& r){OpticalPoint c;for(const auto&p:r)c=c+p;return c*(1./r.size());};
 auto tri=[&](OpticalPoint a,OpticalPoint b,OpticalPoint c,OpticalPoint inward){auto n=cross(b-a,c-a);if(n.dot(inward-(a+b+c)*(1./3))>0)std::swap(b,c);solid->AddFacet(new G4TriangularFacet(v(a),v(b),v(c),ABSOLUTE));};
 for(std::size_t k=0;k+1<rings.size();++k){auto mid=(center(rings[k])+center(rings[k+1]))*.5;for(std::size_t j=0;j<rings[k].size();++j){const auto l=(j+1)%rings[k].size();tri(rings[k][j],rings[k+1][j],rings[k+1][l],mid);tri(rings[k][j],rings[k+1][l],rings[k][l],mid);}}
 for(auto k:{std::size_t(0),rings.size()-1}){const auto& r=rings[k];auto inside=k?center(rings[k-1]):center(rings[1]);for(std::size_t j=1;j+1<r.size();++j)tri(r[0],r[j],r[j+1],inside);}
 solid->SetSolidClosed(true);return solid;
}
G4VSolid* groove(const std::string& name,const MiniSpiralModel& m,double width,double depth){
 std::vector<G4VSolid*> parts;const double z=m.config.thicknessMm/2-depth/2;std::size_t i=0;
 for(const auto&s:m.fibers[0].segments){if(s.section!=MiniSpiralSection::Spiral)continue;const auto n=name+std::to_string(i++);
  if(s.kind==MiniSpiralSegmentKind::Line){auto t=(s.end-s.start).unit(),x=OpticalPoint{-t.y,t.x,0};auto c=(s.start+s.end)*.5;c.z=z;parts.push_back(moved(n,new G4Box(n+"_box",width*mm/2,(s.LengthMm()+2e-6)*mm/2,depth*mm/2),basis(x,t,{0,0,-1}),c));}
  else {auto c=s.center;c.z=z;parts.push_back(moved(n,new G4Tubs(n+"_arc",(s.radiusMm-width/2)*mm,(s.radiusMm+width/2)*mm,depth*mm/2,-1e-8,s.sweepRad+2e-8),basis(s.planeU,s.planeV,cross(s.planeU,s.planeV)),c));}
 }
 std::vector<std::vector<OpticalPoint>> rings;constexpr int count=1200;const double eps=std::max(.0005,2*m.config.approximationToleranceMm);
 for(int j=0;j<=count;++j){const double u=double(j)/count;auto a=m.SPoint(u,0),b=m.SPoint(u,1),t=m.STangent(u,0);auto n=OpticalPoint{-t.y,t.x,0}.unit();const double lo=a.z-m.config.diameterMm/2-eps,hi=b.z+m.config.diameterMm/2+eps;auto p=a-n*(width/2+eps),q=a+n*(width/2+eps);rings.push_back({{p.x,p.y,lo},{q.x,q.y,lo},{q.x,q.y,hi},{p.x,p.y,hi}});}
 parts.push_back(loft(name+"_S_exit",rings));return join(name+"_network",std::move(parts));
}
G4VSolid* tent(const std::string& name,const MiniSpiralModel& m,const Geant4OpticsConfig& c){
 std::vector<std::vector<OpticalPoint>> rings;constexpr int count=1200;const double angle=c.tentAngleDeg*pi/180,co=std::cos(angle),ta=std::tan(angle),floor=m.config.thicknessMm/2;
 for(int j=0;j<=count;++j){const double u=double(j)/count;auto p=m.SPoint(u,1),t=m.STangent(u,1);auto n=OpticalPoint{-t.y,t.x,0}.unit();
  // Both roof planes are tangent to the circular tube. For a horizontal fiber
  // the apex exceeds the top by r(sec30-1)=77.35 um; no physical added gap.
  const double h=p.z+m.config.diameterMm/2*std::sqrt(1-co*co*t.z*t.z)/co+c.tentClearanceMm+std::max(.0005,2*c.miniCurveToleranceMm);
  const double w=std::max(.0005,(h-floor)/ta);auto a=p-n*w,b=p+n*w;rings.push_back({{a.x,a.y,floor},{p.x,p.y,h},{b.x,b.y,floor}});
 }return loft(name,rings);
}
}
const Geant4OpticsGeometry& BuildMiniGeant4Optics(G4VPhysicalVolume* panel,G4VPhysicalVolume* environment,int id,const Geant4OpticsConfig& c){
 c.Validate();if(!panel||!environment)throw std::invalid_argument("Mini needs panel/world");auto* pl=panel->GetLogicalVolume();auto* box=dynamic_cast<G4Box*>(pl->GetSolid());if(!box)throw std::invalid_argument("Mini needs a Box bootstrap");
 if(std::abs(box->GetXHalfLength()/mm-150)>1e-6||std::abs(box->GetYHalfLength()/mm-250)>1e-6||std::abs(box->GetZHalfLength()/mm-4)>1e-6)throw std::invalid_argument("Mini measured panel must be 300x500x8 mm");
 auto g=std::make_unique<Geant4OpticsGeometry>();g->panel=panel;MiniSpiralConfig mc;mc.approximationToleranceMm=c.miniCurveToleranceMm;mc.diameterMm=c.fiberDiameterMm;mc.grooveWidthMm=c.grooveWidthMm;mc.grooveDepthMm=c.grooveDepthMm;mc.minBendRadiusMm=c.minBendRadiusMm;g->mini=std::make_shared<MiniSpiralModel>(BuildMiniSpiralModel(mc));const auto& m=*g->mini;
 const std::string pre="MINE_mini_"+std::to_string(id);auto* nist=G4NistManager::Instance();auto* scint=CreateGeant4ScintillatorMaterial(pre+"_EJ200",pl->GetMaterial(),c);pl->SetMaterial(scint);
 auto* core=CreateGeant4OpticalMaterial(pre+"_core",nist->FindOrBuildMaterial("G4_POLYSTYRENE"),c.nCore);ConfigureGeant4FiberAbsorption(core->GetMaterialPropertiesTable(),c,true);ConfigureGeant4WLS(core->GetMaterialPropertiesTable(),c);
 auto* clad=CreateGeant4OpticalMaterial(pre+"_clad",nist->FindOrBuildMaterial("G4_PLEXIGLASS"),c.nCladding);ConfigureGeant4FiberAbsorption(clad->GetMaterialPropertiesTable(),c,false);
 auto* gel=CreateGeant4GrooveMaterial(pre+"_G608N3",c.grooveMaterial);auto* air=CreateGeant4GrooveMaterial(pre+"_optical_air","air");environment->GetLogicalVolume()->SetMaterial(air);
 auto* coating=BuildGeant4CoatingSurface(pre+"_Mylar",c);auto* world=environment->GetLogicalVolume();const G4Transform3D placement(panel->GetObjectRotationValue(),panel->GetObjectTranslation());
 auto place=[&](const std::string& name,G4VSolid* shape,G4Material* mat,OpticalVolumeInfo info){auto* lv=new G4LogicalVolume(shape,mat,name);auto* pv=new G4PVPlacement(placement,lv,name,world,false,0,false);RegisterGeant4OpticalVolume(pv,info);lv->SetVisAttributes(G4VisAttributes::GetInvisible());return pv;};
 std::array<G4VSolid*,2> outer{};std::vector<G4VSolid*> fiberSolids;
 for(int f=0;f<2;++f){outer[f]=fiber(pre+"_fiber"+std::to_string(f),m.fibers[f],c.fiberDiameterMm/2,c.miniCurveToleranceMm);fiberSolids.push_back(outer[f]);auto* cl=place(pre+"_clad"+std::to_string(f),outer[f],clad,{OpticalVolumeRole::Cladding,id,f,-1});
  auto* in=new G4LogicalVolume(fiber(pre+"_core"+std::to_string(f),m.fibers[f],c.fiberDiameterMm*(.5-c.claddingFraction),c.miniCurveToleranceMm),core,pre+"_coreLV"+std::to_string(f));auto* cp=new G4PVPlacement(nullptr,{},in,pre+"_core"+std::to_string(f),cl->GetLogicalVolume(),false,f,false);RegisterGeant4OpticalVolume(cp,{OpticalVolumeRole::Core,id,f,-1});in->SetVisAttributes(G4VisAttributes::GetInvisible());g->cores.push_back(cp);g->claddings.push_back(cl);g->fiberPaths.push_back(m.fibers[f].samples);
  new G4LogicalBorderSurface(pre+"_fiberMylar"+std::to_string(f),cl,environment,coating);
 }
 auto* allFibers=join(pre+"_allfibers",fiberSolids);auto* channels=groove(pre+"_groove",m,c.grooveWidthMm,c.grooveDepthMm);
 // Siblings with exact common subtractions handle the continuous fibers both
 // inside and above the plastic; no daughter protrudes from its mother.
 pl->SetSolid(new G4SubtractionSolid(pre+"_carved_plastic",box,channels));RegisterGeant4OpticalVolume(panel,{OpticalVolumeRole::Panel,id,-1,-1});
 auto* inside=new G4IntersectionSolid(pre+"_groove_inside",channels,box);auto* fill=place(pre+"_groove_gel",new G4SubtractionSolid(pre+"_gel_no_fibers",inside,allFibers),gel,{OpticalVolumeRole::Filler,id,-1,-1});g->filler=fill;g->fillers.push_back(fill);
 auto* cavity=place(pre+"_air_canopy",new G4SubtractionSolid(pre+"_air_no_fibers",tent(pre+"_roof",m,c),allFibers),air,{OpticalVolumeRole::Filler,id,-1,-1});
 new G4LogicalBorderSurface(pre+"_panelMylar",panel,environment,coating);new G4LogicalBorderSurface(pre+"_grooveMylar",fill,environment,coating);new G4LogicalBorderSurface(pre+"_roofMylar",cavity,environment,coating);
 auto* sensorMat=CreateGeant4OpticalMaterial(pre+"_sensor_material",nist->FindOrBuildMaterial("G4_Si"),c.nScintillator);
 for(int s=0;s<2;++s){auto p=m.readouts[s].center;auto center=p+OpticalPoint{0,-c.sensorGelGapMm/2,0};auto* coupling=place(pre+"_coupling"+std::to_string(s),moved(pre+"_coupler_position"+std::to_string(s),new G4Box(pre+"_coupler_shape"+std::to_string(s),1.5*mm,c.sensorGelGapMm*mm/2,1.5*mm),G4RotationMatrix(),center),gel,{OpticalVolumeRole::Filler,id,-1,-1});g->fillers.push_back(coupling);
  p.y-=c.sensorGelGapMm+.005;auto* sensor=place(pre+"_SiPM"+std::to_string(s),moved(pre+"_sensor_position"+std::to_string(s),new G4Box(pre+"_sensor_shape"+std::to_string(s),1.5*mm,.005*mm,1.5*mm),G4RotationMatrix(),p),sensorMat,{OpticalVolumeRole::Sensor,id,-1,s});g->sensors.push_back(sensor);
  auto* black=new G4OpticalSurface(pre+"_sensor_black"+std::to_string(s),unified,polished,dielectric_metal);auto* props=new G4MaterialPropertiesTable;G4double es[]={1*eV,6*eV},zero[]={0,0};props->AddProperty("REFLECTIVITY",es,zero,2);props->AddProperty("EFFICIENCY",es,zero,2);black->SetMaterialPropertiesTable(props);new G4LogicalSkinSurface(pre+"_sensor_skin"+std::to_string(s),sensor->GetLogicalVolume(),black);
 }
 if(c.checkOverlaps) {
  int probes=0;
  for(int f=0;f<2;++f)for(const auto& seg:m.fibers[f].segments) {
   const auto center=seg.PointAtFraction(.5),t=seg.TangentAtFraction(.5);
   // Probe inside the native tube beyond its bounded centerline approximation.
   // A 0.1 nm inset incorrectly tests the analytic model against a 50 nm chord.
   const double inset=std::max(.0005,2*c.miniCurveToleranceMm);
   const auto n=std::abs(t.z)<.8?cross(t,{0,0,1}).unit():cross(t,{1,0,0}).unit(),b=cross(t,n);
   for(int a=0;a<8;++a){const double phi=2*pi*a/8;const auto point=center+(n*std::cos(phi)+b*std::sin(phi))*(c.fiberDiameterMm/2-inset);const auto q=v(point);
    if(outer[f]->Inside(q)!=kInside) {std::ostringstream why;why<<std::setprecision(17)<<"Mini containment probe outside native fiber f="<<f<<" seg="<<(&seg-m.fibers[f].segments.data())<<" angle="<<a<<" q="<<q<<" inside="<<outer[f]->Inside(q)<<" R="<<seg.radiusMm<<" sweep="<<seg.sweepRad;why<<" safetyOut="<<outer[f]->DistanceToOut(q)<<" centerInside="<<outer[f]->Inside(v(center));throw std::runtime_error(why.str());}
    if(outer[1-f]->Inside(q)==kInside || pl->GetSolid()->Inside(q)==kInside || fill->GetLogicalVolume()->GetSolid()->Inside(q)==kInside || cavity->GetLogicalVolume()->GetSolid()->Inside(q)==kInside)
      throw std::runtime_error("Mini fiber overlaps plastic/filling/air envelope");
    ++probes;
   }
  }
  std::cout<<"MINI_CONTAINMENT_PASS inset_mm="<<std::max(.0005,2*c.miniCurveToleranceMm)<<" surface_probes="<<probes<<std::endl;
 }
 std::cout<<"MINE MINI_SPIRAL_GEOMETRY fibers=2 length_mm="<<m.fibers[0].LengthMm()<<","<<m.fibers[1].LengthMm()<<" segments="<<m.fibers[0].segments.size()<<" S_error_mm="<<m.maxVerifiedApproximationErrorMm<<" lift_spacing_correction_mm="<<m.maxLiftSpacingCorrectionMm<<" sensor_gel_mm="<<c.sensorGelGapMm<<" roof_deg="<<c.tentAngleDeg<<" sipm="<<c.sipmModel<<" opticalHash="<<c.fingerprint<<std::endl;
 return StoreGeant4OpticalGeometry(std::move(g));
}
} // namespace mine
