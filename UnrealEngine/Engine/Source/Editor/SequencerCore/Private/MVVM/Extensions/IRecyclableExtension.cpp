// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/Extensions/IRecyclableExtension.h"
#include "MVVM/ViewModelPtr.h"

namespace UE
{
namespace Sequencer
{

void IRecyclableExtension::CallOnRecycle(const TViewModelPtr<IRecyclableExtension>& RecyclableItem)
{
	if (RecyclableItem)
	{
		RecyclableItem->OnRecycle();
	}
}

} // namespace Sequencer
} // namespace UE

