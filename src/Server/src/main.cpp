#include <80CC.hpp>
#include <App/HeadlessApp.hpp>

#include <memory>
#include <string>
#include <cstring>
#include <spdlog/spdlog.h>

using namespace ettycc;

static void PrintUsage(const char* exe)
{
    spdlog::info("Usage: {} [options]", exe);
    spdlog::info("  --port <port>      Port to host on (default: 7777)");
    spdlog::info("  --tick-rate <hz>   Server tick rate (default: 60)");
    spdlog::info("  --scene <path>     Scene file to load (optional, loads network scene by default)");
    spdlog::info("  --help             Show this message");
}

int main(int argc, char* argv[])
{
    uint16_t    port     = 7777;
    int         tickRate = 60;
    std::string scenePath;

    // ── Parse CLI args ──────────────────────────────────────────────────────
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--tick-rate") == 0 && i + 1 < argc) {
            tickRate = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--scene") == 0 && i + 1 < argc) {
            scenePath = argv[++i];
        } else if (std::strcmp(argv[i], "--help") == 0) {
            PrintUsage(argv[0]);
            return 0;
        } else {
            spdlog::error("Unknown argument: {}", argv[i]);
            PrintUsage(argv[0]);
            return 1;
        }
    }

    spdlog::info("========================================");
    spdlog::info("  80CC Dedicated Server");
    spdlog::info("  Port:      {}", port);
    spdlog::info("  Tick rate: {} Hz", tickRate);
    spdlog::info("========================================");

    // ── Bootstrap ───────────────────────────────────────────────────────────
    auto app    = std::make_shared<HeadlessApp>(tickRate);
    auto engine = std::make_shared<Engine>(app);
    auto globals = std::make_shared<Globals>();

    engine->SetHeadlessMode(true);

    RegisterDependency(Engine, engine);
    RegisterDependency(Globals, globals);

    if (app->Init(argc, argv))
        return 1;

    // ── Network: always host ────────────────────────────────────────────────
    engine->InitNetwork(/*isHost=*/true, port);

    // ── Scene ───────────────────────────────────────────────────────────────
    if (!scenePath.empty()) {
        spdlog::info("[Server] Loading scene from: {}", scenePath);
        engine->LoadScene(scenePath, /*defaultPath=*/false);
    } else {
        spdlog::info("[Server] Loading default network scene...");
        engine->LoadNetworkScene();
    }

    spdlog::info("[Server] Listening on port {}...", port);

    // ── Run ─────────────────────────────────────────────────────────────────
    int exitCode = app->Exec();

    Dependency::getInstance().Clear();

    spdlog::info("[Server] Shutdown complete.");
    return exitCode;
}
