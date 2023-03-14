// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GeometryCollection.cpp: FGeometryCollection methods.
=============================================================================*/

#include "GeometryCollection/Facades/CollectionSelectionFacade.h"

namespace GeometryCollection::Facades
{

	// Groups 
	const FName FSelectionFacade::UnboundGroup = "Unbound";
	const FName FSelectionFacade::WeightedUnboundGroup = "WeightedUnbound";
	const FName FSelectionFacade::BoundGroup = "Bound";
	const FName FSelectionFacade::WeightedBoundGroup = "WeightedBound";

	// Attributes
	const FName FSelectionFacade::IndexAttribute = "Index";
	const FName FSelectionFacade::WeightAttribute = "Weights";
	const FName FSelectionFacade::BoneIndexAttribute = "BoneIndex";

	FSelectionFacade::FSelectionFacade(FManagedArrayCollection* InCollection)
		: Self(InCollection)
	{}

	//
	//  Initialization
	//

	void FSelectionFacade::InitUnboundedGroup(FManagedArrayCollection* Collection, FName GroupName, FName DependencyGroup)
	{
		if (!Collection->HasGroup(GroupName))
		{
			Collection->AddAttribute<TArray<int32>>(FSelectionFacade::IndexAttribute, GroupName, { DependencyGroup });
		}
		ensure(Collection->FindAttributeTyped<TArray<int32>>(FSelectionFacade::IndexAttribute, GroupName) != nullptr);
	}

	void FSelectionFacade::InitWeightedUnboundedGroup(FManagedArrayCollection* Collection, FName GroupName, FName DependencyGroup)
	{
		if (!Collection->HasGroup(GroupName))
		{
			Collection->AddAttribute<TArray<int32>>(FSelectionFacade::IndexAttribute, GroupName, { DependencyGroup });
			Collection->AddAttribute<TArray<float>>(FSelectionFacade::WeightAttribute, GroupName);
		}
		ensure(Collection->FindAttributeTyped<TArray<int32>>(FSelectionFacade::IndexAttribute, GroupName) != nullptr);
		ensure(Collection->FindAttributeTyped<TArray<float>>(FSelectionFacade::WeightAttribute, GroupName) != nullptr);
	}

	void FSelectionFacade::InitBoundedGroup(FManagedArrayCollection* Collection, FName GroupName, FName DependencyGroup, FName BoneDependencyGroup)
	{
		if (!Collection->HasGroup(GroupName))
		{
			Collection->AddAttribute<TArray<int32>>(FSelectionFacade::IndexAttribute, GroupName, { DependencyGroup });
			Collection->AddAttribute<int32>(FSelectionFacade::BoneIndexAttribute, GroupName, { BoneDependencyGroup });
		}
		ensure(Collection->FindAttributeTyped<TArray<int32>>(FSelectionFacade::IndexAttribute, GroupName) != nullptr);
		ensure(Collection->FindAttributeTyped<int32>(FSelectionFacade::BoneIndexAttribute, GroupName) != nullptr);
	}

	void FSelectionFacade::InitWeightedBoundedGroup(FManagedArrayCollection* Collection, FName GroupName, FName DependencyGroup, FName BoneDependencyGroup)
	{
		if (!Collection->HasGroup(GroupName))
		{
			Collection->AddAttribute<TArray<int32>>(FSelectionFacade::IndexAttribute, GroupName, { DependencyGroup });
			Collection->AddAttribute<TArray<float>>(FSelectionFacade::WeightAttribute, GroupName);
			Collection->AddAttribute<int32>(FSelectionFacade::BoneIndexAttribute, GroupName, { BoneDependencyGroup });
		}
		ensure(Collection->FindAttributeTyped<TArray<int32>>(FSelectionFacade::IndexAttribute, GroupName) != nullptr);
		ensure(Collection->FindAttributeTyped<TArray<float>>(FSelectionFacade::WeightAttribute, GroupName) != nullptr);
		ensure(Collection->FindAttributeTyped<int32>(FSelectionFacade::BoneIndexAttribute, GroupName) != nullptr);
	}


	//
	//  AddSelection
	//

	FSelectionFacade::FSelectionKey FSelectionFacade::AddSelection(FManagedArrayCollection* Collection, const TArray<int32>& InIndices, FName DependencyGroup)
	{
		FName GroupName(FSelectionFacade::UnboundGroup.ToString() + "_" + DependencyGroup.ToString());
		InitUnboundedGroup(Collection, GroupName, DependencyGroup);

		int Idx = Collection->AddElements(1, GroupName);
		Collection->ModifyAttribute<TArray<int32>>(FSelectionFacade::IndexAttribute, GroupName)[Idx] = InIndices;
		return FSelectionKey(Idx, FSelectionFacade::UnboundGroup);
	}

	FSelectionFacade::FSelectionKey FSelectionFacade::AddSelection(FManagedArrayCollection* Collection, const TArray<int32>& InIndices, const TArray<float>& InWeights, FName DependencyGroup)
	{
		FName GroupName(FSelectionFacade::WeightedUnboundGroup.ToString() + "_" + DependencyGroup.ToString());
		InitWeightedUnboundedGroup(Collection, GroupName, DependencyGroup);

		int Idx = Collection->AddElements(1, GroupName);
		Collection->ModifyAttribute<TArray<int32>>(FSelectionFacade::IndexAttribute, GroupName)[Idx] = InIndices;
		Collection->ModifyAttribute<TArray<float>>(FSelectionFacade::WeightAttribute, GroupName)[Idx] = InWeights;
		return FSelectionKey(Idx, GroupName);
	}

