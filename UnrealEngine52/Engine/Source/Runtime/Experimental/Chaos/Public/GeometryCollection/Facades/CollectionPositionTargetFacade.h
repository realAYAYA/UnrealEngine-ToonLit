// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryCollection/ManagedArrayAccessor.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "GeometryCollection/Facades/CollectionSelectionFacade.h"

namespace GeometryCollection::Facades
{
	struct CHAOS_API FPositionTargetsData
	{
		TArray<int32> TargetIndex;
		TArray<int32> SourceIndex;
		TArray<float> TargetWeights;
		TArray<float> SourceWeights;
		FString TargetName;
		FString SourceName;
		float Stiffness;
		float Damping;

	};

	/** Kinematic Facade */
	class CHAOS_API FPositionTargetFacade
	{
	public:

		typedef GeometryCollection::Facades::FSelectionFacade::FSelectionKey FBindingKey;

		//
		// Kinematics
		//
		static const FName GroupName;
		static const FName TargetIndex;
		static const FName SourceIndex;
		static const FName Stiffness;
		static const FName Damping;
		static const FName SourceName;
		static const FName TargetName;
		static const FName TargetWeights;
		static const FName SourceWeights;

		FPositionTargetFacade(FManagedArrayCollection& InCollection);
		FPositionTargetFacade(const FManagedArrayCollection& InCollection);

		/** Create the facade attributes. */
		void DefineSchema();

		/** Is the facade defined constant. */
		bool IsConst() const { return Collection == nullptr; }

		/** Is the Facade defined on the collection? */
		bool IsValid() const;

		//
		//  Skeletal Mesh Bone Bindings
		//
		int32 AddPositionTarget(const FPositionTargetsData& InputData);
		FPositionTargetsData GetPositionTarget(const int32 DataIndex) const;
		int32 NumPositionTargets() const { return TargetIndexAttribute.Num(); }

	private:
		const FManagedArrayCollection& ConstCollection;
		FManagedArrayCollection* Collection = nullptr;

		TManagedArrayAccessor<TArray<int32>> TargetIndexAttribute;
		TManagedArrayAccessor<TArray<int32>> SourceIndexAttribute;
		TManagedArrayAccessor<FString> TargetNameAttribute;
		TManagedArrayAccessor<FString> SourceNameAttribute;
		TManagedArrayAccessor<float> StiffnessAttribute;
		TManagedArrayAccessor<float> DampingAttribute;
		TManagedArrayAccessor<TArray<float>> TargetWeightsAttribute;
		TManagedArrayAccessor<TArray<float>> SourceWeightsAttribute;
	};
}
