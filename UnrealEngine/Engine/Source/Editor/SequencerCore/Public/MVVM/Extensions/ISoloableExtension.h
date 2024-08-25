// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "MVVM/ViewModelTypeID.h"
#include "MVVM/Extensions/HierarchicalCacheExtension.h"

namespace UE
{
namespace Sequencer
{

/**
 * An extension for outliner nodes that can be made 'solo'
 */
class SEQUENCERCORE_API ISoloableExtension
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(ISoloableExtension)

	virtual ~ISoloableExtension(){}

	/** Returns whether this item is solo */
	virtual bool IsSolo() const = 0;

	/** Enable or disable solo for this object */
	virtual void SetIsSoloed(bool bIsSoloed) = 0;
};

enum class ECachedSoloState
{
	None                     = 0,

	Soloable                 = 1 << 0,
	SoloableChildren         = 1 << 1,
	Soloed                   = 1 << 2,
	PartiallySoloedChildren  = 1 << 3,
	ImplicitlySoloedByParent = 1 << 4,

	InheritedFromChildren = SoloableChildren | PartiallySoloedChildren,
};
ENUM_CLASS_FLAGS(ECachedSoloState)

class SEQUENCERCORE_API FSoloStateCacheExtension
	: public TFlagStateCacheExtension<ECachedSoloState>
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(FSoloStateCacheExtension)

	using Implements = TImplements<IDynamicExtension, IHierarchicalCache>;

	virtual ~FSoloStateCacheExtension() {}

private:

	ECachedSoloState ComputeFlagsForModel(const FViewModelPtr& ViewModel) override;
	void PostComputeChildrenFlags(const FViewModelPtr& ViewModel, ECachedSoloState& OutThisModelFlags, ECachedSoloState& OutPropagateToParentFlags) override;
};

} // namespace Sequencer
} // namespace UE

