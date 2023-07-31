// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNiagaraOverviewStackNode.h"

#include "EditorFontGlyphs.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Modules/ModuleManager.h"
#include "NiagaraEditorModule.h"
#include "NiagaraEditorSettings.h"
#include "NiagaraEditorStyle.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEditorWidgetsStyle.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraOverviewNode.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraSettings.h"
#include "SGraphPanel.h"
#include "SLevelOfDetailBranchNode.h"
#include "SNiagaraOverviewStack.h"
#include "Stack/SNiagaraStackIssueIcon.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "ViewModels/NiagaraEmitterHandleViewModel.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "ViewModels/NiagaraSystemSelectionViewModel.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/Stack/NiagaraStackItemGroup.h"
#include "ViewModels/Stack/NiagaraStackModuleItem.h"
#include "ViewModels/Stack/NiagaraStackRendererItem.h"
#include "ViewModels/Stack/NiagaraStackViewModel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SToolTip.h"
#include "Widgets/SWidget.h"

#define LOCTEXT_NAMESPACE "NiagaraOverviewStackNode"

constexpr float ThumbnailSize = 24.0f;

void SNiagaraOverviewStackNode::Construct(const FArguments& InArgs, UNiagaraOverviewNode* InNode)
{
	GraphNode = InNode;
	OverviewStackNode = InNode;
	StackViewModel = nullptr;
	OverviewSelectionViewModel = nullptr;
	bIsHoveringThumbnail = false;
	bTopContentBarRefreshPending = true;
	CurrentIssueIndex = -1;

	EmitterHandleViewModelWeak.Reset();

	TopContentBar = SNew(SHorizontalBox);
	
	if (OverviewStackNode->GetOwningSystem() != nullptr)
	{
		FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::Get().LoadModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
		TSharedPtr<FNiagaraSystemViewModel> OwningSystemViewModel = NiagaraEditorModule.GetExistingViewModelForSystem(OverviewStackNode->GetOwningSystem());
		if (OwningSystemViewModel.IsValid())
		{
			if (OverviewStackNode->GetEmitterHandleGuid().IsValid() == false)
			{
				StackViewModel = OwningSystemViewModel->GetSystemStackViewModel();
			}
			else
			{
				EmitterHandleViewModelWeak = OwningSystemViewModel->GetEmitterHandleViewModelById(OverviewStackNode->GetEmitterHandleGuid());
				if (EmitterHandleViewModelWeak.IsValid())
				{
					StackViewModel = EmitterHandleViewModelWeak.Pin()->GetEmitterStackViewModel();
				}
			}
			if (StackViewModel)
			{
				StackViewModel->OnStructureChanged().AddSP(this, &SNiagaraOverviewStackNode::StackViewModelStructureChanged);
				StackViewModel->OnDataObjectChanged().AddSP(this, &SNiagaraOverviewStackNode::StackViewModelDataObjectChanged);
			}
			UMaterial::OnMaterialCompilationFinished().AddSP(this, &SNiagaraOverviewStackNode::OnMaterialCompiled);
			OverviewSelectionViewModel = OwningSystemViewModel->GetSelectionViewModel();
			ScalabilityViewModel = OwningSystemViewModel->GetScalabilityViewModel();

			if(ScalabilityViewModel.IsValid())
			{
				bScalabilityModeActive = ScalabilityViewModel->IsActive();
				ScalabilityViewModel->OnScalabilityModeChanged().AddSP(this, &SNiagaraOverviewStackNode::OnScalabilityModeChanged);
			}
		}
	}

	UpdateGraphNode();
}

SNiagaraOverviewStackNode::~SNiagaraOverviewStackNode()
{
	if(ScalabilityViewModel.IsValid())
	{
		ScalabilityViewModel->OnScalabilityModeChanged().RemoveAll(this);
	}
}

TSharedRef<SWidget> SNiagaraOverviewStackNode::CreateTitleWidget(TSharedPtr<SNodeTitle> NodeTitle)
{
	TSharedRef<SWidget> DefaultTitle = SGraphNode::CreateTitleWidget(NodeTitle);

	if (StackViewModel == nullptr)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(0, 0, 5, 0)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("InvalidNode", "INVALID"))
			]
			+ SHorizontalBox::Slot()
			[
				DefaultTitle
			];
	}

	return SNew(SHorizontalBox)
		// Enabled checkbox
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0)
		[
			SNew(SCheckBox)
			.Visibility(this, &SNiagaraOverviewStackNode::GetEnabledCheckBoxVisibility)
			.IsChecked(this, &SNiagaraOverviewStackNode::GetEnabledCheckState)
			.OnCheckStateChanged(this, &SNiagaraOverviewStackNode::OnEnabledCheckStateChanged)
		]
		// Name
		+ SHorizontalBox::Slot()
		.Padding(3, 0, 0, 0)
		.FillWidth(1.0f)
		[
			DefaultTitle
		];
}

