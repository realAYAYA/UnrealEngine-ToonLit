// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryCollection/ManagedArray.h"
#include "ChaosClothAsset/ClothCollectionOptionalSchemas.h"
#include "ChaosClothAsset/IsUserAttributeType.h"

struct FManagedArrayCollection;

namespace UE::Chaos::ClothAsset
{
	/**
	 * Cloth collection facade data.
	 */
	class FClothCollection final
	{
	public:
		static constexpr int8 MaxNumBoneInfluences = 12; // This should be <= MAX_TOTAL_INFLUENCES defined in GPUSkinPublicDefs.h 
		static constexpr int8 MaxNumTetherAttachments = 4; // This should be <= FClothTetherDataPrivate::MaxNumAttachments

		explicit FClothCollection(const TSharedRef<FManagedArrayCollection>& InManagedArrayCollection);
		~FClothCollection() = default;

		FClothCollection(const FClothCollection&) = delete;
		FClothCollection& operator=(const FClothCollection&) = delete;
		FClothCollection(FClothCollection&&) = delete;
		FClothCollection& operator=(FClothCollection&&) = delete;

		/** Return whether the underlying collection is a valid cloth collection. */
		bool IsValid(EClothCollectionOptionalSchemas OptionalSchemas = EClothCollectionOptionalSchemas::None) const;

		/** Make the underlying collection a cloth collection. */
		void DefineSchema(EClothCollectionOptionalSchemas OptionalSchemas = EClothCollectionOptionalSchemas::None);

		/** Get the number of elements of a group. */
		int32 GetNumElements(const FName& GroupName) const;

		/** Get the number of elements of one of the sub groups that have start/end indices. */
		int32 GetNumElements(const TManagedArray<int32>* StartArray, const TManagedArray<int32>* EndArray, int32 Index) const;

		/** Set the number of elements of a group. */
		void SetNumElements(int32 InNumElements, const FName& GroupName);

		/** Set the number of elements to one of the sub groups while maintaining the correct order of the data, and return the first index of the range. */
		int32 SetNumElements(int32 InNumElements, const FName& GroupName, TManagedArray<int32>* StartArray, TManagedArray<int32>* EndArray, int32 Index);

		/** Remove Elements */
		void RemoveElements(const FName& Group, const TArray<int32>& SortedDeletionList);

		/** Remove Elements. SortedDeletionList should be global indices. */
		void RemoveElements(const FName& GroupName, const TArray<int32>& SortedDeletionList, TManagedArray<int32>* StartArray, TManagedArray<int32>* EndArray, int32 Index);

		template<typename T>
		static inline TConstArrayView<T> GetElements(const TManagedArray<T>* ElementArray, const TManagedArray<int32>* StartArray, const TManagedArray<int32>* EndArray, int32 ArrayIndex);

		template<typename T>
		static inline TArrayView<T> GetElements(TManagedArray<T>* ElementArray, const TManagedArray<int32>* StartArray, const TManagedArray<int32>* EndArray, int32 ArrayIndex);

		template<typename T>
		static inline TConstArrayView<T> GetElements(const TManagedArray<T>* ElementArray);

		template<typename T>
		static inline TArrayView<T> GetElements(TManagedArray<T>* ElementArray);

		/**
		 * Return the difference between the start index of an element to the start index of the first sub-element in the group (base).
		 * Usefull for getting back and forth between LOD/pattern indexation modes.
		 */
		static int32 GetElementsOffset(const TManagedArray<int32>* StartArray, int32 BaseElementIndex, int32 ElementIndex);

		/**
		* Return the ArrayIndex (into StartArray/EndArray) which corresponds with the subarray that contains ElementIndex.
		* Useful for finding which pattern contains a vertex or face index. Returns INDEX_NONE if not found.
		* NOTE: typically we have a small number of patterns (case where this is used), so just doing a linear search since
		* a bisection search would get confused by the empty patterns.
		*/
		static int32 GetArrayIndexForContainedElement(
			const TManagedArray<int32>* StartArray,
			const TManagedArray<int32>* EndArray,
			int32 ElementIndex);

