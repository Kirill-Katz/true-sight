# TrueSight development guide

## Prerequisites

The CMake projects require CMake 3.20 or newer, Ninja, and a C++20 compiler.
The exact library requirements are:

- Desktop client: Qt 6.8+ components Quick, Network, and WebSockets; OpenSSL;
  and the recursive QCoro submodule.
- Server: Boost.System, Boost.JSON, Boost.URL, libpqxx, and OpenSSL.
- Web client: Node.js/npm compatible with the checked-in `package-lock.json`.

The repository does not pin compiler versions or provide a universal dependency
installation script. The GitHub workflows show the supported hosted build
setups for macOS and Windows.

## Clone and initialize

```sh
git clone --recurse-submodules https://github.com/Kirill-Katz/true-sight.git
cd true-sight
```

For an existing clone:

```sh
git submodule update --init --recursive
```

## Build the desktop client

```sh
cmake -S client -B build/client -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/client
```

The client CMake project exports `compile_commands.json`, compiles C++ sources
and QML files, and links Qt Quick/Network/WebSockets, OpenSSL, and QCoro. The
optional `ENABLE_ASAN`, `ENABLE_TSAN`, and `ENABLE_UBSAN` CMake options enable
the corresponding sanitizers; ASan and TSan cannot be enabled together.

## Build the server

```sh
cmake -S server -B build/server -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/server
```

The result is the `TrueSightServer` executable. The build does not launch a
database, apply migrations, or configure runtime environment variables.

## Run PostgreSQL locally

From the repository root:

```sh
docker compose -f server/docker-compose.yml up -d postgres
```

The Compose service exposes PostgreSQL 17 on `localhost:5432` with database
`true_sight`, user `true_sight`, and password `true_sight`. The server source
uses these same values. The Compose file persists data in the
`postgres_data` volume, but it does not apply `server/db/migrations/`; use a
PostgreSQL migration tool or apply the ordered SQL files manually. No such tool
is included here.

Before starting the server, set a JWT secret in its environment:

```sh
# PowerShell
$env:JWT_SECRET = "use-a-local-development-secret"

# POSIX shell
export JWT_SECRET='use-a-local-development-secret'
```

Run the built executable from an environment where PostgreSQL is reachable. It
binds TCP port `8888` and prints `Listening on http://localhost:8888`.

## Run the web client

```sh
cd web_client
npm install
npm run dev
```

The Vite development proxy targets `http://localhost:8888`. The available
scripts are `dev`, `build`, and `preview`. `VITE_API_URL` can override the API
base in the web client, but the backend must permit any resulting cross-origin
requests. The browser implementation uses periodic HTTP refresh rather than
the authenticated WebSocket path.

## Configuration assumptions

The desktop settings store backend and WebSocket URLs, login state, username,
and a private-key file path through Qt settings. The source does not define a
complete default configuration file or a documented production URL. The server
port and PostgreSQL connection string are constants in `server/src/main.cpp`.
Only `JWT_SECRET` is read from the process environment by the server.

Never use real private keys, JWT secrets, passwords, messages, or production
data in the repository. The files under `test_data/` are fixtures for local
experimentation.

## Packaging and releases

The macOS workflow runs on `macos-latest`, installs Qt/OpenSSL/CMake/Ninja and
`create-dmg` with Homebrew, builds Release, deploys Qt frameworks and QML,
ad-hoc signs the app, creates `TrueSight-macOS.dmg`, uploads it, and publishes
it on `v*` tags.

The Windows workflow runs on `windows-latest`, installs Qt 6.8.0, configures
MSVC and vcpkg, installs OpenSSL, builds Release, runs `windeployqt`, creates an
Inno Setup installer, uploads `TrueSight-Setup.exe`, and publishes it on `v*`
tags.

Both workflows also support manual dispatch. They package the desktop client
only. There is no server packaging, database packaging, web deployment, or
automated migration step in the workflows.

## Contribution workflow

The contribution instructions in the root [README](../README.md) are the
canonical fork, branch, local-check, commit, push, and pull-request workflow.
In particular, contributors should run the build for the part they changed,
inspect `git diff`, run `git diff --check`, and describe authentication,
cryptography, protocol, and testing implications in the pull request. The
workflows are not pull-request triggers, so local validation matters.

## Documentation map

- [Architecture and protocols](architecture.md)
- [Share file format v1](share-format-v1.md)
- [Signed message ordering v2](message-protocol-v2.md)
