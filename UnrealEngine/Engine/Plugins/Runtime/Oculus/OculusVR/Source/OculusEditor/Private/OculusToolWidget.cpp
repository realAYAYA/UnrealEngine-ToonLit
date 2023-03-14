// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusToolWidget.h"
#include "OculusEditorSettings.h"
#include "OculusHMDRuntimeSettings.h"
#include "OculusHMD.h"
#include "DetailLayoutBuilder.h"
#include "Engine/RendererSettings.h"
#include "Engine/Blueprint.h"
#include "GeneralProjectSettings.h"
#include "AndroidRuntimeSettings.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "Styling/AppStyle.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "UObject/EnumProperty.h"
#include "EdGraph/EdGraph.h"
#include "Widgets/Input/SCheckBox.h"
#include "UnrealEdMisc.h"

#define CALL_MEMBER_FUNCTION(object, memberFn) ((object).*(memberFn))

#define LOCTEXT_NAMESPACE "OculusToolWidget"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// Misc notes and known issues:
// * I save after every change because UE wasn't prompting to save on exit, but this makes it tough for users to undo, and doesn't prompt shader rebuild. Alternatives?

TSharedRef<SHorizontalBox> SOculusToolWidget::CreateSimpleSetting(SimpleSetting* setting)
{
	auto box = SNew(SHorizontalBox).Visibility(this, &SOculusToolWidget::IsVisible, setting->tag)
		+ SHorizontalBox::Slot().FillWidth(10).VAlign(VAlign_Center)
		[
			SNew(SRichTextBlock)
			.Visibility(this, &SOculusToolWidget::IsVisible, setting->tag)
		.DecoratorStyleSet(&FAppStyle::Get())
		.Text(setting->description).AutoWrapText(true)
		+ SRichTextBlock::HyperlinkDecorator(TEXT("HyperlinkDecorator"), this, &SOculusToolWidget::OnBrowserLinkClicked)
		];

	for (int i = 0; i < setting->actions.Num(); ++i)
	{
		box.Get().AddSlot()
			.AutoWidth().VAlign(VAlign_Top)
			[
				SNew(SButton)
				.Text(setting->actions[i].buttonText)
			.OnClicked(this, setting->actions[i].ClickFunc, true)
			.Visibility(this, &SOculusToolWidget::IsVisible, setting->tag)
			];
	}

	box.Get().AddSlot().AutoWidth().VAlign(VAlign_Top)
		[
			SNew(SButton)
			.Text(LOCTEXT("IgnorePerfRec", "Ignore"))
		.OnClicked(this, &SOculusToolWidget::IgnoreRecommendation, setting->tag)
		.Visibility(this, &SOculusToolWidget::IsVisible, setting->tag)
		];
	return box;
}

EVisibility SOculusToolWidget::IsVisible(FName tag) const
{
	const SimpleSetting* setting = SimpleSettings.Find(tag);
	checkf(setting != NULL, TEXT("Failed to find tag %s."), *tag.ToString());
	if(SettingIgnored(setting->tag)) return EVisibility::Collapsed;
	UDEPRECATED_UOculusEditorSettings* EditorSettings = GetMutableDefault<UDEPRECATED_UOculusEditorSettings>();
	EOculusPlatform targetPlatform = EditorSettings->PerfToolTargetPlatform;
	 
	if(targetPlatform == EOculusPlatform::Mobile && !((int)setting->supportMask & (int)SupportFlags::SupportMobile)) return EVisibility::Collapsed;
	if(targetPlatform == EOculusPlatform::PC && !((int)setting->supportMask & (int)SupportFlags::SupportPC)) return EVisibility::Collapsed;

	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	const bool bForwardShading = UsingForwardShading();
	if (bForwardShading && ((int)setting->supportMask & (int)SupportFlags::ExcludeForward)) return EVisibility::Collapsed;
	if (!bForwardShading && ((int)setting->supportMask & (int)SupportFlags::ExcludeDeferred)) return EVisibility::Collapsed;

	return CALL_MEMBER_FUNCTION(*this, setting->VisFunc)(setting->tag);
}

void SOculusToolWidget::AddSimpleSetting(TSharedRef<SVerticalBox> box, SimpleSetting* setting)
{
		box.Get().AddSlot().AutoHeight()
		.Padding(5, 5)
		[
			CreateSimpleSetting(setting)
		];
}

bool SOculusToolWidget::SettingIgnored(FName settingKey) const
{
	UDEPRECATED_UOculusEditorSettings* EditorSettings = GetMutableDefault<UDEPRECATED_UOculusEditorSettings>();
	bool* ignoreSetting = EditorSettings->PerfToolIgnoreList.Find(settingKey);
	return (ignoreSetting != NULL && *ignoreSetting == true);
}

TSharedRef<SVerticalBox> SOculusToolWidget::NewCategory(TSharedRef<SScrollBox> scroller, FText heading)
{
	scroller.Get().AddSlot()
	.Padding(0, 0)
	[
		SNew(SBorder)
		.BorderImage( FAppStyle::GetBrush("ToolPanel.DarkGroupBorder") )
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot().Padding(5,5).FillWidth(1)
			[
				SNew(SRichTextBlock)
				.TextStyle(FAppStyle::Get(), "ToolBar.Heading")
				.DecoratorStyleSet(&FAppStyle::Get()).AutoWrapText(true)
				.Text(heading)
				+ SRichTextBlock::HyperlinkDecorator(TEXT("HyperlinkDecorator"), this, &SOculusToolWidget::OnBrowserLinkClicked)
			]
		]
	];

	TSharedPtr<SVerticalBox> box;
	scroller.Get().AddSlot()
	.Padding(0, 0, 0, 2)
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SAssignNew(box, SVerticalBox)
		]
	];
	return box.ToSharedRef();
}