		static int32 GetNumSubElements(
			const TManagedArray<int32>* StartArray,
			const TManagedArray<int32>* EndArray,
			const TManagedArray<int32>* StartSubArray,
			const TManagedArray<int32>* EndSubArray,
			int32 LodIndex);

		template<typename T>
		static inline TConstArrayView<T> GetSubElements(
			const TManagedArray<T>* SubElementArray,
			const TManagedArray<int32>* StartArray,
			const TManagedArray<int32>* EndArray,
			const TManagedArray<int32>* StartSubArray,
			const TManagedArray<int32>* EndSubArray,
			int32 ArrayIndex);

		template<typename T>
		static inline TArrayView<T> GetSubElements(
			TManagedArray<T>* SubElementArray,
			const TManagedArray<int32>* StartArray,
			const TManagedArray<int32>* EndArray,
			const TManagedArray<int32>* StartSubArray,
			const TManagedArray<int32>* EndSubArray,
			int32 ArrayIndex);

		template<bool bStart = true, bool bEnd = true>
		static TTuple<int32, int32> GetSubElementsStartEnd(
			const TManagedArray<int32>* StartArray,
			const TManagedArray<int32>* EndArray,
			const TManagedArray<int32>* StartSubArray,
			const TManagedArray<int32>* EndSubArray,
			int32 ArrayIndex);


		/** Useful method for copying data between ArrayViews. Putting here for now. */
		template<typename T>
		static inline void CopyArrayViewData(const TArrayView<T>& To, const TConstArrayView<T>& From);

		template<typename T, TEMPLATE_REQUIRES(!TIsArray<T>::Value)>
		static inline void CopyArrayViewDataAndApplyOffset(const TArrayView<T>& To, const TConstArrayView<T>& From, const T Offset);

		template<typename T>
		static inline void CopyArrayViewDataAndApplyOffset(const TArrayView<TArray<T>>& To, const TConstArrayView<TArray<T>>& From, const T Offset);

		template<typename T>
		static inline uint32 GetElementsTypeHash(const TManagedArray<T>* ElementArray);

		//~ Weight maps
		template<typename T, TEMPLATE_REQUIRES(TIsUserAttributeType<T>::Value)>
		TArray<FName> GetUserDefinedAttributeNames(const FName& GroupName) const;

		template<typename T, TEMPLATE_REQUIRES(TIsUserAttributeType<T>::Value)>
		TManagedArray<T>* FindOrAddUserDefinedAttribute(const FName& Name, const FName& GroupName);

		template<typename T, TEMPLATE_REQUIRES(TIsUserAttributeType<T>::Value)>
		void AddUserDefinedAttribute(const FName& Name, const FName& GroupName)
		{
			FindOrAddUserDefinedAttribute<T>(Name, GroupName);
		}

		void RemoveUserDefinedAttribute(const FName& Name, const FName& GroupName);

		template<typename T, TEMPLATE_REQUIRES(TIsUserAttributeType<T>::Value)>
		bool HasUserDefinedAttribute(const FName& Name, const FName& GroupName) const;

		template<typename T, TEMPLATE_REQUIRES(TIsUserAttributeType<T>::Value)>
		const TManagedArray<T>* GetUserDefinedAttribute(const FName& Name, const FName& GroupName) const;

		template<typename T, TEMPLATE_REQUIRES(TIsUserAttributeType<T>::Value)>
		TManagedArray<T>* GetUserDefinedAttribute(const FName& Name, const FName& GroupName);

		static bool IsValidClothCollectionGroupName(const FName& GroupName);
		static bool IsValidUserDefinedAttributeName(const FName& Name, const FName& GroupName);

