// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaDefs.h"
#include "AvaShapeMesh.h"
#include "AvaShapesDefs.h"
#include "AvaShapeUVParameters.h"
#include "Components/ActorComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "IAvaInteractiveToolsModeDetailsObject.h"
#include "Viewport/Interaction/AvaSnapPoint.h"
#include "Viewport/Interaction/IAvaGizmoObject.h"

#include "AvaShapeDynMeshBase.generated.h"

class AAvaShapeActor;
class UDynamicMeshComponent;
class UStaticMesh;

DECLARE_MULTICAST_DELEGATE_OneParam(FMaskEnabledDelegate, AActor* /** NewMaskActor */);
DECLARE_MULTICAST_DELEGATE_OneParam(FMaskDisabledDelegate, AActor* /** OldMaskActor */);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMaskVisibilityDelegate, const UWorld* /** CurrentWorld */, bool /** bMaskActorVisible */);

UCLASS(MinimalAPI, ClassGroup="Shape", Abstract, BlueprintType, CustomConstructor, EditInlineNew, DefaultToInstanced)
class UAvaShapeDynamicMeshBase
	: public UActorComponent
	, public IAvaInteractiveToolsModeDetailsObject
	, public IAvaGizmoObjectInterface
{
	GENERATED_BODY()

	friend class FAvaDynMeshBaseDetailCustomization;
	friend class FAvaMeshesDetailCustomization;
	friend class FAvaShapeDynamicMeshVisualizer;
	friend class AAvaShapeActor;

public:
	static constexpr float MinSizeValue = UE_SMALL_NUMBER;
	static inline const FVector2D MinSize2D = FVector2D(MinSizeValue);
	static inline const FVector MinSize3D = FVector(MinSizeValue);

	static constexpr uint8 DefaultSubdivisions = 8;
	static constexpr uint8 MaxSubdivisions = 64;

	static constexpr int32 MESH_INDEX_NONE = -1;
	static constexpr int32 MESH_INDEX_ALL = INT_MAX;
	static constexpr int32 MESH_INDEX_PRIMARY = 0;

	static inline FMaskEnabledDelegate OnMaskEnabled;
	static inline FMaskDisabledDelegate OnMaskDisabled;
	static inline FMaskVisibilityDelegate OnMaskVisibility;

	UAvaShapeDynamicMeshBase(const FObjectInitializer& ObjectInitializer)
		: UAvaShapeDynamicMeshBase(FLinearColor::White)
	{}

	UAvaShapeDynamicMeshBase(const FLinearColor& InVertexColor = FLinearColor::White,
		float InUniformScaledSize = 1.f, bool bInAllowEditSize = true);

	// Converts the Outer of this object to a AAvaShapeActor.
	AVALANCHESHAPES_API AAvaShapeActor* GetShapeActor() const;

	// Gets the Dynamic Mesh component from the Shape actor, this will load it if its nullptr
	AVALANCHESHAPES_API UDynamicMeshComponent* GetShapeMeshComponent() const;

	/** Get the name of the shape */
	virtual const FString& GetMeshName() const;

	/** Can we change the size of this shape */
	bool AllowsSizeEditing() const
	{
		return bAllowEditSize;
	}

	// override in child classes, represents the mesh size
	virtual void SetSize3D(const FVector& InSize) {}
	virtual const FVector& GetSize3D() const
	{
		return FVector::ZeroVector;
	}

	// Primary material override
	AVALANCHESHAPES_API void SetUsePrimaryMaterialEverywhere(bool bInUse);
	bool GetUsePrimaryMaterialEverywhere() const
	{
		return bUsePrimaryMaterialEverywhere;
	}

	AVALANCHESHAPES_API void SetUniformScaledSize(float InSize);
	float GetUniformScaledSize() const
	{
		return UniformScaledSize;
	}

	/** Checks if shape size is render-able and not minimal */
	bool IsMeshSizeValid() const;

	/** Checks if the mesh section is valid */
	AVALANCHESHAPES_API bool IsValidMeshIndex(int32 MeshIndex) const;

	AVALANCHESHAPES_API UMaterialInterface* GetMaterial(int32 MeshIndex) const;
	AVALANCHESHAPES_API bool SetMaterial(int32 MeshIndex, UMaterialInterface* NewMaterial);

	AVALANCHESHAPES_API const FAvaShapeParametricMaterial* GetParametricMaterial(int32 MeshIndex) const;
	AVALANCHESHAPES_API FAvaShapeParametricMaterial* GetParametricMaterial(int32 MeshIndex);

	// Internal materials are not copied over, only settings
	AVALANCHESHAPES_API bool SetParametricMaterial(int32 MeshIndex, const FAvaShapeParametricMaterial& NewMaterialParams);

	// Allows custom uv params per mesh section instead of using the primary one
	bool SetOverridePrimaryUVParams(int32 MeshIndex, bool bOverride);

	// Helper function to quickly get what you need instead of using GetMeshData()
	AVALANCHESHAPES_API EAvaShapeUVMode GetMaterialUVMode(int32 MeshIndex) const;
	AVALANCHESHAPES_API bool SetMaterialUVMode(int32 MeshIndex, EAvaShapeUVMode InUVMode);

	EAvaAnchors GetMaterialUVAnchorPreset(int32 MeshIndex) const;
	bool SetMaterialUVAnchorPreset(int32 MeshIndex, EAvaAnchors InUVAnchorPreset);

	AVALANCHESHAPES_API float GetMaterialUVRotation(int32 MeshIndex) const;
	AVALANCHESHAPES_API bool SetMaterialUVRotation(int32 MeshIndex, float InUVRotation);

	AVALANCHESHAPES_API const FVector2D& GetMaterialUVAnchor(int32 MeshIndex) const;
	AVALANCHESHAPES_API bool SetMaterialUVAnchor(int32 MeshIndex, const FVector2D& InUVAnchor);

	AVALANCHESHAPES_API const FVector2D& GetMaterialUVScale(int32 MeshIndex) const;
	AVALANCHESHAPES_API bool SetMaterialUVScale(int32 MeshIndex, const FVector2D& InUVScale);

	AVALANCHESHAPES_API const FVector2D& GetMaterialUVOffset(int32 MeshIndex) const;
	AVALANCHESHAPES_API bool SetMaterialUVOffset(int32 MeshIndex, const FVector2D& InUVOffset);

	AVALANCHESHAPES_API bool GetMaterialHorizontalFlip(int32 MeshIndex) const;
	AVALANCHESHAPES_API bool SetMaterialHorizontalFlip(int32 MeshIndex, bool InHorizontalFlip);

	AVALANCHESHAPES_API bool GetMaterialVerticalFlip(int32 MeshIndex) const;
	AVALANCHESHAPES_API bool SetMaterialVerticalFlip(int32 MeshIndex, bool InVerticalFlip);

	AVALANCHESHAPES_API const FAvaShapeMaterialUVParameters* GetInUseMaterialUVParams(int32 MeshIndex) const;
	AVALANCHESHAPES_API FAvaShapeMaterialUVParameters* GetInUseMaterialUVParams(int32 MeshIndex);

	const FAvaShapeMaterialUVParameters* GetMaterialUVParams(int32 MeshIndex) const;
	bool SetMaterialUVParams(int32 MeshIndex, const FAvaShapeMaterialUVParameters& InParams);

	bool SetMaterialType(int32 MeshIndex, EMaterialType Type);
	AVALANCHESHAPES_API bool IsMaterialType(int32 MeshIndex, EMaterialType Type);

	bool HasMeshRegenWorldLocation() const { return bHasNewMeshRegenWorldLocation; }
	const FVector& GetMeshRegenWorldLocation() const { return MeshRegenWorldLocation; }
	AVALANCHESHAPES_API void SetMeshRegenWorldLocation(const FVector& NewLocation, bool bImmediateUpdate = false);

	// Generate a list of 3d-space snap points for this shape
	virtual TArray<FAvaSnapPoint> GetLocalSnapPoints() const;
	void GetLocalSnapPoints(TArray<FAvaSnapPoint>& Points) const;

	AVALANCHESHAPES_API FTransform GetTransform() const;

	/** Clear the dynamic mesh section with a specific index */
	bool ClearDynamicMeshSection(int32 MeshIndex);

	/** Clear the whole dynamic mesh */
	bool ClearDynamicMesh();

	/** Converts dynamic mesh to static mesh */
	AVALANCHESHAPES_API bool ExportToStaticMesh(UStaticMesh* DestinationMesh);

	// Gets the bounds of the shape, override this in child classes for custom bounds
	// Origin is the center of the box, BoxExtent is half the size, pivot is the default location of the pivot for this shape
	virtual void GetBounds(FVector& Origin, FVector& BoxExtent, FVector& Pivot) const {}

	virtual void OnColorPicked(const FAvaColorChangeData& InNewColorData);
	virtual FAvaColorChangeData GetActiveColor() const;

	/** Get the registered meshes indexes */
	AVALANCHESHAPES_API TSet<int32> GetMeshesIndexes() const;

	/** Returns the name of the meshes */
	AVALANCHESHAPES_API TArray<FName> GetMeshDataNames() const;

	/** find a registered mesh and gets a pointer to it */
	FAvaShapeMeshData* GetMeshData(int32 MeshIndex);

	// apply scale, offset, rotation on dynamic mesh section
	bool ApplyUVsTransform(FAvaShapeMesh& InMesh, FAvaShapeMaterialUVParameters& InParams, FVector2D ShapeSize, FVector2D UVFixOffset, float UVFixRotation = 0.f) const;

protected:
	static bool SetAlignmentSize(AActor* InActor, const FVector& InSizeMultiplier);
	static EAvaAnchors GetAnchorFromNumerics(const FVector2D& AnchorNumeric);
	static FVector2D GetNumericsFromAnchor(EAvaAnchors AnchorEnum);

	//~ Begin UActorComponent
	virtual void OnRegister() override;
	virtual void OnComponentCreated() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	//~ End UActorComponent

	//~ Begin UObject
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;
	virtual void PostEditImport() override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	//~ End UObject

	// triggers when we drop an asset (material) onto our actor
	void OnAssetDropped(UObject* DroppedObj, AActor* TargetActor);
#endif

	// ~Begin IAvaGizmoObjectInterface
	virtual void ToggleGizmo_Implementation(const UAvaGizmoComponent* InGizmoComponent, const bool bShowAsGizmo) override;
	// ~End IAvaGizmoObjectInterface

	EAvaDynamicMeshUpdateState GetMeshUpdateStatus() const { return MeshUpdateStatus; }

	/** Flag all mesh sections dirty to regenerate them */
	void MarkAllMeshesDirty();

	/** Only update vertices from mesh */
	void MarkVerticesDirty();

	// When a parametric material is updated
	void OnParametricMaterialChanged(FAvaShapeParametricMaterial& InMaterial);

	/** Register all meshes once, calls RegisterMeshes() */
	void SetupMeshes();

	/** override this in child classes, to register new meshes */
	virtual void RegisterMeshes() {}

	/** Call to register meshes used for this shape, call this inside RegisterMeshes() */
	bool RegisterMesh(FAvaShapeMeshData& NewMeshData);

	/** Called when a new mesh has been registered */
	virtual void OnRegisteredMesh(int32 MeshIndex) {}

	/** Called when all the meshes have been registered and the setup is done */
	virtual void OnRegisteredMeshes() {}

	/** Returns whether the mesh is dirty, called before creating mesh if visible */
	virtual bool IsMeshDirty(int32 MeshIndex);

	/** override this in child classes, called before creating mesh if visible */
	virtual bool IsMeshVisible(int32 MeshIndex);

	/** Returns the name of the mesh */
	FName GetMeshDataName(int32 InMeshIndex) const;

	/** Returns the materials used by every mesh */
	const TArray<UMaterialInterface*> GetMeshDataMaterials();

	/** Checks to see if attributes are initialized and mesh is ready */
	bool IsDynamicMeshInitialized() const;

	/** Clears everything and reinitialize the DM to use it properly */
	void InitializeDynamicMesh();

	EMaterialType& GetMaterialType(int32 MeshIndex);
	bool SetMaterialDirect(int32 MeshIndex, UMaterialInterface* NewMaterial);
	FAvaShapeMaterialUVParameters* GetMaterialUVParams(int32 MeshIndex);

	// called once the mesh is done updating
	virtual void OnMeshUpdateFinished();

	// called once the pixel size has changed, only in editor
	virtual void OnPixelSizeChanged() {}

	virtual void OnMeshChanged(int32 MeshIndex);
	virtual void OnVertexColorChanged();
	virtual void OnMaterialChanged(int32 MaterialIndex);
	virtual void OnMaterialTypeChanged(int32 MaterialIndex);
	virtual void OnParametricMaterialChanged(int32 MaterialIndex);
	virtual void OnUVParamsChanged(int32 MeshIndex);
	virtual void OnUsesPrimaryUVParamsChanged(int32 MeshIndex);

	virtual bool OnMeshSectionUpdated(FAvaShapeMesh& InMesh);

	// override this in child classes
	virtual void OnScaledSizeChanged() {}

	// override this in child classes, for 2D and 3D shape to update the scale
	virtual void OnSizeChanged() {}

	// Clears the mesh sections that are dirty
	virtual bool ClearMesh();

	// Adds vertex colors to the vertex colour array. Does not empty it.
	virtual bool CreateColors(FAvaShapeMesh& InMesh);

	// Performs a vertex-colour-only update. Then calls OnMeshUpdate to the PMC
	virtual bool UpdateColors();

	// Special case when the mesh is scaled, rather than reconfigured.
	virtual void UpdateVertices();

	// sets the uvs manually on a dynamic mesh section
	bool ApplyUVsManually(FAvaShapeMesh& InMesh);

	// apply a planar projection on a dynamic mesh section
	bool ApplyUVsPlanarProjection(FAvaShapeMesh& InMesh, FRotator PlaneRotation, FVector2D PlaneSize);

	// apply a box projection on a dynamic mesh section
	bool ApplyUVsBoxProjection(FAvaShapeMesh& InMesh, FRotator BoxRotation, FVector BoxSize);

	// override this in child classes when you create UV for a specific mesh
	virtual bool CreateUVs(FAvaShapeMesh& InMesh, FAvaShapeMaterialUVParameters& InParams);

	// override this in child classes to create the mesh if it's visible
	virtual bool CreateMesh(FAvaShapeMesh& InMesh) { return true; }

	// checks whether the topology of the mesh has changed (different vertices count and different triangles count than current)
	bool HasSameTopology(FAvaShapeMesh& InMesh);

	// Creates a section in the dynamic mesh component
	bool CreateDynamicMesh(FAvaShapeMesh& InMesh);

	// Updates a section in the dynamic mesh component
	bool UpdateDynamicMesh(FAvaShapeMesh& InMesh);

	// Invalidates a section and runs the mesh update if necessary
	void InvalidateSection(bool& bInvalidatedSection, bool bRequireUpdate = true, bool bDoAsync = true);

	/** Flag a mesh section dirty to regenerate it */
	void MarkMeshDirty(int32 MeshIndex);

	// Takes all dirty flags, regenerate whatever is needed and updates the DMC, called by RunUpdate or RequireUpdate
	void UpdateDynamicMesh();

	// runs the update for this mesh, async by default
	void RequireUpdate(bool bDoAsync = true);

	// used to check for material changes in slot of DMC to match with our current material settings
	bool CheckMaterialSlotChanges();

	// Changes the material on the shape component and keeps the MDI up to date.
	void SetShapeComponentMaterial(int32 InMaterialIndex, UMaterialInterface* InNewMaterial);

	/** Scale the vertices on all sections of the mesh */
	void ScaleVertices(const FVector& InScale);
	void ScaleVertices(const FVector2D& InScale);

	// Meshes used for the current shape sections
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Material", meta=(EditFixedOrder, EditFixedSize, DisplayAfter="bUsePrimaryMaterialEverywhere", AllowPrivateAccess = "true"))
	TMap<int32, FAvaShapeMeshData> MeshDatas;

#if WITH_EDITORONLY_DATA
	/** Stores select settings to allow restoration when Gizmo mode turned off */
	UPROPERTY()
	TMap<int32, FAvaShapeMeshData> NonGizmoMeshData;
#endif

	// enable mesh size property editing
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shape", meta=(DisplayPriority=1))
	bool bAllowEditSize;

	// the type of size you want to handle
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Shape", Transient, meta=(DisplayName="Size Type", DisplayPriority=1, EditCondition="bAllowEditSize", EditConditionHides, AllowPrivateAccess="true"))
	ESizeType SizeType = ESizeType::UnrealUnit;

	// Uniform scaled size of the mesh
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Transient, Setter, Getter, Category="Shape", meta=(DisplayPriority=1, ClampMin="0.0", DisplayName="Uniform Scaled Size", Units="times", AllowPrivateAccess="true"))
	float UniformScaledSize = 1.f;

	UPROPERTY(BlueprintReadWrite, AdvancedDisplay, Category="Shape", meta=(DisplayPriority=1, DisplayName="Material Vertex Color", AllowPrivateAccess="true"))
	FLinearColor VertexColor;

	// use primary material for every slot available
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetUsePrimaryMaterialEverywhere", Getter="GetUsePrimaryMaterialEverywhere", Category="Material", meta=(DisplayPriority=1, DisplayName="Use Single Material", AllowPrivateAccess="true"))
	bool bUsePrimaryMaterialEverywhere;

	TArray<FAvaSnapPoint> LocalSnapPoints;

