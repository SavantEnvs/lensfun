# lfModifier::ApplyColorModification signed integer overflow

With extreme vignetting coefficients and 16-bit pixels, `libs/lensfun/mod-color.cpp:287`
(`int r = (int (*pixels) * c10 + 512) >> 10`) multiplies a pixel by a huge gain and overflows `int`.
UBSan halts: `signed integer overflow: 26271 * -108451 cannot be represented in type 'int'`.

Reproduce: `/mayhem/fuzz_modifier-standalone mayhem/fuzz_modifier/known-findings/modifycolor-int-overflow/crash.bin`
(exit 134). Upstream code, not patched here.
