// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Settings.h"

namespace mu
{

    //---------------------------------------------------------------------------------------------
    //! Private Settings implementation
    //---------------------------------------------------------------------------------------------
    class Settings::Private : public Base
    {
    public:
        bool m_profile = false;
        uint64 m_streamingCacheBytes = 0;
        int32 m_imageCompressionQuality = 0;
    };


}
