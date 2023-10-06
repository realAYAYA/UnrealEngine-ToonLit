// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRig.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "Misc/RuntimeErrors.h"
#include "IControlRigObjectBinding.h"
#include "HelperUtil.h"
#include "ObjectTrace.h"
#include "RigVMBlueprintGeneratedClass.h"
#include "ControlRigObjectVersion.h"
#include "Rigs/RigHierarchyController.h"
#include "Units/Execution/RigUnit_BeginExecution.h"
#include "Units/Execution/RigUnit_PrepareForExecution.h"
#include "Units/Execution/RigUnit_InverseExecution.h"
#include "Units/Execution/RigUnit_InteractionExecution.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimationPoseData.h"
#include "Internationalization/TextKey.h"
#include "UObject/Package.h"
#if WITH_EDITOR
#include "ControlRigModule.h"
#include "Modules/ModuleManager.h"
#include "RigVMModel/RigVMNode.h"
#include "Engine/Blueprint.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "RigVMDeveloperTypeUtils.h"
#include "Editor.h"
#endif// WITH_EDITOR
#include "ControlRigComponent.h"
#include "Constraints/ControlRigTransformableHandle.h"
#include "RigVMCore/RigVMNativized.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRig)

#define LOCTEXT_NAMESPACE "ControlRig"

DEFINE_LOG_CATEGORY(LogControlRig);

DECLARE_STATS_GROUP(TEXT("ControlRig"), STATGROUP_ControlRig, STATCAT_Advanced);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Control Rig Execution"), STAT_RigExecution, STATGROUP_ControlRig, );
DEFINE_STAT(STAT_RigExecution);

const FName UControlRig::OwnerComponent("OwnerComponent");

//CVar to specify if we should create a float control for each curve in the curve container
//By default we don't but it may be useful to do so for debugging
static TAutoConsoleVariable<int32> CVarControlRigCreateFloatControlsForCurves(
	TEXT("ControlRig.CreateFloatControlsForCurves"),
	0,
	TEXT("If nonzero we create a float control for each curve in the curve container, useful for debugging low level controls."),
	ECVF_Default);

//CVar to specify if we should allow debug drawing in game (during shipped game or PIE)
//By default we don't for performance 
static TAutoConsoleVariable<float> CVarControlRigEnableDrawInterfaceInGame(
	TEXT("ControlRig.EnableDrawInterfaceInGame"),
	0,
	TEXT("If nonzero debug drawing will be enabled during play."),
	ECVF_Default);

UControlRig::UControlRig(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if WITH_EDITOR
	, bEnableAnimAttributeTrace(false)
#endif 
	, DataSourceRegistry(nullptr)
#if WITH_EDITOR
	, PreviewInstance(nullptr)
#endif
	, bCopyHierarchyBeforeConstruction(true)
	, bResetInitialTransformsBeforeConstruction(true)
	, bManipulationEnabled(false)
	, PreConstructionBracket(0)
	, PostConstructionBracket(0)
	, InteractionBracket(0)
	, InterRigSyncBracket(0)
	, ControlUndoBracketIndex(0)
	, InteractionType((uint8)EControlRigInteractionType::None)
	, bInteractionJustBegan(false)
	, DebugBoneRadiusMultiplier(1.f)
#if WITH_EDITOR
	, bRecordSelectionPoseForConstructionMode(true)
	, bIsClearingTransientControls(false)
#endif
{
	EventQueue.Add(FRigUnit_BeginExecution::EventName);
}

void UControlRig::BeginDestroy()
{
	Super::BeginDestroy();
	PreConstructionEvent.Clear();
	PostConstructionEvent.Clear();
	PreForwardsSolveEvent.Clear();
	PostForwardsSolveEvent.Clear();
	SetInteractionRig(nullptr);

#if WITH_EDITOR
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		if(UControlRig* CDO = GetClass()->GetDefaultObject<UControlRig>())
		{
			if (!CDO->HasAnyFlags(RF_BeginDestroyed))
			{
				if (CDO->GetHierarchy())
				{
					CDO->GetHierarchy()->UnregisterListeningHierarchy(GetHierarchy());
				}
			}
		}
	}
#endif

	TRACE_OBJECT_LIFETIME_END(this);
}

UWorld* UControlRig::GetWorld() const
{
	if (ObjectBinding.IsValid())
	{
		AActor* HostingActor = ObjectBinding->GetHostingActor();
		if (HostingActor)
		{
			return HostingActor->GetWorld();
		}

		UObject* Owner = ObjectBinding->GetBoundObject();
		if (Owner)
		{
			return Owner->GetWorld();
		}
	}
	return Super::GetWorld();
}

void UControlRig::Initialize(bool bRequestInit)
{
	TRACE_OBJECT_LIFETIME_BEGIN(this);

	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ControlRig_Initialize);

	Super::Initialize(bRequestInit);

	// Create the data source registry here to avoid UObject creation from Non-Game Threads
	GetDataSourceRegistry();

	// Create the Hierarchy Controller here to avoid UObject creation from Non-Game Threads
	GetHierarchy()->GetController(true);
	
	// should refresh mapping 
	RequestConstruction();
	
	GetHierarchy()->OnModified().RemoveAll(this);
	GetHierarchy()->OnModified().AddUObject(this, &UControlRig::HandleHierarchyModified);
	GetHierarchy()->OnEventReceived().RemoveAll(this);
	GetHierarchy()->OnEventReceived().AddUObject(this, &UControlRig::HandleHierarchyEvent);
	GetHierarchy()->UpdateVisibilityOnProxyControls();
}

void UControlRig::OnAddShapeLibrary(const FControlRigExecuteContext* InContext, const FString& InLibraryName, UControlRigShapeLibrary* InShapeLibrary, bool bReplaceExisting, bool bLogResults)
{
	// don't ever change the CDO
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	if(InShapeLibrary == nullptr)
	{
		if(InContext)
		{
			static constexpr TCHAR Message[] = TEXT("No shape library provided.");
			InContext->Log(EMessageSeverity::Error, Message);
		}
		return;
	}

	// remove the shape library in case it was there before
	ShapeLibraryNameMap.Remove(InShapeLibrary->GetName());
	ShapeLibraries.Remove(InShapeLibrary);

	// if we've removed all shape libraries - let's add the ones from the CDO back
	if (ShapeLibraries.IsEmpty())
	{
		UControlRig* CDO = GetClass()->GetDefaultObject<UControlRig>();
		ShapeLibraries = CDO->ShapeLibraries;
	}

	// if we are supposed to replace the library and the library name is empty
	FString LibraryName = InLibraryName;
	if(LibraryName.IsEmpty() && bReplaceExisting)
	{
		if(ShapeLibraries.Num() == 1)
		{
			LibraryName = ShapeLibraries[0]->GetName();
		}
	}

	if(LibraryName.IsEmpty())
	{
		LibraryName = InShapeLibrary->GetName();
	}

	if(LibraryName != InShapeLibrary->GetName())
	{
		ShapeLibraryNameMap.FindOrAdd(InShapeLibrary->GetName()) = LibraryName;
	}

	if(bReplaceExisting)
	{
		for(int32 Index = 0; Index < ShapeLibraries.Num(); Index++)
		{
			const TSoftObjectPtr<UControlRigShapeLibrary>& ExistingShapeLibrary = ShapeLibraries[Index];
			if(ExistingShapeLibrary.IsNull())
			{
				continue;
			}
			FString ExistingName = ExistingShapeLibrary->GetName();
			if (FString* MapName = ShapeLibraryNameMap.Find(ExistingName))
			{
				ExistingName = *MapName;
			}
			if(ExistingName.Equals(LibraryName, ESearchCase::IgnoreCase))
			{
				ShapeLibraryNameMap.Remove(ExistingShapeLibrary->GetName());
				ShapeLibraries[Index] = InShapeLibrary;
				break;
			}
		}
	}

	ShapeLibraries.AddUnique(InShapeLibrary);

#if WITH_EDITOR
	if(bLogResults)
	{
		static constexpr TCHAR MapFormat[] = TEXT("Control Rig '%s': Shape Library Name Map: '%s' -> '%s'");
		static constexpr TCHAR LibraryFormat[] = TEXT("Control Rig '%s': Shape Library '%s' uses asset '%s'");
		static constexpr TCHAR DefaultShapeFormat[] = TEXT("Control Rig '%s': Shape Library '%s' has default shape '%s'");
		static constexpr TCHAR ShapeFormat[] = TEXT("Control Rig '%s': Shape Library '%s' contains shape %03d: '%s'");

		const FString PathName = GetPathName();

		for(const TPair<FString, FString>& Pair: ShapeLibraryNameMap)
		{
			UE_LOG(LogControlRig, Display, MapFormat, *PathName, *Pair.Key, *Pair.Value);
		}

		const int32 NumShapeLibraries = ShapeLibraries.Num();
		
		for(int32 Index = 0; Index <NumShapeLibraries; Index++)
		{
			const TSoftObjectPtr<UControlRigShapeLibrary>& ShapeLibrary = ShapeLibraries[Index];
			if(ShapeLibrary.IsNull())
			{
				continue;
			}

			UE_LOG(LogControlRig, Display, LibraryFormat, *PathName, *ShapeLibrary->GetName(), *ShapeLibrary->GetPathName());

			const bool bUseNameSpace = ShapeLibraries.Num() > 1;
			const FString DefaultShapeName = UControlRigShapeLibrary::GetShapeName(ShapeLibrary.Get(), bUseNameSpace, ShapeLibraryNameMap, ShapeLibrary->DefaultShape);
			UE_LOG(LogControlRig, Display, DefaultShapeFormat, *PathName, *ShapeLibrary->GetName(), *DefaultShapeName);

			for(int32 ShapeIndex = 0; ShapeIndex < ShapeLibrary->Shapes.Num(); ShapeIndex++)
			{
				const FString ShapeName = UControlRigShapeLibrary::GetShapeName(ShapeLibrary.Get(), bUseNameSpace, ShapeLibraryNameMap, ShapeLibrary->Shapes[ShapeIndex]);
				UE_LOG(LogControlRig, Display, ShapeFormat, *PathName, *ShapeLibrary->GetName(), ShapeIndex, *ShapeName);
			}
		}
	}
#endif
}

bool UControlRig::OnShapeExists(const FName& InShapeName) const
{
	const TArray<TSoftObjectPtr<UControlRigShapeLibrary>>& Libraries = GetShapeLibraries();
	if (UControlRigShapeLibrary::GetShapeByName(InShapeName, GetShapeLibraries(), ShapeLibraryNameMap))
	{
		return true;
	}
	
	return false;
}

bool UControlRig::InitializeVM(const FName& InEventName)
{
	if(!Super::InitializeVM(InEventName))
	{
		return false;
	}

	RequestConstruction();

#if WITH_EDITOR
	// setup the hierarchy's controller log function
	if(URigHierarchyController* HierarchyController = GetHierarchy()->GetController(true))
	{
		HierarchyController->LogFunction = [this](EMessageSeverity::Type InSeverity, const FString& Message)
		{
			const FRigVMExecuteContext& PublicContext = GetExtendedExecuteContext().GetPublicData<>();
			if(RigVMLog)
			{
				RigVMLog->Report(InSeverity,PublicContext.GetFunctionName(),PublicContext.GetInstructionIndex(), Message);
			}
			else
			{
				LogOnce(InSeverity,PublicContext.GetInstructionIndex(), Message);
			}
		};
	}
#endif

	return true;
}

void UControlRig::InitializeFromCDO()
{
	Super::InitializeFromCDO();
	
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	// copy CDO property you need to here
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		// similar to FControlRigBlueprintCompilerContext::CopyTermDefaultsToDefaultObject,
		// where CDO is initialized from BP there,
		// we initialize all other instances of Control Rig from the CDO here
		UControlRig* CDO = GetClass()->GetDefaultObject<UControlRig>();

		// copy hierarchy
		{
			FRigHierarchyValidityBracket ValidityBracketA(GetHierarchy());
			FRigHierarchyValidityBracket ValidityBracketB(CDO->GetHierarchy());
			
			TGuardValue<bool> Guard(GetHierarchy()->GetSuspendNotificationsFlag(), true);
			GetHierarchy()->CopyHierarchy(CDO->GetHierarchy());
			GetHierarchy()->ResetPoseToInitial(ERigElementType::All);
		}

#if WITH_EDITOR
		// current hierarchy should always mirror CDO's hierarchy whenever a change of interest happens
		CDO->GetHierarchy()->RegisterListeningHierarchy(GetHierarchy());
#endif

		// notify clients that the hierarchy has changed
		GetHierarchy()->Notify(ERigHierarchyNotification::HierarchyReset, nullptr);

		// copy hierarchy settings
		HierarchySettings = CDO->HierarchySettings;
		
		// increment the procedural limit based on the number of elements in the CDO
		if(const URigHierarchy* CDOHierarchy = CDO->GetHierarchy())
		{
			HierarchySettings.ProceduralElementLimit += CDOHierarchy->Num();
		}
	}
}

