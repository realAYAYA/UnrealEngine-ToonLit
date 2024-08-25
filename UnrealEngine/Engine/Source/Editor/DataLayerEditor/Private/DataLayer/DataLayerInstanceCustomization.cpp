// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataLayerInstanceCustomization.h"

#include "Containers/Array.h"
#include "DetailLayoutBuilder.h"
#include "Misc/AssertionMacros.h"
#include "Templates/Casts.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"

#define LOCTEXT_NAMESPACE "FDataLayerInstanceDetails"

TSharedRef<IDetailCustomization> FDataLayerInstanceDetails::MakeInstance()
{
	return MakeShareable(new FDataLayerInstanceDetails);
}

void FDataLayerInstanceDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	
	bool bHasRuntime = false;
	for (const TWeakObjectPtr<UObject>& SelectedObject : ObjectsBeingCustomized)
	{
		UDataLayerInstance* DataLayerInstance = Cast<UDataLayerInstance>(SelectedObject.Get());
		if (DataLayerInstance && DataLayerInstance->IsRuntime() && !DataLayerInstance->IsClientOnly() && !DataLayerInstance->IsServerOnly())
		{
			bHasRuntime = true;
			break;
		}
	}
	if (!bHasRuntime)
	{
		DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UDataLayerInstance, InitialRuntimeState));
	}
}

#undef LOCTEXT_NAMESPACE
