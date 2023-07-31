// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/AppStyle.h"
#include "Presentation/PropertyEditor/PropertyEditor.h"
#include "PropertyEditorHelpers.h"
#include "UserInterface/PropertyEditor/PropertyEditorConstants.h"
#include "Widgets/Text/STextBlock.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "SDropTarget.h"


#define LOCTEXT_NAMESPACE "PropertyEditor"

class SPropertyEditorArray : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SPropertyEditorArray )
		: _Font( FAppStyle::GetFontStyle( PropertyEditorConstants::PropertyFontStyle ) ) 
		{}
		SLATE_ATTRIBUTE( FSlateFontInfo, Font )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, const TSharedRef<FPropertyEditor>& InPropertyEditor )
	{
		PropertyEditor = InPropertyEditor;

		TAttribute<FText> TextAttr;
		if( PropertyEditorHelpers::IsStaticArray( *InPropertyEditor->GetPropertyNode() ) )
		{
			// Static arrays need special case handling for their values
			TextAttr.Set( GetArrayTextValue() );
		}
		else
		{
			TextAttr.Bind( this, &SPropertyEditorArray::GetArrayTextValue );
		}

		ChildSlot
		.Padding( 0.0f, 0.0f, 2.0f, 0.0f )
		[
			SNew(SDropTarget)
			.OnDropped(this, &SPropertyEditorArray::OnDragDropTarget)
			.OnAllowDrop(this, &SPropertyEditorArray::WillAddValidElements)
			.OnIsRecognized(this, &SPropertyEditorArray::IsValidAssetDropOp)
			.Content()
			[
				SNew(STextBlock)
				.Text(TextAttr)
				.Font(InArgs._Font)
			]
		];

		SetEnabled( TAttribute<bool>( this, &SPropertyEditorArray::CanEdit ) );
	}

	static bool Supports( const TSharedRef<FPropertyEditor>& InPropertyEditor )
	{
		const FProperty* NodeProperty = InPropertyEditor->GetProperty();

		return PropertyEditorHelpers::IsStaticArray( *InPropertyEditor->GetPropertyNode() ) 
			|| PropertyEditorHelpers::IsDynamicArray( *InPropertyEditor->GetPropertyNode() );
	}

	void GetDesiredWidth( float& OutMinDesiredWidth, float &OutMaxDesiredWidth )
	{
		OutMinDesiredWidth = 170.0f;
		OutMaxDesiredWidth = 170.0f;
	}
private:
	FText GetArrayTextValue() const
	{
		FString ArrayString;
		FPropertyAccess::Result GetValResult = PropertyEditor->GetPropertyHandle()->GetValueAsDisplayString(ArrayString);

		if (GetValResult == FPropertyAccess::MultipleValues)
		{
			return LOCTEXT("MultipleValues", "Multiple Values");
		}
		return FText::Format( LOCTEXT("NumArrayItemsFmt", "{0} Array elements"), FText::AsNumber(PropertyEditor->GetPropertyNode()->GetNumChildNodes()) );
	}

	/** @return True if the property can be edited */
	bool CanEdit() const
	{
		return PropertyEditor.IsValid() ? !PropertyEditor->IsEditConst() : true;
	}

	FReply OnDragDropTarget(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent)
	{
		FObjectProperty* ObjectProperty = nullptr;
		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(PropertyEditor->GetProperty()))
		{
			ObjectProperty = CastField<FObjectProperty>(ArrayProperty->Inner);
		}

		// Only try to add entries if we are dropping on an asset array
		TSharedPtr<FDragDropOperation> DragOperation = InDragDropEvent.GetOperation();
		if (ObjectProperty && DragOperation && DragOperation->IsOfType<FAssetDragDropOp>())
		{
			TSharedPtr<FAssetDragDropOp> DragDropOp = StaticCastSharedPtr<FAssetDragDropOp>(DragOperation);
			if (DragDropOp.IsValid())
			{
				for (FAssetData AssetData : DragDropOp->GetAssets())
				{
					// if the type matches
					if (AssetData.IsInstanceOf(ObjectProperty->PropertyClass))
					{
						PropertyEditor->AddGivenItem(AssetData.GetObjectPathString());
					}
				}
				// Let this bubble up to the rest of the row
				return FReply::Unhandled();

			}
		}
		return FReply::Unhandled();
	}

	bool IsValidAssetDropOp(TSharedPtr<FDragDropOperation> InOperation)
	{
		FObjectProperty* ObjectProperty = nullptr;
		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(PropertyEditor->GetProperty()))
		{
			ObjectProperty = CastField<FObjectProperty>(ArrayProperty->Inner);
		}

		// Only try to add entries if we are dropping on an asset array
		if (ObjectProperty && InOperation->IsOfType<FAssetDragDropOp>())
		{
			return true;
		}
		return false;
	}

	bool WillAddValidElements(TSharedPtr<FDragDropOperation> InOperation)
	{
		FObjectProperty* ObjectProperty = nullptr;

		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(PropertyEditor->GetProperty()))
		{
			ObjectProperty = CastField<FObjectProperty>(ArrayProperty->Inner);
		}

		// Only try to add entries if we are dropping on an asset array
		if (ObjectProperty && InOperation->IsOfType<FAssetDragDropOp>())
		{
			bool bHasOnlyValidElements = true;
			TSharedPtr<FAssetDragDropOp> DragDropOp = StaticCastSharedPtr<FAssetDragDropOp>(InOperation);
			if (DragDropOp.IsValid())
			{
				for (FAssetData AssetData : DragDropOp->GetAssets())
				{
					// if the type does not match
					if (!AssetData.IsInstanceOf(ObjectProperty->PropertyClass))
					{
						bHasOnlyValidElements = false;
						break;
					}
				}
			}
			return bHasOnlyValidElements;
		}
		
		return false;
	}

private:
	TSharedPtr<FPropertyEditor> PropertyEditor;
};

#undef LOCTEXT_NAMESPACE
