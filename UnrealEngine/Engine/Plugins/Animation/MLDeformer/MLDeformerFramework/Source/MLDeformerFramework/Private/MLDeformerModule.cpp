// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerModule.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

// Log category.
MLDEFORMERFRAMEWORK_API DEFINE_LOG_CATEGORY(LogMLDeformer)

#define LOCTEXT_NAMESPACE "MLDeformerModule"

IMPLEMENT_MODULE(UE::MLDeformer::FMLDeformerModule, MLDeformerFramework)

namespace UE::MLDeformer
{
	void FMLDeformerModule::StartupModule()
	{
		// Register an additional shader path for our shaders used inside the deformer graph system.
		const FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("MLDeformerFramework"))->GetBaseDir(), TEXT("Shaders"));
		AddShaderSourceDirectoryMapping(TEXT("/Plugin/MLDeformerFramework"), PluginShaderDir);
	}
}	// namespace UE::MLDeformer

#undef LOCTEXT_NAMESPACE
