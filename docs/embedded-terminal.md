# Embedded terminal architecture

WimForge hosts interactive command sessions with the official Windows
pseudoconsole API (ConPTY). This is the same supported console-hosting
infrastructure used by the open-source Windows Terminal project. A shell is
attached to anonymous input/output pipes and a pseudoconsole process attribute;
WimForge never allocates or displays a separate console window.

The backend deliberately resolves Windows PowerShell and Command Prompt from
the protected directory returned by `GetSystemDirectoryW`. It does not use
`PATH`, `COMSPEC`, or another caller-controlled environment variable to choose
the elevated child executable. Input is written asynchronously, output is
decoded as UTF-8 while preserving ANSI/VT control sequences, terminal resize is
forwarded to ConPTY, and all queues and retained transcript data are bounded.
The child process tree is contained in a kill-on-close Windows job so forced
shutdown cannot leave terminal descendants behind.

The raw stream is retained for a full VT renderer. A separate bounded
`displayTranscript` projection removes terminal control traffic and handles
common carriage-return/backspace updates so a basic QML `TextArea` never
receives escape sequences. The QML start wrapper accepts only `default`,
`powershell`, or `cmd`; it cannot be used to select an arbitrary executable.

ConPTY is available on Windows 10 version 1809 and newer. Other platforms use
an explicit unsupported fallback; they do not silently open an external
terminal.

References and attribution:

- [Microsoft Learn: Creating a pseudoconsole session](https://learn.microsoft.com/windows/console/creating-a-pseudoconsole-session)
- [Microsoft Learn: Pseudoconsoles](https://learn.microsoft.com/windows/console/pseudoconsoles)
- [Microsoft Terminal source (MIT)](https://github.com/microsoft/terminal)

WimForge calls the operating-system ConPTY API and does not vendor Windows
Terminal source code. The Microsoft Terminal link is architectural attribution
and a useful reference implementation, not an additional runtime dependency.
