// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Android/AndroidPlatform.h"
#include "Android/AndroidJava.h"
#include "Android/AndroidApplication.h"

namespace Electra
{

	class IVideoDecoderResourceDelegate
	{
	public:
		virtual ~IVideoDecoderResourceDelegate() {}

		virtual jobject VideoDecoderResourceDelegate_GetCodecSurface() = 0;
	};

}
