// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieGraphUtils.h"

namespace UE::MoviePipeline::RenderGraph
{
	FString GetUniqueName(const TArray<FString>& InExistingNames, const FString& InBaseName)
	{
		int32 Postfix = 0;
		FString NewName = InBaseName;

		while (InExistingNames.Contains(NewName))
		{
			Postfix++;
			NewName = FString::Format(TEXT("{0} {1}"), {InBaseName, Postfix});
		}

		return NewName;
	}
}