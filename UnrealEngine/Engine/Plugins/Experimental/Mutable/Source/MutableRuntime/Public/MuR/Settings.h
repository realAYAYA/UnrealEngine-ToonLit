// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"


namespace mu
{
	// Forward references
    class Settings;

    using SettingsPtr=Ptr<Settings>;
    using SettingsPtrConst=Ptr<const Settings>;



    //! \brief Settings class that can be used to create a customised System instance.
    //! \ingroup runtime
    class MUTABLERUNTIME_API Settings : public RefCounted
    {
    public:

        //! Create new settings with the default values as explained in each method below.
        Settings();

        //! Record internal profiling data.
        //! Disabled by default.
        void SetProfile( bool bEnabled );

        //! Limit the maximum memory in bytes used to store streaming data. A low value will force
        //! more streaming and higher instance construction times, but will use less memory while
        //! building objects.
        //! Defaults to 0, which disables the limit.
        void SetStreamingCache( uint64 bytes );

        //! Set the quality for the image compression algorithms.
        //! \param quality Quality level of the compression. Higher quality increases the instance
        //! generation time. The value is in the range 0 to 4 with 0 being the fastest. It defaults
        //! to 0, which is the fastes for runtime. Tools may want ot set it higher, and low
        //! performance profiles to lower.
        //! Genreal rules are:
        //! 0 - Fastest for runtime
        //! 1 - Best for runtime
        //! 2 - Fast for tools
        //! 3 - Best for tools
        //! 4 - Maximum, with no time limits.
        //! Visit https://work.anticto.com/w/mutable/core/guide/ for
        //! more information.
        void SetImageCompressionQuality( int quality );

        //-----------------------------------------------------------------------------------------
        // Interface pattern
        //-----------------------------------------------------------------------------------------
        class Private;

        Private* GetPrivate() const;

        Settings( const Settings& ) = delete;
        Settings( Settings&& ) = delete;
        Settings& operator=(const Settings&) = delete;
        Settings& operator=(Settings&&) = delete;

    protected:

        //! Forbidden. Manage with the Ptr<> template.
        ~Settings() override;

    private:

        Private* m_pD;

    };


}