	FSelectionFacade::FSelectionKey FSelectionFacade::AddSelection(FManagedArrayCollection* Collection, const int32 InBoneIndex, const TArray<int32>& InIndices, FName DependencyGroup, FName BoneDependencyGroup)
	{
		FName GroupName(FSelectionFacade::BoundGroup.ToString() + "_" + DependencyGroup.ToString());
		InitBoundedGroup(Collection, GroupName, DependencyGroup, BoneDependencyGroup);

		int Idx = Collection->AddElements(1, GroupName);
		Collection->ModifyAttribute<TArray<int32>>(FSelectionFacade::IndexAttribute, GroupName)[Idx] = InIndices;
		Collection->ModifyAttribute<int32>(FSelectionFacade::BoneIndexAttribute, GroupName)[Idx] = InBoneIndex;
		return FSelectionKey(Idx, GroupName);
	}

	FSelectionFacade::FSelectionKey FSelectionFacade::AddSelection(FManagedArrayCollection* Collection, const int32 InBoneIndex, const TArray<int32>& InIndices, const TArray<float>& InWeights, FName DependencyGroup, FName BoneDependencyGroup)
	{
		FName GroupName(FSelectionFacade::WeightedBoundGroup.ToString() + "_" + DependencyGroup.ToString());
		InitWeightedBoundedGroup(Collection, GroupName, DependencyGroup, BoneDependencyGroup);

		int Idx = Collection->AddElements(1, GroupName);
		Collection->ModifyAttribute<TArray<int32>>(FSelectionFacade::IndexAttribute, GroupName)[Idx] = InIndices;
		Collection->ModifyAttribute<TArray<float>>(FSelectionFacade::WeightAttribute, GroupName)[Idx] = InWeights;
		Collection->ModifyAttribute<int32>(FSelectionFacade::BoneIndexAttribute, GroupName)[Idx] = InBoneIndex;
		return FSelectionKey(Idx, GroupName);
	}

	//
	//  GetSelection
	//


	void FSelectionFacade::GetSelection(const FManagedArrayCollection* Collection, const FSelectionFacade::FSelectionKey& Key, TArray<int32>& OutIndices)
	{
		if (Collection->HasGroup(Key.GroupName) && 0 <= Key.Index && Key.Index < Collection->NumElements(Key.GroupName))
		{
			if (Collection->FindAttribute<TArray<int32>>(FSelectionFacade::IndexAttribute, Key.GroupName))
				OutIndices = Collection->GetAttribute<TArray<int32>>(FSelectionFacade::IndexAttribute, Key.GroupName)[Key.Index];
		}
	}

	void FSelectionFacade::GetSelection(const FManagedArrayCollection* Collection, const FSelectionFacade::FSelectionKey& Key, TArray<int32>& OutIndices, TArray<float>& OutWeights)
	{
		if (Collection->HasGroup(Key.GroupName) && 0 <= Key.Index && Key.Index < Collection->NumElements(Key.GroupName))
		{
			if (Collection->FindAttribute<TArray<int32>>(FSelectionFacade::IndexAttribute, Key.GroupName))
				OutIndices = Collection->GetAttribute<TArray<int32>>(FSelectionFacade::IndexAttribute, Key.GroupName)[Key.Index];
			if (Collection->FindAttribute<TArray<float>>(FSelectionFacade::WeightAttribute, Key.GroupName))
				OutWeights = Collection->GetAttribute<TArray<float>>(FSelectionFacade::WeightAttribute, Key.GroupName)[Key.Index];
		}
	}

	void FSelectionFacade::GetSelection(const FManagedArrayCollection* Collection, const FSelectionFacade::FSelectionKey& Key, int32& OutBoneIndex, TArray<int32>& OutIndices)
	{
		if (Collection->HasGroup(Key.GroupName) && 0 <= Key.Index && Key.Index < Collection->NumElements(Key.GroupName))
		{
			if (Collection->FindAttribute<TArray<int32>>(FSelectionFacade::IndexAttribute, Key.GroupName))
				OutIndices = Collection->GetAttribute<TArray<int32>>(FSelectionFacade::IndexAttribute, Key.GroupName)[Key.Index];
			if (Collection->FindAttribute<int32>(FSelectionFacade::BoneIndexAttribute, Key.GroupName))
				OutBoneIndex = Collection->GetAttribute<int32>(FSelectionFacade::BoneIndexAttribute, Key.GroupName)[Key.Index];
		}
	}

	void FSelectionFacade::GetSelection(const FManagedArrayCollection* Collection, const FSelectionFacade::FSelectionKey& Key, int32& OutBoneIndex, TArray<int32>& OutIndices, TArray<float>& OutWeights)
	{
		if (Collection->HasGroup(Key.GroupName) && 0 <= Key.Index && Key.Index < Collection->NumElements(Key.GroupName))
		{
			if (Collection->FindAttribute<TArray<int32>>(FSelectionFacade::IndexAttribute, Key.GroupName))
				OutIndices = Collection->GetAttribute<TArray<int32>>(FSelectionFacade::IndexAttribute, Key.GroupName)[Key.Index];
			if (Collection->FindAttribute<TArray<float>>(FSelectionFacade::WeightAttribute, Key.GroupName))
				OutWeights = Collection->GetAttribute<TArray<float>>(FSelectionFacade::WeightAttribute, Key.GroupName)[Key.Index];
			if (Collection->FindAttribute<int32>(FSelectionFacade::BoneIndexAttribute, Key.GroupName))
				OutBoneIndex = Collection->GetAttribute<int32>(FSelectionFacade::BoneIndexAttribute, Key.GroupName)[Key.Index];
		}
	}

};


