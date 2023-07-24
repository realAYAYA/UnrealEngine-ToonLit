// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

namespace UE::Zen { class FZenServiceInstance; }

namespace UE::Zen
{

class FServiceInstanceManager
{
public:
	TSharedPtr<FZenServiceInstance> GetZenServiceInstance() const;
private:
	mutable uint16 CurrentPort = 0;
	mutable TSharedPtr<FZenServiceInstance> CurrentInstance;
};

} // namespace UE::Zen