		//~ LODs Group (There should be only one LOD per ClothCollection)
		const TManagedArray<FString>* GetPhysicsAssetPathName() const { return PhysicsAssetPathName; }
		const TManagedArray<FString>* GetSkeletalMeshPathName() const { return SkeletalMeshPathName; }

		// ~ Solvers Group
		const TManagedArray<FVector3f>* GetSolverGravity() const { return SolverGravity; }
		const TManagedArray<float>* GetSolverAirDamping() const { return SolverAirDamping; }
		const TManagedArray<int32>* GetSolverSubSteps() const { return SolverSubSteps; }
		const TManagedArray<float>* GetSolverTimeStep() const { return SolverTimeStep; }

		//~ Seam Group
		const TManagedArray<int32>* GetSeamStitchStart() const { return SeamStitchStart; }
		const TManagedArray<int32>* GetSeamStitchEnd() const { return SeamStitchEnd; }

		//~ Seam Stitches Group
		const TManagedArray<FIntVector2>* GetSeamStitch2DEndIndices() const { return SeamStitch2DEndIndices; }
		const TManagedArray<int32>* GetSeamStitch3DIndex() const { return SeamStitch3DIndex; }

		//~ Fabric Group
		const TManagedArray<FVector3f>* GetFabricBendingStiffness() const { return FabricBendingStiffness; }
		const TManagedArray<FVector3f>* GetFabricBucklingStiffness() const { return FabricBucklingStiffness; }
		const TManagedArray<FVector3f>* GetFabricStretchStiffness() const { return FabricStretchStiffness; }
		const TManagedArray<float>* GetFabricBucklingRatio() const { return FabricBucklingRatio; }
		const TManagedArray<float>* GetFabricDensity() const { return FabricDensity; }
		const TManagedArray<float>* GetFabricFriction() const { return FabricFriction; }
		const TManagedArray<float>* GetFabricDamping() const { return FabricDamping; }
		const TManagedArray<float>* GetFabricPressure() const { return FabricPressure; }
		const TManagedArray<int32>* GetFabricLayer() const { return FabricLayer; }
		const TManagedArray<float>* GetFabricCollisionThickness() const { return FabricCollisionThickness; }
		
		//~ Sim Patterns Group
		const TManagedArray<int32>* GetSimVertices2DStart() const { return SimVertices2DStart; }
		const TManagedArray<int32>* GetSimVertices2DEnd() const { return SimVertices2DEnd; }
		const TManagedArray<int32>* GetSimFacesStart() const { return SimFacesStart; }
		const TManagedArray<int32>* GetSimFacesEnd() const { return SimFacesEnd; }
		const TManagedArray<int32>* GetSimPatternFabric() const { return SimPatternFabric; }
		
		//~ Render Patterns Group
		const TManagedArray<int32>* GetRenderVerticesStart() const { return RenderVerticesStart; }
		const TManagedArray<int32>* GetRenderVerticesEnd() const { return RenderVerticesEnd; }
		const TManagedArray<int32>* GetRenderFacesStart() const { return RenderFacesStart; }
		const TManagedArray<int32>* GetRenderFacesEnd() const { return RenderFacesEnd; }
		const TManagedArray<int32>* GetRenderDeformerNumInfluences() const { return RenderDeformerNumInfluences; }
		const TManagedArray<FString>* GetRenderMaterialPathName() const { return RenderMaterialPathName; }

		//~ Sim Faces Group
		const TManagedArray<FIntVector3>* GetSimIndices2D() const { return SimIndices2D; }
		const TManagedArray<FIntVector3>* GetSimIndices3D() const { return SimIndices3D; }

		//~ Sim Vertices 2D Group
		const TManagedArray<FVector2f>* GetSimPosition2D() const { return SimPosition2D; }
		const TManagedArray<int32>* GetSimVertex3DLookup() const { return SimVertex3DLookup; }

