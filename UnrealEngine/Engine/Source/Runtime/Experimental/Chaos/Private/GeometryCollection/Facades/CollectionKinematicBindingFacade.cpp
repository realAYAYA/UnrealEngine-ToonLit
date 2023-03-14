// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	FleshCollection.cpp: FFleshCollection methods.
=============================================================================*/

#include "GeometryCollection/Facades/CollectionKinematicBindingFacade.h"
#include "GeometryCollection/GeometryCollection.h"

namespace Chaos::Facades
{
	// Attributes
	const FName FKinematicBindingFacade::KinematicGroup("Kinematics");
	const FName FKinematicBindingFacade::KinematicBoneBindingIndex("Binding");
	const FName FKinematicBindingFacade::KinematicBoneBindingToGroup("BindingToGroup");


	void FKinematicBindingFacade::Init()
	{
		FName SelectionGroup1 = FName(GeometryCollection::Facades::FSelectionFacade::BoundGroup.ToString() + "_" + FGeometryCollection::VerticesGroup.ToString());
		FManagedArrayCollection::FConstructionParameters BindingGroupDependency(SelectionGroup1);

		// Kinematic Bindings
		Collection.AddAttribute<int32>(FKinematicBindingFacade::KinematicBoneBindingIndex, FKinematicBindingFacade::KinematicGroup, BindingGroupDependency);
		Collection.AddAttribute<FString>(FKinematicBindingFacade::KinematicBoneBindingToGroup, FKinematicBindingFacade::KinematicGroup);
	}

	FKinematicBindingFacade::FBindingKey FKinematicBindingFacade::SetBoneBindings(FManagedArrayCollection* InCollection, const int32 InBoneIndex, const TArray<int32>& InBoneVerts, const TArray<float>& InBoneWeights)
	{
		return GeometryCollection::Facades::FSelectionFacade::AddSelection(InCollection, InBoneIndex, InBoneVerts, InBoneWeights, FGeometryCollection::VerticesGroup);
	}

	void FKinematicBindingFacade::GetBoneBindings(const FManagedArrayCollection* InCollection, const FKinematicBindingFacade::FBindingKey& Key, int32& OutBoneIndex, TArray<int32>& OutBoneVerts, TArray<float>& OutBoneWeights)
	{
		GeometryCollection::Facades::FSelectionFacade::GetSelection(InCollection, Key, OutBoneIndex, OutBoneVerts, OutBoneWeights);
	}


	int32 FKinematicBindingFacade::AddKinematicBinding(FManagedArrayCollection* InCollection, const FKinematicBindingFacade::FBindingKey& Key)
	{
		if (InCollection && InCollection->HasGroup(KinematicGroup))
		{
			int32 NewIndex = InCollection->AddElements(1, KinematicGroup);
			TManagedArray<int32>* KinemaitcBoneBinding = InCollection->FindAttribute<int32>(KinematicBoneBindingIndex, KinematicGroup);
			TManagedArray<FString>* KinemaitcBoneBindingToGroup = InCollection->FindAttribute<FString>(KinematicBoneBindingToGroup, KinematicGroup);

			(*KinemaitcBoneBinding)[NewIndex] = Key.Index;
			(*KinemaitcBoneBindingToGroup)[NewIndex] = Key.GroupName.ToString();

			return NewIndex;
		}
		return INDEX_NONE;
	}

	FKinematicBindingFacade::FBindingKey FKinematicBindingFacade::GetKinematicBindingKey(const FManagedArrayCollection* InCollection, int Index)
	{
		FKinematicBindingFacade::FBindingKey Key;
		if (InCollection && InCollection->HasGroup(KinematicGroup))
		{
			const TManagedArray<int32>* KinemaitcBoneBinding = InCollection->FindAttribute<int32>(KinematicBoneBindingIndex, KinematicGroup);
			const TManagedArray<FString>* KinemaitcBoneBindingToGroup = InCollection->FindAttribute<FString>(KinematicBoneBindingToGroup, KinematicGroup);
			if (KinemaitcBoneBinding && KinemaitcBoneBindingToGroup)
			{
				if (0 <= Index && Index < InCollection->NumElements(KinematicGroup))
				{
					Key.GroupName = FName((*KinemaitcBoneBindingToGroup)[Index]);
					Key.Index = (*KinemaitcBoneBinding)[Index];
				}
			}
		}
		return Key;
	}

}
