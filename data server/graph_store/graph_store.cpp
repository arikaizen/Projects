/**
 * @file graph_store.cpp
 * @brief Implementation of the in-memory graph CRUD engine.
 *
 * Every mutating method follows the same three-step pattern:
 *   1. Validate — throw std::runtime_error for any violated precondition.
 *   2. Mutate   — modify m_graph in place.
 *   3. Persist  — call m_persistence.save(m_graph) to write through to disk.
 *
 * The save() call is always the last step so that a persistence failure leaves
 * the in-memory graph unchanged relative to the on-disk state.  (If save()
 * throws, the mutation has already happened in memory; a more robust design
 * would roll back first, but for this server the simple approach is fine.)
 */

#include "graph_store.h"
#include "persistence.h"   // full definition needed to call save()

#include <algorithm>       // std::remove_if, std::find, std::transform
#include <stdexcept>       // std::runtime_error

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

/**
 * Takes ownership of the graph via move to avoid an expensive deep copy of
 * potentially thousands of entities and observations.  The Persistence reference
 * is stored as-is; it must outlive this object.
 */
GraphStore::GraphStore(Graph graph, Persistence& persistence)
    : m_graph(std::move(graph)), m_persistence(persistence) {}

// ---------------------------------------------------------------------------
// Mutation methods
// ---------------------------------------------------------------------------

void GraphStore::createEntity(const std::string& name, const std::string& type) {
    // Reject duplicates — entity names are the primary key of the graph.
    if (m_graph.entities.count(name)) {
        throw std::runtime_error("Entity already exists: " + name);
    }

    Entity entity;
    entity.name = name;
    entity.type = type;
    // observations starts empty; callers use addObservation to populate it.
    m_graph.entities[name] = std::move(entity);
    m_persistence.save(m_graph);
}

void GraphStore::addObservation(const std::string& entity, const std::string& observation) {
    auto it = m_graph.entities.find(entity);
    if (it == m_graph.entities.end()) {
        throw std::runtime_error("Entity not found: " + entity);
    }
    it->second.observations.push_back(observation);
    m_persistence.save(m_graph);
}

void GraphStore::createRelation(const std::string& from, const std::string& rel,
                                const std::string& to) {
    // Both endpoints must exist before the edge is created — this enforces
    // referential integrity at insertion time.
    if (!m_graph.entities.count(from)) {
        throw std::runtime_error("Source entity not found: " + from);
    }
    if (!m_graph.entities.count(to)) {
        throw std::runtime_error("Target entity not found: " + to);
    }

    // Linear scan to detect duplicates.  For graphs with a very large number
    // of relations, an index keyed on (from, relationType, to) would be faster,
    // but is unnecessary at this scale.
    for (const auto& r : m_graph.relations) {
        if (r.from == from && r.relationType == rel && r.to == to) {
            throw std::runtime_error("Relation already exists");
        }
    }

    m_graph.relations.push_back({from, rel, to});
    m_persistence.save(m_graph);
}

void GraphStore::deleteEntity(const std::string& name) {
    if (!m_graph.entities.count(name)) {
        throw std::runtime_error("Entity not found: " + name);
    }

    // Remove the entity node itself.
    m_graph.entities.erase(name);

    // Cascade: remove every relation that has this entity on either end.
    // The erase-remove idiom is used because std::vector does not support
    // in-place removal of elements matching a predicate directly.
    auto& rels = m_graph.relations;
    rels.erase(
        std::remove_if(rels.begin(), rels.end(),
            [&](const Relation& r) { return r.from == name || r.to == name; }),
        rels.end()
    );

    m_persistence.save(m_graph);
}

void GraphStore::deleteObservation(const std::string& entity,
                                   const std::string& observation) {
    auto it = m_graph.entities.find(entity);
    if (it == m_graph.entities.end()) {
        throw std::runtime_error("Entity not found: " + entity);
    }

    auto& obs = it->second.observations;
    // std::find gives us an iterator to the first matching element.
    auto obsIt = std::find(obs.begin(), obs.end(), observation);
    if (obsIt == obs.end()) {
        throw std::runtime_error("Observation not found on entity: " + entity);
    }

    // erase() on a vector shifts subsequent elements left — O(n) but fine here.
    obs.erase(obsIt);
    m_persistence.save(m_graph);
}

void GraphStore::deleteRelation(const std::string& from, const std::string& rel,
                                const std::string& to) {
    auto& rels = m_graph.relations;

    // std::remove_if reorders the vector so that non-matching elements come
    // first, then returns an iterator to the start of the "removed" tail.
    // If the iterator equals rels.end(), nothing matched — that is an error.
    auto it = std::remove_if(rels.begin(), rels.end(),
        [&](const Relation& r) {
            return r.from == from && r.relationType == rel && r.to == to;
        });

    if (it == rels.end()) {
        throw std::runtime_error("Relation not found");
    }

    // Truncate the vector at the new logical end to physically remove elements.
    rels.erase(it, rels.end());
    m_persistence.save(m_graph);
}

// ---------------------------------------------------------------------------
// Query methods
// ---------------------------------------------------------------------------

std::vector<Entity> GraphStore::searchNodes(const std::string& query) const {
    std::vector<Entity> results;

    // Lower-case the query once so we don't repeat the conversion inside the loop.
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    for (const auto& [name, entity] : m_graph.entities) {
        // Lambda: lower-cases its argument and checks for the query substring.
        auto containsQuery = [&](const std::string& s) {
            std::string lower = s;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            return lower.find(lowerQuery) != std::string::npos;
        };

        // An entity matches if the query appears in its name or type label.
        if (containsQuery(entity.name) || containsQuery(entity.type)) {
            results.push_back(entity);
            continue; // already matched — no need to check observations
        }

        // Also check each observation; stop at the first hit to avoid duplicates.
        for (const auto& obs : entity.observations) {
            if (containsQuery(obs)) {
                results.push_back(entity);
                break;
            }
        }
    }
    return results;
}

std::vector<Relation> GraphStore::getRelations(const std::string& entity) const {
    std::vector<Relation> results;

    // Include both outgoing (entity is source) and incoming (entity is target)
    // edges so callers get a complete picture of the entity's neighbourhood.
    for (const auto& rel : m_graph.relations) {
        if (rel.from == entity || rel.to == entity) {
            results.push_back(rel);
        }
    }
    return results;
}

const Graph& GraphStore::readGraph() const {
    // Returns a const reference to avoid copying the entire graph.
    // The reference is invalidated by any subsequent mutating call.
    return m_graph;
}
