# Embedded Terminal

The Embedded Terminal is WimForge's twelfth desktop route. It hosts an
interactive Windows shell inside the application so commands, output, status,
and cancellation stay in one window. Its startup page ID is `terminal`.

![Embedded Terminal in its safe ready state, showing the administrator-shell selector, neutral working directory, empty bounded ConPTY viewport, command input, and inactive stop controls](https://raw.githubusercontent.com/Ding-Ding-Projects/WimForge/main/docs/screenshots/embedded-terminal.png)

## What is embedded

WimForge uses Microsoft's documented Windows pseudoconsole API, ConPTY. This is
the same operating-system infrastructure used by the open-source Windows
Terminal project. WimForge does not vendor, embed, fork, or link Windows
Terminal source, and Windows Terminal is not a runtime dependency.

The shell is connected to anonymous input/output pipes and a pseudoconsole
process attribute. No console is allocated, so starting a session does not pop
open PowerShell, Command Prompt, `conhost`, or another terminal window. Output
is decoded and displayed in the QML page.

ConPTY requires Windows 10 version 1809 or newer. On an unsupported platform or
Windows version, the page reports **Unsupported**; it does not silently fall
back to an external console.

## Start and use a session

1. Open **Embedded terminal** in the navigation rail.
2. Choose **Default (PowerShell)**, **Windows PowerShell**, or **Command
   Prompt**.
3. Set a working directory with the adjacent folder picker. If the field is empty and a project is open,
   WimForge uses the project root.
4. Select **Start**, type a command, and press Enter or **Send**.

工作資料夾旁邊有 folder picker，唔使手打完整路徑；留空兼有開緊工程時，WimForge 會用工程根目錄。

The page resizes ConPTY when its output area changes. Up and Down recall up to
100 commands from the current UI session. **Ctrl+C** sends the interrupt byte;
**Copy** copies displayed output; **Clear** clears the retained transcript.
Exit code, normal/crashed/terminated state, and errors remain visible after the
shell ends.

The UI is a bounded plain-text projection, not a complete Windows Terminal
renderer. The backend retains raw UTF-8/VT output for a future full renderer,
while the displayed transcript removes ANSI/VT control traffic and applies
common carriage-return and backspace updates. Complex full-screen terminal
applications may therefore look different from a dedicated terminal emulator.

## Elevation boundary

WimForge's Windows application manifest requests administrator elevation at
startup. The embedded shell inherits that token, so commands entered here run
as administrator by default. The terminal is not a sandbox, not a lower-trust
helper, and not an undoable project editor. A command can change the host,
network, registry, disks, services, accounts, or files outside the active
project.

!!! danger "Review before sending"
    Treat every line as an elevated administration command. Verify paths and
    quoting, avoid pasting untrusted instructions, and keep credentials and
    secrets out of the transcript and clipboard. WimForge history cannot undo
    arbitrary shell effects.

The route does not auto-run repository commands, dependency repair, or VM
commands. It starts a shell only when the user selects **Start** and sends input
only when the user submits it.

## Trusted shell selection

The QML-facing start API accepts only `default`, `powershell`, or `cmd`
(`command-prompt` is the internal synonym). It cannot select an arbitrary
executable.

PowerShell and Command Prompt are resolved from the protected Windows system
directory returned by `GetSystemDirectoryW`. WimForge does not trust `PATH`,
`COMSPEC`, the working directory, or a caller-controlled executable path when
choosing the elevated shell. Commands typed *inside* that trusted shell still
use the shell's normal resolution rules, so qualify security-sensitive tools
with known paths when appropriate.

## Process lifetime and stopping

Input and output use worker threads so a slow child or full pipe cannot block
the UI thread. The shell and its descendants are assigned to a Windows job with
kill-on-close containment.

- **Stop** requests a graceful exit and waits three seconds before forcing the
  remaining process tree to stop.
- **Force stop** requires a separate confirmation and immediately terminates
  the shell plus every process it started. Unsaved child-process work can be
  lost.
- Closing the session/application releases the job, preventing descendants
  from being left behind by a forced shutdown.

An application launched through another broker or service may be outside the
job boundary imposed on the terminal process tree. Do not treat containment as
a general host rollback mechanism.

## Bounded input, output, and transcript

The backend deliberately caps retained and queued data:

| Buffer | Default bound | Behavior at the bound |
| --- | ---: | --- |
| Retained raw/display transcript | 1 MiB | Old content is truncated and the page marks the transcript truncated |
| Pending output awaiting the UI | 256 KiB | Excess bytes are discarded under backpressure and counted |
| Pending input awaiting the shell | 64 KiB | Further input is refused rather than growing memory without limit |

The status card reports truncation and the cumulative dropped-output byte
count. A successful command can produce incomplete visible output if those
guards activate; redirect large output to an intentional file and inspect it
with an appropriate bounded tool.

UTF-8 sequences split across pipe reads are carried forward before decoding.
Raw VT sequences are preserved separately, while the display projection strips
escape traffic so terminal controls cannot be injected into the surrounding
QML `TextArea`.

## External-window policy

Command-line tasks started by WimForge use hidden-window process flags while
retaining stdout/stderr pipes, so normal app work reports output in-app rather
than flashing a console. The Embedded Terminal follows the stronger ConPTY
model described above.

The explicit **Open console** action in [Virtual Machine Lab](Virtual-Machine-Lab)
is different: it opens the hypervisor's graphical VM console because that GUI
is the requested destination. It does not open an external command terminal.

## Troubleshooting

### The page says Unsupported

Confirm the host is Windows 10 version 1809 or newer. WimForge will not use an
external-console fallback on older systems.

### A session fails immediately

Check that the requested working directory exists and is accessible to the
elevated process. The status card and error text distinguish startup failure
from a shell that launched and then exited.

### Output is missing

Look for the transcript-truncated warning or a non-zero dropped-byte count.
Large or fast output can intentionally exceed the bounded queues. Redirect it
to a file when complete output is required.

### A full-screen program renders poorly

The current QML view is an ANSI-safe text projection, not a full VT renderer.
Use a non-interactive output mode, or run that specific program in a dedicated
terminal when its rich screen model is essential.

### Stop did not finish promptly

Graceful stop allows three seconds. Use **Force stop** only after reviewing the
warning; it terminates the contained process tree and can discard work.

For implementation detail and upstream references, see the
[embedded-terminal architecture](../embedded-terminal.md). Also see
[Virtual Machine Lab](Virtual-Machine-Lab), [Safety and Recovery](Safety-and-Recovery),
and [Troubleshooting](Troubleshooting).

---

[← Virtual Machine Lab](Virtual-Machine-Lab) · [Settings →](Settings)
