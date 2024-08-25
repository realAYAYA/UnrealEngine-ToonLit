// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVM/ViewModelTypeID.h"
#include "MVVM/Extensions/HierarchicalCacheExtension.h"

namespace UE
{
namespace Sequencer
{

/**
 * An extension for outliner nodes that can be muted
 */
class SEQUENCERCORE_API IMutableExtension
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(IMutableExtension)

	virtual ~IMutableExtension(){}

	/** Returns whether this item is muted */
	virtual bool IsMuted() const = 0;

	/** Set this item's mute state */
	virtual void SetIsMuted(bool bIsMuted) = 0;
};

enum class ECachedMuteState
{
	None                     = 0,

	Mutable                  = 1 << 0,
	MutableChildren          = 1 << 1,
	Muted                    = 1 << 2,
	PartiallyMutedChildren   = 1 << 3,
	ImplicitlyMutedByParent  = 1 << 4,

	InheritedFromChildren = MutableChildren | PartiallyMutedChildren,
};
ENUM_CLASS_FLAGS(ECachedMuteState)

class SEQUENCERCORE_API FMuteStateCacheExtension
	: public TFlagStateCacheExtension<ECachedMuteState>
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(FMuteStateCacheExtension);

private:

	ECachedMuteState ComputeFlagsForModel(const FViewModelPtr& ViewModel) override;
	void PostComputeChildrenFlags(const FViewModelPtr& ViewModel, ECachedMuteState& OutThisModelFlags, ECachedMuteState& OutPropagateToParentFlags) override;
};


} // namespace Sequencer
} // namespace UE

