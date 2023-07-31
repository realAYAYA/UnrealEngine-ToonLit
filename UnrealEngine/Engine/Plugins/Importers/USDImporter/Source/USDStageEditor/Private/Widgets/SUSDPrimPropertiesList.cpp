// Copyright Epic Games, Inc. All Rights Reserved.

#include "SUSDPrimPropertiesList.h"

#include "SUSDStageEditorStyle.h"
#include "UnrealUSDWrapper.h"
#include "USDLog.h"
#include "USDMemory.h"
#include "USDStageActor.h"
#include "USDStageModule.h"
#include "USDTypesConversion.h"

#include "Styling/AppStyle.h"
#include "Engine/World.h"
#include "Modules/ModuleManager.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"

#if USE_USD_SDK

#define LOCTEXT_NAMESPACE "SUsdPrimPropertiesList"

namespace UsdPrimPropertiesListConstants
{
	const FMargin LeftRowPadding( 6.0f, 2.5f, 2.0f, 2.5f );
	const FMargin RightRowPadding( 3.0f, 2.5f, 2.0f, 2.5f );
	const FMargin ComboBoxItemPadding( 3.0f, 0.0f, 2.0f, 0.0f );
	const FMargin NumericEntryBoxItemPadding( 0.0f, 0.0f, 2.0f, 0.0f );
	const float DesiredNumericEntryBoxWidth = 80.0f;

	const TCHAR* NormalFont = TEXT("PropertyWindow.NormalFont");
}

class SUsdPrimPropertyRow;

namespace UsdPrimPropertiesListImpl
{
	enum class EPrimPropertyWidget : uint8
	{
		None,
		Bool,
		U8,
		I32,
		U32,
		I64,
		U64,
		F32,
		F64,
		Text,
		Dropdown,
	};

	enum class EPrimPropertyLabelTypes
	{
		NoLabel,
		RGBA,
		XYZW,
	};

	static TMap<FString, TArray<TSharedPtr<FString>>> TokenDropdownOptions;

	void ResetOptions( const FString& TokenName )
	{
		TokenDropdownOptions.Remove( TokenName );
	}

	TArray< TSharedPtr< FString > >* GetTokenDropdownOptions( const FUsdPrimAttributeViewModel& ViewModel )
	{
		if ( TArray< TSharedPtr< FString> >* FoundOptions = TokenDropdownOptions.Find( ViewModel.Label ) )
		{
			return FoundOptions;
		}

		TArray< TSharedPtr< FString > > Options = ViewModel.GetDropdownOptions();
		if ( Options.Num() == 0 )
		{
			// We don't know the options for this property, so return nullptr so that it can become a regular text input box
			return nullptr;
		}

		return &UsdPrimPropertiesListImpl::TokenDropdownOptions.Add( ViewModel.Label, Options );
	}

	EPrimPropertyLabelTypes GetLabelType( const UsdUtils::FConvertedVtValue& PropertyValue, const FString& ValueRole )
	{
		if ( PropertyValue.Entries.Num() < 1 )
		{
			return EPrimPropertyLabelTypes::NoLabel;
		}

		int32 NumComponents = PropertyValue.Entries[0].Num();
		EPrimPropertyLabelTypes LabelType = EPrimPropertyLabelTypes::NoLabel;
		if ( ValueRole.StartsWith( TEXT( "color" ), ESearchCase::IgnoreCase ) )
		{
			LabelType = EPrimPropertyLabelTypes::RGBA;
		}
		else if ( NumComponents > 1 && NumComponents <= 4 && PropertyValue.SourceType != UsdUtils::EUsdBasicDataTypes::Matrix2d )
		{
			LabelType = EPrimPropertyLabelTypes::XYZW;
		}

		return LabelType;
	}

