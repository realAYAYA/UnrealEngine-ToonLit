// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/LowLevel/GteLogToFile.h>
#include <ThirdParty/GTEngine/LowLevel/GteLogToStdout.h>
#include <memory>

namespace gte
{

class GTE_IMPEXP LogReporter
{
public:
    // Construction and destruction.  Create one of these objects in an
    // application for logging.  The GenerateProject tool creates such code.
    // If you do not want a particular logger, set the flags to
    // LISTEN_FOR_NOTHING and set logFile to "" if you do not want a file.
    ~LogReporter();

    LogReporter(std::string const& logFile, int logFileFlags, int logStdoutFlags);

private:
    std::unique_ptr<LogToFile> mLogToFile;
    std::unique_ptr<LogToStdout> mLogToStdout;

};

}