UControlRig::FAnimAttributeContainerPtrScope::FAnimAttributeContainerPtrScope(UControlRig* InControlRig,
	UE::Anim::FStackAttributeContainer& InExternalContainer)
{
	ControlRig = InControlRig;
	ControlRig->ExternalAnimAttributeContainer = &InExternalContainer;
}

UControlRig::FAnimAttributeContainerPtrScope::~FAnimAttributeContainerPtrScope()
{
	// control rig should not hold on to this container since it is stack allocated
	// and should not be used outside of stack, see FPoseContext
	ControlRig->ExternalAnimAttributeContainer = nullptr;
}

AActor* UControlRig::GetHostingActor() const
{
	return ObjectBinding ? ObjectBinding->GetHostingActor() : nullptr;
}

#if WITH_EDITOR
FText UControlRig::GetCategory() const
{
	return LOCTEXT("DefaultControlRigCategory", "Animation|ControlRigs");
}

FText UControlRig::GetToolTipText() const
{
	return LOCTEXT("DefaultControlRigTooltip", "ControlRig");
}
#endif

bool UControlRig::Execute(const FName& InEventName)
{
	if(!CanExecute())
	{
		return false;
	}

	bool bJustRanInit = false;
	if(bRequiresInitExecution)
	{
		const TGuardValue<float> AbsoluteTimeGuard(AbsoluteTime, AbsoluteTime);
		const TGuardValue<float> DeltaTimeGuard(DeltaTime, DeltaTime);
		if(!InitializeVM(InEventName))
		{
			return false;
		}
		bJustRanInit = true;
	}

	if(EventQueueToRun.IsEmpty())
	{
		EventQueueToRun = EventQueue;
	}

	const bool bIsEventInQueue = EventQueueToRun.Contains(InEventName);
	const bool bIsEventFirstInQueue = !EventQueueToRun.IsEmpty() && EventQueueToRun[0] == InEventName; 
	const bool bIsEventLastInQueue = !EventQueueToRun.IsEmpty() && EventQueueToRun.Last() == InEventName;
	const bool bIsConstructionEvent = InEventName == FRigUnit_PrepareForExecution::EventName;
	const bool bIsForwardSolve = InEventName == FRigUnit_BeginExecution::EventName;
	const bool bIsInteractionEvent = InEventName == FRigUnit_InteractionExecution::EventName;

	ensure(!HasAnyFlags(RF_ClassDefaultObject));
	
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ControlRig_Execute);
	
	FControlRigExecuteContext& PublicContext = GetExtendedExecuteContext().GetPublicDataSafe<FControlRigExecuteContext>();

#if WITH_EDITOR
	PublicContext.SetLog(RigVMLog); // may be nullptr
#endif

	PublicContext.SetDeltaTime(DeltaTime);
	PublicContext.SetAbsoluteTime(AbsoluteTime);
	PublicContext.SetFramesPerSecond(GetCurrentFramesPerSecond());
#if UE_RIGVM_DEBUG_EXECUTION
	PublicContext.bDebugExecution = bDebugExecutionEnabled;
#endif

	if (VM)
	{
		if (VM->GetOuter() != this)
		{
			InstantiateVMFromCDO();
		}

#if WITH_EDITOR
		// default to always clear data after each execution
		// only set a valid first entry event later when execution
		// has passed the initialization stage and there are multiple events present in one evaluation
		// first entry event is used to determined when to clear data during an evaluation
		VM->SetFirstEntryEventInEventQueue(GetExtendedExecuteContext(), NAME_None);
#endif
	}

#if WITH_EDITOR
	if (IsInDebugMode())
	{
		if (UControlRig* CDO = GetClass()->GetDefaultObject<UControlRig>())
		{
			// Copy the breakpoints. This will not override the state of the breakpoints
			DebugInfo.SetBreakpoints(CDO->DebugInfo.GetBreakpoints());

			// If there are any breakpoints, create the Snapshot VM if it hasn't been created yet
			if (DebugInfo.GetBreakpoints().Num() > 0)
			{
				GetSnapshotVM();
			}
		}

		GetExtendedExecuteContext().SetDebugInfo(&DebugInfo);
		GetSnapshotContext() = GetExtendedExecuteContext();
	}
	else
	{
		GetExtendedExecuteContext().SetDebugInfo(nullptr);
		GetSnapshotContext().Reset();
	}
#endif

	FRigUnitContext& Context = PublicContext.UnitContext;

	bool bEnableDrawInterface = false;
#if WITH_EDITOR
	const bool bEnabledDuringGame = CVarControlRigEnableDrawInterfaceInGame->GetInt() != 0;
	const bool bInGame = !(GEditor && !GEditor->GetPIEWorldContext());
	if (bEnabledDuringGame || !bInGame)
	{
		bEnableDrawInterface = true;
	}	
#endif

	// setup the draw interface for debug drawing
	if(!bIsEventInQueue || bIsEventFirstInQueue)
	{
		DrawInterface.Reset();
	}

	if (bEnableDrawInterface)
	{
		PublicContext.SetDrawInterface(&DrawInterface);
	}
	else
	{
		PublicContext.SetDrawInterface(nullptr);
	}

	// setup the animation attribute container
	Context.AnimAttributeContainer = ExternalAnimAttributeContainer;

	// draw container contains persistent draw instructions, 
	// so we cannot call Reset(), which will clear them,
	// instead, we re-initialize them from the CDO
	if (UControlRig* CDO = GetClass()->GetDefaultObject<UControlRig>())
	{
		DrawContainer = CDO->DrawContainer;
	}
	PublicContext.SetDrawContainer(&DrawContainer);

	// setup the data source registry
	Context.DataSourceRegistry = GetDataSourceRegistry();

	// setup the context with further fields
	Context.InteractionType = InteractionType;
	Context.ElementsBeingInteracted = ElementsBeingInteracted;
	PublicContext.Hierarchy = GetHierarchy();

	// allow access to the hierarchy
	Context.HierarchySettings = HierarchySettings;
	check(PublicContext.Hierarchy);

	// allow access to the shape libraries
	PublicContext.OnAddShapeLibraryDelegate.BindUObject(this, &UControlRig::OnAddShapeLibrary);
	PublicContext.OnShapeExistsDelegate.BindUObject(this, &UControlRig::OnShapeExists);

	// allow access to the default hierarchy to allow to reset
	if(!HasAnyFlags(RF_ClassDefaultObject))
	{
		if (UControlRig* CDO = Cast<UControlRig>(GetClass()->GetDefaultObject()))
		{
			if(URigHierarchy* DefaultHierarchy = CDO->GetHierarchy())
			{
				PublicContext.Hierarchy->DefaultHierarchyPtr = DefaultHierarchy;
			}
		}
	}

	// disable any controller access outside of the construction event
	FRigHierarchyEnableControllerBracket DisableHierarchyController(PublicContext.Hierarchy, bIsConstructionEvent);

	// given the outer scene component configure
	// the transform lookups to map transforms from rig space to world space
	if (USceneComponent* SceneComponent = GetOwningSceneComponent())
	{
		PublicContext.SetOwningComponent(SceneComponent);
	}
	else
	{
		PublicContext.SetOwningComponent(nullptr);
		
		if (ObjectBinding.IsValid())
		{
			AActor* HostingActor = ObjectBinding->GetHostingActor();
			if (HostingActor)
			{
				PublicContext.SetOwningActor(HostingActor);
			}
			else if (UObject* Owner = ObjectBinding->GetBoundObject())
			{
				PublicContext.SetWorld(Owner->GetWorld());
			}
		}

		if (PublicContext.GetWorld() == nullptr)
		{
			if (UObject* Outer = GetOuter())
			{
				PublicContext.SetWorld(Outer->GetWorld());
			}
		}
	}
	
	// setup the user data
	// first using the data from the rig,
	// followed by the data coming from the skeleton
	// then the data coming from the skeletal mesh,
	// and in the future we'll also add the data from the control rig component
	PublicContext.AssetUserData.Reset();
	if(const TArray<UAssetUserData*>* ControlRigUserDataArray = GetAssetUserDataArray())
	{
		for(const UAssetUserData* ControlRigUserData : *ControlRigUserDataArray)
		{
			PublicContext.AssetUserData.Add(ControlRigUserData);
		}
	}
	PublicContext.AssetUserData.Remove(nullptr);

	// if we have any referenced elements dirty them
	if(GetHierarchy())
	{
		GetHierarchy()->UpdateReferences(&PublicContext);
	}

	// guard against recursion
	if(IsExecuting())
	{
		UE_LOG(LogControlRig, Warning, TEXT("%s: Execute is being called recursively."), *GetPathName());
		return false;
	}
	if(bIsConstructionEvent)
	{
		if(IsRunningPreConstruction() || IsRunningPostConstruction())
		{
			UE_LOG(LogControlRig, Warning, TEXT("%s: Construction is being called recursively."), *GetPathName());
			return false;
		}
	}

	bool bSuccess = true;

	// we'll special case the construction event here
	if (bIsConstructionEvent)
	{
		// remember the previous selection
		const TArray<FRigElementKey> PreviousSelection = GetHierarchy()->GetSelectedKeys();

		// construction mode means that we are running the construction event
		// constantly for testing purposes.
		const bool bConstructionModeEnabled = IsConstructionModeEnabled();
		{
			if (PreConstructionForUIEvent.IsBound())
			{
				FControlRigBracketScope BracketScope(PreConstructionBracket);
				PreConstructionForUIEvent.Broadcast(this, FRigUnit_PrepareForExecution::EventName);
			}

#if WITH_EDITOR
			// apply the selection pose for the construction mode
			// we are doing this here to make sure the brackets below have the right data
			if(bConstructionModeEnabled)
			{
				ApplySelectionPoseForConstructionMode(InEventName);
			}
#endif

			// disable selection notifications from the hierarchy
			TGuardValue<bool> DisableSelectionNotifications(GetHierarchy()->GetController(true)->bSuspendSelectionNotifications, true);
			{
				// save the current state of all pose elements to preserve user intention, since construction event can
				// run in between forward events
				// the saved pose is reapplied to the rig after construction event as the pose scope goes out of scope
				TUniquePtr<FPoseScope> PoseScope;
				if (!bConstructionModeEnabled)
				{
					// only do this in non-construction mode because 
					// when construction mode is enabled, the control values are cleared before reaching here (too late to save them)
					if (!bJustRanInit)
					{
						PoseScope = MakeUnique<FPoseScope>(this, ERigElementType::ToResetAfterConstructionEvent, TArray<FRigElementKey>(), ERigTransformType::CurrentGlobal);
					}
					else
					{
						PoseScope = MakeUnique<FPoseScope>(this, ERigElementType::ToResetAfterConstructionEvent);
					}
				}
				
				{
					// Copy the hierarchy from the default object onto this one
#if WITH_EDITOR
					FTransientControlScope TransientControlScope(GetHierarchy());
	#endif
					{
						// maintain the initial pose if it ever was set by the client
						FRigPose InitialPose;
						if(!bResetInitialTransformsBeforeConstruction)
						{
							InitialPose = GetHierarchy()->GetPose(true, ERigElementType::ToResetAfterConstructionEvent, FRigElementKeyCollection());
						}

						if(bCopyHierarchyBeforeConstruction)
						{
							GetHierarchy()->ResetToDefault();
						}

						if(InitialPose.Num() > 0)
						{
							GetHierarchy()->SetPose(InitialPose, ERigTransformType::InitialLocal);
						}
					}

					{
	#if WITH_EDITOR
						TUniquePtr<FTransientControlPoseScope> TransientControlPoseScope;
						if (bConstructionModeEnabled)
						{
							// save the transient control value, it should not be constantly reset in construction mode
							TransientControlPoseScope = MakeUnique<FTransientControlPoseScope>(this);
						}
	#endif
						// reset the pose to initial such that construction event can run from a deterministic initial state
						GetHierarchy()->ResetPoseToInitial(ERigElementType::All);
					}

					// clone the shape libraries again from the CDO 
					if (!HasAnyFlags(RF_ClassDefaultObject) && ShapeLibraries.IsEmpty())
					{
						UControlRig* CDO = GetClass()->GetDefaultObject<UControlRig>();
						ShapeLibraries = CDO->ShapeLibraries;
					}

					if (PreConstructionEvent.IsBound())
					{
						FControlRigBracketScope BracketScope(PreConstructionBracket);
						PreConstructionEvent.Broadcast(this, FRigUnit_PrepareForExecution::EventName);
					}

					bSuccess = Execute_Internal(FRigUnit_PrepareForExecution::EventName);
					
				} // destroy FTransientControlScope
				
				if (PostConstructionEvent.IsBound())
				{
					FControlRigBracketScope BracketScope(PostConstructionBracket);
					PostConstructionEvent.Broadcast(this, FRigUnit_PrepareForExecution::EventName);
				}
			}
			
			// set it here to reestablish the selection. the notifications
			// will be eaten since we still have the bSuspend flag on in the controller.
			GetHierarchy()->GetController()->SetSelection(PreviousSelection);
			
		} // destroy DisableSelectionNotifications

		if (bConstructionModeEnabled)
		{
#if WITH_EDITOR
			TUniquePtr<FTransientControlPoseScope> TransientControlPoseScope;
			if (bConstructionModeEnabled)
			{
				// save the transient control value, it should not be constantly reset in construction mode
				TransientControlPoseScope = MakeUnique<FTransientControlPoseScope>(this);
			}
#endif
			GetHierarchy()->ResetPoseToInitial(ERigElementType::Bone);
		}

		// synchronize the selection now with the new hierarchy after running construction
		const TArray<const FRigBaseElement*> CurrentSelection = GetHierarchy()->GetSelectedElements();
		for(const FRigBaseElement* SelectedElement : CurrentSelection)
		{
			if(!PreviousSelection.Contains(SelectedElement->GetKey()))
			{
				GetHierarchy()->Notify(ERigHierarchyNotification::ElementSelected, SelectedElement);
			}
		}
		for(const FRigElementKey& PreviouslySelectedKey : PreviousSelection)
		{
			if(const FRigBaseElement* PreviouslySelectedElement = GetHierarchy()->Find(PreviouslySelectedKey))
			{
				if(!CurrentSelection.Contains(PreviouslySelectedElement))
				{
					GetHierarchy()->Notify(ERigHierarchyNotification::ElementDeselected, PreviouslySelectedElement);
				}
			}
		}
	}
	else
	{
#if WITH_EDITOR
		// only set a valid first entry event when execution
		// has passed the initialization stage and there are multiple events present
		if (EventQueueToRun.Num() >= 2 && VM)
		{
			VM->SetFirstEntryEventInEventQueue(GetExtendedExecuteContext(), EventQueueToRun[0]);
		}

		// Transform Overrride is generated using a Transient Control 
		ApplyTransformOverrideForUserCreatedBones();

		if (bEnableAnimAttributeTrace && ExternalAnimAttributeContainer != nullptr)
		{
			InputAnimAttributeSnapshot.CopyFrom(*ExternalAnimAttributeContainer);
		}
#endif
		
		if (bIsForwardSolve)
		{
			if (PreForwardsSolveEvent.IsBound())
			{
				FControlRigBracketScope BracketScope(PreForwardsSolveBracket);
				PreForwardsSolveEvent.Broadcast(this, FRigUnit_BeginExecution::EventName);
			}
		}

		bSuccess = Execute_Internal(InEventName);

#if WITH_EDITOR
		if (bEnableAnimAttributeTrace && ExternalAnimAttributeContainer != nullptr)
		{
			OutputAnimAttributeSnapshot.CopyFrom(*ExternalAnimAttributeContainer);
		}
#endif

		if (bIsForwardSolve)
		{
			if (PostForwardsSolveEvent.IsBound())
			{
				FControlRigBracketScope BracketScope(PostForwardsSolveBracket);
				PostForwardsSolveEvent.Broadcast(this, FRigUnit_BeginExecution::EventName);
			}
		}
	}

