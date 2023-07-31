// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Presentation/PropertyEditor/PropertyEditor.h"
#include "UserInterface/PropertyTable/SColumnHeader.h"
#include "IPropertyTableCell.h"
#include "UserInterface/PropertyTable/TextPropertyTableCellPresenter.h"
#include "UserInterface/PropertyTable/SPropertyTableCell.h"
#include "UserInterface/PropertyTable/BooleanPropertyTableCellPresenter.h"
#include "UserInterface/PropertyTable/ColorPropertyTableCellPresenter.h"
#include "IPropertyTableRow.h"

class SObjectColumnHeader : public SColumnHeader
{
	public:

	SLATE_BEGIN_ARGS( SObjectColumnHeader )
		: _Style( TEXT("PropertyTable") )
		, _Customization()
	{}
		SLATE_ARGUMENT( FName, Style )
		SLATE_ARGUMENT( TSharedPtr< IPropertyTableCustomColumn >, Customization )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, const TSharedRef< class IPropertyTableColumn >& InPropertyTableColumn, const TSharedRef< class IPropertyTableUtilities >& InPropertyUtilities )
	{
		Style = InArgs._Style;

		SColumnHeader::FArguments ColumnArgs;
		ColumnArgs.Style( Style );
		ColumnArgs.Customization( InArgs._Customization );

		SColumnHeader::Construct( ColumnArgs, InPropertyTableColumn, InPropertyUtilities );
	}

	virtual TSharedRef< SWidget > GenerateCell( const TSharedRef< class IPropertyTableRow >& PropertyTableRow ) override
	{
		TSharedRef< IPropertyTableCell > Cell = Column->GetCell( PropertyTableRow );

		TSharedPtr< IPropertyTableCellPresenter > CellPresenter( NULL );

		if ( Customization.IsValid() )
		{
			CellPresenter = Customization->CreateCellPresenter( Cell, Utilities.ToSharedRef(), Style );
		}

		if ( !CellPresenter.IsValid() && Cell->IsBound() )
		{
			TSharedRef< FPropertyEditor > PropertyEditor = FPropertyEditor::Create( Cell->GetNode().ToSharedRef(), Utilities.ToSharedRef() );

			TWeakFieldPtr< FProperty > Property = PropertyTableRow->GetDataSource()->AsPropertyPath()->GetLeafMostProperty().Property.Get();

			if( Property->IsA( FBoolProperty::StaticClass() ) )
			{
				CellPresenter = MakeShareable( new FBooleanPropertyTableCellPresenter( PropertyEditor ) );
			}
			else if ( CastField<const FStructProperty>(Property.Get()) && (CastField<const FStructProperty>(Property.Get())->Struct->GetFName()==NAME_Color || CastField<const FStructProperty>(Property.Get())->Struct->GetFName()==NAME_LinearColor) )
			{
				CellPresenter = MakeShareable( new FColorPropertyTableCellPresenter( PropertyEditor, Utilities.ToSharedRef() ) );
			}
			else
			{
				CellPresenter = MakeShareable( new FTextPropertyTableCellPresenter( PropertyEditor, Utilities.ToSharedRef() ) );
			}
		}

		return SNew( SPropertyTableCell, Cell )
			.Presenter( CellPresenter )
			.Style( Style );
	}


private:

	FName Style;
};
