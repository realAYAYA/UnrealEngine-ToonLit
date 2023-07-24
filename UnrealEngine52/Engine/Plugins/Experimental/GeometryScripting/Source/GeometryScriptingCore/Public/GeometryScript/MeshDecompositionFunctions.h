// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "GeometryScript/GeometryScriptSelectionTypes.h"
#include "MeshDecompositionFunctions.generated.h"

class UDynamicMesh;


UCLASS(meta = (ScriptName = "GeometryScript_MeshDecomposition"))
class GEOMETRYSCRIPTINGCORE_API UGeometryScriptLibrary_MeshDecompositionFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	/**
	 * Create a new Mesh for each Connected Component of TargetMesh.
	 * New meshes are drawn from MeshPool if it is provided, otherwise new UDynamicMesh instances are allocated
	 * @param ComponentMeshes New List of meshes is returned here
	 * @param MeshPool New meshes in ComponentMeshes output list are allocated from this pool if it is provided (highly recommended!!)
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Decomposition", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	SplitMeshByComponents(  
		UDynamicMesh* TargetMesh, 
		TArray<UDynamicMesh*>& ComponentMeshes,
		UDynamicMeshPool* MeshPool,
		UGeometryScriptDebug* Debug = nullptr);

	/**
	 * Create a new Mesh for each MaterialID of TargetMesh.
	 * New meshes are drawn from MeshPool if it is provided, otherwise new UDynamicMesh instances are allocated
	 * @param ComponentMeshes New List of meshes is returned here
	 * @param ComponentMaterialIDs MaterialID for each Mesh in ComponentMeshes is returned here
	 * @param MeshPool New meshes in ComponentMeshes output list are allocated from this pool if it is provided (highly recommended!!)
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Decomposition", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	SplitMeshByMaterialIDs(  
		UDynamicMesh* TargetMesh, 
		TArray<UDynamicMesh*>& ComponentMeshes,
		TArray<int>& ComponentMaterialIDs,
		UDynamicMeshPool* MeshPool,
		UGeometryScriptDebug* Debug = nullptr);

	/**
	 * Create a new Mesh for each Polygroup of TargetMesh. Note that this may be a *large* number of meshes!
	 * New meshes are drawn from MeshPool if it is provided, otherwise new UDynamicMesh instances are allocated
	 * @param ComponentMeshes New List of meshes is returned here
	 * @param ComponentPolygroups Original Polygroup for each Mesh in ComponentMeshes is returned here
	 * @param MeshPool New meshes in ComponentMeshes output list are allocated from this pool if it is provided (highly recommended!!)
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Decomposition", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	SplitMeshByPolygroups(  
		UDynamicMesh* TargetMesh, 
		FGeometryScriptGroupLayer GroupLayer,
		TArray<UDynamicMesh*>& ComponentMeshes,
		TArray<int>& ComponentPolygroups,
		UDynamicMeshPool* MeshPool,
		UGeometryScriptDebug* Debug = nullptr);

	/**
	 * CopyMeshSelectionToMesh should be used instead of this function
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Decomposition", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	GetSubMeshFromMesh(  
		UDynamicMesh* TargetMesh, 
		UPARAM(DisplayName = "Copy To Submesh", ref) UDynamicMesh* StoreToSubmesh, 
		FGeometryScriptIndexList TriangleList,
		UPARAM(DisplayName = "Copy To Submesh") UDynamicMesh*& StoreToSubmeshOut, 
		UGeometryScriptDebug* Debug = nullptr);

	/**
	 * Extract the triangles identified by Selection from TargetMesh and copy/add them to StoreToSubmesh
	 * @param bAppendToExisting if false (default), StoreToSubmesh is cleared, otherwise selected triangles are appended
	 * @param bPreserveGroupIDs if true, GroupIDs of triangles on TargetMesh are preserved in StoreToSubmesh. Otherwise new GroupIDs are allocated.
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Decomposition", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	CopyMeshSelectionToMesh(  
		UDynamicMesh* TargetMesh, 
		UPARAM(DisplayName = "Copy To Submesh", ref) UDynamicMesh* StoreToSubmesh, 
		FGeometryScriptMeshSelection Selection,
		UPARAM(DisplayName = "Copy To Submesh") UDynamicMesh*& StoreToSubmeshOut, 
		bool bAppendToExisting = false,
		bool bPreserveGroupIDs = false,
		UGeometryScriptDebug* Debug = nullptr);

	/**
	 * Set CopyToMesh to be the same mesh as CopyFromMesh
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Decomposition", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Copy From Mesh") UDynamicMesh* 
	CopyMeshToMesh(  
		UDynamicMesh* CopyFromMesh, 
		UPARAM(DisplayName = "Copy To Mesh", ref) UDynamicMesh* CopyToMesh, 
		UPARAM(DisplayName = "Copy To Mesh") UDynamicMesh*& CopyToMeshOut, 
		UGeometryScriptDebug* Debug = nullptr);

};