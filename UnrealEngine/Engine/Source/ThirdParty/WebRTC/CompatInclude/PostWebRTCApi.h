// Copyright Epic Games, Inc. All Rights Reserved.

// Include this file after including any WebRTC headers
// Use with "PreWebRTCApi.h" included before WebRTC headers

#if defined(PLATFORM_WINDOWS)
    #if defined(WEBRTC_API_NOMINMAX_DEFINED)
        #undef NOMINMAX
        #undef WEBRTC_API_NOMINMAX_DEFINED
    #endif

    #if defined(WEBRTC_API_WIN32_LEAN_AND_MEAN_DEFINED)
        #undef WIN32_LEAN_AND_MEAN
        #undef WEBRTC_API_WIN32_LEAN_AND_MEAN_DEFINED
    #endif
#endif