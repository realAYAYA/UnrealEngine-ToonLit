// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/Text.h"
#include "MovieSceneClipboard.h"
#include "UObject/NameTypes.h"

namespace MovieSceneClipboard
{
	template<> inline FName GetKeyTypeName<FText>()
	{
		static FName Name("Text");
		return Name;
	}
}
