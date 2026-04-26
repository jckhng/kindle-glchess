# Installing On Kindle

## Prerequisites

This package targets jailbroken Kindle devices that can run native KUAL
extensions.

You need:

- Jailbreak for your exact Kindle model and firmware.
- KUAL.
- MRPI if your setup uses MobileRead package installation.
- Enough free USB-storage space for the extension and runtime libraries.

Recommended references:

- Kindle Modding Wiki: https://kindlemodding.org/jailbreaking/
- KUAL and MRPI setup: https://kindlemodding.org/jailbreaking/post-jailbreak/installing-kual-mrpi/
- KUAL MobileRead thread: https://www.mobileread.com/forums/showthread.php?t=203326
- MRPI MobileRead wiki: https://wiki.mobileread.com/wiki/MobileRead_Package_Installer

## Install

Copy `release/kindle-glchess-extension.zip` to your computer and unzip it at the
Kindle USB-storage root.

Expected resulting paths:

```text
/mnt/us/extensions/kindle-chess
/mnt/us/documents/shortcut_kindleglchess.sh
```

Over SSH, fix executable bits if needed:

```sh
chmod 755 /mnt/us/extensions/kindle-chess/*.sh
chmod 755 /mnt/us/extensions/kindle-chess/bin/stockfish.sh
chmod 755 /mnt/us/extensions/kindle-chess/bin/armhf/kindle-chess
chmod 755 /mnt/us/documents/shortcut_kindleglchess.sh
```

Launch from:

```text
KUAL -> Kindle GlChess -> Launch
```

## Restart Or Stop

From KUAL:

```text
Kindle GlChess -> Restart
Kindle GlChess -> Stop
```

Over SSH:

```sh
/mnt/us/extensions/kindle-chess/launch_kindleglchess.sh --restart
/mnt/us/extensions/kindle-chess/stop_kindleglchess.sh
```

## Logs

```sh
tail -n 120 /mnt/us/kindle-glchess.log
tail -n 120 /mnt/us/kindle-glchess-shortcut.log
```

If KUAL works but tapping the document shortcut does nothing, that is expected
on many Kindles. The stock home screen usually does not execute `.sh` files
without an additional script launcher/file association.
