// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ControlRigBlueprintGeneratedClass.h"
#include "UObject/ObjectMacros.h"
#include "Engine/Blueprint.h"
#include "Engine/Texture2D.h"
#include "ControlRigDefines.h"
#include "Rigs/RigHierarchyContainer.h"
#include "Rigs/RigHierarchy.h"
#include "Interfaces/Interface_PreviewMeshProvider.h"
#include "ControlRigGizmoLibrary.h"
#include "ControlRigSchema.h"
#include "RigVMCore/RigVMStatistics.h"
#include "RigVMModel/RigVMClient.h"
#include "ControlRigValidationPass.h"
#include "RigVMBlueprint.h"
#include "Rigs/RigModuleDefines.h"
#include "ModularRigModel.h"

#if WITH_EDITOR
#include "Kismet2/CompilerResultsLog.h"
#endif

#include "ControlRigBlueprint.generated.h"

class URigVMBlueprintGeneratedClass;
class USkeletalMesh;
class UControlRigGraph;
struct FEndLoadPackageContext;

UENUM(BlueprintType)
enum class EControlRigType : uint8
{
	IndependentRig = 0,
	RigModule = 1,
	ModularRig =2,
};


UCLASS(BlueprintType, meta=(IgnoreClassThumbnail))
class CONTROLRIGDEVELOPER_API UControlRigBlueprint : public URigVMBlueprint, public IInterface_PreviewMeshProvider, public IRigHierarchyProvider
{
	GENERATED_UCLASS_BODY()

public:
	UControlRigBlueprint();

	// URigVMBlueprint interface
	virtual UClass* GetRigVMBlueprintGeneratedClassPrototype() const override { return UControlRigBlueprintGeneratedClass::StaticClass(); }
	virtual UClass* GetRigVMSchemaClass() const override { return UControlRigSchema::StaticClass(); }
	virtual UScriptStruct* GetRigVMExecuteContextStruct() const override { return FControlRigExecuteContext::StaticStruct(); }
	virtual UClass* GetRigVMEdGraphClass() const override;
	virtual UClass* GetRigVMEdGraphNodeClass() const override;
	virtual UClass* GetRigVMEdGraphSchemaClass() const override;
	virtual TArray<FString> GeneratePythonCommands(const FString InNewBlueprintName) override;
	virtual UClass* GetRigVMEditorSettingsClass() const override;
	virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;
#if WITH_EDITOR
	virtual const FName& GetPanelPinFactoryName() const override;
	static const FName ControlRigPanelNodeFactoryName;
	virtual IRigVMEditorModule* GetEditorModule() const override;
#endif

	virtual void Serialize(FArchive& Ar) override;

#if WITH_EDITOR

	// UBlueprint interface
	virtual UClass* RegenerateClass(UClass* ClassToRegenerate, UObject* PreviousCDO) override;
	virtual bool SupportedByDefaultBlueprintFactory() const override { return false; }
	virtual bool IsValidForBytecodeOnlyRecompile() const override { return false; }
	virtual void GetTypeActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void GetInstanceActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
	virtual void PostLoad() override;
	virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PostRename(UObject* OldOuter, const FName OldName) override;
	virtual bool RequiresForceLoadMembers(UObject* InObject) const override;

	virtual bool SupportsGlobalVariables() const override { return true; }
	virtual bool SupportsLocalVariables() const override { return !IsModularRig(); }
	virtual bool SupportsFunctions() const override { return !IsModularRig(); }
	virtual bool SupportsEventGraphs() const override { return !IsModularRig(); }


	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;

#endif	// #if WITH_EDITOR

	UFUNCTION(BlueprintCallable, Category = "VM")
	UClass* GetControlRigClass() const;

	bool IsModularRig() const;

	UFUNCTION(BlueprintCallable, Category = "Control Rig Blueprint")
	UControlRig* CreateControlRig() { return Cast<UControlRig>(CreateRigVMHost()); }

	UFUNCTION(BlueprintCallable, Category = "Control Rig Blueprint")
	UControlRig* GetDebuggedControlRig() { return Cast<UControlRig>(GetDebuggedRigVMHost()); } 

