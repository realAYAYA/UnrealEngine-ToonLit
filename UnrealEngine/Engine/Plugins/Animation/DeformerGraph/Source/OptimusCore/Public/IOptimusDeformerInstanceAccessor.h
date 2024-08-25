// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "IOptimusDeformerInstanceAccessor.generated.h"


class UOptimusDeformerInstance;

UINTERFACE()
class OPTIMUSCORE_API UOptimusDeformerInstanceAccessor :
	public UInterface
{
	GENERATED_BODY()
};


class IOptimusDeformerInstanceAccessor
{
	GENERATED_BODY()

public:
	virtual void SetDeformerInstance(UOptimusDeformerInstance* InInstance) = 0;
	virtual UOptimusDeformerInstance* GetDeformerInstance() const = 0;
};