		//~ Sim Vertices 3D Group
		const TManagedArray<FVector3f>* GetSimPosition3D() const { return SimPosition3D; }
		const TManagedArray<FVector3f>* GetSimNormal() const { return SimNormal; }
		const TManagedArray<TArray<int32>>* GetSimBoneIndices() const { return SimBoneIndices; }
		const TManagedArray<TArray<float>>* GetSimBoneWeights() const { return SimBoneWeights; }
		const TManagedArray<TArray<int32>>* GetTetherKinematicIndex() const { return TetherKinematicIndex; }
		const TManagedArray<TArray<float>>* GetTetherReferenceLength() const { return TetherReferenceLength; }
		const TManagedArray<TArray<int32>>* GetSimVertex2DLookup() const { return SimVertex2DLookup; }
		const TManagedArray<TArray<int32>>* GetSeamStitchLookup() const { return SeamStitchLookup; }

		//~ Render Faces Group
		const TManagedArray<FIntVector3>* GetRenderIndices() const { return RenderIndices; }

		//~ Render Vertices Group
		const TManagedArray<FVector3f>* GetRenderPosition() const { return RenderPosition; }
		const TManagedArray<FVector3f>* GetRenderNormal() const { return RenderNormal; }
		const TManagedArray<FVector3f>* GetRenderTangentU() const { return RenderTangentU; }
		const TManagedArray<FVector3f>* GetRenderTangentV() const { return RenderTangentV; }
		const TManagedArray<TArray<FVector2f>>* GetRenderUVs() const { return RenderUVs; }
		const TManagedArray<FLinearColor>* GetRenderColor() const { return RenderColor; }
		const TManagedArray<TArray<int32>>* GetRenderBoneIndices() const { return RenderBoneIndices; }
		const TManagedArray<TArray<float>>* GetRenderBoneWeights() const { return RenderBoneWeights; }
		const TManagedArray<TArray<FVector4f>>* GetRenderDeformerPositionBaryCoordsAndDist() const { return RenderDeformerPositionBaryCoordsAndDist; }
		const TManagedArray<TArray<FVector4f>>* GetRenderDeformerNormalBaryCoordsAndDist() const { return RenderDeformerNormalBaryCoordsAndDist; }
		const TManagedArray<TArray<FVector4f>>* GetRenderDeformerTangentBaryCoordsAndDist() const { return RenderDeformerTangentBaryCoordsAndDist; }
		const TManagedArray<TArray<FIntVector3>>* GetRenderDeformerSimIndices3D() const { return RenderDeformerSimIndices3D; }
		const TManagedArray<TArray<float>>* GetRenderDeformerWeight() const { return RenderDeformerWeight; }
		const TManagedArray<float>* GetRenderDeformerSkinningBlend() const { return RenderDeformerSkinningBlend; }

		//~ LODs Group (There should be only one LOD per ClothCollection)
		TManagedArray<FString>* GetPhysicsAssetPathName(){ return PhysicsAssetPathName; }
		TManagedArray<FString>* GetSkeletalMeshPathName() { return SkeletalMeshPathName; }
		
		//~ Solver Group
		TManagedArray<FVector3f>* GetSolverGravity() { return SolverGravity; }
		TManagedArray<float>* GetSolverAirDamping() { return SolverAirDamping; }
		TManagedArray<int32>* GetSolverSubSteps() { return SolverSubSteps; }
		TManagedArray<float>* GetSolverTimeStep() { return SolverTimeStep; }
		
		//~ Fabric Group
		TManagedArray<FVector3f>* GetFabricBendingStiffness() { return FabricBendingStiffness; }
		TManagedArray<FVector3f>* GetFabricBucklingStiffness() { return FabricBucklingStiffness; }
		TManagedArray<FVector3f>* GetFabricStretchStiffness() { return FabricStretchStiffness; }
		TManagedArray<float>* GetFabricBucklingRatio() { return FabricBucklingRatio; }
		TManagedArray<float>* GetFabricDensity() { return FabricDensity; }
		TManagedArray<float>* GetFabricFriction() { return FabricFriction; }
		TManagedArray<float>* GetFabricDamping() { return FabricDamping; }
		TManagedArray<float>* GetFabricPressure() { return FabricPressure; }
		TManagedArray<int32>* GetFabricLayer() { return FabricLayer; }
		TManagedArray<float>* GetFabricCollisionThickness() { return FabricCollisionThickness; }
		
