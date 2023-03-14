// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Views/TableViewTypeTraits.h"

class UWidgetBlueprint;

namespace UE::MVVM
{
	struct FBindingSource
	{
		FGuid ViewModelId;
		FName Name;
		TWeakObjectPtr<UClass> Class;
		FText DisplayName;

		bool operator==(const FBindingSource& Other) const
		{
			return ViewModelId == Other.ViewModelId && Name == Other.Name && Class == Other.Class;
		}

		bool operator!=(const FBindingSource& Other) const
		{
			return !(operator==(Other));
		}

		friend int32 GetTypeHash(const FBindingSource& Source)
		{
			uint32 Hash = HashCombine(GetTypeHash(Source.ViewModelId), GetTypeHash(Source.Name));
			Hash = HashCombine(Hash, GetTypeHash(Source.Class));
			return Hash;
		}

		bool IsValid() const
		{
			return Class != nullptr && (ViewModelId.IsValid() || !Name.IsNone());
		}

		void Reset()
		{
			ViewModelId = FGuid();
			Name = FName();
			Class = nullptr;
			DisplayName = FText::GetEmpty();
		}

		static FBindingSource CreateForWidget(const UWidgetBlueprint* WidgetBlueprint, FName WidgetName);
		static FBindingSource CreateForViewModel(const UWidgetBlueprint* WidgetBlueprint, FGuid ViewModelId);
		static FBindingSource CreateForViewModel(const UWidgetBlueprint* WidgetBlueprint, FName ViewModelName);
	};
}

template <>
struct TIsValidListItem<UE::MVVM::FBindingSource>
{
	enum
	{
		Value = true
	};
};

template <>
struct TListTypeTraits<UE::MVVM::FBindingSource>
{
	typedef UE::MVVM::FBindingSource NullableType;

	using MapKeyFuncs = TDefaultMapHashableKeyFuncs<UE::MVVM::FBindingSource, TSharedRef<ITableRow>, false>;
	using MapKeyFuncsSparse = TDefaultMapHashableKeyFuncs<UE::MVVM::FBindingSource, FSparseItemInfo, false>;
	using SetKeyFuncs = DefaultKeyFuncs<UE::MVVM::FBindingSource>;

	template<typename U>
	static void AddReferencedObjects(FReferenceCollector&,
		TArray<UE::MVVM::FBindingSource>&,
		TSet<UE::MVVM::FBindingSource>&,
		TMap<const U*, UE::MVVM::FBindingSource>&)
	{
	}

	static bool IsPtrValid(const UE::MVVM::FBindingSource& InPtr)
	{
		return InPtr.IsValid();
	}

	static void ResetPtr(UE::MVVM::FBindingSource& InPtr)
	{
		InPtr.Reset();
	}

	static UE::MVVM::FBindingSource MakeNullPtr()
	{
		return UE::MVVM::FBindingSource();
	}

	static UE::MVVM::FBindingSource NullableItemTypeConvertToItemType(const UE::MVVM::FBindingSource& InPtr)
	{
		return InPtr;
	}

	static FString DebugDump(UE::MVVM::FBindingSource InPtr)
	{
		return InPtr.DisplayName.ToString();
	}

	class SerializerType {};
};
