#!/usr/bin/env bash
set -e

echo "===================================="
echo "HELIX - BUILD COMPLETA"
echo "===================================="
echo

echo "[1/4] make clean"
make clean

echo
echo "[2/4] make"
make

echo
echo "[3/4] make -f Makefile.live clean"
make -f Makefile.live clean

echo
echo "[4/4] make -f Makefile.live"
make -f Makefile.live

echo
chmod +x helix helix-live

echo "===================================="
echo "BUILD COMPLETATA CON SUCCESSO"
echo "===================================="
