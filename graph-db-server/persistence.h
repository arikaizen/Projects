#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include "graph_types.h"
#include <string>
#include <vector>

class Persistence {
public:
    explicit Persistence(const std::string& filePath);

    void save(const Graph& graph);
    Graph load();

private:
    std::string m_filePath;

    static std::string escapeJson(const std::string& s);
    static std::string unescapeJson(const std::string& s);
    static std::string extractString(const std::string& json, const std::string& key);
    static std::vector<std::string> extractArray(const std::string& json, const std::string& key);
};

#endif // PERSISTENCE_H