private:
	/** find a registered mesh and gets a const pointer to it */
	const FAvaShapeMeshData* GetMeshData(int32 MeshIndex) const;

	FAvaShapeMesh* GetMesh(int32 MeshIndex);

	/** Update render state (materials) */
	void MarkMeshRenderStateDirty() const;

	/** Generates the dirty mesh sections */
	bool GenerateMesh();

	/** Generates the uvs for the mesh (all sections) */
	bool GenerateUV();

	/** Generates the tangents for the mesh (all sections) */
	bool GenerateTangents();

	/** Generates the normals for the mesh (all sections), needs the uv set on the mesh */
	bool GenerateNormals();

	/** runs the update for this mesh only if it's required (mesh is not up to date), async by default */
	void RunUpdate(bool bDoAsync = true);

	/** Restore the existing saved cached mesh on the component */
	void RestoreCachedMesh();

	/** Save the existing component mesh into a saved cached mesh */
	void SaveCachedMesh();

	/** Apply primary material everywhere */
	void OnUsePrimaryMaterialEverywhereChanged();

	// used to quickly restore mesh that has been modified after OnMeshUpdateFinished was called (eg extrude, masking)
	TOptional<UE::Geometry::FDynamicMesh3> CachedMesh;

	// only change this value from the base class to avoid thread lock, use RunUpdate() or RequireUpdate()
	std::atomic<EAvaDynamicMeshUpdateState> MeshUpdateStatus = EAvaDynamicMeshUpdateState::UpToDate;

	/** Do not access directly use getter to load it if nullptr */
	TWeakObjectPtr<UDynamicMeshComponent> CachedComponent;

	UPROPERTY()
	FVector MeshRegenWorldLocation;

	bool bHasNewMeshRegenWorldLocation;

	// flag to register meshes only once
	bool bMeshRegistered;

	// whether we are in the tool shape mode and creating this shape
	bool bInCreateMode;

	/** true if all meshes are marked as dirty, false otherwise, use MarkAllMeshesDirty() instead of modifying this */
	bool bAllMeshDirty;

	/** true if at least one meshes is marked as dirty, false otherwise, use MarkMeshDirty(MeshIndex) instead of modifying this */
	bool bAnyMeshDirty;

	// Should only be used when the shape's vertices have changed, but the overall shape has not changed.
	bool bVerticesDirty;

	bool bColorsDirty;
};
