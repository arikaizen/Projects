#include "request_handler.h"
#include "graph_store.h"
#include "protocol.h"

#include <stdexcept>

RequestHandler::RequestHandler(GraphStore& store)
    : m_store(store) {}

std::string RequestHandler::serializeEntity(const Entity& entity) const {
    std::string result = "{\"name\":\"";
    result += protocol::jsonEscape(entity.name);
    result += "\",\"type\":\"";
    result += protocol::jsonEscape(entity.type);
    result += "\",\"observations\":[";
    for (size_t i = 0; i < entity.observations.size(); ++i) {
        if (i > 0) result += ",";
        result += "\"";
        result += protocol::jsonEscape(entity.observations[i]);
        result += "\"";
    }
    result += "]}";
    return result;
}

std::string RequestHandler::serializeRelation(const Relation& relation) const {
    std::string result = "{\"from\":\"";
    result += protocol::jsonEscape(relation.from);
    result += "\",\"relationType\":\"";
    result += protocol::jsonEscape(relation.relationType);
    result += "\",\"to\":\"";
    result += protocol::jsonEscape(relation.to);
    result += "\"}";
    return result;
}

std::string RequestHandler::serializeEntities(const std::vector<Entity>& entities) const {
    std::string result = "[";
    for (size_t i = 0; i < entities.size(); ++i) {
        if (i > 0) result += ",";
        result += serializeEntity(entities[i]);
    }
    result += "]";
    return result;
}

std::string RequestHandler::serializeRelations(const std::vector<Relation>& relations) const {
    std::string result = "[";
    for (size_t i = 0; i < relations.size(); ++i) {
        if (i > 0) result += ",";
        result += serializeRelation(relations[i]);
    }
    result += "]";
    return result;
}

std::string RequestHandler::serializeGraph(const Graph& graph) const {
    std::string result = "{\"entities\":[";
    bool first = true;
    for (const auto& [name, entity] : graph.entities) {
        if (!first) result += ",";
        result += serializeEntity(entity);
        first = false;
    }
    result += "],\"relations\":";
    result += serializeRelations(graph.relations);
    result += "}";
    return result;
}

std::string RequestHandler::handle(const std::string& rawRequest) {
    try {
        protocol::Request req = protocol::parseRequest(rawRequest);

        if (req.command.empty()) {
            return protocol::formatError("Empty command");
        }

        if (req.command == "CREATE_ENTITY") {
            if (req.args.size() < 2)
                return protocol::formatError("Usage: CREATE_ENTITY name | type");
            m_store.createEntity(req.args[0], req.args[1]);
            return protocol::formatOK("{}");

        } else if (req.command == "ADD_OBS") {
            if (req.args.size() < 2)
                return protocol::formatError("Usage: ADD_OBS entity | observation");
            m_store.addObservation(req.args[0], req.args[1]);
            return protocol::formatOK("{}");

        } else if (req.command == "CREATE_REL") {
            if (req.args.size() < 3)
                return protocol::formatError("Usage: CREATE_REL from | relationType | to");
            m_store.createRelation(req.args[0], req.args[1], req.args[2]);
            return protocol::formatOK("{}");

        } else if (req.command == "DELETE_ENTITY") {
            if (req.args.empty())
                return protocol::formatError("Usage: DELETE_ENTITY name");
            m_store.deleteEntity(req.args[0]);
            return protocol::formatOK("{}");

        } else if (req.command == "DELETE_OBS") {
            if (req.args.size() < 2)
                return protocol::formatError("Usage: DELETE_OBS entity | observation");
            m_store.deleteObservation(req.args[0], req.args[1]);
            return protocol::formatOK("{}");

        } else if (req.command == "DELETE_REL") {
            if (req.args.size() < 3)
                return protocol::formatError("Usage: DELETE_REL from | relationType | to");
            m_store.deleteRelation(req.args[0], req.args[1], req.args[2]);
            return protocol::formatOK("{}");

        } else if (req.command == "SEARCH") {
            if (req.args.empty())
                return protocol::formatError("Usage: SEARCH query");
            auto results = m_store.searchNodes(req.args[0]);
            return protocol::formatOK(serializeEntities(results));

        } else if (req.command == "GET_RELATIONS") {
            if (req.args.empty())
                return protocol::formatError("Usage: GET_RELATIONS entity");
            auto results = m_store.getRelations(req.args[0]);
            return protocol::formatOK(serializeRelations(results));

        } else if (req.command == "READ_GRAPH") {
            return protocol::formatOK(serializeGraph(m_store.readGraph()));

        } else {
            return protocol::formatError("Unknown command: " + req.command);
        }

    } catch (const std::exception& e) {
        return protocol::formatError(e.what());
    }
}
