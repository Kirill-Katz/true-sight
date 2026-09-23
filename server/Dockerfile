FROM migrate/migrate:v4.18.3 AS migrate

FROM ubuntu:24.04 AS build

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        libboost-all-dev \
        libpqxx-dev \
        libssl-dev \
        ninja-build \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel

FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        libboost-all-dev \
        libpqxx-dev \
        libssl3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=build /app/build/TrueSightServer .
COPY --from=migrate /migrate /usr/local/bin/migrate
COPY db/migrations ./db/migrations
COPY scripts/migrate.sh ./scripts/migrate.sh

CMD ["./TrueSightServer"]