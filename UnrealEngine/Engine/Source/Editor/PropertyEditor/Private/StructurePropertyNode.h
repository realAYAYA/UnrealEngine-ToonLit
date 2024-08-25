// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PropertyNode.h"
#include "UObject/StructOnScope.h"
#include "IStructureDataProvider.h"

//-----------------------------------------------------------------------------
//	FStructPropertyNode - Used for the root and various sub-nodes
//-----------------------------------------------------------------------------

class FStructurePropertyNode : public FComplexPropertyNode
{
public:
	FStructurePropertyNode() : FComplexPropertyNode() {}
	virtual ~FStructurePropertyNode() override {}

	virtual FStructurePropertyNode* AsStructureNode() override { return this; }
	virtual const FStructurePropertyNode* AsStructureNode() const override { return this; }

	void RemoveStructure(bool bInDestroySelf = true)
	{
		ClearCachedReadAddresses(true);
		DestroyTree(bInDestroySelf);
		StructProvider = nullptr;
		WeakCachedBaseStruct.Reset();
	}

	void SetStructure(TSharedPtr<FStructOnScope> InStructData)
	{
		RemoveStructure(false);
		if (InStructData)
		{
			StructProvider = MakeShared<FStructOnScopeStructureDataProvider>(InStructData);
		}
	}

	void SetStructure(TSharedPtr<IStructureDataProvider> InStructProvider)
	{
		RemoveStructure(false);
		StructProvider = InStructProvider;
	}

	bool HasValidStructData() const
	{
		return StructProvider.IsValid() && StructProvider->IsValid();
	}

	// Returns just the first structure. Please use GetStructProvider() or GetAllStructureData() when dealing with multiple struct instances.
	TSharedPtr<FStructOnScope> GetStructData() const
	{
		if (StructProvider)
		{
			TArray<TSharedPtr<FStructOnScope>> Instances;
			StructProvider->GetInstances(Instances, WeakCachedBaseStruct.Get());
			
			if (Instances.Num() > 0)
			{
				return Instances[0];
			}
		}
		return nullptr;
	}

	void GetAllStructureData(TArray<TSharedPtr<FStructOnScope>>& OutStructs) const
	{
		if (StructProvider)
		{
			StructProvider->GetInstances(OutStructs, WeakCachedBaseStruct.Get());
		}
	}

	TSharedPtr<IStructureDataProvider> GetStructProvider() const
	{
		return StructProvider;
	}

	virtual bool GetReadAddressUncached(const FPropertyNode& InPropertyNode, FReadAddressListData& OutAddresses) const override;

	virtual bool GetReadAddressUncached(const FPropertyNode& InPropertyNode,
		bool InRequiresSingleSelection,
		FReadAddressListData* OutAddresses,
		bool bComparePropertyContents,
		bool bObjectForceCompare,
		bool bArrayPropertiesCanDifferInSize) const override;

	void GetOwnerPackages(TArray<UPackage*>& OutPackages) const
	{
		if (StructProvider)
		{
			TArray<TSharedPtr<FStructOnScope>> Instances;
			StructProvider->GetInstances(Instances, WeakCachedBaseStruct.Get());

			for (TSharedPtr<FStructOnScope>& Instance : Instances)
			{
				// Returning null for invalid instances, to match instance count.
				OutPackages.Add(Instance.IsValid() ? Instance->GetPackage() : nullptr);
			}
		}
	}

