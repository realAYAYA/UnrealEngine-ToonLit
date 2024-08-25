// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/ISlateStyle.h"
#include "Styling/SlateStyle.h"

namespace UE::ConcertReplicationScriptingEditor
{
	class FReplicationScriptingStyle
	{
	public:
	
		static void Initialize();
		static void Shutdown();

		static TSharedPtr<ISlateStyle> Get();
		static FName GetStyleSetName();
	
	private:
	
		static TSharedPtr<FSlateStyleSet> StyleSet;
	};
}


