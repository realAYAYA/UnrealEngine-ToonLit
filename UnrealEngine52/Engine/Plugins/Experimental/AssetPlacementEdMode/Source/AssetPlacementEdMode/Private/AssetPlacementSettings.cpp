// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetPlacementSettings.h"

#include "Editor.h"
#include "PackageTools.h"
#include "PlacementPaletteAsset.h"
#include "PlacementPaletteItem.h"
#include "Factories/AssetFactoryInterface.h"
#include "Subsystems/PlacementSubsystem.h"
#include "Elements/Framework/TypedElementHandle.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Elements/Interfaces/TypedElementAssetDataInterface.h"
#include "Instances/InstancedPlacementClientInfo.h"

#include "AssetToolsModule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AssetPlacementSettings)

bool UAssetPlacementSettings::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	const FName PropertyName = InProperty->GetFName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UAssetPlacementSettings, bAllowNegativeScale))
	{
		return bUseRandomScale;
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UAssetPlacementSettings, bAllowNegativeRotationX))
	{
		return bUseRandomRotationX;
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UAssetPlacementSettings, bAllowNegativeRotationY))
	{
		return bUseRandomRotationY;
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UAssetPlacementSettings, bAllowNegativeRotationZ))
	{
		return bUseRandomRotationZ;
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UAssetPlacementSettings, bInvertNormalAxis))
	{
		return bAlignToNormal;
	}

	return true;
}

void UAssetPlacementSettings::SetPaletteAsset(UPlacementPaletteAsset* InPaletteAsset)
{
	ActivePalette = InPaletteAsset;
	LastActivePalettePath = FSoftObjectPath(InPaletteAsset);
}

UPlacementPaletteClient* UAssetPlacementSettings::AddClientToActivePalette(const FAssetData& InAssetData)
{
	UPlacementPaletteClient* NewPaletteItem = nullptr;
	if (!InAssetData.IsValid() || !GetActivePalette())
	{
		return NewPaletteItem;
	}

	if (InAssetData.GetClass()->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_NotPlaceable))
	{
		return NewPaletteItem;
	}

	if (!GetActivePaletteItems().FindByPredicate([InAssetData](const UPlacementPaletteClient* ItemIter) { return ItemIter ? (ItemIter->AssetPath == InAssetData.ToSoftObjectPath()) : false; }))
	{
		if (UPlacementSubsystem* PlacementSubystem = GEditor->GetEditorSubsystem<UPlacementSubsystem>())
		{
			if (TScriptInterface<IAssetFactoryInterface> AssetFactory = PlacementSubystem->FindAssetFactoryFromAssetData(InAssetData))
			{
				NewPaletteItem = NewObject<UPlacementPaletteClient>(GetMutableActivePalette());
				NewPaletteItem->AssetPath = InAssetData.ToSoftObjectPath();
				FPlacementOptions PlacementOptions;
				PlacementOptions.InstancedPlacementGridGuid = GetActivePaletteGuid();
				NewPaletteItem->SettingsObject = AssetFactory->FactorySettingsObjectForPlacement(InAssetData, PlacementOptions);
				GetMutableActivePalette()->Modify();
				if (NewPaletteItem->SettingsObject)
				{
					// Set the outer of the new object to the current palette
					NewPaletteItem->SettingsObject->Rename(nullptr, GetMutableActivePalette());
				}
				GetMutableActivePalette()->PaletteItems.Add(NewPaletteItem);
			}
		}
	}

	return NewPaletteItem;
}

int32 UAssetPlacementSettings::RemoveClientFromActivePalette(const FAssetData& InAssetData)
{
	return GetMutableActivePalette()->PaletteItems.RemoveAll([InAssetData](const UPlacementPaletteClient* ItemIter) { return ItemIter ? (ItemIter->AssetPath == InAssetData.ToSoftObjectPath()) : false; });
}

TArrayView<const TObjectPtr<UPlacementPaletteClient>> UAssetPlacementSettings::GetActivePaletteItems() const
{
	return MakeArrayView(GetActivePalette()->PaletteItems);
}

FSoftObjectPath UAssetPlacementSettings::GetActivePalettePath() const
{
	return FSoftObjectPath(ActivePalette);
}

const FGuid UAssetPlacementSettings::GetActivePaletteGuid() const
{
	return GetActivePalette()->GridGuid;
}

void UAssetPlacementSettings::ClearActivePaletteItems()
{
	GetMutableActivePalette()->Modify();
	GetMutableActivePalette()->PaletteItems.Empty();
}

void UAssetPlacementSettings::LoadSettings()
{
	LoadConfig();

	if (!UserPalette)
	{
		UserPalette = NewObject<UPlacementPaletteAsset>(this);
	}

	if (!UserGridGuid.IsValid())
	{
		FGuid::Parse(FPlatformMisc::GetLoginId(), UserGridGuid);
	}

	UserPalette->GridGuid = UserGridGuid;

	ActivePalette = Cast<UPlacementPaletteAsset>(LastActivePalettePath.TryLoad());
}

void UAssetPlacementSettings::SaveSettings()
{
	SaveConfig();
}

void UAssetPlacementSettings::SaveActivePalette()
{
	if (ActivePalette)
	{
		UPackageTools::SavePackagesForObjects(TArray<UObject*>({ ActivePalette }));
	}
	else if (UserPalette)
	{
		FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
		UObject* NewPaletteAsset = AssetToolsModule.Get().DuplicateAssetWithDialogAndTitle(FString(), FString(), UserPalette, NSLOCTEXT("AssetPlacementEdMode", "SavePaletteAsDialogTitle", "Save Asset Palette As..."));
		ActivePalette = CastChecked<UPlacementPaletteAsset>(NewPaletteAsset);
		LastActivePalettePath = FSoftObjectPath(ActivePalette);
	}
}

bool UAssetPlacementSettings::DoesActivePaletteSupportElement(const FTypedElementHandle& InElementToCheck) const
{	
	if (TTypedElement<ITypedElementAssetDataInterface> AssetDataInterface = UTypedElementRegistry::GetInstance()->GetElement<ITypedElementAssetDataInterface>(InElementToCheck))
	{
		TArray<FAssetData> ReferencedAssetDatas = AssetDataInterface.GetAllReferencedAssetDatas();
		for (const UPlacementPaletteClient* Item : GetActivePaletteItems())
		{
			if (!Item)
			{
				continue;
			}

			if (ReferencedAssetDatas.FindByPredicate([Item](const FAssetData& ReferencedAssetData) { return (ReferencedAssetData.ToSoftObjectPath() == Item->AssetPath); }))
			{
				return true;
			}

			// The current implementation of the asset data interface for actors requires that individual actors report on assets contained within their components.
			// Not all elements (legacy via actors) do this reliably, so additionally check the supplied factory for a match. 
			if (!Item->FactoryInterface.GetInterface())
			{
				continue;
			}

			FAssetData FoundAssetDataFromFactory = Item->FactoryInterface->GetAssetDataFromElementHandle(InElementToCheck);
			if (FoundAssetDataFromFactory.ToSoftObjectPath() == Item->AssetPath)
			{
				return true;
			}
		}
	}

	return false;
}

const UPlacementPaletteAsset* UAssetPlacementSettings::GetActivePalette() const
{
	if (ActivePalette)
	{
		return ActivePalette;
	}
	return UserPalette;
}

UPlacementPaletteAsset* UAssetPlacementSettings::GetMutableActivePalette()
{
	if (ActivePalette)
	{
		return ActivePalette;
	}
	return UserPalette;
}

