// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "UVEditorToolAnalyticsUtils.h"

#include "UVEditorLayerEditTool.generated.h"

class UUVEditorToolMeshInput;
class UUVEditorChannelEditTool;
class UUVToolEmitChangeAPI;

UCLASS()
class UVEDITORTOOLS_API UUVEditorChannelEditToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;

	// This is a pointer so that it can be updated under the builder without
	// having to set it in the mode after initializing targets.
	const TArray<TObjectPtr<UUVEditorToolMeshInput>>* Targets = nullptr;
};

UENUM()
enum class EChannelEditToolAction
{
	NoAction,

	Add,
	Copy,
	Delete
};


UCLASS()
class UVEDITORTOOLS_API UUVEditorChannelEditSettings : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** UV Layer Edit action to preform */
	UPROPERTY(EditAnywhere, Category = Options, meta = (InvalidEnumValues = "NoAction"))
	EChannelEditToolAction Action = EChannelEditToolAction::Add;
};

UCLASS()
class UVEDITORTOOLS_API UUVEditorChannelEditTargetProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = "UVChannels", meta = (DisplayName = "Asset", GetOptions = GetAssetNames, EditCondition = "bActionNeedsAsset", EditConditionHides = true, HideEditConditionToggle = true))
	FString Asset;

	UFUNCTION()
	const TArray<FString>& GetAssetNames();

	UPROPERTY(EditAnywhere, Category = "UVChannels", meta = (DisplayName = "Target UV Channel", GetOptions = GetUVChannelNames, EditCondition = "bActionNeedsTarget", EditConditionHides = true, HideEditConditionToggle = true))
	FString TargetChannel;

	UPROPERTY(EditAnywhere, Category = "UVChannels", meta = (DisplayName = "Source UV Channel", GetOptions = GetUVChannelNames, EditCondition = "bActionNeedsReference", EditConditionHides = true, HideEditConditionToggle = true))
	FString ReferenceChannel;

	UFUNCTION()
	const TArray<FString>& GetUVChannelNames();

	TArray<FString> UVChannelNames;
	TArray<FString> UVAssetNames;

	// 1:1 with UVAssetNames
	TArray<int32> NumUVChannelsPerAsset;

	UPROPERTY(meta = (TransientToolProperty))
	bool bActionNeedsAsset = true;

	UPROPERTY(meta = (TransientToolProperty))
	bool bActionNeedsReference = false;

	UPROPERTY(meta = (TransientToolProperty))
	bool bActionNeedsTarget = true;

public:
	void Initialize(
		const TArray<TObjectPtr<UUVEditorToolMeshInput>>& TargetsIn,
		bool bInitializeSelection);

	void SetUsageFlags(EChannelEditToolAction Action);

	/**
	 * Verify that the UV asset selection is valid
	 * @param bUpdateIfInvalid if selection is not valid, reset UVAsset to Asset0 or empty if no assets exist
	 * @return true if selection in UVAsset is an entry in UVAssetNames.
	 */
	bool ValidateUVAssetSelection(bool bUpdateIfInvalid);

	/**
	 * Verify that the UV channel selection is valid
	 * @param bUpdateIfInvalid if selection is not valid, UVChannel to UV0 or empty if no UV channels exist
	 * @return true if selection in UVChannel is an entry in UVChannelNamesList.
	 */
	bool ValidateUVChannelSelection(bool bUpdateIfInvalid);


	/**
	 * @return selected UV asset ID, or -1 if invalid selection
	 */
	int32 GetSelectedAssetID();

	/**
	 * @param bForceToZeroOnFailure if true, then instead of returning -1 we return 0 so calling code can fallback to default UV paths
	 * @param bUseReference if true, get the selected reference channel index, otherwise return the target channel's index.
	 * @return selected UV channel index, or -1 if invalid selection, or 0 if invalid selection and bool bForceToZeroOnFailure = true
	 */
	int32 GetSelectedChannelIndex(bool bForceToZeroOnFailure = false, bool bUseReference = false);
};

