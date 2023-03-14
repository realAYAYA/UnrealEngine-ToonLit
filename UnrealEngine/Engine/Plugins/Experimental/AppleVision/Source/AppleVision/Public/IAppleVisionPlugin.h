// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "HAL/ThreadSafeBool.h"

#include "AppleVisionTypes.h"
#include "AppleVisionAvailability.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAppleVision, Log, All);

class UTexture;

/**
 * Base class for implementing Apple Vision async tasks
 */
class APPLEVISION_API FAppleVisionAsyncTaskBase
{
protected:
	FAppleVisionAsyncTaskBase() {}
	virtual ~FAppleVisionAsyncTaskBase() {}

public:
	bool IsDone() const { return bIsDone; }
	bool HadError() const { return bHadError; }
	FString GetErrorReason() const { return Error; }

protected:
	FThreadSafeBool bIsDone;
	FThreadSafeBool bHadError;
	FString Error;
};

class APPLEVISION_API FAppleVisionDetectFacesAsyncTaskBase :
	public FAppleVisionAsyncTaskBase
{
public:
	FAppleVisionDetectFacesAsyncTaskBase() {}
	virtual ~FAppleVisionDetectFacesAsyncTaskBase() {}

	FFaceDetectionResult& GetResult() { return Result; }

private:
	FFaceDetectionResult Result;
};

class IAppleVisionPlugin :
	public IModuleInterface
{
public:
	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IAppleVisionPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked<IAppleVisionPlugin>("AppleVision");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("AppleVision");
	}

	/**
	 * Performs a face detection computer vision task in the background
	 *
	 * @param SourceImage the image to detect faces in (NOTE: must support UAppleImageInterface)
	 *
	 * @return the async task that is doing the conversion
	 */
	virtual TSharedPtr<FAppleVisionDetectFacesAsyncTaskBase, ESPMode::ThreadSafe> DetectFaces(UTexture* SourceImage) = 0;
};

