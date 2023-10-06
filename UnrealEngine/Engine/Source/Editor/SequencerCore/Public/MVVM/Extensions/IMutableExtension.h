// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVM/ViewModelTypeID.h"

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

	/** Returns whether this item is muted, or is in a muted branch */
	virtual bool IsMuted() const = 0;
};

} // namespace Sequencer
} // namespace UE

