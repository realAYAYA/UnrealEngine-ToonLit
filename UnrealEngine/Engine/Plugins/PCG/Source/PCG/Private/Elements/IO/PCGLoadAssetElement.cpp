// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/IO/PCGLoadAssetElement.h"

#include "PCGModule.h"
#include "Helpers/PCGDynamicTrackingHelpers.h"

#include "AssetRegistry/AssetData.h"

UPCGLoadDataAssetSettings::UPCGLoadDataAssetSettings()
{
	Pins = Super::OutputPinProperties();
	bTagOutputsBasedOnOutputPins = true;
}

#if WITH_EDITOR
void UPCGLoadDataAssetSettings::GetStaticTrackedKeys(FPCGSelectionKeyToSettingsMap& OutKeysToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const
{
	if (Asset.IsNull())
	{
		return;
	}

	FPCGSelectionKey Key = FPCGSelectionKey::CreateFromPath(Asset.ToSoftObjectPath());

	OutKeysToSettings.FindOrAdd(Key).Emplace(this, /*bCulling=*/false);
}

void UPCGLoadDataAssetSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property && PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UPCGLoadDataAssetSettings, Asset))
	{
		UpdateFromData();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

FPCGElementPtr UPCGLoadDataAssetSettings::CreateElement() const
{
	return MakeShared<FPCGLoadDataAssetElement>();
}

FString UPCGLoadDataAssetSettings::GetAdditionalTitleInformation() const
{
	return AssetName.IsEmpty() ? Asset.ToSoftObjectPath().GetAssetName() : AssetName;
}

void UPCGLoadDataAssetSettings::SetFromAsset(const FAssetData& InAsset)
{
	Asset = nullptr;

	if (const UClass* AssetClass = InAsset.GetClass())
	{
		if (AssetClass->IsChildOf(UPCGDataAsset::StaticClass()))
		{
			Asset = TSoftObjectPtr<UPCGDataAsset>(InAsset.GetSoftObjectPath());
		}
	}

	UpdateFromData();
	// TODO : notify?
}

void UPCGLoadDataAssetSettings::UpdateFromData()
{
	// Populate pins based on data present, in order, in the data collection.
	if (UPCGDataAsset* AssetData = Asset.LoadSynchronous())
	{
		TArray<FPCGPinProperties> NewPins;
		const FPCGDataCollection& Data = AssetData->Data;

		for (const FPCGTaggedData& TaggedData : Data.TaggedData)
		{
			if (!TaggedData.Data)
			{
				continue;
			}

			FPCGPinProperties* MatchingPin = NewPins.FindByPredicate([&TaggedData](const FPCGPinProperties& PinProperty) { return PinProperty.Label == TaggedData.Pin; });

			if (!MatchingPin)
			{
				NewPins.Emplace_GetRef(TaggedData.Pin, TaggedData.Data->GetDataType());
			}
			else
			{
				MatchingPin->AllowedTypes |= TaggedData.Data->GetDataType();
			}
		}

		Pins = NewPins;
		bTagOutputsBasedOnOutputPins = false;

		// Update rest of cached data (name, tooltip, color, ...)
		AssetName = AssetData->Name;
#if WITH_EDITOR
		AssetDescription = AssetData->Description;
		AssetColor = AssetData->Color;
#endif
	}
	else
	{
		Pins = Super::OutputPinProperties();
		bTagOutputsBasedOnOutputPins = true;

		AssetName = FString();
#if WITH_EDITOR
		AssetDescription = FText::GetEmpty();
		AssetColor = FLinearColor::White;
#endif
	}
}

bool FPCGLoadDataAssetElement::PrepareDataInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGLoadDataAssetElement::PrepareData);

	check(InContext);
	FPCGLoadDataAssetContext* Context = static_cast<FPCGLoadDataAssetContext*>(InContext);

	const UPCGLoadDataAssetSettings* Settings = Context->GetInputSettings<UPCGLoadDataAssetSettings>();
	check(Settings);

	if (Settings->Asset.IsNull())
	{
		return true;
	}

	// Request load, return false if we need to wait, otherwise continue
	if (!Context->WasLoadRequested())
	{
		return !Context->RequestResourceLoad(Context, { Settings->Asset.ToSoftObjectPath() }, !Settings->bSynchronousLoad);
	}
	else
	{
		return true;
	}
}

bool FPCGLoadDataAssetElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGDataAssetElement::Execute);

	check(Context);
	const UPCGLoadDataAssetSettings* Settings = Context->GetInputSettings<UPCGLoadDataAssetSettings>();
	check(Settings);

#if WITH_EDITOR
	if (Context->IsValueOverriden(GET_MEMBER_NAME_CHECKED(UPCGLoadDataAssetSettings, Asset)))
	{
		FPCGDynamicTrackingHelper::AddSingleDynamicTrackingKey(Context, FPCGSelectionKey::CreateFromPath(Settings->Asset.ToSoftObjectPath()), /*bIsCulled=*/false);
	}
#endif

	// At this point, the data should already be loaded
	if (UPCGDataAsset* AssetData = Settings->Asset.LoadSynchronous())
	{
		Context->OutputData = AssetData->Data;

		if (Settings->bTagOutputsBasedOnOutputPins)
		{
			for (FPCGTaggedData& TaggedData : Context->OutputData.TaggedData)
			{
				if (TaggedData.Pin != NAME_None)
				{
					TaggedData.Tags.Add(TaggedData.Pin.ToString());
				}
			}
		}
	}
	else if (!Settings->Asset.IsNull() || Settings->bWarnIfNoAsset)
	{
		PCGE_LOG(Warning, GraphAndLog, FText::Format(NSLOCTEXT("PCGLoadDataAssetSettings", "UnableToLoadAsset", "Unable to load PCG asset '{0}'"), FText::FromString(Settings->Asset.ToString())));
	}

	return true;
}