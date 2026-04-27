#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H

#include "graph_types.h"
#include <string>
#include <vector>

class GraphStore;

class RequestHandler {
public:
    explicit RequestHandler(GraphStore& store);
    std::string handle(const std::string& rawRequest);

private:
    GraphStore& m_store;

    std::string serializeEntity(const Entity& entity) const;
    std::string serializeRelation(const Relation& relation) const;
    std::string serializeEntities(const std::vector<Entity>& entities) const;
    std::string serializeRelations(const std::vector<Relation>& relations) const;
    std::string serializeGraph(const Graph& graph) const;
};

#endif // REQUEST_HANDLER_H
