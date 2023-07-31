// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "SceneUtilityFunctions.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UDynamicMesh;
class UDynamicMeshPool;

USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptCopyMeshFromComponentOptions
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bWantNormals = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bWantTangents = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	FGeometryScriptMeshReadLOD RequestedLOD = FGeometryScriptMeshReadLOD();
};


UCLASS(meta = (ScriptName = "GeometryScript_SceneUtils"))
class GEOMETRYSCRIPTINGCORE_API UGeometryScriptLibrary_SceneUtilityFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	/**
	 * Create a new UDynamicMeshPool object. 
	 * Caller needs to create a UProperty reference to the returned object, or it will be garbage-collected.
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Utility")
	static UPARAM(DisplayName = "Dynamic Mesh Pool") UDynamicMeshPool*
	CreateDynamicMeshPool();


	/**
	 * Copy the mesh from a given Component to a Dynamic Mesh.
	 * StaticMeshComponent, DynamicMeshCompnent, and BrushComponent are supported.
	 * This function offers minimal control over the copying, if more control is needed for Static Meshes, use CopyMeshFromStaticMesh.
	 * @param bTransformToWorld if true, output mesh is in World space
	 * @param LocalToWorld local-to-world transform is returned here (whether mesh is in local or world space)
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Scene", meta = (ExpandEnumAsExecs = "Outcome"))
	static UPARAM(DisplayName = "Dynamic Mesh") UDynamicMesh*
	CopyMeshFromComponent(
		USceneComponent* Component,
		UDynamicMesh* ToDynamicMesh,
		FGeometryScriptCopyMeshFromComponentOptions Options,
		bool bTransformToWorld,
		FTransform& LocalToWorld,
		TEnumAsByte<EGeometryScriptOutcomePins>& Outcome,
		UGeometryScriptDebug* Debug = nullptr);


	/**
	 * Configure the Material set on a PrimitiveComponent, by repeatedly calling SetMaterial.
	 * This is a simple utility function and it's behavior will depend on the specifics of the Component.
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Scene")
	static void
	SetComponentMaterialList(
		UPrimitiveComponent* Component, 
		const TArray<UMaterialInterface*>& MaterialList,
		UGeometryScriptDebug* Debug = nullptr);


	/**
	 * Extract the Collision Geometry from FromObject and copy/approximate it with meshes stored in ToDynamicMesh.
	 * For Simple Collision, FromObject can be a StaticMesh Asset or any PrimitiveComponent
	 * For Complex Collision, FromObject can be a StaticMesh Asset, StaticMeshComponent, or DynamicMeshComponent
	 * @param bTransformToWorld if true, output mesh is in World space
	 * @param LocalToWorld local-to-world transform is returned here (whether mesh is in local or world space)
	 * @param bUseComplexCollision if true, complex collision is extracted, otherwise Simple collision shapes are meshed
	 * @param SphereResolution determines tessellation density of sphere and capsule shapes
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Scene", meta = (ExpandEnumAsExecs = "Outcome"))
	static UPARAM(DisplayName = "Dynamic Mesh") UDynamicMesh*
	CopyCollisionMeshesFromObject(
		UObject* FromObject,
		UDynamicMesh* ToDynamicMesh,
		bool bTransformToWorld,
		FTransform& LocalToWorld,
		TEnumAsByte<EGeometryScriptOutcomePins>& Outcome,
		bool bUseComplexCollision = false,
		int SphereResolution = 16,
		UGeometryScriptDebug* Debug = nullptr);

};