// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/Extensions/DynamicExtensionContainer.h"

namespace UE::Sequencer
{

const void* FDynamicExtensionContainer::CastDynamic(FViewModelTypeID Type) const
{
	for (const FDynamicExtensionInfo& DynamicExtension : DynamicExtensions)
	{
		if (const void* Result = DynamicExtension.TypeTable->Cast(&DynamicExtension.Extension.Get(), Type.GetTypeID()))
		{
			return Result;
		}
	}
	return nullptr;
}

} // namespace UE::Sequencer

