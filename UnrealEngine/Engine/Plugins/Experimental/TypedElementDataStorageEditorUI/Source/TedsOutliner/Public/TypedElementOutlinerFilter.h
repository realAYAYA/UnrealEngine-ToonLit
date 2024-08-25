// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Filters/FilterBase.h"
#include "SceneOutlinerPublicTypes.h"
#include "Elements/Common/TypedElementQueryDescription.h"
#include "Elements/Framework/TypedElementMetaData.h"

class FTypedElementOutlinerMode;

// TEDS-Outliner TODO: Lots of minor missing functionality for icon, tooltip, color etc
class FTEDSOutlinerFilter : public FFilterBase<SceneOutliner::FilterBarType>
{
public:
	FTEDSOutlinerFilter(const FName& InFilterName, TSharedPtr<FFilterCategory> InCategory, FTypedElementOutlinerMode* InTEDSOutlinerMode, const TypedElementDataStorage::FQueryDescription& InFilterQuery);

	/** Returns the system name for this filter */
	virtual FString GetName() const override;

	/** Returns the human readable name for this filter */
	virtual FText GetDisplayName() const override;

	/** Returns the tooltip for this filter, shown in the filters menu */
	virtual FText GetToolTipText() const override;

	/** Returns the color this filter button will be when displayed as a button */
	virtual FLinearColor GetColor() const override;

	/** Returns the name of the icon to use in menu entries */
	virtual FName GetIconName() const override;

	/** If true, the filter will be active in the FilterBar when it is inactive in the UI (i.e the filter pill is grayed out)
	 * @See: FFrontendFilter_ShowOtherDevelopers in Content Browser
	 */
	virtual bool IsInverseFilter() const override;

	/** Notification that the filter became active or inactive */
	virtual void ActiveStateChanged(bool bActive) override;

	/** Called when the right-click context menu is being built for this filter */
	virtual void ModifyContextMenu(FMenuBuilder& MenuBuilder) override;
	
	/** Can be overriden for custom FilterBar subclasses to save settings, currently not implemented in any generic Filter Bar */
	virtual void SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const override;

	/** Can be overriden for custom FilterBar subclasses to load settings, currently not implemented in any generic Filter Bar */
	virtual void LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) override;
	
	/** Returns whether the specified Item passes the Filter's restrictions */
	virtual bool PassesFilter( SceneOutliner::FilterBarType InItem ) const override;

protected:

	FName FilterName;
	FTypedElementOutlinerMode* TEDSOutlinerMode;
	const TypedElementDataStorage::FQueryDescription FilterQuery;
};