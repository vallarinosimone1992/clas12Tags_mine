// Actual swept-square network, daughter containment and navigator/material probe.
#include "mine_geant4_optics.h"
#include "mine_groove_materials.h"
#include "G4Box.hh"
#include "G4GeometryManager.hh"
#include "G4LogicalVolume.hh"
#include "G4Material.hh"
#include "G4MaterialPropertiesTable.hh"
#include "G4Navigator.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4PhysicalConstants.hh"
#include "G4SystemOfUnits.hh"
#include "G4VSolid.hh"
#include <cmath>
#include <iostream>
#include <stdexcept>
void require(bool ok,const char* why) {if(!ok) throw std::runtime_error(why);}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Usage: groove-geometry-test CONFIG");
        auto c=mine::Geant4OpticsConfig::Load(argv[1]);require(c.grooveModel=="square","Expected groove config");
        auto* nist=G4NistManager::Instance();
        auto* wlv=new G4LogicalVolume(new G4Box("world",1000*mm,1000*mm,1000*mm),nist->FindOrBuildMaterial("G4_AIR"),"world");
        auto* world=new G4PVPlacement(nullptr,{},wlv,"world",nullptr,false,0);
        auto* plv=new G4LogicalVolume(new G4Box("panel",500*mm,500*mm,5*mm),nist->FindOrBuildMaterial("G4_PLASTIC_SC_VINYLTOLUENE"),"panel");
        auto* panel=new G4PVPlacement(nullptr,{},plv,"panel",wlv,false,0);
        const auto& g=mine::BuildGeant4Optics(panel,world,1,c);
        require(g.filler,"Missing filling network");
        auto* props=g.filler->GetLogicalVolume()->GetMaterial()->GetMaterialPropertiesTable();
        require(props && !props->GetProperty("ABSLENGTH") && !props->GetProperty("WLSABSLENGTH"),"Unexpected losses in groove filling");
        for(double wave:{350.,402.,425.,476.,589.3,800.}) {
            const double index=props->GetProperty("RINDEX")->Value(1239.841984332/wave*eV);
            require(std::abs(index-mine::grooveRefractiveIndex(c.grooveMaterial,wave))<1e-6,"Incorrect filling index");
        }
        const double groupVelocity=props->GetProperty("GROUPVEL")->Value(1239.841984332/425*eV);
        const double groupIndex=c.grooveMaterial=="gel"?1.4338+3*10520/(425.*425.):mine::grooveRefractiveIndex(c.grooveMaterial,425);
        std::cout<<"GROOVE_GROUP_INDEX native="<<c_light/groupVelocity<<" expected="<<groupIndex<<std::endl;
        require(std::abs(c_light/groupVelocity-groupIndex)<1e-4,"Incorrect group speed in groove filling");
        G4GeometryManager::GetInstance()->CloseGeometry();G4Navigator nav;nav.SetWorldVolume(world);
        int checked=0;
        for(std::size_t f=0;f<g.model.fibers.size();++f) {
            const auto& route=g.model.fibers[f];
            require(g.claddings[f]->GetMotherLogical()==g.filler->GetLogicalVolume(),"Fiber not inside filling mother");
            for(double fraction:{.013,.123,.371,.619,.853,.987}) {
                const double s=route.LengthMm()*fraction;
                const auto p=route.PointAt(s),d=(route.PointAt(s+.001)-route.PointAt(s-.001)).unit();
                const mine::OpticalPoint normal{-d.y,d.x,0};
                auto* found=nav.LocateGlobalPointAndSetup({p.x*mm,p.y*mm,p.z*mm});
                require(found==g.cores[f],"Navigator failed to identify intended core");
                // Square corners would be outside a circular channel of radius 1 mm.
                for(double side:{-1.,1.}) for(double dz:{-.9,.9}) {
                    const auto q=p+normal*(side*.9)+mine::OpticalPoint{0,0,dz};
                    require(g.filler->GetLogicalVolume()->GetSolid()->Inside({q.x*mm,q.y*mm,q.z*mm})!=kOutside,"Missing square groove corner");
                }
                const auto above=p+mine::OpticalPoint{0,0,1.01};
                require(nav.LocateGlobalPointAndSetup({above.x*mm,above.y*mm,above.z*mm})==panel,"Groove is deeper than requested");
                const auto outside=p+mine::OpticalPoint{0,0,-1.01};
                require(nav.LocateGlobalPointAndSetup({outside.x*mm,outside.y*mm,outside.z*mm})==world,"Groove opening is not flush with panel face");
                ++checked;
            }
        }
        std::cout<<"GROOVE_GEOMETRY_PASS material="<<c.grooveMaterial<<" square_mm="<<c.grooveWidthMm
                 <<" center_samples="<<checked<<" n425="<<mine::grooveRefractiveIndex(c.grooveMaterial,425)<<std::endl;
        G4GeometryManager::GetInstance()->OpenGeometry();
    } catch(const std::exception& e) {std::cerr<<"GROOVE ERROR: "<<e.what()<<std::endl;return 1;}
}