void SOculusToolWidget::RebuildLayout()
{
	if (!ScrollingContainer.IsValid()) return;
	TSharedRef<SScrollBox> scroller = ScrollingContainer.ToSharedRef();

	UDEPRECATED_UOculusEditorSettings* EditorSettings = GetMutableDefault<UDEPRECATED_UOculusEditorSettings>();
	uint8 initiallySelected = 0;
	for (uint8 i = 0; i < (uint8)EOculusPlatform::Length; ++i)
	{
		if ((uint8)EditorSettings->PerfToolTargetPlatform == i)
		{
			initiallySelected = i;
		}
	}

	scroller.Get().ClearChildren();

	scroller.Get().AddSlot()
	.Padding(2, 2)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBorder)
			//.BorderImage( FAppStyle::GetBrush("ToolPanel.LightGroupBorder") ).Visibility(this, &SOculusToolWidget::RestartVisible)
			.BorderImage( FAppStyle::GetBrush("SceneOutliner.ChangedItemHighlight") ).Visibility(this, &SOculusToolWidget::RestartVisible)
			.Padding(2)
			[
				SNew(SBorder)
				.BorderImage( FAppStyle::GetBrush("ToolPanel.DarkGroupBorder") )
				.Padding(2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(10).VAlign(VAlign_Center)
					[
						SNew(SRichTextBlock)
						.Text(LOCTEXT("RestartRequired", "<RichTextBlock.TextHighlight>Restart required:You have made changes that require an editor restart to take effect.</>")).DecoratorStyleSet(&FAppStyle::Get())
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
					[
						SNew(SButton)
						.Text(LOCTEXT("RestartNow", "Restart Editor"))
						.OnClicked(this, &SOculusToolWidget::OnRestartClicked)
					]
				]
			]
		]
	];
	
	TSharedRef<SVerticalBox> box = NewCategory(scroller, LOCTEXT("GeneralSettings", "<RichTextBlock.Bold>General Settings</>"));

	box.Get().AddSlot()
	.Padding(5, 5)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(10).VAlign(VAlign_Top)
		[
			SNew(SRichTextBlock)
			.Text(LOCTEXT("TargetPlatform", "Target Platform: (This setting changes which recommendations are displayed, but does NOT modify your project.)"))
		]
		+SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Top)
		[
			SNew(STextComboBox)
			.OptionsSource( &Platforms )
			.InitiallySelectedItem(Platforms[initiallySelected])
			.OnSelectionChanged( this, &SOculusToolWidget::OnChangePlatform )
		]
	];
	/*
	// Omitting this option for now, because the tool is currently something you only need to launch once or twice.
	// If later tabs end up increasing use cases significantly we may re-add.
	box.Get().AddSlot()
	.Padding(5, 5)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(10).VAlign(VAlign_Top)
		[
			SNew(SRichTextBlock)
			.Text(LOCTEXT("ShowToolButtonInEditor", "Add Oculus Tool Button to editor (change appears after restart in Windows -> Developer Tools -> Oculus Tool):"))
		]
		+SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Top)
		[
			SNew(SCheckBox)
			.OnCheckStateChanged( this, &SOculusToolWidget::OnShowButtonChanged )
			.IsChecked( this, &SOculusToolWidget::IsShowButtonChecked )
		]
	];
		*/

	AddSimpleSetting(box, SimpleSettings.Find(FName("StartInVR")));
	AddSimpleSetting(box, SimpleSettings.Find(FName("SupportDash")));
	AddSimpleSetting(box, SimpleSettings.Find(FName("ForwardShading")));
	AddSimpleSetting(box, SimpleSettings.Find(FName("AllowStaticLighting")));
	AddSimpleSetting(box, SimpleSettings.Find(FName("MultiView")));
	AddSimpleSetting(box, SimpleSettings.Find(FName("MobileMultiView")));
	AddSimpleSetting(box, SimpleSettings.Find(FName("MobileMSAA")));
	AddSimpleSetting(box, SimpleSettings.Find(FName("MobilePostProcessing")));
	AddSimpleSetting(box, SimpleSettings.Find(FName("MobileVulkan")));
	AddSimpleSetting(box, SimpleSettings.Find(FName("AndroidManifest")));
	AddSimpleSetting(box, SimpleSettings.Find(FName("AndroidPackaging")));
	AddSimpleSetting(box, SimpleSettings.Find(FName("AndroidQuestArch")));

	box = NewCategory(scroller, LOCTEXT("PostProcessHeader", "<RichTextBlock.Bold>Post-Processing Settings:</>\nThe below settings all refer to your project's post-processing settings. Post-processing can be very expensive in VR, so we recommend disabling many expensive post-processing effects. You can fine-tune your post-processing settings with a Post Process Volume. <a href=\"https://docs.unrealengine.com/SharingAndReleasing/XRDevelopment/VR/VRPerformanceAndProfiling\" id=\"HyperlinkDecorator\">Read more.</>."));
	AddSimpleSetting(box, SimpleSettings.Find(FName("LensFlare")));
	AddSimpleSetting(box, SimpleSettings.Find(FName("AntiAliasing")));

	DynamicLights.Empty();

	for (TObjectIterator<ULightComponentBase> LightItr; LightItr; ++LightItr)
	{
		AActor* owner = LightItr->GetOwner();
		if (owner != NULL && (owner->IsRootComponentStationary() || owner->IsRootComponentMovable()) && !owner->IsHiddenEd() && LightItr->IsVisible() && owner->IsEditable() && owner->IsSelectable() && LightItr->GetWorld() == GEditor->GetEditorWorldContext().World())
		{
			// GetFullGroupName() must be used as the key as GetName() is not unique
			FString lightIgnoreKey = "IgnoreLight_" + LightItr->GetFullGroupName(false);
			if (!SettingIgnored(FName(lightIgnoreKey.GetCharArray().GetData())))
			{
				DynamicLights.Add(LightItr->GetFullGroupName(false), TWeakObjectPtr<ULightComponentBase>(*LightItr));
			}
		}
	}

	if (DynamicLights.Num() > 0)
	{
		box = NewCategory(scroller, LOCTEXT("DynamicLightsHeader", "<RichTextBlock.Bold>Dynamic Lights:</>\nThe following lights are not static. They will use dynamic lighting instead of lightmaps, and will be much more expensive on the GPU. (Most of the cost will show up in the GPU profiler as ShadowDepths and ShadowProjectonOnOpaque.) In some cases they will also give superior results. This is a fidelity-performance tradeoff. <a href=\"https://docs.unrealengine.com/en-us/Engine/Rendering/LightingAndShadows/LightMobility\" id=\"HyperlinkDecorator\">Read more.</>\nFixes: select the light and change its mobility to stationary to pre-compute its lighting. You will need to rebuild lightmaps. Alternatively, you can disable Cast Shadows."));

		for (auto it = DynamicLights.CreateIterator(); it; ++it)
		{
			box.Get().AddSlot()
			.Padding(5, 5)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot().FillWidth(5).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(it->Key))
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("SelectLight", "Select Light"))
					.OnClicked(this, &SOculusToolWidget::SelectLight, it->Key)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("IgnoreLight", "Ignore Light"))
					.OnClicked(this, &SOculusToolWidget::IgnoreLight, it->Key)
				]
			];
		}
	}

	box = NewCategory(scroller, LOCTEXT("ShaderPermutationHeader", "<RichTextBlock.Bold>Shader Permutation Reduction:</>\nThe below settings all refer to your project's shader permutation settings."));
	AddSimpleSetting(box, SimpleSettings.Find(FName("MobileShaderStaticAndCSMShadowReceivers")));
	AddSimpleSetting(box, SimpleSettings.Find(FName("MobileShaderAllowDistanceFieldShadows")));
	AddSimpleSetting(box, SimpleSettings.Find(FName("MobileShaderAllowMovableDirectionalLights")));
	AddSimpleSetting(box, SimpleSettings.Find(FName("MobileMovableSpotlights")));

	box = NewCategory(scroller, FText::GetEmpty());
	box.Get().AddSlot()
	.Padding(10, 5)
	[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(10)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("UnhidePerfIgnores", "Unhide all ignored recommendations.")).AutoWrapText(true)
					.Visibility(this, &SOculusToolWidget::CanUnhideIgnoredRecommendations)
				]
			+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("UnhidePerfIgnoresButton", "Unhide"))
					.OnClicked(this, &SOculusToolWidget::UnhideIgnoredRecommendations)
					.Visibility(this, &SOculusToolWidget::CanUnhideIgnoredRecommendations)
				]
	];
	box.Get().AddSlot()
	.Padding(10, 5).AutoHeight()
	[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(10)
			+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("RefreshButton", "Refresh"))
					.OnClicked(this, &SOculusToolWidget::UnhideIgnoredRecommendations)
				]
	];
}

