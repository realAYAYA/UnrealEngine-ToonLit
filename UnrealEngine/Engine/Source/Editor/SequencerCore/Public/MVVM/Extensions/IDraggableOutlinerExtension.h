// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVM/ViewModelTypeID.h"
#include "Misc/FrameNumber.h"

struct FGuid;

namespace UE
{
namespace Sequencer
{


/**
 * Extension for models that can be dragged
 */
class SEQUENCERCORE_API IDraggableOutlinerExtension
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(IDraggableOutlinerExtension)

	virtual ~IDraggableOutlinerExtension(){}

	/** Returns whether the model can be dragged */
	virtual bool CanDrag() const = 0;
};

} // namespace Sequencer
} // namespace UE

