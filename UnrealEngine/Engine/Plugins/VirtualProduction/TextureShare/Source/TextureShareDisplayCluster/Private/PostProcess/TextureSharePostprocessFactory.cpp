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

	UE_LOG(LogTextureShareDisplayClusterPostProcess, Verbose, TEXT("Instantiating postprocess <%s> id='%s'"), *InConfigurationPostProcess->Type, *InPostProcessId);

	return  MakeShared<FTextureSharePostprocess, ESPMode::ThreadSafe>(InPostProcessId, InConfigurationPostProcess);
}