void SOculusToolWidget::Construct(const FArguments& InArgs)
{
	pendingRestart = false;
	PlatformEnum = StaticEnum<EOculusPlatform>();
	Platforms.Reset(2);

	UDEPRECATED_UOculusEditorSettings* EditorSettings = GetMutableDefault<UDEPRECATED_UOculusEditorSettings>();
	for (uint8 i = 0; i < (uint8)EOculusPlatform::Length; ++i)
	{
		Platforms.Add(MakeShareable(new FString(PlatformEnum->GetDisplayNameTextByIndex((int64)i).ToString())));
	}

	PostProcessVolume = NULL;
	for (TActorIterator<APostProcessVolume> ActorItr(GEditor->GetEditorWorldContext().World()); ActorItr; ++ActorItr)
	{
		PostProcessVolume = *ActorItr;
	}

	SimpleSettings.Add(FName("StartInVR"), {
		FName("StartInVR"),
		LOCTEXT("StartInVRDescription", "Enable the \"Start in VR\" setting to ensure your app starts in VR. (You can also ignore this and pass -vr at the command line.)"),
		&SOculusToolWidget::StartInVRVisibility,
		TArray<SimpleSettingAction>(),
		(int)SupportFlags::SupportPC
	});
	SimpleSettings.Find(FName("StartInVR"))->actions.Add(
		{ LOCTEXT("StartInVRButtonText", "Enable Start in VR"),
		&SOculusToolWidget::StartInVREnable }
	);

	SimpleSettings.Add(FName("SupportDash"), {
		FName("SupportDash"),
		LOCTEXT("SupportDashDescription", "Dash support is not enabled. Click to enable it, but make sure to handle the appropriate focus events. <a href=\"https://developer.oculus.com/documentation/unreal/latest/concepts/unreal-dash/\" id=\"HyperlinkDecorator\">Read more.</>"),
		&SOculusToolWidget::SupportDashVisibility,
		TArray<SimpleSettingAction>(),
		(int)SupportFlags::SupportPC
	});
	SimpleSettings.Find(FName("SupportDash"))->actions.Add(
		{ LOCTEXT("SupportDashButtonText", "Enable Dash Support"),
		&SOculusToolWidget::SupportDashEnable }
	);

	SimpleSettings.Add(FName("ForwardShading"), {
		FName("ForwardShading"),
		LOCTEXT("ForwardShadingDescription", "Forward shading is not enabled for this project. Forward shading is often better suited for VR rendering. <a href=\"https://docs.unrealengine.com/en-us/Engine/Performance/ForwardRenderer\" id=\"HyperlinkDecorator\">Read more.</>"),
		&SOculusToolWidget::ForwardShadingVisibility,
		TArray<SimpleSettingAction>(),
		(int)SupportFlags::SupportPC // | (int)SupportFlags::SupportMobile // not including mobile because mobile is forced to use forward regardless of this setting
	});
	SimpleSettings.Find(FName("ForwardShading"))->actions.Add(
		{ LOCTEXT("ForwardShadingButtonText", "Enable Forward Shading"),
		&SOculusToolWidget::ForwardShadingEnable }
	);

	SimpleSettings.Add(FName("MultiView"), {
		FName("MultiView"),
		LOCTEXT("InstancedStereoDescription", "Instanced stereo is not enabled for this project. Instanced stereo substantially reduces draw calls, and improves rendering performance."),
		&SOculusToolWidget::MultiViewVisibility,
		TArray<SimpleSettingAction>(),
		(int)SupportFlags::SupportPC
	});
	SimpleSettings.Find(FName("MultiView"))->actions.Add(
		{ LOCTEXT("InstancedStereoButtonText", "Enable Instanced Stereo"),
		&SOculusToolWidget::MultiViewEnable }
	);

	SimpleSettings.Add(FName("MobileMultiView"), {
		FName("MobileMultiView"),
		LOCTEXT("MobileMultiViewDescription", "Enable mobile multi-view and direct mobile multi-view to significantly reduce CPU overhead."),
		&SOculusToolWidget::MobileMultiViewVisibility,
		TArray<SimpleSettingAction>(),
		(int)SupportFlags::SupportMobile
	});
	SimpleSettings.Find(FName("MobileMultiView"))->actions.Add(
		{ LOCTEXT("MobileMultiViewButton", "Enable Multi-View"),
		&SOculusToolWidget::MobileMultiViewEnable }
	);

	SimpleSettings.Add(FName("MobileMSAA"), {
		FName("MobileMSAA"),
		LOCTEXT("MobileMSAADescription", "Enable Mobile MSAA 4x to get higher quality antialiasing at a reasonable cost on mobile codepaths."),
		&SOculusToolWidget::MobileMSAAVisibility,
		TArray<SimpleSettingAction>(),
		(int)SupportFlags::SupportMobile
	});
	SimpleSettings.Find(FName("MobileMSAA"))->actions.Add(
		{ LOCTEXT("MobileMSAAButton", "Enable MSAA 4x"),
		&SOculusToolWidget::MobileMSAAEnable }
	);

	SimpleSettings.Add(FName("MobilePostProcessing"), {
		FName("MobilePostProcessing"),
		LOCTEXT("MobileHDRDescription", "Mobile HDR has performance and stability issues in VR. We strongly recommend disabling it."),
		&SOculusToolWidget::MobilePostProcessingVisibility,
		TArray<SimpleSettingAction>(),
		(int)SupportFlags::SupportMobile
	});
	SimpleSettings.Find(FName("MobilePostProcessing"))->actions.Add(
		{ LOCTEXT("MobileHDRButton", "Disable Mobile HDR"),
		&SOculusToolWidget::MobilePostProcessingDisable }
	);

	SimpleSettings.Add(FName("MobileVulkan"), {
		FName("MobileVulkan"),
		LOCTEXT("MobileVulkanDescription", "Oculus recommends using Vulkan as the rendering backend for all mobile apps."),
		&SOculusToolWidget::MobileVulkanVisibility,
		TArray<SimpleSettingAction>(),
		(int)SupportFlags::SupportMobile
		});
	SimpleSettings.Find(FName("MobileVulkan"))->actions.Add(
		{ LOCTEXT("MobileVulkanButton", "Use Vulkan Rendering Backend"),
		&SOculusToolWidget::MobileVulkanEnable }
	);

	SimpleSettings.Add(FName("AndroidManifest"), {
		FName("AndroidManifest"),
		LOCTEXT("AndroidManifestDescription", "You need to select a target device in \"Package for Oculus Mobile device\" for all mobile apps. <a href=\"https://developer.oculus.com/documentation/unreal/latest/concepts/unreal-quick-start-guide-go/\" id=\"HyperlinkDecorator\">Read more.</>"),
		&SOculusToolWidget::AndroidManifestVisibility,
		TArray<SimpleSettingAction>(),
		(int)SupportFlags::SupportMobile
	});
	SimpleSettings.Find(FName("AndroidManifest"))->actions.Add(
		{ LOCTEXT("AndroidManifestButtonQuest", "Select Oculus Quest"),
		&SOculusToolWidget::AndroidManifestQuest }
	);

	SimpleSettings.Add(FName("AndroidPackaging"), {
		FName("AndroidPackaging"),
		LOCTEXT("AndroidPackagingDescription", "Some mobile packaging settings need to be fixed. (SDK versions, and FullScreen Immersive settings.) <a href=\"https://developer.oculus.com/documentation/unreal/latest/concepts/unreal-quick-start-guide-go/\" id=\"HyperlinkDecorator\">Read more.</>"),
		&SOculusToolWidget::AndroidPackagingVisibility,
		TArray<SimpleSettingAction>(),
		(int)SupportFlags::SupportMobile
	});
	SimpleSettings.Find(FName("AndroidPackaging"))->actions.Add(
		{ LOCTEXT("AndroidPackagingButton", "Configure Android Packaging"),
		&SOculusToolWidget::AndroidPackagingFix }
	);

	SimpleSettings.Add(FName("AndroidQuestArch"), {
		FName("AndroidQuestArch"),
		LOCTEXT("AndroidQuestArchDescription", "Oculus Quest store requires 64-bit applications. <a href=\"https://developer.oculus.com/blog/quest-submission-policy-update-64-bit-by-default/\" id=\"HyperlinkDecorator\">Read more.</>"),
		&SOculusToolWidget::AndroidQuestArchVisibility,
		TArray<SimpleSettingAction>(),
		(int)SupportFlags::SupportMobile
	});
	SimpleSettings.Find(FName("AndroidQuestArch"))->actions.Add(
		{ LOCTEXT("AndroidQuestArchButton", "Enable Android Arm64 CPU architecture support"),
		&SOculusToolWidget::AndroidQuestArchFix }
	);

	// Post-Processing Settings
	SimpleSettings.Add(FName("LensFlare"), {
		FName("LensFlare"),
		LOCTEXT("LensFlareDescription", "Lens flare is enabled. It can be expensive, and exhibit visible artifacts in VR."),
		&SOculusToolWidget::LensFlareVisibility,
		TArray<SimpleSettingAction>(),
		(int)SupportFlags::SupportMobile | (int)SupportFlags::SupportPC
	});
	SimpleSettings.Find(FName("LensFlare"))->actions.Add(
		{ LOCTEXT("LensFlareButton", "Disable Lens Flare"),
		&SOculusToolWidget::LensFlareDisable }
	);

	// Only used for PC right now. Mobile MSAA is a separate setting.
	SimpleSettings.Add(FName("AntiAliasing"), {
		FName("AntiAliasing"),
		LOCTEXT("AntiAliasingDescription", "The forward render supports MSAA and Temporal anti-aliasing. Enable one of these for the best VR visual-performance tradeoff. (This button will enable temporal anti-aliasing. You can enable MSAA instead in Edit -> Project Settings -> Rendering.)"),
		&SOculusToolWidget::AntiAliasingVisibility,
		TArray<SimpleSettingAction>(),
		(int)SupportFlags::SupportPC | (int)SupportFlags::ExcludeDeferred
	});
	SimpleSettings.Find(FName("AntiAliasing"))->actions.Add(
		{ LOCTEXT("AntiAliasingButton", "Enable Temporal AA"),
		&SOculusToolWidget::AntiAliasingEnable }
	);

	SimpleSettings.Add(FName("AllowStaticLighting"), {
		FName("AllowStaticLighting"),
		LOCTEXT("AllowStaticLightingDescription", "Your project does not allow static lighting. You should only disallow static lighting if you intend for your project to be 100% dynamically lit."),
		&SOculusToolWidget::AllowStaticLightingVisibility,
		TArray<SimpleSettingAction>(),
		(int)SupportFlags::SupportMobile | (int)SupportFlags::SupportPC
	});
	SimpleSettings.Find(FName("AllowStaticLighting"))->actions.Add(
		{ LOCTEXT("AllowStaticLightingButton", "Allow Static Lighting"),
		&SOculusToolWidget::AllowStaticLightingEnable }
	);

	// Mobile Shader Permutation Reduction
	SimpleSettings.Add(FName("MobileShaderStaticAndCSMShadowReceivers"), {
		FName("MobileShaderStaticAndCSMShadowReceivers"),
		LOCTEXT("MobileShaderStaticAndCSMShadowReceiversDescription", "Your project does not contain any stationary lights. Support Combined Static and CSM Shadowing can be disabled to reduce shader permutations."),
		&SOculusToolWidget::MobileShaderStaticAndCSMShadowReceiversVisibility,
		TArray<SimpleSettingAction>(),
		(int)SupportFlags::SupportMobile
		});
	SimpleSettings.Find(FName("MobileShaderStaticAndCSMShadowReceivers"))->actions.Add(
		{ LOCTEXT("MobileShaderStaticAndCSMShadowReceiversButton", "Disable Support Combined Static and CSM Shadowing"),
		&SOculusToolWidget::MobileShaderStaticAndCSMShadowReceiversDisable }
	);

	SimpleSettings.Add(FName("MobileShaderAllowDistanceFieldShadows"), {
		FName("MobileShaderAllowDistanceFieldShadows"),
		LOCTEXT("MobileShaderAllowDistanceFieldShadowsDescription", "Your project does not contain any stationary lights. Support Support Distance Field Shadows can be disabled to reduce shader permutations."),
		&SOculusToolWidget::MobileShaderAllowDistanceFieldShadowsVisibility,
		TArray<SimpleSettingAction>(),
		(int)SupportFlags::SupportMobile
		});
	SimpleSettings.Find(FName("MobileShaderAllowDistanceFieldShadows"))->actions.Add(
		{ LOCTEXT("MobileShaderAllowDistanceFieldShadowsButton", "Disable Support Support Distance Field Shadows"),
		&SOculusToolWidget::MobileShaderAllowDistanceFieldShadowsDisable }
	);

	SimpleSettings.Add(FName("MobileShaderAllowMovableDirectionalLights"), {
		FName("MobileShaderAllowMovableDirectionalLights"),
		LOCTEXT("MobileShaderAllowMovableDirectionalLightsDescription", "Your project does not contain any movable lights. Support Movable Directional Lights can be disabled to reduce shader permutations."),
		& SOculusToolWidget::MobileShaderAllowMovableDirectionalLightsVisibility,
		TArray<SimpleSettingAction>(),
		(int)SupportFlags::SupportMobile
		});
	SimpleSettings.Find(FName("MobileShaderAllowMovableDirectionalLights"))->actions.Add(
		{ LOCTEXT("MobileShaderAllowMovableDirectionalLightsButton", "Disable Support Movable Directional Lights"),
		&SOculusToolWidget::MobileShaderAllowMovableDirectionalLightsDisable }
	);

	auto scroller = SNew(SScrollBox);
	ScrollingContainer = scroller;
	RebuildLayout();

	ChildSlot
		[
			SNew(SBorder)
			.BorderImage( FAppStyle::GetBrush("ToolPanel.LightGroupBorder") )
			.Padding(2)
			[
				scroller
			]
		];
}

