// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "MeshQueryFunctions.generated.h"

class UDynamicMesh;


UCLASS(meta = (ScriptName = "GeometryScript_MeshQueries"))
class GEOMETRYSCRIPTINGCORE_API UGeometryScriptLibrary_MeshQueryFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static FString GetMeshInfoString( UDynamicMesh* TargetMesh );

	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static bool GetIsDenseMesh( UDynamicMesh* TargetMesh );

	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static bool GetMeshHasAttributeSet( UDynamicMesh* TargetMesh );

	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Bounding Box") FBox GetMeshBoundingBox( UDynamicMesh* TargetMesh );

	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static void GetMeshVolumeArea( UDynamicMesh* TargetMesh, float& SurfaceArea, float& Volume );

	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static bool GetIsClosedMesh( UDynamicMesh* TargetMesh );

	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Num Loops") int32 GetNumOpenBorderLoops( UDynamicMesh* TargetMesh, bool& bAmbiguousTopologyFound );

	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Num Edges") int32 GetNumOpenBorderEdges( UDynamicMesh* TargetMesh );

	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Num Edges") int32 GetNumConnectedComponents( UDynamicMesh* TargetMesh );

	// UDynamicMesh already has this function
	//UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	//static UPARAM(DisplayName = "Triangle Count") int32 GetTriangleCount( UDynamicMesh* TargetMesh );

	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Num Triangle IDs") int32 GetNumTriangleIDs( UDynamicMesh* TargetMesh );

	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static bool GetHasTriangleIDGaps( UDynamicMesh* TargetMesh );

	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static bool IsValidTriangleID( UDynamicMesh* TargetMesh, int32 TriangleID );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	GetAllTriangleIDs( UDynamicMesh* TargetMesh, FGeometryScriptIndexList& TriangleIDList, bool& bHasTriangleIDGaps );


	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Indices") FIntVector GetTriangleIndices( UDynamicMesh* TargetMesh, int32 TriangleID, bool& bIsValidTriangle );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	GetAllTriangleIndices( UDynamicMesh* TargetMesh, FGeometryScriptTriangleList& TriangleList, bool bSkipGaps, bool& bHasTriangleIDGaps);

	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static void GetTrianglePositions( UDynamicMesh* TargetMesh, int32 TriangleID, bool& bIsValidTriangle, FVector& Vertex1, FVector& Vertex2, FVector& Vertex3 );

	/**
	 * Compute the interpolated Position (A*Vertex1 + B*Vertex2 + C*Vertex3), where (A,B,C)=BarycentricCoords and the Vertex positions are taken
	 * from the specified TriangleID of the TargetMesh. 
	 * @param bIsValidTriangle will be returned true if TriangleID exists in TargetMesh, and otherwise will be returned false
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	GetInterpolatedTrianglePosition( 
		UDynamicMesh* TargetMesh, 
		int32 TriangleID, 
		FVector BarycentricCoords, 
		bool& bIsValidTriangle,
		FVector& InterpolatedPosition );


	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Normal") FVector GetTriangleFaceNormal( UDynamicMesh* TargetMesh, int32 TriangleID, bool& bIsValidTriangle );

	/**
	 * Compute the barycentric coordinates (A,B,C) of the Point relative to the specified TriangleID of the TargetMesh.
	 * The properties of barycentric coordinates are such that A,B,C are all positive, A+B+C=1.0, and A*Vertex1 + B*Vertex2 + C*Vertex3 = Point.
	 * So, the barycentric coordinates can be used to smoothly interpolate/blend any other per-triangle-vertex quantities.
	 * The Point must lie in the plane of the Triangle, otherwise the coordinates are somewhat meaningless (but clamped to 0-1 range to avoid catastrophic errors)
	 * The Positions of the Triangle Vertices are also returned for convenience (similar to GetTrianglePositions)
	 * @param bIsValidTriangle will be returned true if TriangleID exists in TargetMesh, and otherwise will be returned false
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ComputeTriangleBarycentricCoords( 
		UDynamicMesh* TargetMesh, 
		int32 TriangleID, 
		bool& bIsValidTriangle, 
		FVector Point, 
		FVector& Vertex1, 
		FVector& Vertex2, 
		FVector& Vertex3, 
		FVector& BarycentricCoords );



	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Vertex Count") int32 GetVertexCount( UDynamicMesh* TargetMesh );

	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Num Vertex IDs") int32 GetNumVertexIDs( UDynamicMesh* TargetMesh );

	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static bool GetHasVertexIDGaps( UDynamicMesh* TargetMesh );

	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static bool IsValidVertexID( UDynamicMesh* TargetMesh, int32 VertexID );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	GetAllVertexIDs( UDynamicMesh* TargetMesh, FGeometryScriptIndexList& VertexIDList, bool& bHasVertexIDGaps );

	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Position") FVector GetVertexPosition( UDynamicMesh* TargetMesh, int32 VertexID, bool& bIsValidVertex );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	GetAllVertexPositions( UDynamicMesh* TargetMesh, FGeometryScriptVectorList& PositionList, bool bSkipGaps, bool& bHasVertexIDGaps );


	//
	// UV Queries
	//


	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod), DisplayName = "Get Num UV Channels")
	static UPARAM(DisplayName = "Num UV Channels") int32 GetNumUVSets( UDynamicMesh* TargetMesh );

	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Bounding Box") FBox2D GetUVSetBoundingBox( UDynamicMesh* TargetMesh, 
																			UPARAM(DisplayName = "UV Channel") int UVSetIndex, 
																			UPARAM(DisplayName = "Is Valid UV Channel") bool& bIsValidUVSet, 
																			UPARAM(DisplayName = "UV Channel Is Empty") bool& bUVSetIsEmpty );

	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static void GetTriangleUVs( UDynamicMesh* TargetMesh, UPARAM(DisplayName = "UV Channel") int32 UVSetIndex, int32 TriangleID, FVector2D& UV1, FVector2D& UV2, FVector2D& UV3, bool& bHaveValidUVs );

	/**
	 * Compute the interpolated UV (A*UV1+ B*UV2+ C*UV3), where (A,B,C)=BarycentricCoords and the UV positions are taken
	 * from the specified TriangleID in the specified UVSet of the TargetMesh. 
	 * @param bIsValidTriangle bTriHasValidUVs be returned true if TriangleID exists in TargetMesh and is set to valid UVs in the UV Set, and otherwise will be returned false
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	GetInterpolatedTriangleUV( 
		UDynamicMesh* TargetMesh, 
		UPARAM(DisplayName = "UV Channel") int32 UVSetIndex, 
		int32 TriangleID, 
		FVector BarycentricCoords, 
		bool& bTriHasValidUVs, 
		FVector2D& InterpolatedUV );


	//
	// Normal queries
	//

	/**
	 * @return true if the TargetMesh has the Normals Attribute enabled (which allows for storing split normals)
	 */
	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Enabled") bool GetHasTriangleNormals( UDynamicMesh* TargetMesh );

	/**
	 * For the specified TriangleID of the TargetMesh, get the Normal vectors at each vertex of the Triangle.
	 * These Normals will be taken from the Normal Overlay, ie they will potentially be split-normals.
	 * @param bTriHasValidNormals will be returned true if TriangleID exists in TargetMesh and has Normals set
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	GetTriangleNormals( 
		UDynamicMesh* TargetMesh, 
		int32 TriangleID, 
		FVector& Normal1, 
		FVector& Normal2, 
		FVector& Normal3, 
		bool& bTriHasValidNormals );

	/**
	 * Compute the interpolated Normal (A*Normal1 + B*Normal2 + C*Normal3), where (A,B,C)=BarycentricCoords and the Normals are taken
	 * from the specified TriangleID in the Normal layer of the TargetMesh. 
	 * @param bTriHasValidNormals will be returned true if TriangleID exists in TargetMesh and has Normals set, and otherwise will be returned false
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	GetInterpolatedTriangleNormal( 
		UDynamicMesh* TargetMesh, 
		int32 TriangleID, 
		FVector BarycentricCoords, 
		bool& bTriHasValidNormals,
		FVector& InterpolatedNormal );

	/**
	 * For the specified TriangleID of the TargetMesh, get the Normal and Tangent vectors at each vertex of the Triangle.
	 * These Normals/Tangents will be taken from the Normal and Tangents Overlays, ie they will potentially be split-normals.
	 * @param bTriHasValidElements will be returned true if TriangleID exists in TargetMesh and has Normals and Tangents set
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	GetTriangleNormalTangents( 
		UDynamicMesh* TargetMesh, 
		int32 TriangleID, 
		bool& bTriHasValidElements,
		FGeometryScriptTriangle& Normals,
		FGeometryScriptTriangle& Tangents,
		FGeometryScriptTriangle& BiTangents);

	/**
	 * Compute the interpolated Normal and Tangents for the specified specified TriangleID in the Normal and Tangent attributes of the TargetMesh. 
	 * @param bTriHasValidElements will be returned true if TriangleID exists in TargetMesh and has Normals and Tangents set, and otherwise will be returned false
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	GetInterpolatedTriangleNormalTangents( 
		UDynamicMesh* TargetMesh, 
		int32 TriangleID, 
		FVector BarycentricCoords, 
		bool& bTriHasValidElements,
		FVector& InterpolatedNormal, 
		FVector& InterpolatedTangent,
		FVector& InterpolatedBiTangent);


	//
	// Vertex Color Queries
	//


	/**
	 * @return true if the TargetMesh has the Vertex Colors attribute enabled
	 */
	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Enabled") bool GetHasVertexColors( UDynamicMesh* TargetMesh );

	/**
	 * For the specified TriangleID of the TargetMesh, get the Vertex Colors at each vertex of the Triangle.
	 * These Colors will be taken from the Vertex Color Attribute, ie they will potentially be split-colors.
	 * @param bTriHasValidVertexColors will be returned true if TriangleID exists in TargetMesh and has Vertex Colors set
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	GetTriangleVertexColors( 
		UDynamicMesh* TargetMesh, 
		int32 TriangleID, 
		FLinearColor& Color1, 
		FLinearColor& Color2, 
		FLinearColor& Color3, 
		bool& bTriHasValidVertexColors );

	/**
	 * Compute the interpolated Vertex Color (A*Color1 + B*Color2 + C*Color3), where (A,B,C)=BarycentricCoords and the Colors are taken
	 * from the specified TriangleID in the Vertex Color layer of the TargetMesh. 
	 * @param bTriHasValidVertexColors will be returned true if TriangleID exists in TargetMesh and has Colors set, and otherwise will be returned false
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	GetInterpolatedTriangleVertexColor( 
		UDynamicMesh* TargetMesh, 
		int32 TriangleID, 
		FVector BarycentricCoords, 
		FLinearColor DefaultColor,
		bool& bTriHasValidVertexColors,
		FLinearColor& InterpolatedColor );


	//
	// Material Queries
	//

	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Enabled") bool GetHasMaterialIDs( UDynamicMesh* TargetMesh );


	//
	// Polygroup Queries
	//

	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod, DisplayName = "GetHasPolyGroups"))
	static UPARAM(DisplayName = "Enabled") bool GetHasPolygroups( UDynamicMesh* TargetMesh );

	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries", meta=(ScriptMethod, DisplayName = "GetNumExtendedPolyGroupLayers"))
	static UPARAM(DisplayName = "Num Layers") int GetNumExtendedPolygroupLayers( UDynamicMesh* TargetMesh );


};