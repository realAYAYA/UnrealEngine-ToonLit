// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "APIEnvir.h"

#include <string>

BEGIN_NAMESPACE_UE_AC

// Copy the string to the pasteboard
extern void SetPasteboardWithString(const utf8_t* InUtf8String);

// Set the string from the pasteboard contents
extern utf8_string GetStringFromPasteboard();

END_NAMESPACE_UE_AC
