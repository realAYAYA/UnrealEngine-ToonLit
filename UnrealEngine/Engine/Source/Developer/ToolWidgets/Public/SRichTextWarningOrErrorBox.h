// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Layout/SBorder.h"
#include "SWarningOrErrorBox.h"

class TOOLWIDGETS_API SRichTextWarningOrErrorBox : public SWarningOrErrorBox
{
public:

	void Construct(const FArguments& InArgs);

private:
	TAttribute<EMessageStyle> MessageStyle;
};