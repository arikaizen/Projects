/**
 * @file graph_store.h
 * @brief In-memory graph engine with write-through persistence.
 *
 * GraphStore is the single authoritative source of truth for the live graph.
 * It owns one Graph value and exposes a typed CRUD API.  Every method that
 * mutates the graph immediately calls Persistence::save() before returning, so
 * the on-disk file is always consistent with the in-memory state.
 *
 * Error handling
 * --------------
 * All methods that can fail (entity not found, duplicate entity, etc.) throw
 * std::runtime_error.  Callers (typically RequestHandler) are expected to catch
 * these and convert them into protocol-level ERROR responses.
 *
 * Thread safety
 * -------------
 * GraphStore is NOT thread-safe.  The server uses a single-threaded select()
 * event loop, so concurrent access from multiple goroutines is not a concern.
 * If multi-threaded access were ever needed, a mutex around every public method
 * would be required.
 *
 * Ownership model
 * ---------------
 * GraphStore takes ownership of the Graph passed at construction (move) and
 * holds a non-owning reference to Persistence.  The Persistence object must
 * outlive the GraphStore.
 */

#ifndef GRAPH_STORE_H
#define GRAPH_STORE_H

#include "graph_types.h"
#include <string>
#include <vector>

// Forward declaration — the full Persistence definition is only needed in
// graph_store.cpp where methods are called on it.
class Persistence;

/**
 * @brief Manages the in-memory graph and synchronizes every change to disk.
 *
 * All nine operations mirror the nine commands understood by RequestHandler.
 * Mutation methods follow the same pattern:
 *   1. Validate preconditions (throw on failure).
 *   2. Mutate m_graph.
 *   3. Call m_persistence.save(m_graph).
 */
class GraphStore {
public:
    /**
     * @brief Constructs a GraphStore from an already-loaded Graph.
     *
     * The graph is moved in (not copied) to avoid an unnecessary deep copy of
     * what could be a large entity/relation set.  Typically called from main()
     * immediately after Persistence::load().
     *
     * @param graph       The initial graph state, usually restored from disk.
     * @param persistence A reference to the persistence layer.  Must remain
     *                    valid for the entire lifetime of this GraphStore.
     */
    GraphStore(Graph graph, Persistence& persistence);

    /**
     * @brief Creates a new entity with an empty observations list.
     *
     * @param name  Unique name for the entity; used as the primary key.
     * @param type  Category label (e.g. "person", "company").
     * @throws std::runtime_error if an entity with the same name already exists.
     */
    void createEntity(const std::string& name, const std::string& type);

    /**
     * @brief Appends a free-text observation to an existing entity.
     *
     * Duplicate observations are allowed; no deduplication is performed.
     *
     * @param entity      Name of the entity to annotate.
     * @param observation The fact to record (e.g. "Speaks Spanish").
     * @throws std::runtime_error if the entity does not exist.
     */
    void addObservation(const std::string& entity, const std::string& observation);

    /**
     * @brief Creates a directed, labelled edge between two existing entities.
     *
     * Both the source and target entities must already exist.  The exact triple
     * (from, rel, to) must be unique — creating the same relation twice is an error.
     *
     * @param from  Name of the source entity.
     * @param rel   Relationship label (e.g. "works_at").
     * @param to    Name of the target entity.
     * @throws std::runtime_error if either entity is missing, or if the identical
     *         relation already exists.
     */
    void createRelation(const std::string& from, const std::string& rel, const std::string& to);

    /**
     * @brief Deletes an entity and all relations that reference it (cascade delete).
     *
     * The cascade ensures referential integrity: no relation can be left pointing
     * at a non-existent entity after this call returns.  Both outgoing
     * (from == name) and incoming (to == name) relations are removed.
     *
     * @param name  Name of the entity to delete.
     * @throws std::runtime_error if the entity does not exist.
     */
    void deleteEntity(const std::string& name);

    /**
     * @brief Removes the first occurrence of a specific observation from an entity.
     *
     * Only the first matching string is removed.  If the same observation appears
     * multiple times, subsequent occurrences remain.
     *
     * @param entity      Name of the entity that owns the observation.
     * @param observation Exact text of the observation to remove.
     * @throws std::runtime_error if the entity does not exist, or if the observation
     *         is not found on that entity.
     */
    void deleteObservation(const std::string& entity, const std::string& observation);

    /**
     * @brief Removes the directed relation identified by the exact triple (from, rel, to).
     *
     * @param from  Source entity name.
     * @param rel   Relationship label.
     * @param to    Target entity name.
     * @throws std::runtime_error if no matching relation exists.
     */
    void deleteRelation(const std::string& from, const std::string& rel, const std::string& to);

    /**
     * @brief Returns all entities whose name, type, or any observation contains
     *        the query string (case-insensitive substring match).
     *
     * An entity is included in the results if the query appears in any one of
     * its three searchable fields: name, type, or observations.  The search
     * stops at the first matching field — an entity is never returned twice.
     *
     * @param query  The substring to search for (compared case-insensitively).
     * @return       A vector of matching Entity copies (order is unspecified).
     */
    std::vector<Entity> searchNodes(const std::string& query) const;

    /**
     * @brief Returns all relations where the given entity appears as source or target.
     *
     * This covers both outgoing edges (entity is the "from" side) and incoming
     * edges (entity is the "to" side), providing a full neighbourhood view.
     *
     * @param entity  Name of the entity whose relations to retrieve.
     * @return        A vector of matching Relation copies; empty if none exist.
     *                No error is thrown if the entity itself does not exist.
     */
    std::vector<Relation> getRelations(const std::string& entity) const;

    /**
     * @brief Returns a const reference to the complete in-memory graph.
     *
     * The reference is valid until the next mutating call.  Callers must not
     * store the reference beyond the current call stack frame.
     *
     * @return Const reference to the live Graph owned by this store.
     */
    const Graph& readGraph() const;

private:
    /** The live, authoritative graph data owned by this store. */
    Graph m_graph;

    /**
     * Reference to the persistence layer.  Stored as a reference (not a pointer
     * or value) because Persistence is non-copyable and must always be valid.
     */
    Persistence& m_persistence;
};

#endif // GRAPH_STORE_H
