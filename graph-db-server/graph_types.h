#ifndef GRAPH_TYPES_H
#define GRAPH_TYPES_H

#include <string>
#include <vector>
#include <unordered_map>

struct Entity {
    std::string name;
    std::string type;
    std::vector<std::string> observations;
};

struct Relation {
    std::string from;
    std::string relationType;
    std::string to;
};

struct Graph {
    std::unordered_map<std::string, Entity> entities;
    std::vector<Relation> relations;
};

#endif // GRAPH_TYPES_H
