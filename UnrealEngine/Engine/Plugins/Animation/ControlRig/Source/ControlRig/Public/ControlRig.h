// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "UObject/SparseDelegate.h"
#include "Engine/EngineBaseTypes.h"
#include "Templates/SubclassOf.h"
#include "ControlRigDefines.h"
#include "ControlRigGizmoLibrary.h"
#include "Rigs/RigHierarchy.h"
#include "Units/RigUnitContext.h"
#include "Animation/NodeMappingProviderInterface.h"
#include "Units/RigUnit.h"
#include "Units/Control/RigUnit_Control.h"
#include "RigVMCore/RigVM.h"
#include "RigVMHost.h"
#include "Components/SceneComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AttributesRuntime.h"
#include "Rigs/RigModuleDefines.h"

#if WITH_EDITOR
#include "RigVMModel/RigVMPin.h"
#include "RigVMModel/Nodes/RigVMUnitNode.h"
#include "RigVMTypeUtils.h"
#endif

#if WITH_EDITOR
#include "AnimPreviewInstance.h"
#endif 

#include "ControlRig.generated.h"

class IControlRigObjectBinding;
class UScriptStruct;
class USkeletalMesh;
class USkeletalMeshComponent;
class AActor;
class UTransformableControlHandle;

struct FReferenceSkeleton;
struct FRigUnit;
struct FRigControl;

CONTROLRIG_API DECLARE_LOG_CATEGORY_EXTERN(LogControlRig, Log, All);

/** Runs logic for mapping input data to transforms (the "Rig") */
UCLASS(Blueprintable, Abstract, editinlinenew)
class CONTROLRIG_API UControlRig : public URigVMHost, public INodeMappingProviderInterface, public IRigHierarchyProvider
{
	GENERATED_UCLASS_BODY()

	friend class UControlRigComponent;
	friend class SControlRigStackView;

public:

	/** Bindable event for external objects to contribute to / filter a control value */
	DECLARE_EVENT_ThreeParams(UControlRig, FFilterControlEvent, UControlRig*, FRigControlElement*, FRigControlValue&);

	/** Bindable event for external objects to be notified of Control changes */
	DECLARE_EVENT_ThreeParams(UControlRig, FControlModifiedEvent, UControlRig*, FRigControlElement*, const FRigControlModifiedContext&);

	/** Bindable event for external objects to be notified that a Control is Selected */
	DECLARE_EVENT_ThreeParams(UControlRig, FControlSelectedEvent, UControlRig*, FRigControlElement*, bool);

	/** Bindable event to manage undo / redo brackets in the client */
	DECLARE_EVENT_TwoParams(UControlRig, FControlUndoBracketEvent, UControlRig*, bool /* bOpen */);

	// To support Blueprints/scripting, we need a different delegate type (a 'Dynamic' delegate) which supports looser style UFunction binding (using names).
	DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_ThreeParams(FOnControlSelectedBP, UControlRig, OnControlSelected_BP, UControlRig*, Rig, const FRigControlElement&, Control, bool, bSelected);

	/** Bindable event to notify object binding change. */
	DECLARE_EVENT_OneParam(UControlRig, FControlRigBoundEvent, UControlRig*);

	static const FName OwnerComponent;

	UFUNCTION(BlueprintCallable, Category = ControlRig)
	static TArray<UControlRig*> FindControlRigs(UObject* Outer, TSubclassOf<UControlRig> OptionalClass);

public:
	virtual UWorld* GetWorld() const override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual UScriptStruct* GetPublicContextStruct() const override { return FControlRigExecuteContext::StaticStruct(); }

	// Returns the settings of the module this instance belongs to
	const FRigModuleSettings& GetRigModuleSettings() const;

	// Returns true if the rig is defined as a rig module
	bool IsRigModule() const;

	// Returns true if this rig is an instance module. Rigs may be a module but not instance
	// when being interacted with the asset editor
	bool IsRigModuleInstance() const;

	// Returns true if this rig is a modular rig (of class UModularRig)
	bool IsModularRig() const;

	// Returns true if this is a standalone rig (of class UControlRig and not modular)
	bool IsStandaloneRig() const;

	// Returns true if this is a native rig (implemented in C++)
	bool IsNativeRig() const;

	// Returns the parent rig hosting this module instance
	UControlRig* GetParentRig() const;

	// Returns the namespace of this module (for example ArmModule::)
	const FString& GetRigModuleNameSpace() const;

	// Returns the redirector from key to key for this rig
	virtual FRigElementKeyRedirector& GetElementKeyRedirector();
	virtual FRigElementKeyRedirector GetElementKeyRedirector() const { return ElementKeyRedirector; }
	
