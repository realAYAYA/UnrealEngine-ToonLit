// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/MergeClothCollectionsNode.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/CollectionClothSelectionFacade.h"
#include "ChaosClothAsset/ClothDataflowTools.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Engine/SkeletalMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MergeClothCollectionsNode)

#define LOCTEXT_NAMESPACE "ChaosClothAssetMergeClothCollectionsNode"

namespace UE::Chaos::ClothAsset::Private
{
	static void LogAndToastDifferentWeightMapNames(const FDataflowNode& DataflowNode, const FString& PropertyName, const FString& InWeightMapName, const FString& OutWeightMapName, const FString& WeightMapName)
	{
		using namespace UE::Chaos::ClothAsset;

		static const FText Headline = LOCTEXT("DifferentWeightMapNamesHeadline", "Different weight map names.");

		const FText Details = FText::Format(
			LOCTEXT(
				"DifferentWeightMapNamesDetails",
				"Two identical Cloth Collection properties '{0}' are being merged but have different weight map names '{1}' and '{2}'. The weight map named '{3}' will be used in the resulting merge."),
			FText::FromString(PropertyName),
			FText::FromString(OutWeightMapName),
			FText::FromString(InWeightMapName),
			FText::FromString(WeightMapName));

		FClothDataflowTools::LogAndToastWarning(DataflowNode, Headline, Details);
	}

	struct FMergedProperty
	{
		FString WeightMapName;
		FVector4f PropertyBounds;
	};

	/** Build weight maps for each properties if necessary */
	static FString BuildWeightMaps(const FDataflowNode& DataflowNode,
		const FCollectionClothConstFacade& InClothFacade, FCollectionClothFacade& OutClothFacade,
		const FVector2f& InPropertyBounds, const FVector2f& OutPropertyBounds,
		const FVector2f& PropertyBounds, const FString& PropertyName,
		const FString& InWeightMapName, const FString& OutWeightMapName, TMap<FString,FMergedProperty>& MergedPropertyMaps)
	{
		const FMergedProperty MergedProperty = {OutWeightMapName + FString(TEXT("_")) + InWeightMapName,
			FVector4f(InPropertyBounds[0], InPropertyBounds[1], OutPropertyBounds[0], OutPropertyBounds[1])};

		for(const TPair<FString, FMergedProperty>& MergedPropertyMap : MergedPropertyMaps)
		{
			if((MergedPropertyMap.Value.WeightMapName == MergedProperty.WeightMapName) &&
			   (MergedPropertyMap.Value.PropertyBounds == MergedProperty.PropertyBounds))
			{
				return MergedPropertyMap.Key;
			}
		}
		FString WeightMapName = PropertyName;
		int32 WeightMapCount = 0;
		
		// the weight map could already been stored on the out collection and linked to different bounds
		// coming from the out collection itself or from previous merge with in collection
		// Since we don't want to break them we need to create a new one on the first available slot
		while(OutClothFacade.GetWeightMap(FName(WeightMapName)).Num() > 0)
		{
			WeightMapName = PropertyName;
			WeightMapName.AppendInt(++WeightMapCount);
		}
		
		// If the low high values of the merged property are the same we don't need to build a weight map
		if(PropertyBounds[0] != PropertyBounds[1])
		{
			// If names are different we must let the user know
			if ((!InWeightMapName.IsEmpty() && InWeightMapName != WeightMapName) || (!OutWeightMapName.IsEmpty() && OutWeightMapName != WeightMapName))
			{
				Private::LogAndToastDifferentWeightMapNames(DataflowNode, PropertyName, InWeightMapName, OutWeightMapName, WeightMapName);	
			}
			MergedPropertyMaps.Add(WeightMapName,MergedProperty);
			
			// Create if necessary a new weight map
			OutClothFacade.AddWeightMap(FName(WeightMapName));
			TArrayView<float> WeightMap = OutClothFacade.GetWeightMap(FName(WeightMapName));

			auto FillWeightMap = [&PropertyBounds, &WeightMap](const TConstArrayView<float> InWeightMap, const FVector2f& InPropertyBounds,
				const int32 InNumVertices, const int32 OutVertexOffset)
			{
				const bool bHasAlreadyValues = (InWeightMap.Num() > 0);
				for(int32 VertexIndex = 0; VertexIndex < InNumVertices; ++VertexIndex)
                {
                	// If no values in the weight map we are using the low value
					const float WeightMapValue = bHasAlreadyValues ? (InWeightMap[VertexIndex] * (InPropertyBounds[1] - InPropertyBounds[0]) +
						InPropertyBounds[0]) : InPropertyBounds[0];
					WeightMap[OutVertexOffset+VertexIndex] = (WeightMapValue - PropertyBounds[0]) / (PropertyBounds[1] - PropertyBounds[0]);
                }
			};
			const int32 InNumVertices = InClothFacade.GetNumSimVertices3D();
			const int32 OutNumVertices = OutClothFacade.GetNumSimVertices3D() - InNumVertices;
			
			FillWeightMap(OutClothFacade.GetWeightMap(FName(OutWeightMapName)), OutPropertyBounds, OutNumVertices, 0);
			FillWeightMap(InClothFacade.GetWeightMap(FName(InWeightMapName)), InPropertyBounds, InNumVertices, OutNumVertices);
		}
		return WeightMapName;
	}

