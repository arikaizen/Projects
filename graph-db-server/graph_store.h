#ifndef GRAPH_STORE_H
#define GRAPH_STORE_H

#include "graph_types.h"
#include <string>
#include <vector>

class Persistence;

class GraphStore {
public:
    GraphStore(Graph graph, Persistence& persistence);

    void createEntity(const std::string& name, const std::string& type);
    void addObservation(const std::string& entity, const std::string& observation);
    void createRelation(const std::string& from, const std::string& rel, const std::string& to);
    void deleteEntity(const std::string& name);
    void deleteObservation(const std::string& entity, const std::string& observation);
    void deleteRelation(const std::string& from, const std::string& rel, const std::string& to);
    std::vector<Entity> searchNodes(const std::string& query) const;
    std::vector<Relation> getRelations(const std::string& entity) const;
    const Graph& readGraph() const;

private:
    Graph m_graph;
    Persistence& m_persistence;
};

#endif // GRAPH_STORE_H
