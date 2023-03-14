// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Framework/PropertyViewer/PropertyPath.h"
#include "Framework/PropertyViewer/PropertyValueFactory.h"


namespace UE::PropertyViewer
{

class INotifyHook;

/** */
class ADVANCEDWIDGETS_API SStringPropertyValue : public SCompoundWidget
{
public:
	static TSharedPtr<SWidget> CreateInstance(const FPropertyValueFactory::FGenerateArgs Args);

public:
	SLATE_BEGIN_ARGS(SStringPropertyValue) {}
		SLATE_ARGUMENT(FPropertyPath, Path);
		SLATE_ARGUMENT(INotifyHook*, NotifyHook);
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FText GetText() const;
	void OnTextCommitted(const FText& InText, ETextCommit::Type InCommitType);

	FPropertyPath Path;
	INotifyHook* NotifyHook = nullptr;
};

} //namespace
