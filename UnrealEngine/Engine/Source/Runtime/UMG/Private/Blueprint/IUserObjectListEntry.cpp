// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "Components/ListView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IUserObjectListEntry)

UUserObjectListEntry::UUserObjectListEntry(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
}

void IUserObjectListEntry::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	Execute_OnListItemObjectSet(Cast<UObject>(this), ListItemObject);
}

UObject* IUserObjectListEntry::GetListItemObjectInternal() const
{
	return UUserObjectListEntryLibrary::GetListItemObject(Cast<UUserWidget>(const_cast<IUserObjectListEntry*>(this)));
}

void IUserObjectListEntry::SetListItemObject(UUserWidget& ListEntryWidget, UObject* ListItemObject)
{
	if (IUserObjectListEntry* NativeImplementation = Cast<IUserObjectListEntry>(&ListEntryWidget))
	{
		NativeImplementation->NativeOnListItemObjectSet(ListItemObject);
	}
	else if (ListEntryWidget.Implements<UUserObjectListEntry>())
	{
		Execute_OnListItemObjectSet(&ListEntryWidget, ListItemObject);
	}
}

UObject* UUserObjectListEntryLibrary::GetListItemObject(TScriptInterface<IUserObjectListEntry> UserObjectListEntry)
{
	if (UUserWidget* EntryWidget = Cast<UUserWidget>(UserObjectListEntry.GetObject()))
	{
		const UListView* OwningListView = Cast<UListView>(UUserListEntryLibrary::GetOwningListView(EntryWidget));
		if (const TObjectPtrWrapTypeOf<UObject*>* ListItem = OwningListView ? OwningListView->ItemFromEntryWidget(*EntryWidget) : nullptr)
		{
			return *ListItem;
		}
	}
	return nullptr;
}