TSharedRef<SWidget> SNiagaraOverviewStackNode::CreateTitleRightWidget()
{
	if (StackViewModel == nullptr)
	{
		return SNullWidget::NullWidget;
	}
	return SNew(SHorizontalBox)

		// open parent button
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(0, 0, 1, 0)
		[
			SNew(SButton)
			.IsFocusable(false)
			.ToolTipText(this, &SNiagaraOverviewStackNode::OpenParentEmitterTooltip)
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.ContentPadding(2)
			.OnClicked(this, &SNiagaraOverviewStackNode::OpenParentEmitter)
			.Visibility(this, &SNiagaraOverviewStackNode::GetOpenParentEmitterVisibility)
			.DesiredSizeScale(FVector2D(14.0f / 30.0f, 14.0f / 30.0f)) // GoToSourceIcon is 30x30, scale down
			.Content()
			[
				SNew(SImage)
				.Image(FNiagaraEditorWidgetsStyle::Get().GetBrush("NiagaraEditor.Stack.GoToSourceIcon"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		]
	
		// version selector
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Right)
		.AutoWidth()
		.Padding(1, 0, 2, 0)
		[
			SNew(SComboButton)
			.HasDownArrow(false)
			.ToolTipText(LOCTEXT("ChangeEmitterVersionToolTip", "Change the parent emitter version"))
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.ForegroundColor(FSlateColor::UseForeground())
			.OnGetMenuContent(this, &SNiagaraOverviewStackNode::GetVersionSelectorDropdownMenu)
			.ContentPadding(FMargin(2))
			.Visibility(this, &SNiagaraOverviewStackNode::GetVersionSelectorVisibility)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Font(FAppStyle::Get().GetFontStyle("FontAwesome.10"))
				.ColorAndOpacity(this, &SNiagaraOverviewStackNode::GetVersionSelectorColor)
				.Text(FEditorFontGlyphs::Random)
			]
		]

		// scalability indicator
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(16.f)
			.HeightOverride(16.f)
			[
				SNew(SImage)
				.Image(FNiagaraEditorStyle::Get().GetBrush("NiagaraEditor.Scalability"))
				.Visibility(this, &SNiagaraOverviewStackNode::GetScalabilityIndicatorVisibility)
				.ToolTipText(FText::FormatOrdered(LOCTEXT("ScalabilityIndicatorToolTip",
					"This {0} has scalability set up. Inspecting and editing scalability is accessible by using scalability mode from the toolbar."), EmitterHandleViewModelWeak.IsValid() ? FText::FromString("emitter") : FText::FromString("system")))
			]
		]
	
		// issue/error icon
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(0, 0, 1, 0)
		[
			SNew(SNiagaraStackIssueIcon, StackViewModel, StackViewModel->GetRootEntry())
			.Visibility(this, &SNiagaraOverviewStackNode::GetIssueIconVisibility)
			.OnClicked(this, &SNiagaraOverviewStackNode::OnCycleThroughIssues)
		];
}

FReply SNiagaraOverviewStackNode::OnCycleThroughIssues()
{
	const TArray<UNiagaraStackEntry*>& ChildrenWithIssues = StackViewModel->GetRootEntry()->GetAllChildrenWithIssues();
	if (ChildrenWithIssues.Num() > 0)
	{
		++CurrentIssueIndex;

		if (CurrentIssueIndex >= ChildrenWithIssues.Num())
		{
			CurrentIssueIndex = 0;
		}

		if (ChildrenWithIssues.IsValidIndex(CurrentIssueIndex))
		{
			UNiagaraStackEntry* ChildIssue = ChildrenWithIssues[CurrentIssueIndex];
			UNiagaraStackEntry* ChildToSelect = Cast<UNiagaraStackModuleItem>(ChildIssue);
			if (ChildToSelect == nullptr)
			{
				ChildToSelect = ChildIssue->GetTypedOuter<UNiagaraStackModuleItem>();
			}

			if (ChildToSelect == nullptr)
			{
				ChildToSelect = Cast<UNiagaraStackItemGroup>(ChildIssue);
			}
			
			if (ChildToSelect != nullptr)
			{
				OverviewSelectionViewModel->UpdateSelectedEntries(TArray<UNiagaraStackEntry*> { ChildToSelect }, TArray<UNiagaraStackEntry*>(), true /* bClearCurrentSelection */ );
			}
		}
	}

	return FReply::Handled();
}

