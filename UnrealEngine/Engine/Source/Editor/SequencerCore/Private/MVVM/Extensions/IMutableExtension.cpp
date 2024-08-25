// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/Extensions/IMutableExtension.h"

namespace UE::Sequencer
{

ECachedMuteState FMuteStateCacheExtension::ComputeFlagsForModel(const FViewModelPtr& ViewModel)
{
	ECachedMuteState ParentFlags = IndividualItemFlags.Last();
	ECachedMuteState ThisModelFlags = ECachedMuteState::None;

	if (EnumHasAnyFlags(ParentFlags, ECachedMuteState::Muted | ECachedMuteState::ImplicitlyMutedByParent))
	{
		ThisModelFlags |= ECachedMuteState::ImplicitlyMutedByParent;
	}

	if (TViewModelPtr<IMutableExtension> Mutable = ViewModel.ImplicitCast())
	{
		ThisModelFlags |= ECachedMuteState::Mutable;
		if (Mutable->IsMuted())
		{
			ThisModelFlags |= ECachedMuteState::Muted;
		}
	}

	return ThisModelFlags;
}

void FMuteStateCacheExtension::PostComputeChildrenFlags(const FViewModelPtr& ViewModel, ECachedMuteState& OutThisModelFlags, ECachedMuteState& OutPropagateToParentFlags)
{
	// --------------------------------------------------------------------
	// Handle mute state propagation
	const bool bIsMutable              = EnumHasAnyFlags(OutThisModelFlags, ECachedMuteState::Mutable);
	const bool bIsMuted                = EnumHasAnyFlags(OutThisModelFlags, ECachedMuteState::Muted);
	const bool bHasMutableChildren     = EnumHasAnyFlags(OutThisModelFlags, ECachedMuteState::MutableChildren);
	const bool bSiblingsPartiallyMuted = EnumHasAnyFlags(OutPropagateToParentFlags, ECachedMuteState::PartiallyMutedChildren);
	const bool bSiblingsFullyMuted     = EnumHasAnyFlags(OutPropagateToParentFlags, ECachedMuteState::Muted);
	const bool bHasAnyMutableSiblings  = EnumHasAnyFlags(OutPropagateToParentFlags, ECachedMuteState::MutableChildren);

	if (bIsMuted)
	{
		if (!bSiblingsPartiallyMuted)
		{
			if (bHasAnyMutableSiblings && !bSiblingsFullyMuted)
			{
				// This is the first muted mutable within the parent, but we know there are already other mutables so this has to be partially muted
				OutPropagateToParentFlags |= ECachedMuteState::PartiallyMutedChildren;
			}
			else
			{
				// Parent is (currently) fully muted because it contains no other mutable children, and is not already partially muted
				OutPropagateToParentFlags |= ECachedMuteState::Muted;
			}
		}
	}
	else if (bIsMutable && bSiblingsFullyMuted)
	{
		// Parent is no longer fully Muted because it contains an unmuted Mutable
		OutPropagateToParentFlags |= ECachedMuteState::PartiallyMutedChildren;
		OutPropagateToParentFlags &= ~ECachedMuteState::Muted;
	}

	if (bIsMutable)
	{
		OutPropagateToParentFlags |= ECachedMuteState::MutableChildren;
	}
}

} // namespace UE::Sequencer

