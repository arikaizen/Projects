/**
 * @file request_handler.h
 * @brief Command router that bridges the protocol layer and the graph store.
 *
 * RequestHandler is the glue between the network and the data layer.  It has
 * one public method — handle() — that receives a raw text line from the server,
 * delegates parsing to protocol::parseRequest(), dispatches to the appropriate
 * GraphStore method, and returns a formatted response line ready to be sent back
 * over the socket.
 *
 * Serialization
 * -------------
 * Query results (entities, relations, the full graph) must be turned into JSON
 * before being embedded in an OK response.  The private serialize* methods
 * handle this, using protocol::jsonEscape() for safe string encoding.
 *
 * JSON output shapes:
 *   Entity:   {"name":"...","type":"...","observations":["...",...]}
 *   Relation: {"from":"...","relationType":"...","to":"..."}
 *   Graph:    {"entities":[<entity>,...], "relations":[<relation>,...]}
 *
 * Error handling
 * --------------
 * handle() wraps the entire dispatch in a try/catch.  Any std::exception thrown
 * by GraphStore (e.g. "Entity not found") is caught and converted into an ERROR
 * response so the server never crashes on a bad client request.
 */

#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H

#include "graph_types.h"
#include <string>
#include <vector>

// Forward declaration — the full GraphStore definition is only needed in the .cpp.
class GraphStore;

/**
 * @brief Parses raw client request lines and dispatches them to GraphStore.
 */
class RequestHandler {
public:
    /**
     * @brief Constructs the handler bound to a GraphStore.
     *
     * @param store  The graph engine to read from and mutate.  Must remain
     *               valid for the entire lifetime of this RequestHandler.
     */
    explicit RequestHandler(GraphStore& store);

    /**
     * @brief Processes one raw request line and returns a complete response line.
     *
     * The raw line is expected to already have its trailing '\n' (and optional
     * '\r') stripped by the server.  This method never throws — all exceptions
     * from GraphStore are caught internally and turned into ERROR responses.
     *
     * @param rawRequest  A single trimmed request line, e.g. "SEARCH alice".
     * @return            A response line ending in '\n', e.g. "OK [...]\n"
     *                    or "ERROR Entity not found: Alice\n".
     */
    std::string handle(const std::string& rawRequest);

private:
    /** The graph engine this handler dispatches to. */
    GraphStore& m_store;

    /**
     * @brief Serializes a single Entity to a JSON object string.
     *
     * @param entity  The entity to serialize.
     * @return        JSON object, e.g. {"name":"Alice","type":"person","observations":["Speaks Spanish"]}
     */
    std::string serializeEntity(const Entity& entity) const;

    /**
     * @brief Serializes a single Relation to a JSON object string.
     *
     * @param relation  The relation to serialize.
     * @return          JSON object, e.g. {"from":"Alice","relationType":"works_at","to":"Acme"}
     */
    std::string serializeRelation(const Relation& relation) const;

    /**
     * @brief Serializes a vector of Entity objects to a JSON array string.
     *
     * @param entities  The entities to serialize.
     * @return          JSON array, e.g. [{...},{...}]; "[]" if empty.
     */
    std::string serializeEntities(const std::vector<Entity>& entities) const;

    /**
     * @brief Serializes a vector of Relation objects to a JSON array string.
     *
     * @param relations  The relations to serialize.
     * @return           JSON array, e.g. [{...},{...}]; "[]" if empty.
     */
    std::string serializeRelations(const std::vector<Relation>& relations) const;

    /**
     * @brief Serializes an entire Graph to a JSON object string.
     *
     * Produces a top-level object with two keys:
     *   "entities"  — JSON array of all entity objects
     *   "relations" — JSON array of all relation objects
     *
     * @param graph  The graph to serialize (typically from GraphStore::readGraph()).
     * @return       JSON object containing both entities and relations.
     */
    std::string serializeGraph(const Graph& graph) const;
};

#endif // REQUEST_HANDLER_H
