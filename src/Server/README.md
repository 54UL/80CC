# 80CC Dedicated Server

Headless dedicated server for the 80CC engine. Runs the full ECS, physics (Bullet3), and networking (ENet) pipeline without any GPU, window, or audio dependencies.

## Building

### Build server alongside the editor (default)

```bash
cmake -B build -S src
cmake --build build
```

This produces both `Entry` (editor) and `80CC_Server` executables.

### Build server only

```bash
cmake -B build -S src -D80CC_DEDICATED_SERVER=ON
cmake --build build
```

## Usage

```
80CC_Server [options]
```

### Options

| Flag | Default | Description |
|------|---------|-------------|
| `--port <port>` | `7777` | UDP port to host on |
| `--tick-rate <hz>` | `60` | Server simulation tick rate |
| `--scene <path>` | *(none)* | Absolute path to a scene JSON file to load |
| `--help` | | Show usage info |

### Examples

```bash
# Start with defaults (port 7777, 60 Hz, built-in network scene)
./80CC_Server

# Custom port and tick rate
./80CC_Server --port 9000 --tick-rate 30

# Load a specific scene
./80CC_Server --scene /path/to/my_scene.json --port 7777

# On Windows
80CC_Server.exe --port 7777
```

### Shutdown

Press `Ctrl+C` to shut down gracefully. The server handles `SIGINT`/`SIGTERM` and drains active connections before exiting.

## What runs on the server

- **ECS** — Full entity/component/system registry
- **Physics** — Bullet3 rigid body and soft body simulation (async-stepped on thread pool)
- **Networking** — ENet host, transform broadcast/sync via lock-free SPSC queues
- **Scene** — Scene graph, serialization/deserialization, `NetworkComponent` replication

## What is skipped

- Rendering (OpenGL, shaders, sprites, cameras, framebuffers)
- Audio (OpenAL)
- Input (keyboard, mouse)
- ImGui / Editor UI
