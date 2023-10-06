// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_API_H
#define RAIL_SDK_RAIL_API_H

#include "rail/sdk/base/rail_define.h"
#include "rail/sdk/rail_event.h"
#include "rail/sdk/rail_factory.h"
#include "rail/sdk/rail_function_helper.h"

#ifdef RAIL_API_EXPORTS
#define RAIL_API extern "C" __declspec(dllexport)
#else
#define RAIL_API extern "C" __declspec(dllimport)
#endif
#define RAIL_CALLTYPE __cdecl

namespace rail {

// platform
RAIL_API bool RAIL_CALLTYPE RailNeedRestartAppForCheckingEnvironment(RailGameID game_id,
                                int32_t argc,
                                const char** argv);
RAIL_API bool RAIL_CALLTYPE RailInitialize();
RAIL_API void RAIL_CALLTYPE RailFinalize();

// rail event
RAIL_API void RAIL_CALLTYPE RailRegisterEvent(RAIL_EVENT_ID event_id, IRailEvent* event_handler);
RAIL_API void RAIL_CALLTYPE RailUnregisterEvent(RAIL_EVENT_ID event_id, IRailEvent* event_handler);
RAIL_API void RAIL_CALLTYPE RailFireEvents();

// rail factory
RAIL_API IRailFactory* RAIL_CALLTYPE RailFactory();

// rail sdk version
RAIL_API void RAIL_CALLTYPE RailGetSdkVersion(RailString* version, RailString* description);

};  // namespace rail

#endif  // RAIL_SDK_RAIL_API_H
