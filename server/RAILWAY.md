# Deploy the backend to Railway

Create a Railway project with two services:

1. Add a PostgreSQL service.
2. Add the repository as a backend service and set its root directory to `server`.

Railway will detect `server/Dockerfile`. Connect the PostgreSQL service variables to
the backend service so that `DATABASE_URL` is available at runtime.

Set the backend service pre-deploy command to:

```sh
sh scripts/migrate.sh
```

The backend start command is:

```sh
./TrueSightServer
```

Railway provides `PORT` automatically. The backend reads both `PORT` and
`DATABASE_URL` from the environment and listens on all IPv4 interfaces.

After deployment, check:

```text
https://YOUR_RAILWAY_DOMAIN/health-check
```

For the Qt client, set these environment variables before building it:

```text
BACKEND_URL=https://YOUR_RAILWAY_DOMAIN
WS_URL=wss://YOUR_RAILWAY_DOMAIN/ws
```

Do not commit database URLs, passwords, private keys, or other Railway secrets.