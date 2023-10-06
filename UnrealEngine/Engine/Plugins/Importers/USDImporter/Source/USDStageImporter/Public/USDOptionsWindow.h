// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"

#include "UsdWrappers/ForwardDeclarations.h"

/**
 * Slate window used to show import/export options for the USDImporter plugin
 */
class USDSTAGEIMPORTER_API SUsdOptionsWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SUsdOptionsWindow ) : _OptionsObject( nullptr ) {}
		SLATE_ARGUMENT( UObject*, OptionsObject )
		SLATE_ARGUMENT( TSharedPtr<SWindow>, WidgetWindow )
		SLATE_ARGUMENT( FText, AcceptText )
		SLATE_ARGUMENT( const UE::FUsdStage*, Stage )
	SLATE_END_ARGS()

public:
	// Show the options window with the given text
	static bool ShowOptions(
		UObject& OptionsObject,
		const FText& WindowTitle,
		const FText& AcceptText,
		const UE::FUsdStage* Stage = nullptr
	);

	// Shortcut functions that show the standard import/export text
	static bool ShowImportOptions( UObject& OptionsObject, const UE::FUsdStage* StageToImport );
	static bool ShowExportOptions( UObject& OptionsObject );

	void Construct( const FArguments& InArgs );
	virtual bool SupportsKeyboardFocus() const override;

	FReply OnAccept();
	FReply OnCancel();

	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	bool UserAccepted() const;
	TArray<FString> GetSelectedFullPrimPaths() const;

private:
	UObject* OptionsObject;

	TWeakPtr< SWindow > Window;
	TSharedPtr< class SUsdStagePreviewTree > StagePreviewTree;

	FText AcceptText;
	bool bAccepted;
};
