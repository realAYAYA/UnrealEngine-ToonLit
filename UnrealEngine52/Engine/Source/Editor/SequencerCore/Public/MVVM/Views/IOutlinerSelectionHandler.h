// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/Extensions/IOutlinerExtension.h"

namespace UE
{
namespace Sequencer
{

class IOutlinerExtension;

class SEQUENCERCORE_API IOutlinerSelectionHandler
	: public TSharedFromThis<IOutlinerSelectionHandler>
{
public:

	virtual ~IOutlinerSelectionHandler(){}

	virtual void SelectOutlinerItems(const TArrayView<TWeakViewModelPtr<IOutlinerExtension>>& Items, bool bRightMouseButtonDown) = 0;
};

} // namespace Sequencer
} // namespace UE

