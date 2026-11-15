# LSL issues found during the LuxAI migration

Tested with `/usr/bin/lsl version`: **0.10.6**, Linux x86_64, 2026-10-25.
These are local findings, not reports sent to any external service.

## Instance parameters in methods

Reproducer: `lsl tests_lsl/repros/instance_parameter.lsl`.

A direct `item.value` succeeds. Passing the same instance as the `item`
parameter of `Receiver.inspect` and evaluating `item.value` fails with
`AttributeError: member value requires a class or instance`. Calling a method
on that parameter also failed in the original LuxAI port. This prevents a
straightforward port of composition between Python classes.

Workaround in the port: pass explicit Dictionary records between modules;
use module functions for operations on those records. No installed compiler
or runtime has been modified.

## Documented file deletion is unavailable

Reproducer (writes only the supplied disposable file):

```sh
lsl tests_lsl/repros/remove_path.lsl /tmp/luxai-lsl-remove-path-repro
```

The installed `reference.md` lists `remove_path(path)` for removing a file or
empty directory. Compilation/execution reports `unknown native function:
remove_path`. Workaround: explicit `/usr/bin/rm` argv for application-owned
files, without shell evaluation.

## Conditional expression in the bundled manual is rejected

Reproducer: `lsl tests_lsl/repros/conditional_from_manual.lsl`.

`03-functions-and-classes.md` demonstrates `"✓" if condition else " "`.
The installed compiler rejects this syntax with E1201. This is a compiler/
documentation mismatch; the port uses explicit `if` statements.

## Nested function diagnostic

Reproducer: `lsl tests_lsl/repros/nested_function_diagnostic.lsl`.

The frontend reports `KeyError: key not found` for this nested function,
instead of a source-level diagnostic. Whether closures are intended to be
supported is separate from the missing diagnostic. The port passes callback
state explicitly and does not depend on closures.

## Unversioned glibc `realpath` binding (cause under investigation)

```sh
lsl compile --unsafe --target linux-x86_64-gnu tests_lsl/repros/realpath_abi.lsl /tmp/lsl-realpath-repro
/tmp/lsl-realpath-repro
readelf -V /tmp/lsl-realpath-repro
```

`realpath("/tmp", NULL)` returns NULL (`errno == EINVAL` in the isolated debug
probe), whereas the same call with a 4096-byte caller-provided buffer succeeds.
The generated ELF has no symbol version information. On this system glibc
provides both `realpath@@GLIBC_2.3` and `realpath@GLIBC_2.2.5`. Binding the
compatibility implementation is a plausible explanation, **not yet confirmed**.
The port uses the explicit output-buffer form, which resolves symlinks correctly
on this machine; it does not substitute lexical path normalization for realpath.

## `None` changes to integer zero in a native integer return path

Reproducer: `tests_lsl/repros/optional_native_return.lsl` (compile with `--unsafe`).

In LSL 0.10.6, a function with one branch returning `None` and another returning
an integer from `__lsl_qbe_call0` produces `0`, `Int`, and `False` for
`optional(True)`, `type(optional(True))`, and `optional(True) == None`.
Expected: `None`, the None type, and `True`. This was first observed while
polling a child process: the missing result was mistaken for exit code zero.
The process boundary now returns a Dictionary with explicit `done` and `code`
fields. Its timeout and exit-status tests cover this path.

## Documented global `args` is unavailable

Reproducer: `lsl tests_lsl/repros/argv_from_manual.lsl`.

The bundled `reference.md` documents a global `args` List. Evaluation reports
`NameError: unknown native variable: args`. The native helper entry points use
`arg_count()` and `arg(index)` instead, both of which work on this installation.

## Quadratic text slicing through an untyped parameter

Reproducer: `lsl tests_lsl/repros/any_text_slice_performance.lsl 20000`.

`value[0:]` on a generic function parameter took about 0.24 seconds for 20,000
ASCII characters; a new local `text = String(value)` followed by `text[0:]`
took about 0.00007 seconds. At 200,000 characters the generic case exceeded
the two-second probe deadline, while the String case took about 0.00055 s;
at 2,000,000 characters the String case took about 0.007 s. Direct builtin
`split()` and statically known text slicing do not show this slowdown.

