#pragma once
#include "work_item.hpp"
#include "llm_client.hpp"

namespace agent {

class AgentContext;  // forward declaration

// Base for all LLM-powered reasoning steps.
class Stage : public WorkItem {
public:
    using WorkItem::WorkItem;
    Kind kind() const override { return Kind::Stage; }

protected:
    // Wraps ctx.llm().complete(req) with pre/post logging via ctx.logger()
    // when an AgentLogger is available.  All stages should call this instead
    // of ctx.llm().complete() directly so every LLM interaction is captured.
    LLMClient::Response llmComplete(AgentContext& ctx, const LLMClient::Request& req);
};

} // namespace agent
