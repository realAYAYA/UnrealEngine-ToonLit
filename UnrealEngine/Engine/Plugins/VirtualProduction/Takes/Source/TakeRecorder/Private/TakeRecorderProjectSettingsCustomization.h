// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "TakeRecorderSettings.h"

class FTakeRecorderProjectSettingsCustomization : public IDetailCustomization
{
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override
	{
		// Pop the take recorder category to the top
		DetailLayout.EditCategory("Take Recorder");

		TArray<TWeakObjectPtr<UObject>> CustomizedObjects;
		DetailLayout.GetObjectsBeingCustomized(CustomizedObjects);

		for (TWeakObjectPtr<UObject> EditObject : CustomizedObjects)
		{
			UTakeRecorderProjectSettings* Settings = Cast<UTakeRecorderProjectSettings>(EditObject.Get());
			if (!Settings)
			{
				continue;
			}

			for (TWeakObjectPtr<UObject> WeakAdditionalSettings : Settings->AdditionalSettings)
			{
				UObject* AdditionalSettings = WeakAdditionalSettings.Get();
				if (!AdditionalSettings)
				{
					continue;
				}

				UClass* Class = AdditionalSettings->GetClass();

				TArray<FProperty*> EditProperties;
				for (FProperty* Property : TFieldRange<FProperty>(Class))
				{
					if (Property && Property->HasAllPropertyFlags(CPF_Edit | CPF_Config))
					{
						EditProperties.Add(Property);
					}
				}

				if (EditProperties.Num() > 0)
				{
					IDetailCategoryBuilder& Category = DetailLayout.EditCategory(*Class->GetDisplayNameText().ToString());

					TArray<UObject*> SettingAsArray = { AdditionalSettings };
					for (FProperty* Property : EditProperties)
					{
						Category.AddExternalObjectProperty(SettingAsArray, Property->GetFName());
					}
				}
			}
		}
	}
};