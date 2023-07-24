// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/ICastable.h"

namespace UE
{
namespace Sequencer
{

void* ICastable::CastRaw(FViewModelTypeID InType)
{
	const void* Result = static_cast<const ICastable*>(this)->CastRaw(InType);
	return const_cast<void*>(Result);
}

const void* ICastable::CastRaw(FViewModelTypeID InType) const
{
	const void* ExtensionPtr = nullptr;
	CastImpl(InType, ExtensionPtr);
	return ExtensionPtr;
}

} // namespace Sequencer
} // namespace UE

