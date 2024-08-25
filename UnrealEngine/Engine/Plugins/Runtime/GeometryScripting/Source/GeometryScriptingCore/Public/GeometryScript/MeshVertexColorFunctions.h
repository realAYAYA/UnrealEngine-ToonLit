// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "GeometryScript/GeometryScriptSelectionTypes.h"
#include "MeshVertexColorFunctions.generated.h"

class UDynamicMesh;


UENUM(BlueprintType)
enum class EGeometryScriptBlurColorMode : uint8
{
	/** Blur the attributes where each neighbor is weighted equally. */
	Uniform = 0,
	/** Blur the attributes where each neighbor is weighted proportionally to the shared edge length. */
	EdgeLength = 1,
	/** Blur the attributes where each neighbor is weighted proportionally to the cotangent weight of the shared edge. */
	CotanWeights = 2
};

USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptBlurMeshVertexColorsOptions
{
	GENERATED_BODY()

	/** Blur red channel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool Red = true;

	/** Blur green channel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool Green = true;

	/** Blur blue channel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool Blue = true;

	/** Blur alpha channel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool Alpha = true;
};



UCLASS(meta = (ScriptName = "GeometryScript_VertexColors"))
class GEOMETRYSCRIPTINGCORE_API UGeometryScriptLibrary_MeshVertexColorFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	/**
	 * Set all vertex colors (optionally specific channels) in the TargetMesh VertexColor Overlay to a constant value
	 * @param Color the constant color to set
	 * @param Flags specify which RGBA channels to set (default all channels)
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|VertexColor", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh*
	SetMeshConstantVertexColor(
		UDynamicMesh* TargetMesh,
		FLinearColor Color,
		FGeometryScriptColorFlags Flags,
		bool bClearExisting = false,
		UGeometryScriptDebug* Debug = nullptr);


	/**
	 * Set the colors in the TargetMesh VertexColor Overlay identified by the Selection to a constant value.
	 * For a Vertex Selection, each existing VertexColor Overlay Element for the vertex is updated.
	 * For a Triangle or PolyGroup Selection, all Overlay Elements in the identified Triangles are updated.
	 * @param Color the constant color to set
	 * @param Flags specify which RGBA channels to set (default all channels)
	 * @param bCreateColorSeam if true, a "hard edge" in the vertex colors is created, by creating new Elements for all the triangles in the selection. If enabled, Vertex selections are converted to Triangle selections, and Flags is ignored. 
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|VertexColor", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh*
	SetMeshSelectionVertexColor(
		UDynamicMesh* TargetMesh,
		FGeometryScriptMeshSelection Selection,
		FLinearColor Color,
		FGeometryScriptColorFlags Flags,
		bool bCreateColorSeam = false,
		UGeometryScriptDebug* Debug = nullptr);


	/**
	 * Set all vertex colors in the TargetMesh VertexColor Overlay to the specified per-vertex colors
	 * @param VertexColorList per-vertex colors. Size must be less than or equal to the MaxVertexID of TargetMesh  (ie gaps are supported)
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|VertexColor", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh*
	SetMeshPerVertexColors(
		UDynamicMesh* TargetMesh,
		FGeometryScriptColorList VertexColorList,
		UGeometryScriptDebug* Debug = nullptr);

	/**
	 * Get a list of single vertex colors for each mesh vertex in the TargetMesh, derived from the VertexColor Overlay.
	 * The VertexColor Overlay may store multiple colors for a single vertex (ie different colors for that vertex on different triangles)
	 * In such cases the colors can either be averaged, or the last color seen will be used, depending on the bBlendSplitVertexValues parameter.
	 * @param ColorList output color list will be stored here. Size will be equal to the MaxVertexID of TargetMesh  (not the VertexCount!)
	 * @param bIsValidColorSet will be set to true if the VertexColor Overlay was valid
	 * @param bHasVertexIDGaps will be set to true if some vertex indices in TargetMesh were invalid, ie MaxVertexID > VertexCount 
	 * @param bBlendSplitVertexValues control how multiple colors at the same vertex should be interpreted
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|VertexColor", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	GetMeshPerVertexColors( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptColorList& ColorList, 
		bool& bIsValidColorSet,
		bool& bHasVertexIDGaps,
		bool bBlendSplitVertexValues = true);

	/**
	 * Apply a SRGB to Linear color transformation on all vertex colors
	 * on the mesh.
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|VertexColor", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh*
	ConvertMeshVertexColorsSRGBToLinear(
		UDynamicMesh* TargetMesh,
		UGeometryScriptDebug* Debug = nullptr);

	/**
	 * Apply a Linear to SRGB color transformation on all vertex colors
	 * on the mesh.
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|VertexColor", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh*
	ConvertMeshVertexColorsLinearToSRGB(
		UDynamicMesh* TargetMesh,
		UGeometryScriptDebug* Debug = nullptr);


	/**
     * Blur the color attribute of the mesh. If the mesh has no color attribute, the function returns the mesh unchanged.
	 * 
     * @param TargetMesh The mesh containing the color attribute. 
     * @param Selection Only vertices in the selection will have their color attribute blurred.
     * @param NumIterations The number of blur iterations.
     * @param Strength Each iteration, we will blur between the vertex of the color at the previous iteration and its neighbors' average by Strength amount (expected to be in the zero to one range).
     * @param BlurMode Determines how neighbors are weighted when computing their average.
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|VertexColor", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh*
	BlurMeshVertexColors(
		UDynamicMesh* TargetMesh,
		FGeometryScriptMeshSelection Selection,
		int NumIterations = 1,
		double Strength = 0.5,
		EGeometryScriptBlurColorMode BlurMode = EGeometryScriptBlurColorMode::Uniform,
		FGeometryScriptBlurMeshVertexColorsOptions Options = FGeometryScriptBlurMeshVertexColorsOptions(),
		UGeometryScriptDebug* Debug = nullptr);
};

