#pragma once

// Construct relevant-time classes and map each temporal edge to its child class.
// Original query lists are released at the end of preparation.
struct CoalescedSolver {
    int N{}, m{}, R{}, effective_states{};
    std::vector<long long> original_ids;
    std::vector<std::vector<Time>> queries;
    std::vector<int> coff, degree, multiplicity, base, child, root;
    std::vector<Time> starts;
    std::ostream* stats = nullptr;
    std::ostream* work_stats = nullptr;
    long long total_scanned=0, total_saturated_edges=0, total_skipped_vertices=0;
    long long active_rounds=0, dependency_visits=0;
    bool dependency_built=false;

    explicit CoalescedSolver(TemporalGraph graph) {
        N=graph.n; m=static_cast<int>(graph.edges.size());
        original_ids=std::move(graph.original_ids);
        queries.resize(N); base.assign(N+1,0); root.resize(N);
        for(int u=0;u<N;++u) queries[u].push_back(0);
        for(const auto& e:graph.edges){
            queries[e.v].push_back(e.t+e.lambda); ++base[e.u+1];
        }
        std::partial_sum(base.begin(),base.end(),base.begin());
        std::vector<int> edges(m),cursor=base;
        for(int e=0;e<m;++e) edges[cursor[graph.edges[e].u]++]=e;
        coff.assign(N+1,0);
        for(int u=0;u<N;++u){
            std::sort(edges.begin()+base[u],edges.begin()+base[u+1],
                [&](int a,int b){return graph.edges[a].t>graph.edges[b].t;});
            auto& q=queries[u]; std::sort(q.begin(),q.end());
            q.erase(std::unique(q.begin(),q.end()),q.end()); R+=static_cast<int>(q.size());
            int count=base[u+1]-base[u],last=-1;
            for(Time t:q){
                while(count>0 && graph.edges[edges[base[u]+count-1]].t<t) --count;
                if(count!=last){
                    starts.push_back(t); degree.push_back(count);
                    multiplicity.push_back(0); last=count;
                }
                const int c=static_cast<int>(degree.size())-1;
                ++multiplicity[c]; if(t==0) root[u]=c;
            }
            coff[u+1]=static_cast<int>(degree.size());
        }
        effective_states=static_cast<int>(degree.size());
        // Replace the edge permutation in place by its compressed child IDs.
        for(int& eid:edges){
            const auto& e=graph.edges[eid];
            eid=static_cast<int>(std::upper_bound(starts.begin()+coff[e.v],
                starts.begin()+coff[e.v+1],e.t+e.lambda)-starts.begin())-1;
        }
        child=std::move(edges);
        std::vector<std::vector<Time>>().swap(queries);
        // graph and all temporary raw-state storage die at the prep boundary.
    }

