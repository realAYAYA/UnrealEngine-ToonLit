// Copyright Epic Games, Inc. All Rights Reserved.

#include "status/Status.h"

#include "status/Storage.h"

namespace sc {

StatusCode Status::get() {
    return StatusStorage::get();
}

bool Status::isOk() {
    return StatusStorage::isOk();
}

HookFunction Status::getHook() {
    return StatusStorage::getHook();
}

void Status::setHook(HookFunction hook) {
    StatusStorage::setHook(hook);
}

}  // namespace sc
