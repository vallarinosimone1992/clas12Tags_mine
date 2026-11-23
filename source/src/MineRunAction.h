#ifndef MINE_RUN_ACTION_H
#define MINE_RUN_ACTION_H

#include "G4UserRunAction.hh"
#include "MineProgress.h"

class G4Event;

// One instance per sequential GEMC process. Each BeamOn resets run counters.
class MineRunAction : public G4UserRunAction {
public:
    MineRunAction();
    void BeginOfRunAction(const G4Run*) override;
    void EndOfRunAction(const G4Run*) override;

    // Covers every EndOfEventAction return, including filtered/skipped events.
    // Exceptions and aborted events never masquerade as completed events.
    class EventCompletion {
    public:
        EventCompletion(MineRunAction* action, const G4Event* event);
        ~EventCompletion();
        void markWritten() { written_ = true; }
    private:
        MineRunAction* action_;
        const G4Event* event_;
        int exceptions_;
        bool written_ = false;
    };
private:
    void finishEvent(bool written, bool aborted);
    mine::RunProgress progress_;
    std::uint64_t previouslyWritten_ = 0;
};
#endif
