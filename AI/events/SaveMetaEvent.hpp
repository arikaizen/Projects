#pragma once

#include "IEvent.hpp"
#include "convo.hpp"

#include <filesystem>
#include <string>

/**
 * SaveMetaEvent
 * ──────────────────────────────────────────────────────────────────────────
 * Responsibility: write meta.json
 *
 * Records enough information to know which save files exist, which tier was
 * last written, and how to reload the processed KV cache correctly.
 *
 * Output path: <output_dir>/meta.json
 *
 * JSON schema:
 * {
 *   "timestamp":        "YYYY-MM-DD_HH-MM-SS",
 *   "tokens_processed": <int>,
 *   "ctx_size":         <int>,
 *   "model_path":       "<string>",
 *   "title":            "<string> | null"
 * }
 */
class SaveMetaEvent : public IEvent {
public:
    /**
     * @param convo       The conversation whose metadata to record.
     * @param model_path  Path to the .gguf file (for reload verification).
     * @param ctx_size    Context window size used when the cache was saved.
     * @param output_dir  Directory where meta.json will be written.
     */
    SaveMetaEvent(AIConvo&              convo,
                  std::string           model_path,
                  int                   ctx_size,
                  std::filesystem::path output_dir);

    void        Run()        override;
    std::string Name() const override { return "SaveMeta"; }

private:
    AIConvo&              _convo;
    std::string           _model_path;
    int                   _ctx_size;
    std::filesystem::path _output_dir;
};
