/**
 * @file request_handler.cpp
 * @brief Implementation of command routing and JSON response serialization.
 *
 * handle() is the single entry point.  It uses protocol::parseRequest() to
 * break the raw line into a command token and an argument list, then a chain
 * of if/else if branches dispatches to the matching GraphStore method.
 *
 * Every branch follows the same structure:
 *   1. Validate argument count; return ERROR if insufficient.
 *   2. Call the GraphStore method (may throw std::runtime_error).
 *   3. Return formatOK() with the appropriate JSON payload.
 *
 * The outer try/catch converts any exception from GraphStore into an ERROR
 * response, so handle() itself never propagates exceptions to the caller.
 */

#include "request_handler.h"
#include "graph_store.h"   // full definition needed to call store methods
#include "protocol.h"      // parseRequest, formatOK, formatError, jsonEscape

#include <stdexcept>       // std::exception

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

RequestHandler::RequestHandler(GraphStore& store)
    : m_store(store) {}

// ---------------------------------------------------------------------------
// Private serialization helpers
// ---------------------------------------------------------------------------

/**
 * Builds the JSON object for one Entity by concatenating key-value pairs.
 * String values are passed through protocol::jsonEscape() so that embedded
 * quotes, backslashes, or control characters do not break the JSON syntax.
 */
std::string RequestHandler::serializeEntity(const Entity& entity) const {
    std::string result = "{\"name\":\"";
    result += protocol::jsonEscape(entity.name);
    result += "\",\"type\":\"";
    result += protocol::jsonEscape(entity.type);
    result += "\",\"observations\":[";

    // Serialize each observation as a quoted, escaped JSON string.
    for (size_t i = 0; i < entity.observations.size(); ++i) {
        if (i > 0) result += ","; // comma between elements, not after the last one
        result += "\"";
        result += protocol::jsonEscape(entity.observations[i]);
        result += "\"";
    }

    result += "]}"; // close observations array and the entity object
    return result;
}

/**
 * Builds the JSON object for one Relation.  All three fields (from,
 * relationType, to) are JSON-escaped because they may contain arbitrary
 * user-supplied text.
 */
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

/** Wraps a list of entity objects in a JSON array. */
std::string RequestHandler::serializeEntities(const std::vector<Entity>& entities) const {
    std::string result = "[";
    for (size_t i = 0; i < entities.size(); ++i) {
        if (i > 0) result += ",";
        result += serializeEntity(entities[i]);
    }
    result += "]";
    return result;
}

/** Wraps a list of relation objects in a JSON array. */
std::string RequestHandler::serializeRelations(const std::vector<Relation>& relations) const {
    std::string result = "[";
    for (size_t i = 0; i < relations.size(); ++i) {
        if (i > 0) result += ",";
        result += serializeRelation(relations[i]);
    }
    result += "]";
    return result;
}

/**
 * Serializes the complete graph as a top-level JSON object.
 * Because Graph::entities is an unordered_map, the order of entities in the
 * output array is iteration order (unspecified, varies by hash implementation).
 * Relations are emitted in their insertion order (vector iteration order).
 */
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

// ---------------------------------------------------------------------------
// Public dispatch method
// ---------------------------------------------------------------------------

/**
 * The outer try/catch is the last line of defence: if GraphStore throws for
 * any reason not already caught by argument validation (e.g. "Entity not
 * found"), the exception message becomes the ERROR payload so the server
 * continues running and the client gets a meaningful reply.
 */
