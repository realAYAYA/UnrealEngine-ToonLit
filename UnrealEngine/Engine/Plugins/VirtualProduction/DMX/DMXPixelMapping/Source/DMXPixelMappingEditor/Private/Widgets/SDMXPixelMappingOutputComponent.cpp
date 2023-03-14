// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SDMXPixelMappingOutputComponent.h"

#include "DMXPixelMappingLayoutSettings.h"
#include "DMXPixelMappingTypes.h"
#include "DMXPixelMappingUtils.h"
#include "DMXRuntimeUtils.h"
#include "Components/DMXPixelMappingOutputComponent.h"
#include "Components/DMXPixelMappingScreenComponent.h"
#include "ViewModels/DMXPixelMappingOutputComponentModel.h"
#include "Toolkits/DMXPixelMappingToolkit.h"

#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "TimerManager.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"


#define LOCTEXT_NAMESPACE "SDMXPixelMappingOutputComponent"

IDMXPixelMappingOutputComponentWidgetInterface::~IDMXPixelMappingOutputComponentWidgetInterface()
{
	RemoveFromCanvas();
}

void IDMXPixelMappingOutputComponentWidgetInterface::AddToCanvas(const TSharedRef<SConstraintCanvas>& InCanvas)
{
	RemoveFromCanvas();

	ParentCanvas = InCanvas;

	ParentCanvas->AddSlot()
		.ZOrder(100)
		.AutoSize(true)
		.Alignment(FVector2D::ZeroVector)
		.Offset_Lambda([this]()
			{
				const FVector2D Postition = GetPosition();

				// In the center of the top left pixel
				return FMargin(Postition.X + .5f, Postition.Y + .5f);
			})
		.Expose(Slot)
		[
			AsWidget()
		];
}

void IDMXPixelMappingOutputComponentWidgetInterface::RemoveFromCanvas()
{
	if (ParentCanvas.IsValid() && Slot)
	{
		ParentCanvas->RemoveSlot(Slot->GetWidget());
	}

	Slot = nullptr;
	ParentCanvas.Reset();
}

void SDMXPixelMappingOutputComponent::Construct(const FArguments& InArgs, const TSharedRef<FDMXPixelMappingToolkit>& InToolkit, TWeakObjectPtr<UDMXPixelMappingOutputComponent> OutputComponent)
{
	if (!OutputComponent.IsValid())
	{
		return;
	}

	WeakToolkit = InToolkit;
	Model = MakeShared<FDMXPixelMappingOutputComponentModel>(InToolkit, OutputComponent);

	BorderBrush.DrawAs = ESlateBrushDrawType::Border;
	BorderBrush.Margin = FMargin(1.f);

	ChildSlot
	[
		SAssignNew(ComponentBox, SBox)
		.WidthOverride_Lambda([this]()
			{
				return Model->GetSize().X;
			})
		.HeightOverride_Lambda([this]()
			{
				return Model->GetSize().Y;
			})
		[
			SNew(SBorder)
			.BorderImage_Lambda([this]()
				{
					BorderBrush.TintColor = Model->GetColor();
					return &BorderBrush;
				})
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Bottom)
				.AutoHeight()
				.Padding(FMargin(0.f, -22.f, 0.f, 0.f))
				[
					SNew(SBox)
					.HeightOverride(22.f)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Bottom)
					[
						SAssignNew(AboveContentBorder, SBorder)
						.BorderImage(FAppStyle::GetBrush("NoBorder"))
					]
				]

				+ SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.FillHeight(1.f / 3.f)
				.Padding(FMargin(-2.f, -2.f, -2.f, -2.f))
				[
					SAssignNew(TopContentBorder, SBorder)
					.BorderImage(FAppStyle::GetBrush("NoBorder"))
				]

				+ SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.FillHeight(1.f / 3.f)
				.Padding(FMargin(-2.f, -2.f, -2.f, -2.f))
				[
					SAssignNew(MiddleContentBorder, SBorder)
					.BorderImage(FAppStyle::GetBrush("NoBorder"))
				]

				+ SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.FillHeight(1.f / 3.f)
				.Padding(FMargin(-2.f, -2.f, -2.f, -2.f))
				[
					SAssignNew(BottomContentBorder, SBorder)
					.BorderImage(FAppStyle::GetBrush("NoBorder"))
				]
			]
		]
	];

	UpdateChildSlots();
	UDMXPixelMappingLayoutSettings::GetOnLayoutSettingsChanged().AddSP(this, &SDMXPixelMappingOutputComponent::UpdateChildSlots);
}

bool SDMXPixelMappingOutputComponent::Equals(UDMXPixelMappingBaseComponent* Component) const
{
	if (Component)
	{
		return Model->Equals(Component);
	}
	return false;
}

