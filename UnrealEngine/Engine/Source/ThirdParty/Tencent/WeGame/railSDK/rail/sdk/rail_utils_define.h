// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_UTILS_DEFINE_H
#define RAIL_SDK_RAIL_UTILS_DEFINE_H

#include "rail/sdk/rail_event.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

enum EnumRailLaunchAppType {
    kRailLaunchAppTypeGameClient = 1,
    kRailLaunchAppTypeDedicatedServer = 2,
};

enum EnumRailImagePixelFormat {
    kRailImagePixelFormatUnknown = 0,   // unknown
    kRailImagePixelFormatR8G8B8A8 = 1,  // standard RGBA in bytes order(RGBA RGBA RGBA)
};

enum EnumRailDirtyWordsType {
    kRailDirtyWordsTypeNormalAllowWords = 0,  // normal allow words
    kRailDirtyWordsTypeEvil = 1,              // illegal,can not be displayed
    kRailDirtyWordsTypeSensitive = 2,         // legal, but contain sensitive
};

enum RailUtilsCrashType {
    kRailUtilsCrashTypeUnknown = 0,
    kRailUtilsCrashTypeWindowsSEH = 1,  // exception_detail will be pointer to EXCEPTION_POINTERS
};

enum RailWarningMessageLevel {
    kRailWarningMessageLevelWarning = 0,
};

enum RailGameSettingMetadataChangedSource {
    kRailGameSettingMetadataChangedUnknow = 0,
    kRailGameSettingMetadataChangedInGame = 1,
    kRailGameSettingMetadataChangedOutGame = 2,
};

struct RailImageDataDescriptor {
    RailImageDataDescriptor() {
        image_width = 0;
        image_height = 0;
        stride_in_bytes = 0;
        bits_per_pixel = 0;
        pixel_format = kRailImagePixelFormatUnknown;
    }

    uint32_t image_width;                   // image width
    uint32_t image_height;                  // image height
    uint32_t stride_in_bytes;               // distance of bytes between two image lines.
    uint32_t bits_per_pixel;                //  bits per pixel
    EnumRailImagePixelFormat pixel_format;  // texture type
};

// Check result could be normal, sensitive and evil.
// for evil, the word being checked can not be displayed
// for sensitive, sensitive strings in it will be replace with '*' if parameter is set
// for normal, the word is ok.
struct RailDirtyWordsCheckResult {
    RailDirtyWordsCheckResult() { dirty_type = kRailDirtyWordsTypeNormalAllowWords; }

    RailString replace_string;
    EnumRailDirtyWordsType dirty_type;
};

struct RailCrashInfo {
    RailUtilsCrashType exception_type;
    void* exception_detail;

    RailCrashInfo() {
        exception_type = kRailUtilsCrashTypeUnknown;
        exception_detail = NULL;
    }
};

class RailCrashBuffer {
  public:
    virtual const char* GetData() const = 0;
    virtual uint32_t GetBufferLength() const = 0;
    virtual uint32_t GetValidLength() const = 0;
    virtual uint32_t SetData(const char* data, uint32_t length, uint32_t offset = 0) = 0;
    virtual uint32_t AppendData(const char* data, uint32_t length) = 0;
};

typedef void (*RailUtilsCrashCallbackFunction)(const RailCrashInfo* crash_info,
                                                RailCrashBuffer* comment_buffer,
                                                RailCrashBuffer* detail_buffer);

// 'uint32_t' is the security level, defined in RailWarningMessageLevel
// 'const char*' is the message
typedef void (*RailWarningMessageCallbackFunction)(uint32_t, const char*);

namespace rail_event {

struct RailGetImageDataResult : public RailEvent<kRailEventUtilsGetImageDataResult> {
    RailGetImageDataResult() {
        result = kFailure;
        user_data = "";
    }

    RailArray<uint8_t> image_data;
    RailImageDataDescriptor image_data_descriptor;
};

struct RailGameSettingMetadataChanged
    : public RailEvent<kRailEventUtilsGameSettingMetadataChanged> {
    RailGameSettingMetadataChanged() {
        result = kFailure;
        source = kRailGameSettingMetadataChangedUnknow;
        rail_id = 0;
    }

    RailGameSettingMetadataChangedSource source;
    RailArray<RailKeyValue> key_values;
};

}  // namespace rail_event

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_UTILS_DEFINE_H
