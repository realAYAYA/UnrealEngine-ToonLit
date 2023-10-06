// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/UnrealString.h"
#include "Containers/Array.h"
#include "GeometryCollection/Facades/CollectionSelectionFacade.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "GeometryCollection/ManagedArrayAccessor.h"

namespace GeometryCollection::Facades
{

	/**
	* FVertexBoneWeightsFacade
	* 
	* Defines common API for storing a vertex weights bound to a bone. This mapping is from the 
	* the vertex to the bone index. The FSelectionFacad will store the mapping from the BoneIndex
	* to the vertex. 
	* Usage:
	*    FVertexBoneWeightsFacade::AddBoneWeights(this, FSelectionFacade(this) );
	* 
	* Then arrays can be accessed later by:
	*	const TManagedArray< TArray<int32> >* BoneIndices = FVertexBoneWeightsFacade::GetBoneIndices(this);
	*	const TManagedArray< TArray<float> >* BoneWeights = FVertexBoneWeightsFacade::GetBoneWeights(this);
	*
	* The following attributes are created on the collection:
	* 
	*	- FindAttribute<TArray<int32>>(FVertexSetInterface::IndexAttribute, FGeometryCollection::VerticesGroup);
	*	- FindAttribute<TArray<float>>(FVertexSetInterface::WeightAttribute, FGeometryCollection::VerticesGroup);
	* 
	*/
	class FVertexBoneWeightsFacade
	{
	public:

		// Attributes
		static CHAOS_API const FName BoneIndexAttributeName;
		static CHAOS_API const FName BoneWeightAttributeName;

		/**
		* FVertexBoneWeightsFacade Constuctor
		*/
		CHAOS_API FVertexBoneWeightsFacade(FManagedArrayCollection& InSelf);
		CHAOS_API FVertexBoneWeightsFacade(const FManagedArrayCollection& InSelf);

		/** Define the facade */
		CHAOS_API void DefineSchema();

		/** Is the Facade const */
		bool IsConst() const { return Collection == nullptr; }

		/** Is the Facade defined on the collection? */
		CHAOS_API bool IsValid() const;

		/** Add bone weight based on the kinematic bindings. */
		CHAOS_API void AddBoneWeightsFromKinematicBindings();

		/** Add bone weight based on the kinematic bindings. */
		CHAOS_API void AddBoneWeight(int32 VertexIndex, int32 BoneIndex, float BoneWeight);

		/** Return the vertex bone indices from the collection. Null if not initialized.  */
		const TManagedArray< TArray<int32> >* FindBoneIndices()  const { return BoneIndexAttribute.Find(); }
		const TManagedArray< TArray<int32> >& GetBoneIndices() const { return BoneIndexAttribute.Get(); }


		/** Return the vertex bone weights from the collection. Null if not initialized. */
		const TManagedArray< TArray<float> >* FindBoneWeights()  const { return BoneWeightAttribute.Find(); }
		const TManagedArray< TArray<float> >& GetBoneWeights() const { return BoneWeightAttribute.Get(); }

	private:
		const FManagedArrayCollection& ConstCollection;
		FManagedArrayCollection* Collection = nullptr;

		TManagedArrayAccessor<TArray<int32>> BoneIndexAttribute;
		TManagedArrayAccessor<TArray<float>> BoneWeightAttribute;
		TManagedArrayAccessor<int32> ParentAttribute;
		TManagedArrayAccessor<FVector3f> VerticesAttribute;

	};

}
