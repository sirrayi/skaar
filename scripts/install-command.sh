#!/bin/sh
# Install the `skaar` terminal command: a tiny shim in ~/.local/bin (or $1)
# that runs scripts/skaar from this repo.
set -eu
REPO=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DEST=${1:-$HOME/.local/bin}
mkdir -p "$DEST"
cat > "$DEST/skaar" <<SHIM
#!/bin/sh
# Installed by $REPO/scripts/install-command.sh
SKAAR_HOME=\${SKAAR_HOME:-$REPO}
export SKAAR_HOME
exec "\$SKAAR_HOME/scripts/skaar" "\$@"
SHIM
chmod +x "$DEST/skaar"
echo "installed $DEST/skaar"
case ":$PATH:" in *":$DEST:"*) ;; *) echo "note: $DEST is not on your PATH" ;; esac
