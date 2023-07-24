// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/LowLevel/GteLogger.h>

namespace gte
{

class GTE_IMPEXP LogToFile : public Logger::Listener
{
public:
    LogToFile(std::string const& filename, int flags);

private:
    virtual void Report(std::string const& message);

    std::string mFilename;
};

}
