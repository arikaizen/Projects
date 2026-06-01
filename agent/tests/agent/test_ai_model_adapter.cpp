// Verifies the AIModel -> LLMClient and AIModel -> MemoryBackend adapters
// using an in-tree deterministic AIModel subclass (no external model needed).

#include "agent/ai_model_llm_client.hpp"
#include "agent/ai_model_memory_backend.hpp"
#include "ai_model/aimodel.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

// Deterministic fake model: Generate echoes the prompt length + a marker;
// Embed produces a tiny bag-of-chars vector so similarity is meaningful.
class FakeModel : public AIModel {
public:
    std::string GetModelName()        const override { return "fake-1"; }
    int         GetMaxContextLength() const override { return 4096; }

protected:
    std::string RawGenerate(const std::string& prompt, float, int) override {
        last_prompt = prompt;
        return "ECHO(" + std::to_string(prompt.size()) + ")";
    }

    std::vector<float> RawEmbed(const std::string& text) override {
        // 26-dim lowercase letter histogram; never all-zero for alpha text.
        std::vector<float> v(26, 0.0f);
        for (unsigned char c : text) {
            if (c >= 'a' && c <= 'z') v[c - 'a'] += 1.0f;
            else if (c >= 'A' && c <= 'Z') v[c - 'A'] += 1.0f;
        }
        // Guard against an all-zero vector (AIModel::Embed rejects those).
        bool any = false;
        for (float f : v) if (f != 0.0f) any = true;
        if (!any) v[0] = 1.0f;
        return v;
    }

public:
    std::string last_prompt;
};

void test_llm_client_adapter() {
    FakeModel model;
    agent::AIModelLLMClient client(model);

    assert(client.modelName() == "fake-1");

    agent::LLMClient::Request req;
    req.system_prompt = "SYS";
    req.user_message  = "USER";
    req.json_mode     = true;
    auto resp = client.complete(req);

    assert(resp.success);
    assert(resp.error.empty());
    assert(resp.content.rfind("ECHO(", 0) == 0);
    // system + "\n\n" + user + json instruction must all be folded into prompt.
    assert(model.last_prompt.find("SYS") != std::string::npos);
    assert(model.last_prompt.find("USER") != std::string::npos);
    assert(model.last_prompt.find("JSON") != std::string::npos);
    std::cout << "[ok] llm_client_adapter: " << resp.content << "\n";
}

void test_llm_client_error_path() {
    FakeModel model;
    agent::AIModelLLMClient client(model);

    // Blank prompt -> AIModel::Generate throws -> adapter returns success=false.
    agent::LLMClient::Request req;  // empty system + empty user
    auto resp = client.complete(req);
    assert(!resp.success);
    assert(!resp.error.empty());
    std::cout << "[ok] llm_client_error_path: " << resp.error << "\n";
}

void test_memory_backend_adapter() {
    FakeModel model;
    agent::AIModelMemoryBackend mem(model);

    mem.write("doc.cat", "the cat sat on the mat", {{"kind", "animal"}});
    mem.write("doc.dog", "a dog ran in the park",  {{"kind", "animal"}});
    mem.write("doc.code","compile the cpp source", {{"kind", "tech"}});

    // list with no filter returns all three.
    assert(mem.list().size() == 3);
    // list with filter matches id or content substring.
    assert(mem.list("cat").size() == 1);
    assert(mem.list("doc.").size() == 3);

    // search ranks by embedding cosine similarity; the closest doc to a
    // cat-heavy query should be doc.cat.
    auto hits = mem.search("the cat and the mat", 2);
    assert(!hits.empty());
    assert(hits.size() <= 2);
    assert(hits.front().id == "doc.cat");
    assert(hits.front().metadata.at("kind") == "animal");
    std::cout << "[ok] memory_backend_adapter: top=" << hits.front().id
              << " score=" << hits.front().score << "\n";

    // remove drops the entry.
    mem.remove("doc.cat");
    assert(mem.list().size() == 2);
    assert(mem.list("cat").empty());
    std::cout << "[ok] memory_backend_adapter: remove\n";
}

} // namespace

int main() {
    test_llm_client_adapter();
    test_llm_client_error_path();
    test_memory_backend_adapter();
    std::cout << "All AIModel adapter tests passed.\n";
    return 0;
}
