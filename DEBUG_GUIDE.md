# LLDB Debugging Guide for shadowhunt

## Quick Start: Running with LLDB

```bash
# Build the project first
cmake --build build

# Run with lldb
lldb ./build/Release/unix_main

# Or set environment variable and run
lldb -- ./build/Release/unix_main --isServer 1
```

## Setting Breakpoints

### 1. Break at a Function
```lldb
(lldb) breakpoint set --name netcon_setup
# or shorter:
(lldb) b netcon_setup

# Break at multiple functions
(lldb) b netcon_setup
(lldb) b sv_setup
(lldb) b sv_init
```

### 2. Break at a Specific File and Line
```lldb
(lldb) breakpoint set --file net_conlayer.c --line 19
# or shorter:
(lldb) b net_conlayer.c:19
```

### 3. Break with Condition (only when con is NULL)
```lldb
(lldb) b netcon_setup
(lldb) breakpoint modify 1 --condition 'con == NULL'
```

### 4. List All Breakpoints
```lldb
(lldb) breakpoint list
# or:
(lldb) br list
```

### 5. Delete Breakpoints
```lldb
(lldb) breakpoint delete 1
# or delete all:
(lldb) breakpoint delete
```

## Running and Stepping

### Start Execution
```lldb
(lldb) run
# or with arguments:
(lldb) run -- --isServer 1
```

### Step Through Code
```lldb
# Step into functions (goes into function calls)
(lldb) step
# or: s

# Step over (executes function calls without going in)
(lldb) next
# or: n

# Step out (finish current function)
(lldb) finish
# or: f

# Continue execution until next breakpoint
(lldb) continue
# or: c
```

## Inspecting Variables

### Print Variables
```lldb
# Print a variable
(lldb) print con
# or: p con

# Print with formatting
(lldb) p/x con          # hexadecimal
(lldb) p/t con          # binary
(lldb) p con            # default format

# Print all local variables
(lldb) frame variable
# or: fr v

# Print specific variable with type info
(lldb) frame variable con
```

### Inspect Structures
```lldb
# Print structure members
(lldb) p *con
(lldb) p con->incomingSequence
(lldb) p con->outgoingSequence

# Print with more detail
(lldb) p -M con
```

### Check if Pointer is NULL
```lldb
(lldb) p con == NULL
(lldb) p con != 0x0
```

## Call Stack Navigation

### View Call Stack
```lldb
(lldb) bt              # backtrace
(lldb) thread backtrace

# More detailed backtrace
(lldb) bt all
```

### Navigate Frames
```lldb
# List frames
(lldb) frame info

# Go to specific frame
(lldb) frame select 1
(lldb) frame select 2

# See variables in different frame
(lldb) frame select 1
(lldb) frame variable
```

## Testing the netcon_setup Fix

### Test Case 1: Verify nextCon is initialized
```lldb
# Set breakpoints
(lldb) b sv_init
(lldb) b sv_setup
(lldb) b netcon_setup

# Run
(lldb) run -- --isServer 1

# When sv_init breaks:
(lldb) p nextCon
# Should show: (netcon_t *) $0 = 0x0000000000000000

# Continue to end of sv_init
(lldb) finish
(lldb) p nextCon
# Should now show a valid address (not NULL)

# Continue to sv_setup
(lldb) continue

# When sv_setup breaks:
(lldb) p nextCon
# Should show valid address

# Continue to netcon_setup
(lldb) continue

# When netcon_setup breaks:
(lldb) p con
(lldb) p con == NULL
# Should be false (0)
```

### Test Case 2: Test NULL pointer handling
```lldb
# Set breakpoint with condition
(lldb) b netcon_setup
(lldb) breakpoint modify 1 --condition 'con == NULL'

# Run
(lldb) run -- --isServer 1

# If breakpoint hits, verify the NULL check works
(lldb) p con
# Should be NULL
(lldb) step
# Should hit the com_error line
```

## Useful Commands Summary

```lldb
# Setup
b netcon_setup          # Set breakpoint
b sv_setup
b sv_init

# Run
run -- --isServer 1     # Start execution

# When breakpoint hits:
p con                   # Print variable
p con == NULL           # Check if NULL
fr v                    # All local variables
bt                      # Call stack

# Navigation
n                       # Next line
s                       # Step into
c                       # Continue
finish                  # Finish function

# Cleanup
breakpoint delete       # Remove all breakpoints
```

## Advanced: Watchpoints

Watch for when a variable changes:
```lldb
# Watch when nextCon is written to
(lldb) watchpoint set variable nextCon

# Watch when con is set to NULL
(lldb) watchpoint set expression -w write -- con
```

## Tips

1. **Use aliases**: Create shortcuts for common commands
   ```lldb
   (lldb) command alias b breakpoint set --name
   (lldb) command alias p print
   ```

2. **Logging**: Automate variable inspection
   ```lldb
   (lldb) breakpoint command add 1
   > p con
   > p con == NULL
   > continue
   > DONE
   ```

3. **Breakpoint commands**: Auto-execute commands when breakpoint hits
   ```lldb
   (lldb) breakpoint command add 1
   Enter your debugger command(s). Type 'DONE' to end.
   > p con
   > bt
   > DONE
   ```
