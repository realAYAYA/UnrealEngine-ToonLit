// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#include <ThirdParty/GTEngine/LowLevel/GteLogReporter.h>
#include <ThirdParty/GTEngine/GTEnginePCH.h>
using namespace gte;

LogReporter::~LogReporter()
{
    if (mLogToStdout)
    {
        Logger::Unsubscribe(mLogToStdout.get());
    }

    if (mLogToFile)
    {
        Logger::Unsubscribe(mLogToFile.get());
    }


}

LogReporter::LogReporter(std::string const& logFile, int logFileFlags,
	int logStdoutFlags)
    :
    mLogToFile(nullptr),
    mLogToStdout(nullptr)

{
    if (logFileFlags != Logger::Listener::LISTEN_FOR_NOTHING)
    {
        mLogToFile = std::make_unique<LogToFile>(logFile, logFileFlags);
        Logger::Subscribe(mLogToFile.get());
    }

    if (logStdoutFlags != Logger::Listener::LISTEN_FOR_NOTHING)
    {
        mLogToStdout = std::make_unique<LogToStdout>(logStdoutFlags);
        Logger::Subscribe(mLogToStdout.get());
    }


}
