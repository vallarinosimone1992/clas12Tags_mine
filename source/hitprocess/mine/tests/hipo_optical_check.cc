// Read-only optical HIPO integration check; needs HIPO4, not Geant4 or ROOT.
#include <hipo4/bank.h>
#include <hipo4/dictionary.h>
#include <hipo4/event.h>
#include <hipo4/reader.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
void require(bool ok,const std::string& why) {if(!ok)throw std::runtime_error(why);}
void sameCount(double actual,long expected,const std::string& what) {
    require(std::isfinite(actual) && actual==static_cast<double>(expected),what);
}
struct Counts {std::array<long,2> arrivals{},detected{};};
}
int main(int argc,char** argv) {
    try {
        require(argc>=2,"Usage: hipo_optical_check FILE [--mode geant4|tabulated] [--events N] [--require-both] [--allow-unjoined]");
        bool explicitOptics=true,requireBoth=false,allowUnjoined=false;
        int expectedEvents=-1,expectedProfile=-1,expectedLayout=-1,expectedIdeal=-1;
        for(int i=2;i<argc;++i) {
            const std::string option=argv[i];
            if(option=="--require-both")requireBoth=true;
            else if(option=="--allow-unjoined")allowUnjoined=true;
            else {
                require(i+1<argc,"Missing value for "+option);const std::string value=argv[++i];
                if(option=="--events")expectedEvents=std::stoi(value);
                else if(option=="--fiber-model")expectedProfile=std::stoi(value);
                else if(option=="--fiber-layout")expectedLayout=std::stoi(value);
                else if(option=="--ideal-sipm")expectedIdeal=std::stoi(value);
                else if(option=="--mode") {
                    require(value=="geant4" || value=="tabulated","Unknown transport mode");
                    explicitOptics=value=="geant4";
                } else throw std::runtime_error("Unknown option: "+option);
            }
        }
        hipo::reader reader;reader.open(argv[1]);require(reader.is_open(),"Cannot open HIPO input");
        hipo::dictionary dictionary;reader.readDictionary(dictionary);
        for(const char* bank:{"MINE::optical","MINE::dgt","MC::True","RUN::config","MC::WLS"})
            require(dictionary.hasSchema(bank),std::string("Missing schema: ")+bank);
        auto& schema=dictionary.getSchema("MINE::optical");
        require(schema.getGroup()==4600 && schema.getItem()==3,"Incorrect MINE::optical bank identifiers");
        const std::map<std::string,int> fields={{"panel",hipo::kInt},{"sensor",hipo::kInt},
            {"track",hipo::kInt},{"parent",hipo::kInt},{"time",hipo::kDouble},
            {"wavelength",hipo::kDouble},{"pathLength",hipo::kDouble},{"detected",hipo::kInt},
            {"pdeApplied",hipo::kInt},{"hitn",hipo::kInt},{"entryRole",hipo::kInt},
            {"fiber",hipo::kInt},{"firstWlsFiber",hipo::kInt},{"nWls",hipo::kInt},{"entryFiber",hipo::kInt}};
        require(schema.getEntries()==static_cast<int>(fields.size()),"Unexpected MINE::optical field count");
        for(const auto& field:fields)
            require(schema.exists(field.first.c_str()) && schema.getEntryType(field.first.c_str())==field.second,
                    "Missing/incorrect optical field: "+field.first);
        long events=0,unjoined=0,depositHits=0,wlsTotal=0;
        Counts totals;std::map<int,long> entryCounts;
        double minTime=std::numeric_limits<double>::infinity(),maxTime=-minTime,maxPath=0;
        while(reader.next()) {
            hipo::event event;reader.read(event);
            hipo::bank optical(schema),response(dictionary.getSchema("MINE::dgt")),truth(dictionary.getSchema("MC::True"));
            event.getStructure(optical);event.getStructure(response);event.getStructure(truth);
            require(explicitOptics || optical.getRows()==0,"Tabulated event contains tracked optical arrivals");
            std::map<int,std::pair<int,int>> panels; // panel -> (response row,hitn)
            for(int row=0;row<response.getRows();++row) {
                const int panel=response.getInt("id",row),hitn=response.getInt("hitn",row);
                if(expectedLayout>=0)require(response.getInt("fiberLayout",row)==expectedLayout,"Incorrect fiberLayout in HIPO");
                if(expectedLayout==7)require(response.getInt("fiberCount",row)==2,"Mini HIPO must report two fibers");
                if(expectedIdeal>=0)require(response.getInt("idealSiPM",row)==expectedIdeal,"Incorrect idealSiPM in HIPO");
                if(expectedProfile>=0)require(response.getInt("fiberModel",row)==expectedProfile,"Incorrect explicit fiberModel in HIPO");
                require(panels.emplace(panel,std::make_pair(row,hitn)).second,"Repeated response for one panel");
                require(response.getInt("opticalTable",row)==(explicitOptics?0:1),"Incorrect table discriminator");
                require(response.getInt("responseMode",row)==(explicitOptics?2:1),"Incorrect responseMode");
                bool joined=false;
                for(int tr=0;tr<truth.getRows();++tr)
                    joined|=truth.getInt("detector",tr)==110 && truth.getInt("hitn",tr)==hitn;
                require(joined,"Deposition response has no MC::True match");
                ++depositHits;
            }
            hipo::bank wls(dictionary.getSchema("MC::WLS"));event.getStructure(wls);
            require(explicitOptics || wls.getRows()==0,"Tabulated event contains explicit WLS absorptions");
            // Absorbed-track -> (fiber,parent); each real WLS absorption terminates that track.
            std::map<int,std::pair<int,int>> absorbed;
            for(int row=0;row<wls.getRows();++row) {
                const int fiber=wls.getInt("fiber",row),panel=wls.getInt("panel",row);
                require(fiber>=0 && absorbed.emplace(wls.getInt("track",row),std::make_pair(fiber,wls.getInt("parent",row))).second,
                        "Invalid fiber or duplicate WLS absorption track");
                require(wls.getInt("secondaryCount",row)==1,"Unity-QY WLS absorption did not produce one secondary");
                require(wls.getDouble("wavelength",row)>0 && std::isfinite(wls.getDouble("time",row)),"Invalid WLS absorption physics");
                const auto hit=panels.find(panel);
                if(hit!=panels.end()) {
                    require(fiber<response.getInt("fiberCount",hit->second.first),"WLS fiber ID outside geometry");
                    require(wls.getInt("hitn",row)==hit->second.second,"WLS absorption has wrong hit association");
                } else require(allowUnjoined && wls.getInt("hitn",row)==-1,"Unjoined WLS absorption without sentinel");
                ++wlsTotal;
            }
            std::map<int,Counts> counts;
            std::set<int> tracks;
            for(int row=0;row<optical.getRows();++row) {
                const int panel=optical.getInt("panel",row),sensor=optical.getInt("sensor",row),
                    track=optical.getInt("track",row),parent=optical.getInt("parent",row),
                    hitn=optical.getInt("hitn",row),applied=optical.getInt("pdeApplied",row),
                    detected=optical.getInt("detected",row),entryRole=optical.getInt("entryRole",row);
                const double time=optical.getDouble("time",row),wave=optical.getDouble("wavelength",row),
                    path=optical.getDouble("pathLength",row);
                require(panel>0 && (sensor==0 || sensor==1),"Invalid optical panel/sensor ID");
                require(entryRole>=-1 && entryRole<=4,"Invalid sensor-entry volume role");
                require(track>0 && parent>=0 && parent!=track && tracks.insert(track).second,"Invalid/repeated optical track ID");
                require(std::isfinite(time) && std::isfinite(wave) && wave>0 && std::isfinite(path) && path>=0,
                        "Invalid arrival time, wavelength or path");
                require((applied==0 && detected==-1) || (applied==1 && (detected==0 || detected==1)),
                        "Invalid PDE decision/sentinel");
                const auto responsePanel=panels.find(panel);
                if(responsePanel==panels.end()) {
                    require(allowUnjoined && hitn==-1 && applied==0,"Unjoined optical-only arrival lacks raw-truth sentinel");
                    ++unjoined;
                } else {
                    require(hitn==responsePanel->second.second,"Optical arrival has wrong panel/hitn association");
                    require(applied==1,"Joined optical arrival has no PDE decision");
                }
                int ancestor=parent,first=-1,last=-1,generations=0;
                while(absorbed.count(ancestor)) {
                    const auto info=absorbed.at(ancestor);
                    if(generations==0)last=info.first;
                    first=info.first;ancestor=info.second;
                    require(++generations<=wls.getRows(),"Cyclic WLS ancestry");
                }
                require(optical.getInt("fiber",row)==last && optical.getInt("firstWlsFiber",row)==first &&
                        optical.getInt("nWls",row)==generations,"Arrival's fiber identity does not match MC::WLS ancestry");
                const int entryFiber=optical.getInt("entryFiber",row);
                require(entryFiber>=-1,"Invalid entryFiber sentinel");
                if(responsePanel!=panels.end())require(entryFiber<response.getInt("fiberCount",responsePanel->second.first),"entryFiber outside physical geometry");
                ++entryCounts[entryFiber];
                ++counts[panel].arrivals[sensor];++totals.arrivals[sensor];
                if(detected==1) {++counts[panel].detected[sensor];++totals.detected[sensor];}
                minTime=std::min(minTime,time);maxTime=std::max(maxTime,time);maxPath=std::max(maxPath,path);
            }
            if(explicitOptics)for(const auto& panel:panels) {
                const int row=panel.second.first;const auto& c=counts[panel.first];
                for(int sensor=0;sensor<2;++sensor) {
                    const auto suffix=std::to_string(sensor);
                    sameCount(response.getDouble(("Ncollected"+suffix).c_str(),row),c.arrivals[sensor],"Ncollected differs from raw arrivals");
                    sameCount(response.getDouble(("Ntransported"+suffix).c_str(),row),c.arrivals[sensor],"Ntransported differs from raw arrivals");
                    sameCount(response.getInt(("Npe"+suffix).c_str(),row),c.detected[sensor],"Npe differs from single-PDE arrival decisions");
                    if(response.getInt("idealSiPM",row))require(c.arrivals[sensor]==c.detected[sensor],"Ideal SiPM rejected an arrival");
                }
            }
            ++events;
        }
        require(expectedEvents<0 || events==expectedEvents,"Unexpected retained event count");
        require(!requireBoth || (totals.arrivals[0]>0 && totals.arrivals[1]>0),"Both sensors did not collect photons");
        std::cout<<"Optical HIPO verification passed: events="<<events<<", depositHits="<<depositHits
            <<", arrivals="<<totals.arrivals[0]<<','<<totals.arrivals[1]
            <<", WLS_absorptions="<<wlsTotal<<", detected="<<totals.detected[0]<<','<<totals.detected[1]<<", unjoined="<<unjoined;
        if(totals.arrivals[0]+totals.arrivals[1])std::cout<<", timeRangeNs="<<minTime<<','<<maxTime<<", maxPathMm="<<maxPath;
        std::cout<<", entryFiberCounts=";for(const auto& entry:entryCounts)std::cout<<entry.first<<':'<<entry.second<<' ';
        std::cout<<'\n';return 0;
    } catch(const std::exception& error) {std::cerr<<"OPTICAL HIPO CHECK ERROR: "<<error.what()<<'\n';return 1;}
}
