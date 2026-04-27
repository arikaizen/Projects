#include "graph_store.h"
#include "persistence.h"

#include <algorithm>
#include <stdexcept>

GraphStore::GraphStore(Graph graph, Persistence& persistence)
    : m_graph(std::move(graph)), m_persistence(persistence) {}

void GraphStore::createEntity(const std::string& name, const std::string& type) {
    if (m_graph.entities.count(name)) {
        throw std::runtime_error("Entity already exists: " + name);
    }
    Entity entity;
    entity.name = name;
    entity.type = type;
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

void GraphStore::createRelation(const std::string& from, const std::string& rel, const std::string& to) {
    if (!m_graph.entities.count(from)) {
        throw std::runtime_error("Source entity not found: " + from);
    }
    if (!m_graph.entities.count(to)) {
        throw std::runtime_error("Target entity not found: " + to);
    }
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
    m_graph.entities.erase(name);

    auto& rels = m_graph.relations;
    rels.erase(
        std::remove_if(rels.begin(), rels.end(),
            [&](const Relation& r) { return r.from == name || r.to == name; }),
        rels.end()
    );
    m_persistence.save(m_graph);
}

void GraphStore::deleteObservation(const std::string& entity, const std::string& observation) {
    auto it = m_graph.entities.find(entity);
    if (it == m_graph.entities.end()) {
        throw std::runtime_error("Entity not found: " + entity);
    }
    auto& obs = it->second.observations;
    auto obsIt = std::find(obs.begin(), obs.end(), observation);
    if (obsIt == obs.end()) {
        throw std::runtime_error("Observation not found on entity: " + entity);
    }
    obs.erase(obsIt);
    m_persistence.save(m_graph);
}

void GraphStore::deleteRelation(const std::string& from, const std::string& rel, const std::string& to) {
    auto& rels = m_graph.relations;
    auto it = std::remove_if(rels.begin(), rels.end(),
        [&](const Relation& r) {
            return r.from == from && r.relationType == rel && r.to == to;
        });
    if (it == rels.end()) {
        throw std::runtime_error("Relation not found");
    }
    rels.erase(it, rels.end());
    m_persistence.save(m_graph);
}

std::vector<Entity> GraphStore::searchNodes(const std::string& query) const {
    std::vector<Entity> results;

    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    for (const auto& [name, entity] : m_graph.entities) {
        auto containsQuery = [&](const std::string& s) {
            std::string lower = s;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            return lower.find(lowerQuery) != std::string::npos;
        };

        if (containsQuery(entity.name) || containsQuery(entity.type)) {
            results.push_back(entity);
            continue;
        }
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
    for (const auto& rel : m_graph.relations) {
        if (rel.from == entity || rel.to == entity) {
            results.push_back(rel);
        }
    }
    return results;
}

const Graph& GraphStore::readGraph() const {
    return m_graph;
}
