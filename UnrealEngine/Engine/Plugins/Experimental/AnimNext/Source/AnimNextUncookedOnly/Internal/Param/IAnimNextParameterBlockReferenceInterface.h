﻿// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Param/ParamType.h"
#include "UObject/Interface.h"
#include "IAnimNextParameterBlockReferenceInterface.generated.h"

class URigVMGraph;
class UAnimNextParameter;
class UAnimNextParameterBlock;

namespace UE::AnimNext::Editor
{
class SParameterBlockView;
class SParameterBlockViewRow;
}

namespace UE::AnimNext::UncookedOnly
{
struct FUtilsPrivate;
}

UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class ANIMNEXTUNCOOKEDONLY_API UAnimNextParameterBlockReferenceInterface : public UInterface
{
	GENERATED_BODY()
};

class ANIMNEXTUNCOOKEDONLY_API IAnimNextParameterBlockReferenceInterface
{
	GENERATED_BODY()

	friend class UE::AnimNext::Editor::SParameterBlockView;
	friend class UE::AnimNext::Editor::SParameterBlockViewRow;
	friend struct UE::AnimNext::UncookedOnly::FUtilsPrivate;

	// Get the block we reference
	virtual const UAnimNextParameterBlock* GetBlock() const = 0;
};