		//~ Seam Group
		TManagedArray<int32>* GetSeamStitchStart() { return SeamStitchStart; }
		TManagedArray<int32>* GetSeamStitchEnd() { return SeamStitchEnd; }

		//~ Seam Stitches Group
		TManagedArray<FIntVector2>* GetSeamStitch2DEndIndices() { return SeamStitch2DEndIndices; }
		TManagedArray<int32>* GetSeamStitch3DIndex() { return SeamStitch3DIndex; }

		//~ Sim Patterns Group
		TManagedArray<int32>* GetSimVertices2DStart() { return SimVertices2DStart; }
		TManagedArray<int32>* GetSimVertices2DEnd() { return SimVertices2DEnd; }
		TManagedArray<int32>* GetSimFacesStart() { return SimFacesStart; }
		TManagedArray<int32>* GetSimFacesEnd() { return SimFacesEnd; }
		TManagedArray<int32>* GetSimPatternFabric() { return SimPatternFabric; }

		//~ Render Patterns Group
		TManagedArray<int32>* GetRenderVerticesStart() { return RenderVerticesStart; }
		TManagedArray<int32>* GetRenderVerticesEnd() { return RenderVerticesEnd; }
		TManagedArray<int32>* GetRenderFacesStart() { return RenderFacesStart; }
		TManagedArray<int32>* GetRenderFacesEnd() { return RenderFacesEnd; }
		TManagedArray<int32>* GetRenderDeformerNumInfluences() { return RenderDeformerNumInfluences; }
		TManagedArray<FString>* GetRenderMaterialPathName() { return RenderMaterialPathName; }

		//~ Sim Faces Group
		TManagedArray<FIntVector3>* GetSimIndices2D() { return SimIndices2D; }
		TManagedArray<FIntVector3>* GetSimIndices3D() { return SimIndices3D; }

		//~ Sim Vertices 2D Group
		TManagedArray<FVector2f>* GetSimPosition2D() { return SimPosition2D; }
		TManagedArray<int32>* GetSimVertex3DLookup() { return SimVertex3DLookup; }

		//~ Sim Vertices 3D Group
		TManagedArray<FVector3f>* GetSimPosition3D() { return SimPosition3D; }
		TManagedArray<FVector3f>* GetSimNormal() { return SimNormal; }
		TManagedArray<TArray<int32>>* GetSimBoneIndices() { return SimBoneIndices; }
		TManagedArray<TArray<float>>* GetSimBoneWeights() { return SimBoneWeights; }
		TManagedArray<TArray<int32>>* GetTetherKinematicIndex() { return TetherKinematicIndex; }
		TManagedArray<TArray<float>>* GetTetherReferenceLength() { return TetherReferenceLength; }
		TManagedArray<TArray<int32>>* GetSimVertex2DLookup() { return SimVertex2DLookup; }
		TManagedArray<TArray<int32>>* GetSeamStitchLookup() { return SeamStitchLookup; }

		//~ Render Faces Group
		TManagedArray<FIntVector3>* GetRenderIndices() { return RenderIndices; }

