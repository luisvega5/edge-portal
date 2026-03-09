FROM debian:bookworm-slim AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY daemons/video /src

RUN mkdir -p build \
 && cd build \
 && cmake .. \
 && cmake --build . -j

# ---- runtime stage ----
FROM debian:bookworm-slim AS runtime

# We need rpicam-vid / rpicam-jpeg inside the container.
# Those come from Raspberry Pi's repo (not Debian), so we add the Pi repo first.
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates curl gnupg \
 && curl -fsSL https://archive.raspberrypi.com/debian/raspberrypi.gpg.key \
    | gpg --dearmor -o /usr/share/keyrings/raspberrypi-archive-keyring.gpg \
 && echo "deb [signed-by=/usr/share/keyrings/raspberrypi-archive-keyring.gpg] http://archive.raspberrypi.com/debian/ bookworm main" \
    > /etc/apt/sources.list.d/raspberrypi.list \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
    rpicam-apps \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=build /src/build/video_daemon /app/video_daemon

ENV VIDEO_PORT=8080

# Default camera orientation correction (override in docker-compose if needed)
ENV RPICAM_JPEG_FLAGS="--vflip --hflip"
ENV RPICAM_VID_FLAGS="--vflip --hflip"

EXPOSE 8080
CMD ["/app/video_daemon"]
