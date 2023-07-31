// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeLayout.h"

class UEdGraphPin;
struct FMutableGraphGenerationContext;


mu::NodeLayoutPtr GenerateMutableSourceLayout(const UEdGraphPin* Pin, FMutableGraphGenerationContext& GenerationContext);
