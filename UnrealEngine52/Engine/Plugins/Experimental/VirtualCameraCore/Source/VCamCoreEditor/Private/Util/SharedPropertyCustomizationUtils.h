// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IPropertyHandle;
class IDetailPropertyRow;
class UVCamStateSwitcherWidget;
struct FSlateFontInfo;

namespace UE::VCamCoreEditor::Private::StateSwitcher
{
	/** Shared logic for customizing UVCamStateSwitcherWidget::CurrentState to display a drop-down widget. */
	void CustomizeCurrentState(UVCamStateSwitcherWidget& StateSwitcher, IDetailPropertyRow& DetailPropertyRow, TSharedRef<IPropertyHandle> CurrentStatePropertyHandle, const FSlateFontInfo& RegularFont);
}
