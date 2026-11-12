#!/usr/bin/env bash
set -e

echo "=== Running mypreprocessor test suite ==="

TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PREPROC="$TEST_DIR/../mypreprocessor"

if [ ! -x "$PREPROC" ]; then
    echo "ERROR: Executable $PREPROC not found or not executable. Run 'make' first."
    exit 1
fi

# Run preprocessor on sample.c with include path set to tests directory
"$PREPROC" -I "$TEST_DIR" "$TEST_DIR/sample.c"

OUTPUT_FILE="$TEST_DIR/sample.i"
if [ ! -f "$OUTPUT_FILE" ]; then
    echo "FAILED: Expected output file $OUTPUT_FILE was not created!"
    exit 1
fi

echo "Generated $OUTPUT_FILE:"
echo "--------------------------------------------------"
cat "$OUTPUT_FILE"
echo "--------------------------------------------------"

# Validation 1: Verify comments are removed (outside of string literals)
if grep -v '"' "$OUTPUT_FILE" | grep -q '//'; then
    echo "FAILED: Standalone single-line comment '//' found in output!"
    exit 1
fi

# Validation 2: Verify macro PI was substituted with 3.14
if grep -q "3.14" "$OUTPUT_FILE"; then
    echo "[PASS] Simple macro substitution (PI -> 3.14)"
else
    echo "FAILED: PI was not replaced with 3.14"
    exit 1
fi

# Validation 3: Verify macro with arguments SQUARE(x) was substituted
if grep -q "((3) \* (3))" "$OUTPUT_FILE" || grep -q "((r) \* (r))" "$OUTPUT_FILE" || grep -q "((3)\*(3))" "$OUTPUT_FILE" || grep -q "((r)\*(r))" "$OUTPUT_FILE"; then
    echo "[PASS] Function-like macro substitution (SQUARE(x))"
else
    echo "FAILED: SQUARE(x) was not expanded"
    exit 1
fi

# Validation 4: Verify identifier PIE is untouched
if grep -q "PIE" "$OUTPUT_FILE"; then
    echo "[PASS] Token boundary check (PIE remained untouched)"
else
    echo "FAILED: PIE was incorrectly replaced"
    exit 1
fi

# Validation 5: Verify conditional compilation works (#if DEBUG block kept, #undef/#ifdef discarded)
if grep -q "debug on" "$OUTPUT_FILE" && ! grep -q "never printed" "$OUTPUT_FILE"; then
    echo "[PASS] Conditional compilation (#if / #else / #undef / #ifdef)"
else
    echo "FAILED: Conditional block mismatch"
    exit 1
fi

echo "=========================================="
echo "All tests passed successfully!"
echo "=========================================="