FVector2D SDMXPixelMappingOutputComponent::GetPosition() const
{
	return Model->GetPosition();
}

void SDMXPixelMappingOutputComponent::UpdateChildSlots()
{
	AboveContentBorder->ClearContent();
	TopContentBorder->ClearContent();
	MiddleContentBorder->ClearContent();
	BottomContentBorder->ClearContent();

	const UDMXPixelMappingLayoutSettings* LayoutSettings = GetDefault<UDMXPixelMappingLayoutSettings>();
	if (!LayoutSettings)
	{
		return;
	}

	if (LayoutSettings->bShowComponentNames && Model->ShouldDrawName())
	{
		if (Model->ShouldDrawNameAbove())
		{
			CreateComponentNameChildSlotAbove();
		}
		else
		{
			CreateComponentNameChildSlotInside();
		}
	}

	if (LayoutSettings->bShowCellIDs && Model->HasCellID())
	{
		CreateCellIDChildSlot();
	}

	if (LayoutSettings->bShowPatchInfo && Model->HasPatchInfo())
	{
		CreatePatchInfoChildSlot();
	}
}

void SDMXPixelMappingOutputComponent::CreateComponentNameChildSlotAbove()
{
	AboveContentBorder->SetContent(
		SNew(SBox)
		.HeightOverride(20.f)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Bottom)
		[
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFit)
			.StretchDirection(EStretchDirection::DownOnly)
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 18))
				.Text_Lambda([this]()
					{
						return Model->GetName();
					})
			]	
		]

	);
}

void SDMXPixelMappingOutputComponent::CreateComponentNameChildSlotInside()
{
	TopContentBorder->SetContent(
		SNew(SBox)
		.HeightOverride(14.f)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		[
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFit)
			.StretchDirection(EStretchDirection::DownOnly)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Top)
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
				.Text_Lambda([this]()
					{
						return Model->GetName();
					})
			]
		]
	);
}

void SDMXPixelMappingOutputComponent::CreateCellIDChildSlot()
{	
	MiddleContentBorder->SetContent(
		SNew(SBox)
		.HeightOverride(14.f)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFit)
			.StretchDirection(EStretchDirection::DownOnly)
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
				.Text_Lambda([this]()
					{
						return Model->GetCellIDText();
					})
			]
		]
	);
}

void SDMXPixelMappingOutputComponent::CreatePatchInfoChildSlot()
{
	BottomContentBorder->SetContent(
		SNew(SBox)
		.HeightOverride(8.f)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Bottom)
		[
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFit)
			.StretchDirection(EStretchDirection::DownOnly)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Bottom)
				[
					SNew(STextBlock)
					.ColorAndOpacity(FLinearColor::Green)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 6))
					.Text_Lambda([this]()
						{
							return Model->GetAddressesText();
						})
				]

				+ SVerticalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Bottom)
				[
					SNew(STextBlock)
					.ColorAndOpacity(FLinearColor::Green)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 6))
					.Text_Lambda([this]()
						{
							return Model->GetFixtureIDText();
						})
				]
			]
		]
	);
}


class SDMXPixelMappingScreenComponentCell
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMXPixelMappingScreenComponentCell)
		: _ChannelOffset(0)
		, _StartingChannel(1)
		, _Universe(1)
	{}
		/** The offset of this cell from the starting address */
		SLATE_ARGUMENT(int32, ChannelOffset)

		/** The starting address of the screen component */
		SLATE_ATTRIBUTE(int32, StartingChannel)

		/** The universe of the screen component */
		SLATE_ATTRIBUTE(int32, Universe)

	SLATE_END_ARGS()

	/** Constructs this widget */
	void Construct(const FArguments& InArgs, const TSharedRef<FDMXPixelMappingScreenComponentModel>& InModel)
	{
		Model = InModel;

		ChannelOffset = InArgs._ChannelOffset;
		Universe = InArgs._Universe;
		StartingChannel = InArgs._StartingChannel;

		BorderBrush.DrawAs = ESlateBrushDrawType::Border;
		BorderBrush.Margin = FMargin(1.f);
		BorderBrush.TintColor = FLinearColor::White.CopyWithNewOpacity(.4f);

		ChildSlot
			[
				SAssignNew(ContentBorder, SBorder)
				.BorderImage(&BorderBrush)
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
			];

		RebuildAddressesContent();

		UDMXPixelMappingLayoutSettings::GetOnLayoutSettingsChanged().AddSP(this, &SDMXPixelMappingScreenComponentCell::RebuildAddressesContent);
	}

