#pragma once

class QProcess;

namespace wimforge {

// Keeps captured child processes in the background on Windows without
// changing QProcess' stdout/stderr pipe setup. This is intentionally not for
// detached provider consoles, which must remain visible to the user.
void configureProcessWithoutConsole(QProcess &process);

} // namespace wimforge