TSharedRef<SWidget> SNiagaraOverviewStackNode::CreateThumbnailWidget(UNiagaraStackEntry* InData, TSharedPtr<SWidget> InWidget, TSharedPtr<SWidget> InTooltipWidget)
{
	TSharedPtr<SToolTip> ThumbnailTooltipWidget;
	// If this is just text, don't constrain the size
	if (InTooltipWidget->GetType() == TEXT("STextBlock"))
	{
		ThumbnailTooltipWidget = SNew(SToolTip)
			.Content()
			[
				InTooltipWidget.ToSharedRef()
			];
	}
	else
	{

		ThumbnailTooltipWidget = SNew(SToolTip)
			.Content()
			[
				SNew(SBox)
				.MaxDesiredHeight(64.0f)
				.MinDesiredHeight(64.0f)
				.MaxDesiredWidth(64.0f)
				.MinDesiredWidth(64.0f)
				[
					InTooltipWidget.ToSharedRef()
				]
			];
	}
	InWidget->SetOnMouseButtonDown(FPointerEventHandler::CreateSP(this, &SNiagaraOverviewStackNode::OnClickedRenderingPreview, InData));
	InWidget->SetOnMouseEnter(FNoReplyPointerEventHandler::CreateSP(this, &SNiagaraOverviewStackNode::SetIsHoveringThumbnail, true));
	InWidget->SetOnMouseLeave(FSimpleNoReplyPointerEventHandler::CreateSP(this, &SNiagaraOverviewStackNode::SetIsHoveringThumbnail, false));
	InWidget->SetToolTip(ThumbnailTooltipWidget);

	return InWidget.ToSharedRef();
}

FReply SNiagaraOverviewStackNode::OnClickedRenderingPreview(const FGeometry& InGeometry, const FPointerEvent& InEvent, UNiagaraStackEntry* InEntry)
{
	if (InEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		TArray<UNiagaraStackEntry*> SelectedEntries;
		SelectedEntries.Add(InEntry);
		TArray<UNiagaraStackEntry*> DeselectedEntries;
		OverviewSelectionViewModel->UpdateSelectedEntries(SelectedEntries, DeselectedEntries, true);

		CurrentIssueIndex = -1;
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SNiagaraOverviewStackNode::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (OverviewStackNode != nullptr)
	{
		if (OverviewStackNode->IsRenamePending() && !SGraphNode::IsRenamePending())
		{
			SGraphNode::RequestRename();
			OverviewStackNode->RenameStarted();
		}

		if (bTopContentBarRefreshPending)
		{
			FillTopContentBar();
			bTopContentBarRefreshPending = false;
		}

		if (ScalabilityWrapper != nullptr && !UseLowDetailNodeContent())
		{
			LastHighDetailSize = ScalabilityWrapper->GetTickSpaceGeometry().Size;
			if (GeometryTickForSize > 0)
			{
				// the stack needs a few tick to fully initialize, so we wait to bit to grab the right low detail geomtry size
				GeometryTickForSize--;
			}
		}
	}
}

void SNiagaraOverviewStackNode::OnMaterialCompiled(class UMaterialInterface* MaterialInterface)
{
	if (EmitterHandleViewModelWeak.IsValid())
	{
		bool bUsingThisMaterial = false;
		EmitterHandleViewModelWeak.Pin()->GetRendererEntries(PreviewStackEntries);
		FNiagaraEmitterInstance* InInstance = EmitterHandleViewModelWeak.Pin()->GetEmitterViewModel()->GetSimulation().IsValid() ? EmitterHandleViewModelWeak.Pin()->GetEmitterViewModel()->GetSimulation().Pin().Get() : nullptr;
		for (UNiagaraStackEntry* Entry : PreviewStackEntries)
		{
			if (UNiagaraStackRendererItem* RendererItem = Cast<UNiagaraStackRendererItem>(Entry))
			{
				TArray<UMaterialInterface*> Materials;
				RendererItem->GetRendererProperties()->GetUsedMaterials(InInstance, Materials);
				if (Materials.Contains(MaterialInterface))
				{
					bUsingThisMaterial = true;
					break;
				}
			}
		}

		if (bUsingThisMaterial)
		{
			bTopContentBarRefreshPending = true;
		}
	}
}

void SNiagaraOverviewStackNode::CreateBottomSummaryExpander()
{
	UNiagaraEmitter* Emitter = EmitterHandleViewModelWeak.IsValid()? EmitterHandleViewModelWeak.Pin()->GetEmitterViewModel()->GetEmitter().Emitter : nullptr;
	if (BottomSummaryExpander.IsValid() || !Emitter)
	{
		return;
	}	
	
	SAssignNew(BottomSummaryExpander, SBorder)
	.BorderImage(FNiagaraEditorWidgetsStyle::Get().GetBrush("NiagaraEditor.SystemOverview.NodeBackgroundBorder"))
	.BorderBackgroundColor(FNiagaraEditorWidgetsStyle::Get().GetColor("NiagaraEditor.SystemOverview.NodeBackgroundColor"))
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	[
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "NoBorder")
		.HAlign(HAlign_Center)
		.ContentPadding(2)
		.ToolTipText(this, &SNiagaraOverviewStackNode::GetSummaryViewCollapseTooltipText)
		.OnClicked(this, &SNiagaraOverviewStackNode::ExpandSummaryViewClicked)
		.IsFocusable(false)
		.Content()
		[
			// add the dropdown button for advanced properties 
			SNew(SImage)
				.Image(this, &SNiagaraOverviewStackNode::GetSummaryViewButtonBrush)
		]		
	];
}

