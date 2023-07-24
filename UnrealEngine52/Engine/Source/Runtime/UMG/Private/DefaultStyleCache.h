// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"

struct FDefaultStyleCache
{
	static FDefaultStyleCache& Get();
	
	const FScrollBarStyle& GetScrollBarStyle() const { return Runtime.ScrollBarStyle; }
	const FComboBoxStyle&  GetComboBoxStyle() const { return Runtime.ComboBoxStyle; }
	const FTableRowStyle&  GetComboBoxRowStyle() const { return Runtime.ComboBoxRowStyle; }

#if WITH_EDITOR
	const FScrollBarStyle& GetEditorScrollBarStyle() const { return Editor.ScrollBarStyle; }
	const FComboBoxStyle&  GetEditorComboBoxStyle() const { return Editor.ComboBoxStyle; }
	const FTableRowStyle&  GetEditorComboBoxRowStyle() const { return Editor.ComboBoxRowStyle; }
#endif
	
private:
	friend class FLazySingleton;

	FDefaultStyleCache();

	struct FStyles
	{
		FScrollBarStyle ScrollBarStyle;
		FComboBoxStyle ComboBoxStyle;
		FTableRowStyle ComboBoxRowStyle;
	};

	FStyles Runtime;

#if WITH_EDITOR
	FStyles Editor;
#endif
};