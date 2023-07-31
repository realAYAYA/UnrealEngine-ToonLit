// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/Views/STreeView.h"
#include "MVVM/ViewModels/ViewModel.h"

namespace UE
{
namespace Sequencer
{

class ISequencerTreeView : public STreeView<TWeakPtr<FViewModel>>
{
};

} // namespace Sequencer
} // namespace UE
