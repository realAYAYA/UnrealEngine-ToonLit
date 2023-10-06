// Copyright Epic Games, Inc. All Rights Reserved.

#include "RevisionControlStyle/RevisionControlStyle.h"

#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "SlateGlobals.h"

#include "Framework/Application/SlateApplication.h"

TSharedPtr< class ISlateStyle > FRevisionControlStyleManager::DefaultRevisionControlStyleInstance = nullptr;
FName FRevisionControlStyleManager::CurrentRevisionControlStyleName;

FName FDefaultRevisionControlStyle::StyleName("DefaultRevisionControlStyle");

// FRevisionControlStyleManager

void FRevisionControlStyleManager::ReloadTextures()
{
	FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
}

const ISlateStyle& FRevisionControlStyleManager::Get()
{
	// If the default style instance doesn't exist (first call), create it and set it as active
	if (!DefaultRevisionControlStyleInstance.IsValid())
	{
		DefaultRevisionControlStyleInstance = MakeShared<FDefaultRevisionControlStyle>();
		SetActiveRevisionControlStyle(DefaultRevisionControlStyleInstance->GetStyleSetName());
	}

	// Find and get the currently active revision control style
	if (const ISlateStyle* RevisionControlStyle = FSlateStyleRegistry::FindSlateStyle(CurrentRevisionControlStyleName))
	{
		return *RevisionControlStyle;
	}

	return *DefaultRevisionControlStyleInstance;
}

FName FRevisionControlStyleManager::GetStyleSetName()
{
	return Get().GetStyleSetName();
}

void FRevisionControlStyleManager::SetActiveRevisionControlStyle(FName InNewActiveRevisionControlStyleName)
{
	// The style needs to be registered with the Slate Style Registry
	if (!FSlateStyleRegistry::FindSlateStyle(InNewActiveRevisionControlStyleName))
	{
		UE_LOG(LogSlate, Error, TEXT("Could not set the active revision control style, make sure the style you are setting exists and is registered with the FSlateStyleRegistry"));
	}
	
	CurrentRevisionControlStyleName = InNewActiveRevisionControlStyleName;
}

void FRevisionControlStyleManager::ResetToDefaultRevisionControlStyle()
{
	SetActiveRevisionControlStyle(DefaultRevisionControlStyleInstance->GetStyleSetName());
}

// FDefaultRevisionControlStyle