	EPrimPropertyWidget GetWidgetType( UsdUtils::EUsdBasicDataTypes SourceType )
	{
		using namespace UsdUtils;

		switch ( SourceType )
		{
		case EUsdBasicDataTypes::Bool:
			return EPrimPropertyWidget::Bool;
			break;
		case EUsdBasicDataTypes::Uchar:
			return EPrimPropertyWidget::U8;
			break;
		case EUsdBasicDataTypes::Int:
		case EUsdBasicDataTypes::Int2:
		case EUsdBasicDataTypes::Int3:
		case EUsdBasicDataTypes::Int4:
			return EPrimPropertyWidget::I32;
			break;
		case EUsdBasicDataTypes::Uint:
			return EPrimPropertyWidget::U32;
			break;
		case EUsdBasicDataTypes::Int64:
			return EPrimPropertyWidget::I64;
			break;
		case EUsdBasicDataTypes::Uint64:
			return EPrimPropertyWidget::U64;
			break;
		case EUsdBasicDataTypes::Half:
		case EUsdBasicDataTypes::Half2:
		case EUsdBasicDataTypes::Half3:
		case EUsdBasicDataTypes::Half4:
		case EUsdBasicDataTypes::Quath:
		case EUsdBasicDataTypes::Float:
		case EUsdBasicDataTypes::Float2:
		case EUsdBasicDataTypes::Float3:
		case EUsdBasicDataTypes::Float4:
		case EUsdBasicDataTypes::Quatf:
			return EPrimPropertyWidget::F32;
			break;
		case EUsdBasicDataTypes::Double:
		case EUsdBasicDataTypes::Double2:
		case EUsdBasicDataTypes::Double3:
		case EUsdBasicDataTypes::Double4:
		case EUsdBasicDataTypes::Timecode:
		case EUsdBasicDataTypes::Matrix2d:
		case EUsdBasicDataTypes::Matrix3d:
		case EUsdBasicDataTypes::Matrix4d:
		case EUsdBasicDataTypes::Quatd:
			return EPrimPropertyWidget::F64;
			break;
		case EUsdBasicDataTypes::Token:
			return EPrimPropertyWidget::Dropdown;
			break;
		case EUsdBasicDataTypes::String:
		case EUsdBasicDataTypes::Asset:
			return EPrimPropertyWidget::Text;
			break;
		default:
			break;
		}

		return EPrimPropertyWidget::None;
	}

	/** Always returns `NumLabels` widgets, regardless of LabelTypes. Those may be the NullWidget though */
	TArray<TSharedRef<SWidget>> GetNumericEntryBoxLabels( int32 NumLabels, EPrimPropertyLabelTypes LabelType )
	{
		const static TArray<const FLinearColor*> Colors = {
			&SNumericEntryBox<int32>::RedLabelBackgroundColor,
			&SNumericEntryBox<int32>::GreenLabelBackgroundColor,
			&SNumericEntryBox<int32>::BlueLabelBackgroundColor,
			&FLinearColor::White
		};

		if ( NumLabels > 4 )
		{
			LabelType = EPrimPropertyLabelTypes::NoLabel;
		}

		TArray<TSharedRef<SWidget>> Labels;
		Labels.Reserve( NumLabels );

		switch ( LabelType )
		{
		case EPrimPropertyLabelTypes::RGBA:
			for ( int32 Index = 0; Index < NumLabels; ++Index )
			{
				Labels.Add( SNumericEntryBox<int32>::BuildNarrowColorLabel( *Colors[ Index ] ) );
			}
			break;
		case EPrimPropertyLabelTypes::XYZW:
			for ( int32 Index = 0; Index < NumLabels; ++Index )
			{
				Labels.Add( SNumericEntryBox<int32>::BuildNarrowColorLabel( *Colors[ Index ] ) );
			}
			break;
		case EPrimPropertyLabelTypes::NoLabel:
		default:
			for ( int32 Index = 0; Index < NumLabels; ++Index )
			{
				Labels.Add( SNullWidget::NullWidget );
			}
			break;
		}

		return Labels;
	}
}

class SUsdPrimPropertyRow : public SMultiColumnTableRow< TSharedPtr< FUsdPrimAttributeViewModel > >
{
	SLATE_BEGIN_ARGS( SUsdPrimPropertyRow ) {}
	SLATE_END_ARGS()

public:
	void Construct( const FArguments& InArgs, const TSharedPtr< FUsdPrimAttributeViewModel >& InUsdPrimProperty, const TSharedRef< STableViewBase >& OwnerTable );
	virtual TSharedRef< SWidget > GenerateWidgetForColumn( const FName& ColumnName ) override;

