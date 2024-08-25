// Copyright Epic Games, Inc. All Rights Reserved.


#include "Constraints/ControlRigTransformableHandle.h"

#include "Components/SkeletalMeshComponent.h"
#include "ControlRig.h"
#include "ControlRigComponent.h"
#include "IControlRigObjectBinding.h"
#include "ControlRigObjectBinding.h"
#include "TransformableHandleUtils.h"
#include "Rigs/RigHierarchyElements.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Sequencer/ControlRigSequencerHelpers.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigTransformableHandle)

namespace ControlHandleLocals
{
	using RigGuard = TGuardValue_Bitfield_Cleanup<TFunction<void()>>;
	
	TSet<UControlRig*> NotifyingRigs;

	bool IsRigNotifying(const UControlRig* InControlRig)
	{
		return InControlRig ? NotifyingRigs.Contains(InControlRig) : false;
	}
}

/**
 * UTransformableControlHandle
 */

UTransformableControlHandle::~UTransformableControlHandle()
{
	UnregisterDelegates();
}

void UTransformableControlHandle::PostLoad()
{
	Super::PostLoad();
	RegisterDelegates();
}

bool UTransformableControlHandle::IsValid(const bool bDeepCheck) const
{
	if (!ControlRig.IsValid() || ControlName == NAME_None)
	{
		return false;
	}

	const FRigControlElement* ControlElement = ControlRig->FindControl(ControlName);
	if (!ControlElement)
	{
		return false;
	}

	if (bDeepCheck)
	{
		const USceneComponent* BoundComponent = GetBoundComponent();
		if (!BoundComponent)
		{
			return false;
		}
	}
	
	return true;
}

void UTransformableControlHandle::PreEvaluate(const bool bTick) const
{
	if (!ControlRig.IsValid() || ControlRig->IsEvaluating())
	{
		return;
	}

	if (ControlRig->IsAdditive())
	{
		if (ControlHandleLocals::IsRigNotifying(ControlRig.Get()))
		{
			return;
		}
		
		if (const USkeletalMeshComponent* SkeletalMeshComponent = GetSkeletalMesh())
		{
			if (!SkeletalMeshComponent->PoseTickedThisFrame())
			{
				return TickTarget();
			}
		}
	}
	
	return bTick ? TickTarget() : ControlRig->Evaluate_AnyThread();
}

void UTransformableControlHandle::TickTarget() const
{
	if (!ControlRig.IsValid())
	{
		return;
	}
	
	if (ControlRig->IsAdditive() && ControlHandleLocals::IsRigNotifying(ControlRig.Get()))
	{
		return;
	}
	
	if (const USkeletalMeshComponent* SkeletalMeshComponent = GetSkeletalMesh())
	{
		return TransformableHandleUtils::TickDependantComponents(SkeletalMeshComponent);
	}

	if (UControlRigComponent* ControlRigComponent = GetControlRigComponent())
	{
		ControlRigComponent->Update();
	}
}

// NOTE should we cache the skeletal mesh and the CtrlIndex to avoid looking for if every time
// probably not for handling runtime changes
void UTransformableControlHandle::SetGlobalTransform(const FTransform& InGlobal) const
{
	const FRigControlElement* ControlElement = GetControlElement();
	if (!ControlElement)
	{
		return;
	}

	const USceneComponent* BoundComponent = GetBoundComponent();
	if (!BoundComponent)
	{
		return;
	}
	
	const FRigElementKey& ControlKey = ControlElement->GetKey();
	const FTransform& ComponentTransform = BoundComponent->GetComponentTransform();

	static const FRigControlModifiedContext Context(EControlRigSetKey::Never);
	static constexpr bool bNotify = false, bSetupUndo = false, bPrintPython = false, bFixEulerFlips = false;

	//use this function so we don't set the preferred angles
	ControlRig->SetControlGlobalTransform(ControlKey.Name, InGlobal.GetRelativeTransform(ComponentTransform),
		bNotify, Context, bSetupUndo, bPrintPython, bFixEulerFlips);
}

void UTransformableControlHandle::SetLocalTransform(const FTransform& InLocal) const
{
	const FRigControlElement* ControlElement = GetControlElement();
	if (!ControlElement)
	{
		return;
	}
	
	URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
	const FRigElementKey& ControlKey = ControlElement->GetKey();
	const int32 CtrlIndex = Hierarchy->GetIndex(ControlKey);
	
	Hierarchy->SetLocalTransform(CtrlIndex, InLocal);
}

