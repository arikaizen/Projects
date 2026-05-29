#pragma once
#include "work_item.hpp"

namespace agent {

// Base for all LLM-powered reasoning steps.
class Stage : public WorkItem {
public:
    using WorkItem::WorkItem;
    Kind kind() const override { return Kind::Stage; }
};

} // namespace agent