	// Merge the property bounds of 2 collections
	static FVector2f MergePropertyBounds(const FVector2f& InPropertyBounds, const FVector2f& OutPropertyBounds)
	{
		FVector2f PropertyBounds(0.0f);
		if(InPropertyBounds[0] <= InPropertyBounds[1])
		{
			if(OutPropertyBounds[0] <= OutPropertyBounds[1])
			{
				PropertyBounds[0] = FMath::Min(InPropertyBounds[0], OutPropertyBounds[0]);
				PropertyBounds[1] = FMath::Max(InPropertyBounds[1], OutPropertyBounds[1]);
			}
			else
			{
				PropertyBounds[0] = FMath::Min(InPropertyBounds[0], OutPropertyBounds[1]);
				PropertyBounds[1] = FMath::Max(InPropertyBounds[1], OutPropertyBounds[0]);
			}
		}
		else
		{
			if(OutPropertyBounds[0] <= OutPropertyBounds[1])
			{
				PropertyBounds[0] = FMath::Min(InPropertyBounds[1], OutPropertyBounds[0]);
				PropertyBounds[1] = FMath::Max(InPropertyBounds[0], OutPropertyBounds[1]);
			}
			else
			{
				PropertyBounds[0] = FMath::Min(InPropertyBounds[1], OutPropertyBounds[1]);
				PropertyBounds[1] = FMath::Max(InPropertyBounds[0], OutPropertyBounds[0]);
			}
		}
		if (FMath::IsNearlyEqual(PropertyBounds[0], PropertyBounds[1]))
		{
			PropertyBounds[1] = PropertyBounds[0];
		}
		return PropertyBounds;
	}

