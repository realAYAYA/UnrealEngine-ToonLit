// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Android/AndroidPlatform.h"
#include "Android/AndroidJava.h"
#include "Android/AndroidApplication.h"
#include "Templates/SharedPointer.h"

namespace Electra
{
	class IVideoDecoderResourceDelegate : public TSharedFromThis<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe>
	{
	public:
		virtual ~IVideoDecoderResourceDelegate() {}

		virtual jobject VideoDecoderResourceDelegate_GetCodecSurface() = 0;
	};

}