		//~ Render Vertices Group
		TManagedArray<FVector3f>* GetRenderPosition() { return RenderPosition; }
		TManagedArray<FVector3f>* GetRenderNormal() { return RenderNormal; }
		TManagedArray<FVector3f>* GetRenderTangentU() { return RenderTangentU; }
		TManagedArray<FVector3f>* GetRenderTangentV() { return RenderTangentV; }
		TManagedArray<TArray<FVector2f>>* GetRenderUVs() { return RenderUVs; }
		TManagedArray<FLinearColor>* GetRenderColor() { return RenderColor; }
		TManagedArray<TArray<int32>>* GetRenderBoneIndices() { return RenderBoneIndices; }
		TManagedArray<TArray<float>>* GetRenderBoneWeights() { return RenderBoneWeights; }
		TManagedArray<TArray<FVector4f>>* GetRenderDeformerPositionBaryCoordsAndDist() { return RenderDeformerPositionBaryCoordsAndDist; }
		TManagedArray<TArray<FVector4f>>* GetRenderDeformerNormalBaryCoordsAndDist() { return RenderDeformerNormalBaryCoordsAndDist; }
		TManagedArray<TArray<FVector4f>>* GetRenderDeformerTangentBaryCoordsAndDist() { return RenderDeformerTangentBaryCoordsAndDist; }
		TManagedArray<TArray<FIntVector3>>* GetRenderDeformerSimIndices3D() { return RenderDeformerSimIndices3D; }
		TManagedArray<TArray<float>>* GetRenderDeformerWeight() { return RenderDeformerWeight; }
		TManagedArray<float>* GetRenderDeformerSkinningBlend() { return RenderDeformerSkinningBlend; }

	private:
		//~ Cloth collection
		TSharedRef<FManagedArrayCollection> ManagedArrayCollection;

		//~ LODs Group
		TManagedArray<FString>* PhysicsAssetPathName;
		TManagedArray<FString>* SkeletalMeshPathName;

		//~ Solvers group
		TManagedArray<FVector3f>* SolverGravity;
		TManagedArray<float>* SolverAirDamping;
		TManagedArray<int32>* SolverSubSteps;
		TManagedArray<float>* SolverTimeStep;

		//~ Fabrics Group
		TManagedArray<FVector3f>* FabricBendingStiffness;
		TManagedArray<FVector3f>* FabricBucklingStiffness;
		TManagedArray<FVector3f>* FabricStretchStiffness;
		TManagedArray<float>* FabricBucklingRatio;
		TManagedArray<float>* FabricDensity;
		TManagedArray<float>* FabricFriction;
		TManagedArray<float>* FabricDamping;
		TManagedArray<float>* FabricPressure;
		TManagedArray<int32>* FabricLayer;
		TManagedArray<float>* FabricCollisionThickness;

		//~ Seam Group
		TManagedArray<int32>* SeamStitchStart;
		TManagedArray<int32>* SeamStitchEnd;

		//~ Seam Stitches Group
		TManagedArray<FIntVector2>* SeamStitch2DEndIndices;  // Stitched 2D vertex indices pair
		TManagedArray<int32>* SeamStitch3DIndex;  // Corresponding stitched 3D vertex

		//~ Sim Patterns Group
		TManagedArray<int32>* SimVertices2DStart;
		TManagedArray<int32>* SimVertices2DEnd;
		TManagedArray<int32>* SimFacesStart;
		TManagedArray<int32>* SimFacesEnd;
		TManagedArray<int32>* SimPatternFabric;

		//~ Render Patterns Group
		TManagedArray<int32>* RenderVerticesStart;
		TManagedArray<int32>* RenderVerticesEnd;
		TManagedArray<int32>* RenderFacesStart;
		TManagedArray<int32>* RenderFacesEnd;
		TManagedArray<int32>* RenderDeformerNumInfluences;  // Number of deformer mapping influences per render vertex, either 0 (no mappings), 1 or 5 (= NUM_INFLUENCES_PER_VERTEX as defined in the ush files)
		TManagedArray<FString>* RenderMaterialPathName;

		//~ Sim Faces Group
		TManagedArray<FIntVector3>* SimIndices2D;
		TManagedArray<FIntVector3>* SimIndices3D;

		//~ Sim Vertices 2D Group
		TManagedArray<FVector2f>* SimPosition2D;
		TManagedArray<int32>* SimVertex3DLookup; // Lookup into corresponding 3D vertices

