// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/CollectionClothSelectionFacade.h"
#include "GeometryCollection/ManagedArrayCollection.h"

#define LOCTEXT_NAMESPACE "FCollectionClothSelectionFacade"

namespace UE::Chaos::ClothAsset
{

	namespace Private
	{
		static const FName SelectionGroup(TEXT("Selection"));
		static const FName NameAttribute(TEXT("Name"));
		static const FName TypeAttribute(TEXT("Type"));
		static const FName IndicesAttribute(TEXT("Indices"));
	}

	// --------------- FCollectionClothSelectionConstFacade -------------------------------

	FCollectionClothSelectionConstFacade::FCollectionClothSelectionConstFacade(const TSharedRef<const FManagedArrayCollection>& InManagedArrayCollection) :
		ManagedArrayCollection(ConstCastSharedRef<FManagedArrayCollection>(InManagedArrayCollection))
	{
		Name = ManagedArrayCollection->FindAttribute<FString>(Private::NameAttribute, Private::SelectionGroup);
		Type = ManagedArrayCollection->FindAttribute<FString>(Private::TypeAttribute, Private::SelectionGroup);
		Indices = ManagedArrayCollection->FindAttribute<TSet<int32>>(Private::IndicesAttribute, Private::SelectionGroup);
	}

	bool FCollectionClothSelectionConstFacade::IsValid() const
	{
		return Name && Type && Indices;
	}

	int32 FCollectionClothSelectionConstFacade::GetNumSelections() const
	{
		return ManagedArrayCollection->NumElements(Private::SelectionGroup);
	}

	const TArrayView<const FString> FCollectionClothSelectionConstFacade::GetName() const
	{
		if (Name)
		{
			return TArrayView<const FString>(Name->GetData(), Name->Num());
		}
		return TArrayView<const FString>();
	}

	const TArrayView<const FString> FCollectionClothSelectionConstFacade::GetType() const
	{
		if (Type)
		{
			return TArrayView<const FString>(Type->GetData(), Type->Num());
		}
		return TArrayView<const FString>();
	}

	const TArrayView<const TSet<int32>> FCollectionClothSelectionConstFacade::GetIndices() const
	{
		if (Indices)
		{
			return TArrayView<const TSet<int32>>(Indices->GetData(), Indices->Num());
		}
		return TArrayView<const TSet<int32>>();
	}

	int32 FCollectionClothSelectionConstFacade::FindSelection(const FString& SearchName) const
	{
		const TArrayView<const FString> Names = GetName();
		for (int32 SelectionIndex = 0; SelectionIndex < Names.Num(); ++SelectionIndex)
		{
			if (Names[SelectionIndex] == SearchName)
			{
				return SelectionIndex;
			}
		}
		return INDEX_NONE;
	}


	// --------------- FCollectionClothSelectionFacade -------------------------------

	FCollectionClothSelectionFacade::FCollectionClothSelectionFacade(const TSharedRef<const FManagedArrayCollection>& ManagedArrayCollection) :
		FCollectionClothSelectionConstFacade(ManagedArrayCollection)
	{}

	void FCollectionClothSelectionFacade::DefineSchema()
	{
		if (!ManagedArrayCollection->HasGroup(Private::SelectionGroup))
		{
			ManagedArrayCollection->AddGroup(Private::SelectionGroup);
		}

		// AddAttribute will only add the attribute if it doesn't already exist, otherwise it will return the existing one
		Name = &ManagedArrayCollection->AddAttribute<FString>(Private::NameAttribute, Private::SelectionGroup);
		Type = &ManagedArrayCollection->AddAttribute<FString>(Private::TypeAttribute, Private::SelectionGroup);
		Indices = &ManagedArrayCollection->AddAttribute<TSet<int32>>(Private::IndicesAttribute, Private::SelectionGroup);
	}

	TArrayView<FString> FCollectionClothSelectionFacade::GetName()
	{
		if (Name)
		{
			return TArrayView<FString>(Name->GetData(), Name->Num());
		}
		return TArrayView<FString>();
	}

	TArrayView<FString> FCollectionClothSelectionFacade::GetType()
	{
		if (Type)
		{
			return TArrayView<FString>(Type->GetData(), Type->Num());
		}
		return TArrayView<FString>();
	}

	TArrayView<TSet<int32>> FCollectionClothSelectionFacade::GetIndices()
	{
		if (Indices)
		{
			return TArrayView<TSet<int32>>(Indices->GetData(), Indices->Num());
		}
		return TArrayView<TSet<int32>>();
	}

	int32 FCollectionClothSelectionFacade::FindOrAddSelection(const FString& InName)
	{
		if (!IsValid())
		{
			return INDEX_NONE;
		}

		// Check for existing selection with matching Name
		const int32 FoundIndex = FindSelection(InName);
		if (FoundIndex != INDEX_NONE)
		{
			return FoundIndex;
		}

		// No selection with the given name exists
		checkf(ManagedArrayCollection->HasGroup(Private::SelectionGroup), TEXT("Expected SelectionGroup to exist in the managed array at this point"));

		const int32 NewIndex = ManagedArrayCollection->AddElements(1, Private::SelectionGroup);
		
		GetName()[NewIndex] = InName;

		return NewIndex;
	}

	bool FCollectionClothSelectionFacade::SetSelection(int32 SelectionIndex, const FString& InType, const TSet<int32>& InSelectedIndices)
	{
		if (!IsValid())
		{
			return false;
		}

		if (SelectionIndex < 0 || SelectionIndex >= GetNumSelections())
		{
			return false;
		}

		GetType()[SelectionIndex] = InType;
		GetIndices()[SelectionIndex] = InSelectedIndices;
		return true;
	}


}	// namespace  UE::Chaos::ClothAsset

#undef LOCTEXT_NAMESPACE
