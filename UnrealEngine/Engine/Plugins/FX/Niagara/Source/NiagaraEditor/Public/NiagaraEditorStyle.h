// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

/** Manages the style which provides resources for niagara editor widgets. */
class NIAGARAEDITOR_API FNiagaraEditorStyle : public FSlateStyleSet
{
public:
	static void Register();
	static void Unregister();
	static void Shutdown();

	/** reloads textures used by slate renderer */
	static void ReloadTextures();

	/** @return The Slate style set for niagara editor widgets */
	static const FNiagaraEditorStyle& Get();

	static void ReinitializeStyle();

private:	
	FNiagaraEditorStyle();
	void InitStats();
	void InitAssetPicker();
	void InitActionMenu();
	void InitEmitterHeader();
	void InitParameters();
	void InitParameterMapView();
	void InitCodeView();
	void InitSelectedEmitter();
	void InitToolbarIcons();
	void InitTabIcons();
	void InitIcons();
	void InitOverview();
	void InitViewportStyle();
	void InitEmitterDetails();
	void InitAssetColors();
	void InitThumbnails();
	void InitClassIcon();
	void InitStackIcons();
	void InitNiagaraSequence();
	void InitPlatformSet();
	void InitDropTarget();
	void InitScriptGraph();
	void InitDebuggerStyle();
	void InitBakerStyle();
	void InitCommonColors();
	void InitOutlinerStyle();
	void InitScalabilityColors();
	void InitScalabilityIcons();
	void InitScratchStyle();
	void InitHierarchyEditor();

	static TSharedPtr<FNiagaraEditorStyle> NiagaraEditorStyle;
};