The native HTML parser hit this path on a bounded 2 MiB response. The port
uses explicit String locals and integer slice offsets in its text and regex
boundaries, and includes a large HTML regression case.

## `glib.pump()` fails after dispatching an event

Reproducer:

```sh
lsl compile --unsafe --target linux-x86_64-gnu tests_lsl/repros/glib_pump_pass.lsl /tmp/luxai-glib-pump-repro
/tmp/luxai-glib-pump-repro
```

Scheduling an idle callback and calling the bundled `glib.pump()` fails with
`NameError: unknown native variable: pass` at `stdlib/glib.lsl:205`. The empty
loop body in that function contains `pass`, which this runtime evaluates as
an unknown variable. The application has a project-local GLib pump whose
loop uses `continue`; the installed standard library is unchanged.

## Documented `cwd()` is unavailable

Reproducer: `lsl tests_lsl/repros/cwd_from_manual.lsl`.

`developer_guides/07-files-io-and-processes.md` documents `cwd()` for the
current directory. It fails with `unknown native function: cwd`. The port
calls GLib's `g_get_current_dir` through `platform.current_dir()` instead.

## `String.strip()`, `lstrip()` and `rstrip()` are quadratic

Reproducer: `lsl tests_lsl/repros/strip_performance.lsl` (2026-10-26, LSL 0.10.6).

Stripping a string that has nothing to strip takes 0.005 s for 2,048 characters,
0.015 s for 4,096, 0.048 s for 8,192 and 0.18 s for 16,384 (quadruples with each
doubling). It does not matter whether the receiver is typed or a character set
is passed. `split`, `replace`, `find` and `startswith` on the same text are
linear. In the native LuxAI this made every 4 KiB SSE event cost about 16 ms
(a 5 MiB answer would take minutes) and made checks on long answers, tool
outputs, text attachments and documentation sections slow.

Workaround: `platform.strip(text, characters, left, right)` walks the UTF-8
buffer natively; `common.strip_text`, `lstrip_text` and `rstrip_text` wrap it
and are used on all paths that can see long text. A regression test streams a
5 MiB answer.

## Integer results of C calls are not sign-extended

Reproducer:

```sh
lsl compile --unsafe --target linux-x86_64-gnu tests_lsl/repros/int_return_sign.lsl /tmp/lsl-int-sign
/tmp/lsl-int-sign
```

`open()` failing returns the C `int` -1, but `__lsl_qbe_call3` yields
4294967295, so `handle < 0` is False. Strictly this is the x86-64 ABI (only
the low 32 bits of `rax` are defined), but the internal call builtins give no
way to declare the return width and the bundled documentation does not warn
about it. In the port the failed `open()` went unnoticed: `read()` on the
bogus descriptor reported "Bad file descriptor" instead of the real reason, and
an exclusive create could not detect an existing file. All `open()`/`poll()`
results in the port now go through `__lsl_qbe_s32`.

## C `float` parameters cannot be passed through the call builtins

Reproducer (needs an isolated X display):

```sh
lsl compile --unsafe --target linux-x86_64-gnu tests_lsl/repros/float_argument.lsl /tmp/lsl-float-arg
env -u DISPLAY xvfb-run -a /tmp/lsl-float-arg
```

`__lsl_qbe_call2(gtk_label_set_xalign, label, Float(0.5))` leaves xalign at 0.0.
The builtin passes an LSL Float as a C `double`; a function declared with
`float` reads only the low 32 bits (all zero for 0.5). `double` parameters
(e.g. `gtk_level_bar_set_value`) work. In the port every centred label (avatar
letter, placeholders, welcome texts, status line) was silently left-aligned.
Workaround: set such properties through a `G_TYPE_DOUBLE` GValue and
`g_object_set_property`, which converts to `float` (`gtk_ext.set_float_property`).