#if WITH_EDITOR

	// for the last event in the queue - clear the log message queue
	if (RigVMLog != nullptr && bEnableLogging)
	{
		if (bJustRanInit)
		{
			RigVMLog->KnownMessages.Reset();
			LoggedMessages.Reset();
		}
		else if(bIsEventLastInQueue)
		{
			for (const FRigVMLog::FLogEntry& Entry : RigVMLog->Entries)
			{
				if (Entry.FunctionName == NAME_None || Entry.InstructionIndex == INDEX_NONE || Entry.Message.IsEmpty())
				{
					continue;
				}

				FString PerInstructionMessage = 
					FString::Printf(
						TEXT("Instruction[%d] '%s': '%s'"),
						Entry.InstructionIndex,
						*Entry.FunctionName.ToString(),
						*Entry.Message
					);

				LogOnce(Entry.Severity, Entry.InstructionIndex, PerInstructionMessage);
			}
		}
	}
#endif

	if(!bIsEventInQueue || bIsEventLastInQueue) 
	{
		DeltaTime = 0.f;
	}

	if (ExecutedEvent.IsBound())
	{
		FControlRigBracketScope BracketScope(ExecuteBracket);
		ExecutedEvent.Broadcast(this, InEventName);
	}

	// close remaining undo brackets from hierarchy
	while(ControlUndoBracketIndex > 0)
	{
		FRigEventContext EventContext;
		EventContext.Event = ERigEvent::CloseUndoBracket;
		EventContext.SourceEventName = InEventName;
		EventContext.LocalTime = PublicContext.GetAbsoluteTime();
		HandleHierarchyEvent(GetHierarchy(), EventContext);
	}

	if (PublicContext.GetDrawInterface() && PublicContext.GetDrawContainer() && bIsEventLastInQueue) 
	{
		PublicContext.GetDrawInterface()->Instructions.Append(PublicContext.GetDrawContainer()->Instructions);

		FRigHierarchyValidityBracket ValidityBracket(GetHierarchy());
		
		GetHierarchy()->ForEach<FRigControlElement>([this](FRigControlElement* ControlElement) -> bool
		{
			const FRigControlSettings& Settings = ControlElement->Settings;

			if (Settings.IsVisible() &&
				!Settings.bIsTransientControl &&
				Settings.bDrawLimits &&
				Settings.LimitEnabled.Contains(FRigControlLimitEnabled(true, true)))
			{
				FTransform Transform = GetHierarchy()->GetGlobalControlOffsetTransformByIndex(ControlElement->GetIndex());
				FRigVMDrawInstruction Instruction(ERigVMDrawSettings::Lines, Settings.ShapeColor, 0.f, Transform);

				switch (Settings.ControlType)
				{
					case ERigControlType::Float:
					{
						if(Settings.LimitEnabled[0].IsOff())
						{
							break;
						}

						FVector MinPos = FVector::ZeroVector;
						FVector MaxPos = FVector::ZeroVector;

						switch (Settings.PrimaryAxis)
						{
							case ERigControlAxis::X:
							{
								MinPos.X = Settings.MinimumValue.Get<float>();
								MaxPos.X = Settings.MaximumValue.Get<float>();
								break;
							}
							case ERigControlAxis::Y:
							{
								MinPos.Y = Settings.MinimumValue.Get<float>();
								MaxPos.Y = Settings.MaximumValue.Get<float>();
								break;
							}
							case ERigControlAxis::Z:
							{
								MinPos.Z = Settings.MinimumValue.Get<float>();
								MaxPos.Z = Settings.MaximumValue.Get<float>();
								break;
							}
						}

						Instruction.Positions.Add(MinPos);
						Instruction.Positions.Add(MaxPos);
						break;
					}
					case ERigControlType::Integer:
					{
						if(Settings.LimitEnabled[0].IsOff())
						{
							break;
						}

						FVector MinPos = FVector::ZeroVector;
						FVector MaxPos = FVector::ZeroVector;

						switch (Settings.PrimaryAxis)
						{
							case ERigControlAxis::X:
							{
								MinPos.X = (float)Settings.MinimumValue.Get<int32>();
								MaxPos.X = (float)Settings.MaximumValue.Get<int32>();
								break;
							}
							case ERigControlAxis::Y:
							{
								MinPos.Y = (float)Settings.MinimumValue.Get<int32>();
								MaxPos.Y = (float)Settings.MaximumValue.Get<int32>();
								break;
							}
							case ERigControlAxis::Z:
							{
								MinPos.Z = (float)Settings.MinimumValue.Get<int32>();
								MaxPos.Z = (float)Settings.MaximumValue.Get<int32>();
								break;
							}
						}

						Instruction.Positions.Add(MinPos);
						Instruction.Positions.Add(MaxPos);
						break;
					}
					case ERigControlType::Vector2D:
					{
						if(Settings.LimitEnabled.Num() < 2)
						{
							break;
						}
						if(Settings.LimitEnabled[0].IsOff() && Settings.LimitEnabled[1].IsOff())
						{
							break;
						}

						Instruction.PrimitiveType = ERigVMDrawSettings::LineStrip;
						FVector3f MinPos = Settings.MinimumValue.Get<FVector3f>();
						FVector3f MaxPos = Settings.MaximumValue.Get<FVector3f>();

						switch (Settings.PrimaryAxis)
						{
							case ERigControlAxis::X:
							{
								Instruction.Positions.Add(FVector(0.f, MinPos.X, MinPos.Y));
								Instruction.Positions.Add(FVector(0.f, MaxPos.X, MinPos.Y));
								Instruction.Positions.Add(FVector(0.f, MaxPos.X, MaxPos.Y));
								Instruction.Positions.Add(FVector(0.f, MinPos.X, MaxPos.Y));
								Instruction.Positions.Add(FVector(0.f, MinPos.X, MinPos.Y));
								break;
							}
							case ERigControlAxis::Y:
							{
								Instruction.Positions.Add(FVector(MinPos.X, 0.f, MinPos.Y));
								Instruction.Positions.Add(FVector(MaxPos.X, 0.f, MinPos.Y));
								Instruction.Positions.Add(FVector(MaxPos.X, 0.f, MaxPos.Y));
								Instruction.Positions.Add(FVector(MinPos.X, 0.f, MaxPos.Y));
								Instruction.Positions.Add(FVector(MinPos.X, 0.f, MinPos.Y));
								break;
							}
							case ERigControlAxis::Z:
							{
								Instruction.Positions.Add(FVector(MinPos.X, MinPos.Y, 0.f));
								Instruction.Positions.Add(FVector(MaxPos.X, MinPos.Y, 0.f));
								Instruction.Positions.Add(FVector(MaxPos.X, MaxPos.Y, 0.f));
								Instruction.Positions.Add(FVector(MinPos.X, MaxPos.Y, 0.f));
								Instruction.Positions.Add(FVector(MinPos.X, MinPos.Y, 0.f));
								break;
							}
						}
						break;
					}
					case ERigControlType::Position:
					case ERigControlType::Scale:
					case ERigControlType::Transform:
					case ERigControlType::TransformNoScale:
					case ERigControlType::EulerTransform:
					{
						FVector3f MinPos = FVector3f::ZeroVector;
						FVector3f MaxPos = FVector3f::ZeroVector;

						// we only check the first three here
						// since we only consider translation anyway
						// for scale it's also the first three
						if(Settings.LimitEnabled.Num() < 3)
						{
							break;
						}
						if(!Settings.LimitEnabled[0].IsOn() && !Settings.LimitEnabled[1].IsOn() && !Settings.LimitEnabled[2].IsOn())
						{
							break;
						}

						switch (Settings.ControlType)
						{
							case ERigControlType::Position:
							case ERigControlType::Scale:
							{
								MinPos = Settings.MinimumValue.Get<FVector3f>();
								MaxPos = Settings.MaximumValue.Get<FVector3f>();
								break;
							}
							case ERigControlType::Transform:
							{
								MinPos = Settings.MinimumValue.Get<FRigControlValue::FTransform_Float>().GetTranslation();
								MaxPos = Settings.MaximumValue.Get<FRigControlValue::FTransform_Float>().GetTranslation();
								break;
							}
							case ERigControlType::TransformNoScale:
							{
								MinPos = Settings.MinimumValue.Get<FRigControlValue::FTransformNoScale_Float>().GetTranslation();
								MaxPos = Settings.MaximumValue.Get<FRigControlValue::FTransformNoScale_Float>().GetTranslation();
								break;
							}
							case ERigControlType::EulerTransform:
							{
								MinPos = Settings.MinimumValue.Get<FRigControlValue::FEulerTransform_Float>().GetTranslation();
								MaxPos = Settings.MaximumValue.Get<FRigControlValue::FEulerTransform_Float>().GetTranslation();
								break;
							}
						}

						Instruction.Positions.Add(FVector(MinPos.X, MinPos.Y, MinPos.Z));
						Instruction.Positions.Add(FVector(MaxPos.X, MinPos.Y, MinPos.Z));
						Instruction.Positions.Add(FVector(MinPos.X, MaxPos.Y, MinPos.Z));
						Instruction.Positions.Add(FVector(MaxPos.X, MaxPos.Y, MinPos.Z));
						Instruction.Positions.Add(FVector(MinPos.X, MinPos.Y, MaxPos.Z));
						Instruction.Positions.Add(FVector(MaxPos.X, MinPos.Y, MaxPos.Z));
						Instruction.Positions.Add(FVector(MinPos.X, MaxPos.Y, MaxPos.Z));
						Instruction.Positions.Add(FVector(MaxPos.X, MaxPos.Y, MaxPos.Z));

						Instruction.Positions.Add(FVector(MinPos.X, MinPos.Y, MinPos.Z));
						Instruction.Positions.Add(FVector(MinPos.X, MaxPos.Y, MinPos.Z));
						Instruction.Positions.Add(FVector(MaxPos.X, MinPos.Y, MinPos.Z));
						Instruction.Positions.Add(FVector(MaxPos.X, MaxPos.Y, MinPos.Z));
						Instruction.Positions.Add(FVector(MinPos.X, MinPos.Y, MaxPos.Z));
						Instruction.Positions.Add(FVector(MinPos.X, MaxPos.Y, MaxPos.Z));
						Instruction.Positions.Add(FVector(MaxPos.X, MinPos.Y, MaxPos.Z));
						Instruction.Positions.Add(FVector(MaxPos.X, MaxPos.Y, MaxPos.Z));

						Instruction.Positions.Add(FVector(MinPos.X, MinPos.Y, MinPos.Z));
						Instruction.Positions.Add(FVector(MinPos.X, MinPos.Y, MaxPos.Z));
						Instruction.Positions.Add(FVector(MaxPos.X, MinPos.Y, MinPos.Z));
						Instruction.Positions.Add(FVector(MaxPos.X, MinPos.Y, MaxPos.Z));
						Instruction.Positions.Add(FVector(MinPos.X, MaxPos.Y, MinPos.Z));
						Instruction.Positions.Add(FVector(MinPos.X, MaxPos.Y, MaxPos.Z));
						Instruction.Positions.Add(FVector(MaxPos.X, MaxPos.Y, MinPos.Z));
						Instruction.Positions.Add(FVector(MaxPos.X, MaxPos.Y, MaxPos.Z));
						break;
					}
				}

				if (Instruction.Positions.Num() > 0)
				{
					DrawInterface.DrawInstruction(Instruction);
				}
			}

			return true;
		});
	}

	if(bIsInteractionEvent)
	{
		bInteractionJustBegan = false;
	}

	if(bIsConstructionEvent)
	{
		RemoveRunOnceEvent(FRigUnit_PrepareForExecution::EventName);
	}

	return bSuccess;
}

