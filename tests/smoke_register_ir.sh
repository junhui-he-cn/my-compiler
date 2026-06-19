#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "usage: $0 /path/to/compiler_demo" >&2
    exit 64
fi

compiler="$1"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

hello_run="$(${compiler} --run "${repo_root}/examples/hello.cd")"
expected_hello_run=$'answer:\n42\ntrue'
if [[ "${hello_run}" != "${expected_hello_run}" ]]; then
    echo "unexpected examples/hello.cd runtime output" >&2
    echo "expected:" >&2
    printf '%s\n' "${expected_hello_run}" >&2
    echo "actual:" >&2
    printf '%s\n' "${hello_run}" >&2
    exit 1
fi

hello_ir="$(${compiler} --ir "${repo_root}/examples/hello.cd")"

grep -Eq '^0000  v[0-9]+ = constant #[0-9]+ 40$' <<<"${hello_ir}"
grep -Eq '^0001  v[0-9]+ = constant #[0-9]+ 2$' <<<"${hello_ir}"
grep -Eq '^0002  v[0-9]+ = add v[0-9]+, v[0-9]+$' <<<"${hello_ir}"
grep -Eq '^0003  store_var @[0-9]+ answer, v[0-9]+$' <<<"${hello_ir}"
grep -Eq '^0[0-9]+  print v[0-9]+$' <<<"${hello_ir}"
if grep -Eq '(^|  )pop($| )' <<<"${hello_ir}"; then
    echo "register IR must not contain pop instructions" >&2
    printf '%s\n' "${hello_ir}" >&2
    exit 1
fi

stdin_source=$'print 1 + 2 * 3;\nprint "a" + "b";\nprint !nil;\nlet x = 10;\nx + 1;\nprint x;\n'
stdin_run="$(printf '%s' "${stdin_source}" | "${compiler}" --run)"
expected_stdin_run=$'7\nab\ntrue\n10'
if [[ "${stdin_run}" != "${expected_stdin_run}" ]]; then
    echo "unexpected stdin runtime output" >&2
    echo "expected:" >&2
    printf '%s\n' "${expected_stdin_run}" >&2
    echo "actual:" >&2
    printf '%s\n' "${stdin_run}" >&2
    exit 1
fi

stdin_ir="$(printf '%s' "${stdin_source}" | "${compiler}" --ir)"
grep -Eq 'v[0-9]+ = multiply v[0-9]+, v[0-9]+' <<<"${stdin_ir}"
grep -Eq 'v[0-9]+ = add v[0-9]+, v[0-9]+' <<<"${stdin_ir}"
grep -Eq 'v[0-9]+ = not v[0-9]+' <<<"${stdin_ir}"
grep -Eq 'store_var @[0-9]+ x, v[0-9]+' <<<"${stdin_ir}"
grep -Eq 'v[0-9]+ = load_var @[0-9]+ x' <<<"${stdin_ir}"
