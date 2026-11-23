// Read-only MC replay exporter. Link against the existing portable reference
// library so seed hashing and initial linear polarization use its actual code.
#include "LightTrapping/Simulation.h"
#include "LightTrapping/Dielectric.h"
#include <TFile.h>
#include <TNamed.h>
#include <TParameter.h>
#include <TTree.h>
#include <TTreeReader.h>
#include <TTreeReaderValue.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>

int main(int argc,char** argv) {
    if(argc!=3) throw std::invalid_argument("usage: export-long-launch matched-reference.root new-replay.csv");
    const std::string metadataPath=std::string(argv[2])+".json";
    for(const auto& path:{std::string(argv[2]),metadataPath}) {
        std::ifstream previous(path);if(previous) throw std::runtime_error("refusing to overwrite replay artifact");
    }
    TFile input(argv[1],"READ");auto* tree=input.Get<TTree>("photons");
    auto* seedText=input.Get<TNamed>("seed");
    auto* lead=input.Get<TParameter<double>>("input_length_mm");
    auto* outer=input.Get<TParameter<double>>("outer_radius_mm");
    auto* cladding=input.Get<TParameter<double>>("cladding_thickness_mm");
    if(input.IsZombie() || !tree || !seedText || !lead || !outer || !cladding)
        throw std::runtime_error("invalid matched-reference ROOT input");
    TTreeReader reader(tree);TTreeReaderValue<Bool_t> reference(reader,"isReference");
    TTreeReaderValue<ULong64_t> photon(reader,"photonId");
    TTreeReaderValue<Int_t> termination(reader,"termination"),end(reader,"fiberEnd");
    TTreeReaderValue<Double_t> time(reader,"transportTimeNs"),path(reader,"totalPathMm");
    TTreeReaderValue<Double_t> x(reader,"sourceX"),y(reader,"sourceY"),z(reader,"sourceZ");
    TTreeReaderValue<Double_t> dx(reader,"directionX"),dy(reader,"directionY"),dz(reader,"directionZ");
    double maxTime=-1,maxPath=0;ULong64_t id=0;LightTrapping::Vec3 position,direction;
    while(reader.Next()) if(*reference && *termination==0 && *end==1 && *time>maxTime) {
        maxTime=*time;maxPath=*path;id=*photon;position={*x,*y,*z};direction={*dx,*dy,*dz};
    }
    if(maxTime<0) throw std::runtime_error("reference contains no output-end core arrival");
    const auto seed=std::stoull(seedText->GetTitle());
    std::mt19937_64 rng(LightTrapping::PhotonSeed(seed,id,0));std::uniform_real_distribution<double> uniform(0,1);
    constexpr double kPi=3.14159265358979323846;
    const double coreRadius=outer->GetVal()-cladding->GetVal();
    const double radius=coreRadius*std::sqrt(uniform(rng)),phi=2*kPi*uniform(rng);
    const double cosine=2*uniform(rng)-1,azimuth=2*kPi*uniform(rng),sine=std::sqrt(std::max(0.,1-cosine*cosine));
    const LightTrapping::Vec3 reconstructedPosition{radius*std::cos(phi),-lead->GetVal()/2,radius*std::sin(phi)};
    const LightTrapping::Vec3 reconstructedDirection{sine*std::cos(azimuth),cosine,sine*std::sin(azimuth)};
    if(LightTrapping::Norm(position-reconstructedPosition)>1e-12 || LightTrapping::Norm(direction-reconstructedDirection)>1e-12)
        throw std::runtime_error("MC source RNG reconstruction does not match stored launch");
    const auto polarization=LightTrapping::LinearPolarization(reconstructedDirection,2*kPi*uniform(rng));
    for(const auto component:polarization) if(std::abs(component.imag())>1e-12)
        throw std::runtime_error("initial replay polarization is not linear");
    // The exact isolated MC's input lead starts at -inputLength; Geant4's
    // corresponding input lead starts at zero. Translate all launch y by L_in.
    position.y+=lead->GetVal();
    std::ofstream output(argv[2]);if(!output) throw std::runtime_error("cannot create replay CSV");
    output<<"x_mm,y_mm,z_mm,dx,dy,dz,px,py,pz\n"<<std::setprecision(17)
          <<position.x<<','<<position.y<<','<<position.z<<','<<direction.x<<','<<direction.y<<','<<direction.z<<','
          <<polarization[0].real()<<','<<polarization[1].real()<<','<<polarization[2].real()<<'\n';
    std::ofstream metadata(metadataPath);
    metadata<<std::setprecision(17)<<"{\n  \"mc_photon_id\": "<<id<<",\n  \"mc_seed\": "<<seed
            <<",\n  \"mc_time_ns\": "<<maxTime<<",\n  \"mc_path_mm\": "<<maxPath
            <<",\n  \"mc_to_geant4_y_translation_mm\": "<<lead->GetVal()<<"\n}\n";
    std::cout<<std::setprecision(17)<<"Replayed MC photon="<<id<<" time_ns="<<maxTime<<" path_mm="<<maxPath
             <<" position_rng_error_mm="<<LightTrapping::Norm(reconstructedPosition-LightTrapping::Vec3{position.x,position.y-lead->GetVal(),position.z})
             <<"; source="<<argv[2]<<std::endl;
}
