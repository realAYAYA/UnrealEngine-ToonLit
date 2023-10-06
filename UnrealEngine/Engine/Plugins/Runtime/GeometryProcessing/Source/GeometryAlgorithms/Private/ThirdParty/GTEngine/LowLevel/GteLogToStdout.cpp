// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#include <ThirdParty/GTEngine/LowLevel/GteLogToStdout.h>
#include <ThirdParty/GTEngine/GTEnginePCH.h>
#include <iostream>
using namespace gte;


LogToStdout::LogToStdout(int flags)
    :
    Logger::Listener(flags)
{
}

void LogToStdout::Report(std::string const& message)
{
    std::cout << message.c_str() << std::flush;
}

