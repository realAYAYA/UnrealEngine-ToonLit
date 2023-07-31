// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/Settings.h"

#include "Misc/AssertionMacros.h"
#include "MuR/SettingsPrivate.h"

namespace mu
{
    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    Settings::Settings()
    {
        m_pD = new Settings::Private();
    }


    //---------------------------------------------------------------------------------------------
    Settings::~Settings()
    {
        check( m_pD );
        delete m_pD;
        m_pD = 0;
    }


    //---------------------------------------------------------------------------------------------
    Settings::Private* Settings::GetPrivate() const
    {
        return m_pD;
    }


    //---------------------------------------------------------------------------------------------
    void Settings::SetProfile( bool bEnabled )
    {
        m_pD->m_profile = bEnabled;
    }


    //---------------------------------------------------------------------------------------------
    void Settings::SetStreamingCache( uint64 bytes )
    {
        m_pD->m_streamingCacheBytes = bytes;
    }


    //---------------------------------------------------------------------------------------------
    void Settings::SetImageCompressionQuality( int quality )
    {
        m_pD->m_imageCompressionQuality = quality;
    }

}