private:
	/** Rebuilds addresses content */
	void RebuildAddressesContent()
	{
		const UDMXPixelMappingLayoutSettings* LayoutSettings = GetDefault<UDMXPixelMappingLayoutSettings>();
		if (!LayoutSettings || !LayoutSettings->bShowPatchInfo)
		{
			return;
		}

		if (!ensureMsgf(StartingChannel.IsSet() && Universe.IsSet(), TEXT("Trying to display universe and address of pixel mapping screen component, but universe and/or address are not set.")))
		{
			return;
		}

		ContentBorder->SetContent
		(
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("NoBorder"))
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(6.f)
			[
				SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFit)
				.StretchDirection(EStretchDirection::DownOnly)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
					.Text_Lambda([this]()
						{
							const int32 UniverseWithOffset = Universe.Get() + ChannelOffset / DMX_MAX_ADDRESS;
							const int32 ChannelWithOffset = [this, UniverseWithOffset]()
							{
								if (UniverseWithOffset > 0)
								{
									return (StartingChannel.Get() + ChannelOffset) % DMX_MAX_ADDRESS;
								}
								return StartingChannel.Get() + ChannelOffset;
							}();

							if (Model->ComponentWantsToShowChannel() && Model->ComponentWantsToShowUniverse())
							{
								FString AddressString = FString::FromInt(UniverseWithOffset) + TEXT(".") + FString::FromInt(ChannelWithOffset);
								return FText::FromString(AddressString);
							}
							else if (Model->ComponentWantsToShowUniverse())
							{
								FString UniverseString = FString::FromInt(UniverseWithOffset);
								return FText::FromString(UniverseString);
							}
							else if (Model->ComponentWantsToShowChannel())
							{
								FString ChannelString = FString::FromInt(ChannelWithOffset);
								return FText::FromString(ChannelString);
							}
							return FText::GetEmpty();
						})
				]
			]
		);
	}

	/** Border to hold contents of the widget */
	TSharedPtr<SBorder> ContentBorder;

	/** The brush used for the border */
	FSlateBrush BorderBrush;

	/** The model used by this widget */
	TSharedPtr<FDMXPixelMappingScreenComponentModel> Model;

	// Slate args
	int32 ChannelOffset = 0;
	TAttribute<int32> Universe;
	TAttribute<int32> StartingChannel;
};

/** Widget to draw the inners of a screen component */
class SDMXPixelMappingScreenComponentGrid
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMXPixelMappingScreenComponentGrid)
	{}
	SLATE_END_ARGS()

	/** Constructs this widget */
	void Construct(const FArguments& InArgs, const TSharedRef<FDMXPixelMappingScreenComponentModel>& InModel)
	{
		Model = InModel;

		ChildSlot
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[
				SAssignNew(GridPanel, SUniformGridPanel)
			];

		CachedCellFormat = Model->GetCellFormat();
		CachedDistribution = Model->GetDistribution();

		RebuildGridContent();
	}

protected:
	//~ Begin SWidget Interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		if ((GridPanel->GetChildren()->Num() != Model->GetNumColumns() * Model->GetNumRows()) ||
			CachedDistribution != Model->GetDistribution() ||
			CachedCellFormat != Model->GetCellFormat())
		{
			// Handle cell format changes
			CachedDistribution = Model->GetDistribution();
			CachedCellFormat = Model->GetCellFormat();

			RebuildGridContent();
		}
	}
	//~ End SWidget Interface

private:
	/** Rebuilds the grid content */
	void RebuildGridContent()
	{
		GridPanel->ClearChildren();

		TArray<int32> CellIndices;
		const int32 TotalCells = Model->GetNumColumns() * Model->GetNumRows();
		for (int32 CellID = 0; CellID < TotalCells; ++CellID)
		{
			CellIndices.Add(CellID);
		}

		TArray<int32> DistributionSortedCellIndices;
		FDMXRuntimeUtils::PixelMappingDistributionSort(Model->GetDistribution(), Model->GetNumColumns(), Model->GetNumRows(), CellIndices, DistributionSortedCellIndices);

		const uint32 NumChannelsPerCell = FDMXPixelMappingUtils::GetNumChannelsPerCell(Model->GetCellFormat());

		uint32 XYIndex = 0;
		for (int32 YIndex = 0; YIndex < Model->GetNumRows(); ++YIndex)
		{
			for (int32 XIndex = 0; XIndex < Model->GetNumColumns(); ++XIndex)
			{
				if (!ensureMsgf(DistributionSortedCellIndices.IsValidIndex(XYIndex), TEXT("Pixel mapping cell index out of range.")))
				{
					break;
				}

				const int32 ChannelOffset = DistributionSortedCellIndices[XYIndex] * NumChannelsPerCell;
				GridPanel->AddSlot(XIndex, YIndex)
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					[
						SNew(SDMXPixelMappingScreenComponentCell, Model.ToSharedRef())
						.ChannelOffset(ChannelOffset)
						.StartingChannel_Lambda([this]()
							{
								return Model->GetStartingChannel();
							})
						.Universe_Lambda([this]()
							{
								return Model->GetUniverse();
							})
					];

				XYIndex++;
			}
		}
	}

	/** Cell format, cached to detect changes */
	EDMXCellFormat CachedCellFormat = EDMXCellFormat::PF_RGB;

	/** Pixelmapping distribution, cached to detect changes */
	EDMXPixelMappingDistribution CachedDistribution = EDMXPixelMappingDistribution::TopLeftToRight;

	/** The actual grid panel */
	TSharedPtr<SUniformGridPanel> GridPanel;

	/** The model used by this widget */
	TSharedPtr<FDMXPixelMappingScreenComponentModel> Model;
};

