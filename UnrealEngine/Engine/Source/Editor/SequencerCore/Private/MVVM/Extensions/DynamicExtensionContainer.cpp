// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/Extensions/DynamicExtensionContainer.h"
#include "MVVM/CastableTypeTable.h"

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

FDynamicExtensionContainerIterator::FDynamicExtensionContainerIterator(IteratorType&& InIterator, FViewModelTypeID InType)
	: CurrentExtension(nullptr)
	, Iterator(MoveTemp(InIterator))
	, Type(InType)
{
	for ( ; Iterator; ++Iterator)
	{
		CurrentExtension = const_cast<void*>(Iterator->TypeTable->Cast(&Iterator->Extension.Get(), Type.GetTypeID()));
		if (CurrentExtension)
		{
			break;
		}
	}
}

FDynamicExtensionContainerIterator& FDynamicExtensionContainerIterator::operator++()
{
	++Iterator;

	for (CurrentExtension = nullptr; Iterator; ++Iterator)
	{
		CurrentExtension = const_cast<void*>(Iterator->TypeTable->Cast(&Iterator->Extension.Get(), Type.GetTypeID()));
		if (CurrentExtension)
		{
			break;
		}
	}

	return *this;
}

bool operator!=(const FDynamicExtensionContainerIterator& A, const FDynamicExtensionContainerIterator& B)
{
	return A.Iterator != B.Iterator || A.Type != B.Type;
}

} // namespace UE::Sequencer