    void print_vertex(const std::vector<int>& h) const {
        for(int u=0;u<N;++u) std::cout<<original_ids[u]<<' '<<h[root[u]]<<'\n';
    }
    std::vector<int> solve(int norder){
        auto h=degree, next=h;
        std::vector<int> freq, changed, all(N), active, next_active;
        std::iota(all.begin(),all.end(),0); active=all;
        std::vector<int> reverse_off, reverse_source, seen;
        long long full_work=effective_states;
        for(int u=0;u<N;++u) full_work+=degree[coff[u]];
        auto build_dependencies=[&](){
            reverse_off.assign(effective_states+1,0);
            for(int u=0;u<N;++u) for(int p=base[u];p<base[u]+degree[coff[u]];++p)
                ++reverse_off[child[p]+1];
            std::partial_sum(reverse_off.begin(),reverse_off.end(),reverse_off.begin());
            reverse_source.resize(reverse_off.back()); auto cursor=reverse_off;
            for(int u=0;u<N;++u) for(int p=base[u];p<base[u]+degree[coff[u]];++p)
                reverse_source[cursor[child[p]]++]=u;
            seen.assign(N,0); dependency_built=true;
        };
        for(int r=1;r<=norder;++r){
            const auto began=MeasureClock::now(); long long D=0,scanned=0,saturated=0,visits=0;
            const int processed_vertices=static_cast<int>(active.size());
            if(processed_vertices<N) ++active_rounds;
            total_skipped_vertices+=N-processed_vertices; changed.clear();
            size_t changed_count=0;
            for(int u:active){
                const int cap=h[coff[u]];
                if(cap==0){
                    // An all-zero slice remains zero in both buffers once
                    // the older layer has caught up; avoid clearing it again.
                    if(next[coff[u]]!=0)
                        std::fill(next.begin()+coff[u],next.begin()+coff[u+1],0);
                    continue;
                }
                freq.assign(cap+1,0); int p=base[u], hv=0,cnt=0;
                for(int j=coff[u+1]-1;j>=coff[u];--j){
                    const int end=base[u]+degree[j];
                    while(p<end){
                        const int x=std::min(h[child[p++]],cap); ++freq[x];
                        if(x>hv) ++cnt;
                        if(cnt>hv){
                            ++hv;cnt-=freq[hv];
                            if(hv==cap) break;
                        }
                    }
                    next[j]=hv;
                    const int delta=h[j]-hv; D+=(long long)multiplicity[j]*delta;
                    changed_count+=delta!=0;
                    if(hv==cap){
                        for(--j;j>=coff[u];--j){next[j]=cap;}
                        break;
                    }
                }
                scanned+=p-base[u]; saturated+=degree[coff[u]]-(p-base[u]);
            }
            const bool track_changes=D!=0 && r<norder &&
                (dependency_built ||
                 (norder-r>=4 && changed_count*8u<static_cast<size_t>(effective_states)));
            // Keep change-list bookkeeping out of the insertion hot loop.
            // When changes are broad and no reverse index exists, no list is
            // needed. Otherwise scan only the just-computed vertex slices.
            if(track_changes)for(int u:active)for(int c=coff[u];c<coff[u+1];++c)
                if(h[c]!=next[c])changed.push_back(c);
            h.swap(next);
            total_scanned+=scanned; total_saturated_edges+=saturated;
            if(D!=0 && r<norder){
                if(!dependency_built && track_changes &&
                    changed_count*8u<static_cast<size_t>(effective_states))
                    build_dependencies();
                if(dependency_built){
                    long long links=0;
                    for(int c:changed) links+=reverse_off[c+1]-reverse_off[c];
                    if(links*2<full_work){
                        next_active.clear(); long long work=0; bool dense=false;
                        for(int c:changed){
                            for(int k=reverse_off[c];k<reverse_off[c+1];++k){
                                ++visits; const int u=reverse_source[k];
                                if(seen[u]==r || h[coff[u]]==0) continue;
                                seen[u]=r; next_active.push_back(u);
                                work+=degree[coff[u]]+coff[u+1]-coff[u];
                            }
                            if(work*2>full_work){dense=true;break;}
                        }
                        if(dense) active=all; else active.swap(next_active);
                    }else active=all;
                }else active=all;
            }
            // A full sweep overwrites all states. Only sparse rounds need the
            // two buffers synchronized, and only the just-changed slots differ.
            if(D!=0 && r<norder && active.size()<static_cast<size_t>(N))
                for(int c:changed) next[c]=h[c];
            dependency_visits+=visits;
            if(stats) *stats<<r<<",otdp_coalesced,-1,"<<D<<",0,0,"
                <<std::setprecision(12)<<seconds(began,MeasureClock::now())<<'\n';
            if(work_stats) *work_stats<<r<<','<<processed_vertices<<','<<scanned<<','
                <<saturated<<','<<changed_count<<','<<visits<<','<<dependency_built<<'\n';
            if(D==0) break;
        }
        return h;
    }
};
