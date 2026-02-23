#include "depcheck.h"
#include <QStandardPaths>

bool DepCheck::yayInstalled()
{
    return !QStandardPaths::findExecutable("yay").isEmpty();
}

bool DepCheck::reflectorInstalled()
{
    return !QStandardPaths::findExecutable("reflector").isEmpty();
}

bool DepCheck::systemUpdatePacmanInstalled()
{
    return !QStandardPaths::findExecutable("schedule-system-update").isEmpty();
}
