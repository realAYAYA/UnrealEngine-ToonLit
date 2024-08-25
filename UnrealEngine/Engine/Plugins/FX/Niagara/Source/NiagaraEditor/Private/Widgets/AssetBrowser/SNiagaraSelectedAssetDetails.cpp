// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/AssetBrowser/SNiagaraSelectedAssetDetails.h"
#include "NiagaraAssetTagDefinitions.h"
#include "NiagaraEditorStyle.h"
#include "NiagaraEditorUtilities.h"
#include "SlateOptMacros.h"
#include "Styling/SlateIconFinder.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SScrollBox.h"

#define LOCTEXT_NAMESPACE "SNiagaraSelectedAssetPreview"

TMap<UClass*, FNiagaraAssetDetailClassInfo> FNiagaraAssetDetailDatabase::NiagaraAssetDetailDatabase;

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void FNiagaraAssetDetailDatabase::Init()
{
	FName SimulationTargetTag("HasGPUEmitter");
	
	// Emitters
	{
		FNiagaraAssetDetailClassInfo EmitterClassInfo;
		EmitterClassInfo.DisplayedProperties =
			{
				// Inheritance
				{
					FDisplayedPropertyData::FShouldDisplayProperty(),
					FDisplayedPropertyData::FGenerateWidget::CreateLambda([](const FAssetData& AssetData) -> TSharedRef<SWidget>
					{
						return SNew(STextBlock)
						.Text(LOCTEXT("Inheritance", "Inheritance"));
					}),
					FDisplayedPropertyData::FGenerateWidget::CreateLambda([](const FAssetData& AssetData) -> TSharedRef<SWidget>
					{
						bool bUseInheritance = false;
						if(FNiagaraEditorUtilities::GetIsInheritableFromAssetRegistryTags(AssetData, bUseInheritance))
						{
							return SNew(STextBlock).Text(bUseInheritance
								? LOCTEXT("Emitter_UseInheritance_Yes", "Yes")
								: LOCTEXT("Emitter_UseInheritance_No", "No")
								);
						}

						return SNullWidget::NullWidget;
					})
				},
				// Simulation Target
				{
					FDisplayedPropertyData::FShouldDisplayProperty(),
					FDisplayedPropertyData::FGenerateWidget::CreateLambda([](const FAssetData& AssetData) -> TSharedRef<SWidget>
					{
						return SNew(STextBlock)
						.Text(LOCTEXT("SimulationTargetLabel", "Runs on"));
					}),
					FDisplayedPropertyData::FGenerateWidget::CreateLambda([SimulationTargetTag](const FAssetData& Asset)
					{
						return SNew(SImage)
						.Image_Lambda([Asset, SimulationTargetTag]() -> const FSlateBrush*
						{
							if(Asset.FindTag(SimulationTargetTag))
							{
								FString Value;
								if(Asset.GetTagValue(SimulationTargetTag, Value))
								{
									if(Value == "True")
									{
										return FNiagaraEditorStyle::Get().GetBrush("NiagaraEditor.Stack.GPUIcon");
									}
									else
									{
										return FNiagaraEditorStyle::Get().GetBrush("NiagaraEditor.Stack.CPUIcon");
									}
								}
							}

							return FAppStyle::GetNoBrush();
						});
					})
				}
		};
		EmitterClassInfo.GetDescriptionDelegate = FNiagaraAssetDetailClassInfo::FGetDescription::CreateLambda([](const FAssetData& AssetData)
		{
			FName DescriptionTagName("TemplateAssetDescription");
			if(AssetData.FindTag(DescriptionTagName))
			{
				FString Description;
				AssetData.GetTagValue(DescriptionTagName, Description);
				return FText::FromString(Description);
			}

			return FText::GetEmpty();
		});
		
		NiagaraAssetDetailDatabase.Add(UNiagaraEmitter::StaticClass(), EmitterClassInfo);
	}

	// Systems
	{
		FNiagaraAssetDetailClassInfo SystemClassInfo;
		SystemClassInfo.DisplayedProperties =
		{
			// Number of emitters
			{
				FDisplayedPropertyData::FShouldDisplayProperty::CreateLambda([](const FAssetData& AssetData) -> bool
				{
					return AssetData.FindTag("NumEmitters");
				}),
				FDisplayedPropertyData::FGenerateWidget::CreateLambda([](const FAssetData& AssetData) -> TSharedRef<SWidget>
				{
					return SNew(STextBlock)
					.Text(LOCTEXT("NumberOfEmitters", "Number of Emitters"));
				}),
				FDisplayedPropertyData::FGenerateWidget::CreateLambda([](const FAssetData& AssetData) -> TSharedRef<SWidget>
				{
					int32 NumEmitters = INDEX_NONE;
					if(AssetData.GetTagValue("NumEmitters", NumEmitters))
					{
						return SNew(STextBlock).Text(FText::AsNumber(NumEmitters));
					}

					return SNullWidget::NullWidget;
				})
			}
		};
		SystemClassInfo.GetDescriptionDelegate = FNiagaraAssetDetailClassInfo::FGetDescription::CreateLambda([](const FAssetData& AssetData)
		{
			FName DescriptionTagName("TemplateAssetDescription");
			if(AssetData.FindTag(DescriptionTagName))
			{
				FString Description;
				AssetData.GetTagValue(DescriptionTagName, Description);
				return FText::FromString(Description);
			}
		
			return FText::GetEmpty();
		});
		
		NiagaraAssetDetailDatabase.Add(UNiagaraSystem::StaticClass(), SystemClassInfo);
	}
}

