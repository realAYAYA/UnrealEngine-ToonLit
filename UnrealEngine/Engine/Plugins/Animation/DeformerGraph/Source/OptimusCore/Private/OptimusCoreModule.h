// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IOptimusCoreModule.h"
#include "OptimusFunctionNodeGraphHeader.h"


class FOptimusCoreModule : public IOptimusCoreModule
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;


	/** IOptimusCoreModule implementation */
	bool RegisterDataInterfaceClass(TSubclassOf<UOptimusComputeDataInterface> InDataInterfaceClass) override;

	void UpdateFunctionReferences(const FSoftObjectPath& InOldGraphPath, const FSoftObjectPath& InNewGraphPath) override;
	
protected:
	void RegisterFunctionReferencesFromAsset(FOptimusFunctionReferenceData& InOutData, const FAssetData& AssetData);
	
	
};

DECLARE_LOG_CATEGORY_EXTERN(LogOptimusCore, Log, All);
