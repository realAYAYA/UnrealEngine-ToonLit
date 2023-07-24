// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_SMALL_OBJECT_SERVICE_H
#define RAIL_SDK_RAIL_SMALL_OBJECT_SERVICE_H

#include "rail/sdk/rail_small_object_service_define.h"

namespace rail {

class IRailSmallObjectServiceHelper {
  public:
    virtual ~IRailSmallObjectServiceHelper() {}

    // download the Object at those indexes
    // @callback RailSmallObjectDownloadResult
    // @param index index list of object you want to download, 0 <= index <= 15
    // @param used for asynchronous interface, will pass to user_data memeber of callback event
    // struct
    // @return kSuccess if success, otherwise failed
    virtual RailResult AsyncDownloadObjects(const RailArray<uint32_t>& indexes,
                        const RailString& user_data) = 0;

    // get content of this object
    // @param index the index of object, must no larger than 16
    // @param content content of this object, it could be binary data(use content.c_str() and
    // content.size())
    // @return kSuccess if success, otherwise failed
    virtual RailResult GetObjectContent(uint32_t index, RailString* content) = 0;

    // query all objects state
    // @callback RailSmallObjectStateQueryResult
    // @param used for asynchronous interface, will pass to user_data memeber of callback event
    // struct
    // @return kSuccess if success, otherwise failed
    virtual RailResult AsyncQueryObjectState(const RailString& user_data) = 0;
};
};  // namespace rail

#endif  // RAIL_SDK_RAIL_SMALL_OBJECT_SERVICE_H
