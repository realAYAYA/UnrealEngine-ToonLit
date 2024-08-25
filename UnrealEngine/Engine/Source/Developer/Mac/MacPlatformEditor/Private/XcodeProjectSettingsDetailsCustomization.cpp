// Copyright Epic Games, Inc. All Rights Reserved.

#include "XcodeProjectSettingsDetailsCustomization.h"
#include "XcodeProjectSettings.h"
#include "HAL/FileManager.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SButton.h"
#include "SourceControlHelpers.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "PropertyHandle.h"

#define LOCTEXT_NAMESPACE "XcodeProjectSettings"

static constexpr FStringView kDefaultMacResourcesFolder(TEXTVIEW("Build/Mac/Resources/"));
static constexpr FStringView kDefaultIOSGeneratedFolder(TEXTVIEW("Build/IOS/UBTGenerated/"));

TSharedRef<IDetailCustomization> FXcodeProjectSettingsDetailsCustomization::MakeInstance()
{
    return MakeShareable(new FXcodeProjectSettingsDetailsCustomization);
}

void FXcodeProjectSettingsDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
    TemplateMacPlist = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UXcodeProjectSettings, TemplateMacPlist));
    TemplateIOSPlist = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UXcodeProjectSettings, TemplateIOSPlist));
    PremadeMacEntitlements = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UXcodeProjectSettings, PremadeMacEntitlements));
    ShippingEntitlements = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UXcodeProjectSettings, ShippingSpecificMacEntitlements));
    
    IDetailCategoryBuilder& PlistCategory = DetailLayout.EditCategory(TEXT("Plist Files"));
    PlistCategory.AddCustomRow(LOCTEXT("InfoPlist", "Info.plist"), false)
    .WholeRowWidget
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
            .Padding(FMargin(0, 5, 0, 10))
            .AutoWidth()
        [
           SNew(SButton)
           .Text(LOCTEXT("RestoreInfoPlist", "Restore Info.plist to default"))
           .ToolTipText(LOCTEXT("RestoreInfoPlistTooltip", "Revert to use default templates copied from Engine"))
           .OnClicked(this, &FXcodeProjectSettingsDetailsCustomization::OnRestorePlistClicked)
         ]
    ];
    
    IDetailCategoryBuilder& ShipEntitlementCategory = DetailLayout.EditCategory(TEXT("Entitlements"));
    ShipEntitlementCategory.AddCustomRow(LOCTEXT("Entitlement", "Entitlement"), false)
    .WholeRowWidget
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
            .Padding(FMargin(0, 5, 0, 10))
            .AutoWidth()
        [
            SNew(SButton)
            .Text(LOCTEXT("RestoreEntitlements", "Restore entitlements to default"))
            .ToolTipText(LOCTEXT("RestoreEntitlementsTooltip", "Revert to use default entitlements copied from Engine"))
            .OnClicked(this, &FXcodeProjectSettingsDetailsCustomization::OnRestoreEntitlementClicked)
        ]
    ];
    
    DetailLayout.SortCategories([](const TMap<FName, IDetailCategoryBuilder*>& CategoryMap)
    {
        for (const TPair<FName, IDetailCategoryBuilder*>& Pair : CategoryMap)
        {
            int32 SortOrder = Pair.Value->GetSortOrder();
            const FName CategoryName = Pair.Key;

            if (CategoryName == "Xcode")
            {
                SortOrder = 0;
            }
            else if(CategoryName == "Plist Files")
            {
                SortOrder = 1;
            }
			else if(CategoryName == "Entitlements")
			{
				SortOrder = 2;
			}
			else if(CategoryName == "Code Signing")
			{
				SortOrder = 3;
			}
			else if(CategoryName == "Privacy Manifests")
			{
				SortOrder = 4;
			}
            else
            {
                // Unknown category, should explicitly set order
                ensureMsgf(false, TEXT("Unknown category %s in XcodeProjectSttings"), *CategoryName.ToString());
                SortOrder = 999;
            }

            Pair.Value->SetSortOrder(SortOrder);
        }

        return;
    });
}

FReply FXcodeProjectSettingsDetailsCustomization::OnRestorePlistClicked()
{
    const UXcodeProjectSettings* Settings = GetDefault<UXcodeProjectSettings>();
    
    // Copy the default plist template from Engine
    FText ErrorMessage;
    if (!SourceControlHelpers::CopyFileUnderSourceControl(FPaths::ProjectDir() + kDefaultMacResourcesFolder + TEXT("Info.Template.plist"),
                                                          FPaths::EngineDir() + kDefaultMacResourcesFolder + TEXT("Info.Template.plist"),
                                                          FText::FromString(TEXT("Info.Template.plist")),
                                                          /*out*/ ErrorMessage))
    {
        FNotificationInfo Info(ErrorMessage);
        Info.ExpireDuration = 3.0f;
        FSlateNotificationManager::Get().AddNotification(Info);
    }
    
    TemplateMacPlist->SetValueFromFormattedString(TEXT("(FilePath=\"/Game/") + (FString)kDefaultMacResourcesFolder + TEXT("Info.Template.plist\")"));
    
    // No need to copy iOS template, it uses generated plist
    
    TemplateIOSPlist->SetValueFromFormattedString(TEXT("(FilePath=\"/Game/") + (FString)kDefaultIOSGeneratedFolder + TEXT("Info.Template.plist\")"));

    return FReply::Handled();
}

FReply FXcodeProjectSettingsDetailsCustomization::OnRestoreEntitlementClicked()
{
    const UXcodeProjectSettings* Settings = GetDefault<UXcodeProjectSettings>();
    
    // Copy the default entitlememt from Engine
    FText ErrorMessage;
    if (!SourceControlHelpers::CopyFileUnderSourceControl(FPaths::ProjectDir() + kDefaultMacResourcesFolder + TEXT("Sandbox.Server.entitlements"),
                                                          FPaths::EngineDir() + kDefaultMacResourcesFolder + TEXT("Sandbox.Server.entitlements"),
                                                          FText::FromString(TEXT("Sandbox.Server.entitlements")),
                                                          /*out*/ ErrorMessage))
    {
        FNotificationInfo Info(ErrorMessage);
        Info.ExpireDuration = 3.0f;
        FSlateNotificationManager::Get().AddNotification(Info);
    }
    
    PremadeMacEntitlements->SetValueFromFormattedString(TEXT("(FilePath=\"/Game/") + (FString)kDefaultMacResourcesFolder + TEXT("Sandbox.Server.entitlements\")"));
    
    // Copy the default entitlememt from Engine
    if (!SourceControlHelpers::CopyFileUnderSourceControl(FPaths::ProjectDir() + kDefaultMacResourcesFolder + TEXT("Sandbox.NoNet.entitlements"),
                                                          FPaths::EngineDir() + kDefaultMacResourcesFolder + TEXT("Sandbox.NoNet.entitlements"),
                                                          FText::FromString(TEXT("Sandbox.NoNet.entitlements")),
                                                          /*out*/ ErrorMessage))
    {
        FNotificationInfo Info(ErrorMessage);
        Info.ExpireDuration = 3.0f;
        FSlateNotificationManager::Get().AddNotification(Info);
    }
    
    ShippingEntitlements->SetValueFromFormattedString(TEXT("(FilePath=\"/Game/") + (FString)kDefaultMacResourcesFolder + TEXT("Sandbox.NoNet.entitlements\")"));

    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
