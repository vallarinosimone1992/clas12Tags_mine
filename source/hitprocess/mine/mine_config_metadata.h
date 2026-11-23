#pragma once
// MINE::config (4600,4): two UTF-8 byte columns, row order is byte order.
// Shorter column is NUL padded. Repeated per event so skims retain provenance.
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
namespace mine {
constexpr const char* ConfigMetadataSchema="sourceByte/B,configByte/B";
constexpr std::size_t MaxConfigMetadataBytes=1024*1024;
inline int ConfigMetadataRows(const std::string& source,const std::string& text){
    const auto n=std::max(source.size(),text.size());
    if(source.empty()||text.empty()||n>MaxConfigMetadataBytes||source.find('\0')!=std::string::npos||text.find('\0')!=std::string::npos)
        throw std::runtime_error("Invalid MINE config metadata size/content");
    return static_cast<int>(n);
}
template<class Bank> void WriteConfigMetadata(Bank& bank,const std::string& source,const std::string& text){
    if(bank.getRows()!=ConfigMetadataRows(source,text))throw std::runtime_error("Incorrect MINE config bank size");
    for(int row=0;row<bank.getRows();++row){bank.putByte("sourceByte",row,row<int(source.size())?static_cast<std::int8_t>(source[row]):0);bank.putByte("configByte",row,row<int(text.size())?static_cast<std::int8_t>(text[row]):0);}
}
template<class Bank> std::pair<std::string,std::string> ReadConfigMetadata(Bank& bank){
    if(bank.getRows()<1||std::size_t(bank.getRows())>MaxConfigMetadataBytes)throw std::runtime_error("Invalid MINE config bank size");
    auto decode=[&](const char* column){std::string result;bool padding=false;for(int row=0;row<bank.getRows();++row){const auto byte=bank.getByte(column,row);if(!byte)padding=true;else{if(padding)throw std::runtime_error("Invalid interior NUL in MINE config metadata");result+=static_cast<char>(byte);}}return result;};
    auto source=decode("sourceByte"),text=decode("configByte");ConfigMetadataRows(source,text);return {source,text};
}
}
