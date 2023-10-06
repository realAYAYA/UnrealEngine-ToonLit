// Copyright Epic Games, Inc. All Rights Reserved.

// Include this file before including any WebRTC headers
// On Windows WebRTC is built with NOMINMAX and WIN32_LEAN_AND_MEAN
// macros defined as a default for its modules. We need to do the same
// before including WebRTC headers so we don't run into issues like 'min'
// being defined as a macro. We also need avoid clashes with "Windows/MinWindows.h"
// as this also defines these macros without checking if they already exist
// and will trigger warnings if included after this header without also including
// "PostWebRTCApi.h" afterwards

#if defined(PLATFORM_WINDOWS)
    #if !defined(NOMINMAX)
        #define NOMINMAX
        #define WEBRTC_API_NOMINMAX_DEFINED
    #endif

    #if !defined(WIN32_LEAN_AND_MEAN)
        #define WIN32_LEAN_AND_MEAN
        #define WEBRTC_API_WIN32_LEAN_AND_MEAN_DEFINED
    #endif
#endif
