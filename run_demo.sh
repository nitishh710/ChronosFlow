#!/usr/bin/env bash
set -e

echo "=== Building ChronosFlow ==="
#!/usr/bin/env bash
# Exit immediately if a command exits with a non-zero status
set -e

echo "=== ChronosFlow: Zero-Copy IPC Engine ==="

echo "[*] Cleaning and preparing build directory..."
rm -rf build
mkdir -p build && cd build

echo "[*] Running CMake and Compiling..."
cmake ..
# Utilize all available CPU cores for faster compilation
make -j$(nproc)

echo ""
echo "=== Launching Pipeline ==="

# 1. Start the Consumer (LLM Process) in the background
echo "[*] Starting Consumer..."
./consumer_llm &
CONSUMER_PID=$!

# Brief pause to ensure the consumer is listening
sleep 1

# 2. Start the Producer (Vision Process) in the foreground
echo "[*] Starting Producer..."
./producer_vision

# 3. Wait for the background consumer process to finish
wait $CONSUMER_PID

echo ""
echo "=== Execution Finished Successfully ==="