	void SetUsdPrimProperty( const TSharedPtr< FUsdPrimAttributeViewModel >& InUsdPrimProperty );

protected:
	FText GetLabel() const { return FText::FromString( UsdPrimAttribute->Label ); }

	// Optional here because SNumericEntryBox uses optional values as input
	template<typename T>
	TOptional<T> GetValue( int32 ComponentIndex ) const
	{
		if ( UsdPrimAttribute->Value.Entries.Num() == 1 ) // Ignore arrays for now
		{
			if ( UsdPrimAttribute->Value.Entries[0].IsValidIndex( ComponentIndex ) )
			{
				if ( T* Value = UsdPrimAttribute->Value.Entries[0][ ComponentIndex ].TryGet<T>() )
				{
					return *Value;
				}
			}
		}

		return {};
	}

	// Other overloads as checkbox/text widgets don't use optional values as input, and these should only ever have one component anyway
	FText GetValueText() const
	{
		if ( UsdPrimAttribute->Value.Entries.Num() == 1 )
		{
			if ( UsdPrimAttribute->Value.Entries[ 0 ].Num() > 0 )
			{
				if ( FString* Value = UsdPrimAttribute->Value.Entries[ 0 ][ 0 ].TryGet<FString>() )
				{
					return FText::FromString( *Value );
				}
			}
		}

		return FText::GetEmpty();
	}

	ECheckBoxState GetValueBool() const
	{
		if ( UsdPrimAttribute->Value.Entries.Num() == 1 )
		{
			if ( UsdPrimAttribute->Value.Entries[ 0 ].Num() > 0 )
			{
				if ( bool* Value = UsdPrimAttribute->Value.Entries[ 0 ][ 0 ].TryGet<bool>() )
				{
					return *Value ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				}
			}
		}

		return ECheckBoxState::Undetermined;
	}

private:
	template<typename T>
	TArray<TAttribute<TOptional<T>>> GetAttributeArray();

	TSharedRef< SWidget > GenerateTextWidget( const TAttribute<FText>& Attribute );
	TSharedRef< SWidget > GenerateEditableTextWidget( const TAttribute<FText>& Attribute, bool bIsReadOnly );
	TSharedRef< SWidget > GenerateCheckboxWidget( const TAttribute<ECheckBoxState>& Attribute );

	template<typename T>
	TSharedRef< SWidget > GenerateSpinboxWidgets(
		const TArray<TAttribute<TOptional<T>>>& Attribute,
		UsdPrimPropertiesListImpl::EPrimPropertyLabelTypes LabelType = UsdPrimPropertiesListImpl::EPrimPropertyLabelTypes::NoLabel
	);

private:
	template<typename T>
	void OnSpinboxValueChanged( T NewValue, int32 ComponentIndex );

	template<typename T>
	void OnSpinboxValueCommitted( T InNewValue, ETextCommit::Type CommitType, int32 ComponentIndex );

	void OnComboBoxSelectionChanged( TSharedPtr<FString> ChosenOption, ESelectInfo::Type SelectInfo );
	void OnTextBoxTextCommitted(const FText& NewText, ETextCommit::Type CommitType);
	void OnCheckBoxCheckStateChanged( ECheckBoxState NewState );

private:
	TSharedPtr< FUsdPrimAttributeViewModel > UsdPrimAttribute;
};

void SUsdPrimPropertyRow::Construct( const FArguments& InArgs, const TSharedPtr< FUsdPrimAttributeViewModel >& InUsdPrimProperty, const TSharedRef< STableViewBase >& OwnerTable )
{
	SetUsdPrimProperty( InUsdPrimProperty );

	SMultiColumnTableRow< TSharedPtr< FUsdPrimAttributeViewModel > >::Construct( SMultiColumnTableRow< TSharedPtr< FUsdPrimAttributeViewModel > >::FArguments(), OwnerTable );
}