void SOculusToolWidget::OnBrowserLinkClicked(const FSlateHyperlinkRun::FMetadata& Metadata)
{
	const FString* url = Metadata.Find(TEXT("href"));

	if ( url != NULL )
	{
		FPlatformProcess::LaunchURL(**url, NULL, NULL);
	}
}

FReply SOculusToolWidget::OnRestartClicked()
{
	FUnrealEdMisc::Get().RestartEditor(true);
	return FReply::Handled();
}

EVisibility SOculusToolWidget::RestartVisible() const
{
	return pendingRestart ? EVisibility::Visible : EVisibility::Collapsed;
}

void SOculusToolWidget::OnChangePlatform(TSharedPtr<FString> ItemSelected, ESelectInfo::Type SelectInfo)
{
	if (!ItemSelected.IsValid())
	{
		return;
	}

	int32 idx = PlatformEnum->GetIndexByNameString(*ItemSelected);
	if (idx != INDEX_NONE)
	{
		UDEPRECATED_UOculusEditorSettings* EditorSettings = GetMutableDefault<UDEPRECATED_UOculusEditorSettings>();
		EditorSettings->PerfToolTargetPlatform = (EOculusPlatform)idx;
		EditorSettings->SaveConfig();
	}
	RebuildLayout();
}

FReply SOculusToolWidget::IgnoreRecommendation(FName tag)
{
	UDEPRECATED_UOculusEditorSettings* EditorSettings = GetMutableDefault<UDEPRECATED_UOculusEditorSettings>();
	EditorSettings->PerfToolIgnoreList.Add(tag, true);
	EditorSettings->SaveConfig();
	return FReply::Handled();
}

