// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusCoreModule.h"

#include "Interfaces/IPluginManager.h"
#include "OptimusComputeDataInterface.h"
#include "OptimusDataTypeRegistry.h"
#include "OptimusDeformer.h"
#include "OptimusObjectVersion.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"
#include "UObject/DevObjectVersion.h"
#include "Modules/ModuleManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "Nodes/OptimusNode_FunctionReference.h"

// Unique serialization id for Optimus .
const FGuid FOptimusObjectVersion::GUID(0x93ede1aa, 0x10ca7375, 0x4df98a28, 0x49b157a0);

static FDevVersionRegistration GRegisterOptimusObjectVersion(FOptimusObjectVersion::GUID, FOptimusObjectVersion::LatestVersion, TEXT("Dev-Optimus"));

void FOptimusCoreModule::StartupModule()
{
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("DeformerGraph"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/Optimus"), PluginShaderDir);

	// Make sure all our types are known at startup.
	FOptimusDataTypeRegistry::RegisterBuiltinTypes();
	FOptimusDataTypeRegistry::RegisterEngineCallbacks();
	UOptimusComputeDataInterface::RegisterAllTypes();
	
}

void FOptimusCoreModule::ShutdownModule()
{
	FOptimusDataTypeRegistry::UnregisterEngineCallbacks();
	FOptimusDataTypeRegistry::UnregisterAllTypes();
	
}


bool FOptimusCoreModule::RegisterDataInterfaceClass(TSubclassOf<UOptimusComputeDataInterface> InDataInterfaceClass)
{
	if (InDataInterfaceClass)
	{
		UOptimusComputeDataInterface* DataInterface = Cast<UOptimusComputeDataInterface>(InDataInterfaceClass->GetDefaultObject());
		if (ensure(DataInterface))
		{
			DataInterface->RegisterTypes();
			return true;
		}
	}
	return false;
}

void FOptimusCoreModule::UpdateFunctionReferences(const FSoftObjectPath& InOldGraphPath, const FSoftObjectPath& InNewGraphPath)
{
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	
	// find all assets in the project
	TArray<FAssetData> AssetDatas;
	AssetRegistryModule.Get().GetAssetsByClass(UOptimusDeformer::StaticClass()->GetClassPathName(), AssetDatas);

	FOptimusFunctionReferenceData FunctionReferenceData;
	
	// loop over all found assets
	for(const FAssetData& AssetData : AssetDatas)
	{
		RegisterFunctionReferencesFromAsset(FunctionReferenceData, AssetData);
	}
	
	if (FOptimusFunctionReferenceNodeSet* FunctionNodeArray = FunctionReferenceData.FunctionReferences.Find(InOldGraphPath))
	{
		for (TSoftObjectPtr<UOptimusNode_FunctionReference> FunctionNodePtr : FunctionNodeArray->Nodes)
		{
			UOptimusNode_FunctionReference* FunctionNode = FunctionNodePtr.LoadSynchronous();

			if (FunctionNode)
			{
				FunctionNode->Modify();
				FunctionNode->RefreshSerializedGraphPath(InNewGraphPath);
				FunctionNode->MarkPackageDirty();
			}
		}
	}
}

void FOptimusCoreModule::RegisterFunctionReferencesFromAsset(FOptimusFunctionReferenceData& InOutData, const FAssetData& AssetData)
{
	FString FunctionReferenceString = AssetData.GetTagValueRef<FString>(UOptimusDeformer::FunctionReferencesAssetTagName);

	if (!FunctionReferenceString.IsEmpty())
	{
		FOptimusFunctionReferenceData AssetFunctionReferenceData;

		FOptimusFunctionReferenceData::StaticStruct()->ImportText(*FunctionReferenceString, &AssetFunctionReferenceData, nullptr, PPF_None, nullptr, {});

		for(const TPair<FSoftObjectPath, FOptimusFunctionReferenceNodeSet>& Pair : AssetFunctionReferenceData.FunctionReferences)
		{
			InOutData.FunctionReferences.FindOrAdd(Pair.Key).Nodes.Append(Pair.Value.Nodes);
		}
	}
}

IMPLEMENT_MODULE(FOptimusCoreModule, OptimusCore)

DEFINE_LOG_CATEGORY(LogOptimusCore);
