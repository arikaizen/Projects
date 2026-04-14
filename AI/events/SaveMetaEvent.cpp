#include "SaveMetaEvent.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

namespace {

std::string NowStamp() {
    const auto now  = std::chrono::system_clock::now();
    const auto secs = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &secs);
#else
    localtime_r(&secs, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%d_%H-%M-%S", &tm);
    return buf;
}

} // namespace

SaveMetaEvent::SaveMetaEvent(AIConvo&              convo,
                             std::string           model_path,
                             int                   ctx_size,
                             std::filesystem::path output_dir)
    : _convo(convo)
    , _model_path(std::move(model_path))
    , _ctx_size(ctx_size)
    , _output_dir(std::move(output_dir))
{}

void SaveMetaEvent::Run() {
    std::filesystem::create_directories(_output_dir);
    const auto path = _output_dir / "meta.json";

    const auto title = _convo.GetTitle();

    json doc;
    doc["timestamp"]        = NowStamp();
    doc["tokens_processed"] = _convo.TokensProcessed();
    doc["ctx_size"]         = _ctx_size;
    doc["model_path"]       = _model_path;
    doc["title"]            = title.has_value() ? json(*title) : json(nullptr);

    std::ofstream out(path);
    if (!out.is_open())
        throw std::runtime_error("SaveMetaEvent: cannot open " + path.string());

    out << doc.dump(2);
    if (!out)
        throw std::runtime_error("SaveMetaEvent: write failed for " + path.string());
}
