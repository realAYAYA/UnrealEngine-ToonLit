// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_SCREENSHOT_H
#define RAIL_SDK_RAIL_SCREENSHOT_H

#include "rail/sdk/base/rail_component.h"
#include "rail/sdk/base/rail_define.h"
#include "rail/sdk/rail_screenshot_define.h"
#include "rail/sdk/rail_user_space_define.h"

namespace rail {

#pragma pack(push, RAIL_SDK_PACKING)

class IRailScreenshot;
class IRailScreenshotHelper {
  public:
    // create a IRailScreenshot object from the raw image data with RGB format.
    // call IRailComponent::Release when you don't need the IRailScreenshot object.
    virtual IRailScreenshot* CreateScreenshotWithRawData(const void* rgb_data,
                                uint32_t len,
                                uint32_t width,
                                uint32_t height) = 0;

    // create a IRailScreenshot object from the local image file.
    // if a thumbnail_file is provided, it must be 200 pixels wide and the same aspect ratio
    // as the screenshot, jpg or png format, otherwise rail will generate one.
    // call IRailComponent::Release when you don't need the IRailScreenshot object.
    // supports image formats:
    // jpeg baseline & progressive (12 bpc/arithmetic not supported, same as stock IJG lib)
    // png 1/2/4/8-bit-per-channel (16 bpc not supported)
    // bmp non-1bpp, non-RLE
    virtual IRailScreenshot* CreateScreenshotWithLocalImage(const RailString& image_file,
                                const RailString& thumbnail_file) = 0;

    // take a screenshot,
    // the call back event is kRailEventScreenshotTakeScreenshotFinished when finished
    virtual void AsyncTakeScreenshot(const RailString& user_data) = 0;

    // set whether the game handles the screenshots operation or not when player presses the
    // hot key.
    // if true is set, the kRailEventScreenshotTakeScreenshotRequest will be sent if the player
    // presses the hot key. otherwise, rail will do the job and
    // kRailEventScreenshotTakeScreenshotFinished will be sent when finished.
    virtual void HookScreenshotHotKey(bool hook) = 0;

    // check if the screenshot hot key is hooked
    virtual bool IsScreenshotHotKeyHooked() = 0;
};

class IRailScreenshot : public IRailComponent {
  public:
    // set location metadata of the screenshot
    virtual bool SetLocation(const RailString& location) = 0;

    // set accessable users to the screenshot.
    // clear accessable users if the size is zero
    // failed and return flase if a rail id is invalid.
    virtual bool SetUsers(const RailArray<RailID>& users) = 0;

    // associate publish files to the screenshot
    // clear accessable published files if the size is zero
    // failed and return flase if a work id is invalid.
    virtual bool AssociatePublishedFiles(const RailArray<SpaceWorkID>& work_files) = 0;

    // publish current screenshot file to player's user space as a work file
    // the call back event is kRailEventScreenshotPublishScreenshotFinished when finished.
    virtual RailResult AsyncPublishScreenshot(const RailString& work_name,
                        const RailString& user_data) = 0;
};

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_SCREENSHOT_H
