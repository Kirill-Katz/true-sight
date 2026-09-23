# TrueSight architecture

## Overview

TrueSight is a C++20 messenger composed of a Qt/QML desktop client, a C++
HTTP/WebSocket server, and a PostgreSQL database. A Vue 3/Vite web client is
also present. The clients perform key generation, key sharing, encryption,
decryption, and message-signature verification; the server stores and relays
message envelopes.

The repository does not contain a deployment service, a server container image,
or an automatic migration runner.

## Repository layout

| Path | Contents |
| --- | --- |
| `client/` | Qt 6/QML desktop application, OpenSSL crypto, HTTP/WebSocket controllers, and the QCoro submodule under `client/external/qcoro`. |
| `server/` | Boost.Asio/Beast HTTP and WebSocket server, controllers, JWT and OpenSSL helpers, PostgreSQL access, and migrations. |
| `web_client/` | Vue 3 SPA, Vite configuration, browser crypto helpers, file handling, and API client. |
| `server/db/migrations/` | Ordered PostgreSQL up/down migration files. |
| `docs/` | Protocol and developer documentation. |
| `test_data/` | Checked-in example key and share files plus a signing script; these are test fixtures, not production credentials. |
| `.github/workflows/` | Manual/tag-triggered macOS and Windows desktop-client packaging workflows. |

## Runtime architecture

The server listens on all IPv4 interfaces on TCP port `8888`. It routes normal
HTTP requests by path and upgrades `/ws` requests to authenticated WebSocket
sessions. It creates a PostgreSQL connection pool with four workers. The
connection string and port are constants in `server/src/main.cpp`; the JWT
secret is read from the `JWT_SECRET` environment variable.

The desktop client builds URLs from `AppSettings` and sends bearer tokens on
HTTP requests. Its startup code uses `ws://localhost:8888/ws` as the WebSocket
URL in the direct WebSocket example; the configured settings are used by the
main client flow. The repository does not define a production endpoint or TLS
termination.

The web client uses the same HTTP API and file/protocol formats. Its Vite
development proxy targets `http://localhost:8888`. Browsers cannot set the
custom `Authorization` header during the WebSocket handshake in this client,
so it refreshes the active conversation over HTTP every five seconds instead.

## Server HTTP API

The routes registered in `server/src/main.cpp` are:

| Method | Path | Authentication | Behavior visible in source |
| --- | --- | --- | --- |
| `GET` | `/health-check` | No | Returns a health response and broadcasts `{"message":"hello"}` to connected WebSocket sessions. |
| `POST` | `/register` | No | Stores a user IID and Ed25519 public-key PEM. Duplicate IIDs return conflict. |
| `POST` | `/get-challenge` | No | Creates or returns an unexpired authentication challenge for a user IID. |
| `POST` | `/validate-challenge` | No | Verifies the challenge with the registered Ed25519 key, marks it used, and returns a JWT. |
| `GET` | `/conversations` | Bearer JWT | Lists peers with their latest message timestamp. |
| `GET` | `/messages` | Bearer JWT | Requires `from`; returns a paged conversation. Defaults are limit `50`, offset `0`. |
| `POST` | `/messages` | Bearer JWT | Stores a version-2 encrypted and signed message envelope, then notifies the recipient if connected. |
| upgrade | `/ws` | Bearer JWT | Authenticates the handshake and registers one in-memory session per IID. |

Message bodies use JSON and binary fields are base64url encoded. The server
validates nonce length (12 bytes), authentication-tag length (16 bytes),
signature length (64 bytes), protocol version (`2`), and positive message
counters. It does not decrypt or verify message signatures before storing them.
Conversation retrieval orders database rows by `created_at` and `message_id`;
clients verify and order signed messages according to the protocol document.

## Authentication and security components

Registration associates a user IID with an Ed25519 public-key PEM. Login is a
challenge-response flow: the server creates a temporary challenge, the client
signs it, and the server verifies the signature before issuing an HMAC-SHA256
(`HS256`) JWT. Tokens contain `sub`, `iat`, `exp`, `jti`, `iss`, and `aud`; the
source sets a 15-minute lifetime and validates the issuer, audience, algorithm,
timestamps, and signature. Protected routes require `Authorization: Bearer ...`.

Client message keys are derived from X25519 shared secrets with HKDF-SHA-256
and direction-specific context. Message plaintext is encrypted with
AES-256-GCM-SIV using a random 12-byte nonce and a 16-byte tag. Ed25519 signs
the version-2 message envelope. The exact share-file and message-envelope
formats are documented in [share-format-v1](share-format-v1.md) and
[message-protocol-v2](message-protocol-v2.md).

These mechanisms are implemented in the clients and server validation code,
but the repository does not document or implement TLS, key rotation, rate
limiting, server-side message authentication, delivery acknowledgements, or
multi-instance WebSocket coordination.

## Database

The final schema represented by the migrations contains:

- `user_to_pk`: generated UUID, unique `user_iid`, Ed25519 public-key PEM, and
  creation timestamp.
- `auth_challenge`: generated UUID, user IID, UTF-8 challenge text, expiry,
  creation timestamp, and `is_used`. Indexes support expiry and user/expiry
  lookups.
- `messages`: generated UUID, sender and receiver IIDs, nonce, ciphertext,
  authentication tag, optional signature for legacy rows, protocol version,
  positive message counter, and creation timestamp. Indexes support both
  directions and sender/receiver lookups.

Migration `000009` adds the version-2 signature and validation constraint while
preserving older rows. The version-2 clients reject legacy unsigned messages;
the migration does not convert them. Apply migrations in numeric order before
using the server. No migration command is provided in the repository.

## Client structure

The desktop client separates QML screens under `client/ui/` from C++
controllers, stores, HTTP/WebSocket helpers, application settings, and crypto
under `client/src/`. QML is compiled into the executable with
`qt_add_qml_module`. QCoro supplies coroutine integration for networking.

The web client contains Vue components under `web_client/src/components/`,
state and orchestration in `src/composables/useClient.js`, API and browser
storage helpers in `src/lib/`, and crypto/protocol handling in
`src/lib/crypto.js`.

## Current status and limits visible in the repository

- The Compose file starts PostgreSQL only; it does not start `TrueSightServer`
  or apply migrations.
- Server database credentials and the listening port are compiled constants.
- `JWT_SECRET` must be set for authentication, but no secret-management or
  production configuration mechanism is included.
- The desktop packaging workflows build only the client. There is no server
  release artifact or deployment workflow.
- CI runs on manual dispatch or pushes of tags matching `v*`; no workflow runs
  automatically for pull requests.
- The web client has a build/dev setup, but no web deployment workflow is
  present in the repository.