void SNiagaraAssetTag::Construct(const FArguments& InArgs, const FNiagaraAssetTagDefinition& InAssetTagDefinition)
{
	AssetTagDefinition = InAssetTagDefinition;
	OnAssetTagActivated = InArgs._OnAssetTagActivated;
	OnAssetTagActivatedTooltip = InArgs._OnAssetTagActivatedTooltip;

	if(OnAssetTagActivated.IsBound() && OnAssetTagActivatedTooltip.IsSet())
	{
		FText TooltipText = FText::FormatOrdered(FText::AsCultureInvariant("{0}{1}"), AssetTagDefinition.Description,
			OnAssetTagActivatedTooltip.GetValue());

		SetToolTipText(TooltipText);
	}
	else
	{
		SetToolTipText(AssetTagDefinition.Description);
	}

	TSharedPtr<SWidget> ContentWidget = nullptr;

	TSharedRef<STextBlock> DisplayNameWidget = SNew(STextBlock)
		.Text(FText::FromString(AssetTagDefinition.AssetTag.ToString()))
		.TextStyle(&FNiagaraEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("NiagaraEditor.AssetBrowser.AssetTag.Text"));

	if(OnAssetTagActivated.IsBound())
	{
		ContentWidget = SNew(SButton)
		.ButtonStyle(&FAppStyle::GetWidgetStyle<FButtonStyle>("HoverHintOnly"))
		.OnClicked(this, &SNiagaraAssetTag::OnClicked)
		[
			DisplayNameWidget
		];
	}
	else
	{
		ContentWidget = DisplayNameWidget;
	}
	
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FNiagaraEditorStyle::Get().GetBrush("NiagaraEditor.AssetBrowser.AssetTag.OuterBorder"))
		.BorderBackgroundColor(AssetTagDefinition.Color)
		.Padding(1.f)
		[
			SNew(SBorder)
			.BorderImage(FNiagaraEditorStyle::Get().GetBrush("NiagaraEditor.AssetBrowser.AssetTag.InnerBorder"))
			.Padding(8.f, 2.f)
			[
				ContentWidget.ToSharedRef()
			]
		]
	];
}

FReply SNiagaraAssetTag::OnClicked() const
{
	OnAssetTagActivated.ExecuteIfBound(AssetTagDefinition);
	return FReply::Handled();
}

