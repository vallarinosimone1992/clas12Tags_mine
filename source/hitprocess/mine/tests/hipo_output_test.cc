#include <hipo4/reader.h>
#include <hipo4/event.h>
#include <hipo4/bank.h>
#include <hipo4/dictionary.h>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void Require(bool condition,const std::string& message) {if(!condition) throw std::runtime_error(message);}
bool Near(double a,double b,double tolerance=1e-10) {return std::isfinite(a) && std::isfinite(b) && std::abs(a-b)<=tolerance*std::max(1.,std::max(std::abs(a),std::abs(b)));}
std::string Trim(const std::string& text) {const auto begin=text.find_first_not_of(" \t\r\n");if(begin==std::string::npos)return {};return text.substr(begin,text.find_last_not_of(" \t\r\n")-begin+1);}
std::map<std::string,int> BankFields(const std::string& path) {
    std::ifstream file(path);Require(static_cast<bool>(file),"Cannot read reference GEMC TEXT bank: "+path);
    std::map<std::string,int> result;std::string line;
    while(std::getline(file,line)) {
        std::istringstream input(line);std::vector<std::string> columns;std::string col;
        while(std::getline(input,col,'|')) columns.push_back(Trim(col));
        if(columns.size()<5 || columns[0]!="mine" || columns[1]=="bankid")continue;
        Require(columns[4]=="Di" || columns[4]=="Dd","Unexpected type in reference bank: "+columns[4]);
        Require(result.emplace(columns[1],columns[4]=="Di"?hipo::kInt:hipo::kDouble).second,"Duplicate TEXT bank field");
    }
    Require(!result.empty(),"Reference TEXT bank is empty");return result;
}
}
int main(int argc,char** argv) {
    try {
        Require(argc>=2,"Usage: hipo_check FILE [--lund] [--events N] [--bank FILE] [--hash INTEGER] [--mode tabulated|geant4]");
        const std::string path=argv[1];bool lund=false;int expectedEvents=2;
        std::string bankFile="/work/clas12Tags_mine/experiments/mine/mine__bank.txt";
        std::uint64_t expectedHash=0;
        std::string mode="tabulated";
        for(int arg=2;arg<argc;++arg) {
            const std::string option=argv[arg];
            if(option=="--lund") lund=true;
            else {
                Require(arg+1<argc,"Missing option argument: "+option);const std::string value=argv[++arg];
                if(option=="--events")expectedEvents=std::stoi(value);
                else if(option=="--bank")bankFile=value;
                else if(option=="--hash")expectedHash=std::stoull(value);
                else if(option=="--mode") {mode=value;Require(mode=="tabulated" || mode=="geant4","Unsupported response mode");}
                else throw std::runtime_error("Unknown option: "+option);
            }
        }
        hipo::reader reader;reader.open(path.c_str());Require(reader.is_open(),"Cannot open HIPO file");
        hipo::dictionary dictionary;reader.readDictionary(dictionary);
        for(const char* name:{"RUN::config","MC::Particle","MC::True","MINE::dgt"})
            Require(dictionary.hasSchema(name),std::string("Missing schema: ")+name);
        if(lund)Require(dictionary.hasSchema("MC::Lund") && dictionary.hasSchema("MC::Event"),"Missing LUND schemas");
        auto& schema=dictionary.getSchema("MINE::dgt");const auto expectedFields=BankFields(bankFile);
        Require(schema.getEntries()==static_cast<int>(expectedFields.size()),"MINE::dgt schema size differs from TEXT bank");
        for(const auto& field:expectedFields)
            Require(schema.exists(field.first.c_str()) && schema.getEntryType(field.first.c_str())==field.second,"Missing/wrongly typed schema field: "+field.first);
        Require(schema.getEntryType("opticalHash")==hipo::kDouble && schema.getEntryType("TDC0")==hipo::kDouble && schema.getEntryType("TDC1")==hipo::kDouble,"Hash and sensor times must retain double precision");
        int events=0,hitEvents=0,rows=0;double firstHash=-1;std::set<int> eventIds;
        while(reader.next()) {
            hipo::event event;reader.read(event);
            hipo::bank config(dictionary.getSchema("RUN::config"));event.getStructure(config);
            Require(config.getRows()==1,"Missing RUN::config event row");
            Require(eventIds.insert(config.getInt("event",0)).second,"Repeated RUN::config event ID");
            hipo::bank particles(dictionary.getSchema("MC::Particle"));event.getStructure(particles);
            Require(particles.getRows()==1,"Fixture should retain one generated particle");
            Require(particles.getInt("pid",0)==13,"Generated primary is not the expected muon");
            Require(Near(particles.getFloat("pz",0),1,1e-6),"MC::Particle momentum is not 1 GeV");
            Require(Near(particles.getFloat("vz",0),-10,1e-6),"MC::Particle vertex should be -10 cm");
            const double expectedX=lund && events==1?70:0;
            Require(Near(particles.getFloat("vx",0),expectedX,1e-6),"Generated transverse vertex differs from fixture");
            if(lund) {
                hipo::bank input(dictionary.getSchema("MC::Lund"));event.getStructure(input);
                Require(input.getRows()==1,"Original LUND particle row was lost");
                Require(input.getInt("pid",0)==13 && Near(input.getFloat("pz",0),1,1e-6),"Original LUND momentum/PID differ");
                Require(Near(input.getFloat("vx",0),expectedX,1e-6) && Near(input.getFloat("vz",0),-10,1e-6),"Original LUND vertex differs");
                hipo::bank header(dictionary.getSchema("MC::Event"));event.getStructure(header);
                Require(header.getRows()==1 && header.getInt("npart",0)==1,"Original LUND header missing");
            }
            hipo::bank response(schema);event.getStructure(response);
            hipo::bank truth(dictionary.getSchema("MC::True"));event.getStructure(truth);
            Require(truth.getRows()==response.getRows(),"Truth and MINE response hit counts differ");
            if(lund && events==1) Require(response.getRows()==0,"Off-panel particle unexpectedly generated MINE hits");
            else Require(response.getRows()>0,"Panel-crossing muon has no MINE response");
            if(response.getRows())++hitEvents;
            for(int row=0;row<response.getRows();++row) {
                ++rows;
                for(const auto& field:expectedFields) {
                    const double value=field.second==hipo::kDouble?response.getDouble(field.first.c_str(),row):response.getInt(field.first.c_str(),row);
                    Require(std::isfinite(value),"Non-finite MINE output: "+field.first);
                }
                Require(response.getInt("opticalTable",row)==(mode=="tabulated"?1:0),"Incorrect opticalTable flag");
                Require(response.getInt("responseMode",row)==(mode=="tabulated"?1:2),"Incorrect responseMode");
                // This fixture is a depositing 1 GeV muon with O(10^4) expected
                // photons. Zero actual scintillation exposes disabled physics.
                Require(mode=="geant4" ? response.getDouble("Nscintillation",row)>0 : response.getDouble("Nscintillation",row)==-1,
                        "Incorrect actual scintillation count/sentinel");
                Require(response.getInt("fiberCount",row)>0,"Missing table fiber count");
                Require(response.getDouble("Edep",row)>0 && response.getDouble("Evis",row)>0 && response.getDouble("Nphotons",row)>0,"Missing deposited energy/light");
                const int hitn=response.getInt("hitn",row);bool joined=false;
                for(int tr=0;tr<truth.getRows();++tr)if(truth.getInt("hitn",tr)==hitn) {
                    Require(truth.getInt("detector",tr)==110,"Wrong MC::True MINE detector ID");
                    Require(Near(response.getDouble("Edep",row),truth.getFloat("totEdep",tr),2e-6),"Deposited energy differs from truth");joined=true;break;
                }
                Require(joined,"MINE hit cannot be joined to MC::True by hitn");
                const double hash=response.getDouble("opticalHash",row);
                Require(hash>0 && hash<4503599627370496. && std::floor(hash)==hash,"Optical fingerprint lost integer/double precision");
                if(firstHash<0)firstHash=hash;else Require(hash==firstHash,"Optical table changed within file");
                if(expectedHash)Require(hash==static_cast<double>(expectedHash),"Optical fingerprint differs from expected source table");
                for(int sensor=0;sensor<2;++sensor) {
                    const auto suffix=std::to_string(sensor);
                    const double collected=response.getDouble(("Ncollected"+suffix).c_str(),row);
                    const double transported=response.getDouble(("Ntransported"+suffix).c_str(),row);
                    const double mean=response.getDouble(("meanNpe"+suffix).c_str(),row);
                    Require(collected>=0 && (mode=="geant4" || collected<=response.getDouble("Nphotons",row)),"Impossible collected photon count");
                    Require(transported>=0 && transported<=collected+1e-9 && mean>=0 && mean<=transported+1e-9,"Invalid coupling/PDE photon bookkeeping");
                    Require(response.getInt(("Npe"+suffix).c_str(),row)>=0 && response.getInt(("Nfired"+suffix).c_str(),row)>=0,"Negative PE/pixel count");
                    if(mode=="geant4" && response.getInt("idealSiPM",row)) {
                        Require(Near(transported,response.getInt(("Npe"+suffix).c_str(),row)),"Ideal explicit photons were resampled or lost at PDE");
                        Require(response.getInt(("Nfired"+suffix).c_str(),row)==response.getInt(("Npe"+suffix).c_str(),row),"Ideal explicit photons lost to saturation");
                    }
                    Require(Near(transported>0?mean/transported:0,response.getDouble(("effectivePDE"+suffix).c_str(),row)),"Mean PE and effective PDE disagree");
                    if(!response.getInt(("timingValid"+suffix).c_str(),row))Require(response.getDouble(("TDC"+suffix).c_str(),row)==-1,"Invalid sensor time lacks sentinel");
                }
                const bool both=response.getInt("timingValid0",row) && response.getInt("timingValid1",row);
                Require(response.getInt("bothTimingValid",row)==static_cast<int>(both),"Combined timing validity differs from channels");
                if(both) {
                    const double t0=response.getDouble("TDC0",row),t1=response.getDouble("TDC1",row);
                    Require(Near(response.getDouble("TDCmean",row),(t0+t1)/2),"Combined mean time differs");
                    Require(Near(response.getDouble("TDCdifference",row),t1-t0),"Combined time difference differs");
                } else Require(response.getDouble("TDCmean",row)==-1 && response.getDouble("TDCdifference",row)==-1,"Missing combined timing sentinel");
            }
            ++events;
        }
        Require(events==expectedEvents,"Wrong number of retained events: "+std::to_string(events));
        Require(hitEvents==(lund?expectedEvents-1:expectedEvents),"Wrong number of hit/no-hit events");
        std::cout<<"HIPO verification passed: events="<<events<<", MINE hit events="<<hitEvents<<", hit rows="<<rows<<", fields="<<expectedFields.size()<<", original LUND="<<(lund?"checked":"not requested")<<", exact opticalHash="<<static_cast<std::uint64_t>(firstHash)<<'\n';
        return 0;
    }catch(const std::exception& error){std::cerr<<"HIPO CHECK ERROR: "<<error.what()<<'\n';return 1;}
}
