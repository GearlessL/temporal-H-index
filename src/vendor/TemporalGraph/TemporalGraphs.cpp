#include "TemporalGraphs.h"
#include <string>
#include <set>

using namespace std;

void TGNode::addEdge(TemporalEdge e) {
    adjlist.push_back(e);
}

std::vector<std::pair<NodeId, Time>> TGNode::getNeighboursAndTimes(const Time t) const {
    std::vector<std::pair<NodeId, Time>> result;
    auto it = lower_bound(adjlist.begin(), adjlist.end(), t,
                          [](auto &e, Time f){return e.t < f;});

    while (it != adjlist.end()) {
        result.emplace_back((*it).v_id, (*it).t + (*it).traversal_time);
        ++it;
    }
    return result;
}

TemporalGraph TemporalGraphStream::toTemporalGraph() {
    TemporalGraph g;

    for (unsigned int i = 0; i < num_nodes; ++i) {
        TGNode node;
        node.id = i;
        g.nodes.push_back(node);
        g.num_nodes++;
    }

    for (auto &e : edges) {
        g.nodes.at(e.u_id).addEdge(e);
    }
    g.num_edges = edges.size();

    g.setInedges();
    return g;
}

void TemporalGraphStream::sort_edges() {
    std::sort(edges.begin(), edges.end(), [](const TemporalEdge & a, const TemporalEdge & b) -> bool { return a.t < b.t; });
}