void SNiagaraAssetTagRow::Construct(const FArguments& InArgs, const FAssetData& Asset)
{
	using namespace FNiagaraEditorUtilities::AssetBrowser;

	TArray<FNiagaraAssetTagDefinition> AllAssetTagDefinitions = GetFlatSortedAssetTagDefinitions();

	TArray<FNiagaraAssetTagDefinition> MatchingAssetTagDefinitions;
	for(const FNiagaraAssetTagDefinition& AssetTagDefinition : AllAssetTagDefinitions)
	{
		if(AssetTagDefinition.DoesAssetDataContainTag(Asset))
		{
			MatchingAssetTagDefinitions.Add(AssetTagDefinition);	
		}		
	}

	TSharedRef<SWrapBox> AssetTagRow = SNew(SWrapBox).UseAllottedSize(true);
	
	for(const FNiagaraAssetTagDefinition& MatchingAssetTagDefinition : MatchingAssetTagDefinitions)
	{
		if(InArgs._DisplayType.IsSet() && InArgs._DisplayType.GetValue() != MatchingAssetTagDefinition.DisplayType)
		{
			continue;
		}
			
		AssetTagRow->AddSlot()
		.Padding(2.f)
		[
			SNew(SNiagaraAssetTag, MatchingAssetTagDefinition)
			.OnAssetTagActivated(InArgs._OnAssetTagActivated)
			.OnAssetTagActivatedTooltip(InArgs._OnAssetTagActivatedTooltip)
		];
	}

	SetVisibility(AssetTagRow->GetChildren()->Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed);

	TSharedRef<SWidget> ContentWidget = AssetTagRow;

	if(InArgs._DisplayType.IsSet())
	{
		FText Label = StaticEnum<ENiagaraAssetTagDefinitionImportance>()->GetDisplayNameTextByValue((int64) InArgs._DisplayType.GetValue());
		
		ContentWidget = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(STextBlock)
			.Text(FText::FormatOrdered(LOCTEXT("AssetTagRowTypeLabel", "{0} Tags"), Label))
			.TextStyle(&FAppStyle::GetWidgetStyle<FTextBlockStyle>("NormalText.Subdued"))
			.ToolTipText(this, &SNiagaraAssetTagRow::GetDisplayTypeTooltipText, InArgs._DisplayType.GetValue())
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			AssetTagRow
		];
	}
	
	ChildSlot
	[
		ContentWidget
	];
}

FText SNiagaraAssetTagRow::GetDisplayTypeTooltipText(ENiagaraAssetTagDefinitionImportance DisplayType) const
{
	static FText TagDescriptionText = LOCTEXT("GeneralTagsLabelTooltip", "Assets can be assigned primary and secondary tags.\nThese tags are defined in an Niagara Asset Tag Definitions asset and can be assigned to an asset by right-clicking it in the Content Browser under 'Manage Tags'.");
	if(DisplayType == ENiagaraAssetTagDefinitionImportance::Primary)
	{
		return TagDescriptionText;
	}
	else if(DisplayType == ENiagaraAssetTagDefinitionImportance::Secondary)
	{
		return FText::FormatOrdered(LOCTEXT("SecondaryTagsLabelTooltip", "{0}{1}"), TagDescriptionText,
			LOCTEXT("SecondaryTagsLabelTooltipSpecific", "\n\nSecondary tags can be clicked on to manage additional filters.\nThese additional filters can also be accessed from the filters menu next to the search bar."));
	}

	return TagDescriptionText;
}

