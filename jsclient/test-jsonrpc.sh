#!/bin/bash
# JSON-RPC 压力测试脚本

SERVER=${1:-"ws://localhost:9998"}
CLIENTS=${2:-100}
DURATION=${3:-60}
VERBOSE=${4:-""}

echo "=========================================="
echo "WebSocket JSON-RPC Stress Test"
echo "=========================================="
echo "Server:    $SERVER"
echo "Clients:   $CLIENTS"
echo "Duration:  ${DURATION}s"
echo "Mode:      $(if [ -n "$VERBOSE" ]; then echo 'Verbose'; else echo 'Quiet (default)'; fi)"
echo "=========================================="
echo ""
echo "Starting stress test..."
echo ""

if [ -n "$VERBOSE" ]; then
  node stress-test.js \
    -s "$SERVER" \
    -c "$CLIENTS" \
    -d "$DURATION" \
    -i 100 \
    -v
else
  node stress-test.js \
    -s "$SERVER" \
    -c "$CLIENTS" \
    -d "$DURATION" \
    -i 100
fi

echo ""
echo "Test completed!"
