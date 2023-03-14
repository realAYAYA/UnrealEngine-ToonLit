// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/Extensions/LinkedOutlinerExtension.h"

#include "MVVM/Extensions/IOutlinerExtension.h"

namespace UE
{
namespace Sequencer
{

FLinkedOutlinerExtension::FLinkedOutlinerExtension()
{
}

TViewModelPtr<IOutlinerExtension> FLinkedOutlinerExtension::GetLinkedOutlinerItem() const
{
	return WeakModel.Pin();
}

void FLinkedOutlinerExtension::SetLinkedOutlinerItem(const TViewModelPtr<IOutlinerExtension>& InOutlinerItem)
{
	WeakModel = InOutlinerItem;
}

} // namespace Sequencer
} // namespace UE

