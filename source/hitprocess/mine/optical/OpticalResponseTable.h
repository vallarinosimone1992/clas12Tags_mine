#pragma once

// Portable response reader: standard C++17 only, suitable for the GEMC HitProcess.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace LightTrapping {
struct OpticalSensorResponse {
    std::uint64_t arrived{0};
    double sumWeight{0},sumWeightSquared{0};
    std::vector<double> timeNs,cdf;
};
struct OpticalResponseCell {
    std::uint64_t generated{0};
    std::array<OpticalSensorResponse,2> sensors;
    double Efficiency(int sensor) const { return generated?sensors.at(sensor).sumWeight/generated:0; }
    double StandardError(int sensor) const {
        if (!generated) return 0;
        const double p=Efficiency(sensor);
        return std::sqrt(std::max(0.0,sensors.at(sensor).sumWeightSquared/generated-p*p)/generated);
    }
};

class OpticalResponseTable {
public:
    std::map<std::string,std::string> metadata;
    std::vector<double> xMm,yMm;
    double xMinMm{0},xMaxMm{0},yMinMm{0},yMaxMm{0};
    std::vector<OpticalResponseCell> cells; // iy*NX+ix

    void Validate() const {
        auto axis=[](const std::vector<double>& nodes,double lo,double hi) {
            if (nodes.empty() || !std::isfinite(lo) || !std::isfinite(hi) || hi<lo)
                throw std::runtime_error("Invalid optical response domain");
            for (std::size_t i=0;i<nodes.size();++i)
                if (!std::isfinite(nodes[i]) || nodes[i]<lo || nodes[i]>hi || (i && nodes[i]<=nodes[i-1]))
                    throw std::runtime_error("Invalid optical response axis");
        };
        axis(xMm,xMinMm,xMaxMm);axis(yMm,yMinMm,yMaxMm);
        if (xMm.size()>std::numeric_limits<std::size_t>::max()/yMm.size() || cells.size()!=xMm.size()*yMm.size())
            throw std::runtime_error("Optical response cell count does not match axes");
        for (const auto& cell:cells) {
            if (cell.sensors[0].arrived>cell.generated || cell.sensors[1].arrived>cell.generated-cell.sensors[0].arrived)
                throw std::runtime_error("More collected photons than generated photons");
            if (cell.Efficiency(0)+cell.Efficiency(1)>1+1e-12)
                throw std::runtime_error("Optical response probabilities exceed unity");
            for (const auto& sensor:cell.sensors) {
                if (!std::isfinite(sensor.sumWeight) || !std::isfinite(sensor.sumWeightSquared) ||
                    sensor.sumWeight<0 || sensor.sumWeightSquared<0 || sensor.sumWeight>sensor.arrived+1e-9 ||
                    sensor.sumWeightSquared>sensor.sumWeight+1e-9 || sensor.timeNs.size()!=sensor.cdf.size() ||
                    sensor.timeNs.size()>sensor.arrived || ((sensor.sumWeight>0)!=!sensor.cdf.empty()))
                    throw std::runtime_error("Invalid optical sensor weights or sample count");
                for (std::size_t i=0;i<sensor.cdf.size();++i) {
                    if (!std::isfinite(sensor.timeNs[i]) || sensor.timeNs[i]<0 ||
                        !std::isfinite(sensor.cdf[i]) || sensor.cdf[i]<0 || sensor.cdf[i]>1 ||
                        (i && (sensor.timeNs[i]<sensor.timeNs[i-1] || sensor.cdf[i]<sensor.cdf[i-1])))
                        throw std::runtime_error("Invalid optical transport-time CDF");
                }
                if (!sensor.cdf.empty() && sensor.cdf.back()!=1.0)
                    throw std::runtime_error("Optical transport-time CDF is not normalized");
            }
        }
    }

    double Efficiency(double x,double y,int sensor) const {
        CheckSensor(sensor);
        double p=0;for (const auto& c:Contributions(x,y)) p+=c.weight*cells[c.index].Efficiency(sensor);
        return p;
    }

    // Inputs are two independent uniforms in [0,1). Spatial interpolation is
    // applied to the joint collection/time response: mixture weights are w*epsilon.
    // Times are sampled from the empirical CDF without a second transport delay.
    double SampleTransportTime(double x,double y,int sensor,double mixtureUniform,double timeUniform) const {
        CheckSensor(sensor);
        if (!(mixtureUniform>=0 && mixtureUniform<1 && timeUniform>=0 && timeUniform<1))
            throw std::invalid_argument("Time sampler requires independent uniforms in [0,1)");
        const auto parts=Contributions(x,y);
        double total=0;for (const auto& c:parts) total+=c.weight*cells[c.index].Efficiency(sensor);
        if (!(total>0)) throw std::runtime_error("Cannot sample transport time for zero collection probability");
        double threshold=mixtureUniform*total;
        const OpticalSensorResponse* chosen=nullptr;
        for (const auto& c:parts) {
            const double weight=c.weight*cells[c.index].Efficiency(sensor);
            if (weight<=0) continue;
            chosen=&cells[c.index].sensors[sensor];
            if (threshold<weight) break;
            threshold-=weight;
        }
        if (!chosen || chosen->cdf.empty()) throw std::runtime_error("Missing conditional transport distribution");
        const auto found=std::upper_bound(chosen->cdf.begin(),chosen->cdf.end(),timeUniform);
        return chosen->timeNs.at(static_cast<std::size_t>(found-chosen->cdf.begin()));
    }

