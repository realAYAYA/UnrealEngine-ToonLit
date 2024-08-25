// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/RigVMBuildData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "RigVMModel/RigVMFunctionLibrary.h"
#include "UObject/UObjectIterator.h"
#include "RigVMModel/RigVMClient.h"
#include "UObject/StrongObjectPtr.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMBuildData)

static const FName FunctionReferenceNodeDataName = TEXT("FunctionReferenceNodeData");

// When the object system has been completely loaded, collect all the references between RigVM graphs 
static FDelayedAutoRegisterHelper GRigVMBuildDataSingletonHelper(EDelayedRegisterRunPhase::EndOfEngineInit, []() -> void
{
	URigVMBuildData::Get()->InitializeIfNeeded();
});

FRigVMReferenceNodeData::FRigVMReferenceNodeData(URigVMFunctionReferenceNode* InReferenceNode)
{
	check(InReferenceNode);
	ReferenceNodePtr = TSoftObjectPtr<URigVMFunctionReferenceNode>(InReferenceNode);
	ReferenceNodePath = ReferenceNodePtr.ToString();
	ReferencedFunctionIdentifier = InReferenceNode->GetReferencedFunctionHeader().LibraryPointer;
}

TSoftObjectPtr<URigVMFunctionReferenceNode> FRigVMReferenceNodeData::GetReferenceNodeObjectPath()
{
	if(ReferenceNodePtr.IsNull())
	{
		ReferenceNodePtr = TSoftObjectPtr<URigVMFunctionReferenceNode>(ReferenceNodePath);
	}
	return ReferenceNodePtr;
}

URigVMFunctionReferenceNode* FRigVMReferenceNodeData::GetReferenceNode()
{
	if(ReferenceNodePtr.IsNull())
	{
		ReferenceNodePtr = TSoftObjectPtr<URigVMFunctionReferenceNode>(ReferenceNodePath);
	}
	if(!ReferenceNodePtr.IsValid())
	{
		ReferenceNodePtr.LoadSynchronous();
	}
	if(ReferenceNodePtr.IsValid())
	{
		return ReferenceNodePtr.Get();
	}
	return nullptr;
}

bool URigVMBuildData::bInitialized = false; 

URigVMBuildData::URigVMBuildData()
: UObject()
, bIsRunningUnitTest(false)
{
}

URigVMBuildData* URigVMBuildData::Get()
{
	// static in a function scope ensures that the GC system is initiated before 
	// the build data constructor is called
	static TStrongObjectPtr<URigVMBuildData> sBuildData;
	if(!sBuildData.IsValid() && IsInGameThread())
	{
		sBuildData = TStrongObjectPtr<URigVMBuildData>(
			NewObject<URigVMBuildData>(
				GetTransientPackage(), 
				TEXT("RigVMBuildData"), 
				RF_Transient));
	}
	return sBuildData.Get();

}

void URigVMBuildData::InitializeIfNeeded()
{
	if (bInitialized)
	{
		return;
	}

	// Find all classes which implement IRigVMClientHost
	TArray<UClass*> ImplementedClasses;
	for (TObjectIterator<UClass> ClassIterator; ClassIterator; ++ClassIterator)
	{
		if (ClassIterator->ImplementsInterface(URigVMClientHost::StaticClass()))
		{
			ImplementedClasses.Add(*ClassIterator);
		}
	}

	// Loop the classes
	for (UClass* Class : ImplementedClasses)
	{
		const FArrayProperty* ReferenceNodeDataProperty =
		   CastField<FArrayProperty>(Class->FindPropertyByName(FunctionReferenceNodeDataName));
		if(ReferenceNodeDataProperty)
		{
			const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

			// find all assets of this class in the project
			TArray<FAssetData> ControlRigAssetDatas;
			FARFilter ControlRigAssetFilter;
			ControlRigAssetFilter.ClassPaths.Add(Class->GetClassPathName());
			AssetRegistryModule.Get().GetAssets(ControlRigAssetFilter, ControlRigAssetDatas);

			// loop over all found assets
			for(const FAssetData& ControlRigAssetData : ControlRigAssetDatas)
			{
				RegisterReferencesFromAsset(ControlRigAssetData);
			}
		}
	}
	
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    AssetRegistryModule.Get().OnAssetAdded().AddStatic(&URigVMBuildData::RegisterReferencesFromAsset);
	
	bInitialized = true;
}

