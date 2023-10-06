// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE::MoviePipeline::RenderGraph
{
	/**
	 * Generate a unique name given a set of existing names and the desired base name. The base name will
	 * be given a postfix value if it conflicts with an existing name (eg, if the base name is "Foo" but
	 * there's already an existing name "Foo", the generated name would be "Foo 1").
	 */
	FString GetUniqueName(const TArray<FString>& InExistingNames, const FString& InBaseName);
}
