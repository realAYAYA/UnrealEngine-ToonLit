// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/Extensions/IPinnableExtension.h"

#include "MVVM/ViewModelPtr.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/ViewModels/ViewModelIterators.h"
#include "SequencerCoreFwd.h"

namespace UE
{
namespace Sequencer
{

bool FPinnableExtensionShim::IsPinned() const
{
	return bIsPinned || bIsInPinnedBranch;
}

void FPinnableExtensionShim::SetPinned(bool bInIsPinned)
{
	bIsPinned = bInIsPinned;
}

void FPinnableExtensionShim::ReportPinnedParent(bool bInIsInPinnedBranch)
{
	bIsInPinnedBranch = bInIsInPinnedBranch;
}

void FPinnableExtensionShim::UpdateCachedPinnedState(TSharedPtr<FViewModel> RootModel)
{
	// Only go through the root-level items, and mark their children as pinned or not
	const bool bIncludeRootModel = true;
	for (const TViewModelPtr<IPinnableExtension>& Item : RootModel->GetChildrenOfType<IPinnableExtension>())
	{
		const bool bIsParentPinned = Item->IsPinned();

		// Report parent state
		for (const TViewModelPtr<IPinnableExtension>& Child : Item.AsModel()->GetDescendantsOfType<IPinnableExtension>())
		{
			Child->ReportPinnedParent(bIsParentPinned);
		}
	}
}

} // namespace Sequencer
} // namespace UE