FDefaultRevisionControlStyle::FDefaultRevisionControlStyle() : FSlateStyleSet(StyleName)
{
	FSlateStyleRegistry::RegisterSlateStyle(*this);

	SetContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	// Any new revision control icons should be added here instead of in StarshipStyle. Any icons there that don't exist here should be moved here and used by calling FRevisionControlStyleManager::Get()
	
	// We use a custom teal color for the branched icons
	BranchedColor = FLinearColor::FromSRGBColor(FColor::FromHex("#00E4A0"));

	// Status icons
	Set("RevisionControl.Icon", new IMAGE_BRUSH_SVG("Starship/SourceControl/Status/RevisionControl", CoreStyleConstants::Icon16x16));
	Set("RevisionControl.Icon.ConnectedBadge", new IMAGE_BRUSH_SVG("Starship/SourceControl/Status/RevisionControlBadgeConnected", CoreStyleConstants::Icon16x16, FStyleColors::Success));
	Set("RevisionControl.Icon.WarningBadge", new IMAGE_BRUSH_SVG("Starship/SourceControl/Status/RevisionControlBadgeWarning", CoreStyleConstants::Icon16x16, FStyleColors::Warning));
	
	// Icons for revision control actions

	Set("RevisionControl.Actions.Sync", new IMAGE_BRUSH_SVG("Starship/SourceControl/RC_Sync", CoreStyleConstants::Icon16x16));
	Set("RevisionControl.Actions.CheckOut", new IMAGE_BRUSH_SVG("Starship/Common/check-circle", CoreStyleConstants::Icon16x16));
	Set("RevisionControl.Actions.SyncAndCheckOut", new IMAGE_BRUSH_SVG("Starship/SourceControl/RC_SyncAndCheckOut", CoreStyleConstants::Icon16x16));
	Set("RevisionControl.Actions.MakeWritable", new IMAGE_BRUSH_SVG("Starship/Common/edit", CoreStyleConstants::Icon16x16));
	Set("RevisionControl.Actions.Add", new IMAGE_BRUSH_SVG("Starship/SourceControl/RC_MarkedForAdd", CoreStyleConstants::Icon16x16));
	Set("RevisionControl.Actions.Submit", new IMAGE_BRUSH_SVG("Starship/SourceControl/RC_CheckIn", CoreStyleConstants::Icon16x16));
	Set("RevisionControl.Actions.History", new IMAGE_BRUSH_SVG("Starship/Common/Recent", CoreStyleConstants::Icon16x16));
	Set("RevisionControl.Actions.Diff", new IMAGE_BRUSH_SVG("Starship/SourceControl/RC_Diff", CoreStyleConstants::Icon16x16));
	Set("RevisionControl.Actions.Revert", new IMAGE_BRUSH_SVG("Starship/SourceControl/icon_SCC_Revert", CoreStyleConstants::Icon16x16));
	Set("RevisionControl.Actions.Merge", new IMAGE_BRUSH_SVG("Starship/Common/Merge", CoreStyleConstants::Icon16x16));
	Set("RevisionControl.Actions.Refresh", new IMAGE_BRUSH_SVG("Starship/Common/Update", CoreStyleConstants::Icon16x16));
	Set("RevisionControl.Actions.ChangeSettings", new IMAGE_BRUSH_SVG("Starship/SourceControl/icon_SCC_Change_Source_Control_Settings", CoreStyleConstants::Icon16x16));
	Set("RevisionControl.Locked", new IMAGE_BRUSH_SVG("Starship/Common/lock", CoreStyleConstants::Icon16x16));

	// Icons representing the various revision control states
	
	Set("RevisionControl.CheckedOut", new IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_CheckedOut", CoreStyleConstants::Icon16x16, FStyleColors::Error));
	
	Set("RevisionControl.OpenForAdd", new IMAGE_BRUSH_SVG("Starship/SourceControl/RC_MarkedForAdd", CoreStyleConstants::Icon16x16, FStyleColors::AccentGreen));
	Set("RevisionControl.MarkedForDelete", new IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_MarkedForDelete", CoreStyleConstants::Icon16x16, FStyleColors::Error));

	Set("RevisionControl.CheckedOutByOtherUser", new IMAGE_BRUSH_SVG("Starship/SourceControl/RC_CheckedOther", CoreStyleConstants::Icon16x16, FStyleColors::Error));
	Set("RevisionControl.CheckedOutByOtherUserBadge", new IMAGE_BRUSH_SVG("Starship/SourceControl/RC_CheckedOtherBadge", CoreStyleConstants::Icon16x16, FStyleColors::AccentBlue));

	Set("RevisionControl.CheckedOutByOtherUserOtherBranch", new IMAGE_BRUSH_SVG("Starship/SourceControl/RC_CheckedBranch", CoreStyleConstants::Icon16x16, BranchedColor));
	Set("RevisionControl.CheckedOutByOtherUserOtherBranchBadge", new IMAGE_BRUSH_SVG("Starship/SourceControl/RC_CheckedOtherBadge", CoreStyleConstants::Icon16x16, FStyleColors::Warning));

	Set("RevisionControl.ModifiedOtherBranch", new IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_ModifiedOtherBranch", CoreStyleConstants::Icon16x16, BranchedColor));
	Set("RevisionControl.ModifiedBadge", new IMAGE_BRUSH_SVG("Starship/SourceControl/RC_BranchModifiedBadge", CoreStyleConstants::Icon16x16, FStyleColors::Warning));
	Set("RevisionControl.ModifiedLocally", new IMAGE_BRUSH_SVG("Starship/SourceControl/RC_ModifiedLocally", CoreStyleConstants::Icon16x16, FStyleColors::Foreground));

	Set("RevisionControl.NotAtHeadRevision", new IMAGE_BRUSH_SVG("Starship/SourceControl/RC_NewerVersion", CoreStyleConstants::Icon16x16, FStyleColors::Warning));
	Set("RevisionControl.NotInDepot", new IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_NotInDepot", CoreStyleConstants::Icon16x16, FStyleColors::Warning));
	
	Set("RevisionControl.Branched", new IMAGE_BRUSH_SVG("Starship/SourceControl/SCC_Branched", CoreStyleConstants::Icon16x16, BranchedColor));

	Set("RevisionControl.Conflicted", new IMAGE_BRUSH_SVG("Starship/SourceControl/RC_Conflicted", CoreStyleConstants::Icon16x16, FStyleColors::Error));
	
	// Misc Icons
	Set("RevisionControl.ChangelistsTab", new IMAGE_BRUSH_SVG("Starship/Common/check-circle", CoreStyleConstants::Icon16x16));
	
	Set("RevisionControl.StatusBar.AtLatestRevision", new IMAGE_BRUSH_SVG("Starship/SourceControl/RC_StatusRemoteUpToDate", CoreStyleConstants::Icon16x16));
	Set("RevisionControl.StatusBar.NotAtLatestRevision", new IMAGE_BRUSH_SVG("Starship/SourceControl/RC_StatusRemoteDownload", CoreStyleConstants::Icon16x16, FStyleColors::Warning));
	Set("RevisionControl.StatusBar.NoLocalChanges", new IMAGE_BRUSH_SVG("Starship/SourceControl/RC_StatusLocalUpToDate", CoreStyleConstants::Icon16x16));
	Set("RevisionControl.StatusBar.HasLocalChanges", new IMAGE_BRUSH_SVG("Starship/SourceControl/RC_StatusLocalUpload", CoreStyleConstants::Icon16x16, FStyleColors::AccentBlue));
}

FDefaultRevisionControlStyle::~FDefaultRevisionControlStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}
	
const FName& FDefaultRevisionControlStyle::GetStyleSetName() const
{
	return StyleName;
}
