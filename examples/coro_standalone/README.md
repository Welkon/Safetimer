# Standalone Coroutine Example

This example demonstrates using `coro_base.h` **independently** without SafeTimer.

## Features

- **Zero Dependencies**: Only requires `coro_base.h`
- **Pure C89**: Works on any platform
- **Minimal RAM**: 2 bytes per coroutine context
- **No Timer Required**: Manual scheduling via function calls

## Build & Run

```bash
# Compile (from this directory)
gcc -std=c89 -I../../include example_standalone.c -o standalone_demo

# Run
./standalone_demo
```

## Expected Output

```
=== Standalone Coroutine Demo ===

--- Counter Coroutine ---
Counter: 0
Counter: 1
Counter: 2
Counter: 3
Counter: 4
Counter finished!

--- State Machine Coroutine ---
State 1: Initializing...
State 2: Processing data=42
State 3: Finalizing...

=== Demo Complete ===
```

## Key Concepts

### 1. Context Definition
```c
typedef struct {
    CORO_CONTEXT;  // Expands to: uint16_t _coro_lc
    int my_data;   // Your custom fields
} my_coro_t;
```

### 2. Coroutine Function
```c
void my_coroutine(my_coro_t *ctx) {
    CORO_BEGIN(ctx);

    // Your code here
    CORO_YIELD();  // Pause and return

    CORO_END();
}
```

### 3. Manual Scheduling
```c
my_coro_t ctx = {0};
while (!CORO_IS_EXITED(&ctx)) {
    my_coroutine(&ctx);  // Call repeatedly
}
```

## Use Cases

- State machines
- Protocol parsers
- Menu systems
- Game AI logic
- Any sequential logic without timing requirements

## Integration with SafeTimer

To add timing features, use `safetimer_coro.h` instead:
- See `examples/coroutine_demo/` for timer-integrated examples
- Automatic handle binding
- Zero-drift SLEEP/WAIT_UNTIL macros
