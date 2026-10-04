#pragma once
#include <array>

namespace cineol::xl {
enum class Graph {
#define CINEOL_XL_GRAPH(name,title,rows,bank,program) name,
#include "graph_list.inc"
#undef CINEOL_XL_GRAPH
};
struct GraphInfo {const char* name;unsigned rows,bank,program;};
inline constexpr std::array<GraphInfo,22> graphs={{
#define CINEOL_XL_GRAPH(name,title,rows,bank,program) {title,rows,bank,program},
#include "graph_list.inc"
#undef CINEOL_XL_GRAPH
}};
inline constexpr GraphInfo graph_info(Graph graph) noexcept {return graphs[unsigned(graph)];}
template<class F> void each_graph(F&& f) {
#define CINEOL_XL_GRAPH(name,title,rows,bank,program) f.template operator()<Graph::name>();
#include "graph_list.inc"
#undef CINEOL_XL_GRAPH
}
}
