#!/bin/bash
# IFMY DIAMOND PERFECT — installation pour Logic Pro
cd "$(dirname "$0")" || exit 1

echo ""
echo "  ◆ IFMY DIAMOND PERFECT — installation"
echo ""

COMP="$HOME/Library/Audio/Plug-Ins/Components"
VST3="$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$COMP" "$VST3"

for p in "IFMY Diamond Perfect.component" "IFMY Diamond Perfect MIDI.component"; do
  if [ -d "$p" ]; then
    rm -rf "$COMP/$p"
    cp -R "$p" "$COMP/"
    xattr -dr com.apple.quarantine "$COMP/$p" 2>/dev/null
    echo "  ✓ $p"
  fi
done

for p in "IFMY Diamond Perfect.vst3" "IFMY Diamond Perfect MIDI.vst3"; do
  if [ -d "$p" ]; then
    rm -rf "$VST3/$p"
    cp -R "$p" "$VST3/"
    xattr -dr com.apple.quarantine "$VST3/$p" 2>/dev/null
  fi
done

# Force macOS à rescanner les Audio Units
killall -9 AudioComponentRegistrar 2>/dev/null

echo ""
echo "  C'est installé. Relance Logic Pro."
echo "  Effet audio  : Audio FX > Audio Units > IFMY > IFMY Diamond Perfect"
echo "  Version MIDI : piste instrument > slot Instrument > AU MIDI-controlled Effects > IFMY"
echo ""
read -n 1 -s -r -p "  Appuie sur une touche pour fermer…"
echo ""
