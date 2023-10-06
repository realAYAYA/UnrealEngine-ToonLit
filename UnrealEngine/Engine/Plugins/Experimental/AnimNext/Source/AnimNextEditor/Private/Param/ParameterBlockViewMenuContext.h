// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ParameterBlockViewMenuContext.generated.h"

namespace UE::AnimNext::Editor
{
	class SParameterBlockView;
}

UCLASS()
class UParameterBlockViewMenuContext : public UObject
{
	GENERATED_BODY()

	friend class UE::AnimNext::Editor::SParameterBlockView;
	
	// The parameter block view that we are editing
	TWeakPtr<UE::AnimNext::Editor::SParameterBlockView> ParameterBlockView;
};