// NOTE should we cache the skeletal mesh and the CtrlIndex to avoid looking for if every time
// probably not for handling runtime changes
FTransform UTransformableControlHandle::GetGlobalTransform() const
{
	const FRigControlElement* ControlElement = GetControlElement();
	if (!ControlElement)
	{
		return FTransform::Identity;
	}
	
	const USceneComponent* BoundComponent = GetBoundComponent();
	if (!BoundComponent)
	{
		return FTransform::Identity;
	}

	const FRigElementKey& ControlKey = ControlElement->GetKey();
	const URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
	const int32 CtrlIndex = Hierarchy->GetIndex(ControlKey);

	const FTransform& ComponentTransform = BoundComponent->GetComponentTransform();
	return Hierarchy->GetGlobalTransform(CtrlIndex) * ComponentTransform;
}

FTransform UTransformableControlHandle::GetLocalTransform() const
{
	const FRigControlElement* ControlElement = GetControlElement();
	if (!ControlElement)
	{
		return FTransform::Identity;
	}

	if (ControlRig->IsAdditive())
	{
		return ControlRig->GetControlLocalTransform(ControlName);
	}
	
	const FRigElementKey& ControlKey = ControlElement->GetKey();
	const URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
	const int32 CtrlIndex = Hierarchy->GetIndex(ControlKey);

	return Hierarchy->GetLocalTransform(CtrlIndex);
}

UObject* UTransformableControlHandle::GetPrerequisiteObject() const
{
	return GetBoundComponent(); 
}

FTickFunction* UTransformableControlHandle::GetTickFunction() const
{
	USceneComponent* BoundComponent = GetBoundComponent();
	return BoundComponent ? &BoundComponent->PrimaryComponentTick : nullptr;
}

uint32 UTransformableControlHandle::ComputeHash(const UControlRig* InControlRig, const FName& InControlName)
{
	return HashCombine(GetTypeHash(InControlRig), GetTypeHash(InControlName));
}

uint32 UTransformableControlHandle::GetHash() const
{
	if (ControlRig.IsValid() && ControlName != NAME_None)
	{
		return ComputeHash(ControlRig.Get(), ControlName);
	}
	return 0;
}

TWeakObjectPtr<UObject> UTransformableControlHandle::GetTarget() const
{
	return GetBoundComponent();
}

USceneComponent* UTransformableControlHandle::GetBoundComponent() const
{
	if (USkeletalMeshComponent* SkeletalMeshComponent = GetSkeletalMesh())
	{
		return SkeletalMeshComponent;	
	}
	return GetControlRigComponent();
}

USkeletalMeshComponent* UTransformableControlHandle::GetSkeletalMesh() const
{
	const TSharedPtr<IControlRigObjectBinding> ObjectBinding = ControlRig.IsValid() ? ControlRig->GetObjectBinding() : nullptr;
   	return ObjectBinding ? Cast<USkeletalMeshComponent>(ObjectBinding->GetBoundObject()) : nullptr;
}

UControlRigComponent* UTransformableControlHandle::GetControlRigComponent() const
{
	const TSharedPtr<IControlRigObjectBinding> ObjectBinding = ControlRig.IsValid() ? ControlRig->GetObjectBinding() : nullptr;
	return ObjectBinding ? Cast<UControlRigComponent>(ObjectBinding->GetBoundObject()) : nullptr;
}

bool UTransformableControlHandle::HasDirectDependencyWith(const UTransformableHandle& InOther) const
{
	const uint32 OtherHash = InOther.GetHash();
	if (OtherHash == 0)
	{
		return false;
	}

	// check whether the other handle is one of the skeletal mesh parent
	if (const USceneComponent* BoundComponent = GetBoundComponent())
	{
		if (GetTypeHash(BoundComponent) == OtherHash)
		{
			// we cannot constrain the skeletal mesh component to one of ControlRig's controls
			return true;
		}
		
		for (const USceneComponent* Comp=BoundComponent->GetAttachParent(); Comp!=nullptr; Comp=Comp->GetAttachParent() )
		{
			const uint32 AttachParentHash = GetTypeHash(Comp);
			if (AttachParentHash == OtherHash)
			{
				return true;
			}
		}
	}
	
	FRigControlElement* ControlElement = GetControlElement();
	if (!ControlElement)
	{
		return false;
	}

	// check whether the other handle is one of the control parent within the CR hierarchy
	static constexpr bool bRecursive = true;
	const URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
	const FRigBaseElementParentArray AllParents = Hierarchy->GetParents(ControlElement, bRecursive);
	const bool bIsParent = AllParents.ContainsByPredicate([this, OtherHash](const FRigBaseElement* Parent)
	{
		const uint32 ParentHash = ComputeHash(ControlRig.Get(), Parent->GetFName());
		return ParentHash == OtherHash;		
	});

	if (bIsParent)
	{
		return true;
	}

	// otherwise, check if there are any dependency in the graph that would cause a cycle
	const TArray<FRigControlElement*> AllControls = Hierarchy->GetControls();
	const int32 IndexOfPossibleParent = AllControls.IndexOfByPredicate([this, OtherHash](const FRigBaseElement* Parent)
	{
		const uint32 ChildHash = ComputeHash(ControlRig.Get(), Parent->GetFName());
		return ChildHash == OtherHash;
	});

	if (IndexOfPossibleParent != INDEX_NONE)
	{
		FRigControlElement* Parent = AllControls[IndexOfPossibleParent];

		FString FailureReason;

#if WITH_EDITOR
		const URigHierarchy::TElementDependencyMap DependencyMap = Hierarchy->GetDependenciesForVM(ControlRig->GetVM());
		const bool bIsParentedTo = Hierarchy->IsParentedTo(ControlElement, Parent, DependencyMap);
#else
		const bool bIsParentedTo = Hierarchy->IsParentedTo(ControlElement, Parent);
#endif
		
		if (bIsParentedTo)
		{
			return true;
		}
	}

	return false;
}

