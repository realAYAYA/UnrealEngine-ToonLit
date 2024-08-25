// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryCollection/ManagedArrayAccessor.h"
#include "GeometryCollection/ManagedArrayCollection.h"

namespace GeometryCollection::Facades
{
	/**
	 * Provides an API to read and manipulate hierarchy in a managed array collection
	 */
	class FCollectionTransformFacade
	{
	public:
		CHAOS_API FCollectionTransformFacade(FManagedArrayCollection& InCollection);
		CHAOS_API FCollectionTransformFacade(const FManagedArrayCollection& InCollection);

		/** Creates the facade attributes. */
		void DefineSchema() {}

		/** Valid if parent and children arrays are available */
		CHAOS_API bool IsValid() const;

		/** Is the facade defined constant. */
		bool IsConst() const { return ParentAttribute.IsConst(); }

		/** Gets the root index */
		CHAOS_API TArray<int32> GetRootIndices() const;

		/**
		* Returns the parent indices from the collection. Null if not initialized.
		*/
		const TManagedArray< int32 >* GetParents() const { return ParentAttribute.Find(); }

		/**
		* Returns the child indicesfrom the collection. Null if not initialized.
		*/
		const TManagedArray< TSet<int32> >* FindChildren() const { return ChildrenAttribute.Find(); }

		/**
		* Returns the child indicesfrom the collection. Null if not initialized.
		*/
		const TManagedArray<FTransform3f>* FindTransforms() const { return TransformAttribute.Find(); }

		/**
		* Returns array of transforms for transforming from bone space to collection space
		* Vertex(inCollectionSpace) = TransformComputed.TransformPosition(Vertex(inBoneSpace))
		*/
		CHAOS_API TArray<FTransform> ComputeCollectionSpaceTransforms() const;

		/**
		* Returns the transform for transforming from bone space to collection space for specified bone
		* Vertex(inCollectionSpace) = TransformComputed.TransformPosition(Vertex(inBoneSpace))
		*/
		CHAOS_API FTransform ComputeCollectionSpaceTransform(int32 BoneIdx) const;

		/** Transforms the pivot of a collection */
		CHAOS_API void SetPivot(const FTransform& InTransform);

		/** Transforms collection */
		CHAOS_API void Transform(const FTransform& InTransform);

		/** Transforms selected bones in the collection */
		CHAOS_API void Transform(const FTransform& InTransform, const TArray<int32>& InSelection);

		/** Builds a FMatrix from all the components */
		static CHAOS_API FMatrix BuildMatrix(const FVector& Translate,
			const uint8 RotationOrder,
			const FVector& Rotate,
			const FVector& Scale,
			const FVector& Shear,
			const float UniformScale,
			const FVector& RotatePivot,
			const FVector& ScalePivot,
			const bool InvertTransformation);

		/** Builds a FTransform from all the components */
		static CHAOS_API FTransform BuildTransform(const FVector& Translate,
			const uint8 RotationOrder,
			const FVector& Rotate,
			const FVector& Scale,
			const float UniformScale,
			const FVector& RotatePivot,
			const FVector& ScalePivot,
			const bool InvertTransformation);

		/** Sets the selected bone's transform to identity */
		CHAOS_API void SetBoneTransformToIdentity(int32 BoneIdx);

	private:
		TManagedArrayAccessor<int32>		ParentAttribute;
		TManagedArrayAccessor<TSet<int32>>	ChildrenAttribute;
		TManagedArrayAccessor<FTransform3f>	TransformAttribute;
	};
}