bool UControlRig::Execute_Internal(const FName& InEventName)
{
	if (VM)
	{
		FRigVMExtendedExecuteContext& Context = GetExtendedExecuteContext();

		static constexpr TCHAR InvalidatedVMFormat[] = TEXT("%s: Invalidated VM - aborting execution.");
		if(VM->IsNativized())
		{
			if(!IsValidLowLevel() ||
				!VM->IsValidLowLevel())
			{
				UE_LOG(LogControlRig, Warning, InvalidatedVMFormat, *GetClass()->GetName());
				return false;
			}
		}
		else
		{
			// sanity check the validity of the VM to ensure stability.
			if(!VM->IsContextValidForExecution(Context) ||
				!IsValidLowLevel() ||
				!VM->IsValidLowLevel() ||
				!VM->GetLiteralMemory()->IsValidLowLevel() ||
				!VM->GetWorkMemory()->IsValidLowLevel())
			{
				UE_LOG(LogControlRig, Warning, InvalidatedVMFormat, *GetClass()->GetName());
				return false;
			}
		}
		
#if UE_RIGVM_PROFILE_EXECUTE_UNITS_NUM
		const uint64 StartCycles = FPlatformTime::Cycles64();
		if(ProfilingRunsLeft <= 0)
		{
			ProfilingRunsLeft = UE_RIGVM_PROFILE_EXECUTE_UNITS_NUM;
			AccumulatedCycles = 0;
		}
#endif
		
		const bool bUseDebuggingSnapshots = !VM->IsNativized();
		
		TArray<URigVMMemoryStorage*> LocalMemory = VM->GetLocalMemoryArray();

#if WITH_EDITOR
		if(bUseDebuggingSnapshots)
		{
			if(URigVM* SnapShotVM = GetSnapshotVM(false)) // don't create it for normal runs
			{
				const bool bIsEventFirstInQueue = !EventQueueToRun.IsEmpty() && EventQueueToRun[0] == InEventName; 
				const bool bIsEventLastInQueue = !EventQueueToRun.IsEmpty() && EventQueueToRun.Last() == InEventName;

				if (GetHaltedAtBreakpoint().IsValid())
				{
					if(bIsEventFirstInQueue)
					{
						VM->CopyFrom(SnapShotVM, false, false, false, true, true);
						GetExtendedExecuteContext() = GetSnapshotContext();
					}
				}
				else if(bIsEventLastInQueue)
				{
					SnapShotVM->CopyFrom(VM, false, false, false, true, true);
					GetSnapshotContext() = GetExtendedExecuteContext();
				}
			}
		}
#endif

		URigHierarchy* Hierarchy = GetHierarchy();
#if WITH_EDITOR

		bool bRecordTransformsAtRuntime = true;
		if(const UObject* Outer = GetOuter())
		{
			if(Outer->IsA<UControlRigComponent>())
			{
				bRecordTransformsAtRuntime = false;
			}
		}
		TGuardValue<bool> RecordTransformsPerInstructionGuard(Hierarchy->bRecordTransformsAtRuntime, bRecordTransformsAtRuntime);
		
		if(Hierarchy->bRecordTransformsAtRuntime)
		{
			Hierarchy->ReadTransformsAtRuntime.Reset();
			Hierarchy->WrittenTransformsAtRuntime.Reset();
		}
		
#endif
		FRigHierarchyExecuteContextBracket HierarchyContextGuard(Hierarchy, &Context);

		const bool bSuccess = VM->Execute(Context, LocalMemory, InEventName) != ERigVMExecuteResult::Failed;

#if UE_RIGVM_PROFILE_EXECUTE_UNITS_NUM
		const uint64 EndCycles = FPlatformTime::Cycles64();
		const uint64 Cycles = EndCycles - StartCycles;
		AccumulatedCycles += Cycles;
		ProfilingRunsLeft--;
		if(ProfilingRunsLeft == 0)
		{
			const double Milliseconds = FPlatformTime::ToMilliseconds64(AccumulatedCycles);
			UE_LOG(LogControlRig, Display, TEXT("%s: %d runs took %.03lfms."), *GetClass()->GetName(), UE_RIGVM_PROFILE_EXECUTE_UNITS_NUM, Milliseconds);
		}
#endif

		return bSuccess;
	}
	return false;
}

void UControlRig::RequestInit()
{
	Super::RequestInit();
	RequestConstruction();
}

void UControlRig::RequestConstruction()
{
	RequestRunOnceEvent(FRigUnit_PrepareForExecution::EventName, 0);
}

bool UControlRig::IsConstructionRequired() const
{
	return IsRunOnceEvent(FRigUnit_PrepareForExecution::EventName);
}

void UControlRig::AdaptEventQueueForEvaluate(TArray<FName>& InOutEventQueueToRun)
{
	Super::AdaptEventQueueForEvaluate(InOutEventQueueToRun);

	if(InteractionType != (uint8)EControlRigInteractionType::None)
	{
		static const FName& InteractionEvent = FRigUnit_InteractionExecution::EventName;
		if(SupportsEvent(InteractionEvent))
		{
			if(InOutEventQueueToRun.IsEmpty())
			{
				InOutEventQueueToRun.Add(InteractionEvent);
			}
			else if(!InOutEventQueueToRun.Contains(FRigUnit_PrepareForExecution::EventName))
			{
				// insert just before the last event so the interaction runs prior to
				// forward solve or backwards solve.
				InOutEventQueueToRun.Insert(InteractionEvent, InOutEventQueueToRun.Num() - 1);
			}
		}
	}
}

void UControlRig::GetMappableNodeData(TArray<FName>& OutNames, TArray<FNodeItem>& OutNodeItems) const
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	OutNames.Reset();
	OutNodeItems.Reset();

	check(DynamicHierarchy);

	// now add all nodes
	DynamicHierarchy->ForEach<FRigBoneElement>([&OutNames, &OutNodeItems, this](FRigBoneElement* BoneElement) -> bool
    {
		OutNames.Add(BoneElement->GetName());
		FRigElementKey ParentKey = DynamicHierarchy->GetFirstParent(BoneElement->GetKey());
		if(ParentKey.Type != ERigElementType::Bone)
		{
			ParentKey.Name = NAME_None;
		}

		const FTransform GlobalInitial = DynamicHierarchy->GetGlobalTransformByIndex(BoneElement->GetIndex(), true);
		OutNodeItems.Add(FNodeItem(ParentKey.Name, GlobalInitial));
		return true;
	});
}

UAnimationDataSourceRegistry* UControlRig::GetDataSourceRegistry()
{
	if (DataSourceRegistry)
	{
		if (DataSourceRegistry->GetOuter() != this)
		{
			DataSourceRegistry = nullptr;
		}
	}
	if (DataSourceRegistry == nullptr)
	{
		DataSourceRegistry = NewObject<UAnimationDataSourceRegistry>(this, NAME_None, RF_Transient);
	}

	return DataSourceRegistry;
}

#if WITH_EDITORONLY_DATA

void UControlRig::PostReinstanceCallback(const UControlRig* Old)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	ObjectBinding = Old->ObjectBinding;
	Initialize();
}

#endif // WITH_EDITORONLY_DATA

TArray<UControlRig*> UControlRig::FindControlRigs(UObject* Outer, TSubclassOf<UControlRig> OptionalClass)
{
	TArray<UControlRig*> Result;
	
	if(Outer == nullptr)
	{
		return Result; 
	}
	
	AActor* OuterActor = Cast<AActor>(Outer);
	if(OuterActor == nullptr)
	{
		OuterActor = Outer->GetTypedOuter<AActor>();
	}
	
	for (TObjectIterator<UControlRig> Itr; Itr; ++Itr)
	{
		UControlRig* RigInstance = *Itr;
		const UClass* RigInstanceClass = RigInstance ? RigInstance->GetClass() : nullptr;
		if (OptionalClass == nullptr || (RigInstanceClass && RigInstanceClass->IsChildOf(OptionalClass)))
		{
			if(RigInstance->IsInOuter(Outer))
			{
				Result.Add(RigInstance);
				continue;
			}

			if(OuterActor)
			{
				if(RigInstance->IsInOuter(OuterActor))
				{
					Result.Add(RigInstance);
					continue;
				}

				if (TSharedPtr<IControlRigObjectBinding> Binding = RigInstance->GetObjectBinding())
				{
					if (AActor* Actor = Binding->GetHostingActor())
					{
						if (Actor == OuterActor)
						{
							Result.Add(RigInstance);
							continue;
						}
					}
				}
			}
		}
	}

	return Result;
}

void UControlRig::Serialize(FArchive& Ar)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FControlRigObjectVersion::GUID);
}

void UControlRig::PostLoad()
{
	Super::PostLoad();

	if(HasAnyFlags(RF_ClassDefaultObject))
	{
		if(DynamicHierarchy)
		{
			// Some dynamic hierarchy objects have been created using NewObject<> instead of CreateDefaultSubObjects.
			// Assets from that version require the dynamic hierarchy to be flagged as below.
			DynamicHierarchy->SetFlags(DynamicHierarchy->GetFlags() | RF_Public | RF_DefaultSubObject);
		}
	}

	FRigInfluenceMapPerEvent NewInfluences;
	for(int32 MapIndex = 0; MapIndex < Influences.Num(); MapIndex++)
	{
		FRigInfluenceMap Map = Influences[MapIndex];
		FName EventName = Map.GetEventName();

		if(EventName == TEXT("Update"))
		{
			EventName = FRigUnit_BeginExecution::EventName;
		}
		else if(EventName == TEXT("Inverse"))
		{
			EventName = FRigUnit_InverseExecution::EventName;
		}
		
		NewInfluences.FindOrAdd(EventName).Merge(Map, true);
	}
	Influences = NewInfluences;
}

TArray<FRigControlElement*> UControlRig::AvailableControls() const
{
	if(DynamicHierarchy)
	{
		return DynamicHierarchy->GetElementsOfType<FRigControlElement>();
	}
	return TArray<FRigControlElement*>();
}

FRigControlElement* UControlRig::FindControl(const FName& InControlName) const
{
	if(DynamicHierarchy == nullptr)
	{
		return nullptr;
	}
	return DynamicHierarchy->Find<FRigControlElement>(FRigElementKey(InControlName, ERigElementType::Control));
}

bool UControlRig::IsConstructionModeEnabled() const
{
	return EventQueueToRun.Num() == 1 && EventQueueToRun.Contains(FRigUnit_PrepareForExecution::EventName);
}

FTransform UControlRig::SetupControlFromGlobalTransform(const FName& InControlName, const FTransform& InGlobalTransform)
{
	if (IsConstructionModeEnabled())
	{
		FRigControlElement* ControlElement = FindControl(InControlName);
		if (ControlElement && !ControlElement->Settings.bIsTransientControl)
		{
			const FTransform ParentTransform = GetHierarchy()->GetParentTransform(ControlElement, ERigTransformType::CurrentGlobal);
			const FTransform OffsetTransform = InGlobalTransform.GetRelativeTransform(ParentTransform);
			GetHierarchy()->SetControlOffsetTransform(ControlElement, OffsetTransform, ERigTransformType::InitialLocal, true, false);
			GetHierarchy()->SetControlOffsetTransform(ControlElement, OffsetTransform, ERigTransformType::CurrentLocal, true, false);

			if(URigHierarchy* DefaultHierarchy = GetHierarchy()->GetDefaultHierarchy())
			{
				if(FRigControlElement* DefaultControlElement = DefaultHierarchy->Find<FRigControlElement>(ControlElement->GetKey()))
				{
					DefaultHierarchy->SetControlOffsetTransform(DefaultControlElement, OffsetTransform, ERigTransformType::InitialLocal, true, true);
					DefaultHierarchy->SetControlOffsetTransform(DefaultControlElement, OffsetTransform, ERigTransformType::CurrentLocal, true, true);
				}
			}
		}
	}
	return InGlobalTransform;
}

