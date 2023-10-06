// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Param/ParamType.h"
#include "UObject/Interface.h"
#include "IAnimNextParameterBlockGraphInterface.generated.h"

class URigVMGraph;
class UAnimNextParameterBlock_EditorData;

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
class ANIMNEXTUNCOOKEDONLY_API UAnimNextParameterBlockGraphInterface : public UInterface
{
	GENERATED_BODY()
};

class ANIMNEXTUNCOOKEDONLY_API IAnimNextParameterBlockGraphInterface
{
	GENERATED_BODY()

	friend class UAnimNextParameterBlock_EditorData;
	friend class UE::AnimNext::Editor::SParameterBlockView;
	friend class UE::AnimNext::Editor::SParameterBlockViewRow;
	friend struct UE::AnimNext::UncookedOnly::FUtilsPrivate;

	// Get the graph
	virtual URigVMGraph* GetGraph() const = 0;
};