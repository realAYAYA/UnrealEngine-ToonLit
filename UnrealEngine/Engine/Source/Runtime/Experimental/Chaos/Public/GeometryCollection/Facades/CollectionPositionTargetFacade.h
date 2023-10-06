// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryCollection/ManagedArrayAccessor.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "GeometryCollection/Facades/CollectionSelectionFacade.h"

namespace GeometryCollection::Facades
{
	struct FPositionTargetsData
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
	class FPositionTargetFacade
	{
	public:

		typedef GeometryCollection::Facades::FSelectionFacade::FSelectionKey FBindingKey;

		//
		// Kinematics
		//
		static CHAOS_API const FName GroupName;
		static CHAOS_API const FName TargetIndex;
		static CHAOS_API const FName SourceIndex;
		static CHAOS_API const FName Stiffness;
		static CHAOS_API const FName Damping;
		static CHAOS_API const FName SourceName;
		static CHAOS_API const FName TargetName;
		static CHAOS_API const FName TargetWeights;
		static CHAOS_API const FName SourceWeights;

		CHAOS_API FPositionTargetFacade(FManagedArrayCollection& InCollection);
		CHAOS_API FPositionTargetFacade(const FManagedArrayCollection& InCollection);

		/** Create the facade attributes. */
		CHAOS_API void DefineSchema();

		/** Is the facade defined constant. */
		bool IsConst() const { return Collection == nullptr; }

		/** Is the Facade defined on the collection? */
		CHAOS_API bool IsValid() const;

		//
		//  Skeletal Mesh Bone Bindings
		//
		CHAOS_API int32 AddPositionTarget(const FPositionTargetsData& InputData);
		CHAOS_API FPositionTargetsData GetPositionTarget(const int32 DataIndex) const;
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
