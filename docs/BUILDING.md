# Building Kindle GlChess

## Requirements

- Docker.
- `make`, `sha256sum`, and a POSIX shell on the host.
- ARM binfmt support if your Docker setup does not already run ARM containers.

On Linux, ARM binfmt can usually be installed with:

```bash
docker run --privileged --rm tonistiigi/binfmt --install arm
```

Docker Desktop often already has this configured.

## Fast Rebuild

From the repository root:

```bash
./docker_rebuild.sh
```

This creates or reuses:

```text
image:     kindle-glchess-armhf-build:bullseye
container: kindle-glchess-armhf-builder
```

The build output is:

```text
kindle-chess
dist/kindle-glchess-extension.zip
```

The script also runs:

```text
smoke-test
```

## Build Without Packaging

```bash
KINDLE_CHESS_PACKAGE=0 ./docker_rebuild.sh
```

## Shell Into The Builder

```bash
./docker_shell.sh
```

Inside the container:

```bash
make clean
make kindle-chess smoke-test
./smoke-test
```

## Rebuild The Docker Image

```bash
./docker_build_image.sh
```

If you moved the repository and the persistent container still points at an old
checkout, recreate it:

```bash
docker rm -f kindle-glchess-armhf-builder
./docker_rebuild.sh
```

## Local Host Build

A local build is only useful if the host has GTK2, Cairo, and librsvg
development headers:

```bash
make
```

For actual Kindle releases, prefer the Docker ARM build so the binary ABI
matches the bundled runtime.

## Packaging Details

`package_extension.sh` creates:

```text
dist/extensions/kindle-chess
dist/documents/shortcut_kindleglchess.sh
dist/kindle-glchess-extension.zip
```

The package contains:

- ARM `kindle-chess` executable.
- KUAL `config.xml` and `menu.json`.
- Launch/stop/log helper scripts.
- GNOME Chess piece SVG assets.
- Runtime libraries copied from the ARM Docker container.
- License and third-party runtime notices.

If you have a compatible ARM Stockfish binary, place it at:

```text
bin/armhf/stockfish
```

before packaging. Otherwise the package includes `stockfish.sh`, which can use
an engine provided by another extension if present.
