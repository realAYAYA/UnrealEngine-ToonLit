// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PropertyNode.h"
#include "UObject/StructOnScope.h"

//-----------------------------------------------------------------------------
//	FStructPropertyNode - Used for the root and various sub-nodes
//-----------------------------------------------------------------------------

class FStructurePropertyNode : public FComplexPropertyNode
{
public:
	FStructurePropertyNode() : FComplexPropertyNode() {}
	virtual ~FStructurePropertyNode() {}

	virtual FStructurePropertyNode* AsStructureNode() override { return this; }
	virtual const FStructurePropertyNode* AsStructureNode() const override { return this; }

	void SetStructure(TSharedPtr<FStructOnScope> InStructData)
	{
		ClearCachedReadAddresses(true);
		DestroyTree();
		StructData = InStructData;
	}

	bool HasValidStructData() const
	{
		return StructData.IsValid() && StructData->IsValid();
	}

	TSharedPtr<FStructOnScope> GetStructData() const
	{
		return StructData;
	}

	bool GetReadAddressUncached(const FPropertyNode& InPropertyNode, FReadAddressListData& OutAddresses) const override
	{
		if (!HasValidStructData())
		{
			return false;
		}

		const FProperty* InItemProperty = InPropertyNode.GetProperty();
		if (!InItemProperty)
		{
			return false;
		}

		UStruct* OwnerStruct = InItemProperty->GetOwnerStruct();
		if (!OwnerStruct || OwnerStruct->IsStructTrashed())
		{
			// Verify that the property is not part of an invalid trash class
			return false;
		}

		uint8* ReadAddress = StructData->GetStructMemory();
		check(ReadAddress);
		OutAddresses.Add(nullptr, InPropertyNode.GetValueBaseAddress(ReadAddress, InPropertyNode.HasNodeFlags(EPropertyNodeFlags::IsSparseClassData) != 0), true);
		return true;
	}

	bool GetReadAddressUncached(const FPropertyNode& InPropertyNode,
		bool InRequiresSingleSelection,
		FReadAddressListData* OutAddresses,
		bool bComparePropertyContents,
		bool bObjectForceCompare,
		bool bArrayPropertiesCanDifferInSize) const override
	{
		if(OutAddresses)
		{
			return GetReadAddressUncached(InPropertyNode, *OutAddresses);
		}
		else
		{
			FReadAddressListData Unused;
			return GetReadAddressUncached(InPropertyNode, Unused);
		}
	}

	UPackage* GetOwnerPackage() const
	{
		return HasValidStructData() ? StructData->GetPackage() : nullptr;
	}

	/** FComplexPropertyNode Interface */
	virtual const UStruct* GetBaseStructure() const override
	{ 
		return HasValidStructData() ? StructData->GetStruct() : nullptr;
	}
	virtual UStruct* GetBaseStructure() override
	{
		const UStruct* Struct = HasValidStructData() ? StructData->GetStruct() : nullptr;
		return const_cast<UStruct*>(Struct);
	}
	virtual TArray<UStruct*> GetAllStructures() override
	{
		TArray<UStruct*> RetVal;
		if (UStruct* BaseStruct = GetBaseStructure())
		{
			RetVal.Add(BaseStruct);
		}
		return RetVal;
	}
	virtual TArray<const UStruct*> GetAllStructures() const override
	{
		TArray<const UStruct*> RetVal;
		if (const UStruct* BaseStruct = GetBaseStructure())
		{
			RetVal.Add(BaseStruct);
		}
		return RetVal;
	}
	virtual int32 GetInstancesNum() const override
	{ 
		return HasValidStructData() ? 1 : 0;
	}
	virtual uint8* GetMemoryOfInstance(int32 Index) const override
	{ 
		check(0 == Index);
		return HasValidStructData() ? StructData->GetStructMemory() : NULL;
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

		uint8* ParentBaseAddress = InParentNode->GetValueAddress(StructBaseAddress, false);
		if (ParentBaseAddress == nullptr)
		{
			return nullptr;
		}

		return InProperty->ContainerPtrToValuePtr<uint8>(ParentBaseAddress);
	}

	virtual TWeakObjectPtr<UObject> GetInstanceAsUObject(int32 Index) const override
	{
		check(0 == Index);
		return NULL;
	}
	virtual EPropertyType GetPropertyType() const override
	{
		return EPT_StandaloneStructure;
	}

	virtual void Disconnect() override
	{
		SetStructure(NULL);
	}

protected:

	/** FPropertyNode interface */
	virtual void InitChildNodes() override;

	virtual uint8* GetValueBaseAddress(uint8* Base, bool bIsSparseData) const override
	{
		check(bIsSparseData == false);
		return HasValidStructData() ? StructData->GetStructMemory() : nullptr;
	}

	virtual bool GetQualifiedName(FString& PathPlusIndex, const bool bWithArrayIndex, const FPropertyNode* StopParent = nullptr, bool bIgnoreCategories = false) const override
	{
		PathPlusIndex += TEXT("Struct");
		return true;
	}

private:
	TSharedPtr<FStructOnScope> StructData;
};