	// Returns the redirector from key to key for this rig
	virtual void SetElementKeyRedirector(const FRigElementKeyRedirector InElementRedirector);

	/** Creates a transformable control handle for the specified control to be used by the constraints system. Should use the UObject from 
	ConstraintsScriptingLibrary::GetManager(UWorld* InWorld)*/
	UFUNCTION(BlueprintCallable, Category = "Control Rig | Constraints")
	UTransformableControlHandle* CreateTransformableControlHandle(const FName& ControlName) const;


#if WITH_EDITOR
	/** Get the category of this ControlRig (for display in menus) */
	virtual FText GetCategory() const;

	/** Get the tooltip text to display for this node (displayed in graphs and from context menus) */
	virtual FText GetToolTipText() const;
#endif

	/** Initialize things for the ControlRig */
	virtual void Initialize(bool bInitRigUnits = true) override;

	/** Initialize the VM */
	virtual bool InitializeVM(const FName& InEventName) override;

	virtual void InitializeVMs(bool bInitRigUnits = true) { Super::Initialize(bInitRigUnits); }
	virtual bool InitializeVMs(const FName& InEventName) { return Super::InitializeVM(InEventName); }

	/** Evaluates the ControlRig */
	virtual void Evaluate_AnyThread() override;

	/** Ticks animation of the skeletal mesh component bound to this control rig */
	bool EvaluateSkeletalMeshComponent(double InDeltaTime);

	/** Removes any stored additive control values */
	void ResetControlValues();

	/** Resets the stored pose coming from the anim sequence.
	 * This usually indicates a new pose should be stored. */
	void ClearPoseBeforeBackwardsSolve();

	/* For additive rigs, will set control values by inverting the pose found after the backwards solve */
	/* Returns the array of control elements that were modified*/
	TArray<FRigControlElement*> InvertInputPose(const TArray<FRigElementKey>& InElements = TArray<FRigElementKey>(), EControlRigSetKey InSetKey = EControlRigSetKey::Never);

	/** Setup bindings to a runtime object (or clear by passing in nullptr). */
	void SetObjectBinding(TSharedPtr<IControlRigObjectBinding> InObjectBinding)
	{
		ObjectBinding = InObjectBinding;
		OnControlRigBound.Broadcast(this);
	}

	TSharedPtr<IControlRigObjectBinding> GetObjectBinding() const
	{
		return ObjectBinding;
	}

	/** Find the actor the rig is bound to, if any */
	UFUNCTION(BlueprintPure, Category = "Control Rig")
	AActor* GetHostingActor() const;

	UFUNCTION(BlueprintPure, Category = "Control Rig")
	URigHierarchy* GetHierarchy()
	{
		return DynamicHierarchy;
	}
	
	virtual URigHierarchy* GetHierarchy() const override
	{
		return DynamicHierarchy;
	}

#if WITH_EDITOR

	// called after post reinstance when compilng blueprint by Sequencer
	void PostReinstanceCallback(const UControlRig* Old);

	// resets the recorded transform changes
	void ResetRecordedTransforms(const FName& InEventName);

#endif // WITH_EDITOR
	
	// BEGIN UObject interface
	virtual void BeginDestroy() override;
	// END UObject interface

	UPROPERTY(transient)
	ERigExecutionType ExecutionType;

	UPROPERTY()
	FRigHierarchySettings HierarchySettings;

	virtual bool Execute(const FName& InEventName) override;
	virtual bool Execute_Internal(const FName& InEventName) override;
	virtual void RequestInit() override;
	virtual void RequestInitVMs()  { Super::RequestInit(); }
	virtual bool SupportsEvent(const FName& InEventName) const override { return Super::SupportsEvent(InEventName); }
	virtual const TArray<FName>& GetSupportedEvents() const override{ return Super::GetSupportedEvents(); }

	template<class T>
	bool SupportsEvent() const
	{
		return SupportsEvent(T::EventName);
	}

	bool AllConnectorsAreResolved(FString* OutFailureReason = nullptr, FRigElementKey* OutConnector = nullptr) const;

	/** Requests to perform construction during the next execution */
	UFUNCTION(BlueprintCallable, Category = "Control Rig")
	void RequestConstruction();

	bool IsConstructionRequired() const;

	/** Contains a backwards solve event */
	UFUNCTION(BlueprintCallable, Category = "Control Rig")
	bool SupportsBackwardsSolve() const;

	virtual void AdaptEventQueueForEvaluate(TArray<FName>& InOutEventQueueToRun) override;

	/** INodeMappingInterface implementation */
	virtual void GetMappableNodeData(TArray<FName>& OutNames, TArray<FNodeItem>& OutNodeItems) const override;

	/** Data Source Registry Getter */
	UAnimationDataSourceRegistry* GetDataSourceRegistry();

