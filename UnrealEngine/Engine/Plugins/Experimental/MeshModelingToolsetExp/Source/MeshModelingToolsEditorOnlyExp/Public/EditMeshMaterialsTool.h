// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshSelectionTool.h"
#include "EditMeshMaterialsTool.generated.h"


/**
 *
 */
UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API UEditMeshMaterialsToolBuilder : public UMeshSelectionToolBuilder
{
	GENERATED_BODY()

public:
	virtual UMeshSurfacePointTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
};






UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API UEditMeshMaterialsToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** */
	UPROPERTY(EditAnywhere, Category = Materials, meta = (TransientToolProperty, DisplayName = "Active Material", GetOptions = GetMaterialNamesFunc, NoResetToDefault))
	FString ActiveMaterial;

	UFUNCTION()
	const TArray<FString>& GetMaterialNamesFunc() { return MaterialNamesList; }

	UPROPERTY(meta = (TransientToolProperty))
	TArray<FString> MaterialNamesList;

	void UpdateFromMaterialsList();
	int32 GetSelectedMaterialIndex() const;

	UPROPERTY(EditAnywhere, Category=Materials)
	TArray<TObjectPtr<UMaterialInterface>> Materials;
};





UENUM()
enum class EEditMeshMaterialsToolActions
{
	NoAction,
	AssignMaterial
};



UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API UEditMeshMaterialsEditActions : public UMeshSelectionToolActionPropertySet
{
	GENERATED_BODY()

public:
	UFUNCTION(CallInEditor, Category = MaterialEdits, meta = (DisplayName = "Assign Active Material", DisplayPriority = 1))
	void AssignActiveMaterial()
	{
		PostMaterialAction(EEditMeshMaterialsToolActions::AssignMaterial);
	}

	void PostMaterialAction(EEditMeshMaterialsToolActions Action);
};






/**
 *
 */
UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API UEditMeshMaterialsTool : public UMeshSelectionTool
{
	GENERATED_BODY()

public:
	virtual void Setup() override;
	virtual void RegisterActions(FInteractiveToolActionSet& ActionSet) override;
	virtual void OnTick(float DeltaTime) override;

	virtual bool CanAccept() const override;

	void RequestMaterialAction(EEditMeshMaterialsToolActions ActionType);

protected:
	UPROPERTY()
	TObjectPtr<UEditMeshMaterialsToolProperties> MaterialProps;

	virtual UMeshSelectionToolActionPropertySet* CreateEditActions() override;
	virtual void AddSubclassPropertySets() override;

	bool bHavePendingSubAction = false;
	EEditMeshMaterialsToolActions PendingSubAction = EEditMeshMaterialsToolActions::NoAction;

	void ApplyMaterialAction(EEditMeshMaterialsToolActions ActionType);
	void AssignMaterialToSelectedTriangles();

	TArray<UMaterialInterface*> CurrentMaterials;
	void OnMaterialSetChanged();

	struct FMaterialSetKey
	{
		TArray<void*> Values;
		bool operator!=(const FMaterialSetKey& Key2) const;
	};
	FMaterialSetKey GetMaterialKey();

	FMaterialSetKey InitialMaterialKey;
	bool bHaveModifiedMaterials = false;
	bool bShowingMaterialSetError = false;
	bool bShowingNotEnoughMaterialsError = false;

	virtual void ApplyShutdownAction(EToolShutdownType ShutdownType) override;

	void ExternalUpdateMaterialSet(const TArray<UMaterialInterface*>& NewMaterialSet);
	friend class FEditMeshMaterials_MaterialSetChange;

private:

	// @return the max material ID used in the PreviewMesh
	int32 FindMaxActiveMaterialID() const;

	// Clamp material IDs to be less than the number of materials
	bool FixInvalidMaterialIDs();

	// Update error messages regarding whether we have enough materials on the current mesh
	void UpdateMaterialSetErrors();

	int32 MaterialSetWatchIndex = -1;
};




/**
 */
class MESHMODELINGTOOLSEDITORONLYEXP_API FEditMeshMaterials_MaterialSetChange : public FToolCommandChange
{
public:
	TArray<UMaterialInterface*> MaterialsBefore;
	TArray<UMaterialInterface*> MaterialsAfter;

	/** Makes the change to the object */
	virtual void Apply(UObject* Object) override;

	/** Reverts change to the object */
	virtual void Revert(UObject* Object) override;

	/** Describes this change (for debugging) */
	virtual FString ToString() const override;
};

