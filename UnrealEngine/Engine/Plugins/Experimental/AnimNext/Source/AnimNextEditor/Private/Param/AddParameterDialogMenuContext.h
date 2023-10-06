// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AddParameterDialogMenuContext.generated.h"

namespace UE::AnimNext::Editor
{
	class SAddParametersDialog;
	struct FParameterToAdd;
}

UCLASS()
class UAddParameterDialogMenuContext : public UObject
{
	GENERATED_BODY()

	friend class UE::AnimNext::Editor::SAddParametersDialog;

	// The add parameter dialog that we are editing
	TWeakPtr<UE::AnimNext::Editor::SAddParametersDialog> AddParametersDialog;

	// The entry that we are editing
	TWeakPtr<UE::AnimNext::Editor::FParameterToAdd> Entry;
};