TSharedRef< SWidget > SUsdPrimPropertyRow::GenerateWidgetForColumn( const FName& ColumnName )
{
	using namespace UsdPrimPropertiesListImpl;

	TSharedRef< SWidget > ColumnWidget = SNullWidget::NullWidget;

	if ( ColumnName == TEXT("PropertyName") )
	{
		ColumnWidget = GenerateTextWidget( { this, &SUsdPrimPropertyRow::GetLabel } );
	}
	else
	{
		EPrimPropertyWidget WidgetType = GetWidgetType( UsdPrimAttribute->Value.SourceType );
		EPrimPropertyLabelTypes LabelType = GetLabelType( UsdPrimAttribute->Value, UsdPrimAttribute->ValueRole );

		switch ( WidgetType )
		{
		case EPrimPropertyWidget::Bool:
			ColumnWidget = GenerateCheckboxWidget( { this, &SUsdPrimPropertyRow::GetValueBool } );
			break;
		case EPrimPropertyWidget::U8:
			ColumnWidget = GenerateSpinboxWidgets( GetAttributeArray<uint8>(), LabelType );
			break;
		case EPrimPropertyWidget::I32:
			ColumnWidget = GenerateSpinboxWidgets( GetAttributeArray<int32>(), LabelType );
			break;
		case EPrimPropertyWidget::U32:
			ColumnWidget = GenerateSpinboxWidgets( GetAttributeArray<uint32>(), LabelType );
			break;
		case EPrimPropertyWidget::I64:
			ColumnWidget = GenerateSpinboxWidgets( GetAttributeArray<int64>(), LabelType );
			break;
		case EPrimPropertyWidget::U64:
			ColumnWidget = GenerateSpinboxWidgets( GetAttributeArray<uint64>(), LabelType );
			break;
		case EPrimPropertyWidget::F32:
			ColumnWidget = GenerateSpinboxWidgets( GetAttributeArray<float>(), LabelType );
			break;
		case EPrimPropertyWidget::F64:
			ColumnWidget = GenerateSpinboxWidgets( GetAttributeArray<double>(), LabelType );
			break;
		case EPrimPropertyWidget::Dropdown:
		{
			TArray< TSharedPtr< FString > >* Options = GetTokenDropdownOptions( *UsdPrimAttribute );

			// Show a dropdown if we know the available options for that token
			if ( Options )
			{
				SAssignNew( ColumnWidget, SBox )
				.HeightOverride( FUsdStageEditorStyle::Get()->GetFloat( "UsdStageEditor.ListItemHeight" ) )
				.VAlign( VAlign_Center )
				[
					SNew( SComboBox< TSharedPtr< FString > > )
					.OptionsSource( Options )
					.OnGenerateWidget_Lambda( [ & ]( TSharedPtr<FString> Option )
					{
						return SUsdPrimPropertyRow::GenerateTextWidget( FText::FromString( *Option ) );
					})
					.OnSelectionChanged( this, &SUsdPrimPropertyRow::OnComboBoxSelectionChanged )
					[
						// Having an editable text box inside the combobox allows the user to pick through the most common ones but to
						// also specify a custom kind/purpose/etc. if they want to
						SNew( SEditableTextBox )
						.Text( this, &SUsdPrimPropertyRow::GetValueText )
						.Font( FAppStyle::GetFontStyle( "PropertyWindow.NormalFont" ) )
						.Padding( FMargin( 3.0f ) )
						// Fixed foreground color or else it will flip when our row is selected, and we already have a background
						.ForegroundColor( FAppStyle::GetSlateColor( TEXT( "Colors.ForegroundHover" ) ) )
						.OnTextCommitted( this, &SUsdPrimPropertyRow::OnTextBoxTextCommitted )
					]
				];

				break;
			}

			// If we don't have any options we intentionally fall down into the 'Text' case
		}
		case EPrimPropertyWidget::Text:
		{
			ColumnWidget = SUsdPrimPropertyRow::GenerateEditableTextWidget( { this, &SUsdPrimPropertyRow::GetValueText }, UsdPrimAttribute->bReadOnly );
			break;
		}
		case EPrimPropertyWidget::None:
		default:
			ColumnWidget = SNullWidget::NullWidget;
			break;
		}
	}

	return SNew( SHorizontalBox )
		+SHorizontalBox::Slot()
		.HAlign( HAlign_Left )
		.VAlign( VAlign_Fill )
		.AutoWidth()
		[
			SNew(SBox)
			.MinDesiredWidth( UsdPrimPropertiesListConstants::DesiredNumericEntryBoxWidth )
			.VAlign(VAlign_Center)
			[
				ColumnWidget
			]
		];
}