	virtual TArray<FRigControlElement*> AvailableControls() const;
	virtual FRigControlElement* FindControl(const FName& InControlName) const;
	virtual bool ShouldApplyLimits() const { return !IsConstructionModeEnabled(); }
	virtual bool IsConstructionModeEnabled() const;
	virtual FTransform SetupControlFromGlobalTransform(const FName& InControlName, const FTransform& InGlobalTransform);
	virtual FTransform GetControlGlobalTransform(const FName& InControlName) const;

	// Sets the relative value of a Control
	template<class T>
	void SetControlValue(const FName& InControlName, T InValue, bool bNotify = true,
		const FRigControlModifiedContext& Context = FRigControlModifiedContext(), bool bSetupUndo = true, bool bPrintPythonCommnds = false, bool bFixEulerFlips = false)
	{
		SetControlValueImpl(InControlName, FRigControlValue::Make<T>(InValue), bNotify, Context, bSetupUndo, bPrintPythonCommnds, bFixEulerFlips);
	}

	// Returns the value of a Control
	FRigControlValue GetControlValue(const FName& InControlName) const
	{
		const FRigElementKey Key(InControlName, ERigElementType::Control);
		if (FRigBaseElement* Element = DynamicHierarchy->Find(Key))
		{
			if (FRigControlElement* ControlElement = Cast<FRigControlElement>(Element))
			{
				return GetControlValue(ControlElement, ERigControlValueType::Current);
			}
		}
		return DynamicHierarchy->GetControlValue(Key);
	}

	FRigControlValue GetControlValue(FRigControlElement* InControl, const ERigControlValueType& InValueType) const;

	// Sets the relative value of a Control
	virtual void SetControlValueImpl(const FName& InControlName, const FRigControlValue& InValue, bool bNotify = true,
		const FRigControlModifiedContext& Context = FRigControlModifiedContext(), bool bSetupUndo = true, bool bPrintPythonCommnds = false, bool bFixEulerFlips = false);

	void SwitchToParent(const FRigElementKey& InElementKey, const FRigElementKey& InNewParentKey, bool bInitial, bool bAffectChildren);

	FTransform GetInitialLocalTransform(const FRigElementKey &InKey)
	{
		if (bIsAdditive)
		{
			// The initial value of all additive controls is always Identity
			return FTransform::Identity;
		}
		return GetHierarchy()->GetInitialLocalTransform(InKey);
	}

	bool SetControlGlobalTransform(const FName& InControlName, const FTransform& InGlobalTransform, bool bNotify = true, const FRigControlModifiedContext& Context = FRigControlModifiedContext(), bool bSetupUndo = true, bool bPrintPythonCommands = false, bool bFixEulerFlips = false);

	virtual FRigControlValue GetControlValueFromGlobalTransform(const FName& InControlName, const FTransform& InGlobalTransform, ERigTransformType::Type InTransformType);

	virtual void SetControlLocalTransform(const FName& InControlName, const FTransform& InLocalTransform, bool bNotify = true, const FRigControlModifiedContext& Context = FRigControlModifiedContext(), bool bSetupUndo = true, bool bFixEulerFlips = false);
	virtual FTransform GetControlLocalTransform(const FName& InControlName) ;

	FVector GetControlSpecifiedEulerAngle(const FRigControlElement* InControlElement, bool bIsInitial = false) const;

	virtual const TArray<TSoftObjectPtr<UControlRigShapeLibrary>>& GetShapeLibraries() const;
	virtual void CreateRigControlsForCurveContainer();
	virtual void GetControlsInOrder(TArray<FRigControlElement*>& SortedControls) const;

	UFUNCTION(BlueprintCallable, Category = "Control Rig")
	virtual void SelectControl(const FName& InControlName, bool bSelect = true);
	UFUNCTION(BlueprintCallable, Category = "Control Rig")
	virtual bool ClearControlSelection();
	UFUNCTION(BlueprintPure, Category = "Control Rig")
	virtual TArray<FName> CurrentControlSelection() const;
	UFUNCTION(BlueprintPure, Category = "Control Rig")
	virtual bool IsControlSelected(const FName& InControlName)const;

	// Returns true if this manipulatable subject is currently
	// available for manipulation / is enabled.
	virtual bool ManipulationEnabled() const
	{
		return bManipulationEnabled;
	}

	// Sets the manipulatable subject to enabled or disabled
	virtual bool SetManipulationEnabled(bool Enabled = true)
	{
		if (bManipulationEnabled == Enabled)
		{
			return false;
		}
		bManipulationEnabled = Enabled;
		return true;
	}

	// Returns a event that can be used to subscribe to
	// filtering control data when needed
	FFilterControlEvent& ControlFilter() { return OnFilterControl; }

