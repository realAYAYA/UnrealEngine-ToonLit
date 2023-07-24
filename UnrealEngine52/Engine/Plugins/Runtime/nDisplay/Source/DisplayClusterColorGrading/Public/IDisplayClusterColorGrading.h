// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class IDisplayClusterColorGradingDrawerSingleton;

/**
 * Display Cluster Color Grading module interface
 */
class IDisplayClusterColorGrading : public IModuleInterface
{
public:
	static constexpr const TCHAR* ModuleName = TEXT("DisplayClusterColorGrading");

public:
	virtual ~IDisplayClusterColorGrading() = default;

	/**
	* Singleton-like access to this module's interface.  This is just for convenience!
	* Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	*
	* @return Returns singleton instance, loading the module on demand if needed
	*/
	static inline IDisplayClusterColorGrading& Get()
	{
		return FModuleManager::GetModuleChecked<IDisplayClusterColorGrading>(ModuleName);
	}

	/**
	* Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	*
	* @return True if the module is loaded and ready to use
	*/
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(ModuleName);
	}

	/** Gets the singleton used to manage the color grading drawer */
	virtual IDisplayClusterColorGradingDrawerSingleton& GetColorGradingDrawerSingleton() const = 0;
};