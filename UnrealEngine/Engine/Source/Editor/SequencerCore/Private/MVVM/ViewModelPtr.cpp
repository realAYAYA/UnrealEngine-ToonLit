// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/ViewModelPtr.h"

namespace UE
{
namespace Sequencer
{

FImplicitViewModelCast FViewModelPtr::ImplicitCast() const
{
	return FImplicitViewModelCast{Model};
}

FImplicitWeakViewModelCast FWeakViewModelPtr::ImplicitCast() const
{
	return FImplicitWeakViewModelCast{WeakModel};
}

FImplicitWeakViewModelPin FWeakViewModelPtr::ImplicitPin() const
{
	return FImplicitWeakViewModelPin{WeakModel};
}


} // namespace Sequencer
} // namespace UE

