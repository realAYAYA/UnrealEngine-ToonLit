// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextInterfaceContext.h"
#include "Units/RigUnitContext.h"

struct FAnimNextInterfaceUnitContext : public FRigUnitContext
{
	FAnimNextInterfaceUnitContext()
		: FRigUnitContext()
		, Interface(nullptr)
		, AnimNextInterfaceContext(nullptr)
		, bResult(nullptr)
	{}

	FAnimNextInterfaceUnitContext(const IAnimNextInterface* InInterface, const UE::AnimNext::Interface::FContext& InAnimNextInterfaceContext, bool& bInResult)
		: FRigUnitContext()
		, Interface(InInterface)
		, AnimNextInterfaceContext(&InAnimNextInterfaceContext)
		, bResult(&bInResult)
	{}

	const IAnimNextInterface* Interface;
	const UE::AnimNext::Interface::FContext* AnimNextInterfaceContext;
	bool* bResult;
};