EVisibility SOculusToolWidget::CanUnhideIgnoredRecommendations() const
{
	UDEPRECATED_UOculusEditorSettings* EditorSettings = GetMutableDefault<UDEPRECATED_UOculusEditorSettings>();
	return EditorSettings->PerfToolIgnoreList.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply SOculusToolWidget::UnhideIgnoredRecommendations()
{
	UDEPRECATED_UOculusEditorSettings* EditorSettings = GetMutableDefault<UDEPRECATED_UOculusEditorSettings>();
	EditorSettings->PerfToolIgnoreList.Empty();
	EditorSettings->SaveConfig();
	RebuildLayout();
	return FReply::Handled();
}

bool SOculusToolWidget::UsingForwardShading() const
{
	UDEPRECATED_UOculusEditorSettings* EditorSettings = GetMutableDefault<UDEPRECATED_UOculusEditorSettings>();
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	EOculusPlatform targetPlatform = EditorSettings->PerfToolTargetPlatform;
	return targetPlatform == EOculusPlatform::Mobile || Settings->bForwardShading;

}

FReply SOculusToolWidget::Refresh()
{
	RebuildLayout();
	return FReply::Handled();
}

void SOculusToolWidget::SuggestRestart()
{
	pendingRestart = true;
}

FReply SOculusToolWidget::ForwardShadingEnable(bool text)
{
	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(ANSI_TO_TCHAR("r.ForwardShading"));
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	Settings->bForwardShading = 1;
	Settings->UpdateSinglePropertyInConfigFile(Settings->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(URendererSettings, bForwardShading)), Settings->GetDefaultConfigFilename());
	SuggestRestart();
	return FReply::Handled();
}

