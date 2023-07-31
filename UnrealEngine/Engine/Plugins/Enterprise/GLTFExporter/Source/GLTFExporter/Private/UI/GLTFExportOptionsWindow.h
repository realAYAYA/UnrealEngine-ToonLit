// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "Options/GLTFExportOptions.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"

class SButton;

class GLTFEXPORTER_API SGLTFExportOptionsWindow : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SGLTFExportOptionsWindow)
		: _ExportOptions(nullptr)
		, _BatchMode()
		{}

		SLATE_ARGUMENT( UGLTFExportOptions*, ExportOptions )
		SLATE_ARGUMENT( TSharedPtr<SWindow>, WidgetWindow )
		SLATE_ARGUMENT( FText, FullPath )
		SLATE_ARGUMENT( bool, BatchMode )
	SLATE_END_ARGS()

	SGLTFExportOptionsWindow();

	void Construct(const FArguments& InArgs);

	FReply OnReset() const;
	FReply OnExport();
	FReply OnExportAll();
	FReply OnCancel();

	/* Begin SCompoundWidget overrides */
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	/* End SCompoundWidget overrides */

	bool ShouldExport() const;
	bool ShouldExportAll() const;

	static void ShowDialog(UGLTFExportOptions* ExportOptions, const FString& FullPath, bool bBatchMode, bool& bOutOperationCanceled, bool& bOutExportAll);

private:

	UGLTFExportOptions* ExportOptions;
	TSharedPtr<class IDetailsView> DetailsView;
	TWeakPtr<SWindow> WidgetWindow;
	TSharedPtr<SButton> ExportButton;
	bool bShouldExport;
	bool bShouldExportAll;
};

#endif
