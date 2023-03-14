// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/RigVMBuildData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMBuildData)

FRigVMReferenceNodeData::FRigVMReferenceNodeData(URigVMFunctionReferenceNode* InReferenceNode)
{
	check(InReferenceNode);
	ReferenceNodePtr = TSoftObjectPtr<URigVMFunctionReferenceNode>(InReferenceNode);
	ReferenceNodePath = ReferenceNodePtr.ToString();
	LibraryNodePtr = TSoftObjectPtr<URigVMLibraryNode>(InReferenceNode->GetReferencedNode());
	ReferencedFunctionPath = LibraryNodePtr.ToString();
}

TSoftObjectPtr<URigVMFunctionReferenceNode> FRigVMReferenceNodeData::GetReferenceNodeObjectPath()
{
	if(ReferenceNodePtr.IsNull())
	{
		ReferenceNodePtr = TSoftObjectPtr<URigVMFunctionReferenceNode>(ReferenceNodePath);
	}
	return ReferenceNodePtr;
}

TSoftObjectPtr<URigVMLibraryNode> FRigVMReferenceNodeData::GetReferencedFunctionObjectPath()
{
	if(LibraryNodePtr.IsNull())
	{
		LibraryNodePtr = TSoftObjectPtr<URigVMFunctionReferenceNode>(ReferencedFunctionPath);
	}
	return LibraryNodePtr;
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

URigVMLibraryNode* FRigVMReferenceNodeData::GetReferencedFunction()
{
	if(LibraryNodePtr.IsNull())
	{
		LibraryNodePtr = TSoftObjectPtr<URigVMLibraryNode>(ReferencedFunctionPath);
	}
	if(!LibraryNodePtr.IsValid())
	{
		LibraryNodePtr.LoadSynchronous();
	}
	if(LibraryNodePtr.IsValid())
	{
		return LibraryNodePtr.Get();
	}
	return nullptr;
}

URigVMBuildData::URigVMBuildData()
: UObject()
, bIsRunningUnitTest(false)
{
}

const FRigVMFunctionReferenceArray* URigVMBuildData::FindFunctionReferences(const URigVMLibraryNode* InFunction) const
{
	check(InFunction);

	const TSoftObjectPtr<URigVMLibraryNode> Key(InFunction);
	return FunctionReferences.Find(Key);
}

void URigVMBuildData::ForEachFunctionReference(const URigVMLibraryNode* InFunction,
                                             TFunction<void(URigVMFunctionReferenceNode*)> PerReferenceFunction) const
{
	check(InFunction);
	
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

void URigVMBuildData::ForEachFunctionReferenceSoftPtr(const URigVMLibraryNode* InFunction,
	TFunction<void(TSoftObjectPtr<URigVMFunctionReferenceNode>)> PerReferenceFunction) const
{
	check(InFunction);

	if (const FRigVMFunctionReferenceArray* ReferencesEntry = FindFunctionReferences(InFunction))
	{
		for (int32 ReferenceIndex = 0; ReferenceIndex < ReferencesEntry->Num(); ReferenceIndex++)
		{
			const TSoftObjectPtr<URigVMFunctionReferenceNode>& Reference = ReferencesEntry->operator [](ReferenceIndex);
			PerReferenceFunction(Reference);
		}
	}
}

void URigVMBuildData::UpdateReferencesForFunctionReferenceNode(URigVMFunctionReferenceNode* InReferenceNode)
{
	check(InReferenceNode);

	if(InReferenceNode->GetOutermost() == GetTransientPackage())
	{
		return;
	}
	
	if(const URigVMLibraryNode* Function = InReferenceNode->GetReferencedNode())
	{
		const TSoftObjectPtr<URigVMLibraryNode> Key(Function);
		FRigVMFunctionReferenceArray* ReferencesEntry = FunctionReferences.Find(Key);
		if (ReferencesEntry == nullptr)
		{
			Modify();
			FunctionReferences.Add(Key);
			ReferencesEntry = FunctionReferences.Find(Key);
		}

		const FString ReferenceNodePathName = InReferenceNode->GetPathName();
		for (int32 ReferenceIndex = 0; ReferenceIndex < ReferencesEntry->Num(); ReferenceIndex++)
		{
			const TSoftObjectPtr<URigVMFunctionReferenceNode>& Reference = ReferencesEntry->operator [](ReferenceIndex);
			if(Reference.ToString() == ReferenceNodePathName)
			{
				return;
			}
		}

		Modify();
		ReferencesEntry->FunctionReferences.Add(InReferenceNode);
		MarkPackageDirty();
	}
}

void URigVMBuildData::RegisterFunctionReference(URigVMLibraryNode* InFunction,
	URigVMFunctionReferenceNode* InReference)
{
	if(InFunction == nullptr || InReference == nullptr)
	{
		return;
	}

	const TSoftObjectPtr<URigVMLibraryNode> FunctionKey(InFunction);
	const TSoftObjectPtr<URigVMFunctionReferenceNode> ReferenceKey(InReference);

	RegisterFunctionReference(FunctionKey, ReferenceKey);
}

void URigVMBuildData::RegisterFunctionReference(TSoftObjectPtr<URigVMLibraryNode> InFunction,
	TSoftObjectPtr<URigVMFunctionReferenceNode> InReference)
{
	if(InFunction.IsNull() || InReference.IsNull())
	{
		return;
	}

	if(FRigVMFunctionReferenceArray* ReferenceEntry = FunctionReferences.Find(InFunction))
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
		FunctionReferences.Add(InFunction, NewReferenceEntry);
	}
	
	MarkPackageDirty();
}

void URigVMBuildData::RegisterFunctionReference(FRigVMReferenceNodeData InReferenceNodeData)
{
	RegisterFunctionReference(InReferenceNodeData.GetReferencedFunctionObjectPath(), InReferenceNodeData.GetReferenceNodeObjectPath());
}

void URigVMBuildData::UnregisterFunctionReference(URigVMLibraryNode* InFunction,
                                                  URigVMFunctionReferenceNode* InReference)
{
	if(InFunction == nullptr || InReference == nullptr)
	{
		return;
	}

	const TSoftObjectPtr<URigVMLibraryNode> FunctionKey(InFunction);
	const TSoftObjectPtr<URigVMFunctionReferenceNode> ReferenceKey(InReference);

	return UnregisterFunctionReference(FunctionKey, ReferenceKey);
}

void URigVMBuildData::UnregisterFunctionReference(TSoftObjectPtr<URigVMLibraryNode> InFunction,
	TSoftObjectPtr<URigVMFunctionReferenceNode> InReference)
{
	if(InFunction.IsNull() || InReference.IsNull())
	{
		return;
	}

	if(FRigVMFunctionReferenceArray* ReferenceEntry = FunctionReferences.Find(InFunction))
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
	for (TTuple<TSoftObjectPtr<URigVMLibraryNode>, FRigVMFunctionReferenceArray>& FunctionReferenceInfo : FunctionReferences)
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