UCLASS()
class UVEDITORTOOLS_API UUVEditorChannelEditAddProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/* Placeholder for future per action settings */
};

UCLASS()
class UVEDITORTOOLS_API UUVEditorChannelEditCopyProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/* Placeholder for future per action settings */
};

UCLASS()
class UVEDITORTOOLS_API UUVEditorChannelEditDeleteProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/* Placeholder for future per action settings */
};

UCLASS()
class UVEDITORTOOLS_API UUVEditorChannelEditToolActionPropertySet : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<UUVEditorChannelEditTool> ParentTool;

	void Initialize(UUVEditorChannelEditTool* ParentToolIn) { ParentTool = ParentToolIn; }
	void PostAction(EChannelEditToolAction Action);
	
	UFUNCTION(CallInEditor, Category = Actions)
	void Apply();
};

/**
 *
 */
UCLASS()
class UVEDITORTOOLS_API UUVEditorChannelEditTool : public UInteractiveTool
{
	GENERATED_BODY()

public:

	/**
	 * The tool will operate on the meshes given here.
	 */
	virtual void SetTargets(const TArray<TObjectPtr<UUVEditorToolMeshInput>>& TargetsIn)
	{
		Targets = TargetsIn;
	}

	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;

	virtual void OnTick(float DeltaTime) override;

	virtual bool HasCancel() const override { return false; }
	virtual bool HasAccept() const override { return false; }
	virtual bool CanAccept() const override { return false; }

	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

	void RequestAction(EChannelEditToolAction ActionType);
	EChannelEditToolAction ActiveAction() const;


protected:
	void ApplyVisbleChannelChange();
	void UpdateChannelSelectionProperties(int32 ChangingAsset);

	void AddChannel();
	void CopyChannel();
	void DeleteChannel();
	int32 ActiveAsset;
	int32 ActiveChannel;
	int32 ReferenceChannel;

	EChannelEditToolAction PendingAction = EChannelEditToolAction::NoAction;

	UPROPERTY()
	TArray<TObjectPtr<UUVEditorToolMeshInput>> Targets;

	UPROPERTY()
	TObjectPtr<UUVEditorChannelEditToolActionPropertySet> ToolActions = nullptr;

	UPROPERTY()
	TObjectPtr<UUVEditorChannelEditSettings> ActionSelectionProperties = nullptr;

	UPROPERTY()
	TObjectPtr<UUVEditorChannelEditTargetProperties> SourceChannelProperties = nullptr;

	UPROPERTY()
	TObjectPtr<UUVEditorChannelEditAddProperties> AddActionProperties = nullptr;

	UPROPERTY()
	TObjectPtr<UUVEditorChannelEditCopyProperties> CopyActionProperties = nullptr;

	UPROPERTY()
	TObjectPtr<UUVEditorChannelEditDeleteProperties> DeleteActionProperties = nullptr;

	UPROPERTY()
	TObjectPtr<UUVToolEmitChangeAPI> EmitChangeAPI = nullptr;

	//
	// Analytics
	//

	struct FActionHistoryItem
	{
		FDateTime Timestamp;
		EChannelEditToolAction ActionType = EChannelEditToolAction::NoAction;

		// if ActionType == Add    then FirstOperandIndex is the index of the added UV layer
		// if ActionType == Delete then FirstOperandIndex is the index of the deleted UV layer
		// if ActionType == Copy   then FirstOperandIndex is the index of the source UV layer
		int32 FirstOperandIndex = -1;
		
		// if ActionType == Add    then SecondOperandIndex is unused
		// if ActionType == Delete then SecondOperandIndex is unused
		// if ActionType == Copy   then SecondOperandIndex is the index of the target UV layer
		int32 SecondOperandIndex = -1;

		bool bDeleteActionWasActuallyClear = false;
	};
	
	TArray<FActionHistoryItem> AnalyticsActionHistory;
	UE::Geometry::UVEditorAnalytics::FTargetAnalytics InputTargetAnalytics;
	FDateTime ToolStartTimeAnalytics;
	void RecordAnalytics();
};
