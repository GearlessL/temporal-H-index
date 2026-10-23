#ifndef TEMPORALGRAPHS_H
#define TEMPORALGRAPHS_H

#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include <algorithm>
#include <limits>
#include <unordered_set>
#include <unordered_map>
#include <map>

using NodeId = unsigned int;
using EdgeId = unsigned int;
using Time = unsigned long;
using Label = long;

struct TemporalEdge;
struct TGNode;
using AdjacenceList = std::vector<TemporalEdge>;

static constexpr auto MAX_UINT_VALUE = std::numeric_limits<int>::max();


struct TGNode {
    NodeId id = 0;
    void addEdge(TemporalEdge t);
    AdjacenceList adjlist;
    AdjacenceList inedeges;
    [[nodiscard]] std::vector<std::pair<NodeId, Time>> getNeighboursAndTimes(Time t) const;
};


struct TemporalEdge {
    TemporalEdge() = default;

    TemporalEdge(NodeId u, NodeId v, Time time, Time tt, EdgeId eid) : u_id(u), v_id(v), t(time),
            traversal_time(tt), id(eid) {  }

    TemporalEdge(NodeId u, NodeId v, Time time, EdgeId eid) : u_id(u), v_id(v), t(time),
            traversal_time(1), id(eid) {  }

    TemporalEdge(NodeId u, NodeId v, Time time) : u_id(u), v_id(v), t(time),
            traversal_time(1), id(0) {  }

    NodeId u_id{}, v_id{};
    Time t{};
    Time traversal_time{};

    bool operator () ( TemporalEdge const * lhs, TemporalEdge const * rhs ) const {
        return lhs->u_id == rhs->u_id && lhs->v_id == rhs->v_id && lhs->t == rhs->t;
    }

    EdgeId id{};

    TemporalEdge(const TemporalEdge& e) : u_id(e.u_id), v_id(e.v_id), t(e.t),
            traversal_time(e.traversal_time), id(e.id), values(e.values), rvalues(e.rvalues) { }

    std::vector<int> values;
    std::vector<double> rvalues;

};


using TGNodes = std::vector<TGNode>;
using TemporalEdges = std::vector<TemporalEdge>;

struct TemporalGraphStream;

struct TemporalGraph {
    TemporalGraph () : num_nodes(0), num_edges(0), label(0), nodes(TGNodes()), minTimeDelta(0) {};

    TemporalGraph (const TemporalGraph& tg) {
        for (TGNode const &n : tg.nodes) {
            TGNode nn;
            nn.id = n.id;
            for (auto &e : n.adjlist) {
                nn.adjlist.push_back(e);
            }
            nodes.push_back(nn);
        }

        num_edges = tg.num_edges;
        num_nodes = tg.num_nodes;
        label = tg.label;
        minTimeDelta = tg.minTimeDelta;
        maxTime = tg.maxTime;
        for (auto e : tg.edgeStream)
            edgeStream.push_back(e);
    }

    unsigned int num_nodes;
    unsigned int num_edges;
    Label label;
    TGNodes nodes;
    Time minTimeDelta;
    Time maxTime{};

    std::vector<TemporalEdge*> edgeStream;

    void setInedges(){
        for (auto &n : nodes) {
            for (auto &e : n.adjlist) {
                nodes[e.v_id].inedeges.push_back(e);
            }
        }
    }
};


struct TemporalGraphStream {
    unsigned long num_nodes;
    TemporalEdges edges;
    void sort_edges();
    TemporalGraph toTemporalGraph();
};


#endif //TEMPORALGRAPHS_H
