#!/usr/bin/env bash
#
# mayhem/build.sh — build lensfun's fuzz harnesses (sanitized, instrumented library) and the
# project's own test suite (normal flags). Runs in /mayhem as `mayhem`; re-runnable and offline.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

FUZZ_BUILD="$SRC/mayhem/build-fuzz"
TEST_BUILD="$SRC/mayhem/build-tests"
# Always configure from scratch: a re-run may see a different CC path than the image build, and
# CMake then drops the cache (and its -D options) mid-configure. ccache keeps the rebuild cheap.
rm -rf "$FUZZ_BUILD" "$TEST_BUILD"

# 1) The library, sanitized + SanCov-instrumented (fuzzer-no-link is unconditional, so even the
#    no-sanitizer build gives the fuzzed code coverage edges), static so each harness is one binary.
#    Build type "None" keeps CMake from appending its own -g (DWARF-5) or -DNDEBUG.
# Two benign UBSan patterns are relaxed for the library (everything else stays halting):
#  nonnull-attribute: _lf_mlstrdup(NULL) does memcpy(dst, NULL, 0) (auxfun.cpp:148), on every entry
#    without a localized name; function: mod-color.cpp casts SSE routines to a generic callback type.
FUZZ_FLAGS="$SANITIZER_FLAGS -fno-sanitize=nonnull-attribute,function -fsanitize=fuzzer-no-link $DEBUG_FLAGS -O1"
cmake -S "$SRC" -B "$FUZZ_BUILD" \
      -DCMAKE_BUILD_TYPE=None \
      -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
      -DCMAKE_C_FLAGS="$FUZZ_FLAGS" -DCMAKE_CXX_FLAGS="$FUZZ_FLAGS" \
      -DBUILD_STATIC=ON -DBUILD_TESTS=OFF -DINSTALL_PYTHON_MODULE=OFF -DINSTALL_HELPER_SCRIPTS=OFF
cmake --build "$FUZZ_BUILD" -j"$MAYHEM_JOBS" --target lensfun

# 2) Harnesses: libFuzzer binary + non-fuzzer -standalone reproducer, both with leak detection
#    compiled out (mayhem/lsan_off.cc).
GLIB_CFLAGS="$(pkg-config --cflags glib-2.0)"
GLIB_LIBS="$(pkg-config --libs glib-2.0)"
HARNESS_FLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS -O1 -std=c++17 -DCONF_LENSFUN_STATIC -I$FUZZ_BUILD $GLIB_CFLAGS"

$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$FUZZ_BUILD/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$FUZZ_BUILD/standalone_main.o"

for t in fuzz_db_xml fuzz_db_search fuzz_modifier; do
  $CXX $HARNESS_FLAGS $LIB_FUZZING_ENGINE "$SRC/mayhem/$t.cc" "$FUZZ_BUILD/libs/lensfun/liblensfun.a" \
       "$FUZZ_BUILD/lsan_off.o" $GLIB_LIBS -o "/mayhem/$t"
  $CXX $HARNESS_FLAGS "$SRC/mayhem/$t.cc" "$FUZZ_BUILD/standalone_main.o" "$FUZZ_BUILD/libs/lensfun/liblensfun.a" \
       "$FUZZ_BUILD/lsan_off.o" $GLIB_LIBS -o "/mayhem/$t-standalone"
done

# 3) The project's own test suite + a known-answer probe, with the project's NORMAL flags (shared lib,
#    default build type, no sanitizers), so mayhem/test.sh only runs them.
cmake -S "$SRC" -B "$TEST_BUILD" \
      -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
      -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS" \
      -DBUILD_TESTS=ON -DINSTALL_PYTHON_MODULE=OFF -DINSTALL_HELPER_SCRIPTS=OFF
cmake --build "$TEST_BUILD" -j"$MAYHEM_JOBS"
$CXX -O2 -std=c++17 $COVERAGE_FLAGS -I"$TEST_BUILD" $GLIB_CFLAGS "$SRC/mayhem/kat.cc" \
     -L"$TEST_BUILD/libs/lensfun" -llensfun -Wl,-rpath,"$TEST_BUILD/libs/lensfun" $GLIB_LIBS \
     -o "$TEST_BUILD/kat"
