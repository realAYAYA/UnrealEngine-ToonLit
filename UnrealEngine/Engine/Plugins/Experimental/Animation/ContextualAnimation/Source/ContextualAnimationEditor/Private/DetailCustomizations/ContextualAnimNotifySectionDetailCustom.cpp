// Copyright Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizations/ContextualAnimNotifySectionDetailCustom.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "PropertyEditorModule.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ContextualAnimMovieSceneNotifySection.h"

TSharedRef<IDetailCustomization> FContextualAnimNotifySectionDetailCustom::MakeInstance()
{
	return MakeShared<FContextualAnimNotifySectionDetailCustom>();
}

void FContextualAnimNotifySectionDetailCustom::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> ObjectList;
	DetailBuilder.GetObjectsBeingCustomized(ObjectList);
	check(ObjectList.Num() > 0);

	Section = Cast<UContextualAnimMovieSceneNotifySection>(ObjectList[0].Get());
	check(Section.IsValid());

	// Add new category to show the properties from the actual AnimNotify object
	if (const FAnimNotifyEvent* NotifyEvent = Section->GetAnimNotifyEvent())
	{
		if (UAnimNotifyState* NotifyState = NotifyEvent->NotifyStateClass)
		{
			IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(TEXT("Notify"), FText::GetEmpty(), ECategoryPriority::Variable);

			TArray<UObject*> ExternalObjects = { NotifyState };
			IDetailPropertyRow* PropertyRow = Category.AddExternalObjects(ExternalObjects);
			PropertyRow->ShouldAutoExpand(true);
		}
	}
}