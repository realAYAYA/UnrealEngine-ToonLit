// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "MeshPaintingToolsetTypes.h"
#include "EditorSubsystem.h"
#include "IMeshPaintComponentAdapter.h"

class FMeshPaintParameters;
class UImportVertexColorOptions;
class UTexture2D;
class UStaticMeshComponent;
class UStaticMesh;
class USkeletalMesh;
class IMeshPaintComponentAdapter;
class UPaintBrushSettings;
class FEditorViewportClient;
class UMeshComponent;
class USkeletalMeshComponent;
class UViewportInteractor;
class FViewport;
class FPrimitiveDrawInterface;
class FSceneView;

struct FStaticMeshComponentLODInfo;
struct FPerComponentVertexColorData;

enum class EMeshPaintDataColorViewMode : uint8;

class MESHPAINTEDITORMODE_API UMeshPaintModeSubsystem : public UEditorSubsystem
{
public:
	/** Forces the Viewport Client to render using the given Viewport Color ViewMode */
	void SetViewportColorMode(EMeshPaintDataColorViewMode ColorViewMode, FEditorViewportClient* ViewportClient);

	/** Sets whether or not the level viewport should be real time rendered move or viewport as parameter? */
	void SetRealtimeViewport(bool bRealtime);


	/** Helper function to import Vertex Colors from a Texture to the specified MeshComponent (makes use of SImportVertexColorsOptions Widget) */
	void ImportVertexColorsFromTexture(UMeshComponent* MeshComponent);

	/** Imports vertex colors from a Texture to the specified Skeletal Mesh according to user-set options */
	void ImportVertexColorsToSkeletalMesh(USkeletalMesh* SkeletalMesh, const UImportVertexColorOptions* Options, UTexture2D* Texture);

	struct FPaintRay
	{
		FVector CameraLocation;
		FVector RayStart;
		FVector RayDirection;
		UViewportInteractor* ViewportInteractor;
	};


	bool RetrieveViewportPaintRays(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI, TArray<FPaintRay>& OutPaintRays);

	/** Imports vertex colors from a Texture to the specified Static Mesh according to user-set options */
	void ImportVertexColorsToStaticMesh(UStaticMesh* StaticMesh, const UImportVertexColorOptions* Options, UTexture2D* Texture);

	/** Imports vertex colors from a Texture to the specified Static Mesh Component according to user-set options */
	void ImportVertexColorsToStaticMeshComponent(UStaticMeshComponent* StaticMeshComponent, const UImportVertexColorOptions* Options, UTexture2D* Texture);

	void PropagateVertexColors(const TArray<UStaticMeshComponent *> StaticMeshComponents);
	bool CanPropagateVertexColors(TArray<UStaticMeshComponent*>& StaticMeshComponents, TArray<UStaticMesh*>& StaticMeshes, int32 NumInstanceVertexColorBytes);
	void CopyVertexColors(const TArray<UStaticMeshComponent*> StaticMeshComponents, TArray<FPerComponentVertexColorData>& CopiedVertexColors);
	bool CanCopyInstanceVertexColors(const TArray<UStaticMeshComponent*>& StaticMeshComponents, int32 PaintingMeshLODIndex);
	void PasteVertexColors(const TArray<UStaticMeshComponent*>& StaticMeshComponents, TArray<FPerComponentVertexColorData>& CopiedColorsByComponent);
	bool CanPasteInstanceVertexColors(const TArray<UStaticMeshComponent*>& StaticMeshComponents, const TArray<FPerComponentVertexColorData>& CopiedColorsByComponent);
	void RemovePerLODColors(const TArray<UMeshComponent*>& PaintableComponents);

	void SwapVertexColors();
	void SaveModifiedTextures();
	bool CanSaveModifiedTextures();
};