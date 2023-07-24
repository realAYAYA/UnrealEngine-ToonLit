// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryCollection/ManagedArrayAccessor.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "GeometryCollection/Facades/CollectionSelectionFacade.h"

namespace GeometryCollection::Facades
{
	/**
	* Transient constraint candidate data.
	*/
	struct CHAOS_API FConstraintOverridesCandidateData
	{
		FConstraintOverridesCandidateData()
			: VertexIndex(INDEX_NONE)
			, BoneIndex(INDEX_NONE)
		{}

		int32 VertexIndex;
		int32 BoneIndex;
	};

	/**
	* Transient constraint target data.
	*/
	struct CHAOS_API FConstraintOverridesTargetData
	{
		FConstraintOverridesTargetData()
			: VertexIndex(INDEX_NONE)
		{}

		int32 VertexIndex;
		FVector3f PositionTarget;
	};

	/**
	* Transient constraint candidates.  Typically stored in the rest collection.
	*/
	class CHAOS_API FConstraintOverrideCandidateFacade
	{
	public:
		static const FName GroupName;
		static const FName TargetIndex;
		static const FName BoneIndex;

		FConstraintOverrideCandidateFacade(FManagedArrayCollection& InCollection);
		FConstraintOverrideCandidateFacade(const FManagedArrayCollection& InCollection);

		void DefineSchema();
		bool IsConst() const { return Collection == nullptr; }
		bool IsValid() const;

		int32 Add(FConstraintOverridesCandidateData& InputData);
		void Clear();
		FConstraintOverridesCandidateData Get(const int32 Index) const;
		int32 Num() const { return TargetIndexAttribute.Num(); }

	private:
		const FManagedArrayCollection& ConstCollection;
		FManagedArrayCollection* Collection = nullptr;

		TManagedArrayAccessor<int32> TargetIndexAttribute;
		TManagedArrayAccessor<int32> BoneIndexAttribute;
	};

	/**
	* Transient constraint targets.  Typically stored in the simulation collection.
	*/
	class CHAOS_API FConstraintOverrideTargetFacade
	{
	public:
		static const FName GroupName;
		static const FName TargetIndex;
		static const FName TargetPosition;

		FConstraintOverrideTargetFacade(FManagedArrayCollection& InCollection);
		FConstraintOverrideTargetFacade(const FManagedArrayCollection& InCollection);

		void DefineSchema();
		bool IsConst() const { return Collection == nullptr; }
		bool IsValid() const;

		int32 Add(FConstraintOverridesTargetData& InputData);
		void Clear();
		FConstraintOverridesTargetData Get(const int32 Index) const;
		int32 GetIndex(const int32 Index) const;
		const FVector3f& GetPosition(const int32 Index) const;
		int32 Num() const { return TargetIndexAttribute.Num(); }

	private:
		const FManagedArrayCollection& ConstCollection;
		FManagedArrayCollection* Collection = nullptr;

		TManagedArrayAccessor<int32> TargetIndexAttribute;
		TManagedArrayAccessor<FVector3f> TargetPositionAttribute;
	};



} // namespace GeometryCollection::Facades