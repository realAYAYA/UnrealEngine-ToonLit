// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "GeometryScript/GeometryScriptSelectionTypes.h"
#include "MeshPolygroupFunctions.generated.h"

class UDynamicMesh;


UCLASS(meta = (ScriptName = "GeometryScript_Polygroups"))
class GEOMETRYSCRIPTINGCORE_API UGeometryScriptLibrary_MeshPolygroupFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Polygroups", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	EnablePolygroups( UDynamicMesh* TargetMesh, UGeometryScriptDebug* Debug = nullptr );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Polygroups", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	SetNumExtendedPolygroupLayers( 
		UDynamicMesh* TargetMesh, 
		int NumLayers,
		UGeometryScriptDebug* Debug = nullptr );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Polygroups", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ClearPolygroups( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptGroupLayer GroupLayer,
		int ClearValue = 0,
		UGeometryScriptDebug* Debug = nullptr );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Polygroups", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	CopyPolygroupsLayer( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptGroupLayer FromGroupLayer,
		FGeometryScriptGroupLayer ToGroupLayer,
		UGeometryScriptDebug* Debug = nullptr );


	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Polygroups", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ConvertUVIslandsToPolygroups( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptGroupLayer GroupLayer,
		int UVLayer = 0,
		UGeometryScriptDebug* Debug = nullptr );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Polygroups", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ConvertComponentsToPolygroups( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptGroupLayer GroupLayer,
		UGeometryScriptDebug* Debug = nullptr );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Polygroups", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ComputePolygroupsFromAngleThreshold( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptGroupLayer GroupLayer,
		float CreaseAngle = 15,
		int MinGroupSize = 2,
		UGeometryScriptDebug* Debug = nullptr );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Polygroups", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ComputePolygroupsFromPolygonDetection( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptGroupLayer GroupLayer,
		bool bRespectUVSeams = true,
		bool bRespectHardNormals = false,
		double QuadAdjacencyWeight = 1.0,
		double QuadMetricClamp = 1.0,
		int MaxSearchRounds = 1,
		UGeometryScriptDebug* Debug = nullptr );

	UFUNCTION(BlueprintPure, Category = "GeometryScript|Polygroups", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Polygroup ID") int32
	GetTrianglePolygroupID( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptGroupLayer GroupLayer, 
		int TriangleID, 
		bool& bIsValidTriangle );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Polygroups", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	DeleteTrianglesInPolygroup( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptGroupLayer GroupLayer,
		int PolygroupID,
		int& NumDeleted,
		bool bDeferChangeNotifications = false,
		UGeometryScriptDebug* Debug = nullptr);

	/**
	* Create list of per-triangle Polygroup IDs for the Polygroup in the Mesh
	* @warning if the mesh is not Triangle-Compact (eg GetHasTriangleIDGaps == false) then the returned list will also have the same gaps
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Polygroups", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	GetAllTrianglePolygroupIDs( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptGroupLayer GroupLayer, 
		UPARAM(ref, DisplayName="Polygroup IDs Out") FGeometryScriptIndexList& PolygroupIDsOut );

	/**
	* Create list of all unique Polygroup IDs that exist in the Polygroup Layer in the Mesh
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Polygroups", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	GetPolygroupIDsInMesh( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptGroupLayer GroupLayer, 
		UPARAM(ref, DisplayName="Polygroup IDs Out") FGeometryScriptIndexList& PolygroupIDsOut );

	/**
	 * Create list of all triangles with the given Polygroup ID in the given GroupLayer (not necessarily a single connected-component)
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Polygroups", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	GetTrianglesInPolygroup( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptGroupLayer GroupLayer, 
		int PolygroupID, 
		UPARAM(ref, DisplayName="Triangle IDs Out") FGeometryScriptIndexList& TriangleIDsOut );


	/**
	 * Set a new Polygroup on all the triangles of the given Selection, for the given GroupLayer.
	 * @param SetPolygroupID explicit new PolygroupID to set
	 * @param bGenerateNewPolygroup if true, SetPolygroupID is ignored and a new unique PolygroupID is generated
	 * @param SetPolygroupIDOut the PolygroupID that was set on the triangles is returned here (whether explicit or auto-generated)
	 * @param bDeferChangeNotifications if true, the UDynamicMesh does not emit a change event/signal for this modification
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Polygroups", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	SetPolygroupForMeshSelection( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptGroupLayer GroupLayer, 
		FGeometryScriptMeshSelection Selection,
		int& SetPolygroupIDOut,
		int SetPolygroupID = 0,
		bool bGenerateNewPolygroup = false,
		bool bDeferChangeNotifications = false);
};