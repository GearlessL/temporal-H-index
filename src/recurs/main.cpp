#include "../common/temporal_graph.hpp"
#include "../common/measurement.hpp"
#include "../vendor/TemporalHIndex/thix_recursive.h"

int main(int argc, char** argv) {
    try {
        const std::string usage = "usage: recurs_official <edge-file> <order> "
            "[--metrics file.json]";
        if (argc == 2 && std::string(argv[1]) == "--help") {
            std::cout << usage << '\n';
            return 0;
        }
        if (argc < 3) throw std::runtime_error(usage);
        const int order = thi::order_arg(argv[2]);
        std::string metrics;
        for (int i = 3; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--metrics" && i + 1 < argc) metrics = argv[++i];
            else throw std::runtime_error("unknown or incomplete option: " + arg);
        }
        if (!metrics.empty() && metrics == argv[1])
            throw std::runtime_error("metrics path must differ from input");
        const auto start = thi::MeasureClock::now();
        auto raw = thi::TemporalGraph::load(argv[1]);
        const auto loaded = thi::MeasureClock::now();
        if (sizeof(Time) < 8)
            throw std::runtime_error("official Recurs requires a 64-bit unsigned long platform");
        auto ids = std::move(raw.original_ids);
        const int vertices = raw.n, edges = static_cast<int>(raw.edges.size());
        TemporalGraphStream stream;
        stream.num_nodes = vertices;
        for (auto& e : raw.edges) {
            if (e.t < 0) {
                std::cerr << "unsupported: official Recurs uses unsigned time; "
                             "negative departures are rejected\n";
                return 3;
            }
            stream.edges.emplace_back(e.u, e.v, static_cast<Time>(e.t),
                                      static_cast<Time>(e.lambda),
                                      static_cast<EdgeId>(stream.edges.size()));
        }
        raw = thi::TemporalGraph();
        stream.sort_edges();
        auto graph = stream.toTemporalGraph();
        stream.edges.clear();
        stream.edges.shrink_to_fit();
        Thix_Recursive solver(vertices, order);
        const auto prepared = thi::MeasureClock::now();
        auto answer = solver.compute_hindex(graph);
        const auto solved = thi::MeasureClock::now();
        thi::write_measurement(metrics, thi::seconds(start, loaded),
                               thi::seconds(loaded, prepared), thi::seconds(prepared, solved),
                               vertices, edges, -1);
        for (auto& result : answer)
            std::cout << ids[result.first] << ' '
                      << static_cast<unsigned long long>(result.second) << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 2;
    }
}
