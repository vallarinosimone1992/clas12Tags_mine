// ROOT interpreter helper. Reads a SINGLE matched bend/reference MC case and
// exports only EndCore arrivals, matching geant4_fiber_benchmark's CSV meaning.
// The reference ROOT is never modified and existing CSV outputs are refused.
#include <TFile.h>
#include <TParameter.h>
#include <TTree.h>
#include <TTreeReader.h>
#include <TTreeReaderValue.h>
#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

void export_reference_fiber_benchmark(const char* inputPath,const char* straightPath,const char* bendPath) {
    for(const char* path:{straightPath,bendPath}) {
        std::ifstream previous(path);
        if(previous) throw std::runtime_error("refusing to overwrite reference CSV");
    }
    TFile input(inputPath,"READ");
    if(input.IsZombie()) throw std::runtime_error("cannot open reference ROOT");
    auto* photons=input.Get<TTree>("photons");
    auto* cases=input.Get<TParameter<int>>("scan_radius_count");
    auto* angles=input.Get<TParameter<int>>("scan_angle_count");
    auto* count=input.Get<TParameter<Long64_t>>("photons_per_case");
    auto* peak=input.Get<TParameter<double>>("emission_peak_nm");
    if(!photons || !cases || !angles || cases->GetVal()!=1 || angles->GetVal()!=1 || !count || !peak)
        throw std::runtime_error("export requires one matched bend/reference MC case with metadata");
    std::ofstream straight(straightPath),bend(bendPath);
    for(auto* output:{&straight,&bend}) {
        if(!*output) throw std::runtime_error("cannot create reference CSV");
        *output<<"event,sensor,path_mm,time_ns,wavelength_nm,core_path_mm,cladding_path_mm\n"<<std::setprecision(17);
    }
    TTreeReader reader(photons);
    TTreeReaderValue<Bool_t> reference(reader,"isReference");
    TTreeReaderValue<ULong64_t> id(reader,"photonId");
    TTreeReaderValue<Int_t> termination(reader,"termination"),end(reader,"fiberEnd");
    TTreeReaderValue<Double_t> path(reader,"totalPathMm"),time(reader,"transportTimeNs");
    TTreeReaderValue<Double_t> core(reader,"corePathMm"),clad(reader,"claddingPathMm");
    std::array<std::array<unsigned long,5>,2> outcomes{};
    std::array<std::array<unsigned long,2>,2> coreEnds{},cladEnds{};
    while(reader.Next()) {
        if(*termination<0 || *termination>4) throw std::runtime_error("invalid MC termination");
        const int geometry=*reference?0:1;++outcomes[geometry][*termination];
        if(*termination==4) throw std::runtime_error("MC has numerically censored photons; do not compare as physical losses");
        if(*termination>1) continue;
        if(*end<0 || *end>1) throw std::runtime_error("invalid MC end");
        if(*termination==1) {++cladEnds[geometry][*end];continue;}
        ++coreEnds[geometry][*end];
        auto& output=*reference?straight:bend;
        output<<*id<<','<<*end<<','<<*path<<','<<*time<<','<<peak->GetVal()<<','<<*core<<','<<*clad<<'\n';
    }
    for(int geometry=0;geometry<2;++geometry) {
        unsigned long total=0;for(auto n:outcomes[geometry]) total+=n;
        if(total!=static_cast<unsigned long>(count->GetVal())) throw std::runtime_error("MC case count does not match metadata");
        std::cout<<(geometry==0?"straight":"bend")<<" generated="<<total
                 <<" core_end0="<<coreEnds[geometry][0]<<" core_end1="<<coreEnds[geometry][1]
                 <<" clad_end0="<<cladEnds[geometry][0]<<" clad_end1="<<cladEnds[geometry][1]
                 <<" escaped="<<outcomes[geometry][2]<<" absorbed="<<outcomes[geometry][3]
                 <<" censored="<<outcomes[geometry][4]<<std::endl;
    }
}
