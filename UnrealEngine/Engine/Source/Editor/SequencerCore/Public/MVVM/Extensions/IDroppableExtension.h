// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVM/ViewModelTypeID.h"

class FText;

namespace UE
{
namespace Sequencer
{

class IDragOperation;

/**
 * Extension for models that can be dragged
 */
class SEQUENCERCORE_API IDroppableExtension
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(IDroppableExtension)

	virtual ~IDroppableExtension(){}

	/** Check whether this droppable extension is able to accept the specified drag */
	virtual bool CanAcceptDrag(IDragOperation& DragOperation, FText* OutInfo) const = 0;
	virtual void OnHover(IDragOperation& DragOperation) = 0;
	virtual void ProcessDragOperation(IDragOperation& DragOperation) = 0;
	virtual void OnUnhover(IDragOperation& DragOperation) = 0;
};

} // namespace Sequencer
} // namespace UE

