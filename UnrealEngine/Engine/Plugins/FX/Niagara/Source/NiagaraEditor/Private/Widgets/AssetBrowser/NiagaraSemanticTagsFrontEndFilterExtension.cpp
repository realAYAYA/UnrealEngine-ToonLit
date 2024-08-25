// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/AssetBrowser/NiagaraSemanticTagsFrontEndFilterExtension.h"

#include "Misc/ConfigCacheIni.h"
#include "NiagaraEditorUtilities.h"

#define LOCTEXT_NAMESPACE "NiagaraSemanticUNiagaraSemanticTagsFrontEndFilterExtension"

bool FFrontendFilter_NiagaraTag::PassesFilter(const FContentBrowserItem& InItem) const
{
	return InItem.GetItemAttributes().Contains(FName(AssetTagDefinition.GetGuidAsString()));
}

FText FFrontendFilter_NiagaraTag_ContentBrowser::GetDisplayName() const
{
	FText BaseText = LOCTEXT("NiagaraAssetTagFilterDisplayName", "Niagara Asset Tags: {0}");
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	if(AssetRegistryModule.Get().IsLoadingAssets())
	{
		return FText::FormatOrdered(BaseText, LOCTEXT("WaitingForAssetRegistryLoad", "Waiting for Asset Registry"));
	}

	if(FilterData->GetNumActiveTagGuids() == 0)
	{
		return FText::FormatOrdered(BaseText, LOCTEXT("Right-click", "Right-click"));
	}
	else if(FilterData->GetNumActiveTagGuids() == 1)
	{
		FNiagaraAssetTagDefinitionReference Reference;
		Reference.SetTagDefinitionReferenceGuid(FilterData->GetActiveTagGuids()[0]);
		const FNiagaraAssetTagDefinition& AssetTagDefinition = FNiagaraEditorUtilities::AssetBrowser::FindTagDefinitionForReference(Reference);

		if(AssetTagDefinition.IsValid())
		{
			return FText::FormatOrdered(BaseText, AssetTagDefinition.AssetTag);
		}
		// In case an active tag was loaded from config that doesn't exist, we display 'Invalid'
		else
		{
			return FText::FormatOrdered(BaseText, LOCTEXT("Invalid", "Invalid"));
		}
	}
	else if(FilterData->GetNumActiveTagGuids() > 1)
	{
		return FText::FormatOrdered(BaseText, LOCTEXT("Complex", "Complex"));

	}
	
	return FText::GetEmpty();
}

void FFrontendFilter_NiagaraTag_ContentBrowser::ModifyContextMenu(FMenuBuilder& MenuBuilder)
{
	UNiagaraTagsContentBrowserFilterContext* ContextObject = NewObject<UNiagaraTagsContentBrowserFilterContext>();
	ContextObject->FilterData = FilterData;
	FToolMenuContext Context(ContextObject);

	MenuBuilder.AddWidget(UToolMenus::Get()->GenerateWidget("NiagaraEditorModule.ContentBrowserNiagaraTags", Context), FText::GetEmpty(), false);
}

bool FFrontendFilter_NiagaraTag_ContentBrowser::PassesFilter(const FContentBrowserItem& InItem) const
{
	for(const FGuid& Guid : FilterData->GetActiveTagGuids())
	{
		if(InItem.GetItemAttributes().Contains(FName(Guid.ToString(EGuidFormats::DigitsWithHyphens))) == false)
		{
			return false;
		}
	}

	return true;
}

void FFrontendFilter_NiagaraTag_ContentBrowser::SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const
{
	FFrontendFilter::SaveSettings(IniFilename, IniSection, SettingsString);

	TArray<FString> TagStrings;
	TagStrings.Reserve(FilterData->GetNumActiveTagGuids());

	for(const FGuid& ActiveTagGuid : FilterData->GetActiveTagGuids())
	{
		TagStrings.Add(ActiveTagGuid.ToString(EGuidFormats::DigitsWithHyphens));
	}

	GConfig->SetArray(*IniSection, *(SettingsString + TEXT(".NiagaraTags")), TagStrings, IniFilename);
}

void FFrontendFilter_NiagaraTag_ContentBrowser::LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString)
{
	FFrontendFilter::LoadSettings(IniFilename, IniSection, SettingsString);

	TArray<FString> TagStrings;
	GConfig->GetArray(*IniSection, *(SettingsString + TEXT(".NiagaraTags")), TagStrings, IniFilename);

	FilterData->ResetActiveTagGuids();

	for (const FString& TagString : TagStrings)
	{
		FGuid TagGuid = FGuid(TagString); 
		if(TagGuid.IsValid())
		{
			FilterData->AddTagGuid(TagGuid);
		}
	}
}

FString FFrontendFilter_NiagaraTag_ContentBrowser::GetReferencerName() const
{
	return "FrontendFilter_NiagaraTag_ContentBrowser";
}

void FFrontendFilter_NiagaraTag_ContentBrowser::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(FilterData);
}

bool FFrontendFilter_NiagaraEmitterInheritance::PassesFilter(const FContentBrowserItem& InItem) const
{
	FAssetData AssetData;
	InItem.Legacy_TryGetAssetData(AssetData);

	if(AssetData.GetClass() != UNiagaraEmitter::StaticClass())
	{
		return false;
	}
	
	bool bUseInheritance = false;
	if(FNiagaraEditorUtilities::GetIsInheritableFromAssetRegistryTags(AssetData, bUseInheritance))
	{
		return bRequiredInheritanceState == bUseInheritance;
	}

	return false;
}

bool FFrontendFilter_NiagaraSystemEffectType::PassesFilter(const FContentBrowserItem& InItem) const
{
	if(InItem.GetItemAttribute("EffectType").IsValid())
	{
		FName EffectType = InItem.GetItemAttribute("EffectType").GetValue<FName>();
		if(EffectType == NAME_None && bRequiresEffectType == false)
		{
			return true;
		}

		if(EffectType != NAME_None && bRequiresEffectType == true)
		{
			return true;
		}
	}
	else
	{
		if(bRequiresEffectType == false)
		{
			return true;
		}
	}

	return false;
}

void UNiagaraSemanticTagsFrontEndFilterExtension::AddFrontEndFilterExtensions(TSharedPtr<FFrontendFilterCategory> DefaultCategory, TArray<TSharedRef<FFrontendFilter>>& InOutFilterList) const
{
	using namespace FNiagaraEditorUtilities::AssetBrowser;
	
	TSharedRef<FFrontendFilterCategory> Category = MakeShared<FFrontendFilterCategory>(LOCTEXT("NiagaraSemanticFilterCategoryLabel", "Niagara Tags"),
		LOCTEXT("NiagaraSemanticFilterCategoryTooltip", "Filter Niagara Assets by Tag"));

	// by not sorting by name we ensure we keep the source & asset order
	// for(const FNiagaraAssetTagDefinition& AssetTagDefinition : GetFlatSortedAssetTagDefinitions(false))
	// {
	// 	TSharedRef<FFrontendFilter_NiagaraTag> NiagaraTagFilter = MakeShared<FFrontendFilter_NiagaraTag>(AssetTagDefinition, Category);
	// 	InOutFilterList.Add(NiagaraTagFilter);
	// }

	TSharedRef<FFrontendFilter_NiagaraTag_ContentBrowser> Filter = MakeShared<FFrontendFilter_NiagaraTag_ContentBrowser>(DefaultCategory);
	InOutFilterList.Add(Filter);
}

#undef LOCTEXT_NAMESPACE
