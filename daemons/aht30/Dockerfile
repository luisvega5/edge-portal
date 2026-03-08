# ---- build stage ----
FROM debian:bookworm-slim AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake linux-libc-dev \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# Copy only the daemon sources
COPY daemons/aht30 /src

RUN mkdir -p build \
 && cd build \
 && cmake .. \
 && cmake --build . -j

# ---- runtime stage ----
FROM debian:bookworm-slim AS runtime

WORKDIR /app

COPY --from=build /src/build/aht30_daemon /app/aht30_daemon

EXPOSE 7070

CMD ["/app/aht30_daemon"]
