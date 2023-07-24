// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/RigVMBuildData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "RigVMModel/RigVMFunctionLibrary.h"
#include "UObject/UObjectIterator.h"
#include "RigVMModel/RigVMClient.h"
#include "UObject/StrongObjectPtr.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMBuildData)

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
	ReferencedHeader = InReferenceNode->GetReferencedFunctionHeader();
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
		   CastField<FArrayProperty>(Class->FindPropertyByName(TEXT("FunctionReferenceNodeData")));
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
				const FString ReferenceNodeDataString =
					ControlRigAssetData.GetTagValueRef<FString>(ReferenceNodeDataProperty->GetFName());
				if(ReferenceNodeDataString.IsEmpty())
				{
					continue;
				}

				// See if it has reference node data, and register the references
				TArray<FRigVMReferenceNodeData> ReferenceNodeDatas;
				ReferenceNodeDataProperty->ImportText_Direct(*ReferenceNodeDataString, &ReferenceNodeDatas, nullptr, EPropertyPortFlags::PPF_None);	
				for(FRigVMReferenceNodeData& ReferenceNodeData : ReferenceNodeDatas)
				{
					if (ReferenceNodeData.ReferencedHeader.IsValid())
					{
						RegisterFunctionReference(ReferenceNodeData.ReferencedHeader.LibraryPointer, ReferenceNodeData.GetReferenceNodeObjectPath());
					}
					else if (!ReferenceNodeData.ReferencedFunctionPath_DEPRECATED.IsEmpty())
					{
						RegisterFunctionReference(ReferenceNodeData);							
					}
				}
			}
		}
	}
	
	bInitialized = true;
}

const FRigVMFunctionReferenceArray* URigVMBuildData::FindFunctionReferences(const FRigVMGraphFunctionIdentifier& InFunction) const
{
	return GraphFunctionReferences.Find(InFunction);
}

void URigVMBuildData::ForEachFunctionReference(const FRigVMGraphFunctionIdentifier& InFunction,
                                               TFunction<void(URigVMFunctionReferenceNode*)> PerReferenceFunction) const
{
	if (const FRigVMFunctionReferenceArray* ReferencesEntry = FindFunctionReferences(InFunction))
	{
		for (int32 ReferenceIndex = 0; ReferenceIndex < ReferencesEntry->Num(); ReferenceIndex++)
		{
			const TSoftObjectPtr<URigVMFunctionReferenceNode>& Reference = ReferencesEntry->operator [](ReferenceIndex);
			if (!Reference.IsValid())
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
	if (InReferenceNodeData.ReferencedHeader.IsValid())
	{
		return RegisterFunctionReference(InReferenceNodeData.ReferencedHeader.LibraryPointer, InReferenceNodeData.GetReferenceNodeObjectPath());
	}

	if (!InReferenceNodeData.ReferencedHeader.LibraryPointer.LibraryNode.IsValid())
	{
		InReferenceNodeData.ReferencedHeader.LibraryPointer.LibraryNode = InReferenceNodeData.ReferencedFunctionPath_DEPRECATED;
	}
	
	check(InReferenceNodeData.ReferencedHeader.LibraryPointer.LibraryNode.IsValid());

	FSoftObjectPath LibraryNodePath = InReferenceNodeData.ReferencedHeader.LibraryPointer.LibraryNode;
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