EVisibility SNiagaraOverviewStackNode::GetScalabilityIndicatorVisibility() const
{
	TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel = EmitterHandleViewModelWeak.Pin();

	if(EmitterHandleViewModel.IsValid())
	{
		if(FVersionedNiagaraEmitterData* EmitterData = EmitterHandleViewModel->GetEmitterHandle()->GetInstance().GetEmitterData())
		{
			bool bIsQualityLevelMaskSetup = EmitterData->Platforms.QualityLevelMask != INDEX_NONE;
			bool bIsScalabilitySetup = EmitterData->ScalabilityOverrides.Overrides.Num() != 0 || (bIsQualityLevelMaskSetup && EmitterData->Platforms.QualityLevelMask != FNiagaraPlatformSet::GetFullQualityLevelMask(GetDefault<UNiagaraSettings>()->QualityLevels.Num())); 
			return bIsScalabilitySetup ? EVisibility::Visible : EVisibility::Collapsed;
		}
	}

	if(UNiagaraSystem* System = OverviewStackNode->GetOwningSystem())
	{
		bool bIsScalabilitySetup = System->GetOverrideScalabilitySettings();
		return bIsScalabilitySetup ? EVisibility::Visible : EVisibility::Collapsed; 
	}
	
	return EVisibility::Collapsed;
}