void UControlRig::CreateRigControlsForCurveContainer()
{
	const bool bCreateFloatControls = CVarControlRigCreateFloatControlsForCurves->GetInt() == 0 ? false : true;
	if(bCreateFloatControls && DynamicHierarchy)
	{
		URigHierarchyController* Controller = DynamicHierarchy->GetController(true);
		if(Controller == nullptr)
		{
			return;
		}
		static const FString CtrlPrefix(TEXT("CTRL_"));

		DynamicHierarchy->ForEach<FRigCurveElement>([this, Controller](FRigCurveElement* CurveElement) -> bool
        {
			const FString Name = CurveElement->GetName().ToString();
			
			if (Name.Contains(CtrlPrefix) && !DynamicHierarchy->Contains(FRigElementKey(*Name, ERigElementType::Curve))) //-V1051
			{
				FRigControlSettings Settings;
				Settings.AnimationType = ERigControlAnimationType::AnimationChannel;
				Settings.ControlType = ERigControlType::Float;
				Settings.bIsCurve = true;
				Settings.bDrawLimits = false;

				FRigControlValue Value;
				Value.Set<float>(CurveElement->Value);

				Controller->AddControl(CurveElement->GetName(), FRigElementKey(), Settings, Value, FTransform::Identity, FTransform::Identity); 
			}

			return true;
		});

		ControlModified().AddUObject(this, &UControlRig::HandleOnControlModified);
	}
}

void UControlRig::HandleOnControlModified(UControlRig* Subject, FRigControlElement* Control, const FRigControlModifiedContext& Context)
{
	if (Control->Settings.bIsCurve && DynamicHierarchy)
	{
		const FRigControlValue Value = DynamicHierarchy->GetControlValue(Control, IsConstructionModeEnabled() ? ERigControlValueType::Initial : ERigControlValueType::Current);
		DynamicHierarchy->SetCurveValue(FRigElementKey(Control->GetName(), ERigElementType::Curve), Value.Get<float>());
	}	
}

bool UControlRig::IsCurveControl(const FRigControlElement* InControlElement) const
{
	return InControlElement->Settings.bIsCurve;
}

FTransform UControlRig::GetControlGlobalTransform(const FName& InControlName) const
{
	if(DynamicHierarchy == nullptr)
	{
		return FTransform::Identity;
	}
	return DynamicHierarchy->GetGlobalTransform(FRigElementKey(InControlName, ERigElementType::Control), false);
}

bool UControlRig::SetControlGlobalTransform(const FName& InControlName, const FTransform& InGlobalTransform, bool bNotify, const FRigControlModifiedContext& Context, bool bSetupUndo, bool bPrintPythonCommands, bool bFixEulerFlips)
{
	FTransform GlobalTransform = InGlobalTransform;
	ERigTransformType::Type TransformType = ERigTransformType::CurrentGlobal;
	if (IsConstructionModeEnabled())
	{
#if WITH_EDITOR
		if(bRecordSelectionPoseForConstructionMode)
		{
			SelectionPoseForConstructionMode.FindOrAdd(FRigElementKey(InControlName, ERigElementType::Control)) = GlobalTransform;
		}
#endif
		TransformType = ERigTransformType::InitialGlobal;
		GlobalTransform = SetupControlFromGlobalTransform(InControlName, GlobalTransform);
	}

	FRigControlValue Value = GetControlValueFromGlobalTransform(InControlName, GlobalTransform, TransformType);
	if (OnFilterControl.IsBound())
	{
		FRigControlElement* Control = FindControl(InControlName);
		if (Control)
		{
			OnFilterControl.Broadcast(this, Control, Value);
		}
	}

	SetControlValue(InControlName, Value, bNotify, Context, bSetupUndo, bPrintPythonCommands, bFixEulerFlips);
	return true;
}

FRigControlValue UControlRig::GetControlValueFromGlobalTransform(const FName& InControlName, const FTransform& InGlobalTransform, ERigTransformType::Type InTransformType)
{
	FRigControlValue Value;

	if (FRigControlElement* ControlElement = FindControl(InControlName))
	{
		if(DynamicHierarchy)
		{
			FTransform Transform = DynamicHierarchy->ComputeLocalControlValue(ControlElement, InGlobalTransform, InTransformType);
			Value.SetFromTransform(Transform, ControlElement->Settings.ControlType, ControlElement->Settings.PrimaryAxis);

			if (ShouldApplyLimits())
			{
				ControlElement->Settings.ApplyLimits(Value);
			}
		}
	}

	return Value;
}

void UControlRig::SetControlLocalTransform(const FName& InControlName, const FTransform& InLocalTransform, bool bNotify, const FRigControlModifiedContext& Context, bool bSetupUndo, bool bFixEulerFlips)
{
	if (FRigControlElement* ControlElement = FindControl(InControlName))
	{
		FRigControlValue Value;
		Value.SetFromTransform(InLocalTransform, ControlElement->Settings.ControlType, ControlElement->Settings.PrimaryAxis);

		if (OnFilterControl.IsBound())
		{
			OnFilterControl.Broadcast(this, ControlElement, Value);
			
		}
		SetControlValue(InControlName, Value, bNotify, Context, bSetupUndo, false /*bPrintPython not defined!*/, bFixEulerFlips);
	}
}

FTransform UControlRig::GetControlLocalTransform(const FName& InControlName)
{
	if(DynamicHierarchy == nullptr)
	{
		return FTransform::Identity;
	}
	return DynamicHierarchy->GetLocalTransform(FRigElementKey(InControlName, ERigElementType::Control));
}

const TArray<TSoftObjectPtr<UControlRigShapeLibrary>>& UControlRig::GetShapeLibraries() const
{
	const TArray<TSoftObjectPtr<UControlRigShapeLibrary>>* LibrariesPtr = &ShapeLibraries;

	if(!GetClass()->IsNative() && ShapeLibraries.IsEmpty())
	{
		if (UControlRig* CDO = Cast<UControlRig>(GetClass()->GetDefaultObject()))
		{
			LibrariesPtr = &CDO->ShapeLibraries;
		}
	}

	const TArray<TSoftObjectPtr<UControlRigShapeLibrary>>& Libraries = *LibrariesPtr;
	for(const TSoftObjectPtr<UControlRigShapeLibrary>& ShapeLibrary : Libraries)
	{
		if (!ShapeLibrary.IsValid())
		{
			ShapeLibrary.LoadSynchronous();
		}
	}

	return Libraries;
}

void UControlRig::SelectControl(const FName& InControlName, bool bSelect)
{
	if(DynamicHierarchy)
	{
		if(URigHierarchyController* Controller = DynamicHierarchy->GetController(true))
		{
			Controller->SelectElement(FRigElementKey(InControlName, ERigElementType::Control), bSelect);
		}
	}
}

bool UControlRig::ClearControlSelection()
{
	if(DynamicHierarchy)
	{
		if(URigHierarchyController* Controller = DynamicHierarchy->GetController(true))
		{
			return Controller->ClearSelection();
		}
	}
	return false;
}

TArray<FName> UControlRig::CurrentControlSelection() const
{
	TArray<FName> SelectedControlNames;

	if(DynamicHierarchy)
	{
		TArray<const FRigBaseElement*> SelectedControls = DynamicHierarchy->GetSelectedElements(ERigElementType::Control);
		for (const FRigBaseElement* SelectedControl : SelectedControls)
		{
			SelectedControlNames.Add(SelectedControl->GetName());
		}
	}
	return SelectedControlNames;
}

bool UControlRig::IsControlSelected(const FName& InControlName)const
{
	if(DynamicHierarchy)
	{
		if(FRigControlElement* ControlElement = FindControl(InControlName))
		{
			return DynamicHierarchy->IsSelected(ControlElement);
		}
	}
	return false;
}

void UControlRig::HandleHierarchyModified(ERigHierarchyNotification InNotification, URigHierarchy* InHierarchy,
    const FRigBaseElement* InElement)
{
	switch(InNotification)
	{
		case ERigHierarchyNotification::ElementSelected:
		case ERigHierarchyNotification::ElementDeselected:
		{
			if(FRigControlElement* ControlElement = Cast<FRigControlElement>((FRigBaseElement*)InElement))
			{
				const bool bSelected = InNotification == ERigHierarchyNotification::ElementSelected;
				ControlSelected().Broadcast(this, ControlElement, bSelected);

				OnControlSelected_BP.Broadcast(this, *ControlElement, bSelected);
			}

#if WITH_EDITOR
			if(IsConstructionModeEnabled())
			{
				if(InElement->GetType() == ERigElementType::Control)
				{
					if(InNotification == ERigHierarchyNotification::ElementSelected)
					{
						SelectionPoseForConstructionMode.FindOrAdd(InElement->GetKey()) = GetHierarchy()->GetGlobalTransform(InElement->GetKey());
					}
					else
					{
						SelectionPoseForConstructionMode.Remove(InElement->GetKey());
					}
				}

				if(InNotification == ERigHierarchyNotification::ElementDeselected)
				{
					ClearTransientControls();
				}
			}
#endif
			break;
		}
		case ERigHierarchyNotification::ControlSettingChanged:
		case ERigHierarchyNotification::ControlShapeTransformChanged:
		{
			if(FRigControlElement* ControlElement = Cast<FRigControlElement>((FRigBaseElement*)InElement))
			{
				ControlModified().Broadcast(this, ControlElement, FRigControlModifiedContext(EControlRigSetKey::Never));
			}
			break;
		}
		default:
		{
			break;
		}
	}
}

#if WITH_EDITOR

FName UControlRig::AddTransientControl(URigVMPin* InPin, FRigElementKey SpaceKey, FTransform OffsetTransform)
{
	if ((InPin == nullptr) || (DynamicHierarchy == nullptr))
	{
		return NAME_None;
	}

	if (InPin->GetCPPType() != TEXT("FVector") &&
		InPin->GetCPPType() != TEXT("FQuat") &&
		InPin->GetCPPType() != TEXT("FTransform"))
	{
		return NAME_None;
	}

	RemoveTransientControl(InPin);

	URigHierarchyController* Controller = DynamicHierarchy->GetController(true);
	if(Controller == nullptr)
	{
		return NAME_None;
	}

	URigVMPin* PinForLink = InPin->GetPinForLink();

	const FName ControlName = GetNameForTransientControl(InPin);
	FTransform ShapeTransform = FTransform::Identity;
	ShapeTransform.SetScale3D(FVector::ZeroVector);

	FRigControlSettings Settings;
	Settings.ControlType = ERigControlType::Transform;
	if (URigVMPin* ColorPin = PinForLink->GetNode()->FindPin(TEXT("Color")))
	{
		if (ColorPin->GetCPPType() == TEXT("FLinearColor"))
		{
			FRigControlValue Value;
			Settings.ShapeColor = Value.SetFromString<FLinearColor>(ColorPin->GetDefaultValue());
		}
	}
	Settings.bIsTransientControl = true;
	Settings.DisplayName = TEXT("Temporary Control");

	Controller->ClearSelection();

    const FRigElementKey ControlKey = Controller->AddControl(
    	ControlName,
    	SpaceKey,
    	Settings,
    	FRigControlValue::Make(FTransform::Identity),
    	OffsetTransform,
    	ShapeTransform, false);

	SetTransientControlValue(InPin);

	return ControlName;
}

bool UControlRig::SetTransientControlValue(URigVMPin* InPin)
{
	const FName ControlName = GetNameForTransientControl(InPin);
	if (FRigControlElement* ControlElement = FindControl(ControlName))
	{
		FString DefaultValue = InPin->GetPinForLink()->GetDefaultValue();
		if (!DefaultValue.IsEmpty())
		{
			if (InPin->GetCPPType() == TEXT("FVector"))
			{
				ControlElement->Settings.ControlType = ERigControlType::Position;
				FRigControlValue Value;
				Value.SetFromString<FVector>(DefaultValue);
				DynamicHierarchy->SetControlValue(ControlElement, Value, ERigControlValueType::Current, false);
			}
			else if (InPin->GetCPPType() == TEXT("FQuat"))
			{
				ControlElement->Settings.ControlType = ERigControlType::Rotator;
				FRigControlValue Value;
				Value.SetFromString<FRotator>(DefaultValue);
				DynamicHierarchy->SetControlValue(ControlElement, Value, ERigControlValueType::Current, false);
			}
			else
			{
				ControlElement->Settings.ControlType = ERigControlType::Transform;
				FRigControlValue Value;
				Value.SetFromString<FTransform>(DefaultValue);
				DynamicHierarchy->SetControlValue(ControlElement, Value, ERigControlValueType::Current, false);
			}
		}
		return true;
	}
	return false;
}

FName UControlRig::RemoveTransientControl(URigVMPin* InPin)
{
	if ((InPin == nullptr) || (DynamicHierarchy == nullptr))
	{
		return NAME_None;
	}

	URigHierarchyController* Controller = DynamicHierarchy->GetController(true);
	if(Controller == nullptr)
	{
		return NAME_None;
	}

	const FName ControlName = GetNameForTransientControl(InPin);
	if(FRigControlElement* ControlElement = FindControl(ControlName))
	{
		DynamicHierarchy->Notify(ERigHierarchyNotification::ElementDeselected, ControlElement);
		if(Controller->RemoveElement(ControlElement))
		{
#if WITH_EDITOR
			SelectionPoseForConstructionMode.Remove(FRigElementKey(ControlName, ERigElementType::Control));
#endif
			return ControlName;
		}
	}

	return NAME_None;
}

