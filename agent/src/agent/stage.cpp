#include "agent/stage.hpp"
#include "agent/agent_context.hpp"
#include "agent/agent_logger.hpp"

namespace agent {

LLMClient::Response Stage::llmComplete(AgentContext& ctx, const LLMClient::Request& req)
{
    if (auto* logger = ctx.logger())
        logger->llmCall(ctx.config().agent_id, name, id,
                        req.system_prompt, req.user_message,
                        req.json_mode, req.temperature, req.max_tokens);

    auto resp = ctx.llm().complete(req);

    if (auto* logger = ctx.logger())
        logger->llmResponse(ctx.config().agent_id, name, id,
                            resp.content, resp.success, resp.error);

    return resp;
}

} // namespace agent
