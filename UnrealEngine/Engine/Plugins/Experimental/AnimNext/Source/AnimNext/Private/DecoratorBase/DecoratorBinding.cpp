// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorBase/DecoratorBinding.h"

#include "DecoratorBase/IDecoratorInterface.h"

namespace UE::AnimNext
{
	FDecoratorInterfaceUID FDecoratorBinding::GetInterfaceUID() const
	{
		return Interface != nullptr ? Interface->GetInterfaceUID() : FDecoratorInterfaceUID();
	}
}