EVisibility SOculusToolWidget::ForwardShadingVisibility(FName tag) const
{
	return UsingForwardShading() ? EVisibility::Collapsed : EVisibility::Visible;
}

FReply SOculusToolWidget::MultiViewEnable(bool text)
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	Settings->bMultiView = 1;
	Settings->UpdateSinglePropertyInConfigFile(Settings->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(URendererSettings, bMultiView)), Settings->GetDefaultConfigFilename());
	SuggestRestart();
	return FReply::Handled();
}

EVisibility SOculusToolWidget::MultiViewVisibility(FName tag) const
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	const bool bMultiView = Settings->bMultiView != 0;

	return bMultiView ? EVisibility::Collapsed : EVisibility::Visible;
}

FReply SOculusToolWidget::MobileMultiViewEnable(bool text)
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	Settings->bMobileMultiView = 1;
	Settings->UpdateSinglePropertyInConfigFile(Settings->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(URendererSettings, bMobileMultiView)), Settings->GetDefaultConfigFilename());
	SuggestRestart();
	return FReply::Handled();
}

EVisibility SOculusToolWidget::MobileMultiViewVisibility(FName tag) const
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	const bool bMMV = Settings->bMobileMultiView != 0;

	return bMMV ? EVisibility::Collapsed : EVisibility::Visible;
}

FReply SOculusToolWidget::MobileMSAAEnable(bool text)
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	Settings->MobileAntiAliasing = EMobileAntiAliasingMethod::MSAA;
	Settings->UpdateSinglePropertyInConfigFile(Settings->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(URendererSettings, MobileAntiAliasing)), Settings->GetDefaultConfigFilename());
	Settings->MSAASampleCount = ECompositingSampleCount::Four;
	Settings->UpdateSinglePropertyInConfigFile(Settings->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(URendererSettings, MSAASampleCount)), Settings->GetDefaultConfigFilename());
	return FReply::Handled();
}

EVisibility SOculusToolWidget::MobileMSAAVisibility(FName tag) const
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	const bool bMobileMSAAValid = Settings->MobileAntiAliasing == EMobileAntiAliasingMethod::MSAA && Settings->MSAASampleCount == ECompositingSampleCount::Four;

	return bMobileMSAAValid ? EVisibility::Collapsed : EVisibility::Visible;
}

FReply SOculusToolWidget::MobileVulkanEnable(bool text)
{
	UAndroidRuntimeSettings* Settings = GetMutableDefault<UAndroidRuntimeSettings>();
	Settings->bSupportsVulkan = true;
	Settings->bBuildForES31 = false;
	Settings->UpdateSinglePropertyInConfigFile(Settings->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UAndroidRuntimeSettings, bSupportsVulkan)), Settings->GetDefaultConfigFilename());
	Settings->UpdateSinglePropertyInConfigFile(Settings->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UAndroidRuntimeSettings, bBuildForES31)), Settings->GetDefaultConfigFilename());
	return FReply::Handled();
}

EVisibility SOculusToolWidget::MobileVulkanVisibility(FName tag) const
{
	UAndroidRuntimeSettings* Settings = GetMutableDefault<UAndroidRuntimeSettings>();
	return Settings->bSupportsVulkan && !Settings->bBuildForES31 ? EVisibility::Collapsed : EVisibility::Visible;
}

FReply SOculusToolWidget::MobilePostProcessingDisable(bool text)
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	Settings->bMobilePostProcessing = 0;
	Settings->UpdateSinglePropertyInConfigFile(Settings->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(URendererSettings, bMobilePostProcessing)), Settings->GetDefaultConfigFilename());
	SuggestRestart();
	return FReply::Handled();
}

EVisibility SOculusToolWidget::MobilePostProcessingVisibility(FName tag) const
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	return Settings->bMobilePostProcessing == 0 ? EVisibility::Collapsed : EVisibility::Visible;
}

FString SOculusToolWidget::GetConfigPath() const
{
	return FString::Printf(TEXT("%sDefaultEngine.ini"), *FPaths::SourceConfigDir());
}

