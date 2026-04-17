# Phase 3 - MyShell

## Overview
This phase extends the Phase 2 shell with background execution and job control.
Commands can be started in the background with `&`, and jobs can be listed,
resumed, moved to the foreground, or terminated.

## Implemented Features
- Shell prompt: `CSE4100-SP-P3>`
- Built-in commands: `cd`, `exit`, `jobs`, `bg`, `fg`, `kill`
- External command execution with `fork()` and `execvp()`
- Pipeline execution with `|`
- Background execution with `&`
- Job table management with job IDs such as `%1`
- Job states: foreground, background, stopped
- Signal handling for `SIGCHLD`, `SIGINT`, and `SIGTSTP`
- Process-group based control for foreground/background jobs

## Job Control Commands
- `jobs` : list current jobs
- `bg %jobid` : continue a stopped job in the background
- `fg %jobid` : move a job to the foreground
- `kill %jobid` : terminate a job

## How It Works
- `eval()` parses the command line, checks for `&`, and splits pipelines.
- `launch_pipeline()` creates child processes for each pipeline stage and puts
  them into the same process group.
- Foreground jobs are waited on with `sigsuspend()` until the job finishes or
  stops.
- Background and stopped jobs are stored in a job table.
- `SIGINT` and `SIGTSTP` are forwarded to the current foreground process group.
- `SIGCHLD` updates job state when child processes exit, stop, or continue.

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
/bin/sleep 10 &
jobs
bg %1
fg %1
kill %1
cat filename | grep -i "abc" &
```

## Notes
- Job IDs must be given in the form `%number`.
- The shell handles non-existent job IDs by printing an error message.
- Input/output redirection (`<`, `>`) is still not implemented in this phase.
