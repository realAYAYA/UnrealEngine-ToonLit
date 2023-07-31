// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"

// SketchUp SDK.
#include "DatasmithSketchUpSDKBegins.h"
#include "SketchUpAPI/unicodestring.h"
#include "DatasmithSketchUpSDKCeases.h"

FString SuConvertString(SUStringRef StringRef);

template<typename FuncType, typename EntityType>
FString SuGetString(FuncType GetTheString, EntityType EntityRef)
{
	SUStringRef StringRef = SU_INVALID;
	SUStringCreate(&StringRef);
	GetTheString(EntityRef, &StringRef); /* we can ignore the returned SU_RESULT */ 
	FString Result = SuConvertString(StringRef);
	SUStringRelease(&StringRef);
	return Result;
}


