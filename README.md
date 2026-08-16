# seashell

A small Unix shell written in C, featuring job control, pipes, I/O redirection, tab completion, and environment variable expansion.

## Features

Like any basic shell, Seashell gives you a REPL that can be used to run programs and move around directories.
On top of this, there are also the following:

### Job Control

Seashell implements several job control features, including foreground/background execution and killing/stopping/resuming jobs.
By default, jobs are run in the foreground and can be run in the background by adding `&` to the end of the input.

| Command | Function                                                                                                          |
| ------- | ----------------------------------------------------------------------------------------------------------------- |
| `jobs`  | Prints running/stopped jobs and their IDs.                                                                        |
| `fg`    | Resumes a stopped or background job in the foreground. Defaults to the most recent job if no job ID is specified. |
| `bg`    | Resumes a stopped job in the background. Defaults to the most recent job if no job ID is specified.               |
| `kill`  | Sends the specified signal to a process or job ID.                                                                |
| Ctrl-C  | Interrupts the current foreground job.                                                                            |
| Ctrl-Z  | Stops the current foreground job.                                                                                 |

### Pipes and I/O Redirection

I/O can be piped and redirected, e.g.:

```shell
cat a.txt b.txt | grep "hello world" -o > c.txt
```

The following redirection operators are available:

| Operator | File                         |
| -------- | ---------------------------- |
| `<`      | `stdin`                      |
| `>`      | `stdout`                     |
| `>>`     | `stdout` (append)            |
| `2>`     | `stderr`                     |
| `2>>`    | `stderr` (append)            |
| `&>`     | `stdout` & `stderr`          |
| `&>>`    | `stdout` & `stderr` (append) |

By default, there can only be a maximum of three redirections per job (not counting pipes).
This number can be changed by redefining the `MAX_REDIRECTS` macro in `options.h` and rebuilding the project.

Note also that later redirection operators can overwrite previous ones, e.g.:
```shell
some-cmd &> a.txt > b.txt # stdout redirected to b.txt, stderr to a.txt
some-cmd > a.txt &> b.txt # stdout and stderr redirected to b.txt (no redirects to a.txt)
```

### Environment variables

Environment variables are expanded during parsing.
For example,
```shell
cd $HOME
```
becomes
```shell
cd /home/username
```

Similarly, `$?` expands to the previous process' exit code, e.g.:
``` shell
echo $? # prints the most recently completed process' exit code
```

### Line editing

Several quality-of-life line editing features are available, such as tab completion of file names and root programs, command history (persistent across sessions), and moving through the input with the arrow keys.
You can check the [GNU readline documentation](https://tiswww.cwru.edu/php/chet/readline/rltop.html#Documentation) for a full list of features.

## Installation

Make sure you have the necessary dependencies installed and use the provided makefile to build the project:

``` shell
make
```

Then run ```./seashell``` to launch a REPL session.

### Dependencies

The only dependency outside of libc is the GNU readline library for the line editing functionality.
Since it is also used by Bash, there is a good chance that it is already installed on your system.
If not, you can install it through your package manager, e.g.:
``` shell
# Arch
pacman -S readline

# Ubuntu / Debian / WSL
apt install libreadline-dev
```

### Tests

The test suite is written in Python and can be run using Pytest:
``` shell
pytest
```

This will do a clean build of the project and run all the tests.

While these tests are sufficient for the scope of this project, if I were to start over from scratch I'd use a shell script or a library like `pexpect` instead, since they are better suited to interactive terminal applications.

## Resources and Acknowledgements
- [This blog post](https://healeycodes.com/building-a-shell) by Andrew Healey initially inspired me to write a shell. The one he builds in the post is pretty minimal, but it's a good starting point. Also, be sure to check out his [GitHub repo](https://github.com/healeycodes/andsh).
- [CS:APP Chapter 8 and 10](https://csapp.cs.cmu.edu/) explain pretty well how shells and I/O work, including the underlying OS concepts like processes, signals, and files and the syscalls used to manipulate them. While you're at it, read the rest of the book too, it's really good.