    void Save(const std::string& path) const {
        Validate();
        std::ofstream out(path);
        if (!out) throw std::runtime_error("Cannot write optical response: "+path);
        out<<std::setprecision(17)<<"MINE_OPTICAL_RESPONSE 1\nMETA "<<metadata.size()<<'\n';
        for (const auto& item:metadata) out<<std::quoted(item.first)<<' '<<std::quoted(item.second)<<'\n';
        out<<"DOMAIN "<<xMinMm<<' '<<xMaxMm<<' '<<yMinMm<<' '<<yMaxMm<<'\n';
        out<<"X "<<xMm.size();for (double v:xMm) out<<' '<<v;out<<'\n';
        out<<"Y "<<yMm.size();for (double v:yMm) out<<' '<<v;out<<'\n';
        for (std::size_t i=0;i<cells.size();++i) {
            const auto& cell=cells[i];out<<"CELL "<<i<<' '<<cell.generated<<'\n';
            for (int s=0;s<2;++s) {
                const auto& response=cell.sensors[s];
                out<<"SENSOR "<<s<<' '<<response.arrived<<' '<<response.sumWeight<<' '
                   <<response.sumWeightSquared<<' '<<response.timeNs.size()<<'\n';
                for (std::size_t k=0;k<response.timeNs.size();++k)
                    out<<response.timeNs[k]<<' '<<response.cdf[k]<<'\n';
            }
        }
        out<<"END\n";out.close();
        if (!out) throw std::runtime_error("Failed writing optical response: "+path);
    }

    static OpticalResponseTable Load(const std::string& path) {
        std::ifstream in(path);if (!in) throw std::runtime_error("Cannot read optical response: "+path);
        auto token=[&](const std::string& expected) {
            std::string value;if (!(in>>value) || value!=expected)
                throw std::runtime_error("Malformed optical response: expected "+expected);
        };
        auto count=[&](std::size_t maximum) {
            std::uint64_t n;if (!(in>>n) || n>maximum) throw std::runtime_error("Invalid optical response size");
            return static_cast<std::size_t>(n);
        };
        token("MINE_OPTICAL_RESPONSE");int version=0;in>>version;
        if (version!=1) throw std::runtime_error("Unsupported optical response format");
        OpticalResponseTable table;token("META");const auto nm=count(1000);
        for (std::size_t i=0;i<nm;++i) {
            std::string k,v;if (!(in>>std::quoted(k)>>std::quoted(v)) || !table.metadata.emplace(k,v).second)
                throw std::runtime_error("Malformed/duplicate optical response metadata");
        }
        token("DOMAIN");in>>table.xMinMm>>table.xMaxMm>>table.yMinMm>>table.yMaxMm;
        token("X");table.xMm.resize(count(1000000));for (auto& v:table.xMm) in>>v;
        token("Y");table.yMm.resize(count(1000000));for (auto& v:table.yMm) in>>v;
        if (table.xMm.empty() || table.yMm.empty() || table.xMm.size()>1000000/table.yMm.size())
            throw std::runtime_error("Optical response has invalid or excessive spatial dimensions");
        table.cells.resize(table.xMm.size()*table.yMm.size());
        for (std::size_t i=0;i<table.cells.size();++i) {
            token("CELL");std::size_t index=std::numeric_limits<std::size_t>::max();in>>index>>table.cells[i].generated;
            if (index!=i) throw std::runtime_error("Invalid optical response cell order");
            for (int s=0;s<2;++s) {
                token("SENSOR");int sensor=-1;auto& r=table.cells[i].sensors[s];
                in>>sensor>>r.arrived>>r.sumWeight>>r.sumWeightSquared;
                if (sensor!=s) throw std::runtime_error("Invalid optical response sensor order");
                const auto n=count(100000000);r.timeNs.resize(n);r.cdf.resize(n);
                for (std::size_t k=0;k<n;++k) in>>r.timeNs[k]>>r.cdf[k];
            }
        }
        token("END");std::string extra;if (in>>extra) throw std::runtime_error("Trailing optical response data");
        table.Validate();return table;
    }
private:
    struct Contribution { std::size_t index;double weight; };
    static void CheckSensor(int sensor) {
        if (sensor<0 || sensor>1) throw std::invalid_argument("Optical response sensor must be 0 or 1");
    }
    std::vector<Contribution> Contributions(double x,double y) const {
        if (!std::isfinite(x) || !std::isfinite(y) || x<xMinMm || x>xMaxMm || y<yMinMm || y>yMaxMm)
            throw std::out_of_range("Position outside the optical response domain");
        auto bracket=[](const std::vector<double>& nodes,double value) {
            if (value<=nodes.front()) return std::pair<std::size_t,double>{0,0};
            if (value>=nodes.back()) return std::pair<std::size_t,double>{nodes.size()-1,0};
            const auto lo=static_cast<std::size_t>(std::upper_bound(nodes.begin(),nodes.end(),value)-nodes.begin()-1);
            return std::pair<std::size_t,double>{lo,(value-nodes[lo])/(nodes[lo+1]-nodes[lo])};
        };
        const auto bx=bracket(xMm,x),by=bracket(yMm,y);
        std::vector<Contribution> parts;
        for (int iy=0;iy<2;++iy) for (int ix=0;ix<2;++ix) {
            const double weight=(ix?bx.second:1-bx.second)*(iy?by.second:1-by.second);
            if (weight==0) continue;
            const auto index=(by.first+iy)*xMm.size()+bx.first+ix;
            if (!cells.at(index).generated) throw std::runtime_error("Optical response interpolation touches an unsampled cell");
            parts.push_back({index,weight});
        }
        return parts;
    }
};
} // namespace LightTrapping