FName UControlRig::AddTransientControl(const FRigElementKey& InElement)
{
	if (!InElement.IsValid())
	{
		return NAME_None;
	}

	if(DynamicHierarchy == nullptr)
	{
		return NAME_None;
	}

	URigHierarchyController* Controller = DynamicHierarchy->GetController(true);
	if(Controller == nullptr)
	{
		return NAME_None;
	}

	const FName ControlName = GetNameForTransientControl(InElement);
	if(DynamicHierarchy->Contains(FRigElementKey(ControlName, ERigElementType::Control)))
	{
		SetTransientControlValue(InElement);
		return ControlName;
	}

	const int32 ElementIndex = DynamicHierarchy->GetIndex(InElement);
	if (ElementIndex == INDEX_NONE)
	{
		return NAME_None;
	}

	FTransform ShapeTransform = FTransform::Identity;
	ShapeTransform.SetScale3D(FVector::ZeroVector);

	FRigControlSettings Settings;
	Settings.ControlType = ERigControlType::Transform;
	Settings.bIsTransientControl = true;
	Settings.DisplayName = TEXT("Temporary Control");

	FRigElementKey Parent;
	switch (InElement.Type)
	{
		case ERigElementType::Bone:
		{
			Parent = DynamicHierarchy->GetFirstParent(InElement);
			break;
		}
		case ERigElementType::Null:
		{
			Parent = InElement;
			break;
		}
		default:
		{
			break;
		}
	}

	const FRigElementKey ControlKey = Controller->AddControl(
        ControlName,
        Parent,
        Settings,
        FRigControlValue::Make(FTransform::Identity),
        FTransform::Identity,
        ShapeTransform, false);

	if (InElement.Type == ERigElementType::Bone)
	{
		// don't allow transient control to modify forward mode poses when we
		// already switched to the construction mode
		if (!IsConstructionModeEnabled())
		{
			if(FRigBoneElement* BoneElement = DynamicHierarchy->Find<FRigBoneElement>(InElement))
			{
				// add a modify bone AnimNode internally that the transient control controls for imported bones only
				// for user created bones, refer to UControlRig::TransformOverrideForUserCreatedBones 
				if (BoneElement->BoneType == ERigBoneType::Imported)
				{ 
					if (PreviewInstance)
					{
						PreviewInstance->ModifyBone(InElement.Name);
					}
				}
				else if (BoneElement->BoneType == ERigBoneType::User)
				{
					// add an empty entry, which will be given the correct value in
					// SetTransientControlValue(InElement);
					TransformOverrideForUserCreatedBones.FindOrAdd(InElement.Name);
				}
			}
		}
	}

	SetTransientControlValue(InElement);

	return ControlName;
}

bool UControlRig::SetTransientControlValue(const FRigElementKey& InElement)
{
	if (!InElement.IsValid())
	{
		return false;
	}

	if(DynamicHierarchy == nullptr)
	{
		return false;
	}

	const FName ControlName = GetNameForTransientControl(InElement);
	if(FRigControlElement* ControlElement = FindControl(ControlName))
	{
		if (InElement.Type == ERigElementType::Bone)
		{
			if (IsConstructionModeEnabled())
			{
				// need to get initial because that is what construction mode uses
				// specifically, when user change the initial from the details panel
				// this will allow the transient control to react to that change
				const FTransform InitialLocalTransform = DynamicHierarchy->GetInitialLocalTransform(InElement);
				DynamicHierarchy->SetTransform(ControlElement, InitialLocalTransform, ERigTransformType::InitialLocal, true, false);
				DynamicHierarchy->SetTransform(ControlElement, InitialLocalTransform, ERigTransformType::CurrentLocal, true, false);
			}
			else
			{
				const FTransform LocalTransform = DynamicHierarchy->GetLocalTransform(InElement);
				DynamicHierarchy->SetTransform(ControlElement, LocalTransform, ERigTransformType::InitialLocal, true, false);
				DynamicHierarchy->SetTransform(ControlElement, LocalTransform, ERigTransformType::CurrentLocal, true, false);

				if (FRigBoneElement* BoneElement = DynamicHierarchy->Find<FRigBoneElement>(InElement))
				{
					if (BoneElement->BoneType == ERigBoneType::Imported)
					{
						if (PreviewInstance)
						{
							if (FAnimNode_ModifyBone* Modify = PreviewInstance->FindModifiedBone(InElement.Name))
							{
								Modify->Translation = LocalTransform.GetTranslation();
								Modify->Rotation = LocalTransform.GetRotation().Rotator();
								Modify->TranslationSpace = EBoneControlSpace::BCS_ParentBoneSpace;
								Modify->RotationSpace = EBoneControlSpace::BCS_ParentBoneSpace;
							}
						}	
					}
					else if (BoneElement->BoneType == ERigBoneType::User)
					{
						if (FTransform* TransformOverride = TransformOverrideForUserCreatedBones.Find(InElement.Name))
						{
							*TransformOverride = LocalTransform;
						}	
					}
				}
			}
		}
		else if (InElement.Type == ERigElementType::Null)
		{
			const FTransform GlobalTransform = DynamicHierarchy->GetGlobalTransform(InElement);
			DynamicHierarchy->SetTransform(ControlElement, GlobalTransform, ERigTransformType::InitialGlobal, true, false);
			DynamicHierarchy->SetTransform(ControlElement, GlobalTransform, ERigTransformType::CurrentGlobal, true, false);
		}

		return true;
	}
	return false;
}

FName UControlRig::RemoveTransientControl(const FRigElementKey& InElement)
{
	if (!InElement.IsValid())
	{
		return NAME_None;
	}

	if(DynamicHierarchy == nullptr)
	{
		return NAME_None;
	}

	URigHierarchyController* Controller = DynamicHierarchy->GetController(true);
	if(Controller == nullptr)
	{
		return NAME_None;
	}

	const FName ControlName = GetNameForTransientControl(InElement);
	if(FRigControlElement* ControlElement = FindControl(ControlName))
	{
		DynamicHierarchy->Notify(ERigHierarchyNotification::ElementDeselected, ControlElement);
		if(Controller->RemoveElement(ControlElement))
		{
#if WITH_EDITOR
			SelectionPoseForConstructionMode.Remove(FRigElementKey(ControlName, ERigElementType::Control));
#endif
			return ControlName;
		}
	}

	return NAME_None;
}

FName UControlRig::GetNameForTransientControl(URigVMPin* InPin) const
{
	check(InPin);
	check(DynamicHierarchy);
	
	const FString OriginalPinPath = InPin->GetOriginalPinFromInjectedNode()->GetPinPath();
	return DynamicHierarchy->GetSanitizedName(FString::Printf(TEXT("ControlForPin_%s"), *OriginalPinPath));
}

FString UControlRig::GetPinNameFromTransientControl(const FRigElementKey& InKey)
{
	FString Name = InKey.Name.ToString();
	if(Name.StartsWith(TEXT("ControlForPin_")))
	{
		Name.RightChopInline(14);
	}
	return Name;
}

FName UControlRig::GetNameForTransientControl(const FRigElementKey& InElement)
{
	if (InElement.Type == ERigElementType::Control)
	{
		return InElement.Name;
	}

	const FName EnumName = *StaticEnum<ERigElementType>()->GetDisplayNameTextByValue((int64)InElement.Type).ToString();
	return *FString::Printf(TEXT("ControlForRigElement_%s_%s"), *EnumName.ToString(), *InElement.Name.ToString());
}

FRigElementKey UControlRig::GetElementKeyFromTransientControl(const FRigElementKey& InKey)
{
	if(InKey.Type != ERigElementType::Control)
	{
		return FRigElementKey();
	}
	
	static FString ControlRigForElementBoneName;
	static FString ControlRigForElementNullName;

	if (ControlRigForElementBoneName.IsEmpty())
	{
		ControlRigForElementBoneName = FString::Printf(TEXT("ControlForRigElement_%s_"),
            *StaticEnum<ERigElementType>()->GetDisplayNameTextByValue((int64)ERigElementType::Bone).ToString());
		ControlRigForElementNullName = FString::Printf(TEXT("ControlForRigElement_%s_"),
            *StaticEnum<ERigElementType>()->GetDisplayNameTextByValue((int64)ERigElementType::Null).ToString());
	}
	
	FString Name = InKey.Name.ToString();
	if(Name.StartsWith(ControlRigForElementBoneName))
	{
		Name.RightChopInline(ControlRigForElementBoneName.Len());
		return FRigElementKey(*Name, ERigElementType::Bone);
	}
	if(Name.StartsWith(ControlRigForElementNullName))
	{
		Name.RightChopInline(ControlRigForElementNullName.Len());
		return FRigElementKey(*Name, ERigElementType::Null);
	}
	
	return FRigElementKey();;
}

void UControlRig::ClearTransientControls()
{
	if(DynamicHierarchy == nullptr)
	{
		return;
	}

	URigHierarchyController* Controller = DynamicHierarchy->GetController(true);
	if(Controller == nullptr)
	{
		return;
	}

	if(bIsClearingTransientControls)
	{
		return;
	}
	TGuardValue<bool> ReEntryGuard(bIsClearingTransientControls, true);

	const TArray<FRigControlElement*> ControlsToRemove = DynamicHierarchy->GetTransientControls();
	for (FRigControlElement* ControlToRemove : ControlsToRemove)
	{
		const FRigElementKey KeyToRemove = ControlToRemove->GetKey();
		if(Controller->RemoveElement(ControlToRemove))
		{
#if WITH_EDITOR
			SelectionPoseForConstructionMode.Remove(KeyToRemove);
#endif
		}
	}
}

void UControlRig::ApplyTransformOverrideForUserCreatedBones()
{
	if(DynamicHierarchy == nullptr)
	{
		return;
	}
	
	for (const auto& Entry : TransformOverrideForUserCreatedBones)
	{
		DynamicHierarchy->SetLocalTransform(FRigElementKey(Entry.Key, ERigElementType::Bone), Entry.Value, false);
	}
}

void UControlRig::ApplySelectionPoseForConstructionMode(const FName& InEventName)
{
	FRigControlModifiedContext ControlValueContext;
	ControlValueContext.EventName = InEventName;

	TGuardValue<bool> DisableRecording(bRecordSelectionPoseForConstructionMode, false);
	for(const TPair<FRigElementKey, FTransform>& Pair : SelectionPoseForConstructionMode)
	{
		const FName ControlName = Pair.Key.Name;
		const FRigControlValue Value = GetControlValueFromGlobalTransform(ControlName, Pair.Value, ERigTransformType::InitialGlobal);
		SetControlValue(ControlName, Value, true, ControlValueContext, false, false, false);
	}
}

#endif

void UControlRig::HandleHierarchyEvent(URigHierarchy* InHierarchy, const FRigEventContext& InEvent)
{
	if (RigEventDelegate.IsBound())
	{
		RigEventDelegate.Broadcast(InHierarchy, InEvent);
	}

	switch (InEvent.Event)
	{
		case ERigEvent::RequestAutoKey:
		{
			int32 Index = InHierarchy->GetIndex(InEvent.Key);
			if (Index != INDEX_NONE && InEvent.Key.Type == ERigElementType::Control)
			{
				if(FRigControlElement* ControlElement = InHierarchy->GetChecked<FRigControlElement>(Index))
				{
					FRigControlModifiedContext Context;
					Context.SetKey = EControlRigSetKey::Always;
					Context.LocalTime = InEvent.LocalTime;
					Context.EventName = InEvent.SourceEventName;
					ControlModified().Broadcast(this, ControlElement, Context);
				}
			}
			break;
		}
		case ERigEvent::OpenUndoBracket:
		case ERigEvent::CloseUndoBracket:
		{
			const bool bOpenUndoBracket = InEvent.Event == ERigEvent::OpenUndoBracket;
			ControlUndoBracketIndex = FMath::Max<int32>(0, ControlUndoBracketIndex + (bOpenUndoBracket ? 1 : -1));
			ControlUndoBracket().Broadcast(this, bOpenUndoBracket);
			break;
		}
		default:
		{
			break;
		}
	}
}

void UControlRig::GetControlsInOrder(TArray<FRigControlElement*>& SortedControls) const
{
	SortedControls.Reset();

	if(DynamicHierarchy == nullptr)
	{
		return;
	}

	SortedControls = DynamicHierarchy->GetControls(true);
}

const FRigInfluenceMap* UControlRig::FindInfluenceMap(const FName& InEventName)
{
	if (UControlRig* CDO = GetClass()->GetDefaultObject<UControlRig>())
	{
		return CDO->Influences.Find(InEventName);
	}
	return nullptr;
}

