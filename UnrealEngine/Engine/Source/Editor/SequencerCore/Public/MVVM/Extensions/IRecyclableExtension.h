// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "SequencerCoreFwd.h"
#include "MVVM/ViewModelTypeID.h"

namespace UE
{
namespace Sequencer
{

class SEQUENCERCORE_API IRecyclableExtension
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(IRecyclableExtension)

	static void CallOnRecycle(const TViewModelPtr<IRecyclableExtension>& RecyclableItem);

	virtual ~IRecyclableExtension(){}

	virtual void OnRecycle() = 0;
};

} // namespace Sequencer
} // namespace UE