/** Simple view of a screen component, useful if many cells are drawn */
class SDMXPixelMappingScreenComponentSimplistic
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMXPixelMappingScreenComponentSimplistic)
	{}
	SLATE_END_ARGS()

	//~ Begin SCompoundWidget interface
	void Construct(const FArguments& InArgs, const TSharedRef<FDMXPixelMappingScreenComponentModel>& InModel)
	{
		ChildSlot
		[
			SNew(SBox)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFit)
				.StretchDirection(EStretchDirection::DownOnly)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 24))
						.ColorAndOpacity(FSlateColor::UseForeground())
						.Text_Lambda([InModel]()
							{
								return FText::Format(LOCTEXT("Num_Pixels", "{0} x {1} pixels"), InModel->GetNumColumns(), InModel->GetNumRows());
							})
					]
					+ SVerticalBox::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 24))
						.ColorAndOpacity(FSlateColor::UseForeground())
						.Text_Lambda([InModel]()
							{
								return FText::FromString(FString::Printf(TEXT("%i.%i"), InModel->GetUniverse(), InModel->GetStartingChannel()));
							})
					]
				]
			]
		];
	}
	//~ Begin SCompoundWidget interface

private:
	/** The model used by this widget */
	TSharedPtr<FDMXPixelMappingScreenComponentModel> Model;
};

void SDMXPixelMappingScreenComponent::Construct(const FArguments& InArgs, const TSharedRef<FDMXPixelMappingToolkit>& InToolkit, TWeakObjectPtr<UDMXPixelMappingScreenComponent> ScreenComponent)
{
	if (!ScreenComponent.IsValid())
	{
		return;
	}

	SetCanTick(true);

	WeakToolkit = InToolkit;
	Model = MakeShared<FDMXPixelMappingScreenComponentModel>(InToolkit, ScreenComponent);

	BorderBrush.DrawAs = ESlateBrushDrawType::Border;
	BorderBrush.Margin = FMargin(1.f);

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride_Lambda([this]()
			{
				return Model->GetSize().X;
			})
		.HeightOverride_Lambda([this]()
			{
				return Model->GetSize().Y;
			})
		[
			SNew(SOverlay)

			+ SOverlay::Slot() 
				[
					SAssignNew(ContentBorder, SBorder)
					.BorderImage(FAppStyle::GetBrush("NoBorder"))
					.Padding(0.f)
				]

			+ SOverlay::Slot() // Required to correctly draw the color around the grid
			[
				SNew(SBorder)
				.BorderImage_Lambda([this]()
					{
						BorderBrush.TintColor = Model->GetColor();
						return &BorderBrush;
					})
			]
		]
	];	

	UpdateContent();
}

bool SDMXPixelMappingScreenComponent::Equals(UDMXPixelMappingBaseComponent* Component) const
{
	if (Component)
	{
		return Model->Equals(Component);
	}
	return false;
}

FVector2D SDMXPixelMappingScreenComponent::GetPosition() const
{
	return Model->GetPosition();
}

void SDMXPixelMappingScreenComponent::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	const bool bShouldDrawSimplisticView = Model->GetNumColumns() * Model->GetNumRows() > 160;
	if (bDrawsSimplisticView != bShouldDrawSimplisticView)
	{
		UpdateContent();
	}
}

void SDMXPixelMappingScreenComponent::UpdateContent()
{
	if(Model->GetNumColumns() * Model->GetNumRows() > 160)
	{
		bDrawsSimplisticView = true;
		ContentBorder->SetContent
		(
			SNew(SDMXPixelMappingScreenComponentSimplistic, Model.ToSharedRef())
		);
	}
	else
	{
		bDrawsSimplisticView = false;
		ContentBorder->SetContent
		(
			SNew(SDMXPixelMappingScreenComponentGrid, Model.ToSharedRef())
		);
	}
}

#undef LOCTEXT_NAMESPACE
