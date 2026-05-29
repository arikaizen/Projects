#pragma once
#include "work_item.hpp"

namespace agent {

// Base for all deterministic operations (file I/O, shell, MCP calls, memory).
class Action : public WorkItem {
public:
    using WorkItem::WorkItem;
    Kind kind() const override { return Kind::Action; }
};

} // namespace agent
