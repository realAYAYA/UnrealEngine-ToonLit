// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Android/AndroidPlatform.h"
#include "Android/AndroidJava.h"
#include "Android/AndroidApplication.h"

class IElectraPlayerResourceDelegate
{
public:
	virtual ~IElectraPlayerResourceDelegate() {}

	virtual jobject GetCodecSurface() = 0;
};
