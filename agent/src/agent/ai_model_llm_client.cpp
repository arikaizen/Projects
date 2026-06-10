#include "agent/ai_model_llm_client.hpp"
#include <stdexcept>
#include <string>

namespace agent {

// Try to extract tool calls from raw model output.
// Recognises two formats that local models commonly produce:
//
//   Format A — XML-style tags:
//     <tool_call>{"name":"fn","arguments":{...}}</tool_call>
//
//   Format B — bare JSON object or array with "name"/"arguments" keys:
//     {"name":"fn","arguments":{...}}
//     [{"name":"fn","arguments":{...}}, ...]
static std::vector<LLMClient::ToolCall> parseToolCalls(const std::string& text)
{
    std::vector<LLMClient::ToolCall> calls;

    // Format A: scan for <tool_call>...</tool_call> blocks
    const std::string open_tag  = "<tool_call>";
    const std::string close_tag = "</tool_call>";
    std::string::size_type pos  = 0;
    while (true) {
        auto start = text.find(open_tag, pos);
        if (start == std::string::npos) break;
        auto end = text.find(close_tag, start + open_tag.size());
        if (end == std::string::npos) break;

        std::string block = text.substr(start + open_tag.size(),
                                        end - (start + open_tag.size()));
        pos = end + close_tag.size();

        try {
            auto j = nlohmann::json::parse(block);
            if (j.contains("name") && j["name"].is_string()) {
                LLMClient::ToolCall tc;
                tc.id        = j.value("id", std::string{});
                tc.name      = j["name"].get<std::string>();
                tc.arguments = j.value("arguments", nlohmann::json::object());
                calls.push_back(std::move(tc));
            }
        } catch (...) {}
    }
    if (!calls.empty()) return calls;

    // Format B: try parsing the entire text as JSON
    try {
        auto j = nlohmann::json::parse(text);

        auto extract = [&](const nlohmann::json& item) {
            if (!item.is_object()) return;
            if (!item.contains("name") || !item["name"].is_string()) return;
            LLMClient::ToolCall tc;
            tc.id        = item.value("id", std::string{});
            tc.name      = item["name"].get<std::string>();
            tc.arguments = item.value("arguments",
                           item.value("parameters", nlohmann::json::object()));
            calls.push_back(std::move(tc));
        };

        if (j.is_array()) {
            for (const auto& item : j) extract(item);
        } else {
            extract(j);
        }
    } catch (...) {}

    return calls;
}

LLMClient::Response AIModelLLMClient::complete(const Request& req) {
    try {
        std::string prompt;
        if (!req.system_prompt.empty()) {
            prompt += req.system_prompt;
            prompt += "\n\n";
        }
        prompt += req.user_message;

        if (req.json_mode) {
            prompt += "\n\nRespond with valid JSON only. Do not include any prose "
                      "outside the JSON value.";
        }

        std::string out = m_model.Generate(prompt, req.temperature, req.max_tokens);

        Response resp;
        resp.content    = out;
        resp.success    = true;
        resp.tool_calls = parseToolCalls(out);
        return resp;
    } catch (const std::exception& e) {
        return Response{ /*content=*/"", /*success=*/false, /*error=*/e.what(), /*tool_calls=*/{} };
    } catch (...) {
        return Response{ "", false, "AIModelLLMClient::complete: unknown error", {} };
    }
}

} // namespace agent
