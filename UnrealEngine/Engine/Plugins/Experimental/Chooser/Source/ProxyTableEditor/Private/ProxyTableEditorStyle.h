// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

namespace UE::ProxyTableEditor
{
	
class FProxyTableEditorStyle : FSlateStyleSet
{
public:
    FProxyTableEditorStyle();
	static void Initialize();
	static void Shutdown();

	static const ISlateStyle& Get();

private:

	static TSharedPtr< FProxyTableEditorStyle > StyleInstance;
};

}
