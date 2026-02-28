#!/bin/bash
# Quick test script for debugging netcon_setup with lldb

echo "Building project..."
cmake --build build

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

echo ""
echo "Starting lldb with breakpoints..."
echo "Breakpoints will be set at:"
echo "  1. sv_init (to check nextCon initialization)"
echo "  2. sv_setup (to check nextCon before calling netcon_setup)"
echo "  3. netcon_setup (to verify con parameter)"
echo ""

lldb ./build/Release/unix_main <<EOF
# Set breakpoints
breakpoint set --name sv_init
breakpoint set --name sv_setup  
breakpoint set --name netcon_setup

# Add commands to breakpoint 3 (netcon_setup) to auto-print con
breakpoint command add 3
print con
print con == NULL
frame variable
continue
DONE

# Run with server mode
run -- --isServer 1
EOF
