# pocketsh

(cleaned up school project. removed the assignment identifiers for future OS student lurkers)

A small interactive shell in C, written to understand how `fork`, `exec`,
file descriptors and signal handling actually fit together instead of just
reading about them.

It is about 300 lines and deliberately stays there. It is not a replacement
for bash: there is no scripting, no job control beyond a background `&`, no
pipes, no quoting rules.

The source is kept as a working notebook rather than a polished artifact. The
notes-to-self, the scratch glossary of `fork`/`wait`/`exec`, and the
commented-out approaches that did not survive are all still in there on
purpose, because the point of the project was the learning rather than the
shell.

## Build and run

```sh
make
./pocketsh
```

Requires a C compiler and a POSIX system (Linux or macOS).

## What it does

```text
psh> ls -la
psh> cd ~/projects
psh> wc -l < notes.txt > count.txt
psh> sleep 10 &
[bg] started 48213
psh> status
exit 0
psh> echo my pid is $$
my pid is 48207
psh> exit
```

| Feature | Notes |
| --- | --- |
| External commands | Looked up on `PATH` via `execvp` |
| `cd [dir]` | Builtin; no argument goes to `$HOME` |
| `status` | Builtin; prints the exit code or signal of the last foreground command |
| `exit` | Builtin |
| `< file` / `> file` | Input and output redirection, in either order |
| `cmd &` | Runs in the background; stdin/stdout go to `/dev/null` unless redirected |
| `$$` | Expands to the shell's own pid, anywhere in the line |
| `#` | A line starting with `#` is a comment |

## Signals

- **Ctrl-C** is ignored by the shell itself and kills only the current
  foreground child, which is what you want from a shell prompt.
- **Ctrl-Z** toggles background jobs off and on. While they are off, a
  trailing `&` is ignored and everything runs in the foreground. The handler
  uses `sigsetjmp`/`siglongjmp` to return to the top of the read loop so the
  state change is reported immediately rather than after the next command.
  A plain flag checked once per loop would have been the simpler call.
- Finished background jobs are reaped with `waitpid(-1, ..., WNOHANG)` at the
  top of each loop, so their exit notices print above the next prompt instead
  of interrupting whatever you are typing.

## Known rough edges

Kept honest rather than quietly polished:

- `fgets` has no EOF check, so Ctrl-D at the prompt re-runs the previous
  line rather than exiting. Use `exit`.
- `chdir` return values are not checked, so a bad `cd` fails silently.
- Two of the `struct sigaction` values in `main` are not zero-initialized
  before their handlers are set.
- Tokenizing splits on spaces and newlines only, with no tabs and no quoting,
  so `echo "a b"` is two arguments and keeps its quotes.

## Reading

- Arpaci-Dusseau & Arpaci-Dusseau, *Operating Systems: Three Easy Pieces*, ch. 5
- Kerrisk, *The Linux Programming Interface*, ch. 5, 6, 20, 21, 24-26
- Kernighan & Ritchie, *The C Programming Language*, ch. 5

## License

MIT. See [LICENSE](LICENSE).
