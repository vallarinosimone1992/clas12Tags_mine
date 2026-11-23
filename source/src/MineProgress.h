#ifndef MINE_PROGRESS_H
#define MINE_PROGRESS_H

#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>

namespace mine {
// Wall-clock accounting, independent of Geant4 event IDs and output filters.
class RunProgress {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    explicit RunProgress(double intervalSeconds = 5.0) : interval_(intervalSeconds) {
        if (!std::isfinite(interval_) || interval_ <= 0)
            throw std::invalid_argument("MINE_PROGRESS_SECONDS must be finite and > 0");
    }
    void begin(std::uint64_t requested, TimePoint now = Clock::now()) {
        requested_ = requested;
        completed_ = written_ = aborted_ = 0;
        start_ = lastReport_ = now;
        reported_ = false;
    }
    void finishEvent(bool written, bool aborted = false) {
        if (aborted) ++aborted_;
        else ++completed_;
        if (written) ++written_;
    }
    bool due(TimePoint now = Clock::now()) const {
        // The final line is emitted by EndOfRunAction, including very short runs.
        if (completed_ >= requested_) return false;
        return (!reported_ && completed_ > 0) || seconds(now - lastReport_) >= interval_;
    }
    std::string report(bool final = false, TimePoint now = Clock::now()) {
        const double elapsed = seconds(now - start_);
        const double rate = elapsed > 0 ? completed_ / elapsed : 0;
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << "MINE " << (final ? (completed_ >= requested_ ? "run completed" : "run interrupted") : "progress")
            << ": events=" << completed_ << '/' << requested_;
        if (requested_ > 0)
            out << " (" << std::fixed << std::setprecision(1) << 100.0 * completed_ / requested_ << "%)";
        out << " | written=" << written_ << " | elapsed=" << duration(elapsed)
            << " | rate=" << std::fixed << std::setprecision(2) << rate << " ev/s | ETA=";
        if (final && completed_ < requested_) out << "n/a (run ended before completion)";
        else if (completed_ >= requested_) out << "00:00:00";
        else if (rate > 0) out << duration((requested_ - completed_) / rate);
        else out << "waiting for the first event";
        if (aborted_) out << " | aborted events=" << aborted_;
        reported_ = true;
        lastReport_ = now;
        return out.str();
    }
    std::uint64_t written() const { return written_; }

private:
    static double seconds(Clock::duration value) {
        return std::chrono::duration<double>(value).count();
    }
    static std::string duration(double seconds) {
        const auto total = static_cast<std::uint64_t>(std::ceil(seconds > 0 ? seconds : 0));
        std::ostringstream out;
        out << std::setfill('0') << std::setw(2) << total / 3600 << ':'
            << std::setw(2) << (total / 60) % 60 << ':' << std::setw(2) << total % 60;
        return out.str();
    }
    double interval_;
    std::uint64_t requested_ = 0, completed_ = 0, written_ = 0, aborted_ = 0;
    TimePoint start_{}, lastReport_{};
    bool reported_ = false;
};
}
#endif