		//~ Sim Vertices 3D Group
		TManagedArray<FVector3f>* SimPosition3D;
		TManagedArray<FVector3f>* SimNormal;  // Used for capture, maxdistance, backstop authoring ...etc
		TManagedArray<TArray<int32>>* SimBoneIndices;
		TManagedArray<TArray<float>>* SimBoneWeights;
		TManagedArray<TArray<int32>>* TetherKinematicIndex;
		TManagedArray<TArray<float>>* TetherReferenceLength;
		TManagedArray<TArray<int32>>* SimVertex2DLookup; // Lookup into corresponding 2D vertices
		TManagedArray<TArray<int32>>* SeamStitchLookup; // Lookup into any seam stitches which weld this vertex

		//~ Render Faces Group
		TManagedArray<FIntVector3>* RenderIndices;

		//~ Render Vertices Group
		TManagedArray<FVector3f>* RenderPosition;
		TManagedArray<FVector3f>* RenderNormal;
		TManagedArray<FVector3f>* RenderTangentU;
		TManagedArray<FVector3f>* RenderTangentV;
		TManagedArray<TArray<FVector2f>>* RenderUVs;
		TManagedArray<FLinearColor>* RenderColor;
		TManagedArray<TArray<int32>>* RenderBoneIndices;
		TManagedArray<TArray<float>>* RenderBoneWeights;
		TManagedArray<TArray<FVector4f>>* RenderDeformerPositionBaryCoordsAndDist;  // Barycentric coords and distance along normal for the position of the final vert
		TManagedArray<TArray<FVector4f>>* RenderDeformerNormalBaryCoordsAndDist;  // Barycentric coords and distance along normal for the location of the unit normal endpoint
		TManagedArray<TArray<FVector4f>>* RenderDeformerTangentBaryCoordsAndDist;  // Barycentric coords and distance along normal for the location of the unit Tangent endpoint
		TManagedArray<TArray<FIntVector3>>* RenderDeformerSimIndices3D;  // The source mesh triangle
		TManagedArray<TArray<float>>* RenderDeformerWeight;  // For weighted averaging of multiple triangle influences
		TManagedArray<float>* RenderDeformerSkinningBlend;  // Render weight map. How much the vertex actually contributes, value between 0 (fully deformed) and 1 (fully skinned)
	};

	template<typename T>
	inline TConstArrayView<T> FClothCollection::GetElements(const TManagedArray<T>* ElementArray, const TManagedArray<int32>* StartArray, const TManagedArray<int32>* EndArray, int32 ArrayIndex)
	{
		if (StartArray && EndArray && ElementArray)
		{
			const int32 Start = (*StartArray)[ArrayIndex];
			const int32 End = (*EndArray)[ArrayIndex];
			if (Start != INDEX_NONE && End != INDEX_NONE)
			{
				return TConstArrayView<T>(ElementArray->GetData() + Start, End - Start + 1);
			}
			checkf(Start == End, TEXT("Only one boundary of the range is set to INDEX_NONE, when both should."));
		}
		return TConstArrayView<T>();
	}

	template<typename T>
	inline TArrayView<T> FClothCollection::GetElements(TManagedArray<T>* ElementArray, const TManagedArray<int32>* StartArray, const TManagedArray<int32>* EndArray, int32 ArrayIndex)
	{
		if (StartArray && EndArray && ElementArray)
		{
			const int32 Start = (*StartArray)[ArrayIndex];
			const int32 End = (*EndArray)[ArrayIndex];
			if (Start != INDEX_NONE && End != INDEX_NONE)
			{
				return TArrayView<T>(ElementArray->GetData() + Start, End - Start + 1);
			}
			checkf(Start == End, TEXT("Only one boundary of the range is set to INDEX_NONE, when both should."));
		}
		return TArrayView<T>();
	}

	template<typename T>
	inline TConstArrayView<T> FClothCollection::GetElements(const TManagedArray<T>* ElementArray)
	{
		if (ElementArray)
		{
			return TConstArrayView<T>(ElementArray->GetData(), ElementArray->Num());
		}
		return TConstArrayView<T>();
	}

