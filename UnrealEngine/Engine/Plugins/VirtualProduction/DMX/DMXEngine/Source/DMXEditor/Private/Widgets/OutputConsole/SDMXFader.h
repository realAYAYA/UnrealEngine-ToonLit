// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXProtocolCommon.h"
#include "DMXProtocolTypes.h"

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "UObject/NameTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"

struct FSpinBoxStyle;
class SBorder;
class SInlineEditableTextBlock;
template<typename NumericType> class SSpinBoxVertical;
class STextBlock;


/** Individual fader UI class */
class SDMXFader
	: public SCompoundWidget
{	
	DECLARE_DELEGATE_OneParam(FDMXFaderDelegate, TSharedRef<SDMXFader>);

public:
	SLATE_BEGIN_ARGS(SDMXFader)
		: _FaderName()
		, _Value(0)
		, _MaxValue(255)
		, _MinValue(0)
		, _UniverseID(1)
		, _StartingAddress(1)
		, _EndingAddress(1)
	{}
		/** The name displayed above the fader */
		SLATE_ARGUMENT(FText, FaderName)

		/** The value of the fader */
		SLATE_ARGUMENT(int32, Value)

		/** The max value of the fader */
		SLATE_ARGUMENT(int32, MaxValue)

		/** The min value of the fader */
		SLATE_ARGUMENT(int32, MinValue)

		/** The universe the fader sends DMX to */
		SLATE_ARGUMENT(int32, UniverseID)

		/** The starting channel Address to send DMX to */
		SLATE_ARGUMENT(int32, StartingAddress)

		/** The end channel Address to send DMX to */
		SLATE_ARGUMENT(int32, EndingAddress)

		/** Called when the fader gets selected */
		SLATE_EVENT(FDMXFaderDelegate, OnRequestDelete)

		/** Called when the fader got selected */
		SLATE_EVENT(FDMXFaderDelegate, OnRequestSelect)

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs);

	/** Flags the fader selected and highlights it */
	void Select();

	/** Flags the fader unselected clears the highlight */
	void Unselect();

	/** Returns wether the fader is flagged selected */
	bool IsSelected() const { return bSelected; }

private:
	/** Sanetizes DMX properties (Universe ID etc.) if they hold invalid values */
	void SanetizeDMXProperties();

	/** True if the fader is selected */
	bool bSelected = false;

protected:
	//~ Begin SWidget implementation
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	//~ End SWidget implementation

	/** Generates a widget to edit the adresses the fader sould send DMX to */
	TSharedRef<SWidget> GenerateAdressEditWidget();

public:
	/** Returns the name of the fader */
	FString GetFaderName() const { return FaderName; };

	/** Returns the universe ID to which to should send DMX to */
	int32 GetUniverseID() const { return UniverseID; }

	/** Returns the Starting Channel of where to send DMX to */
	int32 GetStartingAddress() const { return StartingAddress; }

	/** Returns the Ending Channel to which to send DMX to */
	int32 GetEndingAddress() const { return EndingAddress; }

	/** Gets the value of the fader */
	uint8 GetValue() const;

	/** Sets the value of the fader by a percentage value */
	void SetValueByPercentage(float InNewPercentage);

	/** Gets the min value of the fader */
	uint8 GetMinValue() const { return MinValue; }

	/** Gets the max value of the fader */
	uint8 GetMaxValue() const { return MaxValue; }

private:
	/** Cached Name of the Fader */
	FString FaderName;

	/** The universe the should send to fader */
	int32 UniverseID;

	/** The starting channel Address to send DMX to */
	int32 StartingAddress;

	/** The end channel Address to send DMX to */
	int32 EndingAddress;

	/** The current Fader Value */
	uint8 Value;

	/** The minimum Fader Value */
	uint8 MinValue;

	/** The maximum Fader Value */
	uint8 MaxValue;

