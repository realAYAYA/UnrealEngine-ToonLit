// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "MVVM/ViewModelTypeID.h"

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

	/** Returns whether this item is solo, or in a solo branch */
	virtual bool IsSolo() const = 0;
};

} // namespace Sequencer
} // namespace UE

