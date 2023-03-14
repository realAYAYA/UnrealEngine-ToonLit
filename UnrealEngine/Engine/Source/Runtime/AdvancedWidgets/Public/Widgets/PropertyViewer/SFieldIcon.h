// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Framework/PropertyViewer/FieldIconFinder.h"


namespace UE::PropertyViewer
{

/** */
class ADVANCEDWIDGETS_API SFieldIcon : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFieldIcon) {}
		SLATE_ARGUMENT(TOptional<const FFieldColorSettings>, OverrideColorSettings);
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const UClass* Class);
	void Construct(const FArguments& InArgs, const UScriptStruct* Struct);
	void Construct(const FArguments& InArgs, const FProperty* Property);
	void Construct(const FArguments& InArgs, const UFunction* FunctionToDisplay);
};

} //namespace
