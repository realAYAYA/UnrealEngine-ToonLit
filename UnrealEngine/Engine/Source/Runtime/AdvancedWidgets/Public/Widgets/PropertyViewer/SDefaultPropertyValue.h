// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Framework/PropertyViewer/PropertyPath.h"
#include "Framework/PropertyViewer/PropertyValueFactory.h"


namespace UE::PropertyViewer
{

/** */
class ADVANCEDWIDGETS_API SDefaultPropertyValue : public SCompoundWidget
{
public:
	static TSharedPtr<SWidget> CreateInstance(const FPropertyValueFactory::FGenerateArgs Args);

public:
	SLATE_BEGIN_ARGS(SDefaultPropertyValue) {}
		SLATE_ARGUMENT(FPropertyPath, Path);
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FText GetText() const;

	FPropertyPath Path;
};

} //namespace