std::string RequestHandler::handle(const std::string& rawRequest) {
    try {
        protocol::Request req = protocol::parseRequest(rawRequest);

        // An empty command means the client sent a blank line — ignore silently.
        if (req.command.empty()) {
            return protocol::formatError("Empty command");
        }

        // ------------------------------------------------------------------
        // CREATE_ENTITY name | type
        // Creates a new entity node.  Returns "{}" on success.
        // ------------------------------------------------------------------
        if (req.command == "CREATE_ENTITY") {
            if (req.args.size() < 2)
                return protocol::formatError("Usage: CREATE_ENTITY name | type");
            m_store.createEntity(req.args[0], req.args[1]);
            return protocol::formatOK("{}");

        // ------------------------------------------------------------------
        // ADD_OBS entity | observation
        // Appends a free-text fact to an existing entity.
        // ------------------------------------------------------------------
        } else if (req.command == "ADD_OBS") {
            if (req.args.size() < 2)
                return protocol::formatError("Usage: ADD_OBS entity | observation");
            m_store.addObservation(req.args[0], req.args[1]);
            return protocol::formatOK("{}");

        // ------------------------------------------------------------------
        // CREATE_REL from | relationType | to
        // Creates a directed edge between two existing entities.
        // ------------------------------------------------------------------
        } else if (req.command == "CREATE_REL") {
            if (req.args.size() < 3)
                return protocol::formatError("Usage: CREATE_REL from | relationType | to");
            m_store.createRelation(req.args[0], req.args[1], req.args[2]);
            return protocol::formatOK("{}");

        // ------------------------------------------------------------------
        // DELETE_ENTITY name
        // Removes the entity and cascades to all connected relations.
        // ------------------------------------------------------------------
        } else if (req.command == "DELETE_ENTITY") {
            if (req.args.empty())
                return protocol::formatError("Usage: DELETE_ENTITY name");
            m_store.deleteEntity(req.args[0]);
            return protocol::formatOK("{}");

        // ------------------------------------------------------------------
        // DELETE_OBS entity | observation
        // Removes the first matching observation from the entity.
        // ------------------------------------------------------------------
        } else if (req.command == "DELETE_OBS") {
            if (req.args.size() < 2)
                return protocol::formatError("Usage: DELETE_OBS entity | observation");
            m_store.deleteObservation(req.args[0], req.args[1]);
            return protocol::formatOK("{}");

        // ------------------------------------------------------------------
        // DELETE_REL from | relationType | to
        // Removes the exact directed edge identified by the triple.
        // ------------------------------------------------------------------
        } else if (req.command == "DELETE_REL") {
            if (req.args.size() < 3)
                return protocol::formatError("Usage: DELETE_REL from | relationType | to");
            m_store.deleteRelation(req.args[0], req.args[1], req.args[2]);
            return protocol::formatOK("{}");

        // ------------------------------------------------------------------
        // SEARCH query
        // Returns a JSON array of entities matching the query (case-insensitive
        // substring match across name, type, and observations).
        // ------------------------------------------------------------------
        } else if (req.command == "SEARCH") {
            if (req.args.empty())
                return protocol::formatError("Usage: SEARCH query");
            auto results = m_store.searchNodes(req.args[0]);
            return protocol::formatOK(serializeEntities(results));

        // ------------------------------------------------------------------
        // GET_RELATIONS entity
        // Returns a JSON array of all edges where the entity appears as
        // source or target.
        // ------------------------------------------------------------------
        } else if (req.command == "GET_RELATIONS") {
            if (req.args.empty())
                return protocol::formatError("Usage: GET_RELATIONS entity");
            auto results = m_store.getRelations(req.args[0]);
            return protocol::formatOK(serializeRelations(results));

        // ------------------------------------------------------------------
        // READ_GRAPH
        // Returns the entire graph as a JSON object with "entities" and
        // "relations" arrays.
        // ------------------------------------------------------------------
        } else if (req.command == "READ_GRAPH") {
            return protocol::formatOK(serializeGraph(m_store.readGraph()));

        // ------------------------------------------------------------------
        // Unknown command
        // ------------------------------------------------------------------
        } else {
            return protocol::formatError("Unknown command: " + req.command);
        }

    } catch (const std::exception& e) {
        // GraphStore threw (e.g. entity not found, duplicate, etc.).
        // Convert the exception message to a protocol-level error response.
        return protocol::formatError(e.what());
    }
}