// if there's no skeletal mesh bound then the handle is not valid so no need to do anything else
FTickPrerequisite UTransformableControlHandle::GetPrimaryPrerequisite() const
{
	if (FTickFunction* TickFunction = GetTickFunction())
	{
		return FTickPrerequisite( GetBoundComponent(), *TickFunction); 
	}
	
	static const FTickPrerequisite DummyPrerex;
	return DummyPrerex;
}

FRigControlElement* UTransformableControlHandle::GetControlElement() const
{
	if (!ControlRig.IsValid() || ControlName == NAME_None)
	{
		return nullptr;
	}

	return ControlRig->FindControl(ControlName);
}

void UTransformableControlHandle::UnregisterDelegates() const
{
#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectsReplaced.RemoveAll(this);
#endif
	
	if (ControlRig.IsValid())
	{
		if (URigHierarchy* Hierarchy = ControlRig->GetHierarchy())
		{
			Hierarchy->OnModified().RemoveAll(this);
		}
		ControlRig->ControlModified().RemoveAll(this);

		if (const TSharedPtr<IControlRigObjectBinding> Binding = ControlRig->GetObjectBinding())
		{
			Binding->OnControlRigBind().RemoveAll(this);
		}
		ControlRig->ControlRigBound().RemoveAll(this);
	}
}

void UTransformableControlHandle::RegisterDelegates()
{
	UnregisterDelegates();

#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectsReplaced.AddUObject(this, &UTransformableControlHandle::OnObjectsReplaced);
#endif

	// make sure the CR is loaded so that we can register delegates
	if (ControlRig.IsPending())
	{
		ControlRig.LoadSynchronous();
	}
	
	if (ControlRig.IsValid())
	{
		if (URigHierarchy* Hierarchy = ControlRig->GetHierarchy())
		{
			Hierarchy->OnModified().AddUObject(this, &UTransformableControlHandle::OnHierarchyModified);
		}

		// NOTE BINDER: this has to be done before binding UTransformableControlHandle::OnControlModified
		if (!ControlRig->ControlModified().IsBoundToObject(&GetEvaluationBinding()))
		{
			ControlRig->ControlModified().AddRaw(&GetEvaluationBinding(), &FControlEvaluationGraphBinding::HandleControlModified);
		}
		
		ControlRig->ControlModified().AddUObject(this, &UTransformableControlHandle::OnControlModified);
		if (!ControlRig->ControlRigBound().IsBoundToObject(this))
		{
			ControlRig->ControlRigBound().AddUObject(this, &UTransformableControlHandle::OnControlRigBound);
		}
		OnControlRigBound(ControlRig.Get());
	}
}

void UTransformableControlHandle::OnHierarchyModified(
	ERigHierarchyNotification InNotif,
	URigHierarchy* InHierarchy,
	const FRigBaseElement* InElement)
{
	if (!ControlRig.IsValid())
	{
	 	return;
	}

	const URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
	if (!Hierarchy || InHierarchy != Hierarchy)
	{
		return;
	}

	switch (InNotif)
	{
		case ERigHierarchyNotification::ElementRemoved:
		{
			// FIXME this leaves the constraint invalid as the element won't exist anymore
			// find a way to remove this from the constraints list 
			break;
		}
		case ERigHierarchyNotification::ElementRenamed:
		{
			const FName OldName = Hierarchy->GetPreviousName(InElement->GetKey());
			if (OldName == ControlName)
			{
				ControlName = InElement->GetFName();
			}
			break;
		}
		default:
			break;
	}
}