	// Returns a event that can be used to subscribe to
	// change notifications coming from the manipulated subject.
	FControlModifiedEvent& ControlModified() { return OnControlModified; }

	// Returns a event that can be used to subscribe to
	// selection changes coming from the manipulated subject.
	FControlSelectedEvent& ControlSelected() { return OnControlSelected; }

	// Returns an event that can be used to subscribe to
	// Undo Bracket requests such as Open and Close.
	FControlUndoBracketEvent & ControlUndoBracket() { return OnControlUndoBracket; }
	
	FControlRigBoundEvent& ControlRigBound() { return OnControlRigBound; };

	bool IsCurveControl(const FRigControlElement* InControlElement) const;

	DECLARE_EVENT_TwoParams(UControlRig, FControlRigExecuteEvent, class UControlRig*, const FName&);
#if WITH_EDITOR
	FControlRigExecuteEvent& OnPreConstructionForUI_AnyThread() { return PreConstructionForUIEvent; }
#endif
	FControlRigExecuteEvent& OnPreConstruction_AnyThread() { return PreConstructionEvent; }
	FControlRigExecuteEvent& OnPostConstruction_AnyThread() { return PostConstructionEvent; }
	FControlRigExecuteEvent& OnPreForwardsSolve_AnyThread() { return PreForwardsSolveEvent; }
	FControlRigExecuteEvent& OnPostForwardsSolve_AnyThread() { return PostForwardsSolveEvent; }
	FControlRigExecuteEvent& OnPreAdditiveValuesApplication_AnyThread() { return PreAdditiveValuesApplicationEvent; }
	FRigEventDelegate& OnRigEvent_AnyThread() { return RigEventDelegate; }

	// Setup the initial transform / ref pose of the bones based upon an anim instance
	// This uses the current refpose instead of the RefSkeleton pose.
	virtual void SetBoneInitialTransformsFromAnimInstance(UAnimInstance* InAnimInstance);

	// Setup the initial transform / ref pose of the bones based upon an anim instance proxy
	// This uses the current refpose instead of the RefSkeleton pose.
	virtual void SetBoneInitialTransformsFromAnimInstanceProxy(const FAnimInstanceProxy* InAnimInstanceProxy);

	// Setup the initial transform / ref pose of the bones based upon skeletal mesh component (ref skeleton)
	// This uses the RefSkeleton pose instead of the current refpose (or vice versae is bUseAnimInstance == true)
	virtual void SetBoneInitialTransformsFromSkeletalMeshComponent(USkeletalMeshComponent* InSkelMeshComp, bool bUseAnimInstance = false);

	// Setup the initial transforms / ref pose of the bones based on a skeletal mesh
	// This uses the RefSkeleton pose instead of the current refpose.
	virtual void SetBoneInitialTransformsFromSkeletalMesh(USkeletalMesh* InSkeletalMesh);

	// Setup the initial transforms / ref pose of the bones based on a reference skeleton
	// This uses the RefSkeleton pose instead of the current refpose.
	virtual void SetBoneInitialTransformsFromRefSkeleton(const FReferenceSkeleton& InReferenceSkeleton);

private:

	void SetBoneInitialTransformsFromCompactPose(FCompactPose* InCompactPose);

public:
	
	const FRigControlElementCustomization* GetControlCustomization(const FRigElementKey& InControl) const;
	void SetControlCustomization(const FRigElementKey& InControl, const FRigControlElementCustomization& InCustomization);

	virtual void PostInitInstanceIfRequired() override;
#if WITH_EDITORONLY_DATA
	static void DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass);
#endif

	virtual USceneComponent* GetOwningSceneComponent() override;

	void SetDynamicHierarchy(TObjectPtr<URigHierarchy> InHierarchy);

protected:

	virtual void PostInitInstance(URigVMHost* InCDO) override;

	UPROPERTY()
	TMap<FRigElementKey, FRigControlElementCustomization> ControlCustomizations;

	UPROPERTY()
	TObjectPtr<URigHierarchy> DynamicHierarchy;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TSoftObjectPtr<UControlRigShapeLibrary> GizmoLibrary_DEPRECATED;
#endif

	UPROPERTY()
	TArray<TSoftObjectPtr<UControlRigShapeLibrary>> ShapeLibraries;

	UPROPERTY(transient)
	TMap<FString, FString> ShapeLibraryNameMap;

	/** Runtime object binding */
	TSharedPtr<IControlRigObjectBinding> ObjectBinding;

#if WITH_EDITORONLY_DATA
	// you either go Input or Output, currently if you put it in both place, Output will override
	UPROPERTY()
	TMap<FName, FCachedPropertyPath> InputProperties_DEPRECATED;

	UPROPERTY()
	TMap<FName, FCachedPropertyPath> OutputProperties_DEPRECATED;
