#include "MineRunAction.h"
#include "G4Event.hh"
#include "G4Exception.hh"
#include "G4Run.hh"
#include "G4RunManager.hh"
#include <cstdlib>
#include <exception>
#include <iostream>

namespace {
double progressInterval() {
    const char* value = std::getenv("MINE_PROGRESS_SECONDS");
    if (!value) return 5.0;
    char* end = nullptr;
    const double interval = std::strtod(value, &end);
    if (end == value || *end != '\0' || !std::isfinite(interval) || interval <= 0) {
        G4Exception("MineRunAction", "MINE_PROGRESS_CONFIG", FatalException,
                    "MINE_PROGRESS_SECONDS must be a finite number > 0 (default 5 seconds)");
        return 5.0;
    }
    return interval;
}
}

MineRunAction::MineRunAction() : progress_(progressInterval()) {}

void MineRunAction::BeginOfRunAction(const G4Run* run) {
    const auto requested = G4RunManager::GetRunManager()->GetNumberOfEventsToBeProcessed();
    progress_.begin(requested);
    std::cout << "MINE run started " << run->GetRunID() << ": requested events=" << requested
              << " | previously written in this process=" << previouslyWritten_
              << " | ETA available after the first event" << std::endl;
}

void MineRunAction::EndOfRunAction(const G4Run*) {
    std::cout << progress_.report(true) << std::endl;
    previouslyWritten_ += progress_.written();
    std::cout << "MINE total events written in this process=" << previouslyWritten_
              << " (output finalized when GEMC closes)" << std::endl;
}

void MineRunAction::finishEvent(bool written, bool aborted) {
    progress_.finishEvent(written, aborted);
    if (progress_.due()) std::cout << progress_.report() << std::endl;
}

MineRunAction::EventCompletion::EventCompletion(MineRunAction* action, const G4Event* event)
    : action_(action), event_(event), exceptions_(std::uncaught_exceptions()) {}

MineRunAction::EventCompletion::~EventCompletion() {
    if (action_ && std::uncaught_exceptions() == exceptions_)
        action_->finishEvent(written_, event_->IsAborted());
}