FReply SOculusToolWidget::AndroidManifestQuest(bool text)
{
	UAndroidRuntimeSettings* Settings = GetMutableDefault<UAndroidRuntimeSettings>();
	Settings->PackageForOculusMobile.Add(EOculusMobileDevice::Quest);
	Settings->PackageForOculusMobile.Add(EOculusMobileDevice::Quest2);
	Settings->SaveConfig(CPF_Config, *Settings->GetDefaultConfigFilename()); // UpdateSinglePropertyInConfigFile does not support arrays
	return FReply::Handled();
}

const int MIN_SDK_VERSION = 23;

EVisibility SOculusToolWidget::AndroidManifestVisibility(FName tag) const
{
	UAndroidRuntimeSettings* Settings = GetMutableDefault<UAndroidRuntimeSettings>();
	return Settings->PackageForOculusMobile.Num() <= 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply SOculusToolWidget::AndroidPackagingFix(bool text)
{
	UAndroidRuntimeSettings* Settings = GetMutableDefault<UAndroidRuntimeSettings>();
	Settings->bFullScreen = true;
	Settings->MinSDKVersion = MIN_SDK_VERSION;
	Settings->TargetSDKVersion = MIN_SDK_VERSION;
	Settings->UpdateSinglePropertyInConfigFile(Settings->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UAndroidRuntimeSettings, bFullScreen)), Settings->GetDefaultConfigFilename());
	Settings->UpdateSinglePropertyInConfigFile(Settings->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UAndroidRuntimeSettings, MinSDKVersion)), Settings->GetDefaultConfigFilename());
	Settings->UpdateSinglePropertyInConfigFile(Settings->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UAndroidRuntimeSettings, TargetSDKVersion)), Settings->GetDefaultConfigFilename());
	return FReply::Handled();
}

EVisibility SOculusToolWidget::AndroidPackagingVisibility(FName tag) const
{
	UAndroidRuntimeSettings* Settings = GetMutableDefault<UAndroidRuntimeSettings>();
	return (
		Settings->MinSDKVersion < MIN_SDK_VERSION || 
		Settings->TargetSDKVersion < MIN_SDK_VERSION ||
		!Settings->bFullScreen
	) ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply SOculusToolWidget::AndroidQuestArchFix(bool text)
{
	UAndroidRuntimeSettings* Settings = GetMutableDefault<UAndroidRuntimeSettings>();
	Settings->bBuildForArm64 = true;
	Settings->bBuildForX8664 = false;
	Settings->UpdateSinglePropertyInConfigFile(Settings->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UAndroidRuntimeSettings, bBuildForArm64)), Settings->GetDefaultConfigFilename());
	Settings->UpdateSinglePropertyInConfigFile(Settings->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UAndroidRuntimeSettings, bBuildForX8664)), Settings->GetDefaultConfigFilename());
	return FReply::Handled();
}

EVisibility SOculusToolWidget::AndroidQuestArchVisibility(FName tag) const
{
	UAndroidRuntimeSettings* Settings = GetMutableDefault<UAndroidRuntimeSettings>();
	return (Settings->PackageForOculusMobile.Contains(EOculusMobileDevice::Quest) || Settings->PackageForOculusMobile.Contains(EOculusMobileDevice::Quest2)) && !Settings->bBuildForArm64 ?
		EVisibility::Visible : EVisibility::Collapsed;
}

FReply SOculusToolWidget::AntiAliasingEnable(bool text)
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	Settings->DefaultFeatureAntiAliasing = EAntiAliasingMethod::AAM_TemporalAA; // TODO(TSR)
	Settings->UpdateSinglePropertyInConfigFile(Settings->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(URendererSettings, DefaultFeatureAntiAliasing)), Settings->GetDefaultConfigFilename());
	return FReply::Handled();
}

EVisibility SOculusToolWidget::AntiAliasingVisibility(FName tag) const
{
	// TODO: can we get MSAA level? 2 is fast, 4 is reasonable, anything higher is insane.
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();

	static IConsoleVariable* CVarMSAACount = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MSAACount"));
	CVarMSAACount->Set(4);

	const bool bAADisabled = UsingForwardShading() && Settings->DefaultFeatureAntiAliasing != EAntiAliasingMethod::AAM_TemporalAA && Settings->DefaultFeatureAntiAliasing != EAntiAliasingMethod::AAM_MSAA;

	return bAADisabled ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply SOculusToolWidget::AllowStaticLightingEnable(bool text)
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	Settings->bAllowStaticLighting = true;
	Settings->UpdateSinglePropertyInConfigFile(Settings->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(URendererSettings, bAllowStaticLighting)), Settings->GetDefaultConfigFilename());
	SuggestRestart();
	return FReply::Handled();
}

EVisibility SOculusToolWidget::AllowStaticLightingVisibility(FName tag) const
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	return Settings->bAllowStaticLighting ? EVisibility::Collapsed : EVisibility::Visible;
}

FReply SOculusToolWidget::MobileShaderStaticAndCSMShadowReceiversDisable(bool text)
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	Settings->bMobileEnableStaticAndCSMShadowReceivers = false;
	Settings->UpdateSinglePropertyInConfigFile(Settings->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(URendererSettings, bMobileEnableStaticAndCSMShadowReceivers)), Settings->GetDefaultConfigFilename());
	SuggestRestart();
	return FReply::Handled();
}

EVisibility SOculusToolWidget::MobileShaderStaticAndCSMShadowReceiversVisibility(FName tag) const
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	if (!Settings->bMobileEnableStaticAndCSMShadowReceivers)
	{
		return EVisibility::Collapsed;
	}

	for (const auto& kvp : DynamicLights)
	{
		AActor* owner = kvp.Value->GetOwner();
		if (owner != NULL && owner->IsRootComponentStationary())
		{
			return EVisibility::Collapsed;
		}
	}

	return EVisibility::Visible;
}

FReply SOculusToolWidget::MobileShaderAllowDistanceFieldShadowsDisable(bool text)
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	Settings->bMobileAllowDistanceFieldShadows = false;
	Settings->UpdateSinglePropertyInConfigFile(Settings->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(URendererSettings, bMobileAllowDistanceFieldShadows)), Settings->GetDefaultConfigFilename());
	SuggestRestart();
	return FReply::Handled();
}