#endif

private:
	
	void HandleOnControlModified(UControlRig* Subject, FRigControlElement* Control, const FRigControlModifiedContext& Context);

public:
	
	class CONTROLRIG_API FAnimAttributeContainerPtrScope
	{
	public:
		FAnimAttributeContainerPtrScope(UControlRig* InControlRig, UE::Anim::FStackAttributeContainer& InExternalContainer);
		~FAnimAttributeContainerPtrScope();

		UControlRig* ControlRig;
	};
	
private:
	UPROPERTY(Transient)
	FRigVMExtendedExecuteContext RigVMExtendedExecuteContext;

	UE::Anim::FStackAttributeContainer* ExternalAnimAttributeContainer;

#if WITH_EDITOR
	void SetEnableAnimAttributeTrace(bool bInEnable)
	{
		bEnableAnimAttributeTrace = bInEnable;
	};
	
	bool bEnableAnimAttributeTrace;
	
	UE::Anim::FHeapAttributeContainer InputAnimAttributeSnapshot;
	UE::Anim::FHeapAttributeContainer OutputAnimAttributeSnapshot;
#endif
	
	/** The registry to access data source */
	UPROPERTY(Transient)
	TObjectPtr<UAnimationDataSourceRegistry> DataSourceRegistry;

	/** Broadcasts a notification when launching the construction event */
	FControlRigExecuteEvent PreConstructionForUIEvent;

	/** Broadcasts a notification just before the controlrig is setup. */
	FControlRigExecuteEvent PreConstructionEvent;

	/** Broadcasts a notification whenever the controlrig has been setup. */
	FControlRigExecuteEvent PostConstructionEvent;

	/** Broadcasts a notification before a forward solve has been initiated */
	FControlRigExecuteEvent PreForwardsSolveEvent;
	
	/** Broadcasts a notification after a forward solve has been initiated */
	FControlRigExecuteEvent PostForwardsSolveEvent;

	/** Broadcasts a notification before additive controls have been applied */
	FControlRigExecuteEvent PreAdditiveValuesApplicationEvent;

	/** Handle changes within the hierarchy */
	void HandleHierarchyModified(ERigHierarchyNotification InNotification, URigHierarchy* InHierarchy, const FRigBaseElement* InElement);

protected:
	
	virtual void RunPostConstructionEvent();

private:
#if WITH_EDITOR
	/** Add a transient / temporary control used to interact with a node */
	FName AddTransientControl(const URigVMUnitNode* InNode, const FRigDirectManipulationTarget& InTarget);

	/** Sets the value of a transient control based on a node */
	bool SetTransientControlValue(const URigVMUnitNode* InNode, TSharedPtr<FRigDirectManipulationInfo> InInfo);

	/** Remove a transient / temporary control used to interact with a node */
	FName RemoveTransientControl(const URigVMUnitNode* InNode, const FRigDirectManipulationTarget& InTarget);

	FName AddTransientControl(const FRigElementKey& InElement);

	/** Sets the value of a transient control based on a bone */
	bool SetTransientControlValue(const FRigElementKey& InElement);

	/** Remove a transient / temporary control used to interact with a bone */
	FName RemoveTransientControl(const FRigElementKey& InElement);

	static FName GetNameForTransientControl(const FRigElementKey& InElement);
	FName GetNameForTransientControl(const URigVMUnitNode* InNode, const FRigDirectManipulationTarget& InTarget) const;
	static FString GetNodeNameFromTransientControl(const FRigElementKey& InKey);
	static FString GetTargetFromTransientControl(const FRigElementKey& InKey);
	TSharedPtr<FRigDirectManipulationInfo> GetRigUnitManipulationInfoForTransientControl(const FRigElementKey& InKey);
	
	static FRigElementKey GetElementKeyFromTransientControl(const FRigElementKey& InKey);
	bool CanAddTransientControl(const URigVMUnitNode* InNode, const FRigDirectManipulationTarget& InTarget, FString* OutFailureReason);

	/** Removes all  transient / temporary control used to interact with pins */
	void ClearTransientControls();

	UAnimPreviewInstance* PreviewInstance;

	// this is needed because PreviewInstance->ModifyBone(...) cannot modify user created bones,
	TMap<FName, FTransform> TransformOverrideForUserCreatedBones;
	
public:
	
	void ApplyTransformOverrideForUserCreatedBones();
	void ApplySelectionPoseForConstructionMode(const FName& InEventName);
	
#endif

