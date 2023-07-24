// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryCollection/ManagedArrayAccessor.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "GeometryCollection/Facades/CollectionSelectionFacade.h"

namespace GeometryCollection::Facades
{
	/** Kinematic Facade */
	class CHAOS_API FKinematicBindingFacade
	{
	public:

		typedef GeometryCollection::Facades::FSelectionFacade::FSelectionKey FBindingKey;

		//
		// Kinematics
		//
		static const FName KinematicGroup;
		static const FName KinematicBoneBindingIndex;
		static const FName KinematicBoneBindingToGroup;

		FKinematicBindingFacade(FManagedArrayCollection& InCollection);
		FKinematicBindingFacade(const FManagedArrayCollection& InCollection);

		/** Create the facade attributes. */
		void DefineSchema();

		/** Is the facade defined constant. */
		bool IsConst() const { return Collection==nullptr; }

		/** Is the Facade defined on the collection? */
		bool IsValid() const;

		//
		//  Skeletal Mesh Bone Bindings
		//
		FBindingKey SetBoneBindings(const int32 BoneIndex, const TArray<int32>& Vertices, const TArray<float>& Weights);
		void GetBoneBindings(const FBindingKey& Key, int32& OutBoneIndex, TArray<int32>& OutBoneVerts, TArray<float>& OutBoneWeights) const;

		int32 AddKinematicBinding(const FBindingKey& Key);
		int32 NumKinematicBindings() const { return KinemaitcBoneBindingAttribute.Num(); }
		FBindingKey GetKinematicBindingKey(int Index) const;

	private:
		const FManagedArrayCollection& ConstCollection;
		FManagedArrayCollection* Collection = nullptr;

		TManagedArrayAccessor<int32> KinemaitcBoneBindingAttribute;
		TManagedArrayAccessor<FString> KinemaitcBoneBindingToGroupAttribute;
	};
}
