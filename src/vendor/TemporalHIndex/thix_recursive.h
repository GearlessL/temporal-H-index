#ifndef TGH_THIX_RECURSIVE_H
#define TGH_THIX_RECURSIVE_H

#include "../TemporalGraph/TemporalGraphs.h"
#include <map>

uint H(std::vector<uint> &v);

struct Thix_Recursive {

    Thix_Recursive(uint num_nodes, uint n);

    uint hf(TemporalGraph &tg, NodeId nid, uint n, Time t);

    std::vector<std::vector<std::map<Time, uint>>> hfm;

    uint n_order;

    std::vector<std::pair<NodeId, double>> compute_hindex(TemporalGraph &tg);

};



#endif //TGH_THIX_RECURSIVE_H