protected:

	void HandleHierarchyEvent(URigHierarchy* InHierarchy, const FRigEventContext& InEvent);
	FRigEventDelegate RigEventDelegate;

	void RestoreShapeLibrariesFromCDO();
	void OnAddShapeLibrary(const FControlRigExecuteContext* InContext, const FString& InLibraryName, UControlRigShapeLibrary* InShapeLibrary, bool bLogResults);
	bool OnShapeExists(const FName& InShapeName) const;
	virtual void InitializeVMsFromCDO() { Super::InitializeFromCDO(); }
	virtual void InitializeFromCDO() override;


	UPROPERTY()
	FRigInfluenceMapPerEvent Influences;

	const FRigInfluenceMap* FindInfluenceMap(const FName& InEventName);


	FRigElementKeyRedirector ElementKeyRedirector;

public:

	// UObject interface
#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	float GetDebugBoneRadiusMultiplier() const { return DebugBoneRadiusMultiplier; }
	static FRigUnit* GetRigUnitInstanceFromScope(TSharedPtr<FStructOnScope> InScope);

public:
	//~ Begin IInterface_AssetUserData Interface
	virtual const TArray<UAssetUserData*>* GetAssetUserDataArray() const override;
	//~ End IInterface_AssetUserData Interface
protected:
	mutable TArray<TObjectPtr<UAssetUserData>> CombinedAssetUserData;

	UPROPERTY(Transient, DuplicateTransient)
	mutable TMap<FName, TObjectPtr<UDataAssetLink>> ExternalVariableDataAssetLinks;

	DECLARE_DELEGATE_RetVal(TArray<TObjectPtr<UAssetUserData>>, FGetExternalAssetUserData);
	FGetExternalAssetUserData GetExternalAssetUserDataDelegate;

private:

	void CopyPoseFromOtherRig(UControlRig* Subject);

protected:
	bool bCopyHierarchyBeforeConstruction;
	bool bResetInitialTransformsBeforeConstruction;
	bool bResetCurrentTransformsAfterConstruction;
	bool bManipulationEnabled;

	int32 PreConstructionBracket;
	int32 PostConstructionBracket;
	int32 PreForwardsSolveBracket;
	int32 PostForwardsSolveBracket;
	int32 PreAdditiveValuesApplicationBracket;
	int32 InteractionBracket;
	int32 InterRigSyncBracket;
	int32 ControlUndoBracketIndex;
	uint8 InteractionType;
	TArray<FRigElementKey> ElementsBeingInteracted;
#if WITH_EDITOR
	TArray<TSharedPtr<FRigDirectManipulationInfo>> RigUnitManipulationInfos;
#endif
	bool bInteractionJustBegan;

	TWeakObjectPtr<USceneComponent> OuterSceneComponent;

	bool IsRunningPreConstruction() const
	{
		return PreConstructionBracket > 0;
	}

	bool IsRunningPostConstruction() const
	{
		return PostConstructionBracket > 0;
	}

	bool IsInteracting() const
	{
		return InteractionBracket > 0;
	}

	uint8 GetInteractionType() const
	{
		return InteractionType;
	}

	bool IsSyncingWithOtherRig() const
	{
		return InterRigSyncBracket > 0;
	}


#if WITH_EDITOR
	static void OnHierarchyTransformUndoRedoWeak(URigHierarchy* InHierarchy, const FRigElementKey& InKey, ERigTransformType::Type InTransformType, const FTransform& InTransform, bool bIsUndo, TWeakObjectPtr<UControlRig> WeakThis)
	{
		if(WeakThis.IsValid() && InHierarchy != nullptr)
		{
			WeakThis->OnHierarchyTransformUndoRedo(InHierarchy, InKey, InTransformType, InTransform, bIsUndo);
		}
	}
#endif
	
	void OnHierarchyTransformUndoRedo(URigHierarchy* InHierarchy, const FRigElementKey& InKey, ERigTransformType::Type InTransformType, const FTransform& InTransform, bool bIsUndo);

	FFilterControlEvent OnFilterControl;
	FControlModifiedEvent OnControlModified;
	FControlSelectedEvent OnControlSelected;
	FControlUndoBracketEvent OnControlUndoBracket;
	FControlRigBoundEvent OnControlRigBound;

	UPROPERTY(BlueprintAssignable, Category = ControlRig, meta=(DisplayName="OnControlSelected"))
	FOnControlSelectedBP OnControlSelected_BP;

	TArray<FRigElementKey> QueuedModifiedControls;

private:

#if WITH_EDITORONLY_DATA

	/** Whether controls are visible */
	UPROPERTY(transient)
	bool bControlsVisible;

#endif


