#include "../common/temporal_graph.hpp"
#include "../common/measurement.hpp"
using namespace thi;

// Clean reimplementation of Algorithm 1 (STREAM) from
// Oettershagen, Kriege, Mutzel, KDD 2023.
// The paper/official implementation assumes uniform transition time lambda=1.

static uint32_t Ht(const std::vector<std::pair<Time,uint32_t>>& c, Time t) {
    const uint32_t n=(uint32_t)c.size();
    std::vector<uint32_t> bucket(n+1,0);
    for(const auto &p:c){
        if(p.first<=t) break; // vectors are appended in reverse chronological order
        ++bucket[std::min<uint32_t>(p.second,n)];
    }
    uint32_t cnt=0;
    for(uint32_t k=n;;--k){ cnt+=bucket[k]; if(cnt>=k) return k; }
}
static uint32_t Hfinal(const std::vector<std::pair<Time,uint32_t>>& c){
    std::vector<uint32_t> vals; vals.reserve(c.size());
    for(auto &p:c) vals.push_back(p.second);
    return h_index_static(vals);
}

int main(int argc,char**argv){
    try {
    if(argc==2 && std::string(argv[1])=="--help"){std::cout<<"usage: stream <edge-file> <order> [--metrics file.json]\n";return 0;}
    if(argc!=3 && !(argc==5 && std::string(argv[3])=="--metrics")){ std::cerr<<"usage: stream <edge-file> <order> [--metrics file.json]\n"; return 1; }
    if(argc==5 && std::string(argv[4])==argv[1]) throw std::runtime_error("metrics path must differ from input");
    auto start=MeasureClock::now();
    TemporalGraph g=TemporalGraph::load(argv[1]);
    auto loaded=MeasureClock::now();
    int order=order_arg(argv[2]);
    for(auto &e:g.edges) if(e.lambda!=1){
        std::cerr<<"unsupported: STREAM requires uniform lambda=1; found lambda="<<e.lambda<<"\n"; return 3;
    }
    std::stable_sort(g.edges.begin(),g.edges.end(),[](const Edge&a,const Edge&b){return a.t>b.t;});
    auto prepared=MeasureClock::now();
    using Pair=std::pair<Time,uint32_t>;
    std::vector<std::vector<std::vector<Pair>>> pi(g.n,
        std::vector<std::vector<Pair>>(order+1));
    std::vector<uint32_t> deg(g.n,0);
    std::vector<Time> last(g.n,std::numeric_limits<Time>::max());

    for(const auto &e:g.edges){
        if(e.t<0) continue; // root query is q=0; lambda=1 implies no negative child is needed
        if(last[e.u]>e.t){ last[e.u]=e.t; deg[e.u]=(uint32_t)pi[e.u][0].size(); }
        if(last[e.v]>e.t){ last[e.v]=e.t; deg[e.v]=(uint32_t)pi[e.v][0].size(); }
        pi[e.u][0].push_back({e.t,1});
        if(order==0) continue;
        pi[e.u][1].push_back({e.t,deg[e.v]});
        for(int j=1;j<order;++j)
            pi[e.u][j+1].push_back({e.t,Ht(pi[e.v][j],e.t)});
    }
    std::vector<uint32_t> answer(g.n);
    for(int v=0;v<g.n;++v){
        uint32_t ans = order==0 ? (uint32_t)pi[v][0].size() : Hfinal(pi[v][order]);
        answer[v]=ans;
    }
    auto solved=MeasureClock::now();
    write_measurement(argc==5?argv[4]:"",seconds(start,loaded),seconds(loaded,prepared),
                      seconds(prepared,solved),g.n,(int)g.edges.size(),-1);
    for(int v=0;v<g.n;++v)std::cout<<g.original_ids[v]<<" "<<answer[v]<<"\n";
    return 0;
    } catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<"\n";return 2;}
}
