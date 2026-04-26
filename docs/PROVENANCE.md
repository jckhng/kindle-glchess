# Provenance And Licensing

Kindle GlChess is an unofficial Kindle-focused derivative of GNOME Chess /
glchess rules and assets from GNOME Games.

It is also informed by the GnomeGames4Kindle porting work. Original GNOME Chess
code and artwork remain credited to the GNOME Games authors. Kindle porting
groundwork and packaging references are credited to GnomeGames4Kindle and its
author(s).

This repository is not an official GNOME project and is not an official
GnomeGames4Kindle release.

## What Comes From GNOME Chess

- Chess rule implementation and game-state logic, vendored as generated C in
  `vendor/`.
- Piece SVG artwork in `assets/pieces/simple` and `assets/pieces/fancy`.
- Project licensing basis in `licenses/`.

## What Is Kindle-Specific

- `main.c`, `board.c`, and related glue for a compact GTK2/Cairo Kindle UI.
- KUAL extension files in `extension/`.
- Docker ARM build and packaging scripts.
- Runtime-library bundling for easier Kindle installation.
- Release packaging and Kindle launch scripts maintained for this derivative.

## License Notes

The GNOME-derived code and assets keep their original GPL-family licensing.
The license texts included with this repository are in:

```text
licenses/
```

The release zip also bundles shared runtime libraries from Debian Bullseye ARM.
Those libraries keep their own upstream licenses. The generated extension
package includes:

```text
extensions/kindle-chess/LICENSES/RUNTIME-LIBS.txt
extensions/kindle-chess/LICENSES/THIRD-PARTY-NOTICE.txt
```

If publishing binary releases, keep the license files and runtime notices with
the package.
