// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "UObject/WeakObjectPtr.h"
#include "IControlRigEditorModule.h"
#include "IControlRigModule.h"
#include "IAnimationEditor.h"
#include "IAnimationEditorModule.h"

class UBlueprint;
class IAssetTypeActions;
class UMaterial;
class UAnimSequence;
class USkeletalMesh;
class USkeleton;
class FToolBarBuilder;
class FExtender;
class FUICommandList;
class UMovieSceneTrack;
class FControlRigGraphPanelNodeFactory;
class FControlRigGraphPanelPinFactory;

class FControlRigEditorModule : public IControlRigEditorModule
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** IControlRigEditorModule interface */
	virtual TSharedRef<IControlRigEditor> CreateControlRigEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UControlRigBlueprint* Blueprint) override;
	DECLARE_DELEGATE_RetVal_TwoParams(TSharedRef<FExtender>, FControlRigEditorToolbarExtender, const TSharedRef<FUICommandList> /*InCommandList*/, TSharedRef<IControlRigEditor> /*InControlRigEditor*/);
	virtual TArray<FControlRigEditorToolbarExtender>& GetAllControlRigEditorToolbarExtenders() override { return ControlRigEditorToolbarExtenders; }
	/** IHasMenuExtensibility interface */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() override { return MenuExtensibilityManager; }

	/** IHasToolBarExtensibility interface */
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() override { return ToolBarExtensibilityManager; }
	
	/** Animation Toolbar Extender*/
	void AddControlRigExtenderToToolMenu(FName InToolMenuName);
	TSharedRef< SWidget > GenerateAnimationMenu(TWeakPtr<IAnimationEditor> InAnimationEditor);
	void ToggleIsDrivenByLevelSequence(UAnimSequence* AnimSequence)const;
	bool IsDrivenByLevelSequence(UAnimSequence* AnimSequence)const;
	void EditWithFKControlRig(UAnimSequence* AnimSequence, USkeletalMesh* SkelMesh, USkeleton* InSkeleton);
	void BakeToControlRig(UClass* InClass,UAnimSequence* AnimSequence, USkeletalMesh* SkelMesh,USkeleton* InSkeleton);
	static void OpenLevelSequence(UAnimSequence* AnimSequence);
	static void UnLinkLevelSequence(UAnimSequence* AnimSequence);
	void ExtendAnimSequenceMenu();

	virtual void GetTypeActions(UControlRigBlueprint* CRB, FBlueprintActionDatabaseRegistrar& ActionRegistrar) override;
	virtual void GetInstanceActions(UControlRigBlueprint* CRB, FBlueprintActionDatabaseRegistrar& ActionRegistrar) override;
	virtual FConnectionDrawingPolicy* CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, class UEdGraph* InGraphObj) override;
	virtual void GetContextMenuActions(const UControlRigGraphSchema* Schema, class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;

	/* FStructureEditorUtils::INotifyOnStructChanged Interface, used to respond to changes to user defined structs */
	virtual void PreChange(const UUserDefinedStruct* Changed, FStructureEditorUtils::EStructureEditorChangeInfo ChangedType) override;
	virtual void PostChange(const UUserDefinedStruct* Changed, FStructureEditorUtils::EStructureEditorChangeInfo ChangedType) override;
private:
	/** Handle a new animation controller blueprint being created */
	void HandleNewBlueprintCreated(UBlueprint* InBlueprint);

	/** Handle for our sequencer control rig parameter track editor */
	FDelegateHandle ControlRigParameterTrackCreateEditorHandle;

	TArray<TSharedRef<IAssetTypeActions>> RegisteredAssetTypeActions;

	/** StaticClass is not safe on shutdown, so we cache the name, and use this to unregister on shut down */
	TArray<FName> ClassesToUnregisterOnShutdown;
	TArray<FName> PropertiesToUnregisterOnShutdown;

	/** Extensibility managers */
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;
	TArray<FControlRigEditorToolbarExtender> ControlRigEditorToolbarExtenders;

	/** Node factory for the control rig graph */
	TSharedPtr<FControlRigGraphPanelNodeFactory> ControlRigGraphPanelNodeFactory;

	/** Pin factory for the control rig graph */
	TSharedPtr<FControlRigGraphPanelPinFactory> ControlRigGraphPanelPinFactory;

	/** Delegate handles for blueprint utils */
	FDelegateHandle RefreshAllNodesDelegateHandle;
	FDelegateHandle ReconstructAllNodesDelegateHandle;
	FDelegateHandle BlueprintVariableCustomizationHandle;

	/** Param to hold Filter Result to pass to Filter*/
	bool bFilterAssetBySkeleton;

	/** Handles for all registered workflows */
	TArray<int32> WorkflowHandles;
};