// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraTypes.h"
#include "EdGraph/EdGraphPin.h"

class FNiagaraBlueprintUtil
{
public:
	/**
	Attempts to convert the existing Niagara type to a matching type for BP pins
	*/
	static NIAGARABLUEPRINTNODES_API FEdGraphPinType TypeDefinitionToBlueprintType(const FNiagaraTypeDefinition& TypeDef);
};
