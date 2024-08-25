// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailTreeNode.h"

class FCustomDetailsViewItemId
{
	template<typename T>
	static constexpr bool TFindPropertyCondition_V = TModels_V<CStaticClassProvider, T> || TModels_V<CStaticStructProvider, T>;

public:
	// This protects it from being taken by other new items added to EDetailNodeType.
	static constexpr uint32 CustomItemType = static_cast<uint32>(-1);

	FCustomDetailsViewItemId() = default;

	CUSTOMDETAILSVIEW_API static FCustomDetailsViewItemId MakeCategoryId(FName InCategoryName);
	CUSTOMDETAILSVIEW_API static FCustomDetailsViewItemId MakePropertyId(FProperty* Property);
	CUSTOMDETAILSVIEW_API static FCustomDetailsViewItemId MakePropertyId(const TSharedPtr<IPropertyHandle>& InPropertyHandle);
	CUSTOMDETAILSVIEW_API static FCustomDetailsViewItemId MakePropertyId(FName InItemName);

	template <
		typename InOwningStructType
		UE_REQUIRES(TFindPropertyCondition_V<InOwningStructType>)
	>
	static FCustomDetailsViewItemId MakePropertyId(FName InPropertyName)
	{
		if constexpr (TModels<CStaticClassProvider, InOwningStructType>::Value)
		{
			return FCustomDetailsViewItemId::MakePropertyId(FindFProperty<FProperty>(InOwningStructType::StaticClass(), InPropertyName));
		}
		else if constexpr (TModels<CStaticStructProvider, InOwningStructType>::Value)
		{
			return FCustomDetailsViewItemId::MakePropertyId(FindFProperty<FProperty>(InOwningStructType::StaticStruct(), InPropertyName));
		}
		else
		{
			return FCustomDetailsViewItemId();
		}
	}

	static FCustomDetailsViewItemId MakeFromDetailTreeNode(const TSharedRef<IDetailTreeNode>& InDetailTreeNode);

	friend uint32 GetTypeHash(const FCustomDetailsViewItemId& InItemId)
	{
		return InItemId.CachedHash;
	}

	bool operator==(const FCustomDetailsViewItemId& InOtherItemId) const
	{
		return CachedHash == InOtherItemId.CachedHash
			&& ItemType == InOtherItemId.ItemType
			&& ItemName == InOtherItemId.ItemName;
	}

	bool operator!=(const FCustomDetailsViewItemId& InOtherItemId) const
	{
		return !(*this == InOtherItemId);
	}

	bool IsValid() const { return bHasCachedHash; }

	uint32 GetItemType() const { return ItemType; }

	bool IsType(EDetailNodeType InType) const;

private:
	void CalculateTypeHash()
	{
		CachedHash = GetTypeHash(ItemName);
		CachedHash = HashCombine(CachedHash, ItemType);
		bHasCachedHash = true;
	}

	FString ItemName;
	uint32 ItemType     = 0;
	uint32 CachedHash   = 0;
	bool bHasCachedHash = false;
};
