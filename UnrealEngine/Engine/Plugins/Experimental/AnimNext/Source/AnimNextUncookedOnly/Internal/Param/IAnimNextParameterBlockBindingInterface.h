// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Param/ParamType.h"
#include "UObject/Interface.h"
#include "IAnimNextParameterBlockBindingInterface.generated.h"

class UAnimNextParameter;
class UAnimNextParameterLibrary;
class UAnimNextParameterBlock_EditorData;

namespace UE::AnimNext::Editor
{
	class SParameterBlockView;
	class SParameterBlockViewRow;
}

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class ANIMNEXTUNCOOKEDONLY_API UAnimNextParameterBlockBindingInterface : public UInterface
{
	GENERATED_BODY()
};

class ANIMNEXTUNCOOKEDONLY_API IAnimNextParameterBlockBindingInterface
{
	GENERATED_BODY()

	friend class UAnimNextParameterBlock_EditorData;
	friend class UE::AnimNext::Editor::SParameterBlockView;
	friend class UE::AnimNext::Editor::SParameterBlockViewRow;
	friend struct UE::AnimNext::UncookedOnly::FUtils;

	// Get the parameter type
	virtual FAnimNextParamType GetParamType() const = 0;

	// Get the parameter name
	virtual FName GetParameterName() const = 0;

	// Set the parameter name
	virtual void SetParameterName(FName InName, bool bSetupUndoRedo = true) = 0;

	// Resolve the underlying parameter in the library
	virtual const UAnimNextParameter* GetParameter() const = 0;

	// Get the parameter library hosting the parameter
	virtual const UAnimNextParameterLibrary* GetLibrary() const = 0;
};