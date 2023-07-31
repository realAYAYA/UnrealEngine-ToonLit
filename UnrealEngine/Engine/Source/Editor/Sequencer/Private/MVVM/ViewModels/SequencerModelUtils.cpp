// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/ViewModels/SequencerModelUtils.h"

#include "Algo/Reverse.h"
#include "HAL/PlatformCrt.h"
#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/Extensions/ITrackExtension.h"
#include "MVVM/ViewModelPtr.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/ViewModels/ViewModelIterators.h"
#include "SequencerCoreFwd.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

namespace UE
{
namespace Sequencer
{

TViewModelPtr<ITrackExtension> GetParentTrackNodeAndNamePath(const TViewModelPtr<IOutlinerExtension>& Node, TArray<FName>& OutNamePath)
{
	using namespace UE::Sequencer;

	OutNamePath.Add(Node->GetIdentifier());

	for (const TViewModelPtr<IOutlinerExtension>& Parent : Node.AsModel()->GetAncestorsOfType<IOutlinerExtension>())
	{
		if (TViewModelPtr<ITrackExtension> Track = Parent.ImplicitCast())
		{
			Algo::Reverse(OutNamePath);
			return Track;
		}

		OutNamePath.Add(Parent->GetIdentifier());
	}

	OutNamePath.Empty();
	return nullptr;
}

} // namespace Sequencer
} // namespace UE
