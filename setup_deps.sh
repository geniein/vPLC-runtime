#!/bin/bash
set -e

echo "=========================================================="
echo " vPLC Runtime - Linux Dependency & Build Installer"
echo "=========================================================="

# 1. Install system utilities and mosquitto
echo "[1/4] Installing build essentials and mosquitto..."
sudo apt-get update
sudo apt-get install -y build-essential cmake libmosquitto-dev git

# 2. Build & Install snap7
if ldconfig -p | grep -q "libsnap7"; then
    echo "[2/4] snap7 library is already installed. Skipping..."
else
    echo "[2/4] Building and installing snap7 from source..."
    TEMP_DIR=$(mktemp -d)
    echo "Cloning snap7 into $TEMP_DIR..."
    git clone https://github.com/SCADACS/snap7.git "$TEMP_DIR/snap7"
    
    cd "$TEMP_DIR/snap7/build/unix"
    
    # Detect architecture (supports x86_64, arm etc.)
    ARCH=$(uname -m)
    if [ "$ARCH" = "x86_64" ]; then
        MAKE_FILE="x86_64_linux.mk"
        BIN_DIR="x86_64-linux"
    elif [[ "$ARCH" == arm* ]] || [ "$ARCH" = "aarch64" ]; then
        # Default to standard arm v7 or similar, or aarch64
        if [ "$ARCH" = "aarch64" ]; then
            MAKE_FILE="aarch64_linux.mk"
            BIN_DIR="aarch64-linux"
        else
            MAKE_FILE="arm_v7_linux.mk"
            BIN_DIR="arm_v7-linux"
        fi
    else
        MAKE_FILE="x86_64_linux.mk"
        BIN_DIR="x86_64-linux"
    fi

    echo "Building snap7 using $MAKE_FILE..."
    make -f "$MAKE_FILE" all
    
    echo "Copying libsnap7.so to /usr/local/lib..."
    sudo cp "../bin/$BIN_DIR/libsnap7.so" /usr/local/lib/
    
    # Note: snap7.h is already safely vendored inside src/3rdparty/
    # No need to copy it to /usr/local/include, bypassing permission restrictions!
    
    echo "Updating library cache..."
    sudo ldconfig
    echo "snap7 library installed successfully!"
fi

# 3. Build vPLC project using CMake
echo "[3/4] Generating build files via CMake..."
cd "/home/ingenie/vPLC-runtime"
mkdir -p build
cd build
cmake ..

echo "[4/4] Building vPLC and shared simulator libraries..."
make -j$(nproc)

# Copy output libraries to the project root so it can load them
echo "Copying compiled libraries and executable to project root..."
cp vPlc ..
cp libmock_logic.so ..
cp libassembly_logic.so ..

echo "=========================================================="
echo " 🎉 Build complete! You can now run vPLC:"
echo "    ./vPlc --tank      (Water Tank Simulator)"
echo "    ./vPlc --assembly  (Assembly Line Simulator)"
echo "=========================================================="