	/** FComplexPropertyNode Interface */
	virtual const UStruct* GetBaseStructure() const override
	{ 
		if (StructProvider)
		{
			return StructProvider->GetBaseStructure();
		}
		return nullptr; 
	}
	virtual UStruct* GetBaseStructure() override
	{
		if (StructProvider)
		{
			return const_cast<UStruct*>(StructProvider->GetBaseStructure());
		}
		return nullptr; 
	}
	virtual TArray<UStruct*> GetAllStructures() override
	{
		TArray<UStruct*> RetVal;
		if (StructProvider)
		{
			TArray<TSharedPtr<FStructOnScope>> Instances;
			StructProvider->GetInstances(Instances, WeakCachedBaseStruct.Get());
			
			for (TSharedPtr<FStructOnScope>& Instance : Instances)
			{
				const UStruct* Struct = Instance.IsValid() ? Instance->GetStruct() : nullptr;
				if (Struct)
				{
					RetVal.AddUnique(const_cast<UStruct*>(Struct));
				}
			}
		}

		return RetVal;
	}
	virtual TArray<const UStruct*> GetAllStructures() const override
	{
		TArray<const UStruct*> RetVal;
		if (StructProvider)
		{
			TArray<TSharedPtr<FStructOnScope>> Instances;
			StructProvider->GetInstances(Instances, WeakCachedBaseStruct.Get());
			
			for (TSharedPtr<FStructOnScope>& Instance : Instances)
			{
				const UStruct* Struct = Instance.IsValid() ? Instance->GetStruct() : nullptr;
				if (Struct)
				{
					RetVal.AddUnique(Struct);
				}
			}
		}
		return RetVal;
	}
	virtual int32 GetInstancesNum() const override
	{
		if (StructProvider)
		{
			TArray<TSharedPtr<FStructOnScope>> Instances;
			StructProvider->GetInstances(Instances, WeakCachedBaseStruct.Get());
			
			return Instances.Num();
		}
		return 0;
		
	}
	virtual uint8* GetMemoryOfInstance(int32 Index) const override
	{
		if (StructProvider)
		{
			TArray<TSharedPtr<FStructOnScope>> Instances;
			StructProvider->GetInstances(Instances, WeakCachedBaseStruct.Get());
			if (Instances.IsValidIndex(Index) && Instances[Index].IsValid())
			{
				return Instances[Index]->GetStructMemory();
			}
		}
		return nullptr;
	}
	virtual uint8* GetValuePtrOfInstance(int32 Index, const FProperty* InProperty, const FPropertyNode* InParentNode) const override
	{ 
		if (InProperty == nullptr || InParentNode == nullptr)
		{
			return nullptr;
		}

		uint8* StructBaseAddress = GetMemoryOfInstance(Index);
		if (StructBaseAddress == nullptr)
		{
			return nullptr;
		}

		uint8* ParentBaseAddress = InParentNode->GetValueAddress(StructBaseAddress, false, /*bIsStruct=*/true);
		if (ParentBaseAddress == nullptr)
		{
			return nullptr;
		}

		return InProperty->ContainerPtrToValuePtr<uint8>(ParentBaseAddress);
	}

	virtual TWeakObjectPtr<UObject> GetInstanceAsUObject(int32 Index) const override
	{
		return nullptr;
	}
	virtual EPropertyType GetPropertyType() const override
	{
		return EPT_StandaloneStructure;
	}

	virtual void Disconnect() override
	{
		ClearCachedReadAddresses(true);
		DestroyTree();
		StructProvider = nullptr;
	}

	/** Generates a single child from the provided property name.  Any existing children are destroyed */
	virtual TSharedPtr<FPropertyNode> GenerateSingleChild(FName ChildPropertyName) override;

protected:

	virtual EPropertyDataValidationResult EnsureDataIsValid() override;

	/** FPropertyNode interface */
	virtual void InitChildNodes() override;

    virtual void InitBeforeNodeFlags() override;
	void InternalInitChildNodes(FName SinglePropertyName);

	virtual uint8* GetValueBaseAddress(uint8* Base, bool bIsSparseData, bool bIsStruct) const override;

	virtual bool GetQualifiedName(FString& PathPlusIndex, const bool bWithArrayIndex, const FPropertyNode* StopParent = nullptr, bool bIgnoreCategories = false) const override
	{
		bool bAddedAnything = false;
		const TSharedPtr<FPropertyNode> ParentNode = ParentNodeWeakPtr.Pin();
		if (ParentNode && StopParent != ParentNode.Get())
		{
			bAddedAnything = ParentNode->GetQualifiedName(PathPlusIndex, bWithArrayIndex, StopParent, bIgnoreCategories);
		}

		if (bAddedAnything)
		{
			PathPlusIndex += TEXT(".");
		}

		PathPlusIndex += TEXT("Struct");
		bAddedAnything = true;

		return bAddedAnything;
	}

private:
	TSharedPtr<IStructureDataProvider> StructProvider;

	/** The base struct at the time InitChildNodes() was called. */
	TWeakObjectPtr<const UStruct> WeakCachedBaseStruct = nullptr;
};
