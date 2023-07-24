// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Engine/PostProcessVolume.h"
#include "Framework/Text/SlateHyperlinkRun.h"
#include "Components/LightComponentBase.h"

class SOculusToolWidget;

enum class SupportFlags : int
{
	None = 0x00,
	SupportPC = 0x01,
	SupportMobile = 0x02,
	ExcludeForward = 0x04,
	ExcludeDeferred = 0x08
};

typedef struct _SimpleSettingAction
{
	FText buttonText;
	FReply(SOculusToolWidget::*ClickFunc)(bool);
} SimpleSettingAction;

typedef struct _SimpleSetting
{
	FName tag;
	FText description;
	EVisibility(SOculusToolWidget::*VisFunc)(FName) const;
	TArray<SimpleSettingAction> actions;
	int supportMask; // bitfield of SupportFlags
} SimpleSetting;

/** Widget allowing the user to create new gameplay tags */
class SOculusToolWidget : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SOculusToolWidget)
	{}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs);

protected:
	void RebuildLayout();
	void SuggestRestart();

	void AddSimpleSetting(TSharedRef<SVerticalBox> scroller, SimpleSetting* setting);
	TSharedRef<SHorizontalBox> CreateSimpleSetting(SimpleSetting* setting);
	TSharedRef<SVerticalBox> NewCategory(TSharedRef<SScrollBox> scroller, FText heading);

	void OnBrowserLinkClicked(const FSlateHyperlinkRun::FMetadata& Metadata);
	FReply OnRestartClicked();
	EVisibility RestartVisible() const;
	FReply IgnoreRecommendation(FName tag);
	FReply UnhideIgnoredRecommendations();
	bool UsingForwardShading() const;
	FString GetConfigPath() const;
	FReply Refresh();
	EVisibility CanUnhideIgnoredRecommendations() const;
	EVisibility IsVisible(FName tag) const;
	bool SettingIgnored(FName settingKey) const;

	void OnChangePlatform(TSharedPtr<FString> ItemSelected, ESelectInfo::Type SelectInfo);

	FReply ForwardShadingEnable(bool text);
	EVisibility ForwardShadingVisibility(FName tag) const;

	FReply MultiViewEnable(bool text);
	EVisibility MultiViewVisibility(FName tag) const;

	FReply StartInVREnable(bool text);
	EVisibility StartInVRVisibility(FName tag) const;

	FReply SupportDashEnable(bool text);
	EVisibility SupportDashVisibility(FName tag) const;

	FReply LensFlareDisable(bool text);
	EVisibility LensFlareVisibility(FName tag) const;

	FReply MobileMultiViewEnable(bool text);
	EVisibility MobileMultiViewVisibility(FName tag) const;

	FReply MobileMSAAEnable(bool text);
	EVisibility MobileMSAAVisibility(FName tag) const;

	FReply MobilePostProcessingDisable(bool text);
	EVisibility MobilePostProcessingVisibility(FName tag) const;

	FReply MobileVulkanEnable(bool text);
	EVisibility MobileVulkanVisibility(FName tag) const;

	FReply AndroidManifestGo(bool text);
	FReply AndroidManifestQuest(bool text);
	EVisibility AndroidManifestVisibility(FName tag) const;

	FReply AndroidPackagingFix(bool text);
	EVisibility AndroidPackagingVisibility(FName tag) const;

	FReply AndroidQuestArchFix(bool text);
	EVisibility AndroidQuestArchVisibility(FName tag) const;

	FReply AntiAliasingEnable(bool text);
	EVisibility AntiAliasingVisibility(FName tag) const;

	FReply AllowStaticLightingEnable(bool text);
	EVisibility AllowStaticLightingVisibility(FName tag) const;

	FReply MobileShaderStaticAndCSMShadowReceiversDisable(bool text);
	EVisibility MobileShaderStaticAndCSMShadowReceiversVisibility(FName tag) const;

	FReply MobileShaderAllowDistanceFieldShadowsDisable(bool text);
	EVisibility MobileShaderAllowDistanceFieldShadowsVisibility(FName tag) const;

	FReply MobileShaderAllowMovableDirectionalLightsDisable(bool text);
	EVisibility MobileShaderAllowMovableDirectionalLightsVisibility(FName tag) const;

	FReply SelectLight(FString lightName);
	FReply IgnoreLight(FString lightName);
	
	void OnShowButtonChanged( ECheckBoxState NewState );
	ECheckBoxState IsShowButtonChecked() const;

	APostProcessVolume* PostProcessVolume;
	UEnum* PlatformEnum;
	TArray<TSharedPtr<FString>> Platforms;
	TMap<FName, SimpleSetting> SimpleSettings;

	TMap<FString, TWeakObjectPtr<ULightComponentBase> > DynamicLights;

	TSharedPtr<SScrollBox> ScrollingContainer;

	bool pendingRestart;
};
