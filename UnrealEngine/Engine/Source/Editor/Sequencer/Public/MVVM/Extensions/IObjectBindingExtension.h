// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVM/ViewModelTypeID.h"

struct FGuid;

namespace UE
{
namespace Sequencer
{

class SEQUENCER_API IObjectBindingExtension
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(IObjectBindingExtension)

	virtual ~IObjectBindingExtension(){}

	virtual FGuid GetObjectGuid() const = 0;
};

} // namespace Sequencer
} // namespace UE

