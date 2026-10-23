#include "../common/temporal_graph.hpp"
#include "../common/measurement.hpp"
#include <numeric>
using namespace thi;
#include "coalesced_solver.hpp"

int main(int argc, char** argv) {
    try {
        const std::string usage = "usage: thor <edge-file> <order> "
            "[--metrics file.json] [--stats file.csv] [--work-stats file.csv]";
        if (argc == 2 && std::string(argv[1]) == "--help") {
            std::cout << usage << '\n';
            return 0;
        }
        if (argc < 3) throw std::runtime_error(usage);
        const int order = order_arg(argv[2]);
        std::string metrics_path, stats_path, work_path;
        for (int i = 3; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--metrics" && i + 1 < argc) metrics_path = argv[++i];
            else if (arg == "--stats" && i + 1 < argc) stats_path = argv[++i];
            else if (arg == "--work-stats" && i + 1 < argc) work_path = argv[++i];
            else throw std::runtime_error("unknown or incomplete option: " + arg);
        }
        const std::vector<std::string> paths = {argv[1], metrics_path, stats_path, work_path};
        for (size_t i = 0; i < paths.size(); ++i)
            for (size_t j = i + 1; j < paths.size(); ++j)
                if (!paths[i].empty() && paths[i] == paths[j])
                    throw std::runtime_error("input and output paths must differ");
        std::ofstream stats, work_stats;
        if (!stats_path.empty()) {
            stats.open(stats_path);
            if (!stats) throw std::runtime_error("cannot open stats output");
            stats << "order,mode,A,D,attempt_updates,attempt_repairs,seconds\n";
        }
        if (!work_path.empty()) {
            work_stats.open(work_path);
            if (!work_stats) throw std::runtime_error("cannot open work statistics output");
            work_stats << "order,vertices_scanned,edges_scanned,edges_skipped_saturation,"
                          "changed_classes,dependency_visits,dependency_built\n";
        }
        const auto start = MeasureClock::now();
        auto graph = TemporalGraph::load(argv[1]);
        const auto loaded = MeasureClock::now();
        CoalescedSolver solver(std::move(graph));
        const auto prepared = MeasureClock::now();
        if (stats.is_open()) solver.stats = &stats;
        if (work_stats.is_open()) solver.work_stats = &work_stats;
        auto answer = solver.solve(order);
        const auto solved = MeasureClock::now();
        write_measurement(metrics_path, seconds(start, loaded), seconds(loaded, prepared),
                          seconds(prepared, solved), solver.N, solver.m,
                          solver.R, solver.effective_states);
        solver.print_vertex(answer);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 2;
    }
}