void URigVMBuildData::RegisterReferencesFromAsset(const FAssetData& InAssetData)
{
	URigVMBuildData* BuildData = URigVMBuildData::Get();

	// It's faster to check for a key directly then trying to get the class
	FAssetDataTagMapSharedView::FFindTagResult FoundValue = InAssetData.TagsAndValues.FindTag(FunctionReferenceNodeDataName);

	if (FoundValue.IsSet())
	{
		if (UClass* Class = InAssetData.GetClass())
		{
			const FArrayProperty* ReferenceNodeDataProperty =
				  CastField<FArrayProperty>(Class->FindPropertyByName(FunctionReferenceNodeDataName));
			if(ReferenceNodeDataProperty)
			{
				const FString ReferenceNodeDataString = FoundValue.AsString();
				if(ReferenceNodeDataString.IsEmpty())
				{
					return;
				}

				// See if it has reference node data, and register the references
				TArray<FRigVMReferenceNodeData> ReferenceNodeDatas;
				ReferenceNodeDataProperty->ImportText_Direct(*ReferenceNodeDataString, &ReferenceNodeDatas, nullptr, EPropertyPortFlags::PPF_None);	
				for(FRigVMReferenceNodeData& ReferenceNodeData : ReferenceNodeDatas)
				{
					if (ReferenceNodeData.ReferencedFunctionIdentifier.LibraryNode.IsValid())
					{
						BuildData->RegisterFunctionReference(ReferenceNodeData.ReferencedFunctionIdentifier, ReferenceNodeData.GetReferenceNodeObjectPath());
					}
					else if (ReferenceNodeData.ReferencedHeader_DEPRECATED.IsValid())
					{
						BuildData->RegisterFunctionReference(ReferenceNodeData.ReferencedHeader_DEPRECATED.LibraryPointer, ReferenceNodeData.GetReferenceNodeObjectPath());
					}
					else if (!ReferenceNodeData.ReferencedFunctionPath_DEPRECATED.IsEmpty())
					{
						BuildData->RegisterFunctionReference(ReferenceNodeData);
					}
				}
			}
		}
	}
}

const FRigVMFunctionReferenceArray* URigVMBuildData::FindFunctionReferences(const FRigVMGraphFunctionIdentifier& InFunction) const
{
	return GraphFunctionReferences.Find(InFunction);
}

void URigVMBuildData::ForEachFunctionReference(const FRigVMGraphFunctionIdentifier& InFunction,
                                               TFunction<void(URigVMFunctionReferenceNode*)> PerReferenceFunction,
                                               bool bLoadIfNecessary) const
{
	if (const FRigVMFunctionReferenceArray* ReferencesEntry = FindFunctionReferences(InFunction))
	{
		for (int32 ReferenceIndex = 0; ReferenceIndex < ReferencesEntry->Num(); ReferenceIndex++)
		{
			const TSoftObjectPtr<URigVMFunctionReferenceNode>& Reference = ReferencesEntry->operator [](ReferenceIndex);
			if (bLoadIfNecessary && !Reference.IsValid())
			{
				Reference.LoadSynchronous();
			}
			if (Reference.IsValid())
			{
				PerReferenceFunction(Reference.Get());
			}
		}
	}
}

void URigVMBuildData::ForEachFunctionReferenceSoftPtr(const FRigVMGraphFunctionIdentifier& InFunction,
                                                      TFunction<void(TSoftObjectPtr<URigVMFunctionReferenceNode>)> PerReferenceFunction) const
{
	if (const FRigVMFunctionReferenceArray* ReferencesEntry = FindFunctionReferences(InFunction))
	{
		for (int32 ReferenceIndex = 0; ReferenceIndex < ReferencesEntry->Num(); ReferenceIndex++)
		{
			const TSoftObjectPtr<URigVMFunctionReferenceNode>& Reference = ReferencesEntry->operator [](ReferenceIndex);
			PerReferenceFunction(Reference);
		}
	}
}

void URigVMBuildData::RegisterFunctionReference(const FRigVMGraphFunctionIdentifier& InFunction, URigVMFunctionReferenceNode* InReference)
{
	if(InReference == nullptr)
	{
		return;
	}

	const TSoftObjectPtr<URigVMFunctionReferenceNode> ReferenceKey(InReference);

	RegisterFunctionReference(InFunction, ReferenceKey);
}

void URigVMBuildData::RegisterFunctionReference(const FRigVMGraphFunctionIdentifier& InFunction,
                                                TSoftObjectPtr<URigVMFunctionReferenceNode> InReference)
{
	if(InReference.IsNull())
	{
		return;
	}

	if(FRigVMFunctionReferenceArray* ReferenceEntry = GraphFunctionReferences.Find(InFunction))
	{
		if(ReferenceEntry->FunctionReferences.Contains(InReference))
		{
			return;
		}

		Modify();
		ReferenceEntry->FunctionReferences.Add(InReference);
	}
	else
	{
		Modify();
		FRigVMFunctionReferenceArray NewReferenceEntry;
		NewReferenceEntry.FunctionReferences.Add(InReference);
		GraphFunctionReferences.Add(InFunction, NewReferenceEntry);
	}
	
	MarkPackageDirty();
}

