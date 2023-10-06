// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "MVVM/ViewModels/ViewModelHierarchy.h"
#include "SequencerCoreFwd.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"

class FDelegateHandle;

namespace UE
{
namespace Sequencer
{

class FViewModel;

class SEQUENCERCORE_API FSharedViewModelData : public TSharedFromThis<FSharedViewModelData>
{
public:
	void PreHierarchicalChange(const TSharedPtr<FViewModel>& InChangedModel);
	void BroadcastHierarchicalChange(const TSharedPtr<FViewModel>& InChangedModel);

	void ReportLatentHierarchicalOperations();

	FSimpleMulticastDelegate& SubscribeToHierarchyChanged(const TSharedPtr<FViewModel>& InModel);
	void UnsubscribeFromHierarchyChanged(const TSharedPtr<FViewModel>& InModel, FDelegateHandle InHandle);
	void UnsubscribeFromHierarchyChanged(const TSharedPtr<FViewModel>& InModel, const void* InUserObject);

	void PurgeStaleHandlers();

private:
	friend FViewModelHierarchyOperation;

	TMap<TWeakPtr<FViewModel>, FSimpleMulticastDelegate> HierarchyChangedEventsByModel;

	FViewModelHierarchyOperation* CurrentHierarchicalOperation = nullptr;
	TUniquePtr<FViewModelHierarchyOperation> LatentOperation;
};


} // namespace Sequencer
} // namespace UE