void UTransformableControlHandle::OnControlModified(
	UControlRig* InControlRig,
	FRigControlElement* InControl,
	const FRigControlModifiedContext& InContext)
{
	if (!InControlRig || !InControl)
	{
		return;
	}

	if (bNotifying)
	{
		return;
	}

	if (!ControlRig.IsValid() || ControlName == NAME_None)
	{
		return;
	}

	if (HandleModified().IsBound() && (ControlRig == InControlRig))
	{
		const EHandleEvent Event = InContext.bConstraintUpdate ?
			EHandleEvent::GlobalTransformUpdated : EHandleEvent::LocalTransformUpdated;

		if (InControl->GetFName() == ControlName)
		{	// if that handle is wrapping InControl
			if (InContext.bConstraintUpdate)
			{
				GetEvaluationBinding().bPendingFlush = true;
			}
			
			// guard from re-entrant notification
			const ControlHandleLocals::RigGuard NotificationGuard([ControlRig = ControlRig.Get()]()
			{
				ControlHandleLocals::NotifyingRigs.Remove(ControlRig);
			});
			ControlHandleLocals::NotifyingRigs.Add(ControlRig.Get());
			
			Notify(Event);
		}
		else if (Event == EHandleEvent::GlobalTransformUpdated)
		{
			// the control being modified is not the one wrapped by this handle 
			if (const FRigControlElement* Control = ControlRig->FindControl(ControlName))
			{
				if (InContext.bConstraintUpdate)
				{
					GetEvaluationBinding().bPendingFlush = true;
				}

				// guard from re-entrant notification 
				const ControlHandleLocals::RigGuard NotificationGuard([ControlRig = ControlRig.Get()]()
				{
					ControlHandleLocals::NotifyingRigs.Remove(ControlRig);
				});
				ControlHandleLocals::NotifyingRigs.Add(ControlRig.Get());
				
				const bool bPreTick = !ControlRig->IsAdditive();
				Notify(EHandleEvent::UpperDependencyUpdated, bPreTick);
			}
		}
	}
}

void UTransformableControlHandle::OnControlRigBound(UControlRig* InControlRig)
{
	if (!InControlRig)
	{
		return;
	}

	if (!ControlRig.IsValid() || ControlRig != InControlRig)
	{
		return;
	}

	if (const TSharedPtr<IControlRigObjectBinding> Binding = ControlRig->GetObjectBinding())
	{
		if (!Binding->OnControlRigBind().IsBoundToObject(this))
		{
			Binding->OnControlRigBind().AddUObject(this, &UTransformableControlHandle::OnObjectBoundToControlRig);
		}
	}
}

void UTransformableControlHandle::OnObjectBoundToControlRig(UObject* InObject)
{
	if (!ControlRig.IsValid() || !InObject)
	{
		return;
	}

	const TSharedPtr<IControlRigObjectBinding> Binding = ControlRig->GetObjectBinding();
	const UObject* CurrentObject = Binding ? Binding->GetBoundObject() : nullptr;
	if (CurrentObject == InObject)
	{
		const UWorld* World = GetWorld();
		if (!World)
		{
			if (const USceneComponent* BoundComponent = GetBoundComponent())
			{
				World = BoundComponent->GetWorld();
			}
		}
		
		if (World && InObject->GetWorld() == World)
		{
			Notify(EHandleEvent::ComponentUpdated);
		}
	}
}

TArrayView<FMovieSceneFloatChannel*>  UTransformableControlHandle::GetFloatChannels(const UMovieSceneSection* InSection) const
{
	return FControlRigSequencerHelpers::GetFloatChannels(ControlRig.Get(),
		ControlName, InSection);
}

TArrayView<FMovieSceneDoubleChannel*>  UTransformableControlHandle::GetDoubleChannels(const UMovieSceneSection* InSection) const
{
	static const TArrayView<FMovieSceneDoubleChannel*> EmptyChannelsView;
	return EmptyChannelsView;
}

