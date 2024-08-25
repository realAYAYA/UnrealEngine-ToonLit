// Copyright Epic Games, Inc. All Rights Reserved.

#include "EvaluationVM/EvaluationVM.h"

#include "Animation/Skeleton.h"
#include "BoneContainer.h"
#include "EvaluationVM/KeyframeState.h"

namespace UE::AnimNext
{
	const FEvaluationVMStackName KEYFRAME_STACK_NAME = FName(TEXT("KeyframeStack"));

	FEvaluationVMStack::~FEvaluationVMStack()
	{
		FEvaluationVMStackEntry* EntryPtr = Top;
		while (EntryPtr != nullptr)
		{
			if (TypeDestructor)
			{
				TypeDestructor(EntryPtr->GetValuePtr());
			}

			FEvaluationVMStackEntry* PrevEntryPtr = EntryPtr->Prev;

			FMemory::Free(EntryPtr);

			EntryPtr = PrevEntryPtr;
		}
	}

	FEvaluationVM::FEvaluationVM(EEvaluationFlags InEvaluationFlags, const UE::AnimNext::FReferencePose& InReferencePose, int32 InCurrentLOD)
		: ReferencePose(&InReferencePose)
		, CurrentLOD(InCurrentLOD)
		, EvaluationFlags(InEvaluationFlags)
	{
		CurveFilter.SetFilterMode(UE::Anim::ECurveFilterMode::DisallowAll);

		//const UE::Anim::FCurveFilterSettings CurveFilterSettings(UE::Anim::ECurveFilterMode::DisallowAll);
		//if(USkeleton* Skeleton = const_cast<USkeleton*>(InReferencePose.Skeleton.Get())) // const_cast because the bone container takes a mutable reference
		{
			//BoneContainer.InitializeTo(InReferencePose.GetLODBoneIndexToMeshBoneIndexMap(InCurrentLOD), CurveFilterSettings, *Skeleton);

			// TODO: In AnimInstanceProxy this is how we initialize the bone container, we need to get the component somehow or we
			// gotta figure out how to support ref pose overrides
#if 0
			// Use the shared bone container
			RequiredBones = Component->GetSharedRequiredBones();

			// The first anim instance will initialize the required bones, all others will re-use it
			if (!RequiredBones->IsValid())
			{
				RequiredBones->InitializeTo(Component->RequiredBones, Component->GetCurveFilterSettings(), *Asset);

				// If there is a ref pose override, we want to replace ref pose in RequiredBones
				// Update ref pose in required bones structure (either set it, or clear it, depending on if one is set on the Component)
				RequiredBones->SetRefPoseOverride(Component->GetRefPoseOverride());
			}
#endif
		}
	}

	bool FEvaluationVM::IsValid() const
	{
		return ReferencePose != nullptr;
	}

	void FEvaluationVM::Shrink()
	{
		InternalStacks.Shrink();
	}

	FKeyframeState FEvaluationVM::MakeReferenceKeyframe(bool bAdditiveKeyframe) const
	{
		// TODO: It would be great if we could support immutable poses that we can push/pop like normal mutable poses
		// This would allow us to cheaply push reference/identity poses
		// Tasks that consume poses can then choose to re-use a mutable pose (to avoid allocating a new one) and only
		// allocate one if all their inputs are immutable.
		// This would reduce the number of required intermediate poses.

		FKeyframeState Keyframe;

		if (EnumHasAnyFlags(EvaluationFlags, EEvaluationFlags::Bones))
		{
			const bool bInitWithRefPose = true;

			Keyframe.Pose.PrepareForLOD(*ReferencePose, CurrentLOD, bInitWithRefPose, bAdditiveKeyframe);
		}

		if (EnumHasAnyFlags(EvaluationFlags, EEvaluationFlags::Curves))
		{
			Keyframe.Curves.SetFilter(&CurveFilter);
		}

		return Keyframe;
	}

	FKeyframeState FEvaluationVM::MakeUninitializedKeyframe(bool bAdditiveKeyframe) const
	{
		FKeyframeState Keyframe;

		if (EnumHasAnyFlags(EvaluationFlags, EEvaluationFlags::Bones))
		{
			const bool bInitWithRefPose = false;

			Keyframe.Pose.PrepareForLOD(*ReferencePose, CurrentLOD, bInitWithRefPose, bAdditiveKeyframe);
		}

		if (EnumHasAnyFlags(EvaluationFlags, EEvaluationFlags::Curves))
		{
			Keyframe.Curves.SetFilter(&CurveFilter);
		}

		return Keyframe;
	}

	FEvaluationVMStack& FEvaluationVM::GetOrCreateStack(const FEvaluationVMStackName& StackName, uint32 TypeID)
	{
		if (FEvaluationVMStack* Stack = InternalStacks.FindByHash(StackName.NameHash, StackName.Name))
		{
			return *Stack;
		}

		FEvaluationVMStack& Stack = InternalStacks.AddByHash(StackName.NameHash, StackName.Name, FEvaluationVMStack());
		Stack.Name = StackName.Name;
		Stack.TypeID = TypeID;

		return Stack;
	}

	FEvaluationVMStack* FEvaluationVM::FindStack(const FEvaluationVMStackName& StackName, uint32 TypeID)
	{
		FEvaluationVMStack* Stack = InternalStacks.FindByHash(StackName.NameHash, StackName.Name);
		if (Stack == nullptr)
		{
			return nullptr;
		}

		if (!ensureMsgf(Stack->TypeID == TypeID, TEXT("Type mismatch! This evaluation stack is being queried with a different type than it was created with")))
		{
			return nullptr;
		}

		return Stack;
	}

	const FEvaluationVMStack* FEvaluationVM::FindStack(const FEvaluationVMStackName& StackName, uint32 TypeID) const
	{
		const FEvaluationVMStack* Stack = InternalStacks.FindByHash(StackName.NameHash, StackName.Name);
		if (Stack == nullptr)
		{
			return nullptr;
		}

		if (!ensureMsgf(Stack->TypeID == TypeID, TEXT("Type mismatch! This evaluation stack is being queried with a different type than it was created with")))
		{
			return nullptr;
		}

		return Stack;
	}

	const void* FEvaluationVM::PeekValueImpl(const FEvaluationVMStackName& StackName, uint32 TypeID, uint32 Offset) const
	{
		const FEvaluationVMStack* Stack = FindStack(StackName, TypeID);
		if (Stack == nullptr)
		{
			// Stack not found
			return nullptr;
		}

		const FEvaluationVMStackEntry* EntryPtr = Stack->Top;
		if (EntryPtr == nullptr)
		{
			// Stack is empty
			return nullptr;
		}

		uint32 Count = 0;
		while (EntryPtr != nullptr && Count < Offset)
		{
			EntryPtr = EntryPtr->Prev;
			Count++;
		}

		if (Count != Offset)
		{
			// Offset is too far, not enough entries
			check(EntryPtr == nullptr);
			return nullptr;
		}

		check(EntryPtr != nullptr);
		return EntryPtr->GetValuePtr();
	}
}
