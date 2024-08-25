// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include <memory>
/**
 * 
 */

DECLARE_LOG_CATEGORY_EXTERN(LogRenderDocTextureGraph, Log, All);

namespace TextureGraphEditor
{
	class TEXTUREGRAPHENGINE_API RenderDocManager
	{

	public:
		RenderDocManager();
		~RenderDocManager();
		void												Initialize();

		void												CaptureNextBatch();
		void												CapturePreviousBatch();
		void												BeginCapture();
		void												EndCapture();
};
	typedef std::unique_ptr<RenderDocManager>				RenderDocManagerPtr;
}