TSharedRef<SWidget> SNiagaraOverviewStackNode::CreateNodeContentArea()
{
	TSharedPtr<SWidget> ContentWidget;
	if (StackViewModel != nullptr && OverviewSelectionViewModel != nullptr)
	{
		ContentWidget = SNew(SBox)
			.MaxDesiredWidth(300)
			[
				SNew(SNiagaraOverviewStack, *StackViewModel, *OverviewSelectionViewModel)
			];
	}
	else
	{
		ContentWidget = SNullWidget::NullWidget;
	}

	FillTopContentBar();

	TSharedPtr<SVerticalBox> NodeBox;
	
	// NODE CONTENT AREA
	TSharedRef<SWidget> NodeWidget = SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("NoBorder"))
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Padding(FMargin(2, 2, 2, 4))
		[
			SAssignNew(NodeBox, SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.Padding(2.0f, 2.0f)
			[
				TopContentBar.ToSharedRef()
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.Padding(0.0f)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				[
					// LEFT
					SAssignNew(LeftNodeBox, SVerticalBox)
				]
				+SHorizontalBox::Slot()
				[
					SNew(SBorder)
					.BorderImage(FNiagaraEditorWidgetsStyle::Get().GetBrush("NiagaraEditor.SystemOverview.NodeBackgroundBorder"))
					.BorderBackgroundColor(FStyleColors::Panel)
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					.Padding(FMargin(0, 0, 0, 4))
					[
						ContentWidget.ToSharedRef()
					]
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				[
					// RIGHT
					SAssignNew(RightNodeBox, SVerticalBox)
				]
			]
		];

	CreateBottomSummaryExpander();

	if (BottomSummaryExpander.IsValid())
	{
		NodeBox->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.Padding(2.0f, 2.0f)
		[
			BottomSummaryExpander.ToSharedRef()
		];
	}

	TSharedRef<SWidget> DetailedContent = SNew(SLevelOfDetailBranchNode)
		.UseLowDetailSlot(this, &SNiagaraOverviewStackNode::UseLowDetailNodeContent)
		.LowDetail()
		[
			SNew(SBox)
			.WidthOverride(this, &SNiagaraOverviewStackNode::GetLowDetailDesiredWidth)
			.HeightOverride(this, &SNiagaraOverviewStackNode::GetLowDetailDesiredHeight)
			[
				SNew(STextBlock)
				.Text(this, &SNiagaraOverviewStackNode::GetLowDetailNodeTitle)
				.TextStyle(&FNiagaraEditorWidgetsStyle::Get().GetWidgetStyle<FTextBlockStyle>("NiagaraEditor.SystemOverview.ZoomedOutNodeFont"))
				.Visibility(EVisibility::HitTestInvisible)
				.Clipping(EWidgetClipping::Inherit)
			]
		]
		.HighDetail()
		[
			NodeWidget
		];

	ScalabilityWrapper = SNew(SOverlay)
	+ SOverlay::Slot()
	[
		DetailedContent
	]
	+ SOverlay::Slot()
	.Padding(0, 1)
	[
		SNew(SBorder)
		.Visibility(this, &SNiagaraOverviewStackNode::ShowExcludedOverlay)
		.BorderImage(FNiagaraEditorStyle::Get().GetBrush("NiagaraEditor.SystemOverview.ExcludedFromScalability.NodeBody"))
		.BorderBackgroundColor(TAttribute<FSlateColor>(this, &SNiagaraOverviewStackNode::GetScalabilityTintAlpha))
	];

	return ScalabilityWrapper.ToSharedRef();

}

bool SNiagaraOverviewStackNode::UseLowDetailNodeContent() const
{
	const UNiagaraEditorSettings* NiagaraSettings = GetDefault<UNiagaraEditorSettings>();
	if (LastHighDetailSize.IsNearlyZero() || GeometryTickForSize > 0 || !NiagaraSettings->bSimplifyStackNodesAtLowResolution)
	{
		return false;
	}
	
	if (const SGraphPanel* MyOwnerPanel = GetOwnerPanel().Get())
	{
		return (MyOwnerPanel->GetCurrentLOD() <= EGraphRenderingLOD::LowDetail);
	}
	return false;
}

FVector2D SNiagaraOverviewStackNode::GetLowDetailDesiredSize() const
{
	return LastHighDetailSize;
}

FOptionalSize SNiagaraOverviewStackNode::GetLowDetailDesiredWidth() const
{
	return LastHighDetailSize.X;
}

FOptionalSize SNiagaraOverviewStackNode::GetLowDetailDesiredHeight() const
{
	return LastHighDetailSize.Y;
}

namespace NiagaraOverviewStackNode
{
	struct StringRange
	{
		int32 Start = 0;
		int32 End = 0;
	};
	
	bool IsUpperAlphaNumeric(const TCHAR& c)
	{
		return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
	}

	// this method splits long emitter names for the low details view by inserting new lines
	// and abbreviating large pieces when necessary
	FString PrettySplitString(const FString& Input)
	{
		TArray<FStringView, TInlineAllocator<16>> Parts;

		// split at convenient places
		int32 Start = 0;
		for (int i = 0; i < Input.Len() - 1; i++)
		{
			TCHAR currentChar = Input[i];
			TCHAR nextChar = Input[i + 1];

			bool bIsLowerAlpha = currentChar >= 'a' && currentChar <= 'z';

			// skip '_' at the beginning or in sequence
			if (currentChar == '_')
			{
				Start = i + 1;
			}
			// replace '_' in the middle of the name with a newline
			else if (nextChar == '_')
			{
				Parts.Add(FStringView(&Input[Start], i - Start + 1));
				i++;
				Start = i + 1;
			}
			// split when changing from lowercase to uppercase or number
			else if (bIsLowerAlpha && IsUpperAlphaNumeric(nextChar))
			{
				Parts.Add(FStringView(&Input[Start], i - Start + 1));
				Start = i + 1;
			}
		}
		// add the end piece
		if (Start < Input.Len())
		{
			Parts.Add(FStringView(&Input[Start], Input.Len() - Start));
		}

		// assemble the pieces
		const UNiagaraEditorSettings* NiagaraSettings = GetDefault<UNiagaraEditorSettings>();
		int32 MaxLength = FMath::Max(3, NiagaraSettings->LowResolutionNodeMaxNameChars);
		TStringBuilder<128> SplitNameBuilder;
		int32 RunningLength = 0;
		for (const FStringView& Split : Parts)
		{
			// merge small parts together, only insert a newline between big parts
			if (Split.Len() + RunningLength > MaxLength && SplitNameBuilder.Len() > 0)
			{
				SplitNameBuilder.AppendChar('\n');
				RunningLength = 0;
			}
			
			// if the name is too long, abbreviate with ...
			if (Split.Len() > MaxLength)
			{
				SplitNameBuilder.Append(Split.Left(MaxLength - 2));
				SplitNameBuilder.Append(TEXT("…"));
			}
			else
			{
				SplitNameBuilder.Append(Split);
			}
			RunningLength += Split.Len();
		}

		return SplitNameBuilder.ToString();
	}
}

FText SNiagaraOverviewStackNode::GetLowDetailNodeTitle() const
{
	if (FString Title = GetEditableNodeTitle(); LowDetailTitleCache.Key != Title)
	{
		LowDetailTitleCache.Key = Title;
		LowDetailTitleCache.Value = FText::FromString(NiagaraOverviewStackNode::PrettySplitString(Title));
	}
	return LowDetailTitleCache.Value;
}

void SNiagaraOverviewStackNode::StackViewModelStructureChanged(ENiagaraStructureChangedFlags Flags)
{
	bTopContentBarRefreshPending = true;
}

void SNiagaraOverviewStackNode::StackViewModelDataObjectChanged(TArray<UObject*> ChangedObjects, ENiagaraDataObjectChange ChangeType)
{
	for (UObject* ChangedObject : ChangedObjects)
	{
		if (ChangedObject->IsA<UNiagaraRendererProperties>())
		{
			bTopContentBarRefreshPending = true;
			break;
		}
	}
}

void SNiagaraOverviewStackNode::FillTopContentBar()
{
	if (TopContentBar.IsValid() && TopContentBar->GetChildren())
	{
		TopContentBar->ClearChildren();
	}
	if (EmitterHandleViewModelWeak.IsValid())
	{		
		// Isolate toggle button
		TopContentBar->AddSlot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(2, 0, 0, 0)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
				.HAlign(HAlign_Center)
				.ContentPadding(1)
				.ToolTipText(this, &SNiagaraOverviewStackNode::GetToggleIsolateToolTip)
				.OnClicked(this, &SNiagaraOverviewStackNode::OnToggleIsolateButtonClicked)
				.Visibility(this, &SNiagaraOverviewStackNode::GetToggleIsolateVisibility)
				.IsFocusable(false)
				.Content()
				[
					SNew(SImage)
					.Image(FNiagaraEditorStyle::Get().GetBrush("NiagaraEditor.Isolate"))
					.ColorAndOpacity(this, &SNiagaraOverviewStackNode::GetToggleIsolateImageColor)
				]
			];

		EmitterHandleViewModelWeak.Pin()->GetRendererEntries(PreviewStackEntries);
		FNiagaraEmitterInstance* InInstance = EmitterHandleViewModelWeak.Pin()->GetEmitterViewModel()->GetSimulation().IsValid() ? EmitterHandleViewModelWeak.Pin()->GetEmitterViewModel()->GetSimulation().Pin().Get() : nullptr;

		FToolBarBuilder ToolBarBuilder(nullptr, FMultiBoxCustomization::None, nullptr, true);
		ToolBarBuilder.SetLabelVisibility(EVisibility::Collapsed);
		
		for (int32 StackEntryIndex = 0; StackEntryIndex < PreviewStackEntries.Num(); StackEntryIndex++)
		{
			UNiagaraStackEntry* Entry = PreviewStackEntries[StackEntryIndex];
			if (UNiagaraStackRendererItem* RendererItem = Cast<UNiagaraStackRendererItem>(Entry))
			{
				TArray<TSharedPtr<SWidget>> Widgets;
				RendererItem->GetRendererProperties()->GetRendererWidgets(InInstance, Widgets, UThumbnailManager::Get().GetSharedThumbnailPool());
				TArray<TSharedPtr<SWidget>> TooltipWidgets;
				RendererItem->GetRendererProperties()->GetRendererTooltipWidgets(InInstance, TooltipWidgets, UThumbnailManager::Get().GetSharedThumbnailPool());
				check(Widgets.Num() == TooltipWidgets.Num());
				for (int32 WidgetIndex = 0; WidgetIndex < Widgets.Num(); WidgetIndex++)
				{
					ToolBarBuilder.AddWidget(
						SNew(SBox)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						.MinDesiredHeight(ThumbnailSize)
						.MinDesiredWidth(ThumbnailSize)
						.MaxDesiredHeight(ThumbnailSize)
						.MaxDesiredWidth(ThumbnailSize)
						.Visibility(this, &SNiagaraOverviewStackNode::GetEnabledCheckBoxVisibility)
						[
							CreateThumbnailWidget(Entry, Widgets[WidgetIndex], TooltipWidgets[WidgetIndex])
						]
					);
				}

				// if we had a widget for this entry, add a separator for the next entry's widgets, except for the last entry
				if(Widgets.Num() > 0 && StackEntryIndex < PreviewStackEntries.Num() - 1)
				{
					ToolBarBuilder.AddSeparator();
				}
			}
		}

		TopContentBar->AddSlot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.MaxWidth(300.f)
		[
			ToolBarBuilder.MakeWidget()
		];
	}

	TopContentBar->AddSlot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.FillWidth(1.0f)
		[
			SNullWidget::NullWidget
		];
}

