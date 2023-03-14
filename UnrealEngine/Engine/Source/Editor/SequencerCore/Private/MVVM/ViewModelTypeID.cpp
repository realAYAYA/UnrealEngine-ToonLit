// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/ViewModelTypeID.h"

namespace UE
{
namespace Sequencer
{

FViewModelTypeID FViewModelTypeID::RegisterNew()
{
	static uint32 ID = 0;
	return FViewModelTypeID(ID++);
}

FViewModelTypeID FViewModelTypeID::Invalid()
{
	return FViewModelTypeID();
}

} // namespace Sequencer
} // namespace UE

