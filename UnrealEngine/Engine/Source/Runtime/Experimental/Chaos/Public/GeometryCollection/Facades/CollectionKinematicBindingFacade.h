// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "GeometryCollection/Facades/CollectionSelectionFacade.h"

namespace Chaos::Facades
{
	/** Kinematic Facade */
	class CHAOS_API FKinematicBindingFacade
	{
		FManagedArrayCollection& Collection;

	public:
		typedef GeometryCollection::Facades::FSelectionFacade::FSelectionKey FBindingKey;

		FKinematicBindingFacade(FManagedArrayCollection& InCollection) : Collection(InCollection) {}

		void Init();

		//
		//  Skeletal Mesh Bone Bindings
		//
		static FBindingKey SetBoneBindings(FManagedArrayCollection*, const int32 BoneIndex, const TArray<int32>& Vertices, const TArray<float>& Weights);
		static void GetBoneBindings(const FManagedArrayCollection*, const FBindingKey& Key, int32& OutBoneIndex, TArray<int32>& OutBoneVerts, TArray<float>& OutBoneWeights);

		//
		// Kinematics
		//
		static const FName KinematicGroup;
		static const FName KinematicBoneBindingIndex;
		static const FName KinematicBoneBindingToGroup;

		static int32 AddKinematicBinding(FManagedArrayCollection*, const FBindingKey& Key);
		static int32 NumKinematicBindings(const FManagedArrayCollection* Collection) { return Collection->NumElements(KinematicGroup); }
		static FBindingKey GetKinematicBindingKey(const FManagedArrayCollection* InCollection, int Index);

	};
}