	/** IInterface_PreviewMeshProvider interface */
	UFUNCTION(BlueprintCallable, Category = "Control Rig Blueprint")
	virtual void SetPreviewMesh(USkeletalMesh* PreviewMesh, bool bMarkAsDirty = true) override;
	
	UFUNCTION(BlueprintCallable, Category = "Control Rig Blueprint")
	virtual USkeletalMesh* GetPreviewMesh() const override;

	UFUNCTION(BlueprintPure, Category = "Control Rig Blueprint")
	bool IsControlRigModule() const;

#if WITH_EDITORONLY_DATA
	
	bool CanTurnIntoControlRigModule_Blueprint(bool InAutoConvertHierarchy = false) const { return CanTurnIntoControlRigModule(InAutoConvertHierarchy); }

	bool CanTurnIntoControlRigModule(bool InAutoConvertHierarchy, FString* OutErrorMessage = nullptr) const;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "TurnIntoControlRigModule", ScriptName = "TurnIntoControlRigModule"), Category = "Control Rig Blueprint")
	bool TurnIntoControlRigModule_Blueprint() { return TurnIntoControlRigModule(); }

	bool TurnIntoControlRigModule(bool InAutoConvertHierarchy = false, FString* OutErrorMessage = nullptr);

	UFUNCTION(BlueprintPure, meta = (DisplayName = "CanTurnIntoStandaloneRig", ScriptName = "CanTurnIntoStandaloneRig"), Category = "Control Rig Blueprint")
	bool CanTurnIntoStandaloneRig_Blueprint() const { return CanTurnIntoStandaloneRig(); }

	bool CanTurnIntoStandaloneRig(FString* OutErrorMessage = nullptr) const;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "TurnIntoStandaloneRig", ScriptName = "TurnIntoStandaloneRig"), Category = "Control Rig Blueprint")
	bool TurnIntoStandaloneRig_Blueprint() { return TurnIntoStandaloneRig(); }

	bool TurnIntoStandaloneRig(FString* OutErrorMessage = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Control Rig Blueprint")
	TArray<URigVMNode*> ConvertHierarchyElementsToSpawnerNodes(URigHierarchy* InHierarchy, TArray<FRigElementKey> InKeys, bool bRemoveElements = true);

#endif // WITH_EDITORONLY_DATA

	UFUNCTION(BlueprintPure, Category = "Control Rig Blueprint")
	UTexture2D* GetRigModuleIcon() const;

	DECLARE_EVENT_OneParam(UControlRigBlueprint, FOnRigTypeChanged, UControlRigBlueprint*);

	FOnRigTypeChanged& OnRigTypeChanged() { return OnRigTypeChangedDelegate; }

	/// IRigHierarchyProvider interface
	virtual URigHierarchy* GetHierarchy() const override
	{
		return Hierarchy;
	}

	UPROPERTY(EditAnywhere, Category = "Modular Rig")
	FModularRigSettings ModularRigSettings;

	UPROPERTY(EditAnywhere, Category = "Hierarchy")
	FRigHierarchySettings HierarchySettings;

	UPROPERTY(EditAnywhere, Category = "Hierarchy", AssetRegistrySearchable)
	FRigModuleSettings RigModuleSettings;

	UPROPERTY()
	TMap<FRigElementKey, FRigElementKey> ConnectionMap;

protected:

	FOnRigTypeChanged OnRigTypeChangedDelegate;
	
	void UpdateExposedModuleConnectors() const;

	bool ResolveConnector(const FRigElementKey& DraggedKey, const FRigElementKey& TargetKey, bool bSetupUndoRedo = true);

	void UpdateConnectionMapFromModel();

	/** Asset searchable information about exposed public functions on this rig */
	UPROPERTY(AssetRegistrySearchable)
	TArray<FRigVMOldPublicFunctionData> PublicFunctions_DEPRECATED;

	virtual void SetupDefaultObjectDuringCompilation(URigVMHost* InCDO) override;

public:

	virtual void SetupPinRedirectorsForBackwardsCompatibility() override;

	UFUNCTION(BlueprintCallable, Category = "VM")
	static TArray<UControlRigBlueprint*> GetCurrentlyOpenRigBlueprints();

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TSoftObjectPtr<UControlRigShapeLibrary> GizmoLibrary_DEPRECATED;

	UPROPERTY(EditAnywhere, Category = Shapes)
	TArray<TSoftObjectPtr<UControlRigShapeLibrary>> ShapeLibraries;

	const FControlRigShapeDefinition* GetControlShapeByName(const FName& InName) const;

	UPROPERTY(transient, DuplicateTransient, meta = (DisplayName = "VM Statistics", DisplayAfter = "VMCompileSettings"))
	FRigVMStatistics Statistics_DEPRECATED;
#endif

	UPROPERTY(EditAnywhere, Category = "Drawing")
	FRigVMDrawContainer DrawContainer;

#if WITH_EDITOR
	/** Remove a transient / temporary control used to interact with a pin */
	FName AddTransientControl(const URigVMUnitNode* InNode, const FRigDirectManipulationTarget& InTarget);

	/** Remove a transient / temporary control used to interact with a pin */
	FName RemoveTransientControl(const URigVMUnitNode* InNode, const FRigDirectManipulationTarget& InTarget);

	/** Remove a transient / temporary control used to interact with a bone */
	FName AddTransientControl(const FRigElementKey& InElement);

	/** Remove a transient / temporary control used to interact with a bone */
	FName RemoveTransientControl(const FRigElementKey& InElement);

	/** Removes all  transient / temporary control used to interact with pins */
	void ClearTransientControls();

#endif

	UPROPERTY(EditAnywhere, Category = "Influence Map")
	FRigInfluenceMapPerEvent Influences;

public:

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FRigHierarchyContainer HierarchyContainer_DEPRECATED;
#endif

	UPROPERTY(BlueprintReadOnly, Category = "Hierarchy")
	TObjectPtr<URigHierarchy> Hierarchy;

	UFUNCTION(BlueprintCallable, Category = "Hierarchy")
	URigHierarchyController* GetHierarchyController() { return Hierarchy->GetController(true); }

	UPROPERTY(BlueprintReadOnly, Category = "Modules")
	FModularRigModel ModularRigModel;

	UFUNCTION(BlueprintCallable, Category = "Modules")
	UModularRigController* GetModularRigController();

	UFUNCTION(BlueprintCallable, Category = "Control Rig Blueprint")
	void RecompileModularRig();

	UPROPERTY(AssetRegistrySearchable)
	EControlRigType ControlRigType;

	UPROPERTY(AssetRegistrySearchable)
	FName ItemTypeDisplayName = TEXT("Control Rig");

private:

	/** Whether or not this rig has an Inversion Event */
	UPROPERTY(AssetRegistrySearchable)
	bool bSupportsInversion;

	/** Whether or not this rig has Controls on It */
	UPROPERTY(AssetRegistrySearchable)
	bool bSupportsControls;

	/** The default skeletal mesh to use when previewing this asset */
#if WITH_EDITORONLY_DATA
	UPROPERTY(AssetRegistrySearchable)
	TSoftObjectPtr<USkeletalMesh> PreviewSkeletalMesh;
#endif

	/** The skeleton from import into a hierarchy */
	UPROPERTY(DuplicateTransient, AssetRegistrySearchable, EditAnywhere, Category="Control Rig Blueprint")
	TSoftObjectPtr<UObject> SourceHierarchyImport;

	/** The skeleton from import into a curve */
	UPROPERTY(DuplicateTransient, AssetRegistrySearchable, EditAnywhere, Category="Control Rig Blueprint")
	TSoftObjectPtr<UObject> SourceCurveImport;

	/** If set to true, this control rig has animatable controls */
	UPROPERTY(AssetRegistrySearchable)
	bool bExposesAnimatableControls;

private:

	static TArray<UControlRigBlueprint*> sCurrentlyOpenedRigBlueprints;

	virtual void PathDomainSpecificContentOnLoad() override;
	virtual void PatchFunctionsOnLoad() override;
	void PatchRigElementKeyCacheOnLoad();
	void PatchPropagateToChildren();

protected:
	virtual void CreateMemberVariablesOnLoad() override;
	virtual void PatchVariableNodesOnLoad() override;

public:
	void UpdateElementKeyRedirector(UControlRig* InControlRig) const;
	void PropagatePoseFromInstanceToBP(UControlRig* InControlRig) const;
	void PropagatePoseFromBPToInstances() const;
	void PropagateHierarchyFromBPToInstances() const;
	void PropagateDrawInstructionsFromBPToInstances() const;
	void PropagatePropertyFromBPToInstances(FRigElementKey InRigElement, const FProperty* InProperty) const;
	void PropagatePropertyFromInstanceToBP(FRigElementKey InRigElement, const FProperty* InProperty, UControlRig* InInstance) const;
	void PropagateModuleHierarchyFromBPToInstances() const;
	void UpdateModularDependencyDelegates();
	void OnModularDependencyVMCompiled(UObject* InBlueprint, URigVM* InVM, FRigVMExtendedExecuteContext& InExecuteContext);
	void OnModularDependencyChanged(URigVMBlueprint* InBlueprint);
	void RequestConstructionOnAllModules();
	void RefreshModuleVariables();
	void RefreshModuleVariables(const FRigModuleReference* InModule);
	void RefreshModuleConnectors();
	void RefreshModuleConnectors(const FRigModuleReference* InModule);

	/**
	* Returns the modified event, which can be used to 
	* subscribe to topological changes happening within the hierarchy. The event is broadcast only after all hierarchy instances are up to date
	* @return The event used for subscription.
	*/
	FRigHierarchyModifiedEvent& OnHierarchyModified() { return HierarchyModifiedEvent; }

	FOnRigVMRefreshEditorEvent& OnModularRigPreCompiled() { return ModularRigPreCompiled; }
	FOnRigVMRefreshEditorEvent& OnModularRigCompiled() { return ModularRigCompiled; }

private:

	UPROPERTY()
	TObjectPtr<UControlRigValidator> Validator;

	FRigHierarchyModifiedEvent	HierarchyModifiedEvent;
	FOnRigVMRefreshEditorEvent ModularRigPreCompiled;
	FOnRigVMRefreshEditorEvent ModularRigCompiled;

	UPROPERTY(transient, DuplicateTransient)
	int32 ModulesRecompilationBracket = 0;


	void HandleHierarchyModified(ERigHierarchyNotification InNotification, URigHierarchy* InHierarchy, const FRigBaseElement* InElement);

	void HandleRigModulesModified(EModularRigNotification InNotification, const FRigModuleReference* InModule);

#if WITH_EDITOR
	virtual void HandlePackageDone() override;
	virtual void HandleConfigureRigVMController(const FRigVMClient* InClient, URigVMController* InControllerToConfigure) override;
#endif

	// Class used to temporarily cache all 
	// current control values and reapply them
	// on destruction
	class CONTROLRIGDEVELOPER_API FControlValueScope
	{
	public: 
		FControlValueScope(UControlRigBlueprint* InBlueprint);
		~FControlValueScope();

	private:

		UControlRigBlueprint* Blueprint;
		TMap<FName, FRigControlValue> ControlValues;
	};

	UPROPERTY()
	float DebugBoneRadius;

#if WITH_EDITOR
		
public:

	/** Shape libraries to load during package load completed */ 
	TArray<FString> ShapeLibrariesToLoadOnPackageLoaded;

#endif

private:

	friend class FControlRigBlueprintCompilerContext;
	friend class SRigHierarchy;
	friend class SRigCurveContainer;
	friend class FControlRigEditor;
	friend class UEngineTestControlRig;
	friend class FControlRigEditMode;
	friend class FControlRigBlueprintActions;
	friend class FControlRigDrawContainerDetails;
	friend class UDefaultControlRigManipulationLayer;
	friend struct FRigValidationTabSummoner;
	friend class UAnimGraphNode_ControlRig;
	friend class UControlRigThumbnailRenderer;
	friend class FControlRigGraphDetails;
	friend class FControlRigEditorModule;
	friend class UControlRigComponent;
	friend struct FControlRigGraphSchemaAction_PromoteToVariable;
	friend class UControlRigGraphSchema;
	friend class FControlRigBlueprintDetails;
};