void UControlRig::SetInteractionRig(UControlRig* InInteractionRig)
{
	if (InteractionRig == InInteractionRig)
	{
		return;
	}

	if (InteractionRig != nullptr)
	{
		InteractionRig->ControlModified().RemoveAll(this);
		InteractionRig->OnInitialized_AnyThread().RemoveAll(this);
		InteractionRig->OnExecuted_AnyThread().RemoveAll(this);
		InteractionRig->ControlSelected().RemoveAll(this);
		OnInitialized_AnyThread().RemoveAll(InteractionRig);
		OnExecuted_AnyThread().RemoveAll(InteractionRig);
		ControlSelected().RemoveAll(InteractionRig);
	}

	InteractionRig = InInteractionRig;

	if (InteractionRig != nullptr)
	{
		SetInteractionRigClass(InteractionRig->GetClass());

		InteractionRig->Initialize(true);
		InteractionRig->CopyPoseFromOtherRig(this);
		InteractionRig->RequestConstruction();
		InteractionRig->Execute(FRigUnit_BeginExecution::EventName);

		InteractionRig->ControlModified().AddUObject(this, &UControlRig::HandleInteractionRigControlModified);
		InteractionRig->OnInitialized_AnyThread().AddUObject(this, &UControlRig::HandleInteractionRigInitialized);
		InteractionRig->OnExecuted_AnyThread().AddUObject(this, &UControlRig::HandleInteractionRigExecuted);
		InteractionRig->ControlSelected().AddUObject(this, &UControlRig::HandleInteractionRigControlSelected, false);
		OnInitialized_AnyThread().AddUObject(ToRawPtr(InteractionRig), &UControlRig::HandleInteractionRigInitialized);
		OnExecuted_AnyThread().AddUObject(ToRawPtr(InteractionRig), &UControlRig::HandleInteractionRigExecuted);
		ControlSelected().AddUObject(ToRawPtr(InteractionRig), &UControlRig::HandleInteractionRigControlSelected, true);

		FControlRigBracketScope BracketScope(InterRigSyncBracket);
		InteractionRig->HandleInteractionRigExecuted(this, FRigUnit_BeginExecution::EventName);
	}
}

void UControlRig::SetInteractionRigClass(TSubclassOf<UControlRig> InInteractionRigClass)
{
	if (InteractionRigClass == InInteractionRigClass)
	{
		return;
	}

	InteractionRigClass = InInteractionRigClass;

	if(InteractionRigClass)
	{
		if(InteractionRig != nullptr)
		{
			if(InteractionRig->GetClass() != InInteractionRigClass)
			{
				SetInteractionRig(nullptr);
			}
		}

		if(InteractionRig == nullptr)
		{
			UControlRig* NewInteractionRig = NewObject<UControlRig>(this, InteractionRigClass);
			SetInteractionRig(NewInteractionRig);
		}
	}
}

#if WITH_EDITOR

void UControlRig::PreEditChange(FProperty* PropertyAboutToChange)
{
	// for BP user authored properties let's ignore changes since they
	// will be distributed from the BP anyway to all archetype instances.
	if(PropertyAboutToChange && !PropertyAboutToChange->IsNative())
	{
		if(const UBlueprint* Blueprint = Cast<UBlueprint>(GetClass()->ClassGeneratedBy))
		{
			const struct FBPVariableDescription* FoundVariable = Blueprint->NewVariables.FindByPredicate(
				[PropertyAboutToChange](const struct FBPVariableDescription& NewVariable)
				{
					return NewVariable.VarName == PropertyAboutToChange->GetFName();
				}
			);

			if(FoundVariable)
			{
				return;
			}
		}
	}
	
	Super::PreEditChange(PropertyAboutToChange);

	if (PropertyAboutToChange && PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRig, InteractionRig))
	{
		SetInteractionRig(nullptr);
	}
	else if (PropertyAboutToChange && PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRig, InteractionRigClass))
	{
		SetInteractionRigClass(nullptr);
	}
}

void UControlRig::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRig, InteractionRig))
	{
		UControlRig* NewInteractionRig = InteractionRig;
		SetInteractionRig(nullptr);
		SetInteractionRig(NewInteractionRig);
	}
	else if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRig, InteractionRigClass))
	{
		TSubclassOf<UControlRig> NewInteractionRigClass = InteractionRigClass;
		SetInteractionRigClass(nullptr);
		SetInteractionRigClass(NewInteractionRigClass);
		if (NewInteractionRigClass == nullptr)
		{
			SetInteractionRig(nullptr);
		}
	}
}
#endif

const TArray<UAssetUserData*>* UControlRig::GetAssetUserDataArray() const
{
	if(HasAnyFlags(RF_ClassDefaultObject))
	{
		return &ToRawPtrTArrayUnsafe(AssetUserData);
	}

	CombinedAssetUserData.Reset();
	CombinedAssetUserData.Append(AssetUserData);

	if(OuterSceneComponent.IsValid())
	{
		if(const USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(OuterSceneComponent))
		{
			if(USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMeshAsset())
			{
				if(const USkeleton* Skeleton = SkeletalMesh->GetSkeleton())
				{
					if(const TArray<UAssetUserData*>* SkeletonUserDataArray = Skeleton->GetAssetUserDataArray())
					{
						for(UAssetUserData* SkeletonUserData : *SkeletonUserDataArray)
						{
							CombinedAssetUserData.Add(SkeletonUserData);
						}
					}
				}
				if(const TArray<UAssetUserData*>* SkeletalMeshUserDataArray = SkeletalMesh->GetAssetUserDataArray())
				{
					for(UAssetUserData* SkeletalMeshUserData : *SkeletalMeshUserDataArray)
					{
						CombinedAssetUserData.Add(SkeletalMeshUserData);
					}
				}
			}
		}
	}

	return &ToRawPtrTArrayUnsafe(CombinedAssetUserData);
}

void UControlRig::CopyPoseFromOtherRig(UControlRig* Subject)
{
	check(DynamicHierarchy);
	check(Subject);
	URigHierarchy* OtherHierarchy = Subject->GetHierarchy();
	check(OtherHierarchy);

	for (FRigBaseElement* Element : *DynamicHierarchy)
	{
		FRigBaseElement* OtherElement = OtherHierarchy->Find(Element->GetKey());
		if(OtherElement == nullptr)
		{
			continue;
		}

		if(OtherElement->GetType() != Element->GetType())
		{
			continue;
		}

		if(FRigBoneElement* BoneElement = Cast<FRigBoneElement>(Element))
		{
			FRigBoneElement* OtherBoneElement = CastChecked<FRigBoneElement>(OtherElement);
			const FTransform Transform = OtherHierarchy->GetTransform(OtherBoneElement, ERigTransformType::CurrentLocal);
			DynamicHierarchy->SetTransform(BoneElement, Transform, ERigTransformType::CurrentLocal, true, false);
		}
		else if(FRigCurveElement* CurveElement = Cast<FRigCurveElement>(Element))
		{
			FRigCurveElement* OtherCurveElement = CastChecked<FRigCurveElement>(OtherElement);
			const float Value = OtherHierarchy->GetCurveValue(OtherCurveElement);
			DynamicHierarchy->SetCurveValue(CurveElement, Value, false);
		}
	}
}

void UControlRig::HandleInteractionRigControlModified(UControlRig* Subject, FRigControlElement* Control, const FRigControlModifiedContext& Context)
{
	check(Subject);

	if (IsSyncingWithOtherRig() || IsExecuting())
	{
		return;
	}
	FControlRigBracketScope BracketScope(InterRigSyncBracket);

	if (Subject != InteractionRig)
	{
		return;
	}

	if (const FRigInfluenceMap* InfluenceMap = Subject->FindInfluenceMap(Context.EventName))
	{
		if (const FRigInfluenceEntry* InfluenceEntry = InfluenceMap->Find(Control->GetKey()))
		{
			for (const FRigElementKey& AffectedKey : *InfluenceEntry)
			{
				if (AffectedKey.Type == ERigElementType::Control)
				{
					if (FRigControlElement* AffectedControl = FindControl(AffectedKey.Name))
					{
						QueuedModifiedControls.Add(AffectedControl->GetKey());
					}
				}
				else if (AffectedKey.Type == ERigElementType::Bone)
				{
					// special case controls with a CONTROL suffix
					FName BoneControlName = *FString::Printf(TEXT("%s_CONTROL"), *AffectedKey.Name.ToString());
					if (FRigControlElement* AffectedControl = FindControl(BoneControlName))
					{
						QueuedModifiedControls.Add(AffectedControl->GetKey());
					}
				}
				else if(AffectedKey.Type == ERigElementType::Curve)
				{
					// special case controls with a CONTROL suffix
					FName CurveControlName = *FString::Printf(TEXT("%s_CURVE_CONTROL"), *AffectedKey.Name.ToString());
					if (FRigControlElement* AffectedControl = FindControl(CurveControlName))
					{
						QueuedModifiedControls.Add(AffectedControl->GetKey());
					}
				}
			}
		}
	}

}

void UControlRig::HandleInteractionRigInitialized(URigVMHost* Subject, const FName& EventName)
{
	check(Subject);

	if (IsSyncingWithOtherRig())
	{
		return;
	}
	FControlRigBracketScope BracketScope(InterRigSyncBracket);
	RequestInit();
}

void UControlRig::HandleInteractionRigExecuted(URigVMHost* Subject, const FName& EventName)
{
	check(Subject);
	UControlRig* SubjectRig = CastChecked<UControlRig>(Subject);

	if (IsSyncingWithOtherRig() || IsExecuting())
	{
		return;
	}
	FControlRigBracketScope BracketScope(InterRigSyncBracket);

	CopyPoseFromOtherRig(SubjectRig);
	Execute(FRigUnit_InverseExecution::EventName);

	FRigControlModifiedContext Context;
	Context.EventName = FRigUnit_InverseExecution::EventName;
	Context.SetKey = EControlRigSetKey::DoNotCare;

	for (const FRigElementKey& QueuedModifiedControl : QueuedModifiedControls)
	{
		if(FRigControlElement* ControlElement = FindControl(QueuedModifiedControl.Name))
		{
			ControlModified().Broadcast(this, ControlElement, Context);
		}
	}
}

void UControlRig::HandleInteractionRigControlSelected(UControlRig* Subject, FRigControlElement* Control, bool bSelected, bool bInverted)
{
	check(Subject);

	if (IsSyncingWithOtherRig() || IsExecuting())
	{
		return;
	}
	if (Subject->IsSyncingWithOtherRig() || Subject->IsExecuting())
	{
		return;
	}
	FControlRigBracketScope BracketScope(InterRigSyncBracket);

	const FRigInfluenceMap* InfluenceMap = nullptr;
	if (bInverted)
	{
		InfluenceMap = FindInfluenceMap(FRigUnit_BeginExecution::EventName);
	}
	else
	{
		InfluenceMap = Subject->FindInfluenceMap(FRigUnit_BeginExecution::EventName);
	}

	if (InfluenceMap)
	{
		FRigInfluenceMap InvertedMap;
		if (bInverted)
		{
			InvertedMap = InfluenceMap->Inverse();
			InfluenceMap = &InvertedMap;
		}

		struct Local
		{
			static void SelectAffectedElements(UControlRig* ThisRig, const FRigInfluenceMap* InfluenceMap, const FRigElementKey& InKey, bool bSelected, bool bInverted)
			{
				if (const FRigInfluenceEntry* InfluenceEntry = InfluenceMap->Find(InKey))
				{
					for (const FRigElementKey& AffectedKey : *InfluenceEntry)
					{
						if (AffectedKey.Type == ERigElementType::Control)
						{
							ThisRig->SelectControl(AffectedKey.Name, bSelected);
						}

						if (bInverted)
						{
							if (AffectedKey.Type == ERigElementType::Control)
							{
								ThisRig->SelectControl(AffectedKey.Name, bSelected);
							}
						}
						else
						{
							if (AffectedKey.Type == ERigElementType::Control)
							{
								ThisRig->SelectControl(AffectedKey.Name, bSelected);
							}
							else if (AffectedKey.Type == ERigElementType::Bone ||
								AffectedKey.Type == ERigElementType::Curve)
							{
								FName ControlName = *FString::Printf(TEXT("%s_CONTROL"), *AffectedKey.Name.ToString());
								ThisRig->SelectControl(ControlName, bSelected);
							}
						}
					}
				}
			}
		};

		Local::SelectAffectedElements(this, InfluenceMap, Control->GetKey(), bSelected, bInverted);

		if (bInverted)
		{
			const FString ControlName = Control->GetName().ToString();
			if (ControlName.EndsWith(TEXT("_CONTROL")))
			{
				const FString BaseName = ControlName.Left(ControlName.Len() - 8);
				Local::SelectAffectedElements(this, InfluenceMap, FRigElementKey(*BaseName, ERigElementType::Bone), bSelected, bInverted);
				Local::SelectAffectedElements(this, InfluenceMap, FRigElementKey(*BaseName, ERigElementType::Curve), bSelected, bInverted);
			}
		}
	}
}

void UControlRig::SetBoneInitialTransformsFromAnimInstance(UAnimInstance* InAnimInstance)
{
	FMemMark Mark(FMemStack::Get());
	FCompactPose OutPose;
	OutPose.ResetToRefPose(InAnimInstance->GetRequiredBones());
	SetBoneInitialTransformsFromCompactPose(&OutPose);
}

