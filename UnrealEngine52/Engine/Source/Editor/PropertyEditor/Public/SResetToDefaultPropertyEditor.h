// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IDetailPropertyRow.h"
#include "Input/Reply.h"
#include "Internationalization/Text.h"
#include "Layout/Visibility.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

// Forward decl
class FResetToDefaultOverride;
struct FGeometry;

/** Widget showing the reset to default value button */
class PROPERTYEDITOR_API SResetToDefaultPropertyEditor : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SResetToDefaultPropertyEditor )
		: _NonVisibleState( EVisibility::Hidden )
		{}
		SLATE_ARGUMENT( EVisibility, NonVisibleState )
		SLATE_ARGUMENT( TOptional<FResetToDefaultOverride>, CustomResetToDefault )
	SLATE_END_ARGS()

	~SResetToDefaultPropertyEditor();

	void Construct( const FArguments& InArgs, const TSharedPtr< class IPropertyHandle>& InPropertyHandle );

private:
	FText GetResetToolTip() const;

	EVisibility GetDiffersFromDefaultAsVisibility() const;

	void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

	FReply OnResetClicked();

	void UpdateDiffersFromDefaultState();

private:
	TOptional<FResetToDefaultOverride> OptionalCustomResetToDefault;

	TSharedPtr< class IPropertyHandle > PropertyHandle;

	EVisibility NonVisibleState;

	bool bValueDiffersFromDefault;
};
