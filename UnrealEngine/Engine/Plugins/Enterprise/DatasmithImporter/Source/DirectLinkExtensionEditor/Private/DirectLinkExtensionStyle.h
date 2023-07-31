// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateStyle.h"

class ISlateStyle;
class FSlateStyleSet;

namespace UE::DatasmithImporter
{
	class FDirectLinkExtensionStyle
	{
	public:
		static void Initialize();

		static void Shutdown();

		static const ISlateStyle& Get();

		static FName GetStyleSetName();
	private:
		static FString InContent(const FString& RelativePath, const TCHAR* Extension);

	private:
		static TUniquePtr<FSlateStyleSet> StyleSet;
	};
}
