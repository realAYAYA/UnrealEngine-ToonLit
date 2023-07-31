// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Framework/PropertyViewer/PropertyPath.h"
#include "Framework/PropertyViewer/PropertyValueFactory.h"

enum class ECheckBoxState : uint8;

namespace UE::PropertyViewer
{

class INotifyHook;

/** */
class ADVANCEDWIDGETS_API SBoolPropertyValue : public SCompoundWidget
{
public:
	static TSharedPtr<SWidget> CreateInstance(const FPropertyValueFactory::FGenerateArgs Args);

public:
	SLATE_BEGIN_ARGS(SBoolPropertyValue) {}
		SLATE_ARGUMENT(FPropertyPath, Path);
		SLATE_ARGUMENT(INotifyHook*, NotifyHook);
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	ECheckBoxState HandleIsChecked() const;
	void HandleCheckStateChanged(ECheckBoxState NewState);

	FPropertyPath Path;
	INotifyHook* NotifyHook = nullptr;
};

} //namespace
