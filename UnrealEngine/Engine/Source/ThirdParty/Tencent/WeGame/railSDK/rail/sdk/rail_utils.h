// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_UTILS_H
#define RAIL_SDK_RAIL_UTILS_H

#include "rail/sdk/base/rail_define.h"
#include "rail/sdk/rail_utils_define.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

class IRailUtils {
  public:
    // @desc Get the number of seconds elapsed since the game is started
    // @return The number seconds elapsed since the game is started
    virtual uint32_t GetTimeCountSinceGameLaunch() = 0;

    // @desc Get the number of seconds elapsed since the computer is started
    // @return The number of seconds elapsed since the computer is started
    virtual uint32_t GetTimeCountSinceComputerLaunch() = 0;

    // @desc Get Rail server time calculated as the number of seconds since Jan 1, 1970
    // @return The number of seconds elapsed since Jan 1, 1970
    virtual uint32_t GetTimeFromServer() = 0;

    // @desc Get image data of the specified path asynchronously
    // 'image_path' set as a URL like "http://xxx.xxx.xx/test.png" will lead to a download.
    // If a local 'image_path' is used, you will get local image information.
    // 'scale_to_width' and 'scale_to_height' with non-zero values will scale the retrieved images,
    // otherwise the resolution will keep unchanged.
    // Check RailGetImageDataResult for image data and info. The pixel data are in RGBA format.
    // @return Returns kSuccess on success
    virtual RailResult AsyncGetImageData(const RailString& image_path,
                        uint32_t scale_to_width,   // 0 for no scale
                        uint32_t scale_to_height,  // 0 for no scale
                        const RailString& user_data) = 0;

    // @desc Get error string for a given error code
    // @param result The error code
    // @param error_string The error string for the given error code
    virtual void GetErrorString(RailResult result, RailString* error_string) = 0;

    // @desc Filter out sensitive words for a string
    // The max length for 'words' is kRailCommonUtilsCheckDirtyWordsStringMaxLength
    // @param words The string to filter out sensitive words
    // @param replace_sensitive If true '*' will be used to replace sensitive words
    // @param check_result The result string after filtering
    // @return Returns kSuccess on success
    virtual RailResult DirtyWordsFilter(const RailString& words,
                        bool replace_sensitive,
                        RailDirtyWordsCheckResult* check_result) = 0;

    // @desc Get the type of Rail platform
    // @return kRailPlatformTGP or kRailPlatformQQGame
    virtual EnumRailPlatformType GetRailPlatformType() = 0;

    // @desc
    virtual RailResult GetLaunchAppParameters(EnumRailLaunchAppType app_type,
                        RailString* parameter) = 0;

    // @desc Return the language code of WeGame client. You can call
    // IRailGame::GetPlayerSelectedLanguageCode interface instead. This will be more proper.
    virtual RailResult GetPlatformLanguageCode(RailString* language_code) = 0;

    // @desc Register a callback function for Rail crash. On crashes, 'callback_func' will
    // be called, so that developers can use the buffer sent to the callback
    // to write more info about the crash. Callback should be designed with following rules
    //    1. Use of the application heap is forbidden.
    //    2. Resource allocation must be limited.
    //    3. Library code (like STL) that may lead to heap allocation should be avoided.
    // @param callback_func The callback function to register
    // @return Returns kSuccess on success
    virtual RailResult RegisterCrashCallback(
                        const RailUtilsCrashCallbackFunction callback_func) = 0;

    // @desc Unregister the crash callback function registered earlier
    // @return Returns kSuccess on success
    virtual RailResult UnRegisterCrashCallback() = 0;

    // @desc Set the callback function for warning message. It works only in development mode.
    // @return Returns kSuccess on success
    virtual RailResult SetWarningMessageCallback(RailWarningMessageCallbackFunction callback) = 0;

    // @desc Get two-letter country code according to the IP address used for login
    // The county code format follows ISO 3166-1-alpha-2, i.e. 'CN', 'US', 'HK' etc.
    // The result will be cached to local memory after the first call. Later calls will directly
    // use the cached country code.
    // @param country_code The country code following ISO 3166-1-alpha-2
    // @return Returns kSuccess on success
    virtual RailResult GetCountryCodeOfCurrentLoggedInIP(RailString* country_code) = 0;
};

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_UTILS_H
