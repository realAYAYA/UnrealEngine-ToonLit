// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_FUNCTION_HELPER_H
#define RAIL_SDK_RAIL_FUNCTION_HELPER_H

#include "rail/sdk/base/rail_define.h"
#include "rail/sdk/rail_event.h"
#include "rail/sdk/rail_factory.h"
#include "rail/sdk/base/va_arg_helper.h"

namespace rail {
namespace helper {

#define RAIL_TYPEDEF typedef
#define RAIL_SDK_DEFINE_FUNC_PTR(x) (__cdecl * RailFuncPtr_##x)

// define function pointers

// platform
RAIL_TYPEDEF bool RAIL_SDK_DEFINE_FUNC_PTR(RailNeedRestartAppForCheckingEnvironment)(
                    RailGameID game_id,
                    int32_t argc,
                    const char** argv);
RAIL_TYPEDEF bool RAIL_SDK_DEFINE_FUNC_PTR(RailInitialize)();
RAIL_TYPEDEF void RAIL_SDK_DEFINE_FUNC_PTR(RailFinalize)();

// rail event
RAIL_TYPEDEF void RAIL_SDK_DEFINE_FUNC_PTR(RailRegisterEvent)(RAIL_EVENT_ID event_id,
                    IRailEvent* event_handler);
RAIL_TYPEDEF void RAIL_SDK_DEFINE_FUNC_PTR(RailUnregisterEvent)(RAIL_EVENT_ID event_id,
                    IRailEvent* event_handler);
RAIL_TYPEDEF void RAIL_SDK_DEFINE_FUNC_PTR(RailFireEvents)();

// rail factory
RAIL_TYPEDEF IRailFactory* RAIL_SDK_DEFINE_FUNC_PTR(RailFactory)();

// rail version
RAIL_TYPEDEF void RAIL_SDK_DEFINE_FUNC_PTR(RailGetSdkVersion)(RailString* version,
                    RailString* description);


// define dynamic invoke function from dll helper macro

#define RAIL_SDK_CALL(MODULE, FUNC, ...)                                                     \
    RailFuncPtr_##FUNC func_##FUNC = RailGetGSDKFunction<RailFuncPtr_##FUNC>(MODULE, #FUNC); \
    if (func_##FUNC != NULL) {                                                               \
        func_##FUNC(RAIL_VA_ARGS(__VA_ARGS__));                                              \
    }

#define RAIL_SDK_RET_CALL(MODULE, FUNC, default_ret, ...)                                    \
    RailFuncPtr_##FUNC func_##FUNC = RailGetGSDKFunction<RailFuncPtr_##FUNC>(MODULE, #FUNC); \
    if (func_##FUNC != NULL) {                                                               \
        return func_##FUNC(RAIL_VA_ARGS(__VA_ARGS__));                                       \
    } else {                                                                                 \
        return default_ret;                                                                  \
    }

template<class T>
T RailGetGSDKFunction(HMODULE module, const char* func_name) {
    if (module != NULL && func_name != NULL) {
        T func_t = (T)GetProcAddress(module, func_name);
        return func_t;
    }

    return NULL;
}

// invoke function from dll
class Invoker {
  public:
    explicit Invoker(HMODULE module) { module_ = module; }
    ~Invoker() {}

    bool RailNeedRestartAppForCheckingEnvironment(RailGameID game_id,
            int32_t argc,
            const char** argv) {
        RAIL_SDK_RET_CALL(module_,
            RailNeedRestartAppForCheckingEnvironment,
            true,
            game_id,
            argc,
            argv);
    }

    bool RailInitialize() { RAIL_SDK_RET_CALL(module_, RailInitialize, false); }
    void RailFinalize() { RAIL_SDK_CALL(module_, RailFinalize); }

    void RailRegisterEvent(RAIL_EVENT_ID event_id, IRailEvent* event_handler) {
        RAIL_SDK_CALL(module_, RailRegisterEvent, event_id, event_handler);
    }
    void RailUnregisterEvent(RAIL_EVENT_ID event_id, IRailEvent* event_handler) {
        RAIL_SDK_CALL(module_, RailUnregisterEvent, event_id, event_handler);
    }
    void RailFireEvents() { RAIL_SDK_CALL(module_, RailFireEvents); }

    IRailFactory* RailFactory() { RAIL_SDK_RET_CALL(module_, RailFactory, NULL); }

    void RailGetSdkVersion(RailString* version, RailString* description) {
        RAIL_SDK_CALL(module_, RailGetSdkVersion, version, description);
    }

  private:
    HMODULE module_;
};

}  // namespace helper
};  // namespace rail

#endif  // RAIL_SDK_RAIL_FUNCTION_HELPER_H