bool UTransformableControlHandle::AddTransformKeys(const TArray<FFrameNumber>& InFrames,
	const TArray<FTransform>& InTransforms,
	const EMovieSceneTransformChannel& InChannels,
	const FFrameRate& InTickResolution,
	UMovieSceneSection*,
	const bool bLocal) const
{
	if (!ControlRig.IsValid() || ControlName == NAME_None || InFrames.IsEmpty() || InFrames.Num() != InTransforms.Num())
	{
		return false;
	}
	auto KeyframeFunc = [this, bLocal](const FTransform& InTransform, const FRigControlModifiedContext& InKeyframeContext)
	{
		UControlRig* InControlRig = ControlRig.Get();
		static constexpr bool bNotify = true;
		static constexpr bool bUndo = false;
		static constexpr bool bFixEuler = true;

		if (bLocal)
		{
			InControlRig->SetControlLocalTransform(ControlName, InTransform, bNotify, InKeyframeContext, bUndo, bFixEuler);
			if (InControlRig->IsAdditive())
			{
				InControlRig->Evaluate_AnyThread();
			}
			return;
		}
		
		InControlRig->SetControlGlobalTransform(ControlName, InTransform, bNotify, InKeyframeContext, bUndo, bFixEuler);
		if (InControlRig->IsAdditive())
		{
			InControlRig->Evaluate_AnyThread();
		}
	};

	FRigControlModifiedContext KeyframeContext;
	KeyframeContext.SetKey = EControlRigSetKey::Always;
	KeyframeContext.KeyMask = static_cast<uint32>(InChannels);

	for (int32 Index = 0; Index < InFrames.Num(); ++Index)
	{
		const FFrameNumber& Frame = InFrames[Index];
		KeyframeContext.LocalTime = InTickResolution.AsSeconds(FFrameTime(Frame));

		KeyframeFunc(InTransforms[Index], KeyframeContext);
	}

	return true;
}

//for control rig need to check to see if the control rig is different then we may need to update it based upon what we are now bound to
void UTransformableControlHandle::ResolveBoundObjects(FMovieSceneSequenceID LocalSequenceID, IMovieScenePlayer& Player, UObject* SubObject)
{
	if (const UControlRig* InControlRig = Cast<UControlRig>(SubObject))
	{
		if (ControlRig != InControlRig)
		{
			for (const TWeakObjectPtr<> ParentObject : ConstraintBindingID.ResolveBoundObjects(LocalSequenceID, Player))
			{
				const UObject* Bindable = FControlRigObjectBinding::GetBindableObject(ParentObject.Get());
				if (InControlRig->GetObjectBinding() && InControlRig->GetObjectBinding()->GetBoundObject() == Bindable)
				{
					UnregisterDelegates();
					ControlRig = InControlRig;
					RegisterDelegates();
				}
				break; //just do one
			}
		}
	}
}

UTransformableHandle* UTransformableControlHandle::Duplicate(UObject* NewOuter) const
{
	UTransformableControlHandle* HandleCopy = DuplicateObject<UTransformableControlHandle>(this, NewOuter, GetFName());
	HandleCopy->ControlRig = ControlRig;
	HandleCopy->ControlName = ControlName;
	return HandleCopy;
}
#if WITH_EDITOR

FString UTransformableControlHandle::GetLabel() const
{
	return ControlName.ToString();
}

FString UTransformableControlHandle::GetFullLabel() const
{
	const USceneComponent* BoundComponent = GetBoundComponent();
	if (!BoundComponent)
	{
		static const FString DummyLabel;
		return DummyLabel;
	}
	
	const AActor* Actor = BoundComponent->GetOwner();
	const FString ControlRigLabel = Actor ? Actor->GetActorLabel() : BoundComponent->GetName();
	return FString::Printf(TEXT("%s/%s"), *ControlRigLabel, *ControlName.ToString() );
}

void UTransformableControlHandle::OnObjectsReplaced(const TMap<UObject*, UObject*>& InOldToNewInstances)
{
	if (UObject* NewObject = InOldToNewInstances.FindRef(ControlRig.Get()))
	{
		if (UControlRig* NewControlRig = Cast<UControlRig>(NewObject))
		{
			UnregisterDelegates();
			ControlRig = NewControlRig;
			RegisterDelegates();
		}
	}
}

#endif

FControlEvaluationGraphBinding& UTransformableControlHandle::GetEvaluationBinding()
{
	static FControlEvaluationGraphBinding EvaluationBinding;
	return EvaluationBinding;
}

void FControlEvaluationGraphBinding::HandleControlModified(UControlRig* InControlRig, FRigControlElement* InControl, const FRigControlModifiedContext& InContext)
{
	if (!bPendingFlush || !InContext.bConstraintUpdate)
	{
		return;
	}
	
	if (!InControlRig || !InControl)
	{
		return;
	}

	// flush all pending evaluations if any
	if (UWorld* World = InControlRig->GetWorld())
	{
		const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(World);
		Controller.FlushEvaluationGraph();
	}
	bPendingFlush = false;
}