void URigVMBuildData::RegisterFunctionReference(FRigVMReferenceNodeData InReferenceNodeData)
{
	if (InReferenceNodeData.ReferencedFunctionIdentifier.LibraryNode.IsValid())
	{
		return RegisterFunctionReference(InReferenceNodeData.ReferencedFunctionIdentifier, InReferenceNodeData.GetReferenceNodeObjectPath());
	}

	if (!InReferenceNodeData.ReferencedFunctionIdentifier.LibraryNode.IsValid())
	{
		InReferenceNodeData.ReferencedFunctionIdentifier = InReferenceNodeData.ReferencedHeader_DEPRECATED.LibraryPointer;
	}

	if (!InReferenceNodeData.ReferencedFunctionIdentifier.LibraryNode.IsValid())
	{
		InReferenceNodeData.ReferencedFunctionIdentifier.LibraryNode = InReferenceNodeData.ReferencedFunctionPath_DEPRECATED;
	}
	
	check(InReferenceNodeData.ReferencedFunctionIdentifier.LibraryNode.IsValid());

	FSoftObjectPath LibraryNodePath = InReferenceNodeData.ReferencedFunctionIdentifier.LibraryNode;
	TSoftObjectPtr<URigVMLibraryNode> LibraryNodePtr = TSoftObjectPtr<URigVMLibraryNode>(LibraryNodePath);

	// Try to find a FunctionIdentifier with the same LibraryNodePath
	bool bFound = false;
	for (TPair< FRigVMGraphFunctionIdentifier, FRigVMFunctionReferenceArray >& Pair : GraphFunctionReferences)
	{
		if (Pair.Key.LibraryNode == LibraryNodePath)
		{
			Pair.Value.FunctionReferences.Add(InReferenceNodeData.GetReferenceNodeObjectPath());
			bFound = true;
			break;
		}
	}

	// Otherwise, lets add a new identifier, even if it has no function host
	if (!bFound)
	{
		FRigVMGraphFunctionIdentifier Pointer(nullptr, LibraryNodePath);
		if (LibraryNodePtr.IsValid())
		{
			Pointer.HostObject = Cast<UObject>(LibraryNodePtr.Get()->GetFunctionHeader().GetFunctionHost());
		}
		FRigVMFunctionReferenceArray RefArray;
		RefArray.FunctionReferences.Add(InReferenceNodeData.GetReferenceNodeObjectPath());
		GraphFunctionReferences.Add(Pointer, RefArray);
	}
}

void URigVMBuildData::UnregisterFunctionReference(const FRigVMGraphFunctionIdentifier& InFunction,
                                                  URigVMFunctionReferenceNode* InReference)
{
	if(InReference == nullptr)
	{
		return;
	}

	const TSoftObjectPtr<URigVMFunctionReferenceNode> ReferenceKey(InReference);

	return UnregisterFunctionReference(InFunction, ReferenceKey);
}

void URigVMBuildData::UnregisterFunctionReference(const FRigVMGraphFunctionIdentifier& InFunction,
                                                  TSoftObjectPtr<URigVMFunctionReferenceNode> InReference)
{
	if(InReference.IsNull())
	{
		return;
	}

	if(FRigVMFunctionReferenceArray* ReferenceEntry = GraphFunctionReferences.Find(InFunction))
	{
		if(!ReferenceEntry->FunctionReferences.Contains(InReference))
		{
			return;
		}

		Modify();
		ReferenceEntry->FunctionReferences.Remove(InReference);
		MarkPackageDirty();
	}
}

void URigVMBuildData::ClearInvalidReferences()
{
	if (bIsRunningUnitTest)
	{
		return;
	}
	
	Modify();
	
	// check each function's each reference
	int32 NumRemoved = 0;
	for (TTuple<FRigVMGraphFunctionIdentifier, FRigVMFunctionReferenceArray>& FunctionReferenceInfo : GraphFunctionReferences)
	{
		FRigVMFunctionReferenceArray* ReferencesEntry = &FunctionReferenceInfo.Value;

		static FString sTransientPackagePrefix;
		if(sTransientPackagePrefix.IsEmpty())
		{
			sTransientPackagePrefix = GetTransientPackage()->GetPathName();
		}
		static const FString sTempPrefix = TEXT("/Temp/");

		NumRemoved += ReferencesEntry->FunctionReferences.RemoveAll([](TSoftObjectPtr<URigVMFunctionReferenceNode> Referencer)
		{
			// ignore keys / references within the transient package
			const FString ReferencerString = Referencer.ToString();
			return ReferencerString.StartsWith(sTransientPackagePrefix) || ReferencerString.StartsWith(sTempPrefix);
		});
	}

	if (NumRemoved > 0)
	{
		MarkPackageDirty();
	}
}