protected:
	/** An additive control rig runs a backwards solve before applying additive control values
	 * and running the forward solve
	 */
	UPROPERTY()
	bool bIsAdditive;

	struct FRigSetControlValueInfo
	{
		FRigControlValue Value;
		bool bNotify;
		FRigControlModifiedContext Context;
		bool bSetupUndo;
		bool bPrintPythonCommnds;
		bool bFixEulerFlips;
	};
	struct FRigSwitchParentInfo
	{
		FRigElementKey NewParent;
		bool bInitial;
		bool bAffectChildren;
	};
	FRigPose PoseBeforeBackwardsSolve;
	FRigPose ControlsAfterBackwardsSolve;
	TMap<FRigElementKey, FRigSetControlValueInfo> ControlValues; // Layered Rigs: Additive values in local space (to add after backwards solve)
	TMap<FRigElementKey, FRigSwitchParentInfo> SwitchParentValues; // Layered Rigs: Parent switching values to perform after backwards solve


private:
	float DebugBoneRadiusMultiplier;
	
public:
	
#if WITH_EDITOR	

	void ToggleControlsVisible() { bControlsVisible = !bControlsVisible; }
	void SetControlsVisible(const bool bIsVisible) { bControlsVisible = bIsVisible; }
	bool GetControlsVisible()const { return bControlsVisible;}

#endif
	
	virtual bool IsAdditive() const { return bIsAdditive; }
	void SetIsAdditive(const bool bInIsAdditive)
	{
		bIsAdditive = bInIsAdditive;
		if (URigHierarchy* Hierarchy = GetHierarchy())
		{
			Hierarchy->bUsePreferredEulerAngles = !bIsAdditive;
		}
	}

private:

	// Class used to temporarily cache current pose of the hierarchy
	// restore it on destruction, similar to UControlRigBlueprint::FControlValueScope
	class FPoseScope
	{
	public:
		FPoseScope(UControlRig* InControlRig, ERigElementType InFilter = ERigElementType::All,
			const TArray<FRigElementKey>& InElements = TArray<FRigElementKey>(), const ERigTransformType::Type InTransformType = ERigTransformType::CurrentLocal);
		~FPoseScope();

	private:

		UControlRig* ControlRig;
		ERigElementType Filter;
		FRigPose CachedPose;
		ERigTransformType::Type TransformType;
	};

	UPROPERTY()
	FRigModuleSettings RigModuleSettings;

	UPROPERTY(transient)
	mutable FString RigModuleNameSpace;


public:

#if WITH_EDITOR

	// Class used to temporarily cache current transient controls
	// restore them after a CopyHierarchy call
	class FTransientControlScope
	{
	public:
		FTransientControlScope(TObjectPtr<URigHierarchy> InHierarchy);
		~FTransientControlScope();
	
	private:
		// used to match URigHierarchyController::AddControl(...)
		struct FTransientControlInfo
		{
			FName Name;
			// transient control should only have 1 parent, with weight = 1.0
			FRigElementKey Parent;
			FRigControlSettings Settings;
			FRigControlValue Value;
			FTransform OffsetTransform;
			FTransform ShapeTransform;
		};
		
		TArray<FTransientControlInfo> SavedTransientControls;
		TObjectPtr<URigHierarchy> Hierarchy;
	};

	// Class used to temporarily cache current pose of transient controls
	// restore them after a ResetPoseToInitial call,
	// which allows user to move bones in construction mode
	class FTransientControlPoseScope
	{
	public:
		FTransientControlPoseScope(TObjectPtr<UControlRig> InControlRig)
		{
			ControlRig = InControlRig;

			TArray<FRigControlElement*> TransientControls = ControlRig->GetHierarchy()->GetTransientControls();
			TArray<FRigElementKey> Keys;
			for(FRigControlElement* TransientControl : TransientControls)
			{
				Keys.Add(TransientControl->GetKey());
			}

			if(Keys.Num() > 0)
			{
				CachedPose = ControlRig->GetHierarchy()->GetPose(false, ERigElementType::Control, TArrayView<FRigElementKey>(Keys));
			}
		}
		~FTransientControlPoseScope()
		{
			check(ControlRig);

			if(CachedPose.Num() > 0)
			{
				ControlRig->GetHierarchy()->SetPose(CachedPose);
			}
		}
	
	private:
		
		UControlRig* ControlRig;
		FRigPose CachedPose;	
	};	

	bool bRecordSelectionPoseForConstructionMode;
	TMap<FRigElementKey, FTransform> SelectionPoseForConstructionMode;
	bool bIsClearingTransientControls;

	FRigPose InputPoseOnDebuggedRig;
	
#endif

