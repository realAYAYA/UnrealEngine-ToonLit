// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/TypedElementTypeInfoWidget.h"

#include "MassActorSubsystem.h"
#include "SceneOutlinerHelpers.h"
#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "Elements/Columns/TypedElementTypeInfoColumns.h"
#include "Elements/Interfaces/Capabilities/TypedElementUiTextCapability.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"

void UTypedElementTypeInfoWidgetFactory::RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
	ITypedElementDataStorageUiInterface& DataStorageUi) const
{
	DataStorageUi.RegisterWidgetFactory<FTypedElementTypeInfoWidgetConstructor>(FName(TEXT("General.Cell")),
		TypedElementDataStorage::FColumn<FTypedElementClassTypeInfoColumn>());

}

TMap<FName, const FSlateBrush*> FTypedElementTypeInfoWidgetConstructor::CachedIconMap;

FTypedElementTypeInfoWidgetConstructor::FTypedElementTypeInfoWidgetConstructor()
	: Super(FTypedElementTypeInfoWidgetConstructor::StaticStruct())
	, bUseIcon(false)
{
}

FTypedElementTypeInfoWidgetConstructor::FTypedElementTypeInfoWidgetConstructor(const UScriptStruct* InTypeInfo)
	: Super(InTypeInfo)
	, bUseIcon(false)
{
	
}

TSharedPtr<SWidget> FTypedElementTypeInfoWidgetConstructor::CreateWidget(
	const TypedElementDataStorage::FMetaDataView& Arguments	)
{
	bUseIcon = false;
	
	// Check if the caller provided metadata to use an icon widget
	TypedElementDataStorage::FMetaDataEntryView MetaDataEntryView = Arguments.FindGeneric("TypedElementTypeInfoWidget_bUseIcon");
	if(MetaDataEntryView.IsSet())
	{
		check(MetaDataEntryView.IsType<bool>());

		bUseIcon = *MetaDataEntryView.TryGetExact<bool>();
	}

	if(bUseIcon)
	{
		return SNew(SImage)
					.DesiredSizeOverride(FVector2D(16.f, 16.f))
					.ColorAndOpacity(FSlateColor::UseForeground());
	}
	else
	{
		return SNew(SHorizontalBox);
	}
}

bool FTypedElementTypeInfoWidgetConstructor::FinalizeWidget(ITypedElementDataStorageInterface* DataStorage,
	ITypedElementDataStorageUiInterface* DataStorageUi, TypedElementRowHandle Row, const TSharedPtr<SWidget>& Widget)
{
	checkf(Widget, TEXT("Referenced widget is not valid. A constructed widget may not have been cleaned up. This can "
		"also happen if this processor is running in the same phase as the processors responsible for cleaning up old "
		"references."));

	TypedElementRowHandle TargetRow = DataStorage->GetColumn<FTypedElementRowReferenceColumn>(Row)->Row;

	if (const FTypedElementClassTypeInfoColumn* TypeInfoColumn = DataStorage->GetColumn<FTypedElementClassTypeInfoColumn>(TargetRow))
	{
		if(bUseIcon)
		{
			checkf(Widget->GetType() == SImage::StaticWidgetClass().GetWidgetType(),
				TEXT("Stored widget with FTypedElementLabelWidgetConstructor doesn't match type %s, but was a %s."),
				*(SImage::StaticWidgetClass().GetWidgetType().ToString()),
				*(Widget->GetTypeAsString()));

			SImage* WidgetInstance = static_cast<SImage*>(Widget.Get());
			
			WidgetInstance->SetImage(GetIconForRow(DataStorage, Row, TypeInfoColumn));
		}
		else
		{
			checkf(Widget->GetType() == SHorizontalBox::StaticWidgetClass().GetWidgetType(),
				TEXT("Stored widget with FTypedElementLabelWidgetConstructor doesn't match type %s, but was a %s."),
				*(SHorizontalBox::StaticWidgetClass().GetWidgetType().ToString()),
				*(Widget->GetTypeAsString()));
				
			SHorizontalBox* WidgetInstance = static_cast<SHorizontalBox*>(Widget.Get());

			TSharedPtr<SWidget> ActualWidget;

			// Check if we have a hyperlink for this object
			if (const FTypedElementUObjectColumn* ObjectColumn = DataStorage->GetColumn<FTypedElementUObjectColumn>(TargetRow))
			{
				ActualWidget = SceneOutliner::FSceneOutlinerHelpers::GetClassHyperlink(ObjectColumn->Object.Get());
			}

			// If not, we simply show a text block with the type
			if(!ActualWidget)
			{
				TSharedPtr<STextBlock> TextBlock = SNew(STextBlock)
								.ColorAndOpacity(FSlateColor::UseSubduedForeground())
								.Text(FText::FromString(TypeInfoColumn->TypeInfo.Get()->GetName()));

				TextBlock->AddMetadata(MakeShared<TTypedElementUiTextCapability<STextBlock>>(*TextBlock));
				ActualWidget = TextBlock;
			}

			WidgetInstance->AddSlot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(8, 0, 0, 0)
						[
							ActualWidget.ToSharedRef()
						];
		}
	}

	return true;
}

const FSlateBrush* FTypedElementTypeInfoWidgetConstructor::GetIconForRow(ITypedElementDataStorageInterface* DataStorage, TypedElementRowHandle Row, const FTypedElementClassTypeInfoColumn* TypeInfoColumn)
{
	/* The logic here is very similar to SActorTreeLabel::GetIcon in ActorTreeItem.cpp which allows the actor to specify
	 * an override for the icon, and has a fallback to the class icon if not.
	 */
	
	const UClass* Type = TypeInfoColumn->TypeInfo.Get();
	FName IconName;

	// Allow the actor the first chance to provide an icon override
	FMassActorFragment* ActorStore = DataStorage->GetColumn<FMassActorFragment>(Row);
	if(ActorStore)
	{
		if(const AActor* Actor = ActorStore->Get())
		{
			IconName = Actor->GetCustomIconName();
		}
	}

	if(IconName == NAME_None)
	{
		IconName = Type->GetFName();
	}

	// Check the cache if we already found an icon for this class
	if(const FSlateBrush** CachedBrush = CachedIconMap.Find(IconName))
	{
		if(*CachedBrush)
		{
			return *CachedBrush;
		}
	}

	if(const FSlateBrush* FoundSlateBrush = FSlateIconFinder::FindIconForClass(Type).GetOptionalIcon())
	{
		CachedIconMap.Add(IconName, FoundSlateBrush);
		return FoundSlateBrush;
	}

	// Fallback to the regular actor icon if we haven't found any specific icon
	return FSlateIconFinder::FindIconForClass(AActor::StaticClass()).GetOptionalIcon();;
}
