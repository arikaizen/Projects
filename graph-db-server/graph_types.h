/**
 * @file graph_types.h
 * @brief Core data structures shared across all components of the graph database.
 *
 * This file defines the three fundamental types that the entire system is built on:
 *   - Entity  : a named node in the graph with a type label and free-text observations
 *   - Relation: a directed, labelled edge connecting two entities
 *   - Graph   : the top-level container that owns all entities and relations
 *
 * These are plain aggregates (no methods, no invariants) so that every layer —
 * GraphStore, Persistence, RequestHandler — can operate on them directly without
 * coupling to a specific class hierarchy.
 */

#ifndef GRAPH_TYPES_H
#define GRAPH_TYPES_H

#include <string>
#include <vector>
#include <unordered_map>

/**
 * @brief A node in the knowledge graph.
 *
 * An Entity represents a distinct concept, object, or person that the graph
 * tracks.  Each entity has:
 *   - A unique name that serves as its primary key inside Graph::entities.
 *   - A type label (e.g. "person", "company") used for classification and search.
 *   - An ordered list of free-text observations — facts known about this entity.
 *
 * Observations are stored in insertion order and may be duplicate; callers are
 * responsible for deduplication if that property is desired.
 */
struct Entity {
    /** Unique identifier for this entity; used as the key in Graph::entities. */
    std::string name;

    /** Category label describing what kind of thing this entity is (e.g. "person"). */
    std::string type;

    /**
     * Ordered list of free-text facts about this entity.
     * Examples: "Speaks Spanish", "Founded in 2002", "Located in San Francisco".
     */
    std::vector<std::string> observations;
};

/**
 * @brief A directed, labelled edge between two entities.
 *
 * A Relation records that one entity stands in a specific relationship to another.
 * The triple (from, relationType, to) together forms a unique key: the same
 * directed relationship between the same pair of entities may not appear twice.
 *
 * Example: { from="Alice", relationType="works_at", to="Acme" }
 */
struct Relation {
    /** Name of the source entity (must exist in Graph::entities). */
    std::string from;

    /** Label describing the nature of the relationship (e.g. "works_at", "knows"). */
    std::string relationType;

    /** Name of the target entity (must exist in Graph::entities). */
    std::string to;
};

/**
 * @brief The top-level in-memory graph container.
 *
 * Graph owns all entities and relations.  It is the single source of truth that
 * GraphStore mutates and Persistence serializes to disk.
 *
 * Design notes:
 *   - entities uses an unordered_map keyed by entity name for O(1) lookup,
 *     insert, and delete.  Iteration order is unspecified.
 *   - relations uses a plain vector because relations are always scanned
 *     linearly (by from/to name) and the total count is expected to remain
 *     manageable.  A multimap or adjacency list could be substituted if
 *     relation counts grow very large.
 */
struct Graph {
    /** All known entities, indexed by their unique name for O(1) access. */
    std::unordered_map<std::string, Entity> entities;

    /** All directed edges between entities, in insertion order. */
    std::vector<Relation> relations;
};

#endif // GRAPH_TYPES_H
