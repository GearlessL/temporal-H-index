#include "../common/temporal_graph.hpp"
#include "../common/measurement.hpp"
using namespace thi;

#ifndef THOR_USE_COALESCING
#define THOR_USE_COALESCING 0
#endif

struct InsertH {
    int cap=0,h=0,cnt_next=0,total=0;
    std::vector<int> freq;
    explicit InsertH(int c=0):cap(c),freq(c+1,0){}
    void add(int x){
        ++total; x=std::max(0,std::min(x,cap)); ++freq[x];
        if(x>=h+1) ++cnt_next;
        if(cnt_next>=h+1){ ++h; cnt_next-=freq[h]; }
    }
    int value()const{return h;}
    int slack()const{
        if(h==0) return total; // never becomes invalid below zero
        return cnt_next + freq[h] - h;
    }
};

struct Solver {
    TemporalGraph g; int N{},m{},R{},effective_states{};
    std::vector<std::vector<Time>> Q;
    std::vector<int> off, sid_vertex, sid_local, q0sid;
    std::vector<std::vector<int>> out, rev;
    std::vector<int> base_pos, eid_at_pos;

    explicit Solver(TemporalGraph gg):g(std::move(gg)){prep();}
    // Keep the original baseline preparation and per-layer work for
    // comparability with the reported Basic/NP implementation.
    void prep(){
        N=g.n;m=(int)g.edges.size();Q.assign(N,{});for(int v=0;v<N;++v)Q[v].push_back(0);
        for(auto &e:g.edges)Q[e.v].push_back(e.t+e.lambda);
        off.assign(N+1,0);q0sid.assign(N,-1);
        for(int v=0;v<N;++v){auto &q=Q[v];std::sort(q.begin(),q.end());q.erase(std::unique(q.begin(),q.end()),q.end());off[v+1]=off[v]+(int)q.size();}
        R=off[N];effective_states=R;sid_vertex.resize(R);sid_local.resize(R);for(int v=0;v<N;++v)for(int j=0;j<(int)Q[v].size();++j){int s=off[v]+j;sid_vertex[s]=v;sid_local[s]=j;if(Q[v][j]==0)q0sid[v]=s;}
        out.assign(N,{});rev.assign(R,{});
        for(int e=0;e<m;++e){auto &x=g.edges[e];auto it=std::lower_bound(Q[x.v].begin(),Q[x.v].end(),x.t+x.lambda);x.child_sid=off[x.v]+(int)(it-Q[x.v].begin());out[x.u].push_back(e);rev[x.child_sid].push_back(e);}
        base_pos.assign(N+1,0);eid_at_pos.resize(m);int p=0;
        for(int u=0;u<N;++u){auto &a=out[u];std::stable_sort(a.begin(),a.end(),[&](int x,int y){return g.edges[x].t>g.edges[y].t;});base_pos[u]=p;for(int eid:a){g.edges[eid].pos=p;eid_at_pos[p]=eid;++p;}}base_pos[N]=p;
    }
    std::vector<int> H0()const{
        std::vector<int> h(R,0);for(int u=0;u<N;++u){int p=0;auto &a=out[u];for(int j=(int)Q[u].size()-1;j>=0;--j){Time q=Q[u][j];while(p<(int)a.size()&&g.edges[a[p]].t>=q)++p;h[off[u]+j]=p;}}return h;
    }
    std::pair<std::vector<int>,std::vector<int>> layer(const std::vector<int>&prev)const{
        std::vector<int> h(R),slack(R);for(int u=0;u<N;++u){InsertH ih((int)out[u].size());int p=0;for(int j=(int)Q[u].size()-1;j>=0;--j){Time q=Q[u][j];while(p<(int)out[u].size()&&g.edges[out[u][p]].t>=q){ih.add(prev[g.edges[out[u][p]].child_sid]);++p;}int s=off[u]+j;h[s]=ih.value();slack[s]=ih.slack();}}return {std::move(h),std::move(slack)};
    }
    void print_vertex(const std::vector<int>&h)const{for(int v=0;v<N;++v)std::cout<<g.original_ids[v]<<" "<<h[q0sid[v]]<<"\n";}


