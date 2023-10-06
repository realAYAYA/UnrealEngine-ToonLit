// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "GeometryScript/GeometryScriptSelectionTypes.h"
#include "MeshPolygroupFunctions.generated.h"

class UDynamicMesh;


UCLASS(meta = (ScriptName = "GeometryScript_PolyGroups"))
class GEOMETRYSCRIPTINGCORE_API UGeometryScriptLibrary_MeshPolygroupFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/**
	* Enables the standard PolyGroup Layer on the Target Mesh.
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|PolyGroups", meta = (ScriptMethod, DisplayName = "EnablePolyGroups"))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	EnablePolygroups( UDynamicMesh* TargetMesh, UGeometryScriptDebug* Debug = nullptr );

	/**
	* Sets the number of extended PolyGroup Layers on a Mesh.
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|PolyGroups", meta = (ScriptMethod, DisplayName = "SetNumExtendedPolyGroupLayers"))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	SetNumExtendedPolygroupLayers( 
		UDynamicMesh* TargetMesh, 
		int NumLayers,
		UGeometryScriptDebug* Debug = nullptr );

	/**
	* Resets the triangle PolyGroup assignments within a PolyGroup Layer to the given Clear Value (or 0 if no Clear Value is specified).
	* Note, this will have no effect if PolyGroups have not been enabled on the mesh, or if the requested Group Layer does not exist. 
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|PolyGroups", meta = (ScriptMethod, DisplayName = "ClearPolyGroups"))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ClearPolygroups( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptGroupLayer GroupLayer,
		int ClearValue = 0,
		UGeometryScriptDebug* Debug = nullptr );

	/**
	* Copies the triangle PolyGroup assignments from one layer on the Target Mesh to another.
	* Note, this will have no effect if PolyGroups have not been enabled on the mesh, or if one of the requested Group Layers does not exist.
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|PolyGroups", meta = (ScriptMethod, DisplayName = "CopyPolyGroupsLayer"))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	CopyPolygroupsLayer( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptGroupLayer FromGroupLayer,
		FGeometryScriptGroupLayer ToGroupLayer,
		UGeometryScriptDebug* Debug = nullptr );

	/**
	* Creates and assigns a new PolyGroup for each disconnected UV island of a Mesh.
	* Note, this will have no effect if either the requested UV Layer or Group Layer does not exist.
	* @param GroupLayer indicates PolyGroup Layer that will be populated with unique values for each UV island.
	* @param UVLayer specifies the UV Layer used to construct the PolyGroups.
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|PolyGroups", meta = (ScriptMethod, DisplayName = "ConvertUVIslandsToPolyGroups"))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ConvertUVIslandsToPolygroups( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptGroupLayer GroupLayer,
		int UVLayer = 0,
		UGeometryScriptDebug* Debug = nullptr );

	/**
	* Creates and assigns a new PolyGroup for each disconnected component of a Mesh. Regions of a mesh are disconnected they do not have a triangle in common.
	* Note, this will have no effect if the Group Layer does not exist.
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|PolyGroups", meta = (ScriptMethod, DisplayName = "ConvertComponentsToPolyGroups"))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ConvertComponentsToPolygroups( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptGroupLayer GroupLayer,
		UGeometryScriptDebug* Debug = nullptr );

	/**
	* Sets PolyGroups by partitioning the mesh based on an edge crease/opening-angle.
	* Note, this will have no effect if the Group Layer does not exist.
	* @param GroupLayer indicates the PolyGroup Layer that will be populated.
	* @param CreaseAngle measured in degrees and used when comparing adjacent faces.
	* @param MinGroupSize the minimum number of triangles in each PolyGroup.
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|PolyGroups", meta = (ScriptMethod, DisplayName = "ComputePolyGroupsFromAngleThreshold"))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ComputePolygroupsFromAngleThreshold( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptGroupLayer GroupLayer,
		float CreaseAngle = 15,
		int MinGroupSize = 2,
		UGeometryScriptDebug* Debug = nullptr );

	/**
	* Sets PolyGroups by identifying adjacent triangles that form reasonable quads. Note any triangles that do not neatly pair to form quads will receive their own PolyGroup.  
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|PolyGroups", meta = (ScriptMethod, DisplayName = "ComputePolyGroupsFromPolygonDetection"))
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

	/**
	* Gets the PolyGroup ID associated with the specified Triangle ID and stored in the Group Layer.
	* If the Group Layer or the Triangle does not exist, the value 0 will be returned and bIsValidTriangle set to false.
	* @param GroupLayer indicates the layer on the Target Mesh to query.
	* @param TriangleID identifies a triangle in the Target Mesh.
	* @param bIsValidTriangle will be populated on return with false if either the Group Layer or the Triangle does not exist.
	*/
	UFUNCTION(BlueprintPure, Category = "GeometryScript|PolyGroups", meta = (ScriptMethod, DisplayName = "GetTrianglePolyGroupID"))
	static UPARAM(DisplayName = "PolyGroup ID") int32
	GetTrianglePolygroupID( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptGroupLayer GroupLayer, 
		int TriangleID, 
		bool& bIsValidTriangle );

	/**
	 * Deletes all triangles from the Target Mesh that have a particular PolyGroup ID, in the specific Group Layer.
	 * @param GroupLayer specifies the PolyGroup Layer to query.
	 * @param PolyGroup ID identifies the triangles in the Target Mesh to delete.
	 * @param NumDeleted on return will contain the number of triangles deleted from the Target Mesh.
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|PolyGroups", meta = (ScriptMethod, DisplayName = "DeleteTrianglesInPolyGroup"))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	DeleteTrianglesInPolygroup( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptGroupLayer GroupLayer,
		UPARAM(DisplayName = "PolyGroup ID") int PolygroupID,
		int& NumDeleted,
		bool bDeferChangeNotifications = false,
		UGeometryScriptDebug* Debug = nullptr);

	/**
	* Create list of per-triangle PolyGroup IDs for the PolyGroup in the Mesh
	* @warning if the mesh is not Triangle-Compact (eg GetHasTriangleIDGaps == false) then the returned list will also have the same gaps
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|PolyGroups", meta = (ScriptMethod, DisplayName = "GetAllTrianglePolyGroupIDs"))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	GetAllTrianglePolygroupIDs( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptGroupLayer GroupLayer, 
		UPARAM(ref, DisplayName="PolyGroup IDs Out") FGeometryScriptIndexList& PolygroupIDsOut );

	/**
	* Create list of all unique PolyGroup IDs that exist in the PolyGroup Layer in the Mesh
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|PolyGroups", meta = (ScriptMethod, DisplayName = "GetPolyGroupIDsInMesh"))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	GetPolygroupIDsInMesh( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptGroupLayer GroupLayer, 
		UPARAM(ref, DisplayName="PolyGroup IDs Out") FGeometryScriptIndexList& PolygroupIDsOut );

	/**
	 * Create list of all triangles with the given PolyGroup ID in the given GroupLayer (not necessarily a single connected-component)
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|PolyGroups", meta = (ScriptMethod, DisplayName = "GetTrianglesInPolyGroup"))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	GetTrianglesInPolygroup( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptGroupLayer GroupLayer, 
		UPARAM(DisplayName = "PolyGroup ID") int PolygroupID, 
		UPARAM(ref, DisplayName="Triangle IDs Out") FGeometryScriptIndexList& TriangleIDsOut );


	/**
	 * Set a new PolyGroup on all the triangles of the given Selection, for the given GroupLayer.
	 * @param SetPolygroupID explicit new PolyGroupID to set
	 * @param bGenerateNewPolygroup if true, SetPolyGroupID is ignored and a new unique PolyGroupID is generated
	 * @param SetPolygroupIDOut the PolyGroupID that was set on the triangles is returned here (whether explicit or auto-generated)
	 * @param bDeferChangeNotifications if true, the UDynamicMesh does not emit a change event/signal for this modification
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|PolyGroups", meta = (ScriptMethod, DisplayName = "SetPolyGroupForMeshSelection"))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	SetPolygroupForMeshSelection( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptGroupLayer GroupLayer, 
		FGeometryScriptMeshSelection Selection,
		UPARAM(DisplayName = "Set PolyGroup ID Out") int& SetPolygroupIDOut,
		UPARAM(DisplayName = "Set PolyGroup ID") int SetPolygroupID = 0,
		UPARAM(DisplayName = " Generate New PolyGroup") bool bGenerateNewPolygroup = false,
		bool bDeferChangeNotifications = false);
};