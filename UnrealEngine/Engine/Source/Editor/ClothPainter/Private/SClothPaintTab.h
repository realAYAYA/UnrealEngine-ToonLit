// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;
class IPersonaToolkit;
class ISkeletalMeshEditor;
class SClothAssetSelector;
class SClothPaintWidget;
class SScrollBox;
class UClothingAssetCommon;
struct FGeometry;

class CLOTHPAINTER_API SClothPaintTab : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SClothPaintTab)
	{}	
	SLATE_ARGUMENT(TWeakPtr<class FAssetEditorToolkit>, InHostingApp)
	SLATE_END_ARGS()

	SClothPaintTab();
	~SClothPaintTab();

	/** SWidget functions */
	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** Setup and teardown the cloth paint UI */
	void EnterPaintMode();
	void ExitPaintMode();

protected:

	/** Called from the selector when the asset selection changes (Asset, LOD, Mask) */
	void OnAssetSelectionChanged(TWeakObjectPtr<UClothingAssetCommon> InAssetPtr, int32 InLodIndex, int32 InMaskIndex);

	/** Whether or not the asset config section is enabled for editing */
	bool IsAssetDetailsPanelEnabled();

	/** Helpers for getting editor objects */
	ISkeletalMeshEditor* GetSkeletalMeshEditor() const;
	TSharedRef<IPersonaToolkit> GetPersonaToolkit() const;

	TWeakPtr<class FAssetEditorToolkit> HostingApp;
	
	TSharedPtr<SClothAssetSelector> SelectorWidget;
	TSharedPtr<SClothPaintWidget> ModeWidget;
	TSharedPtr<SScrollBox> ContentBox;
	TSharedPtr<IDetailsView> DetailsView;

	bool bModeApplied;
};
