// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Modules/ModuleManager.h"
#include "Misc/Guid.h"

class FChaosVDTraceManager;
class FChaosVDEngine;
class SDockTab;
class FSpawnTabArgs;
struct FGuid;

DECLARE_LOG_CATEGORY_EXTERN(LogChaosVDEditor, Log, Log);
class FChaosVDModule : public IModuleInterface
{
public:

	static FChaosVDModule& Get();

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	TSharedPtr<FChaosVDTraceManager>& GetTraceManager() { return ChaosVDTraceManager; };

private:

	void RegisterClassesCustomDetails() const;

	TSharedRef<SDockTab> SpawnMainTab(const FSpawnTabArgs& Args);

	void RegisterChaosVDInstance(const FGuid& InstanceGuid, TSharedPtr<FChaosVDEngine> Instance);
	void DeregisterChaosVDInstance(const FGuid& InstanceGuid);
	
	TMap<FGuid, TSharedPtr<FChaosVDEngine>> ActiveChaosVDInstances;

	TSharedPtr<FChaosVDTraceManager> ChaosVDTraceManager;
};
