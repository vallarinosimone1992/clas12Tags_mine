#include "mine_config_metadata.h"
#include <hipo4/reader.h>
#include <hipo4/writer.h>
#include <fstream>
#include <sstream>
#include <iostream>
std::string read(const char* p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Cannot read config");std::ostringstream s;s<<f.rdbuf();return s.str();}
int main(int argc,char** argv){try{
 if(argc!=5&&argc!=6)throw std::runtime_error("Usage: TEST read INPUT SOURCE CONFIG | TEST copy INPUT OUTPUT SOURCE CONFIG");
 const bool copy=std::string(argv[1])=="copy";if(copy!=(argc==6))throw std::runtime_error("Invalid mode");
 const std::string source=argv[copy?4:3],text=read(argv[copy?5:4]);
 hipo::reader input;input.open(argv[2]);hipo::dictionary d;input.readDictionary(d);
 hipo::schema schema("MINE::config",4600,4);schema.parse(mine::ConfigMetadataSchema);
 hipo::writer output;if(copy){for(const auto& name:d.getSchemaList())if(name!="MINE::config")output.getDictionary().addSchema(d.getSchema(name.c_str()));output.getDictionary().addSchema(schema);output.open(argv[3]);}
 else {if(!d.hasSchema("MINE::config"))throw std::runtime_error("No config schema");schema=d.getSchema("MINE::config");}
 int events=0;
 while(input.next()){
  hipo::event event;input.read(event);
  if(copy){hipo::bank bank(schema,mine::ConfigMetadataRows(source,text));mine::WriteConfigMetadata(bank,source,text);event.getEventBuffer().resize(event.getSize()+2*mine::ConfigMetadataRows(source,text)+4096);event.addStructure(bank);output.addEvent(event);}
  else {hipo::bank bank(schema);event.getStructure(bank);const auto data=mine::ReadConfigMetadata(bank);if(data.first!=source||data.second!=text)throw std::runtime_error("Config/path round-trip mismatch");}
  ++events;
 }
 if(copy)output.close();if(!events)throw std::runtime_error("No events checked");
 // Encoding rejects NUL corruption and oversized payloads.
 bool rejected=false;try{mine::ConfigMetadataRows(source,std::string("a\0b",3));}catch(const std::runtime_error&){rejected=true;}if(!rejected)throw std::runtime_error("NUL accepted");
 std::cout<<"HIPO_CONFIG_METADATA_PASS mode="<<(copy?"copy":"read")<<" events="<<events<<" source="<<source<<std::endl;return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