void SUsdPrimPropertyRow::SetUsdPrimProperty( const TSharedPtr< FUsdPrimAttributeViewModel >& InUsdPrimProperty )
{
	UsdPrimAttribute = InUsdPrimProperty;
}

template<typename T>
TArray<TAttribute<TOptional<T>>> SUsdPrimPropertyRow::GetAttributeArray()
{
	using AttrType = TAttribute<TOptional<T>>;

	TArray<AttrType> Attributes;
	if ( UsdPrimAttribute->Value.Entries.Num() == 1 )
	{
		for ( int32 ComponentIndex = 0; ComponentIndex < UsdPrimAttribute->Value.Entries[0].Num(); ++ComponentIndex )
		{
			Attributes.Add( AttrType::Create( AttrType::FGetter::CreateSP( this, &SUsdPrimPropertyRow::template GetValue<T>, ComponentIndex ) ) );
		}
	}

	return Attributes;
}

TSharedRef< SWidget > SUsdPrimPropertyRow::GenerateTextWidget( const TAttribute<FText>& Attribute )
{
	return SNew( SBox )
		.HeightOverride( FUsdStageEditorStyle::Get()->GetFloat( "UsdStageEditor.ListItemHeight" ) )
		.MinDesiredWidth( UsdPrimPropertiesListConstants::DesiredNumericEntryBoxWidth )
		.VAlign( VAlign_Center )
		[
			SNew( STextBlock )
			.Text( Attribute )
			.Font( FAppStyle::GetFontStyle( "PropertyWindow.NormalFont" ) )
			.Margin( UsdPrimPropertiesListConstants::RightRowPadding )
		];
}

TSharedRef< SWidget > SUsdPrimPropertyRow::GenerateEditableTextWidget( const TAttribute<FText>& Attribute, bool bIsReadOnly )
{
	return SNew( SBox )
		.HeightOverride( FUsdStageEditorStyle::Get()->GetFloat( "UsdStageEditor.ListItemHeight" ) )
		.MinDesiredWidth( UsdPrimPropertiesListConstants::DesiredNumericEntryBoxWidth )
		.VAlign( VAlign_Center )
		[
			SNew( SEditableTextBox )
			.Text( Attribute )
			.IsReadOnly( bIsReadOnly )
			.Font( FAppStyle::GetFontStyle( "PropertyWindow.NormalFont" ) )
			.OnTextCommitted( this, &SUsdPrimPropertyRow::OnTextBoxTextCommitted )
			// Fixed foreground color or else it will flip when our row is selected, and we already have a background
			.ForegroundColor( FAppStyle::GetSlateColor( bIsReadOnly ? TEXT( "Colors.Foreground" ) : TEXT( "Colors.ForegroundHover" ) ) )
		];
}

TSharedRef< SWidget > SUsdPrimPropertyRow::GenerateCheckboxWidget( const TAttribute<ECheckBoxState>& Attribute )
{
	return SNew( SBox )
		.HeightOverride( FUsdStageEditorStyle::Get()->GetFloat( "UsdStageEditor.ListItemHeight" ) )
		.VAlign( VAlign_Center )
		[
			SNew( SCheckBox )
			.IsChecked( Attribute )
			.OnCheckStateChanged( this, &SUsdPrimPropertyRow::OnCheckBoxCheckStateChanged )
		];
}