public:
	UE_DEPRECATED(5.4, "InteractionRig is no longer used") UFUNCTION(BlueprintGetter, meta = (DeprecatedFunction, DeprecationMessage = "InteractionRig is no longer used"))
	UControlRig* GetInteractionRig() const
	{
#if WITH_EDITORONLY_DATA
		return InteractionRig_DEPRECATED;
#else
		return nullptr;
#endif
	}

	UE_DEPRECATED(5.4, "InteractionRig is no longer used")
	UFUNCTION(BlueprintSetter, meta = (DeprecatedFunction, DeprecationMessage = "InteractionRig is no longer used"))
	void SetInteractionRig(UControlRig* InInteractionRig) {}

	UE_DEPRECATED(5.4, "InteractionRig is no longer used")
	UFUNCTION(BlueprintGetter, meta = (DeprecatedFunction, DeprecationMessage = "InteractionRig is no longer used"))
	TSubclassOf<UControlRig> GetInteractionRigClass() const
	{
#if WITH_EDITORONLY_DATA
		return InteractionRigClass_DEPRECATED;
#else
		return nullptr;
#endif
	}

	UE_DEPRECATED(5.4, "InteractionRig is no longer used")
	UFUNCTION(BlueprintSetter, meta = (DeprecatedFunction, DeprecationMessage = "InteractionRig is no longer used"))
	void SetInteractionRigClass(TSubclassOf<UControlRig> InInteractionRigClass) {}

	uint32 GetShapeLibraryHash() const;
	
private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UControlRig> InteractionRig_DEPRECATED;

	UPROPERTY()
	TSubclassOf<UControlRig> InteractionRigClass_DEPRECATED;
#endif

	friend class FControlRigBlueprintCompilerContext;
	friend struct FRigHierarchyRef;
	friend class FControlRigEditor;
	friend class SRigCurveContainer;
	friend class SRigHierarchy;
	friend class SControlRigAnimAttributeView;
	friend class UEngineTestControlRig;
 	friend class FControlRigEditMode;
	friend class UControlRigBlueprint;
	friend class UControlRigComponent;
	friend class UControlRigBlueprintGeneratedClass;
	friend class FControlRigInteractionScope;
	friend class UControlRigValidator;
	friend struct FAnimNode_ControlRig;
	friend struct FAnimNode_ControlRigBase;
	friend class URigHierarchy;
	friend class UFKControlRig;
	friend class UControlRigGraph;
	friend class AControlRigControlActor;
	friend class AControlRigShapeActor;
	friend class FRigTransformElementDetails;
	friend class FControlRigEditorModule;
	friend class UModularRig;
};

class CONTROLRIG_API FControlRigBracketScope
{
public:

	FControlRigBracketScope(int32& InBracket)
		: Bracket(InBracket)
	{
		Bracket++;
	}

	~FControlRigBracketScope()
	{
		Bracket--;
	}

private:

	int32& Bracket;
};

class CONTROLRIG_API FControlRigInteractionScope
{
public:

	FControlRigInteractionScope(UControlRig* InControlRig)
		: ControlRig(InControlRig)
		, InteractionBracketScope(InControlRig->InteractionBracket)
		, SyncBracketScope(InControlRig->InterRigSyncBracket)
	{
		InControlRig->GetHierarchy()->StartInteraction();
	}

	FControlRigInteractionScope(
		UControlRig* InControlRig,
		const FRigElementKey& InKey,
		EControlRigInteractionType InInteractionType = EControlRigInteractionType::All
	)
		: ControlRig(InControlRig)
		, InteractionBracketScope(InControlRig->InteractionBracket)
		, SyncBracketScope(InControlRig->InterRigSyncBracket)
	{
		InControlRig->ElementsBeingInteracted = { InKey };
		InControlRig->InteractionType = (uint8)InInteractionType;
		InControlRig->bInteractionJustBegan = true;
		InControlRig->GetHierarchy()->StartInteraction();
	}

	FControlRigInteractionScope(
		UControlRig* InControlRig,
		const TArray<FRigElementKey>& InKeys,
		EControlRigInteractionType InInteractionType = EControlRigInteractionType::All
	)
		: ControlRig(InControlRig)
		, InteractionBracketScope(InControlRig->InteractionBracket)
	, SyncBracketScope(InControlRig->InterRigSyncBracket)
	{
		InControlRig->ElementsBeingInteracted = InKeys;
		InControlRig->InteractionType = (uint8)InInteractionType;
		InControlRig->bInteractionJustBegan = true;
		InControlRig->GetHierarchy()->StartInteraction();
	}

	~FControlRigInteractionScope()
	{
		if(ensure(ControlRig.IsValid()))
		{
			ControlRig->GetHierarchy()->EndInteraction();
			ControlRig->InteractionType = (uint8)EControlRigInteractionType::None;
			ControlRig->bInteractionJustBegan = false;
			ControlRig->ElementsBeingInteracted.Reset();
		}
	}

private:

	TWeakObjectPtr<UControlRig> ControlRig;
	FControlRigBracketScope InteractionBracketScope;
	FControlRigBracketScope SyncBracketScope;
};
