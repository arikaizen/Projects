// test_factory.cpp — WorkFactory unit tests
//
// Covers:
//   - Registering an item, creating it, verifying its concrete type
//   - Duplicate registration throws
//   - Creating an unknown name throws
//   - toCatalogJson() contains all registered items
//   - isRegistered() returns the correct bool
//   - inputSchema() returns the registered schema

#include "test_helper.hpp"
#include "agent/action.hpp"
#include <typeinfo>

// ── Minimal concrete Action for testing ─────────────────────────────────────

namespace {

struct NullAction : agent::Action {
    NullAction(std::string id, nlohmann::json inp)
        : agent::Action(std::move(id), "NullAction", std::move(inp)) {}

    agent::WorkResult execute(agent::AgentContext&) override {
        agent::WorkResult r;
        r.item_id   = id;
        r.item_name = name;
        r.item_kind = "Action";
        r.success   = true;
        r.timestamp = std::chrono::system_clock::now();
        return r;
    }
};

} // anonymous namespace

int main() {
    std::cout << "=== test_factory ===\n";

    // ── Section 1: Register and create ───────────────────────────────────────
    test::section("register and create");
    {
        agent::WorkFactory factory;

        const nlohmann::json schema = {
            {"type", "object"},
            {"properties", {{"arg", {{"type", "string"}}}}}
        };

        CHECK_NOTHROW(factory.registerItem(
            agent::WorkItemSpec{"NullAction", "A no-op action for testing",
                                agent::WorkItem::Kind::Action, schema},
            [](std::string id, nlohmann::json inp) -> std::unique_ptr<agent::WorkItem> {
                return std::make_unique<NullAction>(std::move(id), std::move(inp));
            }
        ));

        // isRegistered
        CHECK(factory.isRegistered("NullAction"));
        CHECK(!factory.isRegistered("DoesNotExist"));

        // create returns a non-null pointer
        std::unique_ptr<agent::WorkItem> item;
        CHECK_NOTHROW(item = factory.create("NullAction", "n1", {{"arg","hello"}}));
        CHECK(item != nullptr);

        // Correct type and fields
        CHECK_EQ(item->id,   std::string("n1"));
        CHECK_EQ(item->name, std::string("NullAction"));
        CHECK(item->kind() == agent::WorkItem::Kind::Action);

        // Dynamic cast succeeds
        NullAction* cast_ptr = dynamic_cast<NullAction*>(item.get());
        CHECK(cast_ptr != nullptr);
    }

    // ── Section 2: Duplicate registration throws ─────────────────────────────
    test::section("duplicate registration throws");
    {
        agent::WorkFactory factory;
        auto reg = [&]() {
            factory.registerItem(
                agent::WorkItemSpec{"Widget", "desc", agent::WorkItem::Kind::Action, {}},
                [](std::string id, nlohmann::json inp) -> std::unique_ptr<agent::WorkItem> {
                    return std::make_unique<NullAction>(std::move(id), std::move(inp));
                }
            );
        };
        CHECK_NOTHROW(reg());
        CHECK_THROW(reg());  // second call with same name must throw
    }

    // ── Section 3: Unknown name throws on create ──────────────────────────────
    test::section("unknown name throws on create");
    {
        agent::WorkFactory factory;
        CHECK_THROW(factory.create("Nonexistent", "x", {}));
    }

    // ── Section 4: toCatalogJson contains registered items ────────────────────
    test::section("toCatalogJson contains registered items");
    {
        agent::WorkFactory factory;
        factory.registerItem(
            agent::WorkItemSpec{"Alpha", "First action",  agent::WorkItem::Kind::Action, {}},
            [](std::string id, nlohmann::json inp) -> std::unique_ptr<agent::WorkItem> {
                return std::make_unique<NullAction>(std::move(id), std::move(inp));
            }
        );
        factory.registerItem(
            agent::WorkItemSpec{"Beta",  "Second action", agent::WorkItem::Kind::Action, {}},
            [](std::string id, nlohmann::json inp) -> std::unique_ptr<agent::WorkItem> {
                return std::make_unique<NullAction>(std::move(id), std::move(inp));
            }
        );

        auto catalog = factory.toCatalogJson();
        CHECK(catalog.is_array() || catalog.is_object());

        // Catalog must serialise to a string that contains both names
        std::string catalog_str = catalog.dump();
        CHECK(catalog_str.find("Alpha") != std::string::npos);
        CHECK(catalog_str.find("Beta")  != std::string::npos);
    }

    // ── Section 5: inputSchema returns the registered schema ─────────────────
    test::section("inputSchema returns correct schema");
    {
        agent::WorkFactory factory;
        const nlohmann::json schema = {
            {"type", "object"},
            {"properties", {{"x", {{"type", "integer"}}}}}
        };
        factory.registerItem(
            agent::WorkItemSpec{"Schemed", "has schema", agent::WorkItem::Kind::Action, schema},
            [](std::string id, nlohmann::json inp) -> std::unique_ptr<agent::WorkItem> {
                return std::make_unique<NullAction>(std::move(id), std::move(inp));
            }
        );

        const nlohmann::json* s = factory.inputSchema("Schemed");
        CHECK(s != nullptr);
        if (s) {
            CHECK_EQ((*s)["type"].get<std::string>(), std::string("object"));
            CHECK(s->contains("properties"));
        }

        // Unknown name returns nullptr
        const nlohmann::json* missing = factory.inputSchema("NoSuchThing");
        CHECK(missing == nullptr);
    }

    // ── Section 6: listSpecs contains all registered specs ───────────────────
    test::section("listSpecs round-trip");
    {
        agent::WorkFactory factory;
        factory.registerItem(
            agent::WorkItemSpec{"A", "desc-a", agent::WorkItem::Kind::Action, {}},
            [](std::string id, nlohmann::json inp) -> std::unique_ptr<agent::WorkItem> {
                return std::make_unique<NullAction>(std::move(id), std::move(inp));
            }
        );
        factory.registerItem(
            agent::WorkItemSpec{"B", "desc-b", agent::WorkItem::Kind::Stage, {}},
            [](std::string id, nlohmann::json inp) -> std::unique_ptr<agent::WorkItem> {
                return std::make_unique<NullAction>(std::move(id), std::move(inp));
            }
        );

        auto specs = factory.listSpecs();
        CHECK_EQ(static_cast<int>(specs.size()), 2);

        bool found_a = false, found_b = false;
        for (const auto& sp : specs) {
            if (sp.name == "A") { found_a = true; CHECK_EQ(sp.description, std::string("desc-a")); }
            if (sp.name == "B") { found_b = true; CHECK(sp.kind == agent::WorkItem::Kind::Stage); }
        }
        CHECK(found_a);
        CHECK(found_b);
    }

    test::summary();
    return test::all_passed() ? 0 : 1;
}
