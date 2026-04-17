# Phase 2 - MyShell

## Overview
This phase extends the basic shell from Phase 1 with support for pipelines.
Multiple commands connected by `|` are executed as separate child processes, and
the output of one command becomes the input of the next command.

## Implemented Features
- Shell prompt: `CSE4100-SP-P2>`
- Built-in commands: `cd`, `exit`
- External command execution with `fork()` and `execvp()`
- Multiple chained pipes such as `cmd1 | cmd2 | cmd3`
- Quoted argument parsing for single and double quotes
- Empty input handling

## How It Works
- `eval()` checks whether the command line contains a pipe.
- A normal command is parsed with `parse_command()` and executed with `fork()`.
- A piped command is executed by `run_pipe()`.
- `run_pipe()` recursively splits the command line at `|`, creates pipes with
  `pipe()`, redirects standard input/output with `dup2()`, and runs each stage
  in a separate process.
- The parent waits for all child processes created for the pipeline.

## Build
```bash
make
```

## Run
```bash
./myshell
```

## Example Commands
```bash
ls -al | grep myshell
cat myshell.c | grep "execvp"
cat myshell.c | grep -i "abc" | sort -r
printf "a\nb\n" | grep b
```

## Notes
- Redirection (`<`, `>`) is not implemented in this phase.
- Background execution (`&`) is not implemented in this phase.
- Built-in commands inside a pipeline are treated like normal external commands.
