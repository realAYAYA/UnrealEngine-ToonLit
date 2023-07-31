// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVM/ViewModelTypeID.h"

namespace UE
{
namespace Sequencer
{

class SEQUENCERCORE_API IResizableExtension
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(IResizableExtension)

	virtual ~IResizableExtension(){}

	virtual bool IsResizable() const = 0;
	virtual void Resize(float NewSize) = 0;
};

} // namespace Sequencer
} // namespace UE

