#pragma once
#include <chrono>
#include <fstream>
#include <iomanip>
#include <ostream>
#include <stdexcept>
#include <string>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

namespace thi {
using MeasureClock = std::chrono::steady_clock;

inline double seconds(MeasureClock::time_point a, MeasureClock::time_point b) {
    return std::chrono::duration<double>(b - a).count();
}

inline void write_peak_rss(std::ostream& out) {
#if defined(__unix__) || defined(__APPLE__)
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        long long bytes = usage.ru_maxrss;
#ifndef __APPLE__
        bytes *= 1024;
#endif
        out << ",\"peak_rss_bytes\":" << bytes;
    }
#else
    (void)out;
#endif
}

inline void write_measurement(const std::string& path, double load, double prep,
                              double solve, int vertices, int edges, int states,
                              int effective_states = -1) {
    if (path.empty()) return;
    std::ofstream out(path);
    if (!out) throw std::runtime_error("cannot write metrics: " + path);
    out << std::setprecision(12)
        << "{\"load_s\":" << load << ",\"prep_s\":" << prep
        << ",\"solve_s\":" << solve << ",\"algorithm_s\":" << (prep + solve)
        << ",\"total_s\":" << (load + prep + solve)
        << ",\"vertices\":" << vertices << ",\"edges\":" << edges
        << ",\"states\":" << states << ",\"effective_states\":"
        << (effective_states < 0 ? states : effective_states);
    write_peak_rss(out);
    out << "}\n";
}
} // namespace thi
