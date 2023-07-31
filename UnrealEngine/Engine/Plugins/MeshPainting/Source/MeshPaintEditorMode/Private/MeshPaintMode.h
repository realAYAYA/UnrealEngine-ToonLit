// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/UEdMode.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandList.h"
#include "Containers/ArrayView.h"
#include "LevelEditorViewport.h"	// For FTrackingTransaction
#include "UnrealEdMisc.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/Commands/UICommandList.h"
#include "IMeshPaintComponentAdapter.h"
#include "MeshPaintHelpers.h"
#include "MeshVertexPaintingTool.h"
#include "Tools/LegacyEdModeInterfaces.h"
#include "MeshPaintMode.generated.h"

class UMeshVertexPaintingToolProperties;
class UMeshColorPaintingToolProperties;
class UMeshTexturePaintingToolProperties;
class UMeshPaintModeSettings;
class IMeshPaintComponentAdapter;
class UMeshComponent;
class UMeshToolManager;

/**
 * Mesh paint Mode.  Extends editor viewports with the ability to paint data on meshes
 */
UCLASS()
class UMeshPaintMode : public UEdMode, public ILegacyEdModeViewportInterface
{
public:
	GENERATED_BODY()

	/** Default constructor for UMeshPaintMode */
	UMeshPaintMode();
	static UMeshVertexPaintingToolProperties* GetVertexToolProperties();
	static UMeshColorPaintingToolProperties* GetColorToolProperties();
	static UMeshWeightPaintingToolProperties* GetWeightToolProperties();
	static UMeshTexturePaintingToolProperties* GetTextureToolProperties();
	static UMeshPaintMode* GetMeshPaintMode();
	virtual void Enter() override;
	virtual void Exit() override;
	virtual void CreateToolkit() override;
	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;
	static FName MeshPaintMode_Color;
	static FName MeshPaintMode_Texture;
	static FName MeshPaintMode_Weights;
	static FString VertexSelectToolName;
	static FString TextureSelectToolName;
	static FString ColorPaintToolName;
	static FString WeightPaintToolName;
	static FString TexturePaintToolName;

	virtual TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> GetModeCommands() const override;
	/** Returns the instance of ComponentClass found in the current Editor selection */
	template<typename ComponentClass>
	TArray<ComponentClass*> GetSelectedComponents() const;

	uint32 GetCachedVertexDataSize() const
	{
		return CachedVertexDataSize;
	}

protected:

	/** Binds UI commands to actions for the mesh paint mode */
	virtual void BindCommands() override;

	// UEdMode interface
	virtual void OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;
	virtual void OnToolEnded(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;
	virtual void ActorSelectionChangeNotify() override;
	virtual void ActivateDefaultTool() override;
	virtual void UpdateOnPaletteChange(FName NewPalette);
	// end UEdMode Interface
	void UpdateSelectedMeshes();

	void CheckSelectionForTexturePaintCompat(const TArray<UMeshComponent*>& CurrentMeshComponents);
	

	void UpdateOnMaterialChange(bool bInvalidateHitProxies);
	void OnObjectsReplaced(const TMap<UObject*, UObject*>& InOldToNewInstanceMap);
	void OnResetViewMode();
	void FillWithVertexColor();
	void PropagateVertexColorsToAsset();
	bool CanPropagateVertexColors() const;
	void ImportVertexColors();
	void SavePaintedAssets();
	bool CanSaveMeshPackages() const;
	bool CanRemoveInstanceColors() const;
	bool CanPasteInstanceVertexColors() const;
	bool CanCopyInstanceVertexColors() const;
	bool CanPropagateVertexColorsToLODs() const;
	/** Copy and pasting functionality from and to a StaticMeshComponent (currently only supports per instance)*/
	void CopyVertexColors();
	void PasteVertexColors();
	void FixVertexColors();
	bool DoesRequireVertexColorsFixup() const;
	void RemoveVertexColors();
	void PropagateVertexColorsToLODs();
	void UpdateCachedVertexDataSize();
	void CycleMeshLODs(int32 Direction);
	void CycleTextures(int32 Direction);
	bool CanCycleTextures() const;
	void CommitAllPaintedTextures();
	int32 GetNumberOfPendingPaintChanges();
	void OnVertexPaintFinished();

protected:
	UPROPERTY(Transient)
	TObjectPtr<UMeshPaintModeSettings> ModeSettings;


	// End vertex paint state
	FGetSelectedMeshComponents MeshComponentDelegate;
	uint32 CachedVertexDataSize;
	bool bRecacheVertexDataSize;

	FDelegateHandle PaletteChangedHandle;

};