template<typename T>
TSharedRef< SWidget > SUsdPrimPropertyRow::GenerateSpinboxWidgets( const TArray<TAttribute<TOptional<T>>>& Attributes, UsdPrimPropertiesListImpl::EPrimPropertyLabelTypes LabelType )
{
	if ( Attributes.Num() == 0 )
	{
		return SNullWidget::NullWidget;
	}

	TArray<TSharedRef<SWidget>> Labels = UsdPrimPropertiesListImpl::GetNumericEntryBoxLabels( Attributes.Num(), LabelType );

	// If we're some type of matrix attribute, we'll want to display multiple rows
	const int32 NumRows = LabelType != UsdPrimPropertiesListImpl::EPrimPropertyLabelTypes::NoLabel
		? 1 // If we have labels we're definitely not a matrix type
		: Attributes.Num() == 4
			? 2
			: Attributes.Num() == 9
				? 3
				: Attributes.Num() == 16
					? 4
					: 1;

	const int32 NumColumns = Attributes.Num() / NumRows;

	TSharedPtr<SVerticalBox> VertBox = SNew( SVerticalBox );
	for ( int32 RowIndex = 0; RowIndex < NumRows; ++RowIndex )
	{
		TSharedPtr<SHorizontalBox> HorizBox = SNew( SHorizontalBox );
		for ( int32 ColumnIndex = 0; ColumnIndex < NumColumns; ++ColumnIndex )
		{
			const int32 ComponentIndex = RowIndex * NumColumns + ColumnIndex;
			const TAttribute<TOptional<T>>& Attribute = Attributes[ ComponentIndex ];

			TSharedRef<SWidget> EntryBox = SNew( SNumericEntryBox<T> )
				.AllowSpin( true )
				.Font( FAppStyle::GetFontStyle( "PropertyWindow.NormalFont" ) )
				.ShiftMouseMovePixelPerDelta( 1 )
				.SupportDynamicSliderMaxValue( true )
				.SupportDynamicSliderMinValue( true )
				.OnValueChanged( this, &SUsdPrimPropertyRow::OnSpinboxValueChanged<T>, ComponentIndex )
				.OnValueCommitted( this, &SUsdPrimPropertyRow::OnSpinboxValueCommitted<T>, ComponentIndex )
				.Value( Attribute )
				.MinValue( TOptional<T>() )
				.MaxValue( TOptional<T>() )
				.MinSliderValue( TOptional<T>() )
				.MaxSliderValue( TOptional<T>() )
				.SliderExponent( T( 1 ) )
				.Delta( T( 0 ) )
				.LabelPadding( FMargin(3) )
				.LabelLocation( SNumericEntryBox<T>::ELabelLocation::Inside )
				.Label()
				[
					Labels[ ComponentIndex ]
				];

			HorizBox->AddSlot()
				.Padding( UsdPrimPropertiesListConstants::NumericEntryBoxItemPadding )
				[
					SNew( SBox )
					.MinDesiredWidth( UsdPrimPropertiesListConstants::DesiredNumericEntryBoxWidth )
					.MaxDesiredWidth( UsdPrimPropertiesListConstants::DesiredNumericEntryBoxWidth )
					[
						EntryBox
					]
				];
		}

		VertBox->AddSlot()
			[
				SNew( SBox )
				.HeightOverride( FUsdStageEditorStyle::Get()->GetFloat( "UsdStageEditor.ListItemHeight" ) )
				.VAlign( VAlign_Center )
				[
					HorizBox.ToSharedRef()
				]
			];
	}

	return VertBox.ToSharedRef();
}

template<typename T>
void SUsdPrimPropertyRow::OnSpinboxValueChanged( T NewValue, int32 ComponentIndex )
{
	UsdPrimAttribute->Value.Entries[ 0 ][ ComponentIndex ].Set<T>( NewValue );
}

template<typename T>
void SUsdPrimPropertyRow::OnSpinboxValueCommitted( T InNewValue, ETextCommit::Type CommitType, int32 ComponentIndex )
{
	if ( CommitType == ETextCommit::OnCleared || UsdPrimAttribute->bReadOnly )
	{
		return;
	}

	FScopedTransaction Transaction(
		FText::Format(
			LOCTEXT( "SetUsdAttributeTransaction", "Set attribute '{0}'" ),
			FText::FromString( UsdPrimAttribute->Label )
		)
	);

	UsdUtils::FConvertedVtValue NewValue = UsdPrimAttribute->Value;
	NewValue.Entries[ 0 ][ ComponentIndex ].Set<T>( InNewValue );

	UsdPrimAttribute->SetAttributeValue( NewValue );
}

