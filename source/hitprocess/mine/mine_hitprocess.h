#ifndef MINE_HITPROCESS_H
#define MINE_HITPROCESS_H
#include "HitProcess.h"
#include "mine_response.h"

class mine_HitProcess : public HitProcess {
public:
    void initWithRunNumber(int) override;
    map<string, double> integrateDgt(MHit*, int) override;
    map<string, vector<int>> multiDgt(MHit*, int) override { return {}; }
    map<int, vector<double>> chargeTime(MHit*, int) override;
    double voltage(double charge, double time, double forTime) override;
    vector<identifier> processID(vector<identifier>, G4Step*, detector) override;
    // Dark noise is currently simulated only within a depositing hit's gate.
    // No channel inventory exists for generating independent noise-only MHits.
    vector<MHit*> electronicNoise() override { return {}; }
    static HitProcess* createHitClass() { return new mine_HitProcess; }

    // Caller owns the result; reuse it for timing studies without resampling RNG.
    mine::Response response(MHit*);
};
#endif
