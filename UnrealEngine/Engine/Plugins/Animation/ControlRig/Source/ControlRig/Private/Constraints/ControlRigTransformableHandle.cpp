// Copyright Epic Games, Inc. All Rights Reserved.


#include "Constraints/ControlRigTransformableHandle.h"

#include "ControlRig.h"
#include "ControlRigObjectBinding.h"
#include "IControlRigObjectBinding.h"
#include "Rigs/RigHierarchyElements.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"
#include "Sections/MovieScene3DTransformSection.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigTransformableHandle)

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

bool UTransformableControlHandle::IsValid() const
{
	if (!ControlRig.IsValid() || ControlName == NAME_None)
	{
		return false;
	}

	const USkeletalMeshComponent* SkeletalMeshComponent = GetSkeletalMesh();
	if (!SkeletalMeshComponent)
	{
		return false;
	}

	const FRigControlElement* ControlElement = ControlRig->FindControl(ControlName);
	if (!ControlElement)
	{
		return false;
	}
	
	return true;
}

void UTransformableControlHandle::TickForBaking()
{
	USkeletalMeshComponent* SkeletalMeshComponent = GetSkeletalMesh();
	if (SkeletalMeshComponent)
	{
		const AActor* Parent = SkeletalMeshComponent->GetOwner();
		while (Parent)
		{
			TArray<USkeletalMeshComponent*> MeshComps;
			Parent->GetComponents(MeshComps, true);

			for (USkeletalMeshComponent* MeshComp : MeshComps)
			{
				MeshComp->TickAnimation(0.03f, false);
				MeshComp->RefreshBoneTransforms();
				MeshComp->RefreshFollowerComponents();
				MeshComp->UpdateComponentToWorld();
				MeshComp->FinalizeBoneTransform();
				MeshComp->MarkRenderTransformDirty();
				MeshComp->MarkRenderDynamicDataDirty();
			}

			Parent = Parent->GetAttachParentActor();
		}
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

	const USkeletalMeshComponent* SkeletalMeshComponent = GetSkeletalMesh();
	if (!SkeletalMeshComponent)
	{
		return;
	}
	
	URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
	const FRigElementKey& ControlKey = ControlElement->GetKey();
	const int32 CtrlIndex = Hierarchy->GetIndex(ControlKey);
	
	const FTransform& ComponentTransform = SkeletalMeshComponent->GetComponentTransform();
	Hierarchy->SetGlobalTransform(CtrlIndex, InGlobal.GetRelativeTransform(ComponentTransform));
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
	
	const USkeletalMeshComponent* SkeletalMeshComponent = GetSkeletalMesh();
	if (!SkeletalMeshComponent)
	{
		return FTransform::Identity;
	}

	const FRigElementKey& ControlKey = ControlElement->GetKey();
	const URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
	const int32 CtrlIndex = Hierarchy->GetIndex(ControlKey);

	const FTransform& ComponentTransform = SkeletalMeshComponent->GetComponentTransform();
	return Hierarchy->GetGlobalTransform(CtrlIndex) * ComponentTransform;
}

FTransform UTransformableControlHandle::GetLocalTransform() const
{
	const FRigControlElement* ControlElement = GetControlElement();
	if (!ControlElement)
	{
		return FTransform::Identity;
	}
	
	const FRigElementKey& ControlKey = ControlElement->GetKey();
	const URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
	const int32 CtrlIndex = Hierarchy->GetIndex(ControlKey);

	return Hierarchy->GetLocalTransform(CtrlIndex);
}

UObject* UTransformableControlHandle::GetPrerequisiteObject() const
{
	return GetSkeletalMesh(); 
}

FTickFunction* UTransformableControlHandle::GetTickFunction() const
{
	USkeletalMeshComponent* SkelMeshComponent = GetSkeletalMesh();
	return SkelMeshComponent ? &SkelMeshComponent->PrimaryComponentTick : nullptr;
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
	return GetSkeletalMesh();
}

USkeletalMeshComponent* UTransformableControlHandle::GetSkeletalMesh() const
{
	const TSharedPtr<IControlRigObjectBinding> ObjectBinding = ControlRig.IsValid() ? ControlRig->GetObjectBinding() : nullptr;
   	return ObjectBinding ? Cast<USkeletalMeshComponent>(ObjectBinding->GetBoundObject()) : nullptr;
}

bool UTransformableControlHandle::HasDirectDependencyWith(const UTransformableHandle& InOther) const
{
	const uint32 OtherHash = InOther.GetHash();
	if (OtherHash == 0)
	{
		return false;
	}

	// check whether the other handle is one of the skeletal mesh parent
	if (const USkeletalMeshComponent* SkeletalMeshComponent = GetSkeletalMesh())
	{
		for (const USceneComponent* Comp=SkeletalMeshComponent->GetAttachParent(); Comp!=nullptr; Comp=Comp->GetAttachParent() )
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
		const uint32 ParentHash = ComputeHash(ControlRig.Get(), Parent->GetName());
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
		const uint32 ChildHash = ComputeHash(ControlRig.Get(), Parent->GetName());
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
		return FTickPrerequisite( GetSkeletalMesh(), *TickFunction); 
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
				ControlName = InElement->GetName();
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

	if (!ControlRig.IsValid() || ControlName == NAME_None)
	{
		return;
	}

	if (ControlRig == InControlRig && InControl->GetName() == ControlName)
	{
		if(OnHandleModified.IsBound())
		{
			const EHandleEvent Event = InContext.bConstraintUpdate ?
				EHandleEvent::GlobalTransformUpdated : EHandleEvent::LocalTransformUpdated; 
			OnHandleModified.Broadcast(this, Event);
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
		const UWorld* ThisWorld = GetWorld();
		if (ThisWorld && InObject->GetWorld() == ThisWorld)
		{
			OnHandleModified.Broadcast(this, EHandleEvent::ComponentUpdated);
		}
	}
}

static TPair<const FChannelMapInfo*, int32> GetInfoAndNumFloatChannels(
	const UControlRig* InControlRig,
	const FName& InControlName,
	const UMovieSceneControlRigParameterSection* InSection)
{
	const FRigControlElement* ControlElement = InControlRig ? InControlRig->FindControl(InControlName) : nullptr;
	auto GetNumFloatChannels = [](const ERigControlType& InControlType)
	{
		switch (InControlType)
		{
		case ERigControlType::Position:
		case ERigControlType::Scale:
		case ERigControlType::Rotator:
			return 3;
		case ERigControlType::TransformNoScale:
			return 6;
		case ERigControlType::Transform:
		case ERigControlType::EulerTransform:
			return 9;
		default:
			break;
		}
		return 0;
	};

	const int32 NumFloatChannels = ControlElement ? GetNumFloatChannels(ControlElement->Settings.ControlType) : 0;
	const FChannelMapInfo* ChannelInfo = InSection ? InSection->ControlChannelMap.Find(InControlName) : nullptr;

	return { ChannelInfo, NumFloatChannels };
}
TArrayView<FMovieSceneFloatChannel*>  UTransformableControlHandle::GetFloatChannels(const UMovieSceneSection* InSection) const
{
	// no floats for transform sections
	static const TArrayView<FMovieSceneFloatChannel*> EmptyChannelsView;

	const FChannelMapInfo* ChannelInfo = nullptr;
	int32 NumChannels = 0;
	const UMovieSceneControlRigParameterSection* CRSection = Cast<UMovieSceneControlRigParameterSection>(InSection);
	if (CRSection == nullptr)
	{
		return EmptyChannelsView;
	}

	Tie(ChannelInfo, NumChannels) = GetInfoAndNumFloatChannels(ControlRig.Get(),ControlName, CRSection);

	if (ChannelInfo == nullptr || NumChannels == 0)
	{
		return EmptyChannelsView;
	}

	// return a sub view that just represents the control's channels
	const TArrayView<FMovieSceneFloatChannel*> FloatChannels = InSection->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
	const int32 ChannelStartIndex = ChannelInfo->ChannelIndex;
	return FloatChannels.Slice(ChannelStartIndex, NumChannels);
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
			return InControlRig->SetControlLocalTransform(ControlName, InTransform, bNotify, InKeyframeContext, bUndo, bFixEuler);
		}
		InControlRig->SetControlGlobalTransform(ControlName, InTransform, bNotify, InKeyframeContext, bUndo, bFixEuler);
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
	if (UControlRig* InControlRig = Cast<UControlRig>(SubObject))
	{
		if (ControlRig != InControlRig)
		{
			for (TWeakObjectPtr<> ParentObject : ConstraintBindingID.ResolveBoundObjects(LocalSequenceID, Player))
			{
				USceneComponent* Component = nullptr;
				if (AActor* Actor = Cast<AActor>(ParentObject.Get()))
				{
					Component = Actor->GetRootComponent();
				}
				else if (USceneComponent* Comp = Cast<USceneComponent>(ParentObject.Get()))
				{
					Component = Comp;
				}

				if (InControlRig->GetObjectBinding() && InControlRig->GetObjectBinding()->GetBoundObject() == Component)
				{
					ControlRig = InControlRig;
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
	const USkeletalMeshComponent* SkeletalMesh = GetSkeletalMesh();
	if (!SkeletalMesh)
	{
		static const FString DummyLabel;
		return DummyLabel;
	}
	
	const AActor* Actor = SkeletalMesh->GetOwner();
	const FString ControlRigLabel = Actor ? Actor->GetActorLabel() : SkeletalMesh->GetName();
	return FString::Printf(TEXT("%s/%s"), *ControlRigLabel, *ControlName.ToString() );
}

void UTransformableControlHandle::OnObjectsReplaced(const TMap<UObject*, UObject*>& InOldToNewInstances)
{
	if (UObject* NewObject = InOldToNewInstances.FindRef(ControlRig.Get()))
	{
		if (UControlRig* NewControlRig = Cast<UControlRig>(NewObject))
		{
			if (URigHierarchy* Hierarchy = ControlRig->GetHierarchy())
			{
				Hierarchy->OnModified().RemoveAll(this);
			}
			
			ControlRig = NewControlRig;
			
			if (URigHierarchy* Hierarchy = ControlRig->GetHierarchy())
			{
				Hierarchy->OnModified().AddUObject(this, &UTransformableControlHandle::OnHierarchyModified);
			}
		}
	}
}

#endif
