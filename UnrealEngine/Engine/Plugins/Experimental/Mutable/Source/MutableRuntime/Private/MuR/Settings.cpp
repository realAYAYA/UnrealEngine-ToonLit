// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/Settings.h"

#include "Misc/AssertionMacros.h"

namespace mu
{
  
	Settings::Settings()
    {
    }


    Settings::~Settings()
    {
    }


    //---------------------------------------------------------------------------------------------
    void Settings::SetProfile( bool bEnabled )
    {
        bProfile = bEnabled;
    }


    //---------------------------------------------------------------------------------------------
    void Settings::SetWorkingMemoryBytes( uint64 Bytes )
    {
        WorkingMemoryBytes = Bytes;
    }


    //---------------------------------------------------------------------------------------------
    void Settings::SetImageCompressionQuality( int32 Quality )
    {
        ImageCompressionQuality = Quality;
    }

}