void SUsdPrimPropertyRow::OnComboBoxSelectionChanged( TSharedPtr<FString> ChosenOption, ESelectInfo::Type SelectInfo )
{
	if ( UsdPrimAttribute->bReadOnly )
	{
		return;
	}

	FScopedTransaction Transaction(
		FText::Format(
			LOCTEXT( "SetUsdAttributeTransaction", "Set attribute '{0}'" ),
			FText::FromString( UsdPrimAttribute->Label )
		)
	);

	UsdUtils::FConvertedVtValue NewValue = UsdPrimAttribute->Value;
	NewValue.Entries[ 0 ][ 0 ] = UsdUtils::FConvertedVtValueComponent( TInPlaceType<FString>(), *ChosenOption );

	UsdPrimAttribute->SetAttributeValue( NewValue );
}

void SUsdPrimPropertyRow::OnTextBoxTextCommitted( const FText& NewText, ETextCommit::Type CommitType )
{
	if ( CommitType == ETextCommit::OnCleared || UsdPrimAttribute->bReadOnly )
	{
		return;
	}

	FScopedTransaction Transaction(
		FText::Format(
			LOCTEXT( "SetUsdAttributeTransaction", "Set attribute '{0}'" ),
			FText::FromString( UsdPrimAttribute->Label )
		)
	);

	UsdUtils::FConvertedVtValue NewValue = UsdPrimAttribute->Value;
	NewValue.Entries[ 0 ][ 0 ] = UsdUtils::FConvertedVtValueComponent( TInPlaceType<FString>(), NewText.ToString() );

	UsdPrimAttribute->SetAttributeValue( NewValue );
}

void SUsdPrimPropertyRow::OnCheckBoxCheckStateChanged( ECheckBoxState NewState )
{
	if ( NewState == ECheckBoxState::Undetermined || UsdPrimAttribute->bReadOnly )
	{
		return;
	}

	FScopedTransaction Transaction(
		FText::Format(
			LOCTEXT( "SetUsdAttributeTransaction", "Set attribute '{0}'" ),
			FText::FromString( UsdPrimAttribute->Label )
		)
	);

	UsdUtils::FConvertedVtValue NewValue = UsdPrimAttribute->Value;
	NewValue.Entries[ 0 ][ 0 ] = UsdUtils::FConvertedVtValueComponent( TInPlaceType<bool>(), NewState == ECheckBoxState::Checked );

	UsdPrimAttribute->SetAttributeValue( NewValue );
}

void SUsdPrimPropertiesList::Construct( const FArguments& InArgs )
{
	// Clear map as usd file may have additional Kinds now
	UsdPrimPropertiesListImpl::ResetOptions(TEXT("Kind"));

	SAssignNew( HeaderRowWidget, SHeaderRow )

	+SHeaderRow::Column( FName( TEXT("PropertyName") ) )
	.DefaultLabel( LOCTEXT( "PropertyName", "Property Name" ) )
	.FillWidth( 25.f )

	+SHeaderRow::Column( FName( TEXT("PropertyValue") ) )
	.DefaultLabel( LOCTEXT( "PropertyValue", "Value" ) )
	.FillWidth( 75.f );

	SListView::Construct
	(
		SListView::FArguments()
		.ListItemsSource( &ViewModel.PrimAttributes )
		.OnGenerateRow( this, &SUsdPrimPropertiesList::OnGenerateRow )
		.HeaderRow( HeaderRowWidget )
	);
}

TSharedRef< ITableRow > SUsdPrimPropertiesList::OnGenerateRow( TSharedPtr< FUsdPrimAttributeViewModel > InDisplayNode, const TSharedRef< STableViewBase >& OwnerTable )
{
	return SNew( SUsdPrimPropertyRow, InDisplayNode, OwnerTable );
}

void SUsdPrimPropertiesList::GeneratePropertiesList( const UE::FUsdStageWeak& UsdStage, const TCHAR* InPrimPath )
{
	const float TimeCode = 0.f;
	ViewModel.Refresh( UsdStage, InPrimPath, TimeCode );
}

void SUsdPrimPropertiesList::SetPrimPath( const UE::FUsdStageWeak& UsdStage, const TCHAR* InPrimPath )
{
	GeneratePropertiesList( UsdStage, InPrimPath );
	RequestListRefresh();
}

#undef LOCTEXT_NAMESPACE

#endif // #if USE_USD_SDK
