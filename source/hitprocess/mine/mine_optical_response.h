#ifndef MINE_OPTICAL_RESPONSE_H
#define MINE_OPTICAL_RESPONSE_H
#include "optical/OpticalResponseTable.h"
#include "mine_parameters.h"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>

namespace mine {
// Immutable table shared by all hit processes/threads, loaded once per process.
// All RNG draws are supplied by Geant4 in mine_response.cc.
struct TabulatedOptics {
    LightTrapping::OpticalResponseTable table;
    ReadoutMode mode;
    int model=-1, layout=7, fiberCount=0;
    double halfX=0, halfY=0, halfZ=0, additionalWlsDecay=0, wavelengthNm=0;
    std::uint64_t fingerprint=0; // Low 52 bits of file FNV-1a, exactly representable in bank double.
    std::string sourcePath;

    const std::string& Text(const std::string& key) const {
        const auto found=table.metadata.find(key);
        if (found==table.metadata.end())
            throw std::invalid_argument("MINE optical table missing "+key+"; re-export the raw MC with the current exporter");
        return found->second;
    }
    double Number(const std::string& key) const {
        const auto& value=Text(key);std::size_t end=0;double result;
        try {result=std::stod(value,&end);} catch(...) {throw std::invalid_argument("MINE invalid optical metadata: "+key);}
        if(end!=value.size() || !std::isfinite(result)) throw std::invalid_argument("MINE invalid optical metadata: "+key);
        return result;
    }
    bool Flag(const std::string& key) const {
        const auto& value=Text(key);
        if(value!="true" && value!="false") throw std::invalid_argument("MINE invalid optical flag: "+key);
        return value=="true";
    }
    explicit TabulatedOptics(LightTrapping::OpticalResponseTable response):table(std::move(response)) {
        table.Validate();
        if(Text("gemc_contract_version")!="1" || Text("sensor_count")!="2")
            throw std::invalid_argument("MINE unsupported optical-table GEMC contract or sensor count");
        if(Text("timing_reference")!="arrivalTimeNs - productionTimeNs; ns; full optical transport, no direct-flight subtraction" ||
           Flag("scintillation_decay_included") || Flag("sipm_and_electronics_included"))
            throw std::invalid_argument("MINE table must contain full optical transport only, without scintillation/SiPM/electronics");
        const auto type=Text("fiber_type"),profile=Text("fiber_model");
        if(type!="wls" && type!="clear") throw std::invalid_argument("MINE unsupported table fiber type");
        mode=type=="wls"?ReadoutMode::WLS:ReadoutMode::DirectFiber;
        if(type=="wls") {
            if(profile!="Y-11" && profile!="YS-6") throw std::invalid_argument("MINE unsupported WLS table profile");
            model=profile=="Y-11"?0:1;
        } else if(profile!="clear") throw std::invalid_argument("MINE inconsistent clear-fiber table profile");
        const std::map<std::string,int> layouts={{"parallel_fan_in",1},{"compact_fan_in",1},
            {"double_serpentine",2},{"diagonal_corner_fan",3},{"triple_serpentine",4},
            {"panel_double_serpentine",5},{"straight",6}};
        const auto geometry=layouts.find(Text("fiber_geometry"));
        if(geometry!=layouts.end()) layout=geometry->second; // 7 denotes another table-defined layout.
        const double count=Number("number_of_fibers");
        if(count<1 || count>1000000 || std::floor(count)!=count) throw std::invalid_argument("MINE invalid table fiber count");
        fiberCount=static_cast<int>(count);
        halfX=Number("scintillator_half_size_x_mm")*CLHEP::mm;
        halfY=Number("scintillator_half_size_y_mm")*CLHEP::mm;
        halfZ=Number("scintillator_thickness_mm")*CLHEP::mm/2;
        additionalWlsDecay=Number("additional_wls_decay_ns")*CLHEP::ns;
        wavelengthNm=Number("readout_wavelength_nm");
        const bool included=Flag("wls_decay_included");
        const double originalDecay=Number("wls_decay_ns");
        if(halfX<=0 || halfY<=0 || halfZ<=0 || additionalWlsDecay<0 || wavelengthNm<=0 || originalDecay<0 ||
           (included && additionalWlsDecay!=0) || (type=="clear" && (included || additionalWlsDecay!=0)) ||
           (type=="wls" && included!=(originalDecay>0)))
            throw std::invalid_argument("MINE inconsistent table dimensions, wavelength or WLS delay");
        for(const char* key:{"bulk_attenuation_mm","fiber_attenuation_mm"})
            if(Number(key)<0) throw std::invalid_argument("MINE negative table attenuation length");
        if(Number("wall_limit_photons")!=0 || Number("fiber_limit_photons")!=0)
            throw std::invalid_argument("MINE optical table is censored by numerical transport limits; check the MC before using it");
        if(std::abs(table.xMinMm+halfX/CLHEP::mm)>1e-6 || std::abs(table.xMaxMm-halfX/CLHEP::mm)>1e-6 ||
           std::abs(table.yMinMm+halfY/CLHEP::mm)>1e-6 || std::abs(table.yMaxMm-halfY/CLHEP::mm)>1e-6)
            throw std::invalid_argument("MINE optical table must cover the entire local XY panel; export a full-panel uniform MC");
        for(const auto& cell:table.cells) if(!cell.generated)
            throw std::invalid_argument("MINE optical table contains unsampled spatial cells");
    }
    void ValidatePanel(const G4ThreeVector& half) const {
        for(const auto& pair:{std::pair<double,double>{half.x(),halfX},{half.y(),halfY},{half.z(),halfZ}})
            if(!std::isfinite(pair.first) || std::abs(pair.first-pair.second)>1e-6*CLHEP::mm)
                throw std::invalid_argument("MINE GEMC Box dimensions do not match the optical response panel");
    }
    // Only roundoff at a physical boundary is clamped; no extrapolation outside the panel.
    double Coordinate(double v,double half) const {
        if(!std::isfinite(v) || std::abs(v)>half+1e-6*CLHEP::mm)
            throw std::invalid_argument("MINE step lies outside the optical response panel");
        return std::max(-half,std::min(half,v))/CLHEP::mm;
    }
    double Efficiency(const G4ThreeVector& point,int sensor) const {
        Coordinate(point.z(),halfZ);
        return table.Efficiency(Coordinate(point.x(),halfX),Coordinate(point.y(),halfY),sensor);
    }
    double SampleTime(const G4ThreeVector& point,int sensor,double mixture,double time) const {
        return table.SampleTransportTime(Coordinate(point.x(),halfX),Coordinate(point.y(),halfY),sensor,mixture,time)*CLHEP::ns;
    }
    static TabulatedOptics Load(const std::string& path) {
        TabulatedOptics result(LightTrapping::OpticalResponseTable::Load(path));
        result.sourcePath=path;
        std::ifstream bytes(path,std::ios::binary);std::uint64_t hash=14695981039346656037ULL;char c;
        while(bytes.get(c)) {hash^=static_cast<unsigned char>(c);hash*=1099511628211ULL;}
        if(!bytes.eof()) throw std::runtime_error("MINE cannot fingerprint optical response");
        result.fingerprint=hash&((1ULL<<52)-1);return result;
    }
};
inline std::string OpticalResponsePath() {
    if(const auto* path=std::getenv("MINE_OPTICAL_RESPONSE")) {
        if(!*path) throw std::invalid_argument("MINE_OPTICAL_RESPONSE is empty");
        return path;
    }
    for(const char* key:{"GEMC_DATA_DIR","GEMC"}) if(const auto* root=std::getenv(key))
        if(*root) return std::string(root)+"/experiments/mine/responses/optical_response.dat";
    for(const char* relative:{"experiments/mine/responses/optical_response.dat","responses/optical_response.dat"})
        if(std::filesystem::is_directory(std::filesystem::path(relative).parent_path())) return relative;
    throw std::runtime_error("MINE cannot locate optical_response.dat; set GEMC_DATA_DIR or MINE_OPTICAL_RESPONSE");
}
inline const TabulatedOptics& ActiveOpticalResponse() {
    static const TabulatedOptics response=[] {
        const auto path=OpticalResponsePath();auto optics=TabulatedOptics::Load(path);
        std::cout<<"MINE optical response: "<<path<<"; fingerprint="<<optics.fingerprint
                 <<"; geometry="<<optics.Text("fiber_geometry")<<"; fibers="<<optics.fiberCount
                 <<"; profile="<<optics.Text("fiber_model")<<"; panel_mm="
                 <<2*optics.halfX/CLHEP::mm<<'x'<<2*optics.halfY/CLHEP::mm<<'x'<<2*optics.halfZ/CLHEP::mm
                 <<"; attenuation_mm="<<optics.Text("bulk_attenuation_mm")<<','<<optics.Text("fiber_attenuation_mm")
                 <<"; WLS_decay_in_table="<<optics.Text("wls_decay_included")
                 <<"; added_WLS_decay_ns="<<optics.additionalWlsDecay/CLHEP::ns<<std::endl;
        if(optics.Number("bulk_attenuation_mm")==0 || optics.Number("fiber_attenuation_mm")==0)
            std::cout<<"MINE: zero attenuation length in table means that loss is deliberately disabled; no GEMC attenuation is added."<<std::endl;
        return optics;
    }();
    return response;
}
}
#endif
