#!/bin/bash
# Automated xv6 demo script
# Usage: ./demo.sh

cd "$(dirname "$0")"

# Build first
echo "Building xv6..."
make clean > /dev/null 2>&1
make > /dev/null 2>&1

if [ $? -ne 0 ]; then
    echo "Build failed. Make sure you have the i386-elf toolchain installed."
    exit 1
fi

echo "Starting xv6 demo..."
echo "================================"
echo "This demo will show:"
echo "  1. P2: getlastcat syscall"
echo "  2. P5: mmap test program"
echo "================================"
echo ""

# Create a command sequence file
cat > /tmp/xv6_demo_cmds << 'EOF'
echo
echo "=== Demo 1: getlastcat syscall ==="
getlastcat
cat README
getlastcat
echo
echo "=== Demo 2: mmap test ==="
test
echo
echo "=== Demo complete! ==="
EOF

# Run QEMU with the demo commands
# Using expect for interactive control
if command -v expect &> /dev/null; then
    expect << 'EXPECT_SCRIPT'
    set timeout 60
    spawn make qemu-nox

    # Wait for shell prompt
    expect "$ "

    # Demo 1: getlastcat
    send "echo '=== Demo 1: getlastcat syscall ==='\r"
    expect "$ "

    send "getlastcat\r"
    expect "$ "

    send "cat README\r"
    expect "$ "

    send "getlastcat\r"
    expect "$ "

    # Demo 2: mmap test
    send "echo '=== Demo 2: mmap test ==='\r"
    expect "$ "

    send "test\r"
    expect "$ "

    send "echo '=== Demo complete! Press Ctrl-A X to exit ==='\r"
    expect "$ "

    # Keep it running for user to see
    interact
EXPECT_SCRIPT
else
    echo "For fully automated demo, install 'expect': brew install expect"
    echo ""
    echo "Starting xv6 manually. Run these commands:"
    echo "  getlastcat"
    echo "  cat README"
    echo "  getlastcat"
    echo "  test"
    echo ""
    echo "Press Ctrl-A X to exit QEMU"
    echo ""
    make qemu-nox
fi
