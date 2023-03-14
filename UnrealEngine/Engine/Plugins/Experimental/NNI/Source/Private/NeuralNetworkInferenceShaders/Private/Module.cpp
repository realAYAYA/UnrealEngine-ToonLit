// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module.h"
#include "Interfaces/IPluginManager.h"
#include "ShaderCore.h"
#include "Utils.h"

#define LOCTEXT_NAMESPACE "NeuralNetworkInferenceShaders"

// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
void FNeuralNetworkInferenceShadersModule::StartupModule()
{
	// Add shader directory
	const TSharedPtr<IPlugin> NeuralNetworkInferencePlugin = IPluginManager::Get().FindPlugin(TEXT("NeuralNetworkInference"));
	if (NeuralNetworkInferencePlugin.IsValid())
	{
		const FString RealShaderDirectory = NeuralNetworkInferencePlugin->GetBaseDir() + TEXT("/Shaders/"); // TEXT("../../../Engine/Plugins/NeuralNetworkInference/Shaders");
		const FString VirtualShaderDirectory = TEXT("/Plugins/NeuralNetworkInference");
		AddShaderSourceDirectoryMapping(VirtualShaderDirectory, RealShaderDirectory);
	}
	else
	{
		UE_LOG(LogNeuralNetworkInferenceShaders, Warning,
			TEXT("FNeuralNetworkInferenceShadersModule::StartupModule(): NeuralNetworkInferencePlugin was nullptr, shaders directory not added."));
	}
}

// This function may be called during shutdown to clean up your module. For modules that support dynamic reloading,
// we call this function before unloading the module.
void FNeuralNetworkInferenceShadersModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FNeuralNetworkInferenceShadersModule, NeuralNetworkInferenceShaders);

#undef LOCTEXT_NAMESPACE
