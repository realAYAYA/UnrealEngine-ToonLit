// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "GeometryScript/GeometryScriptSelectionTypes.h"
#include "MeshBasicEditFunctions.generated.h"

class UDynamicMesh;
namespace UE::Geometry
{
	class FDynamicMesh3;
}


USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptSimpleMeshBuffers
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	TArray<FVector> Vertices;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	TArray<FVector> Normals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	TArray<FVector2D> UV0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	TArray<FVector2D> UV1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	TArray<FVector2D> UV2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	TArray<FVector2D> UV3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	TArray<FVector2D> UV4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	TArray<FVector2D> UV5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	TArray<FVector2D> UV6;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	TArray<FVector2D> UV7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	TArray<FLinearColor> VertexColors;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	TArray<FIntVector> Triangles;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	TArray<int> TriGroupIDs;
};


// Options for how attributes from a source and target mesh are combined into the target mesh
UENUM(BlueprintType)
enum class EGeometryScriptCombineAttributesMode : uint8
{
	// Include attributes enabled on either the source or target mesh
	EnableAllMatching,
	// Only include attributes that are already enabled on the target mesh
	UseTarget,
	// Make the target mesh have only the attributes that are enabled on the source mesh
	UseSource
};


/**
 * Control how details like mesh attributes are handled when one mesh is appended to another
 */
USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptAppendMeshOptions
{
	GENERATED_BODY()
public:

	// How attributes from each mesh are combined into the result
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	EGeometryScriptCombineAttributesMode CombineMode =
		EGeometryScriptCombineAttributesMode::EnableAllMatching;

	void UpdateAttributesForCombineMode(UE::Geometry::FDynamicMesh3& Target, const UE::Geometry::FDynamicMesh3& Source);
};


UCLASS(meta = (ScriptName = "GeometryScript_MeshEdits"))
class GEOMETRYSCRIPTINGCORE_API UGeometryScriptLibrary_MeshBasicEditFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshEdits", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	DiscardMeshAttributes( 
		UDynamicMesh* TargetMesh, 
		bool bDeferChangeNotifications = false );



	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshEdits", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	SetVertexPosition( 
		UDynamicMesh* TargetMesh, 
		int VertexID, 
		FVector NewPosition, 
		bool& bIsValidVertex, 
		bool bDeferChangeNotifications = false );


	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshEdits", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	AddVertexToMesh( 
		UDynamicMesh* TargetMesh, 
		FVector NewPosition, 
		int& NewVertexIndex,
		bool bDeferChangeNotifications = false );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshEdits", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	AddVerticesToMesh( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptVectorList NewPositionsList, 
		FGeometryScriptIndexList& NewIndicesList,
		bool bDeferChangeNotifications = false );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshEdits", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	DeleteVertexFromMesh( 
		UDynamicMesh* TargetMesh, 
		int VertexID,
		bool& bWasVertexDeleted,
		bool bDeferChangeNotifications = false );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshEdits", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	DeleteVerticesFromMesh( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptIndexList VertexList,
		int& NumDeleted,
		bool bDeferChangeNotifications = false );


	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshEdits", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	AddTriangleToMesh( 
		UDynamicMesh* TargetMesh, 
		FIntVector NewTriangle,
		int& NewTriangleIndex,
		int NewTriangleGroupID = 0,
		bool bDeferChangeNotifications = false,
		UGeometryScriptDebug* Debug = nullptr );


	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshEdits", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	AddTrianglesToMesh( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptTriangleList NewTrianglesList,
		FGeometryScriptIndexList& NewIndicesList,
		int NewTriangleGroupID = 0,
		bool bDeferChangeNotifications = false,
		UGeometryScriptDebug* Debug = nullptr );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshEdits", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	DeleteTriangleFromMesh( 
		UDynamicMesh* TargetMesh, 
		int TriangleID,
		bool& bWasTriangleDeleted,
		bool bDeferChangeNotifications = false );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshEdits", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	DeleteTrianglesFromMesh( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptIndexList TriangleList,
		int& NumDeleted,
		bool bDeferChangeNotifications = false );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshEdits", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	DeleteSelectedTrianglesFromMesh( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptMeshSelection Selection,
		int& NumDeleted,
		bool bDeferChangeNotifications = false );

	/**
	 * Apply AppendTransform to AppendMesh and then add its geometry to the TargetMesh
	 * @param AppendOptions Control how details like mesh attributes are handled when one mesh is appended to another
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshEdits", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	AppendMesh( 
		UDynamicMesh* TargetMesh, 
		UDynamicMesh* AppendMesh, 
		FTransform AppendTransform, 
		bool bDeferChangeNotifications = false,
		FGeometryScriptAppendMeshOptions AppendOptions = FGeometryScriptAppendMeshOptions(),
		UGeometryScriptDebug* Debug = nullptr);

	/**
	 * For each transform in AppendTransforms, apply the transform to AppendMesh and then add its geometry to the TargetMesh.
	 * @param ConstantTransform the Constant transform will be applied after each Append transform
	 * @param bConstantTransformIsRelative if true, the Constant transform is applied "in the frame" of the Append Transform, otherwise it is applied as a second transform in local coordinates (ie rotate around the AppendTransform X axis, vs around the local X axis)
	 * @param AppendOptions Control how details like mesh attributes are handled when one mesh is appended to another
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshEdits", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	AppendMeshTransformed( 
		UDynamicMesh* TargetMesh, 
		UDynamicMesh* AppendMesh, 
		const TArray<FTransform>& AppendTransforms, 
		FTransform ConstantTransform,
		bool bConstantTransformIsRelative = true,
		bool bDeferChangeNotifications = false,
		FGeometryScriptAppendMeshOptions AppendOptions = FGeometryScriptAppendMeshOptions(),
		UGeometryScriptDebug* Debug = nullptr);

	/**
	 * Repeatedly apply AppendTransform to the AppendMesh, each time adding the geometry to TargetMesh.
	 * @param RepeatCount number of times to repeat the transform-append cycle
	 * @param bApplyTransformToFirstInstance if true, the AppendTransform is applied before the first mesh append, otherwise it is applied after
	 * @param AppendOptions Control how details like mesh attributes are handled when one mesh is appended to another
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshEdits", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	AppendMeshRepeated( 
		UDynamicMesh* TargetMesh, 
		UDynamicMesh* AppendMesh, 
		FTransform AppendTransform, 
		int RepeatCount = 1,
		bool bApplyTransformToFirstInstance = true,
		bool bDeferChangeNotifications = false,
		FGeometryScriptAppendMeshOptions AppendOptions = FGeometryScriptAppendMeshOptions(),
		UGeometryScriptDebug* Debug = nullptr);



	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshEdits", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	AppendBuffersToMesh( 
		UDynamicMesh* TargetMesh, 
		const FGeometryScriptSimpleMeshBuffers& Buffers,
		FGeometryScriptIndexList& NewTriangleIndicesList,
		int MaterialID = 0,
		bool bDeferChangeNotifications = false,
		UGeometryScriptDebug* Debug = nullptr );

};