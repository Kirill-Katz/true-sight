#!/usr/bin/env sh
set -eu

: "${DATABASE_URL:?DATABASE_URL is required}"

migrate \
    -path /app/db/migrations \
    -database "${DATABASE_URL}" \
    up