// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVM/ViewModelTypeID.h"
#include "Templates/SharedPointer.h"

namespace UE
{
namespace Sequencer
{

class SEQUENCERCORE_API IHotspotExtension
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(IHotspotExtension)

	virtual ~IHotspotExtension(){}
};

} // namespace Sequencer
} // namespace UE

