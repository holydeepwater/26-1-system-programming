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
- A normal command is parsed with `parseline()` and executed with `fork()`.
- A piped command is executed by `run_pipe()`.
- `run_pipe()` recursively splits the command line at `|`, creates pipes with
  `pipe()`, redirects standard input/output with `dup2()`, and runs each stage
  in a separate child process.
- The parent process closes unused pipe ends and waits for the pipeline child
  processes to finish.

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
ls | grep myshell
cat myshell.c | grep "run_pipe"
cat myshell.c | grep -i "pipe" | sort -r
ls | grep .c | sort
cat myshell.c | grep -i "cd"
echo "hello world" | grep world
```

## Notes
- Redirection (`<`, `>`) is not implemented in this phase.
- Background execution (`&`) is not implemented in this phase.
- Quoted arguments are supported for single and double quotes.
- Built-in commands inside a pipeline are executed in child processes,
  so `cd` inside a pipeline does not change the parent shell directory.
