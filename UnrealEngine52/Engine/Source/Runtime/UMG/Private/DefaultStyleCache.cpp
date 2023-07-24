// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefaultStyleCache.h"

#include "Misc/LazySingleton.h"
#include "Styling/SlateTypes.h"
#include "Styling/UMGCoreStyle.h"

#if WITH_EDITOR
#include "Styling/CoreStyle.h"
#endif

FDefaultStyleCache& FDefaultStyleCache::Get()
{
	return TLazySingleton<FDefaultStyleCache>::Get();
}

FDefaultStyleCache::FDefaultStyleCache()
{
	if (!IsRunningDedicatedServer())
	{
		Runtime.ScrollBarStyle = FUMGCoreStyle::Get().GetWidgetStyle<FScrollBarStyle>("ScrollBar");
		Runtime.ScrollBarStyle.UnlinkColors();
		Runtime.ComboBoxStyle = FUMGCoreStyle::Get().GetWidgetStyle<FComboBoxStyle>("ComboBox");
		Runtime.ComboBoxStyle.UnlinkColors();
		Runtime.ComboBoxRowStyle = FUMGCoreStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row");
		Runtime.ComboBoxRowStyle.UnlinkColors();

#if WITH_EDITOR
		Editor.ScrollBarStyle = FCoreStyle::Get().GetWidgetStyle<FScrollBarStyle>("ScrollBar");
		Editor.ScrollBarStyle.UnlinkColors();
		Editor.ComboBoxStyle = FCoreStyle::Get().GetWidgetStyle<FComboBoxStyle>("EditorUtilityComboBox");
		Editor.ComboBoxStyle.UnlinkColors();
		Editor.ComboBoxRowStyle = FCoreStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row");
		Editor.ComboBoxRowStyle.UnlinkColors();
#endif
	}
}
