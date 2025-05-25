# ASAN Issues

## Known Issues with AddressSanitizer

### Random Crashes During Compilation with ASAN

When compiling the tests with AddressSanitizer enabled (`--asan` flag), the compilation process may **randomly** crash with a `DEADLYSIGNAL` error message repeated multiple times. This is a known issue with the current version of AddressSanitizer.

**Important Note**: The error is completely random and intermittent. Sometimes the compilation and execution will work perfectly fine with ASAN enabled, and other times it will fail with the `DEADLYSIGNAL` error. This has been verified through multiple test runs.

**Environment Details:**
- GCC Version: 11.4.0 (Ubuntu 11.4.0-1ubuntu1~22.04)
- ASAN Version: Bundled with GCC 11.4.0
- Operating System: Ubuntu 22.04

**Error Pattern:**
```
AddressSanitizer:DEADLYSIGNAL
<repeated multiple times>
```

**Workaround:**
1. Try running the build command again without any changes
2. If the issue persists, try using Valgrind instead of ASAN for memory checking
3. For critical debugging, consider using a different compiler version or disabling ASAN temporarily

### ASAN Environment Variables

When running tests with ASAN enabled, the following environment variables are set to improve the debugging experience:

```
ASAN_OPTIONS="handle_segv=0:allow_user_segv_handler=1:detect_leaks=1:print_stacktrace=1:halt_on_error=0"
```

These options:
- Prevent ASAN from handling SEGV signals directly
- Allow the application to use its own SEGV handler
- Enable leak detection
- Print stack traces for errors
- Continue execution after errors (don't halt immediately)

## Usage Recommendations

1. **For Regular Testing:** Use the standard test execution without ASAN
   ```
   ./libs/cpp_dbc/run_test_cpp_dbc.sh
   ```

2. **For Memory Leak Detection:** Use Valgrind instead of ASAN
   ```
   ./libs/cpp_dbc/run_test_cpp_dbc.sh --valgrind
   ```

3. **For ASAN Testing (when needed):** Be prepared for occasional compilation failures
   ```
   ./libs/cpp_dbc/run_test_cpp_dbc.sh --asan
   ```
   If compilation fails, simply try again or use Valgrind instead.