EVisibility SNiagaraOverviewStackNode::GetIssueIconVisibility() const
{
	return StackViewModel->HasIssues() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SNiagaraOverviewStackNode::GetEnabledCheckBoxVisibility() const
{
	return EmitterHandleViewModelWeak.IsValid() ? EVisibility::Visible : EVisibility::Collapsed;
}

ECheckBoxState SNiagaraOverviewStackNode::GetEnabledCheckState() const
{
	return EmitterHandleViewModelWeak.IsValid() && EmitterHandleViewModelWeak.Pin()->GetIsEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SNiagaraOverviewStackNode::OnEnabledCheckStateChanged(ECheckBoxState InCheckState)
{
	TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel = EmitterHandleViewModelWeak.Pin();
	if (EmitterHandleViewModel.IsValid())
	{
		EmitterHandleViewModel->SetIsEnabled(InCheckState == ECheckBoxState::Checked);
	}
}

FReply SNiagaraOverviewStackNode::OnToggleIsolateButtonClicked()
{
	TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel = EmitterHandleViewModelWeak.Pin();
	if (EmitterHandleViewModel.IsValid())
	{
		bool bShouldBeIsolated = !EmitterHandleViewModel->GetIsIsolated();
		EmitterHandleViewModel->SetIsIsolated(bShouldBeIsolated);
	}
	return FReply::Handled();
}

FText SNiagaraOverviewStackNode::GetToggleIsolateToolTip() const
{
	TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel = EmitterHandleViewModelWeak.Pin();
	return EmitterHandleViewModel.IsValid() && EmitterHandleViewModel->GetIsIsolated()
		? LOCTEXT("TurnOffEmitterIsolation", "Disable emitter isolation.")
		: LOCTEXT("IsolateThisEmitter", "Enable isolation for this emitter.");
}

EVisibility SNiagaraOverviewStackNode::GetToggleIsolateVisibility() const
{
	TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel = EmitterHandleViewModelWeak.Pin();
	return EmitterHandleViewModel.IsValid() &&
		EmitterHandleViewModel->GetOwningSystemEditMode() == ENiagaraSystemViewModelEditMode::SystemAsset 
		? EVisibility::Visible 
		: EVisibility::Collapsed;
}

FSlateColor SNiagaraOverviewStackNode::GetToggleIsolateImageColor() const
{
	TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel = EmitterHandleViewModelWeak.Pin();
	return EmitterHandleViewModel.IsValid() && 
		EmitterHandleViewModel->GetIsIsolated()
		? FAppStyle::GetSlateColor("SelectionColor")
		: FLinearColor::Gray;
}

FSlateColor SNiagaraOverviewStackNode::GetScalabilityTintAlpha() const
{
	if(EmitterHandleViewModelWeak.IsValid())
	{
		float ScalabilityBaseAlpha = FNiagaraEditorUtilities::GetScalabilityTintAlpha(EmitterHandleViewModelWeak.Pin()->GetEmitterHandle());
		return FLinearColor(1, 1, 1, ScalabilityBaseAlpha * GetGraphZoomDistanceAlphaMultiplier());
	}

	return FLinearColor(1, 1, 1, 1);
}

void SNiagaraOverviewStackNode::OnScalabilityModeChanged(bool bActive)
{
	bScalabilityModeActive = bActive;
}

EVisibility SNiagaraOverviewStackNode::ShowExcludedOverlay() const
{
	// we only want actual results in scalability mode and for nodes representing emitters (not system nodes)
	if(bScalabilityModeActive && EmitterHandleViewModelWeak.IsValid())
	{		
		return EmitterHandleViewModelWeak.Pin()->GetEmitterHandle()->GetEmitterData()->IsAllowedByScalability() ? EVisibility::Hidden : EVisibility::HitTestInvisible;
	}
	
	return EVisibility::Hidden; 
}

float SNiagaraOverviewStackNode::GetGraphZoomDistanceAlphaMultiplier() const
{
	// we lower the alpha if the zoom amount is high
	float ZoomAmount = OwnerGraphPanelPtr.Pin()->GetZoomAmount();
	return FMath::Lerp(0.4f, 1.f, 1 - ZoomAmount);
}

FReply SNiagaraOverviewStackNode::OpenParentEmitter()
{
	TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel = EmitterHandleViewModelWeak.Pin();
	if (EmitterHandleViewModel.IsValid())
	{
		FNiagaraEditorUtilities::OpenParentEmitterForEdit(EmitterHandleViewModel->GetEmitterViewModel());
	}
	return FReply::Handled();
}

FText SNiagaraOverviewStackNode::OpenParentEmitterTooltip() const
{
	TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel = EmitterHandleViewModelWeak.Pin();
	if (EmitterHandleViewModel.IsValid() && EmitterHandleViewModel->GetEmitterViewModel()->HasParentEmitter() && EmitterHandleViewModel->GetEmitterViewModel()->GetParentEmitter().Emitter->IsVersioningEnabled())
	{
		FVersionedNiagaraEmitter ParentEmitter = EmitterHandleViewModel->GetEmitterViewModel()->GetParentEmitter();
		FText ParentName = FText::FromString(ParentEmitter.Emitter->GetUniqueEmitterName());
		if (FVersionedNiagaraEmitterData* EmitterData = ParentEmitter.GetEmitterData())
		{
			return FText::Format(LOCTEXT("OpenAndFocusVersionedParentToolTip", "Open and Focus Parent Emitter:\n{0} - v{1}.{2}"), ParentName, EmitterData->Version.MajorVersion, EmitterData->Version.MinorVersion);
		}
	}
	return LOCTEXT("OpenAndFocusParentEmitterToolTip", "Open and Focus Parent Emitter");
}

EVisibility SNiagaraOverviewStackNode::GetOpenParentEmitterVisibility() const
{
	TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel = EmitterHandleViewModelWeak.Pin();
	return EmitterHandleViewModel.IsValid() && 
		EmitterHandleViewModel->GetEmitterViewModel()->HasParentEmitter()
		? EVisibility::Visible 
		: EVisibility::Collapsed;
}

EVisibility SNiagaraOverviewStackNode::GetVersionSelectorVisibility() const
{
	TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel = EmitterHandleViewModelWeak.Pin();
	return EmitterHandleViewModel.IsValid() && 
		EmitterHandleViewModel->GetEmitterViewModel()->HasParentEmitter() &&
		EmitterHandleViewModel->GetEmitterViewModel()->GetParentEmitter().Emitter->IsVersioningEnabled()
		? EVisibility::Visible 
		: EVisibility::Collapsed;
}

FSlateColor SNiagaraOverviewStackNode::GetVersionSelectorColor() const
{
	if (TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel = EmitterHandleViewModelWeak.Pin())
	{
		FVersionedNiagaraEmitter ParentEmitter = EmitterHandleViewModel->GetEmitterViewModel()->GetParentEmitter();
		if (ParentEmitter.Emitter && ParentEmitter.Emitter->IsVersioningEnabled())
		{
			FNiagaraAssetVersion ExposedVersion = ParentEmitter.Emitter->GetExposedVersion();
			if (ParentEmitter.GetEmitterData() == nullptr || ParentEmitter.GetEmitterData()->Version < ExposedVersion)
			{
				return FNiagaraEditorWidgetsStyle::Get().GetColor("NiagaraEditor.Stack.IconColor.VersionUpgrade");
			}
		}
	}
	return FNiagaraEditorWidgetsStyle::Get().GetColor("NiagaraEditor.Stack.FlatButtonColor");
}

TSharedRef<SWidget> SNiagaraOverviewStackNode::GetVersionSelectorDropdownMenu()
{
	constexpr bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr);

	if (TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel = EmitterHandleViewModelWeak.Pin())
	{
		FVersionedNiagaraEmitter ParentEmitter = EmitterHandleViewModel->GetEmitterViewModel()->GetParentEmitter();
		TArray<FNiagaraAssetVersion> AssetVersions = ParentEmitter.Emitter->GetAllAvailableVersions();
		for (FNiagaraAssetVersion& Version : AssetVersions)
		{
			if (!Version.bIsVisibleInVersionSelector)
			{
				continue;
			}
			FVersionedNiagaraEmitterData* EmitterData = ParentEmitter.Emitter->GetEmitterData(Version.VersionGuid);
			bool bIsSelected = ParentEmitter.Version == Version.VersionGuid;
		
			FText Tooltip = LOCTEXT("NiagaraSelectVersion_Tooltip", "Select this version to use for the emitter");
			if (EmitterData && !EmitterData->VersionChangeDescription.IsEmpty())
			{
				Tooltip = FText::Format(LOCTEXT("NiagaraSelectVersionChangelist_Tooltip", "Select this version to use for the emitter. Change description for this version:\n{0}"), EmitterData->VersionChangeDescription);
			}
		
			FUIAction UIAction(FExecuteAction::CreateSP(this, &SNiagaraOverviewStackNode::SwitchToVersion, Version),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([bIsSelected]() { return bIsSelected; }));
			FText Format = (Version == ParentEmitter.Emitter->GetExposedVersion()) ? FText::FromString("{0}.{1}*") : FText::FromString("{0}.{1}");
			FText Label = FText::Format(Format, Version.MajorVersion, Version.MinorVersion);
			MenuBuilder.AddMenuEntry(Label, Tooltip, FSlateIcon(), UIAction, NAME_None, EUserInterfaceActionType::RadioButton);	
		}
	}

	return MenuBuilder.MakeWidget();
}

// this switches the referenced parent version, for the version selector in the toolbar see FNiagaraSystemViewModel::ChangeEmitterVersion
void SNiagaraOverviewStackNode::SwitchToVersion(FNiagaraAssetVersion Version)
{
	if (TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel = EmitterHandleViewModelWeak.Pin())
	{
		FNiagaraEditorUtilities::SwitchParentEmitterVersion(EmitterHandleViewModel->GetEmitterViewModel(), EmitterHandleViewModel->GetOwningSystemViewModel(), Version.VersionGuid);
	}
}

const FSlateBrush* SNiagaraOverviewStackNode::GetSummaryViewButtonBrush() const
{
	const UNiagaraEmitterEditorData* EditorData = EmitterHandleViewModelWeak.IsValid()? &EmitterHandleViewModelWeak.Pin()->GetEmitterViewModel()->GetEditorData() : nullptr;
	if (BottomSummaryExpander->IsHovered())
	{
		return EditorData && EditorData->ShouldShowSummaryView()
			? FAppStyle::GetBrush("DetailsView.PulldownArrow.Down.Hovered")
			: FAppStyle::GetBrush("DetailsView.PulldownArrow.Up.Hovered");
	}
	else
	{
		return EditorData && EditorData->ShouldShowSummaryView()
			? FAppStyle::GetBrush("DetailsView.PulldownArrow.Down")
			: FAppStyle::GetBrush("DetailsView.PulldownArrow.Up");
	}	
}

FText SNiagaraOverviewStackNode::GetSummaryViewCollapseTooltipText() const
{
	const UNiagaraEmitterEditorData* EditorData = EmitterHandleViewModelWeak.IsValid()? &EmitterHandleViewModelWeak.Pin()->GetEmitterViewModel()->GetEditorData() : nullptr;
	return EditorData && EditorData->ShouldShowSummaryView()? LOCTEXT("HideAdvancedToolTip", "Show Full Emitter") : LOCTEXT("ShowAdvancedToolTip", "Show Emitter Summary");
}

FReply SNiagaraOverviewStackNode::ExpandSummaryViewClicked()
{
	if (UNiagaraEmitterEditorData* EditorData = EmitterHandleViewModelWeak.IsValid()? &EmitterHandleViewModelWeak.Pin()->GetEmitterViewModel()->GetOrCreateEditorData() : nullptr)
	{
		EditorData->ToggleShowSummaryView();
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