    std::ostream* stats = nullptr;
    MeasureClock::time_point round_start;
    void start_round(){round_start=MeasureClock::now();}
    void record(int order, const char* mode, long long A, long long D,
                long long processed=0, long long repairs=0) const {
        if(stats) *stats << order << ',' << mode << ',' << A << ',' << D
                         << ',' << processed << ',' << repairs << ',' << std::setprecision(12)
                         << seconds(round_start,MeasureClock::now()) << '\n';
    }
    // Exact coalescing: query times with an identical eligible edge prefix
    // share all recurrence values, even for nonuniform and zero transitions.
    std::vector<int> solve_otdp_coalesced(int norder){
        const auto degree=H0();
        std::vector<int> cls(R),coff(N+1,0),cdegree,multiplicity,child(m);
        for(int u=0;u<N;++u){
            int last=-1;
            for(int j=0;j<(int)Q[u].size();++j){
                int x=off[u]+j;
                if(last!=degree[x]){cdegree.push_back(degree[x]);multiplicity.push_back(0);last=degree[x];}
                cls[x]=(int)cdegree.size()-1;++multiplicity.back();
            }
            coff[u+1]=(int)cdegree.size();
        }
        effective_states=(int)cdegree.size();
        for(int eid=0;eid<m;++eid)child[eid]=cls[g.edges[eid].child_sid];
        auto h=cdegree;
        for(int r=1;r<=norder;++r){
            start_round();std::vector<int> next(effective_states);long long D=0;
            for(int u=0;u<N;++u){
                InsertH ih((int)out[u].size());int p=0;
                for(int j=coff[u+1]-1;j>=coff[u];--j){
                    while(p<cdegree[j]){ih.add(h[child[out[u][p]]]);++p;}
                    next[j]=ih.value();D+=(long long)multiplicity[j]*(h[j]-next[j]);
                }
            }
            record(r,"otdp_coalesced",-1,D);h=std::move(next);if(D==0)break;
        }
        std::vector<int> expanded(R);
        for(int x=0;x<R;++x)expanded[x]=h[cls[x]];
        return expanded;
    }

    std::vector<int> solve_otdp(int norder){
        auto h=H0();
        for(int r=1;r<=norder;++r){
            start_round();
            auto next=layer(h).first;
            long long D=0;
            for(int x=0;x<R;++x) if(next[x]!=h[x]){
                D+=h[x]-next[x];
            }
            record(r,"otdp",-1,D);
            h=std::move(next);
            if(D==0) break;
        }
        return h;
    }

};

int main(int argc, char** argv) {
    try {
        const std::string usage = std::string("usage: ") + argv[0] +
            " <edge-file> <order> [--stats file.csv] [--metrics file.json]";
        if (argc == 2 && std::string(argv[1]) == "--help") {
            std::cout << usage << '\n';
            return 0;
        }
        if (argc < 3) throw std::runtime_error(usage);
        const int order = order_arg(argv[2]);
        std::string stats_path, metrics_path;
        for (int i = 3; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--stats" && i + 1 < argc) stats_path = argv[++i];
            else if (arg == "--metrics" && i + 1 < argc) metrics_path = argv[++i];
            else throw std::runtime_error("unknown or incomplete option: " + arg);
        }
        if ((!stats_path.empty() && stats_path == argv[1]) ||
            (!metrics_path.empty() && metrics_path == argv[1]) ||
            (!stats_path.empty() && stats_path == metrics_path))
            throw std::runtime_error("input and output paths must differ");
        std::ofstream stats;
        if (!stats_path.empty()) {
            stats.open(stats_path);
            if (!stats) throw std::runtime_error("cannot open stats output");
            stats << "order,mode,A,D,attempt_updates,attempt_repairs,seconds\n";
        }
        const auto start = MeasureClock::now();
        auto graph = TemporalGraph::load(argv[1]);
        const auto loaded = MeasureClock::now();
        Solver solver(std::move(graph));
        const auto prepared = MeasureClock::now();
        if (stats.is_open()) solver.stats = &stats;
#if THOR_USE_COALESCING
        auto answer = solver.solve_otdp_coalesced(order);
#else
        auto answer = solver.solve_otdp(order);
#endif
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
