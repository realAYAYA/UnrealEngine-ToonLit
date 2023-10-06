// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/MergeClothCollectionsNode.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "Dataflow/DataflowInputOutput.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MergeClothCollectionsNode)

#define LOCTEXT_NAMESPACE "ChaosClothAssetMergeClothCollectionsNode"

FChaosClothAssetMergeClothCollectionsNode::FChaosClothAssetMergeClothCollectionsNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Collection);
	RegisterOutputConnection(&Collection, &Collection);
}

void FChaosClothAssetMergeClothCollectionsNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		using namespace UE::Chaos::ClothAsset;
		using namespace Chaos::Softs;

		// Evaluate in collection
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(InCollection));

		// Keep track of whether any of these collections are valid cloth collections
		FCollectionClothFacade ClothFacade(ClothCollection);
		bool bAreAnyValid = ClothFacade.IsValid();

		// Make it a valid cloth collection if needed
		if (!bAreAnyValid)
		{
			ClothFacade.DefineSchema();
		}

		FCollectionPropertyMutableFacade PropertyFacade(ClothCollection);
		bAreAnyValid |= PropertyFacade.IsValid();

		// Iterate through the inputs and append them to LOD 0
		const TArray<const FManagedArrayCollection*> Collections = GetCollections();
		for (int32 InputIndex = 1; InputIndex < Collections.Num(); ++InputIndex)
		{
			FManagedArrayCollection OtherCollection = GetValue<FManagedArrayCollection>(Context, Collections[InputIndex]);  // Can't use a const reference here sadly since the facade needs a SharedRef to be created
			const TSharedRef<const FManagedArrayCollection> OtherClothCollection = MakeShared<const FManagedArrayCollection>(MoveTemp(OtherCollection));
			const FCollectionClothConstFacade OtherClothFacade(OtherClothCollection);
			if (OtherClothFacade.IsValid())
			{
				ClothFacade.Append(OtherClothFacade);
				bAreAnyValid = true;
			}
			// Copy properties
			const FCollectionPropertyConstFacade OtherPropertyFacade(OtherClothCollection);
			if (OtherPropertyFacade.IsValid())
			{
				constexpr bool bUpdateExistingProperties = true; // Want last one wins.
				PropertyFacade.Append(OtherClothCollection.ToSharedPtr(), bUpdateExistingProperties);
				bAreAnyValid = true;
			}
		}

		// Set the output
		if (bAreAnyValid)
		{
			// Use the merged cloth collection, but only if there were at least one valid input cloth collections
			SetValue(Context, MoveTemp(*ClothCollection), &Collection);
		}
		else
		{
			// Otherwise pass through the first input unchanged
			const FManagedArrayCollection& Passthrough = GetValue<FManagedArrayCollection>(Context, &Collection);
			SetValue(Context, Passthrough, &Collection);
		}
	}
}

Dataflow::FPin FChaosClothAssetMergeClothCollectionsNode::AddPin()
{
	auto AddInput = [this](const FManagedArrayCollection* InCollection) -> Dataflow::FPin
	{
		RegisterInputConnection(InCollection);
		const FDataflowInput* const Input = FindInput(InCollection);
		return { Dataflow::FPin::EDirection::INPUT, Input->GetType(), Input->GetName() };
	};

	switch (NumInputs)
	{
	case 1: ++NumInputs; return AddInput(&Collection1);
	case 2: ++NumInputs; return AddInput(&Collection2);
	case 3: ++NumInputs; return AddInput(&Collection3);
	case 4: ++NumInputs; return AddInput(&Collection4);
	case 5: ++NumInputs; return AddInput(&Collection5);
	default: break;
	}

	return Super::AddPin();
}

Dataflow::FPin FChaosClothAssetMergeClothCollectionsNode::RemovePin()
{
	auto RemoveInput = [this](const FManagedArrayCollection* InCollection) -> Dataflow::FPin
	{
		const FDataflowInput* const Input = FindInput(InCollection);
		check(Input);
		Dataflow::FPin Pin = { Dataflow::FPin::EDirection::INPUT, Input->GetType(), Input->GetName() };
		UnregisterInputConnection(InCollection);  // This will delete the input, so set the pin before that
		return Pin;
	};

	switch (NumInputs - 1)
	{
	case 1: --NumInputs; return RemoveInput(&Collection1);
	case 2: --NumInputs; return RemoveInput(&Collection2);
	case 3: --NumInputs; return RemoveInput(&Collection3);
	case 4: --NumInputs; return RemoveInput(&Collection4);
	case 5: --NumInputs; return RemoveInput(&Collection5);
	default: break;
	}
	return Super::AddPin();
}

TArray<const FManagedArrayCollection*> FChaosClothAssetMergeClothCollectionsNode::GetCollections() const
{
	TArray<const FManagedArrayCollection*> Collections;
	Collections.SetNumUninitialized(NumInputs);

	for (int32 InputIndex = 0; InputIndex < NumInputs; ++InputIndex)
	{
		switch (InputIndex)
		{
		case 0: Collections[InputIndex] = &Collection; break;
		case 1: Collections[InputIndex] = &Collection1; break;
		case 2: Collections[InputIndex] = &Collection2; break;
		case 3: Collections[InputIndex] = &Collection3; break;
		case 4: Collections[InputIndex] = &Collection4; break;
		case 5: Collections[InputIndex] = &Collection5; break;
		default: Collections[InputIndex] = nullptr; check(false); break;
		}
	}
	return Collections;
}

void FChaosClothAssetMergeClothCollectionsNode::Serialize(FArchive& Ar)
{
	if (Ar.IsLoading())
	{
		const int32 NumInputsToAdd = NumInputs - 1;
		NumInputs = 1;  // AddPin will increment it again
		for (int32 InputIndex = 0; InputIndex < NumInputsToAdd; ++InputIndex)
		{
			AddPin();
		}
		check(NumInputsToAdd == NumInputs - 1);
	}
}

#undef LOCTEXT_NAMESPACE
