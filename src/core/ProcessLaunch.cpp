#include "ProcessLaunch.h"

#include <QProcess>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace wimforge {

void configureProcessWithoutConsole(QProcess &process)
{
#ifdef Q_OS_WIN
    const QProcess::CreateProcessArgumentModifier previous =
        process.createProcessArgumentsModifier();
    process.setCreateProcessArgumentsModifier(
        [previous](QProcess::CreateProcessArguments *arguments) {
            if (previous)
                previous(arguments);
            arguments->flags |= CREATE_NO_WINDOW;
        });
#else
    static_cast<void>(process);
#endif
}

} // namespace wimforge
