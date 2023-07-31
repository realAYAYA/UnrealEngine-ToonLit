// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BevelType.h"
#include "Util.h"
#include "Mesh.h"
#include "ContourNode.h"


/** Makes actual bevel. */
class FMeshCreator final
{
public:
	FMeshCreator();

	/**
	 * CreateMeshes
	 * @param Root - Tree of contours.	 
	 * @param Extrude - Orthogonal (to front cap) offset value.
	 * @param Bevel - Bevel value (bevel happens before extrude).
	 * @param Type - Defines shape of beveled part.
	 * @param BevelSegments - Segments count.
	 * @param bOutline - Front face has outline and is not filled.
	 * @param OutlineExpand - Offsets the outline by the specified amount.
	 */
	void CreateMeshes(const TSharedContourNode& Root, const float Extrude, const float Bevel, const EText3DBevelType Type, const int32 BevelSegments, const bool bOutline, const float OutlineExpand = 0.5f);
	void SetFrontAndBevelTextureCoordinates(const float Bevel);
	void MirrorGroups(const float Extrude);
	void BuildMesh(UStaticMesh* StaticMesh, class UMaterial* DefaultMaterial);

private:
	TSharedPtr<class FText3DGlyph> Glyph;
	TSharedRef<class FData> Data;
	TSharedPtr<class FContourList> Contours;


	/**
	 * Create 'Front' part of glyph.
	 * @param Root - Tree root.
	 * @param bOutline - Front face has outline and is not filled.
	 * @param OutlineExpand - Offsets the outline by the specified amount.
	 */
	void CreateFrontMesh(const TSharedContourNode& Root, const bool bOutline, const float& OutlineExpand = 0.5f);
	/**
	 * Create 'Bevel' part of glyph (actually half of it, will be mirrored later).
	 * @param Bevel - Bevel value (bevel happens before extrude).
	 * @param Type - Defines shape of beveled part.
	 * @param BevelSegments - Segments count.
	 */
	void CreateBevelMesh(const float Bevel, const EText3DBevelType Type, const int32 BevelSegments);
	/**
	 * Create 'Extrude' part of glyph.
	 * @param Extrude - Extrude value.
	 * @param Bevel - Bevel value (bevel happens before extrude).
	 * @param Type - Defines shape of beveled part.
	 * @param bFlipNormals - Reverse the geometry facing direction.
	 */
	void CreateExtrudeMesh(float Extrude, float Bevel, const EText3DBevelType Type, bool bFlipNormals = false);

	void MirrorGroup(const EText3DGroupType TypeIn, const EText3DGroupType TypeOut, const float Extrude);


	/**
	 * Recursively compute vertex count in tree.
	 * @param Node - Tree node.
	 * @param OutVertexCount - Pointer to counter.
	 */
	void AddToVertexCount(const TSharedContourNode& Node, int32& OutVertexCount);
	/**
	 * Triangulate solid region with holes and convert contours to old format.
	 * @param Node - Tree node.
	 * @param OutVertexIndex - Pointer to last used vertex index.
	 * @param bOutline - Front face has outline and is not filled.
	 */
	void TriangulateAndConvert(const TSharedContourNode& Node, int32& OutVertexIndex, const bool bOutline);

	void MakeOutline(float OutlineExpand = 0.5f);

	void BevelLinearWithSegments(const float Extrude, const float Expand, const int32 BevelSegments, const FVector2D Normal);
	void BevelCurve(const float Angle, const int32 BevelSegments, TFunction<FVector2D(const float CurrentCos, const float CurrentSin, const float NextCos, const float Next)> ComputeOffset);
	void BevelWithSteps(const float Bevel, const int32 Steps, const int32 BevelSegments);

	/**
	 * Bevel one segment.
	 * @param Extrude - Documented in ContourList.h.
	 * @param Expand - Documented in ContourList.h.
	 * @param NormalStart - Normal at start of segment (minimum DoneExpand).
	 * @param NormalEnd - Normal at end of segment (maximum DoneExpand).
	 * @param bSmooth - Is angle between start of this segment and end of previous segment smooth?
	 */
	void BevelLinear(const float Extrude, const float Expand, FVector2D NormalStart, FVector2D NormalEnd, const bool bSmooth);

	/**
	 * Duplicate contour vertices (used to make sharp angle between bevel steps)
	 */
	void DuplicateContourVertices();

	/**
	 * Continue with trivial bevel till FData::Expand.
	 */
	void BevelPartsWithoutIntersectingNormals();

	/**
	 * Clear PathPrev and PathNext.
	 * @param Point - Point which paths should be cleared.
	 */
	void EmptyPaths(const FPartPtr& Point) const;

	/**
	 * Same as previous function but does not cover the intersection-case.
	 * @param Point - Point that should be expanded.
	 * @param TextureCoordinates - Texcture coordinates of added vertices.
	 */
	void ExpandPoint(const FPartPtr& Point, const FVector2D TextureCoordinates = FVector2D(0.f, 0.f));
	/**
	 * Common code for expanding, vertices are added uninitialized.
	 * @param Point - Expanded point.
	 */
	void ExpandPointWithoutAddingVertices(const FPartPtr& Point) const;

	/**
	 * Add vertex for smooth point.
	 * @param Point - Expanded point.
	 * @param TextureCoordinates - Texture coordinates of added vertex.
	 */
	void AddVertexSmooth(const FPartConstPtr& Point, const FVector2D TextureCoordinates);
	/**
	 * Add vertex for sharp point.
	 * @param Point - Expanded point.
	 * @param Edge - Edge from which TangentX and TangentZ will be assigned.
	 * @param TextureCoordinates - Texture coordinates of added vertex.
	 */
	void AddVertexSharp(const FPartConstPtr& Point, const FPartConstPtr& Edge, const FVector2D TextureCoordinates);
};
