#include "thix_recursive.h"

using namespace std;

uint H(vector<uint> &c) {
    uint n = c.size();
    vector<uint> bucket(n + 1);
    for (uint i = 0; i < n; i++) {
        uint x = c[i];
        if (x >= n) {
            bucket[n]++;
        } else {
            bucket[x]++;
        }
    }
    uint cnt = 0;
    for (uint i = n;; i--) {
        cnt += bucket[i];
        if (cnt >= i) return i;
    }
}

Thix_Recursive::Thix_Recursive(uint num_nodes, uint n) {
    n_order = n;
    hfm.resize(num_nodes, vector<map<Time, uint>>(n + 1));
}

uint Thix_Recursive::hf(TemporalGraph &tg, NodeId nid, uint n, Time t) {
    if (hfm[nid][n].find(t) != hfm[nid][n].end()) {
        return hfm[nid][n][t];
    }

    if (tg.nodes[nid].adjlist.empty()) {
        return 0;
    }

    if (n == 0) {
        auto neighbors = tg.nodes[nid].getNeighboursAndTimes(t);
        hfm[nid][n][t] = neighbors.size();
        return neighbors.size();
    }
    vector<uint> hs;

    auto neighbors = tg.nodes[nid].getNeighboursAndTimes(t);
    for (auto &v: neighbors) {
        auto hv = hf(tg, v.first, n - 1, v.second);
        hs.push_back(hv);
    }

    auto h = H(hs);
    hfm[nid][n][t] = h;
    return h;
}

std::vector<pair<NodeId, double>> Thix_Recursive::compute_hindex(TemporalGraph &tg) {
    vector<pair<NodeId, double>> result;

    for (uint i = 0; i < tg.num_nodes; ++i) {
        double h = hf(tg, i, n_order, 0);
        result.emplace_back(i, h);
    }

    return result;
}
