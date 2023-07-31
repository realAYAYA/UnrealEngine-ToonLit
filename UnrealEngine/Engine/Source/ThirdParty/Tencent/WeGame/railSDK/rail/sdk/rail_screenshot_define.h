// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_SCREENSHOT_DEFINE_H
#define RAIL_SDK_RAIL_SCREENSHOT_DEFINE_H

#include "rail/sdk/base/rail_define.h"
#include "rail/sdk/rail_event.h"
#include "rail/sdk/rail_user_space_define.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

namespace rail_event {

struct TakeScreenshotResult : public RailEvent<kRailEventScreenshotTakeScreenshotFinished> {
    TakeScreenshotResult() {
        result = kFailure;
        image_file_size = 0;
        thumbnail_file_size = 0;
    }
    RailString image_file_path;  // utf8
    uint32_t image_file_size;
    RailString thumbnail_filepath;  // utf8
    uint32_t thumbnail_file_size;
};

struct ScreenshotRequestInfo : public RailEvent<kRailEventScreenshotTakeScreenshotRequest> {};

struct PublishScreenshotResult : public RailEvent<kRailEventScreenshotPublishScreenshotFinished> {
    PublishScreenshotResult() { result = kFailure; }
    SpaceWorkID work_id;  // screenshot spacework id
};

}  // namespace rail_event

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_SCREENSHOT_DEFINE_H
