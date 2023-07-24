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
	class CHAOS_API FCollectionTransformFacade
	{
	public:
		FCollectionTransformFacade(FManagedArrayCollection& InCollection);
		FCollectionTransformFacade(const FManagedArrayCollection& InCollection);

		/** Creates the facade attributes. */
		void DefineSchema() {}

		/** Valid if parent and children arrays are available */
		bool IsValid() const;

		/** Is the facade defined constant. */
		bool IsConst() const { return ParentAttribute.IsConst(); }

		/** Gets the root index */
		TArray<int32> GetRootIndices() const;

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
		const TManagedArray< FTransform >* FindTransforms() const { return TransformAttribute.Find(); }

		/**
		* Returns array of transforms for transforming from bone space to collection space
		* Vertex(inCollectionSpace) = TransformComputed.TransformPosition(Vertex(inBoneSpace))
		*/
		TArray<FTransform> ComputeCollectionSpaceTransforms() const;

		/**
		* Returns the transform for transforming from bone space to collection space for specified bone
		* Vertex(inCollectionSpace) = TransformComputed.TransformPosition(Vertex(inBoneSpace))
		*/
		FTransform ComputeCollectionSpaceTransform(int32 BoneIdx) const;

		/** Transforms the pivot of a collection */
		void SetPivot(const FTransform& InTransform);

		/** Transforms collection */
		void Transform(const FTransform& InTransform);

		/** Transforms selected bones in the collection */
		void Transform(const FTransform& InTransform, const TArray<int32>& InSelection);

		/** Builds a FMatrix from all the components */
		static FMatrix BuildMatrix(const FVector& Translate,
			const uint8 RotationOrder,
			const FVector& Rotate,
			const FVector& Scale,
			const FVector& Shear,
			const float UniformScale,
			const FVector& RotatePivot,
			const FVector& ScalePivot,
			const bool InvertTransformation);

		/** Builds a FTransform from all the components */
		static FTransform BuildTransform(const FVector& Translate,
			const uint8 RotationOrder,
			const FVector& Rotate,
			const FVector& Scale,
			const float UniformScale,
			const FVector& RotatePivot,
			const FVector& ScalePivot,
			const bool InvertTransformation);

		/** Sets the selected bone's transform to identity */
		void SetBoneTransformToIdentity(int32 BoneIdx);

	private:
		TManagedArrayAccessor<int32>		ParentAttribute;
		TManagedArrayAccessor<TSet<int32>>	ChildrenAttribute;
		TManagedArrayAccessor<FTransform>	TransformAttribute;
	};
}