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
class FRigVMEdGraphPanelNodeFactory;
class FControlRigGraphPanelPinFactory;

class FControlRigEditorModule : public IControlRigEditorModule
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** FRigVMEditorModule interface */
	virtual UClass* GetRigVMBlueprintClass() const override;
	virtual void GetNodeContextMenuActions(URigVMBlueprint* RigVMBlueprint, const URigVMEdGraphNode* EdGraphNode, URigVMNode* ModelNode, UToolMenu* Menu) const override;
	virtual void GetPinContextMenuActions(URigVMBlueprint* RigVMBlueprint, const UEdGraphPin* EdGraphPin, URigVMPin* ModelPin, UToolMenu* Menu) const override;

	/** IControlRigEditorModule interface */
	virtual TSharedRef<IControlRigEditor> CreateControlRigEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UControlRigBlueprint* Blueprint) override;
	
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

private:

	/** Handle for our sequencer control rig parameter track editor */
	FDelegateHandle ControlRigParameterTrackCreateEditorHandle;

	TArray<TSharedRef<IAssetTypeActions>> RegisteredAssetTypeActions;

	/** StaticClass is not safe on shutdown, so we cache the name, and use this to unregister on shut down */
	TArray<FName> ClassesToUnregisterOnShutdown;
	TArray<FName> PropertiesToUnregisterOnShutdown;
	
	/** Param to hold Filter Result to pass to Filter*/
	bool bFilterAssetBySkeleton;

	/** Handles for all registered workflows */
	TArray<int32> WorkflowHandles;
};