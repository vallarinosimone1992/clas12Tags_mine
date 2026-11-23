#include "../../../source/src/MineProgress.h"
#include <cassert>
#include <iostream>
#include <limits>

int main() {
    using mine::RunProgress;
    const RunProgress::TimePoint t{};
    const auto at = [t](int seconds) { return t + std::chrono::seconds(seconds); };
    RunProgress p(5);
    p.begin(10, t);
    assert(p.report(false, t).find("ETA=waiting for the first event") != std::string::npos);
    p.begin(10, t);
    p.finishEvent(true);
    assert(p.due(at(2))); // First completion is always visible.
    const auto first = p.report(false, at(2));
    assert(first.find("events=1/10 (10.0%)") != std::string::npos);
    assert(first.find("written=1") != std::string::npos);
    assert(first.find("rate=0.50 ev/s | ETA=00:00:18") != std::string::npos);
    p.finishEvent(false); // Filtered/skip: processed, not written.
    assert(!p.due(at(6)));
    assert(p.due(at(7)));
    const auto second = p.report(false, at(8));
    assert(second.find("events=2/10") != std::string::npos);
    assert(second.find("written=1") != std::string::npos);
    assert(second.find("ETA=00:00:32") != std::string::npos);
    p.finishEvent(false, true); // An aborted event is not completed.
    const auto interrupted = p.report(true, at(9));
    assert(interrupted.find("run interrupted: events=2/10") != std::string::npos);
    assert(interrupted.find("ETA=n/a") != std::string::npos);
    assert(interrupted.find("aborted events=1") != std::string::npos);
    assert(p.written() == 1);
    p.begin(1, at(20)); // GUI/macro run counters reset, even if event IDs repeat.
    p.finishEvent(true);
    assert(!p.due(at(22))); // EndOfRun reports the last event without a duplicate.
    const auto last = p.report(true, at(22));
    assert(last.find("run completed: events=1/1 (100.0%)") != std::string::npos);
    assert(last.find("written=1") != std::string::npos);
    assert(last.find("ETA=00:00:00") != std::string::npos);
    p.begin(2, at(30));
    const auto empty = p.report(true, at(31));
    assert(empty.find("events=0/2") != std::string::npos);
    assert(empty.find("ETA=n/a") != std::string::npos);
    for (double invalid : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::quiet_NaN()}) {
        bool caught = false;
        try { RunProgress bad(invalid); } catch (const std::invalid_argument&) { caught = true; }
        assert(caught);
    }
    std::cout << "Progress accounting/ETA/reset/filter/abort checks passed\n";
}