private:
	/** Called when the delete button was clicked */
	FReply OnDeleteClicked();

	/** Handles when the user changes the Fader value */
	void HandleValueChanged(uint8 NewValue);

	/** Called when the FaderName border was Clicked */
	FReply OnFaderNameBorderClicked(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent);

	/** Called when the fader name changes */
	void OnFaderNameCommitted(const FText& NewFaderName, ETextCommit::Type InCommit);

	/** Called when the UniverseID border was Clicked, to give some more click-space to users */
	FReply OnUniverseIDBorderClicked(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent);

	/** Called when the Min Value border was Clicked, to give some more click-space to users */
	FReply OnMinValueBorderClicked(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent);

	/** Called when the Max Value border was Clicked, to give some more click-space to users */
	FReply OnMaxValueBorderClicked(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent);

	/** Called when the Starting Adress border was Clicked, to give some more click-space to users */
	FReply OnStartingAddressBorderClicked(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent);

	/** Called when the Ending Adress was Clicked, to give some more click-space to users */
	FReply OnEndingAddressBorderClicked(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent);

	/** Returns the UniverseID as text */
	FText GetUniverseIDAsText() const { return FText::FromString(FString::FromInt(UniverseID)); }

	/** Called when the UniverseID was commited */
	void OnUniverseIDCommitted(const FText& UniverseIDText, ETextCommit::Type InCommit);

	/** Returns the max value as text */
	FText GetStartingAddressAsText() const { return FText::FromString(FString::FromInt(StartingAddress)); }

	/** Called when the UniverseID was commited */
	void OnStartingAddressCommitted(const FText& StartingAddressText, ETextCommit::Type InCommit);

	/** Returns the max value as text */
	FText GetEndingAddressAsText() const { return FText::FromString(FString::FromInt(EndingAddress)); }

	/** Called when the UniverseID was commited */
	void OnEndingAddressCommitted(const FText& EndingAddressText, ETextCommit::Type InCommit);

	/** Returns the max value as text */
	FText GetMaxValueAsText() const { return FText::FromString(FString::FromInt(MaxValue)); }

	/** Called when the max value was commited */
	void OnMaxValueCommitted(const FText& MaxValueText, ETextCommit::Type InCommit);

	/** Returns the min value as text */
	FText GetMinValueAsText() const { return FText::FromString(FString::FromInt(MinValue)); }

	/** Called when the min value was commited */
	void OnMinValueCommitted(const FText& MinValueText, ETextCommit::Type InCommit);

	/**Change fader background color on hover */
	const FSlateBrush* GetBorderImage() const;

private:
	/** Background of the fader */
	TSharedPtr<SBorder> BackgroundBorder;

	/** SpinBox Style of the fader */
	FSpinBoxStyle OutputFaderStyle;

	/** Widget showing the freely definable name of the fader */
	TSharedPtr<SInlineEditableTextBlock> FaderNameTextBox;

	/** The actual editable fader */
	TSharedPtr<SSpinBoxVertical<uint8>> FaderSpinBox;

	/** Textblock to edit the Max Value of the Fader */
	TSharedPtr<SInlineEditableTextBlock> MaxValueEditableTextBlock;

	/** Textblock to edit the Min Value of the Fader */
	TSharedPtr<SInlineEditableTextBlock> MinValueEditableTextBlock;

	/** Textblock to edit the UniverseID */
	TSharedPtr<SInlineEditableTextBlock> UniverseIDEditableTextBlock;

	/** Textblock to edit the Starting Adress */
	TSharedPtr<SInlineEditableTextBlock> StartingAddressEditableTextBlock;

	/** Textblock to edit the Ending Adress */
	TSharedPtr<SInlineEditableTextBlock> EndingAddressEditableTextBlock;

	/** Called when the fader wants to be deleted */
	FDMXFaderDelegate OnRequestDelete;

	/** Called when the fader wants to be selected */
	FDMXFaderDelegate OnRequestSelect;
};
