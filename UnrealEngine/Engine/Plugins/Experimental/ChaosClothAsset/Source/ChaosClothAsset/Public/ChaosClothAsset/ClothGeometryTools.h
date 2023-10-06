// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

struct FManagedArrayCollection;
namespace UE::Geometry { class FDynamicMesh3; }

namespace UE::Chaos::ClothAsset
{
	/**
	 * Geometry tools operating on cloth collections.
	 */
	struct CHAOSCLOTHASSET_API FClothGeometryTools
	{

		/** Return whether at least one pattern of this collection has any faces to simulate. */
		static bool HasSimMesh(const TSharedRef<const FManagedArrayCollection>& ClothCollection);

		/** Return whether at least one pattern of this collection has any faces to render. */
		static bool HasRenderMesh(const TSharedRef<const FManagedArrayCollection>& ClothCollection);

		/** Delete the render mesh data. */
		static void DeleteRenderMesh(const TSharedRef<FManagedArrayCollection>& ClothCollection);

		/** Delete the sim mesh data. */
		static void DeleteSimMesh(const TSharedRef<FManagedArrayCollection>& ClothCollection);

		/** Remove all tethers. */
		static void DeleteTethers(const TSharedRef<FManagedArrayCollection>& ClothCollection);

		/** Turn the sim mesh portion of this ClothCollection into a render mesh. */
		static void CopySimMeshToRenderMesh(const TSharedRef<FManagedArrayCollection>& ClothCollection, const FString& RenderMaterialPathName, bool bSingleRenderPattern);

		/** Reverse the mesh normals. Will reverse all normals if pattern selection is empty. */
		static void ReverseMesh(
			const TSharedRef<FManagedArrayCollection>& ClothCollection,
			bool bReverseSimMeshNormals,
			bool bReverseSimMeshWindingOrder,
			bool bReverseRenderMeshNormals,
			bool bReverseRenderMeshWindingOrder,
			const TArray<int32>& SimPatternSelection,
			const TArray<int32>& RenderPatternSelection);

		/**
		 * Set the skinning weights for all of the sim/render vertices in ClothCollection to be bound to the root node.
		 *
		 * @param Lods if empty will apply the change to all LODs. Otherwise only LODs specified in the array (if exist) are affected.
		 */
		static void BindMeshToRootBone(
			const TSharedRef<FManagedArrayCollection>& ClothCollection,
			bool bBindSimMesh,
			bool bBindRenderMesh);

		/**
		* Unwrap and build SimMesh data from a DynamicMesh
		*/
		static void BuildSimMeshFromDynamicMesh(
			const TSharedRef<FManagedArrayCollection>& ClothCollection,
			const UE::Geometry::FDynamicMesh3& DynamicMesh, int32 UVChannelIndex, const FVector2f& UVScale, bool bAppend);

		/**
		* Remove (topologically) degenerate triangles. Remove any vertices that aren't in a triangle. Compact any lookup arrays that contain INDEX_NONEs.
		* Remove any empty patterns.
		*/
		static void CleanupAndCompactMesh(const TSharedRef<FManagedArrayCollection>& ClothCollection);
	};
}  // End namespace UE::Chaos::ClothAsset