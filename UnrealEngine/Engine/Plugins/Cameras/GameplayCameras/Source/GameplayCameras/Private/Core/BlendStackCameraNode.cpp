// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/BlendStackCameraNode.h"

#include "Core/BlendCameraNode.h"
#include "Core/BlendStackRootCameraNode.h"
#include "Core/CameraAsset.h"
#include "Core/CameraEvaluationContext.h"
#include "Core/CameraMode.h"
#include "Core/CameraRuntimeInstantiator.h"
#include "Core/CameraSystemEvaluator.h"
#include "Nodes/Blends/PopBlendCameraNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BlendStackCameraNode)

void UBlendStackCameraNode::Push(const FBlendStackCameraPushParams& Params)
{
	if (!Entries.IsEmpty())
	{
		// Don't push anything if what is being requested is already the
		// active camera mode.
		const FCameraModeEntry& TopEntry(Entries.Top());
		if (!TopEntry.bIsFrozen
				&& TopEntry.OriginalCameraMode == Params.CameraMode
				&& TopEntry.EvaluationContext == Params.EvaluationContext)
		{
			return;
		}
	}

	// Create the new root node. Instantiated objects will go inside it.
	UBlendStackRootCameraNode* EntryRootNode = NewObject<UBlendStackRootCameraNode>(this, NAME_None);

	// Instantiate the camera mode's node tree.
	FCameraRuntimeInstantiationParams NodeTreeInstParams;
	NodeTreeInstParams.InstantiationOuter = EntryRootNode;
	NodeTreeInstParams.bAllowRecyling = true;

	FCameraRuntimeInstantiator& Instantiator = Params.Evaluator->GetRuntimeInstantiator();
	UCameraNode* ModeRootNode = Instantiator.InstantiateCameraNodeTree(Params.CameraMode->RootNode, NodeTreeInstParams);

	// Find a transition and instantiate its blend. If not transition is found,
	// make a camera cut transition.
	UBlendCameraNode* Blend = nullptr;
	if (const FCameraModeTransition* Transition = FindTransition(Params))
	{
		FCameraRuntimeInstantiationParams BlendInstParams;
		BlendInstParams.InstantiationOuter = EntryRootNode;
		// No recycling on the blend.

		UBlendCameraNode* ModeBlend = CastChecked<UBlendCameraNode>(
				Instantiator.InstantiateCameraNodeTree(Transition->Blend, BlendInstParams));
		Blend = ModeBlend;
	}
	else
	{
		Blend = NewObject<UPopBlendCameraNode>(EntryRootNode, NAME_None);
	}

	EntryRootNode->Initialize(Blend, ModeRootNode);

	// Make a new entry and add it to the stack.
	FCameraModeEntry NewEntry;
	NewEntry.EvaluationContext = Params.EvaluationContext;
	NewEntry.OriginalCameraMode = Params.CameraMode;
	NewEntry.RootNode = EntryRootNode;
	NewEntry.bIsFirstFrame = true;

	Entries.Add(MoveTemp(NewEntry));
}

void UBlendStackCameraNode::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UBlendStackCameraNode* TypedThis = CastChecked<UBlendStackCameraNode>(InThis);
	for (FCameraModeEntry& Entry : TypedThis->Entries)
	{
		Collector.AddReferencedObject(Entry.OriginalCameraMode);
		Collector.AddReferencedObject(Entry.RootNode);
	}
}

FCameraNodeChildrenView UBlendStackCameraNode::OnGetChildren()
{
	FCameraNodeChildrenView View;
	for (FCameraModeEntry& Entry : Entries)
	{
		View.Add(Entry.RootNode);
	}
	return View;
}

