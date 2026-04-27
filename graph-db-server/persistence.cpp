#include "persistence.h"

#include <fstream>
#include <stdexcept>
#include <cstdio>

Persistence::Persistence(const std::string& filePath)
    : m_filePath(filePath) {}

std::string Persistence::escapeJson(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (unsigned char c : s) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n";  break;
            case '\r': result += "\\r";  break;
            case '\t': result += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    result += buf;
                } else {
                    result += static_cast<char>(c);
                }
                break;
        }
    }
    return result;
}

std::string Persistence::unescapeJson(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            ++i;
            switch (s[i]) {
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                case '/':  result += '/';  break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                case 'u':
                    if (i + 4 < s.size()) {
                        unsigned int cp = 0;
                        for (int j = 1; j <= 4; ++j) {
                            unsigned char h = static_cast<unsigned char>(s[i + j]);
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp += h - '0';
                            else if (h >= 'a' && h <= 'f') cp += h - 'a' + 10;
                            else if (h >= 'A' && h <= 'F') cp += h - 'A' + 10;
                        }
                        i += 4;
                        if (cp < 0x80) {
                            result += static_cast<char>(cp);
                        } else if (cp < 0x800) {
                            result += static_cast<char>(0xC0 | (cp >> 6));
                            result += static_cast<char>(0x80 | (cp & 0x3F));
                        } else {
                            result += static_cast<char>(0xE0 | (cp >> 12));
                            result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                            result += static_cast<char>(0x80 | (cp & 0x3F));
                        }
                    }
                    break;
                default:
                    result += s[i];
                    break;
            }
        } else {
            result += s[i];
        }
    }
    return result;
}

std::string Persistence::extractString(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\":\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return "";
    pos += searchKey.size();

    std::string raw;
    while (pos < json.size()) {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            raw += json[pos];
            raw += json[pos + 1];
            pos += 2;
        } else if (json[pos] == '"') {
            break;
        } else {
            raw += json[pos++];
        }
    }
    return unescapeJson(raw);
}

std::vector<std::string> Persistence::extractArray(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\":[";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return {};
    pos += searchKey.size();

    std::vector<std::string> result;
    while (pos < json.size() && json[pos] != ']') {
        if (json[pos] == '"') {
            ++pos;
            std::string raw;
            while (pos < json.size()) {
                if (json[pos] == '\\' && pos + 1 < json.size()) {
                    raw += json[pos];
                    raw += json[pos + 1];
                    pos += 2;
                } else if (json[pos] == '"') {
                    break;
                } else {
                    raw += json[pos++];
                }
            }
            result.push_back(unescapeJson(raw));
            if (pos < json.size()) ++pos; // skip closing quote
        } else {
            ++pos;
        }
    }
    return result;
}

void Persistence::save(const Graph& graph) {
    std::ofstream file(m_filePath, std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + m_filePath);
    }

    for (const auto& [name, entity] : graph.entities) {
        file << "{\"type\":\"entity\",\"name\":\""
             << escapeJson(entity.name)
             << "\",\"entityType\":\""
             << escapeJson(entity.type)
             << "\",\"observations\":[";
        for (size_t i = 0; i < entity.observations.size(); ++i) {
            if (i > 0) file << ",";
            file << "\"" << escapeJson(entity.observations[i]) << "\"";
        }
        file << "]}\n";
    }

    for (const auto& rel : graph.relations) {
        file << "{\"type\":\"relation\",\"from\":\""
             << escapeJson(rel.from)
             << "\",\"relationType\":\""
             << escapeJson(rel.relationType)
             << "\",\"to\":\""
             << escapeJson(rel.to)
             << "\"}\n";
    }
}

Graph Persistence::load() {
    Graph graph;
    std::ifstream file(m_filePath);
    if (!file.is_open()) return graph;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::string type = extractString(line, "type");
        if (type == "entity") {
            Entity entity;
            entity.name = extractString(line, "name");
            entity.type = extractString(line, "entityType");
            entity.observations = extractArray(line, "observations");
            if (!entity.name.empty()) {
                graph.entities[entity.name] = std::move(entity);
            }
        } else if (type == "relation") {
            Relation rel;
            rel.from = extractString(line, "from");
            rel.relationType = extractString(line, "relationType");
            rel.to = extractString(line, "to");
            if (!rel.from.empty() && !rel.to.empty()) {
                graph.relations.push_back(std::move(rel));
            }
        }
    }
    return graph;
}
