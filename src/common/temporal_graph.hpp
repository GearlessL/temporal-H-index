#pragma once
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace thi {
using Time = long long;
inline long long integer(const std::string& s) {
    long long x{};
    auto r = std::from_chars(s.data(), s.data()+s.size(), x);
    if (r.ec != std::errc{} || r.ptr != s.data()+s.size())
        throw std::runtime_error("invalid integer: " + s);
    return x;
}
inline int order_arg(const std::string& s) {
    auto x = integer(s);
    if (x < 0 || x >= std::numeric_limits<int>::max())
        throw std::runtime_error("order must be in [0, INT_MAX-1]");
    return static_cast<int>(x);
}
struct Edge {
    int u{}, v{};
    Time t{}, lambda{1};
    int child_sid{-1};
    int pos{-1};
};
struct TemporalGraph {
    int n{};
    std::vector<long long> original_ids;
    std::vector<Edge> edges;
    static TemporalGraph load(const std::string& path) {
        std::ifstream fin(path);
        if (!fin) throw std::runtime_error("cannot open input: " + path);
        TemporalGraph g;
        std::unordered_map<long long,int> ids;
        auto getid = [&](long long x) {
            auto it = ids.find(x);
            if (it != ids.end()) return it->second;
            if (ids.size() >= static_cast<size_t>(std::numeric_limits<int>::max()/8))
                throw std::runtime_error("too many vertices for compact 32-bit indices");
            int id = static_cast<int>(ids.size());
            ids.emplace(x,id); g.original_ids.push_back(x); return id;
        };
        std::string line;
        size_t line_no = 0;
        while (std::getline(fin,line)) {
            ++line_no;
            auto comment = line.find_first_of("#%");
            if (comment != std::string::npos) line.resize(comment);
            std::istringstream iss(line);
            std::vector<std::string> fields;
            for (std::string f; iss >> f;) fields.push_back(f);
            if (fields.empty()) continue;
            try {
                if (fields[0] == "@vertex") {
                    if (fields.size() != 2) throw std::runtime_error("expected @vertex id");
                    getid(integer(fields[1])); continue;
                }
                if (fields.size() != 3 && fields.size() != 4)
                    throw std::runtime_error("expected u v departure [transition]");
                auto u = integer(fields[0]), v = integer(fields[1]);
                Time t = integer(fields[2]), l = fields.size()==4 ? integer(fields[3]) : 1;
                if (l < 0) throw std::runtime_error("transition time must be nonnegative");
                if (t > std::numeric_limits<Time>::max()-l)
                    throw std::runtime_error("arrival time overflows signed 64-bit integer");
                if (g.edges.size()+ids.size()+2 >= static_cast<size_t>(std::numeric_limits<int>::max()/8))
                    throw std::runtime_error("graph too large for compact 32-bit indices");
                int du=getid(u), dv=getid(v);
                g.edges.push_back({du,dv,t,l,-1,-1});
            } catch (const std::exception& e) {
                throw std::runtime_error(path+":"+std::to_string(line_no)+": "+e.what());
            }
        }
        if (fin.bad()) throw std::runtime_error("input read failed: "+path);
        g.n=static_cast<int>(ids.size());
        return g;
    }
};
inline uint32_t h_index_static(const std::vector<uint32_t>& vals) {
    const uint32_t n=static_cast<uint32_t>(vals.size());
    std::vector<uint32_t> bucket(n+1,0);
    for(uint32_t x:vals) ++bucket[std::min(x,n)];
    uint32_t s=0;
    for(uint32_t k=n;;--k){s+=bucket[k];if(s>=k)return k;}
}
} // namespace thi