void UBlendStackCameraNode::OnRun(const FCameraNodeRunParams& Params, FCameraNodeRunResult& OutResult)
{
	// Start by evaluating all the root nodes in the stack.
	for (FCameraModeEntry& Entry : Entries)
	{
		FCameraNodeRunParams CurParams(Params);
		CurParams.bIsFirstFrame = Entry.bIsFirstFrame;

		FCameraNodeRunResult& CurResult(Entry.Result);

		if (!Entry.bIsFrozen)
		{
			// If the context in which this camera mode runs doesn't have a valid result,
			// skip it.
			const UCameraEvaluationContext* CurContext = Entry.EvaluationContext.Get();
			if (UNLIKELY(CurContext == nullptr))
			{
				CurResult.bIsValid = false;
				continue;
			}
			const FCameraNodeRunResult& ContextResult(CurContext->GetInitialResult());
			if (UNLIKELY(!ContextResult.bIsValid))
			{
				CurResult.bIsValid = false;
				continue;
			}

			// Start with the input given to us.
			CurResult.CameraPose = OutResult.CameraPose;
			CurResult.CameraPose.ClearAllChangedFlags();

			// Override it with whatever the evaluation context has set on its result.
			CurResult.CameraPose.OverrideChanged(ContextResult.CameraPose);
			CurResult.bIsCameraCut = OutResult.bIsCameraCut || ContextResult.bIsCameraCut;
			CurResult.bIsValid = true;

			// Run the camera mode!
			Entry.RootNode->Run(CurParams, CurResult);
		}
		else
		{
			// Only evaluate the blend via the root node.
			Entry.RootNode->Run(CurParams, CurResult);
		}
	}

	// Now blend all the results, keeping track of blends that have reached 100% so
	// that we can remove any camera modes below (since the would have been completely
	// blended out by that).
	int32 EntryIndex = 0;
	int32 PopEntriesBelow = INDEX_NONE;
	for (FCameraModeEntry& Entry : Entries)
	{
		FCameraNodeRunResult& CurResult(Entry.Result);
		if (UNLIKELY(!CurResult.bIsValid))
		{
			continue;
		}

		const FCameraPoseFlags ChangedFlags(CurResult.CameraPose.GetChangedFlags());

		FCameraNodeRunParams CurParams(Params);
		CurParams.bIsFirstFrame = Entry.bIsFirstFrame;
		FCameraNodeBlendParams BlendParams(CurParams, CurResult);

		FCameraNodeBlendResult BlendResult(OutResult);

		UBlendCameraNode* EntryBlend = Entry.RootNode->GetBlend();
		if (EntryBlend)
		{
			EntryBlend->BlendResults(BlendParams, BlendResult);

			if (BlendResult.bIsBlendFull && BlendResult.bIsBlendFinished)
			{
				PopEntriesBelow = EntryIndex;
			}
		}
		else
		{
			OutResult.CameraPose.OverrideChanged(CurResult.CameraPose);

			PopEntriesBelow = EntryIndex;
		}

		CurResult.CameraPose.SetChangedFlags(ChangedFlags);
		
		++EntryIndex;
	}

	// Pop out camera modes that have been blended out.
	if (bAutoPop && PopEntriesBelow != INDEX_NONE)
	{
		FCameraRuntimeInstantiator& Instantiator = Params.Evaluator->GetRuntimeInstantiator();
		for (int32 Index = 0; Index < PopEntriesBelow; ++Index)
		{
			FCameraModeEntry& Entry = Entries[0];
			if (!Entry.bIsFrozen)
			{
				// Recycle the camera mode's node hierarchy if possible.
				// Reset all nodes in it so they are back to their default state, ready to be
				// re-used later.
				TObjectPtr<const UCameraNode> OriginalRootNode = Entry.OriginalCameraMode->RootNode;
				TObjectPtr<UCameraNode> RecycledRootNode = Entry.RootNode->GetRootNode();
				Entry.RootNode->Reset(FCameraNodeResetParams());

				Instantiator.RecycleInstantiatedObject(OriginalRootNode, RecycledRootNode);
			}
			Entries.RemoveAt(0);
		}
	}

	// Reset first frame flags.
	for (FCameraModeEntry& Entry : Entries)
	{
		Entry.bIsFirstFrame = false;
	}
}