	static ::Chaos::Softs::ECollectionPropertyFlags MergePropertyFlags(
		const ::Chaos::Softs::FCollectionPropertyConstFacade& InPropertyFacade,
			  ::Chaos::Softs::FCollectionPropertyMutableFacade& OutPropertyFacade,
			  const int32 InKeyIndex,
			  const int32 OutKeyIndex,
			  const ::Chaos::Softs::ECollectionPropertyFlags InPropertyFlags,
			  const FString& PropertyName)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		// TODO: GetFlags needs to return an ECollectionPropertyFlags, not an uint8, but the uint8 getter needs to be deprecated first
		const ::Chaos::Softs::ECollectionPropertyFlags OutPropertyFlags = (::Chaos::Softs::ECollectionPropertyFlags)OutPropertyFacade.GetFlags(OutKeyIndex);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		
		::Chaos::Softs::ECollectionPropertyFlags PropertyFlags;
		if(!OutPropertyFacade.IsEnabled(OutKeyIndex) && InPropertyFacade.IsEnabled(InKeyIndex))
		{
			PropertyFlags = InPropertyFlags;
		}
		else if(OutPropertyFacade.IsEnabled(OutKeyIndex) && !InPropertyFacade.IsEnabled(InKeyIndex))
		{
			PropertyFlags = OutPropertyFlags;
		}
		else
		{
			PropertyFlags = OutPropertyFlags;
			if(OutPropertyFacade.IsAnimatable(OutKeyIndex) || InPropertyFacade.IsAnimatable(InKeyIndex))
			{
				EnumAddFlags(PropertyFlags, ::Chaos::Softs::ECollectionPropertyFlags::Animatable);  // Animatable
			}
			if(!ensure(OutPropertyFacade.IsIntrinsic(OutKeyIndex) == InPropertyFacade.IsIntrinsic(InKeyIndex)))
			{
				UE_LOG(LogChaosClothAssetDataflowNodes, Warning, TEXT("MergeClothCollectionsNode: Mismatch in intrinsic flag onto %s property"), *PropertyName);
			}
			if(!ensure(OutPropertyFacade.IsLegacy(OutKeyIndex) == InPropertyFacade.IsLegacy(InKeyIndex)))
			{
				UE_LOG(LogChaosClothAssetDataflowNodes, Warning, TEXT("MergeClothCollectionsNode: Mismatch in legacy flag onto %s property"), *PropertyName);
			}
			if(!ensure(OutPropertyFacade.IsInterpolable(OutKeyIndex) == InPropertyFacade.IsInterpolable(InKeyIndex)))
			{
				UE_LOG(LogChaosClothAssetDataflowNodes, Warning, TEXT("MergeClothCollectionsNode: Mismatch in interpolable flag onto %s property"), *PropertyName);
			}
		}
		return PropertyFlags;
	}

	/** Append input properties to the output property facade and add potential weight maps */
	static void AppendInputProperties(const FDataflowNode& DataflowNode,
		const FCollectionClothConstFacade& InClothFacade,
			  FCollectionClothFacade& OutClothFacade,
		const ::Chaos::Softs::FCollectionPropertyConstFacade& InPropertyFacade,
			  ::Chaos::Softs::FCollectionPropertyMutableFacade& OutPropertyFacade)
	{
		const int32 InNumInKeys = InPropertyFacade.Num();
		TMap<FString,FMergedProperty> MergedPropertyMaps;
		for (int32 InKeyIndex = 0; InKeyIndex < InNumInKeys; ++InKeyIndex)
		{
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			// TODO: GetFlags needs to return an ECollectionPropertyFlags, not an uint8, but the uint8 getter needs to be deprecated first
			const ::Chaos::Softs::ECollectionPropertyFlags InPropertyFlags = (::Chaos::Softs::ECollectionPropertyFlags)InPropertyFacade.GetFlags(InKeyIndex);
			PRAGMA_ENABLE_DEPRECATION_WARNINGS

			// Get the matching output key for the given input one 
			const FString& InPropertyKey = InPropertyFacade.GetKey(InKeyIndex);
			int32 OutKeyIndex = OutPropertyFacade.GetKeyIndex(InPropertyKey);

			// We first check if the output key exists into the output facade
			bool bOverrideProperty = true;
			if(OutKeyIndex != INDEX_NONE)
			{
				if(InPropertyFacade.IsInterpolable(InKeyIndex))
				{
					// If it exists we compute the min of the property low values and the max of the property high values
					const FVector2f InPropertyBounds = InPropertyFacade.GetWeightedFloatValue(InKeyIndex);
					const FVector2f OutPropertyBounds = OutPropertyFacade.GetWeightedFloatValue(OutKeyIndex);
					const FVector2f PropertyBounds = MergePropertyBounds(InPropertyBounds, OutPropertyBounds);
					
					const ::Chaos::Softs::ECollectionPropertyFlags PropertyFlags = MergePropertyFlags(
						InPropertyFacade, OutPropertyFacade, InKeyIndex, OutKeyIndex, InPropertyFlags, InPropertyKey);
					
                    OutPropertyFacade.SetFlags(OutKeyIndex, PropertyFlags);
                    OutPropertyFacade.SetWeightedFloatValue(OutKeyIndex, PropertyBounds);
    
                    // We keep the string value to be the one in the output if defined
                    const FString WeightMapName = BuildWeightMaps(DataflowNode, InClothFacade, OutClothFacade,
                    	InPropertyBounds, OutPropertyBounds, PropertyBounds, InPropertyKey,
							InPropertyFacade.GetStringValue(InKeyIndex), OutPropertyFacade.GetStringValue(OutKeyIndex), MergedPropertyMaps);

					OutPropertyFacade.SetStringValue(OutKeyIndex, WeightMapName);
					bOverrideProperty = false;
				}
			}
			else
			{
				// If not we add a new property with the flags/bounds/string of the input one
				if (!OutPropertyFacade.IsValid())
				{
					OutPropertyFacade.DefineSchema();
				}
				OutKeyIndex = OutPropertyFacade.AddProperty(InPropertyKey, InPropertyFlags);
			}
			if(bOverrideProperty)
			{
				OutPropertyFacade.SetFlags(OutKeyIndex, InPropertyFlags);
				OutPropertyFacade.SetWeightedValue(OutKeyIndex, InPropertyFacade.GetLowValue<FVector3f>(InKeyIndex), InPropertyFacade.GetHighValue<FVector3f>(InKeyIndex));
				OutPropertyFacade.SetStringValue(OutKeyIndex, InPropertyFacade.GetStringValue(InKeyIndex));
			}
		}
	}

	static bool AreSkeletalMeshesCompatible(const FDataflowNode& DataflowNode, const FCollectionClothConstFacade& Cloth1, const FCollectionClothConstFacade& Cloth2)
	{
		/** Disallow merging cloth facades with incompatible ref skeletons. */
		const FString& SkeletalMeshPathName1 = Cloth1.GetSkeletalMeshPathName();
		const FString& SkeletalMeshPathName2 = Cloth2.GetSkeletalMeshPathName();
		if (SkeletalMeshPathName1.IsEmpty() || SkeletalMeshPathName2.IsEmpty() || SkeletalMeshPathName1 == SkeletalMeshPathName2)
		{
			return true;
		}

		static const FText ErrorHeadline = LOCTEXT("IncompatibleSkeletalMeshesHeadline", "Incompatible Skeletal Meshes.");
		const USkeletalMesh* const SkeletalMesh1 = LoadObject<USkeletalMesh>(nullptr, *SkeletalMeshPathName1, nullptr, LOAD_None, nullptr);
		const USkeletalMesh* const SkeletalMesh2 = LoadObject<USkeletalMesh>(nullptr, *SkeletalMeshPathName2, nullptr, LOAD_None, nullptr);
		if (!SkeletalMesh1 || !SkeletalMesh2)
		{
			const FText Details = FText::Format(
				LOCTEXT(
					"IncompatibleSkeletalMeshesLoadFailureDetails",
					"Cloth collections failed to merge due to failing to load SkeletalMesh \"{0}\" to check compatibility."),
				!SkeletalMesh1 ? FText::FromString(SkeletalMeshPathName1) : FText::FromString(SkeletalMeshPathName2));

			FClothDataflowTools::LogAndToastWarning(DataflowNode, ErrorHeadline, Details);
			return false;
		}

		const FReferenceSkeleton& RefSkeleton1 = SkeletalMesh1->GetRefSkeleton();
		const FReferenceSkeleton& RefSkeleton2 = SkeletalMesh2->GetRefSkeleton();
		if (RefSkeleton1.GetNum() != RefSkeleton2.GetNum())
		{
			const FText Details = FText::Format(
				LOCTEXT(
					"IncompatibleSkeletalMeshesNumBonesDetails",
					"Cloth collections failed to merge due to incompatible Skeletal Meshes, \"{0}\" and \"{1}\". RefSkeleton Bone counts {2} != {3}."),
				FText::FromString(SkeletalMeshPathName1),
				FText::FromString(SkeletalMeshPathName2),
				RefSkeleton1.GetNum(),
				RefSkeleton2.GetNum());

			FClothDataflowTools::LogAndToastWarning(DataflowNode, ErrorHeadline, Details);
			return false;
		}

		const TArray<FMeshBoneInfo>& RefBoneInfo1 = RefSkeleton1.GetRefBoneInfo();
		const TArray<FMeshBoneInfo>& RefBoneInfo2 = RefSkeleton2.GetRefBoneInfo();
		const TArray<FTransform>& RefBonePose1 = RefSkeleton1.GetRefBonePose();
		const TArray<FTransform>& RefBonePose2 = RefSkeleton2.GetRefBonePose();
		for (int32 BoneIndex = 0; BoneIndex < RefSkeleton1.GetNum(); ++BoneIndex)
		{
			if (!(RefBoneInfo1[BoneIndex] == RefBoneInfo2[BoneIndex]))
			{
				const FText Details = FText::Format(
					LOCTEXT(
						"IncompatibleSkeletalMeshesRefBoneInfoDetails",
						"Cloth collections failed to merge due to incompatible Skeletal Meshes, \"{0}\" and \"{1}\". RefBoneInfos are mismatched at BoneIndex {2}."),
					FText::FromString(SkeletalMeshPathName1),
					FText::FromString(SkeletalMeshPathName2),
					BoneIndex);

				FClothDataflowTools::LogAndToastWarning(DataflowNode, ErrorHeadline, Details);
				return false;
			}
			if (!RefBonePose1[BoneIndex].Equals(RefBonePose2[BoneIndex]))
			{
				const FText Details = FText::Format(
					LOCTEXT(
						"IncompatibleSkeletalMeshesRefBonePoseDetails",
						"Cloth collections failed to merge due to incompatible Skeletal Meshes, \"{0}\" and \"{1}\". RefBonePoses are mismatched at BoneIndex {2}."),
					FText::FromString(SkeletalMeshPathName1),
					FText::FromString(SkeletalMeshPathName2),
					BoneIndex);

				FClothDataflowTools::LogAndToastWarning(DataflowNode, ErrorHeadline, Details);
				return false;
			}
		}

		return true;
	}
}

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

		FCollectionClothSelectionFacade SelectionFacade(ClothCollection);
		bAreAnyValid |= SelectionFacade.IsValid();

		// Iterate through the inputs and append them to LOD 0
		const TArray<const FManagedArrayCollection*> Collections = GetCollections();
		for (int32 InputIndex = 1; InputIndex < Collections.Num(); ++InputIndex)
		{
			FManagedArrayCollection OtherCollection = GetValue<FManagedArrayCollection>(Context, Collections[InputIndex]);  // Can't use a const reference here sadly since the facade needs a SharedRef to be created
			const TSharedRef<const FManagedArrayCollection> OtherClothCollection = MakeShared<const FManagedArrayCollection>(MoveTemp(OtherCollection));

			// Selections need to update with offsets. Gather offsets before appending cloth data.
			const FCollectionClothSelectionConstFacade OtherSelectionFacade(OtherClothCollection);
			TMap<FName, int32> GroupNameOffsets;
			if (OtherSelectionFacade.IsValid())
			{
				const TArray<FName> SelectionNames = OtherSelectionFacade.GetNames();
				for (const FName& SelectionName : SelectionNames)
				{
					const FName GroupName = OtherSelectionFacade.GetSelectionGroup(SelectionName);
					if (!GroupNameOffsets.Find(GroupName))
					{
						GroupNameOffsets.Add(GroupName) = ClothCollection->NumElements(GroupName); // NumElements will return zero if the group doesn't exist.
					}
				}
			}

			// Append cloth
			const FCollectionClothConstFacade OtherClothFacade(OtherClothCollection);
			if (OtherClothFacade.IsValid() && Private::AreSkeletalMeshesCompatible(*this, ClothFacade, OtherClothFacade))
			{
				ClothFacade.Append(OtherClothFacade);
				bAreAnyValid = true;
			}

			// Append selections (with offsets)
			if (OtherSelectionFacade.IsValid())
			{
				constexpr bool bUpdateExistingSelections = true; // Want last one wins.
				SelectionFacade.AppendWithOffsets(OtherSelectionFacade, bUpdateExistingSelections, GroupNameOffsets);
				bAreAnyValid = true;
			}

			// Copy properties
			const FCollectionPropertyConstFacade OtherPropertyFacade(OtherClothCollection);
			if (OtherPropertyFacade.IsValid())
			{
				// Change that boolean to come back to the old behavior
				static constexpr bool bOverrideProperties = false;
				if(bOverrideProperties)
				{
					constexpr bool bUpdateExistingProperties = true; // Want last one wins.
					PropertyFacade.Append(OtherClothCollection.ToSharedPtr(), bUpdateExistingProperties);
				}
				else
				{
					Private::AppendInputProperties(*this, OtherClothFacade, ClothFacade, OtherPropertyFacade, PropertyFacade);
				}
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

Dataflow::FPin FChaosClothAssetMergeClothCollectionsNode::GetPinToRemove() const
{
	auto PinToRemove = [this](const FManagedArrayCollection* InCollection) -> Dataflow::FPin
	{
		const FDataflowInput* const Input = FindInput(InCollection);
		check(Input);
		return { Dataflow::FPin::EDirection::INPUT, Input->GetType(), Input->GetName() };
	};

	switch (NumInputs - 1)
	{
	case 1: return PinToRemove(&Collection1);
	case 2: return PinToRemove(&Collection2);
	case 3: return PinToRemove(&Collection3);
	case 4: return PinToRemove(&Collection4);
	case 5: return PinToRemove(&Collection5);
	default: break;
	}
	return Super::GetPinToRemove();
}

void FChaosClothAssetMergeClothCollectionsNode::OnPinRemoved(const Dataflow::FPin& Pin)
{
	auto CheckPinRemoved = [this, &Pin](const FManagedArrayCollection* InCollection)
	{
		check(Pin.Direction == Dataflow::FPin::EDirection::INPUT);
#if DO_CHECK
		const FDataflowInput* const Input = FindInput(InCollection);
		check(Input);
		check(Input->GetName() == Pin.Name);
		check(Input->GetType() == Pin.Type);
#endif
	};

	switch (NumInputs - 1)
	{
	case 1:
		CheckPinRemoved(&Collection1);
		--NumInputs;
		break;
	case 2:
		CheckPinRemoved(&Collection2);
		--NumInputs;
		break;
	case 3:
		CheckPinRemoved(&Collection3);
		--NumInputs;
		break;
	case 4:
		CheckPinRemoved(&Collection4);
		--NumInputs;
		break;
	case 5:
		CheckPinRemoved(&Collection5);
		--NumInputs;
		break;
	default:
		checkNoEntry();
		break;
	}

	return Super::OnPinRemoved(Pin);
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