	template<typename T>
	inline TArrayView<T> FClothCollection::GetElements(TManagedArray<T>* ElementArray)
	{
		if (ElementArray)
		{
			return TArrayView<T>(ElementArray->GetData(), ElementArray->Num());
		}
		return TArrayView<T>();
	}

	template<typename T>
	inline TConstArrayView<T> FClothCollection::GetSubElements(
		const TManagedArray<T>* SubElementArray,
		const TManagedArray<int32>* StartArray,
		const TManagedArray<int32>* EndArray,
		const TManagedArray<int32>* StartSubArray,
		const TManagedArray<int32>* EndSubArray,
		int32 ArrayIndex)
	{
		if (StartArray && EndArray && SubElementArray)
		{
			const TTuple<int32, int32> StartEnd = GetSubElementsStartEnd(StartArray, EndArray, StartSubArray, EndSubArray, ArrayIndex);
			const int32 Start = StartEnd.Get<0>();
			const int32 End = StartEnd.Get<1>();
			if (Start != INDEX_NONE && End != INDEX_NONE)
			{
				return TConstArrayView<T>(SubElementArray->GetData() + Start, End - Start + 1);
			}
			checkf(Start == End, TEXT("Only one boundary of the range is set to INDEX_NONE, when both should."));
		}
		return TConstArrayView<T>();
	}

	template<typename T>
	inline TArrayView<T> FClothCollection::GetSubElements(
		TManagedArray<T>* SubElementArray,
		const TManagedArray<int32>* StartArray,
		const TManagedArray<int32>* EndArray,
		const TManagedArray<int32>* StartSubArray,
		const TManagedArray<int32>* EndSubArray,
		int32 ArrayIndex)
	{
		if (StartArray && EndArray && SubElementArray)
		{
			const TTuple<int32, int32> StartEnd = GetSubElementsStartEnd(StartArray, EndArray, StartSubArray, EndSubArray, ArrayIndex);
			const int32 Start = StartEnd.Get<0>();
			const int32 End = StartEnd.Get<1>();
			if (Start != INDEX_NONE && End != INDEX_NONE)
			{
				return TArrayView<T>(SubElementArray->GetData() + Start, End - Start + 1);
			}
			checkf(Start == End, TEXT("Only one boundary of the range is set to INDEX_NONE, when both should."));
		}
		return TArrayView<T>();
	}

	template<typename T>
	inline void FClothCollection::CopyArrayViewData(const TArrayView<T>& To, const TConstArrayView<T>& From)
	{
		check(To.Num() == From.Num());
		for (int32 Index = 0; Index < To.Num(); ++Index)
		{
			To[Index] = From[Index];
		}
	}

	template<typename T, typename TEnableIf<!TIsArray<T>::Value, int>::type>
	inline void FClothCollection::CopyArrayViewDataAndApplyOffset(const TArrayView<T>& To, const TConstArrayView<T>& From, const T Offset)
	{
		check(To.Num() == From.Num());
		for (int32 Index = 0; Index < To.Num(); ++Index)
		{
			To[Index] = From[Index] + Offset;
		}
	}

	template<typename T>
	inline void FClothCollection::CopyArrayViewDataAndApplyOffset(const TArrayView<TArray<T>>& To, const TConstArrayView<TArray<T>>& From, const T Offset)
	{
		check(To.Num() == From.Num());
		for (int32 Index = 0; Index < To.Num(); ++Index)
		{
			To[Index] = From[Index];
			for (T& Value : To[Index])
			{
				Value += Offset;
			}
		}
	}

	template<typename T>
	inline uint32 FClothCollection::GetElementsTypeHash(const TManagedArray<T>* ElementArray)
	{
		if (ElementArray)
		{
			return GetTypeHash(*ElementArray);
		}
		return GetTypeHash(ElementArray);
	}
}  // End namespace UE::Chaos::ClothAsset
