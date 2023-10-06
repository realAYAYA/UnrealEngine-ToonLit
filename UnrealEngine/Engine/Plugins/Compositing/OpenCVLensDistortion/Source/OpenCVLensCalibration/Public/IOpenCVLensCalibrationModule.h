// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#define OPENCV_LENSCALIBRATION_MODULE_NAME TEXT("OpenCVLensCalibration")

DECLARE_LOG_CATEGORY_EXTERN(LogOpenCVLensCalibration, Verbose, All);

/**
 * The public interface to this module
 */
class IOpenCVLensCalibrationModule : public IModuleInterface
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IOpenCVLensCalibrationModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IOpenCVLensCalibrationModule>(OPENCV_LENSCALIBRATION_MODULE_NAME);
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(OPENCV_LENSCALIBRATION_MODULE_NAME);
	}
};

#undef OPENCV_LENSCALIBRATION_MODULE_NAME

