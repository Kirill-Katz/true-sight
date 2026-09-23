# TrueSight

TrueSight is a C++ chat application with a Qt/QML desktop client and a C++ server backed by PostgreSQL.

## Developer documentation

- [Architecture, API behavior, database, security, and current limitations](docs/architecture.md)
- [Build, local PostgreSQL, configuration, packaging, CI, and development workflow](docs/development.md)
- [Signed message ordering v2](docs/message-protocol-v2.md)
- [Share file format v1](docs/share-format-v1.md)

## Contributing: step by step

You need a [GitHub account](https://github.com/signup) and [Git installed](https://git-scm.com/downloads). You do **not** need permission to push to this repository: you will make a copy (a *fork*) under your own GitHub account and propose your changes with a *pull request* (PR).

### 1. Fork the repository

1. Open [TrueSight on GitHub](https://github.com/Kirill-Katz/true-sight).
2. Click **Fork** in the top-right corner, then **Create fork**.
3. You should now be on `https://github.com/YOUR_USERNAME/true-sight`.

If the buttons look different, follow [GitHub's illustrated fork guide](https://docs.github.com/en/pull-requests/how-tos/work-with-forks/fork-a-repo).

### 2. Download your fork

Open a terminal and replace `YOUR_USERNAME` with your GitHub username:

```sh
git clone --recurse-submodules https://github.com/YOUR_USERNAME/true-sight.git
cd true-sight
git remote add upstream https://github.com/Kirill-Katz/true-sight.git
```

`origin` is your fork; `upstream` is the original repository. `--recurse-submodules` downloads the client's QCoro dependency too. If you already cloned without it, run `git submodule update --init --recursive` inside the repository.

### 3. Make a branch

Give each change its own branch. For example:

```sh
git switch -c fix/conversation-list
```

Do not work directly on `main`. Keep the change focused: one bug fix or feature per PR is easiest to review.

### 4. Make and check your change

Edit the files you need. The main directories are:

- `client/` — Qt/QML desktop application.
- `server/` — C++ API and SQL migrations.
- `.github/workflows/` — macOS and Windows client build workflows.

Build the part you changed before opening a PR. You need CMake 3.20+, Ninja, a C++20 compiler, and the appropriate dependencies installed:

```sh
# Client: Qt 6.8+ (Quick, Network, WebSockets), OpenSSL, and QCoro submodule
cmake -S client -B build/client -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/client

# Server: Boost (system, json, url), libpqxx, and OpenSSL
cmake -S server -B build/server -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/server
```

Run only the commands for the part you changed. If CMake cannot find a dependency, consult the [macOS](.github/workflows/build-macos.yml) or [Windows](.github/workflows/build-windows.yml) workflow for the packages and configuration used by CI. The [server Compose file](server/docker-compose.yml) starts **PostgreSQL only**; it does not start the API or apply the SQL migrations in `server/db/migrations/`. The client defaults to `http://localhost:8888` for its backend.

Before committing, check exactly what changed:

```sh
git status
git diff --check
git diff
```

This is a security-sensitive messenger. Never commit real private keys, key bundles, JWT secrets, passwords, personal messages, or production data. Use disposable test data and explain any change to authentication, cryptography, or the message protocol in your PR.

### 5. Commit and push to your fork

Stage only the files you intended to change. Replace the example path and commit message:

```sh
git add path/to/changed-file
git diff --cached
git commit -m "Fix conversation list rendering"
git push -u origin fix/conversation-list
```

If Git asks you to authenticate, follow [GitHub's authentication instructions](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/about-authentication-to-github). Do not put a password or token in the repository.

### 6. Open a pull request

1. Open your fork on GitHub. After the push, click **Compare & pull request**. If you do not see it, open the original repository's **Pull requests** tab and click **New pull request** → **compare across forks**.
2. Check that the **base** is `Kirill-Katz/true-sight:main` and the **compare** branch is `YOUR_USERNAME/true-sight:fix/conversation-list` (or your actual branch name).
3. Include the issue number in the PR description. Write `Closes #123` if the PR fixes issue 123 (GitHub will close it when the PR is merged), or `Related to #123` if it only relates to that issue. If there is no issue yet, open one first.
4. Write what changed, why, and how you tested it. Mention anything you could not test.
5. Click **Create pull request**. Choose **Draft pull request** if the work is not ready for review.

GitHub has a [step-by-step guide to PRs from forks](https://docs.github.com/en/pull-requests/how-tos/create-pull-requests/creating-a-pull-request-from-a-fork) and a [guide to linking PRs to issues](https://docs.github.com/en/issues/tracking-your-work-with-issues/using-issues/linking-a-pull-request-to-an-issue).

If a reviewer asks for changes, edit the **same branch**, commit, and push again. Your PR updates automatically; you do not need to open another one.

**Note:** The current macOS and Windows build workflows run manually or on `v*` tags, not automatically for every PR. Report the local build or checks you ran in the PR description.