EVisibility SOculusToolWidget::MobileShaderAllowDistanceFieldShadowsVisibility(FName tag) const
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	if (!Settings->bMobileAllowDistanceFieldShadows)
	{
		return EVisibility::Collapsed;
	}

	for (const auto& kvp : DynamicLights)
	{
		AActor* owner = kvp.Value->GetOwner();
		if (owner != NULL && owner->IsRootComponentStationary())
		{
			return EVisibility::Collapsed;
		}
	}

	return EVisibility::Visible;
}

FReply SOculusToolWidget::MobileShaderAllowMovableDirectionalLightsDisable(bool text)
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	Settings->bMobileAllowMovableDirectionalLights = false;
	Settings->UpdateSinglePropertyInConfigFile(Settings->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(URendererSettings, bMobileAllowMovableDirectionalLights)), Settings->GetDefaultConfigFilename());
	SuggestRestart();
	return FReply::Handled();
}

EVisibility SOculusToolWidget::MobileShaderAllowMovableDirectionalLightsVisibility(FName tag) const
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	if (!Settings->bMobileAllowMovableDirectionalLights)
	{
		return EVisibility::Collapsed;
	}

	for (const auto& kvp : DynamicLights)
	{
		AActor* owner = kvp.Value->GetOwner();
		if (owner != NULL && owner->IsRootComponentMovable())
		{
			return EVisibility::Collapsed;
		}
	}

	return EVisibility::Visible;
}

void SOculusToolWidget::OnShowButtonChanged(ECheckBoxState NewState)
{
	GConfig->SetBool(TEXT("/Script/OculusEditor.OculusEditorSettings"), TEXT("bAddMenuOption"), NewState == ECheckBoxState::Checked ? true : false, FString::Printf(TEXT("%sDefaultEditor.ini"), *FPaths::SourceConfigDir()));
	GConfig->Flush(0);
}

ECheckBoxState SOculusToolWidget::IsShowButtonChecked() const
{
	bool v;
	GConfig->GetBool(TEXT("/Script/OculusEditor.OculusEditorSettings"), TEXT("bAddMenuOption"), v, FString::Printf(TEXT("%sDefaultEditor.ini"), *FPaths::SourceConfigDir()));
	return v ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

FReply SOculusToolWidget::LensFlareDisable(bool text)
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	Settings->bDefaultFeatureLensFlare = false;
	Settings->UpdateSinglePropertyInConfigFile(Settings->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(URendererSettings, bDefaultFeatureLensFlare)), Settings->GetDefaultConfigFilename());

	if (PostProcessVolume != NULL)
	{
		PostProcessVolume->Settings.bOverride_LensFlareIntensity = 0;
		Settings->SaveConfig();
	}

	return FReply::Handled();
}

EVisibility SOculusToolWidget::LensFlareVisibility(FName tag) const
{
	URendererSettings* Settings = GetMutableDefault<URendererSettings>();
	bool bLensFlare = Settings->bDefaultFeatureLensFlare != 0;

	if (PostProcessVolume != NULL)
	{
		if (PostProcessVolume->Settings.bOverride_LensFlareIntensity != 0)
		{
			bLensFlare = PostProcessVolume->Settings.LensFlareIntensity > 0.0f;
		}
	}

	return bLensFlare ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply SOculusToolWidget::SelectLight(FString lightName)
{
	const TWeakObjectPtr< ULightComponentBase>* weakPtr = DynamicLights.Find(lightName);
	if (weakPtr)
	{
		ULightComponentBase* light = weakPtr->Get();
		GEditor->SelectNone(true, true);
		GEditor->SelectActor(light->GetOwner(), true, true);
		GEditor->SelectComponent(light, true, true, true);
	}
	return FReply::Handled();
}

FReply SOculusToolWidget::IgnoreLight(FString lightName)
{
	UDEPRECATED_UOculusEditorSettings* EditorSettings = GetMutableDefault<UDEPRECATED_UOculusEditorSettings>();
	FString lightIgnoreKey = "IgnoreLight_" + lightName;
	EditorSettings->PerfToolIgnoreList.Add(FName(lightIgnoreKey.GetCharArray().GetData()), true);
	EditorSettings->SaveConfig();
	return FReply::Handled();
}

FReply SOculusToolWidget::StartInVREnable(bool text)
{
	UGeneralProjectSettings* Settings = GetMutableDefault<UGeneralProjectSettings>();
	Settings->bStartInVR = 1;
	Settings->UpdateSinglePropertyInConfigFile(Settings->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UGeneralProjectSettings, bStartInVR)), Settings->GetDefaultConfigFilename());
	return FReply::Handled();
}

EVisibility SOculusToolWidget::StartInVRVisibility(FName tag) const
{
	const UGeneralProjectSettings* Settings = GetDefault<UGeneralProjectSettings>();
	const bool bStartInVR = Settings->bStartInVR != 0;
	return bStartInVR ? EVisibility::Collapsed : EVisibility::Visible;
	return EVisibility::Collapsed;
}

FReply SOculusToolWidget::SupportDashEnable(bool text)
{
	UDEPRECATED_UOculusHMDRuntimeSettings* Settings = GetMutableDefault<UDEPRECATED_UOculusHMDRuntimeSettings>();
	Settings->bSupportsDash = true;
	Settings->UpdateSinglePropertyInConfigFile(Settings->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UDEPRECATED_UOculusHMDRuntimeSettings, bSupportsDash)), Settings->GetDefaultConfigFilename());
	return FReply::Handled();
}

EVisibility SOculusToolWidget::SupportDashVisibility(FName tag) const
{
	const UDEPRECATED_UOculusHMDRuntimeSettings* Settings = GetDefault<UDEPRECATED_UOculusHMDRuntimeSettings>();
	return Settings->bSupportsDash ? EVisibility::Collapsed : EVisibility::Visible;
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

#undef LOCTEXT_NAMESPACE
