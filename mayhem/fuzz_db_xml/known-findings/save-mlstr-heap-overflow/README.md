# lfDatabase::Save reads past a multi-language string

`crash.xml` (a trimmed copy of lensfun's own `data/db` mount entries, with a `lang="ru"` name) loads fine,
and then `lfDatabase::Save()` aborts under ASan with a heap-buffer-overflow read in
`_lf_xml_printf_mlstr` (`libs/lensfun/auxfun.cpp:183`, `strchr (cur, 0) + 1` walks past the end of the
`lfMLstr`), called from `database.cpp:909`.

Reproduce: `/mayhem/fuzz_db_xml-standalone mayhem/fuzz_db_xml/known-findings/save-mlstr-heap-overflow/crash.xml`
(exit 134). Upstream code, not patched here; the input is kept out of `testsuite/` so the seeds stay clean.
