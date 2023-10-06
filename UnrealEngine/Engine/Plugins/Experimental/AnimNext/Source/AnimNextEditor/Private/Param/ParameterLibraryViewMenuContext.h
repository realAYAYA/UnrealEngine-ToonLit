// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ParameterLibraryViewMenuContext.generated.h"

namespace UE::AnimNext::Editor
{
class SParameterLibraryView;
}

UCLASS()
class UParameterLibraryViewMenuContext : public UObject
{
	GENERATED_BODY()

	friend class UE::AnimNext::Editor::SParameterLibraryView;
	
	// The parameter library view that we are editing
	TWeakPtr<UE::AnimNext::Editor::SParameterLibraryView> ParameterLibraryView;
};