const FCameraModeTransition* UBlendStackCameraNode::FindTransition(const FBlendStackCameraPushParams& Params) const
{
	const UCameraEvaluationContext* ToContext = Params.EvaluationContext.Get();
	const UCameraAsset* ToCameraAsset = ToContext ? ToContext->GetCameraAsset() : nullptr;
	const UCameraMode* ToCameraMode = Params.CameraMode;

	// Find a transition that works for blending towards ToCameraMode.
	// If the stack isn't empty, we need to find a transition that works between the previous and 
	// next camera modes. If the stack is empty, we blend the new camera mode in from nothing if
	// appropriate.
	if (!Entries.IsEmpty())
	{
		const FCameraModeTransition* TransitionToUse = nullptr;

		// Start by looking at exit transitions on the last active (top) camera mode.
		const FCameraModeEntry& TopEntry = Entries.Top();

		const UCameraEvaluationContext* FromContext = TopEntry.EvaluationContext.Get();
		const UCameraAsset* FromCameraAsset = FromContext ? FromContext->GetCameraAsset() : nullptr;
		const UCameraMode* FromCameraMode = TopEntry.OriginalCameraMode;

		if (!TopEntry.bIsFrozen)
		{
			// Look for exit transitions on the last active camera mode itself.
			TransitionToUse = FindTransition(
					FromCameraMode->ExitTransitions,
					FromCameraMode, FromCameraAsset, false,
					ToCameraMode, ToCameraAsset);
			if (TransitionToUse)
			{
				return TransitionToUse;
			}
			
			// Look for exit transitions on its parent camera asset.
			if (FromCameraAsset)
			{
				TransitionToUse = FindTransition(
						FromCameraAsset->ExitTransitions,
						FromCameraMode, FromCameraAsset, false,
						ToCameraMode, ToCameraAsset);
				if (TransitionToUse)
				{
					return TransitionToUse;
				}
			}
		}

		// Now look at enter transitions on the new camera mode.
		TransitionToUse = FindTransition(
				ToCameraMode->EnterTransitions,
				FromCameraMode, FromCameraAsset, TopEntry.bIsFrozen,
				ToCameraMode, ToCameraAsset);
		if (TransitionToUse)
		{
			return TransitionToUse;
		}

		// Look at enter transitions on its parent camera asset.
		if (ToCameraAsset)
		{
			TransitionToUse = FindTransition(
					ToCameraAsset->EnterTransitions,
					FromCameraMode, FromCameraAsset, TopEntry.bIsFrozen,
					ToCameraMode, ToCameraAsset);
			if (TransitionToUse)
			{
				return TransitionToUse;
			}
		}
	}
	else if (bBlendFirstCameraMode)
	{
		return FindTransition(
				ToCameraMode->EnterTransitions,
				nullptr, nullptr, false,
				ToCameraMode, ToCameraAsset);
	}

	return nullptr;
}

const FCameraModeTransition* UBlendStackCameraNode::FindTransition(
			TArrayView<const FCameraModeTransition> Transitions, 
			const UCameraMode* FromCameraMode, const UCameraAsset* FromCameraAsset, bool bFromFrozen,
			const UCameraMode* ToCameraMode, const UCameraAsset* ToCameraAsset) const
{
	FCameraModeTransitionConditionMatchParams MatchParams;
	MatchParams.FromCameraMode = FromCameraMode;
	MatchParams.FromCameraAsset = FromCameraAsset;
	MatchParams.ToCameraMode = ToCameraMode;
	MatchParams.ToCameraAsset = ToCameraAsset;

	// The transition should be used if all its conditions pass.
	for (const FCameraModeTransition& Transition : Transitions)
	{
		bool bConditionsPass = true;
		for (const UCameraModeTransitionCondition* Condition : Transition.Conditions)
		{
			if (!Condition->TransitionMatches(MatchParams))
			{
				bConditionsPass = false;
				break;
			}
		}

		if (bConditionsPass)
		{
			return &Transition;
		}
	}

	return nullptr;
}

