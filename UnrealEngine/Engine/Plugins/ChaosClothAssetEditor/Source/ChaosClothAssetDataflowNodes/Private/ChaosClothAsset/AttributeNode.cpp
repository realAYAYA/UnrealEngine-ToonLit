// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/AttributeNode.h"
#include "ChaosClothAsset/ClothCollectionGroup.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/ClothDataflowTools.h"
#include "Dataflow/DataflowInputOutput.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AttributeNode)

#define LOCTEXT_NAMESPACE "ChaosClothAssetAttributeNode"

FChaosClothAssetAttributeNode::FChaosClothAssetAttributeNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Collection);
	RegisterOutputConnection(&Collection, &Collection);
	RegisterOutputConnection(&Name);
}

void FChaosClothAssetAttributeNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		using namespace UE::Chaos::ClothAsset;

		// Evaluate in collection
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(InCollection));

		FCollectionClothFacade Cloth(ClothCollection);
		const FName GroupName = *Group.Name;
		
		if (Cloth.IsValid() && !Name.IsEmpty())
		{
			if (ClothCollection->HasGroup(GroupName))
			{
				switch (Type)
				{
				case EChaosClothAssetNodeAttributeType::Integer:
					Cloth.AddUserDefinedAttribute<int32>(*Name, GroupName);
					{
						TArrayView<int32> Values = Cloth.GetUserDefinedAttribute<int32>(*Name, GroupName);
						for (int32& Value : Values)
						{
							Value = IntValue;
						}
					}
					break;
				case EChaosClothAssetNodeAttributeType::Float:
					Cloth.AddUserDefinedAttribute<float>(*Name, GroupName);
					{
						TArrayView<float> Values = Cloth.GetUserDefinedAttribute<float>(*Name, GroupName);
						for (float& Value : Values)
						{
							Value = FloatValue;
						}
					}
					break;
				case EChaosClothAssetNodeAttributeType::Vector:
					Cloth.AddUserDefinedAttribute<FVector3f>(*Name, GroupName);
					{
						TArrayView<FVector3f> Values = Cloth.GetUserDefinedAttribute<FVector3f>(*Name, GroupName);
						for (FVector3f& Value : Values)
						{
							Value = VectorValue;
						}
					}
					break;
				}
			}
			else if (!GroupName.IsNone())
			{
				FClothDataflowTools::LogAndToastWarning(
					*this,
					LOCTEXT("CreateAttributeHeadline", "Invalid Group"),
					FText::Format(LOCTEXT("CreateAttributeDetail", "No group \"{0}\" currently exists on the input collection"), FText::FromName(GroupName)));
			}
		}
		SetValue(Context, MoveTemp(*ClothCollection), &Collection);
	}
	else if (Out->IsA<FString>(&Name))
	{
		SetValue(Context, Name, &Name);
	}
}

void FChaosClothAssetAttributeNode::OnSelected(Dataflow::FContext& Context)
{
	using namespace UE::Chaos::ClothAsset;

	// Re-evaluate the input collection
	FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
	const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(InCollection));
	FCollectionClothFacade Cloth(ClothCollection);

	// Update the list of used group for the UI customization
	const TArray<FName> GroupNames = ClothCollection->GroupNames();
	CachedCollectionGroupNames.Reset(GroupNames.Num());
	for (const FName& GroupName : GroupNames)
	{
		if (Cloth.IsValidClothCollectionGroupName(GroupName))  // Restrict to the cloth facade groups
		{
			CachedCollectionGroupNames.Emplace(GroupName);
		}
	}
}

void FChaosClothAssetAttributeNode::OnDeselected()
{
	// Clean up, to avoid another toolkit picking up the wrong context evaluation
	CachedCollectionGroupNames.Reset();
}


#undef LOCTEXT_NAMESPACE
