# Kindle GlChess Extension Files

Copy these files into your Kindle extension folder:

- `config.xml` -> `/mnt/us/extensions/kindle-chess/config.xml`
- `menu.json` -> `/mnt/us/extensions/kindle-chess/menu.json`
- `launch_kindleglchess.sh` -> `/mnt/us/extensions/kindle-chess/launch_kindleglchess.sh`
- `stop_kindleglchess.sh` -> `/mnt/us/extensions/kindle-chess/stop_kindleglchess.sh`
- `tail_log_kindleglchess.sh` -> `/mnt/us/extensions/kindle-chess/tail_log_kindleglchess.sh`

Optional document shortcut:

- `shortcut_kindleglchess.sh` -> `/mnt/us/documents/shortcut_kindleglchess.sh`

Make the scripts executable on the Kindle:

```sh
chmod 755 /mnt/us/extensions/kindle-chess/*.sh
chmod 755 /mnt/us/documents/shortcut_kindleglchess.sh
```

KUAL is the reliable tap-launch path. The document shortcut is only useful on
Kindles that have a shell-script document association installed; the stock
Kindle home screen normally does not execute `.sh` files directly.

If tapping does nothing, launch from KUAL or run this over SSH:

```sh
/mnt/us/extensions/kindle-chess/launch_kindleglchess.sh --restart
tail -n 80 /mnt/us/kindle-glchess-shortcut.log
tail -n 80 /mnt/us/kindle-glchess.log
```

The launcher expects the app binary at:

`/mnt/us/extensions/kindle-chess/bin/armhf/kindle-chess`

It will look for a Stockfish engine in this order:

1. `/mnt/us/extensions/kindle-chess/bin/armhf/stockfish`
2. `/mnt/us/extensions/kindle-chess/bin/stockfish.sh`
3. `/mnt/us/extensions/gnomegames/bin/stockfish.sh`

For a fully self-contained engine package, place a compatible ARM Stockfish
binary here:

`/mnt/us/extensions/kindle-chess/bin/armhf/stockfish`
