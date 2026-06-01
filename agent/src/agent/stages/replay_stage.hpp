#pragma once
#include "agent/stage.hpp"
#include "agent/work_factory.hpp"
#include <map>
#include <string>

namespace agent {

// Replays a cached plan without calling the LLM.
// Remaps all item IDs to "replay_N" to avoid history collisions, rewrites
// $ref strings in inputs, pushes the items, then wires ObserveStage as usual.
class ReplayStage : public Stage {
public:
    ReplayStage(std::string id, nlohmann::json inputs = {});
    WorkResult execute(AgentContext& ctx) override;
    std::string description() const override;

private:
    static nlohmann::json remapRefs(const nlohmann::json& j,
                                     const std::map<std::string, std::string>& id_map);
};

void registerReplayStage(WorkFactory& factory);

} // namespace agent