void UControlRig::SetBoneInitialTransformsFromAnimInstanceProxy(const FAnimInstanceProxy* InAnimInstanceProxy)
{
	FMemMark Mark(FMemStack::Get());
	FCompactPose OutPose;
	OutPose.ResetToRefPose(InAnimInstanceProxy->GetRequiredBones());
	SetBoneInitialTransformsFromCompactPose(&OutPose);
}

void UControlRig::SetBoneInitialTransformsFromSkeletalMeshComponent(USkeletalMeshComponent* InSkelMeshComp, bool bUseAnimInstance)
{
	check(InSkelMeshComp);
	check(DynamicHierarchy);
	
	if (!bUseAnimInstance && (InSkelMeshComp->GetAnimInstance() != nullptr))
	{
		SetBoneInitialTransformsFromAnimInstance(InSkelMeshComp->GetAnimInstance());
	}
	else
	{
		SetBoneInitialTransformsFromSkeletalMesh(InSkelMeshComp->GetSkeletalMeshAsset());
	}
}


void UControlRig::SetBoneInitialTransformsFromSkeletalMesh(USkeletalMesh* InSkeletalMesh)
{
	if (ensure(InSkeletalMesh))
	{ 
		SetBoneInitialTransformsFromRefSkeleton(InSkeletalMesh->GetRefSkeleton());
	}
}

void UControlRig::SetBoneInitialTransformsFromRefSkeleton(const FReferenceSkeleton& InReferenceSkeleton)
{
	check(DynamicHierarchy);

	DynamicHierarchy->ForEach<FRigBoneElement>([this, InReferenceSkeleton](FRigBoneElement* BoneElement) -> bool
	{
		if(BoneElement->BoneType == ERigBoneType::Imported)
		{
			const int32 BoneIndex = InReferenceSkeleton.FindBoneIndex(BoneElement->GetName());
			if (BoneIndex != INDEX_NONE)
			{
				const FTransform LocalInitialTransform = InReferenceSkeleton.GetRefBonePose()[BoneIndex];
				DynamicHierarchy->SetTransform(BoneElement, LocalInitialTransform, ERigTransformType::InitialLocal, true, false);
				DynamicHierarchy->SetTransform(BoneElement, LocalInitialTransform, ERigTransformType::CurrentLocal, true, false);
			}
		}
		return true;
	});
	bResetInitialTransformsBeforeConstruction = false;
	RequestConstruction();
}

void UControlRig::SetBoneInitialTransformsFromCompactPose(FCompactPose* InCompactPose)
{
	check(InCompactPose);

	if(!InCompactPose->IsValid())
	{
		return;
	}
	if(!InCompactPose->GetBoneContainer().IsValid())
	{
		return;
	}
	
	FMemMark Mark(FMemStack::Get());

	DynamicHierarchy->ForEach<FRigBoneElement>([this, InCompactPose](FRigBoneElement* BoneElement) -> bool
		{
			if (BoneElement->BoneType == ERigBoneType::Imported)
			{
				int32 MeshIndex = InCompactPose->GetBoneContainer().GetPoseBoneIndexForBoneName(BoneElement->GetName());
				if (MeshIndex != INDEX_NONE)
				{
					FCompactPoseBoneIndex CPIndex = InCompactPose->GetBoneContainer().MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
					if (CPIndex != INDEX_NONE)
					{
						FTransform LocalInitialTransform = InCompactPose->GetRefPose(CPIndex);
						DynamicHierarchy->SetTransform(BoneElement, LocalInitialTransform, ERigTransformType::InitialLocal, true, false);
					}
				}
			}
			return true;
		});

	bResetInitialTransformsBeforeConstruction = false;
	RequestConstruction();
}

const FRigControlElementCustomization* UControlRig::GetControlCustomization(const FRigElementKey& InControl) const
{
	check(InControl.Type == ERigElementType::Control);

	if(const FRigControlElementCustomization* Customization = ControlCustomizations.Find(InControl))
	{
		return Customization;
	}

	if(DynamicHierarchy)
	{
		if(const FRigControlElement* ControlElement = DynamicHierarchy->Find<FRigControlElement>(InControl))
		{
			return &ControlElement->Settings.Customization;
		}
	}

	return nullptr;
}

void UControlRig::SetControlCustomization(const FRigElementKey& InControl, const FRigControlElementCustomization& InCustomization)
{
	check(InControl.Type == ERigElementType::Control);
	
	ControlCustomizations.FindOrAdd(InControl) = InCustomization;
}

void UControlRig::PostInitInstanceIfRequired()
{
	if(GetHierarchy() == nullptr || VM == nullptr)
	{
		if(HasAnyFlags(RF_ClassDefaultObject))
		{
			PostInitInstance(nullptr);
		}
		else
		{
			UControlRig* CDO = GetClass()->GetDefaultObject<UControlRig>();
			PostInitInstance(CDO);
		}
	}
}

#if WITH_EDITORONLY_DATA
void UControlRig::DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass)
{
	Super::DeclareConstructClasses(OutConstructClasses, SpecificSubclass);
	OutConstructClasses.Add(FTopLevelAssetPath(URigHierarchy::StaticClass()));
	OutConstructClasses.Add(FTopLevelAssetPath(UAnimationDataSourceRegistry::StaticClass()));
}

#endif

USceneComponent* UControlRig::GetOwningSceneComponent()
{
	if(OuterSceneComponent == nullptr)
	{
		const FControlRigExecuteContext& PublicContext = GetExtendedExecuteContext().GetPublicDataSafe<FControlRigExecuteContext>();
		const FRigUnitContext& Context = PublicContext.UnitContext;

		USceneComponent* SceneComponentFromRegistry = Context.DataSourceRegistry->RequestSource<USceneComponent>(UControlRig::OwnerComponent);
		if (SceneComponentFromRegistry)
		{
			OuterSceneComponent = SceneComponentFromRegistry;
		}

		if(OuterSceneComponent == nullptr)
		{
			OuterSceneComponent = Super::GetOwningSceneComponent();
		}
	}
	return OuterSceneComponent.Get();
}

void UControlRig::PostInitInstance(URigVMHost* InCDO)
{
	const EObjectFlags SubObjectFlags =
	HasAnyFlags(RF_ClassDefaultObject) ?
		RF_Public | RF_DefaultSubObject :
		RF_Transient | RF_Transactional;

	FRigVMExtendedExecuteContext& Context = GetExtendedExecuteContext();
	// set up the VM
	VM = NewObject<URigVM>(this, TEXT("VM"), SubObjectFlags);
	Context.SetContextPublicDataStruct(FControlRigExecuteContext::StaticStruct());

	// Cooked platforms will load these pointers from disk.
	// In certain scenarios RequiresCookedData wil be false but the PKG_FilterEditorOnly will still be set (UEFN)
	if (!FPlatformProperties::RequiresCookedData() && !GetClass()->RootPackageHasAnyFlags(PKG_FilterEditorOnly))
	{
		VM->GetMemoryByType(ERigVMMemoryType::Work, true);
		VM->GetMemoryByType(ERigVMMemoryType::Literal, true);
		VM->GetMemoryByType(ERigVMMemoryType::Debug, true);
	}

	Context.ExecutionReachedExit().AddUObject(this, &UControlRig::HandleExecutionReachedExit);
	UpdateVMSettings();

	// set up the hierarchy
	DynamicHierarchy = NewObject<URigHierarchy>(this, TEXT("DynamicHierarchy"), SubObjectFlags);

#if WITH_EDITOR
	const TWeakObjectPtr<UControlRig> WeakThis = this;
	DynamicHierarchy->OnUndoRedo().AddStatic(&UControlRig::OnHierarchyTransformUndoRedoWeak, WeakThis);
#endif

	if(!HasAnyFlags(RF_ClassDefaultObject) && InCDO)
	{
		InCDO->PostInitInstanceIfRequired();

		VM->CopyFrom(InCDO->GetVM(), false, false, false, true); // we need the external properties to keep the Hash consistent with CDO
		VM->SetVMHash(VM->ComputeVMHash());
		DynamicHierarchy->CopyHierarchy(CastChecked<UControlRig>(InCDO)->GetHierarchy());

		// This has to be calculated after the copy, as the CDO memory is lazily instantiated and it affects the Hash
		const uint32 CDOVMHash = ComputeAndUpdateCDOHash(InCDO);
		Context.VMHash = CDOVMHash;

		if (VM->GetVMHash() != CDOVMHash)
		{
			UE_LOG(LogRigVM
				, Warning
				, TEXT("ControlRig : CDO VM Hash [%d] is different from calculated VM Hash [%d]. Please recompile ControlRig used at Asset : [%s]")
				, CDOVMHash
				, VM->GetVMHash()
				, *GetPathName());
		}
	}
	else // we are the CDO
	{
		// for default objects we need to check if the CDO is rooted. specialized Control Rigs
		// such as the FK control rig may not have a root since they are part of a C++ package.

		// since the sub objects are created after the constructor
		// GC won't consider them part of the CDO, even if they have the sub object flags
		// so even if CDO is rooted and references these sub objects, 
		// it is not enough to keep them alive.
		// Hence, we have to add them to root here.
		if(GetClass()->IsNative())
		{
			VM->AddToRoot();
			DynamicHierarchy->AddToRoot();
		}
	}

	RequestInit();
}

UTransformableControlHandle* UControlRig::CreateTransformableControlHandle(
	UObject* InOuter,
	const FName& InControlName) const
{
	auto IsConstrainable = [this](const FName& InControlName)
	{
		const FRigControlElement* ControlElement = FindControl(InControlName);
		if (!ControlElement)
		{
			return false;
		}
		
		const FRigControlSettings& ControlSettings = ControlElement->Settings;
		if (ControlSettings.ControlType == ERigControlType::Bool ||
			ControlSettings.ControlType == ERigControlType::Float ||
			ControlSettings.ControlType == ERigControlType::Integer)
		{
			return false;
		}

		return true;
	};

	if (!IsConstrainable(InControlName))
	{
		return nullptr;
	}
	
	UTransformableControlHandle* CtrlHandle = NewObject<UTransformableControlHandle>(InOuter, NAME_None, RF_Transactional);
	check(CtrlHandle);
	CtrlHandle->ControlRig = this;
	CtrlHandle->ControlName = InControlName;
	CtrlHandle->RegisterDelegates();
	return CtrlHandle;
}

void UControlRig::OnHierarchyTransformUndoRedo(URigHierarchy* InHierarchy, const FRigElementKey& InKey, ERigTransformType::Type InTransformType, const FTransform& InTransform, bool bIsUndo)
{
	if(InKey.Type == ERigElementType::Control)
	{
		if(FRigControlElement* ControlElement = InHierarchy->Find<FRigControlElement>(InKey))
		{
			ControlModified().Broadcast(this, ControlElement, FRigControlModifiedContext(EControlRigSetKey::Never));
		}
	}
}

UControlRig::FPoseScope::FPoseScope(UControlRig* InControlRig, ERigElementType InFilter, const TArray<FRigElementKey>& InElements, const ERigTransformType::Type InTransformType)
: ControlRig(InControlRig)
, Filter(InFilter)
, TransformType(InTransformType)
{
	check(InControlRig);
	const TArrayView<const FRigElementKey> ElementView(InElements.GetData(), InElements.Num());
	CachedPose = InControlRig->GetHierarchy()->GetPose(IsInitial(InTransformType), InFilter, ElementView);
}

UControlRig::FPoseScope::~FPoseScope()
{
	check(ControlRig);

	ControlRig->GetHierarchy()->SetPose(CachedPose, TransformType);
}

#if WITH_EDITOR

UControlRig::FTransientControlScope::FTransientControlScope(TObjectPtr<URigHierarchy> InHierarchy)
	:Hierarchy(InHierarchy)
{
	for (FRigControlElement* Control : Hierarchy->GetTransientControls())
	{
		FTransientControlInfo Info;
		Info.Name = Control->GetName();
		Info.Parent = Hierarchy->GetFirstParent(Control->GetKey());
		Info.Settings = Control->Settings;
		// preserve whatever value that was produced by this transient control at the moment
		Info.Value = Hierarchy->GetControlValue(Control->GetKey(),ERigControlValueType::Current);
		Info.OffsetTransform = Hierarchy->GetControlOffsetTransform(Control, ERigTransformType::CurrentLocal);
		Info.ShapeTransform = Hierarchy->GetControlShapeTransform(Control, ERigTransformType::CurrentLocal);
				
		SavedTransientControls.Add(Info);
	}
}

UControlRig::FTransientControlScope::~FTransientControlScope()
{
	if (URigHierarchyController* Controller = Hierarchy->GetController())
	{
		for (const FTransientControlInfo& Info : SavedTransientControls)
		{
			Controller->AddControl(
				Info.Name,
				Info.Parent,
				Info.Settings,
				Info.Value,
				Info.OffsetTransform,
				Info.ShapeTransform,
				false,
				false
			);
		}
	}
}

#endif
 
#undef LOCTEXT_NAMESPACE

