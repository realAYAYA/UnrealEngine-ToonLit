// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ContentBrowserFrontEndFilterExtension.h"
#include "NiagaraAssetTagDefinitions.h"
#include "NiagaraMenuFilters.h"
#include "NiagaraSemanticTagsFrontEndFilterExtension.generated.h"

#define LOCTEXT_NAMESPACE "NiagaraFrontendFilters"

/** A frontend filter used for Niagara specific windows. One filter per tag. */
class FFrontendFilter_NiagaraTag : public FFrontendFilter
{
public:
	FFrontendFilter_NiagaraTag(FNiagaraAssetTagDefinition InAssetTagDefinition, TSharedPtr<FFrontendFilterCategory> Category) : FFrontendFilter(Category), AssetTagDefinition(InAssetTagDefinition)
	{}

	virtual FLinearColor GetColor() const override { return AssetTagDefinition.Color; }
	virtual FString GetName() const override { return AssetTagDefinition.AssetTag.ToString(); }
	virtual FText GetDisplayName() const override { return FText::FromString(AssetTagDefinition.AssetTag.ToString()); }
	virtual FText GetToolTipText() const override { return AssetTagDefinition.Description; }
	
	virtual bool PassesFilter(const FContentBrowserItem& InItem) const override;
private:
	FNiagaraAssetTagDefinition AssetTagDefinition;
};

/** A special case filter for the content browser.
 *  It has a right click context menu that shows available asset tags and serializes them using only their guids */
class FFrontendFilter_NiagaraTag_ContentBrowser : public FFrontendFilter, public FGCObject
{
public:
	FFrontendFilter_NiagaraTag_ContentBrowser(TSharedPtr<FFrontendFilterCategory> Category) : FFrontendFilter(Category)
	{
		FilterData = NewObject<UNiagaraTagsContentBrowserFilterData>();
		FilterData->OnActiveTagGuidsChanged().AddLambda([this]()
		{
			BroadcastChangedEvent();
		});
	}
	
	virtual FString GetName() const override { return "Niagara Asset Tags"; }
	virtual FText GetDisplayName() const override;
	virtual FText GetToolTipText() const override { return LOCTEXT("NiagaraAssetTagFilterTooltip", "Right-click to specify the Niagara Tags to filter for.\nA combination of tags will filter for assets owning all tags."); }
	virtual void ModifyContextMenu(FMenuBuilder& MenuBuilder) override;
	
	virtual bool PassesFilter(const FContentBrowserItem& InItem) const override;

	virtual void SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const override;
	virtual void LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) override;

	virtual FString GetReferencerName() const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
private:
	TObjectPtr<UNiagaraTagsContentBrowserFilterData> FilterData;
};

class FFrontendFilter_NiagaraEmitterInheritance : public FFrontendFilter
{
public:
	FFrontendFilter_NiagaraEmitterInheritance(bool bInRequiredInheritanceState, TSharedPtr<FFrontendFilterCategory> Category) : FFrontendFilter(Category), bRequiredInheritanceState(bInRequiredInheritanceState)
	{}

	virtual FLinearColor GetColor() const override { return FLinearColor::Red; }
	virtual FString GetName() const override { return FString::Printf(TEXT("Inheritance: %s"), bRequiredInheritanceState ? TEXT("Yes") : TEXT("No")); }
	virtual FText GetDisplayName() const override { return FText::FormatOrdered(LOCTEXT("NiagarEmitterInheritanceFilterDisplayName", "Inheritance: {0}"), bRequiredInheritanceState ?
		LOCTEXT("NiagarEmitterInheritanceFilterDisplayName_Yes", "Yes") : LOCTEXT("NiagarEmitterInheritanceFilterDisplayName_No", "No")); }
	virtual FText GetToolTipText() const override
	{
		if(bRequiredInheritanceState)
		{
			return LOCTEXT("NiagaraEmitterInheritanceFilterTooltip_Yes", "Only display emitters with Inheritance");
		}

		return LOCTEXT("NiagaraEmitterInheritanceFilterTooltip_No", "Only display emitters without Inheritance");
	}
	
	virtual bool PassesFilter(const FContentBrowserItem& InItem) const override;
private:
	bool bRequiredInheritanceState;
};

class FFrontendFilter_NiagaraSystemEffectType : public FFrontendFilter
{
public:
	FFrontendFilter_NiagaraSystemEffectType(bool bInRequiresEffectType, TSharedPtr<FFrontendFilterCategory> Category) : FFrontendFilter(Category), bRequiresEffectType(bInRequiresEffectType)
	{}

	virtual FLinearColor GetColor() const override { return FLinearColor::Red; }
	virtual FString GetName() const override { return FString::Printf(TEXT("Effect Type: %s"), bRequiresEffectType ? TEXT("Yes") : TEXT("No")); }
	virtual FText GetDisplayName() const override { return FText::FormatOrdered(LOCTEXT("NiagaraSystemEffectTypeFilterDisplayName", "Effect Type: {0}"), bRequiresEffectType ?
		LOCTEXT("NiagaraSystemEffectTypeFilterDisplayName_Yes", "Yes") : LOCTEXT("NiagaraSystemEffectTypeFilterDisplayName_No", "No")); }
	virtual FText GetToolTipText() const override
	{
		if(bRequiresEffectType)
		{
			return LOCTEXT("NiagaraSystemEffectTypeFilterTooltip_Yes", "Only display Niagara Systems with an effect type");
		}

		return LOCTEXT("NiagaraSystemEffectTypeFilterTooltip_No", "Only display Niagara Systems without Inheritance");
	}
	
	virtual bool PassesFilter(const FContentBrowserItem& InItem) const override;
private:
	bool bRequiresEffectType;
};

/**
 * 
 */
UCLASS()
class NIAGARAEDITOR_API UNiagaraSemanticTagsFrontEndFilterExtension : public UContentBrowserFrontEndFilterExtension
{
	GENERATED_BODY()

	virtual void AddFrontEndFilterExtensions(TSharedPtr<class FFrontendFilterCategory> DefaultCategory, TArray< TSharedRef<class FFrontendFilter> >& InOutFilterList) const override;
};

#undef LOCTEXT_NAMESPACE
