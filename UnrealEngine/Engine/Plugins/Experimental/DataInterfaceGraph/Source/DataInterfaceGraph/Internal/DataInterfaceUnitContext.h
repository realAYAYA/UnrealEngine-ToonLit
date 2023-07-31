// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataInterfaceContext.h"
#include "Units/RigUnitContext.h"

struct FDataInterfaceUnitContext : public FRigUnitContext
{
	FDataInterfaceUnitContext(const IDataInterface* InInterface, const UE::DataInterface::FContext& InDataInterfaceContext, bool& bInResult)
		: Interface(InInterface)
		, DataInterfaceContext(InDataInterfaceContext)
		, bResult(bInResult)
	{}

	const IDataInterface* Interface = nullptr;
	const UE::DataInterface::FContext& DataInterfaceContext;
	bool& bResult;
};
