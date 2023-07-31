// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#include <ThirdParty/GTEngine/LowLevel/GteLogToFile.h>
#include <ThirdParty/GTEngine/GTEnginePCH.h>
#include <fstream>
using namespace gte;


LogToFile::LogToFile(std::string const& filename, int flags)
    :
    Logger::Listener(flags),
    mFilename(filename)
{
    std::ofstream logFile(filename);
    if (logFile)
    {
        // This clears the file contents from any previous runs.
        logFile.close();
    }
    else
    {
        // The file cannot be opened.  Use a null string for Report to know
        // not to attempt opening the file for append.
        mFilename = "";
    }
}

void LogToFile::Report(std::string const& message)
{
    if (mFilename != "")
    {
        // Open for append.
        std::ofstream logFile(mFilename,
            std::ios_base::out | std::ios_base::app);
        if (logFile)
        {
            logFile << message.c_str();
            logFile.close();
        }
        else
        {
            // The file cannot be opened.  Use a null string for Report not
            // to attempt opening the file for append on the next call.
            mFilename = "";
        }
    }
}

