// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVM/ViewModelTypeID.h"

namespace UE
{
namespace Sequencer
{

class SEQUENCERCORE_API IDimmableExtension
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(IDimmableExtension)

	virtual ~IDimmableExtension(){}

	virtual bool IsDimmed() const = 0;
};

} // namespace Sequencer
} // namespace UE

