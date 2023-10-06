// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/UnrealString.h"
#include "Containers/Array.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "GeometryCollection/ManagedArrayAccessor.h"

namespace GeometryCollection::Facades
{

	/**
	* FTetrahedralBindings
	* 
	* Interface for storing and retrieving bindings of surfaces (typically SkeletalMesh or StaticMesh) to
	* tetrahedral meshes.  Bindings data for each surface is grouped by a mesh id and a level of detail.
	*/
	class FTetrahedralBindings
 	{
	public:

		// groups
		static CHAOS_API const FName MeshBindingsGroupName;

		//
		// Attributes
		//

		static CHAOS_API const FName MeshIdAttributeName;

		//! Tet or Tri vertex indices.
		static CHAOS_API const FName ParentsAttributeName;
		//! Barycentric weight of each tet/tri vertex.
		static CHAOS_API const FName WeightsAttributeName;
		//! Offset vector from barycentric tri position.
		static CHAOS_API const FName OffsetsAttributeName;
		//! Per vertex amount for deformer masking.
		static CHAOS_API const FName MaskAttributeName;

		/**
		* FSelectionFacade Constuctor
		* @param VertixDependencyGroup : GroupName the index attribute is dependent on. 
		*/
		CHAOS_API FTetrahedralBindings(FManagedArrayCollection& InSelf);
		CHAOS_API FTetrahedralBindings(const FManagedArrayCollection& InSelf);
		CHAOS_API virtual ~FTetrahedralBindings();

		/** 
		* Create the facade schema. 
		*/
		CHAOS_API void DefineSchema();

		/** Returns \c true if the facade is operating on a read-only geometry collection. */
		bool IsConst() const { return MeshIdAttribute.IsConst(); }

		/** 
		* Returns \c true if the Facade defined on the collection, and is initialized to
		* a valid bindings group.
		*/
		CHAOS_API bool IsValid() const;

		/**
		* Given a \p MeshId (by convention \code Mesh->GetPrimaryAssetId() \endcode) and
		* a Level Of Detail rank, generate the associated bindings group name.
		*/
		static CHAOS_API FName GenerateMeshGroupName(const int32 TetMeshIdx, const FName& MeshId, const int32 LOD);

		/**
		* For a given \p MeshId and \p LOD, return the associated tetrahedral mesh index.
		*/
		CHAOS_API int32 GetTetMeshIndex(const FName& MeshId, const int32 LOD) const;

		/**
		* Returns \c true if the specified bindings group exists.
		*/
		CHAOS_API bool ContainsBindingsGroup(const int32 TetMeshIdx, const FName& MeshId, const int32 LOD) const;
		CHAOS_API bool ContainsBindingsGroup(const FName& GroupName) const;

		/**
		* Create a new bindings group, allocating new arrays.
		*/
		CHAOS_API void AddBindingsGroup(const int32 TetMeshIdx, const FName& MeshId, const int32 LOD);
		CHAOS_API void AddBindingsGroup(const FName& GroupName);
		
		/**
		* Initialize local arrays to point at a bindings group associated with \p MeshId 
		* and \p LOD.  Returns \c false if it doesn't exist.
		*/
		CHAOS_API bool ReadBindingsGroup(const int32 TetMeshIdx, const FName& MeshId, const int32 LOD);
		CHAOS_API bool ReadBindingsGroup(const FName& GroupName);

		/**
		* Removes a group from the list of bindings groups, removes the bindings arrays 
		* from the geometry collection, and removes the group if it's empty.
		*/
		CHAOS_API void RemoveBindingsGroup(const int32 TetMeshIdx, const FName& MeshId, const int32 LOD);
		CHAOS_API void RemoveBindingsGroup(const FName& GroupName);

		/**
		* Authors bindings data.
		* 
		* \p Parents are indicies of vertices; tetrahedron or surface triangle where final 
		*    elem is \c INDEX_NONE.
		* \p Weights are barycentric coordinates.
		* \p Offsets are vectors from the barycentric point to the location, in the case 
		*    of a surface binding.
		* \p Mask are per-vertex multipliers on the deformer, 0 for no deformation, 1.0 for 
		*    full deformation.
		*/
		CHAOS_API void SetBindingsData(const TArray<FIntVector4>& ParentsIn, const TArray<FVector4f>& WeightsIn, const TArray<FVector3f>& OffsetsIn, const TArray<float>& MaskIn);
		void SetBindingsData(const TArray<FIntVector4>& ParentsIn, const TArray<FVector4f>& WeightsIn, const TArray<FVector3f>& OffsetsIn)
		{
			TArray<float> MaskTmp; MaskTmp.SetNum(ParentsIn.Num());
			for (int32 i = 0; i < ParentsIn.Num(); i++)
			{
				MaskTmp[i] = 1.0;
			}
			SetBindingsData(ParentsIn, WeightsIn, OffsetsIn, MaskTmp);
		}

		/**
		* Get Parents array.
		*/
		const TManagedArrayAccessor<FIntVector4>* GetParentsRO() const { return Parents.Get(); }
		      TManagedArrayAccessor<FIntVector4>* GetParents() { check(!IsConst()); return Parents.Get(); }
		/**
		* Get Weights array.
		*/
		const TManagedArrayAccessor<FVector4f>* GetWeightsRO() const { return Weights.Get(); }
		      TManagedArrayAccessor<FVector4f>* GetWeights() { check(!IsConst()); return Weights.Get(); }
		/**
		* Get Offsets array.
		*/
		const TManagedArrayAccessor<FVector3f>* GetOffsetsRO() const { return Offsets.Get(); }
		      TManagedArrayAccessor<FVector3f>* GetOffsets() { check(!IsConst());  return Offsets.Get(); }
		/**
		* Get Mask array.
		*/
		const TManagedArrayAccessor<float>* GetMaskRO() const { return Mask.Get(); }
		      TManagedArrayAccessor<float>* GetMask() { check(!IsConst()); return Mask.Get(); }

	private:
		TManagedArrayAccessor<FString> MeshIdAttribute;

		TUniquePtr<TManagedArrayAccessor<FIntVector4>> Parents;
		TUniquePtr<TManagedArrayAccessor<FVector4f>> Weights;
		TUniquePtr<TManagedArrayAccessor<FVector3f>> Offsets;
		TUniquePtr<TManagedArrayAccessor<float>> Mask;
	};

}
