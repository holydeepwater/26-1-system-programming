# Phase 1 - MyShell

## 1. Overview
This program implements a simple Linux shell for Phase 1 of Project #2.

The shell repeatedly prints a prompt, reads a command line from standard input,
parses it into arguments, and executes the command.

In this phase, built-in commands (`cd`, `exit`) are handled directly by the shell,
while other commands are executed in a child process using `fork()` and `execvp()`.

---

## 2. Implemented Features

- Display shell prompt: `CSE4100-SP-P2>`
- Read command line using `fgets()`
- Parse input into arguments (space and tab separated)
- Built-in commands:
  - `cd` (change directory)
  - `exit` (terminate shell)
- Execute external commands:
  - `ls`, `mkdir`, `rmdir`, `touch`, `cat`, `echo`, etc.
- Parent process waits for child process to finish
- Ignore empty input

---

## 3. Program Structure

### main()
- Prints prompt
- Reads user input
- Calls `eval()` function

### eval()
- Copies input into buffer
- Parses input using `parseline()`
- Checks for built-in commands
- If not built-in:
  - Creates child process using `fork()`
  - Executes command using `execvp()`
  - Parent waits using `waitpid()`

### builtin_command()
- Handles:
  - `cd`: changes directory using `chdir()`
  - `exit`: terminates shell

### parseline()
- Removes newline character
- Splits input into tokens using space and tab
- Stores tokens in `argv[]`

---

## 4. Compilation

```bash
make