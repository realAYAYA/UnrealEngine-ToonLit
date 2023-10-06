// Copyright Epic Games, Inc. All Rights Reserved.

#include "PostProcess/TextureSharePostprocessFactory.h"
#include "PostProcess/TextureSharePostprocess.h"

#include "Module/TextureShareDisplayClusterLog.h"
#include "DisplayClusterConfigurationTypes_Base.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureSharePostprocessFactory
//////////////////////////////////////////////////////////////////////////////////////////////
TSharedPtr<IDisplayClusterPostProcess, ESPMode::ThreadSafe> FTextureSharePostprocessFactory::Create(const FString& InPostProcessId, const FDisplayClusterConfigurationPostprocess* InConfigurationPostProcess)
{
	check(InConfigurationPostProcess != nullptr);

	if (ExistsTextureSharePostprocess.IsValid())
	{
		UE_LOG(LogTextureShareDisplayClusterPostProcess, Error, TEXT("Can't Instantiating postprocess <%s> id='%s' : There can only be one TextureShare postprocess per application. "), *InConfigurationPostProcess->Type, *InPostProcessId);

		return nullptr;
	}

	UE_LOG(LogTextureShareDisplayClusterPostProcess, Verbose, TEXT("Instantiating postprocess <%s> id='%s'"), *InConfigurationPostProcess->Type, *InPostProcessId);
	TSharedPtr<IDisplayClusterPostProcess, ESPMode::ThreadSafe> Result = MakeShared<FTextureSharePostprocess, ESPMode::ThreadSafe>(InPostProcessId, InConfigurationPostProcess);

	// Save weak ref to TS postprocess instance
	ExistsTextureSharePostprocess = Result;

	return Result;
}

bool FTextureSharePostprocessFactory::CanBeCreated(const FDisplayClusterConfigurationPostprocess* InConfigurationPostProcess) const
{
	// This postprocess used as a singletone
	return !ExistsTextureSharePostprocess.IsValid();
}
