# Installation (Build and run instructions)
Quick guide to build and run 80cc engine editor

# Dependencies installation (optional)
installl dependencies with:

```bash
vcpkg install --manifest
```

but due to vcpkg being linked to cmake dependencies will be downloaded automatically

# Compilation
todo...

#### Run `install-external.sh`
run this script in order to download external repos, this are quite different than the vcpkg version and also experimental.
`./install-external.sh`

# Running
- Add env var ASSETS_80CC TO DEFINE editor working path (no multi-proyects for the moment....)

```bash
set ASSETS_80CC=<some path>/assets
```