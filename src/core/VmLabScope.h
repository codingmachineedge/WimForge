#pragma once

#include <QString>

namespace wimforge::vmlab {

struct VmLabScope
{
    QString projectId;
    QString root;
    QString catalogPath;
    QString managedRoot;
    bool projectScoped = false;
};

[[nodiscard]] QString ensureProjectScopeId(const QString &projectDirectory,
                                           QString *error = nullptr);
[[nodiscard]] QString vmLabApplicationDataRoot();
[[nodiscard]] VmLabScope resolveVmLabScope(const QString &projectDirectory = {},
                                           QString *error = nullptr);

} // namespace wimforge::vmlab