void SNiagaraSelectedAssetDetails::Construct(const FArguments& InArgs, const FAssetData& Asset)
{
	AssetData = Asset;
	
	ShowThumbnail = InArgs._ShowThumbnail;
	OnAssetTagActivated = InArgs._OnAssetTagActivated;
	OnAssetTagActivatedTooltip = InArgs._OnAssetTagActivatedTooltip;
	
	ChildSlot
	[
		SNew(SBox)
		.MinDesiredWidth(200.f)
		.MaxDesiredWidth(450.f)
		.Padding(11.5f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(0.f, 0.f, 0.f, 22.f)
			[
				CreateAssetThumbnailWidget()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f)
			[
				CreateTitleWidget()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(150.f)
			.Padding(0.f, 6.f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SNew(SBox)
					.MaxDesiredWidth(InArgs._MaxDesiredDescriptionWidth)
					[
						CreateDescriptionWidget()
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 6.f)
			[
				CreateOptionalPropertiesList()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 6.f)
			[
				CreateAssetTagRow()
			]
		]
	]; 
}

TSharedRef<SWidget> SNiagaraSelectedAssetDetails::CreateAssetThumbnailWidget()
{
	CurrentAssetThumbnail = MakeShared<FAssetThumbnail>(AssetData, 256.f, 256.f, UThumbnailManager::Get().GetSharedThumbnailPool());
	FAssetThumbnailConfig Config;
	Config.bAllowRealTimeOnHovered = false;
	
	return SNew(SBox)
	.WidthOverride(256.f)
	.HeightOverride(192.f)
	.Visibility(ShowThumbnail)
	[
		SNew(SScaleBox)
		.Stretch(EStretch::ScaleToFill)
		[
			CurrentAssetThumbnail->MakeThumbnailWidget(Config)
		]
	];
}

TSharedRef<SWidget> SNiagaraSelectedAssetDetails::CreateTitleWidget()
{
	return SNew(SVerticalBox)
	+ SVerticalBox::Slot()
	.AutoHeight()
	.HAlign(HAlign_Left)
	.Padding(2.f)
	[
		SNew(STextBlock)
		.Text(FText::FromName(AssetData.AssetName))
		.TextStyle(&FNiagaraEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("NiagaraEditor.AssetBrowser.AssetTitle"))
	]
	+ SVerticalBox::Slot()
	.AutoHeight()
	.HAlign(HAlign_Left)
	.Padding(2.f)
	[
		CreateTypeWidget()
	];
}

TSharedRef<SWidget> SNiagaraSelectedAssetDetails::CreateTypeWidget()
{
	return SNew(SHorizontalBox)
	+ SHorizontalBox::Slot()
	.AutoWidth()
	.Padding(2.f)
	[
		SNew(SImage)
		.Image(FSlateIconFinder::FindIconForClass(AssetData.GetClass()).GetIcon())
	]
	+ SHorizontalBox::Slot()
	.AutoWidth()
	[
		SNew(STextBlock)
		.Text(AssetData.GetClass()->GetDisplayNameText())
		.TextStyle(&FNiagaraEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("NiagaraEditor.AssetBrowser.AssetType"))
	];
}

TSharedRef<SWidget> SNiagaraSelectedAssetDetails::CreateDescriptionWidget()
{
	if(FNiagaraAssetDetailDatabase::NiagaraAssetDetailDatabase.Contains(AssetData.GetClass()))
	{
		FText Description = FNiagaraAssetDetailDatabase::NiagaraAssetDetailDatabase[AssetData.GetClass()].GetDescriptionDelegate.Execute(AssetData);
		return SNew(STextBlock)
			.AutoWrapText(true)
			.Text(Description)
			.Visibility(Description.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible);
	}

	return SNew(SBox).Visibility(EVisibility::Collapsed);
}

TSharedRef<SWidget> SNiagaraSelectedAssetDetails::CreateOptionalPropertiesList()
{
	if(FNiagaraAssetDetailDatabase::NiagaraAssetDetailDatabase.Contains(AssetData.GetClass()))
	{
		TSharedRef<SVerticalBox> Result = SNew(SVerticalBox);
		
		for(const FDisplayedPropertyData& DisplayedProperty : FNiagaraAssetDetailDatabase::NiagaraAssetDetailDatabase[AssetData.GetClass()].DisplayedProperties)
		{			
			if(DisplayedProperty.ShouldDisplayPropertyDelegate.IsBound())
			{
				if(DisplayedProperty.ShouldDisplayPropertyDelegate.Execute(AssetData) == false)
				{
					continue;
				}
			}
			
			Result->AddSlot()
			.AutoHeight()
			.Padding(10.f, 3.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					DisplayedProperty.NameWidgetDelegate.Execute(AssetData)
				]
				+ SHorizontalBox::Slot()
				[
					SNew(SSpacer)
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.AutoWidth()
				[
					DisplayedProperty.ValueWidgetDelegate.Execute(AssetData)
				]
			];

			Result->AddSlot()
			.AutoHeight()
			.Padding(10.f, 3.f)
			[
				SNew(SSeparator)
				.Orientation(Orient_Horizontal)
				.SeparatorImage(FNiagaraEditorStyle::Get().GetBrush("NiagaraEditor.AssetBrowser.PropertySeparator"))
				.Thickness(1.f)
			];
		}

		if(Result->NumSlots() == 0)
		{
			return SNew(SBox).Visibility(EVisibility::Collapsed);
		}
		
		return Result;
	}

	return SNew(SBox).Visibility(EVisibility::Collapsed);
}

TSharedRef<SWidget> SNiagaraSelectedAssetDetails::CreateAssetTagRow()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SNiagaraAssetTagRow, AssetData)
			//.OnAssetTagActivated(OnAssetTagActivated)
			//.OnAssetTagActivatedTooltip(OnAssetTagActivatedTooltip)
			.DisplayType(ENiagaraAssetTagDefinitionImportance::Primary)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SNiagaraAssetTagRow, AssetData)
			.OnAssetTagActivated(OnAssetTagActivated)
			.DisplayType(ENiagaraAssetTagDefinitionImportance::Secondary)
			.OnAssetTagActivatedTooltip(OnAssetTagActivatedTooltip)
		];	
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
