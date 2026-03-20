#!/bin/bash

# PRISM Environment Bootstrap Script
echo "--- [PRISM: checking dependencies] ---"

# 1. Detect Package Manager
if command -v apt &> /dev/null; then
    PM="apt"
    UPDATE="apt update"
    INSTALL="apt install -y"
    # Ubuntu/Debian specific package names
    LIBS="build-essential libcurl4-openssl-dev libjson-c-dev pkg-config"
elif command -v pkg &> /dev/null; then
    PM="pkg"
    UPDATE="pkg update"
    INSTALL="pkg install -y"
    # Termux specific package names
    LIBS="clang make curl libcurl libjson-c pkg-config"
else
    echo "Error: Unknown package manager. Please install dependencies manually."
    exit 1
fi

echo "Detected system: $PM"

# 2. Check and Install
for pkg in $LIBS; do
    if $PM list --installed 2>/dev/null | grep -q "^$pkg"; then
        echo "[OK] $pkg is already installed."
    else
        echo "[..] Installing $pkg..."
        sudo $UPDATE && sudo $INSTALL $pkg || $UPDATE && $INSTALL $pkg
    fi
done

echo "--- [PRISM: setup complete] ---"
