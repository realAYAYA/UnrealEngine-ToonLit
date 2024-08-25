// Copyright Epic Games, Inc. All Rights Reserved.
#include "STG_SelectionPreview.h"
#include "Brushes/SlateImageBrush.h"
#include "EdGraph/TG_EdGraphSchema.h"
#include "TG_Graph.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SButton.h"
#include <Widgets/Input/SCheckBox.h>
#include "Engine/Texture2D.h"
#include "EdGraph/TG_EdGraph.h"
#include "EdGraph/TG_PinSelectionManager.h"
#include "TG_Pin.h"
#include "TG_Graph.h"
#include "TG_Var.h"
#include "Device/DeviceManager.h"
#include "Device/FX/DeviceBuffer_FX.h"
#include "STG_TexturePreviewViewport.h"
#include "Slate/SceneViewport.h"
#include "TG_Texture.h"
#include "TG_Editor.h"
#include "TG_HelperFunctions.h"
#include "Styling/StyleColors.h"
#include "Brushes/SlateNoResource.h"
#include "Transform/Utility/T_TextureHistogram.h"
#include "STextureHistogram.h"
#include "SPrimaryButton.h"


void STG_SelectionPreview::Construct(const FArguments& InArgs)
{
	const float TEXT_PADDING = 2.0;
	UncheckedBrush = new FSlateRoundedBoxBrush(FStyleColors::Input, CoreStyleConstants::InputFocusRadius);
	CheckedBrush = new FSlateRoundedBoxBrush(FStyleColors::InputOutline, CoreStyleConstants::InputFocusRadius);
	OnBlobSelectionChanged = InArgs._OnBlobSelectionChanged;
	ChildSlot
		[
			SAssignNew(VerticalBox, SVerticalBox)

			/*+ SVerticalBox::Slot()

			.Padding(TEXT_PADDING)
			.AutoHeight()
			[
				SAssignNew(HeadingText, STextBlock)
				.Text(FText::FromString("No output to preview"))
				.AutoWrapText(true)
			]*/
			+ SVerticalBox::Slot()

			.Padding(TEXT_PADDING)
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				SNew(SHorizontalBox)

				/// DEBUG ONLY
#if UE_BUILD_DEBUG
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Right)
					[
						SNew(SButton)
							[
								SNew(SPrimaryButton)
									.Text(FText::FromName("Dbg Dmp"))
									.OnClicked(this, &STG_SelectionPreview::OnDumpClick)
							]
					]
#endif /// UE_BUILD_DEBUG
					
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				.AutoWidth()
				[
					 SNew(SBox)
						.HAlign(HAlign_Right)
						.VAlign(VAlign_Center)
						[
							SNew(SCheckBox)
								.Style(&FAppStyle::Get().GetWidgetStyle<FCheckBoxStyle>("ToggleButtonCheckBoxAlt"))
								.Type(ESlateCheckBoxType::ToggleButton)
								.UncheckedImage(UncheckedBrush)
								.UncheckedHoveredImage(UncheckedBrush)
								.CheckedHoveredImage(CheckedBrush)
								.CheckedImage(CheckedBrush)
								.Padding(FMargin(4))
								.ToolTipText(NSLOCTEXT("STG_SelectionPreview", "LockSelectionButton_ToolTip", "Locks the output of current selected node"))
								.OnCheckStateChanged(this, &STG_SelectionPreview::OnLockChanged)
								.IsChecked(this, &STG_SelectionPreview::IsPreviewLocked)
								[
									SNew(SImage)
										.ColorAndOpacity(FSlateColor::UseForeground())
										.Image(this, &STG_SelectionPreview::OnGetLockButtonImageResource)
								]
						]
				]

					//Zoom Dropdown Menu
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Right)
					.Padding(1.0f)
					[
						MakeZoomControlWidget()
					]

				//Zoom Plus button
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				.Padding(1.0f)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "TextureEditor.MipmapButtonStyle")
					.OnClicked(this, &STG_SelectionPreview::HandleZoomPlusButtonClicked)
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush("Icons.Plus"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]

				//Zoom Minus button
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				.Padding(1.0f)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "TextureEditor.MipmapButtonStyle")
					.OnClicked(this, &STG_SelectionPreview::HandleZoomMinusButtonClicked)
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush("Icons.Minus"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				.Padding(1.0f)
				.FillWidth(0.9)
				[
					SNew(SHorizontalBox)

					//Output selection menu
					//+ SHorizontalBox::Slot()
					//.AutoWidth()
					//.VAlign(VAlign_Center)
					//.HAlign(HAlign_Right)
					//.Padding(2.0f, 0.0f, 4.0f, 0.0f)
					//[
					//	SNew(SBox)
					//	.MaxDesiredWidth(100)
					//	[
					//		SNew(SComboButton)
					//		.OnGetMenuContent(this, &STG_SelectionPreview::OnGenerateOutputMenu)
					//		.ButtonContent()
					//		[
					//			SAssignNew(OutputTextBlock, STextBlock)
					//			.Justification(ETextJustify::Center)
					//			.Text(this, &STG_SelectionPreview::HandleOutputText)//FText::Format(NSLOCTEXT("STG_SelectionPreview", "Output", "Output {0}"), SpecifiedOutput))
					//		]
					//	]
					//]

					//RGBA buttons
					+ SHorizontalBox::Slot()
					.Padding(4.0f, 0.0f, 2.0f, 0.0f)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					.FillWidth(0.1)
					[
						SAssignNew(RGBAButtons, STG_RGBAButtons)
					]
				]
			]
		];

	//Update the ViewPort Texture
	ConstructBlobView();
	ZoomMode = ETSZoomMode::Fill;
}

ECheckBoxState STG_SelectionPreview::IsPreviewLocked() const
{
	if (IsLocked)
	{
		return ECheckBoxState::Checked;
	}
	else
	{
		return ECheckBoxState::Unchecked;
	}
}

FReply STG_SelectionPreview::HandleZoomPlusButtonClicked()
{
	ZoomIn();
	return FReply::Handled();
}

FReply STG_SelectionPreview::HandleZoomMinusButtonClicked()
{
	ZoomOut();
	return FReply::Handled();
}

#if UE_BUILD_DEBUG
#include "STG_OutputSelectionDlg.h"

FReply STG_SelectionPreview::OnDumpClick()
{
	if (!PreviewBlob || !SelectedNode)
		return FReply::Handled();

	FString AssetPath = SelectedNode->GetGraph()->GetPathName();
	FString DefaultDirectory = FPaths::GetPath(AssetPath);

	TiledBlobPtr Blob = std::static_pointer_cast<TiledBlob>(PreviewBlob);
	const BlobPtrTiles& Tiles = Blob->GetTiles();

	FString ExportPath = DefaultDirectory + "/" + Blob->Name(); /// "d:/Temp/";

	for (size_t Row = 0; Row < Blob->Rows(); Row++)
	{
		for (size_t Col = 0; Col < Blob->Cols(); Col++)
		{
			BlobPtr Tile = Blob->GetTile(Row, Col);
			if (Tile)
			{
				DeviceBuffer_FX* FXBuffer = static_cast<DeviceBuffer_FX*>(Tile->GetBufferRef().get());
				if (FXBuffer && FXBuffer->GetTexture() && FXBuffer->GetTexture()->IsRenderTarget())
				{
					FXBuffer->Raw().then([=](RawBufferPtr Raw)
						{
							FString TilePath = FString::Printf(TEXT("%s/Tile-%d,%d.png"), *ExportPath, (int32)Row, (int32)Col);
							TextureHelper::ExportRaw(Raw, TilePath);
						});
					//UTextureRenderTarget2D* RT = FXBuffer->GetTexture()->GetRenderTarget();
					//Tex::SaveImage(RT, ExportPath, TileName);
				}
			}
		}
	}

	/// Also dump combine
	Blob->CombineTiles(false, false).then([=](BufferResultPtr Result)
		{
			Blob->GetBufferRef()->Raw().then([=](RawBufferPtr Raw)
				{
					FString CombinedPath = FString::Printf(TEXT("%s/Combined.png"), *ExportPath);
					TextureHelper::ExportRaw(Raw, CombinedPath);
				});
		});

	UE_LOG(LogTemp, Warning, TEXT("Exporting to directory: %s"), *DefaultDirectory);

	return FReply::Handled();
}
#endif /// UE_BUILD_DEBUG

TSharedRef<SWidget> STG_SelectionPreview::OnGenerateOutputMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	UTG_EdGraphNode* Node = SelectedNode;

	if (GetIsLockedNode())
	{
		Node = LockedNode;
	}

	if (!Node)
	{
		return MenuBuilder.MakeWidget();
	}

	for (auto pin : Node->Pins)
	{
		if (pin->Direction == EEdGraphPinDirection::EGPD_Output)
		{
			FString PinName = pin->GetName();

			MenuBuilder.AddMenuEntry(
				FText::FromString(PinName),
				FText::FromString(PinName),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &STG_SelectionPreview::HandleOutputChanged, PinName),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([this, PinName]() {return SpecifiedOutputName == PinName; })
				));
		}
	}

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> STG_SelectionPreview::MakeZoomControlWidget()
{
	const FMargin ToolbarSlotPadding(4.0f, 1.0f);
	const FMargin ToolbarButtonPadding(4.0f, 0.0f);

	FMenuBuilder ZoomMenuBuilder(true, NULL);
	{
		FUIAction Zoom25Action(FExecuteAction::CreateSP(this, &STG_SelectionPreview::HandleZoomMenuEntryClicked, 0.25));
		ZoomMenuBuilder.AddMenuEntry(NSLOCTEXT("STG_SelectionPreview", "Zoom25Action", "25%"), NSLOCTEXT("STG_SelectionPreview", "Zoom25ActionHint", "Show the texture at a quarter of its size."), FSlateIcon(), Zoom25Action);

		FUIAction Zoom50Action(FExecuteAction::CreateSP(this, &STG_SelectionPreview::HandleZoomMenuEntryClicked, 0.5));
		ZoomMenuBuilder.AddMenuEntry(NSLOCTEXT("STG_SelectionPreview", "Zoom50Action", "50%"), NSLOCTEXT("STG_SelectionPreview", "Zoom50ActionHint", "Show the texture at half its size."), FSlateIcon(), Zoom50Action);

		FUIAction Zoom100Action(FExecuteAction::CreateSP(this, &STG_SelectionPreview::HandleZoomMenuEntryClicked, 1.0));
		ZoomMenuBuilder.AddMenuEntry(NSLOCTEXT("STG_SelectionPreview", "Zoom100Action", "100%"), NSLOCTEXT("STG_SelectionPreview", "Zoom100ActionHint", "Show the texture in its original size."), FSlateIcon(), Zoom100Action);

		FUIAction Zoom200Action(FExecuteAction::CreateSP(this, &STG_SelectionPreview::HandleZoomMenuEntryClicked, 2.0));
		ZoomMenuBuilder.AddMenuEntry(NSLOCTEXT("STG_SelectionPreview", "Zoom200Action", "200%"), NSLOCTEXT("STG_SelectionPreview", "Zoom200ActionHint", "Show the texture at twice its size."), FSlateIcon(), Zoom200Action);

		FUIAction Zoom400Action(FExecuteAction::CreateSP(this, &STG_SelectionPreview::HandleZoomMenuEntryClicked, 4.0));
		ZoomMenuBuilder.AddMenuEntry(NSLOCTEXT("STG_SelectionPreview", "Zoom400Action", "400%"), NSLOCTEXT("STG_SelectionPreview", "Zoom400ActionHint", "Show the texture at four times its size."), FSlateIcon(), Zoom400Action);
		
		ZoomMenuBuilder.AddMenuSeparator();

		FUIAction ZoomFitAction(
			FExecuteAction::CreateSP(this, &STG_SelectionPreview::HandleZoomMenuFitClicked),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &STG_SelectionPreview::IsZoomMenuFitChecked)
		);
		ZoomMenuBuilder.AddMenuEntry(NSLOCTEXT("STG_SelectionPreview", "ZoomFitAction", "Scale To Fit"), NSLOCTEXT("STG_SelectionPreview", "ZoomFitActionHint", "Scales the texture down to fit within the viewport if needed."), FSlateIcon(), ZoomFitAction, NAME_None, EUserInterfaceActionType::RadioButton);
		
		FUIAction ZoomFillAction(
			FExecuteAction::CreateSP(this, &STG_SelectionPreview::HandleZoomMenuFillClicked),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &STG_SelectionPreview::IsZoomMenuFillChecked)
		);
		ZoomMenuBuilder.AddMenuEntry(NSLOCTEXT("STG_SelectionPreview", "ZoomFillAction", "Scale To Fill"), NSLOCTEXT("STG_SelectionPreview", "ZoomFillActionHint", "Scales the texture up and down to fill the viewport."), FSlateIcon(), ZoomFillAction, NAME_None, EUserInterfaceActionType::RadioButton);
	}

	// zoom Dropdown
	TSharedRef<SWidget> ZoomControl =

		SNew(SBox)
		.MaxDesiredWidth(150)
		[
			SNew(SComboButton)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Justification(ETextJustify::Center)
				.Text(this, &STG_SelectionPreview::HandleZoomPercentageText)
			]
			.MenuContent()
			[
				ZoomMenuBuilder.MakeWidget()
			]
		];

	return ZoomControl;
}

void STG_SelectionPreview::HandleZoomMenuEntryClicked(double ZoomValue)
{
	SetCustomZoomLevel(ZoomValue);
}

FText STG_SelectionPreview::HandleZoomPercentageText() const
{
	double DisplayedZoomLevel = CalculateDisplayedZoomLevel();
	FText ZoomLevelPercent = FText::AsPercent(DisplayedZoomLevel);

	// For fit and fill, show the effective zoom level in parenthesis - eg. "Fill (220%)"
	static const FText ZoomModeWithPercentFormat = NSLOCTEXT("STG_SelectionPreview", "ZoomModeWithPercentFormat", "{ZoomMode} ({ZoomPercent})");
	if (ZoomMode == ETSZoomMode::Fit)
	{
		static const FText ZoomModeFit = NSLOCTEXT("STG_SelectionPreview", "ZoomModeFit", "Fit");
		return FText::FormatNamed(ZoomModeWithPercentFormat, TEXT("ZoomMode"), ZoomModeFit, TEXT("ZoomPercent"), ZoomLevelPercent);
	}

	if (ZoomMode == ETSZoomMode::Fill)
	{
		static const FText ZoomModeFill = NSLOCTEXT("STG_SelectionPreview", "ZoomModeFill", "Fill");
		return FText::FormatNamed(ZoomModeWithPercentFormat, TEXT("ZoomMode"), ZoomModeFill, TEXT("ZoomPercent"), ZoomLevelPercent);
	}
	// If custom, then just the percent is enough
	return ZoomLevelPercent;
}

void STG_SelectionPreview::HandleZoomMenuFitClicked()
{
	ZoomMode = ETSZoomMode::Fit;
}

bool STG_SelectionPreview::IsZoomMenuFitChecked() const
{
	return ZoomMode == ETSZoomMode::Fit;
}

void STG_SelectionPreview::HandleZoomMenuFillClicked()
{
	ZoomMode = ETSZoomMode::Fill;
}

bool STG_SelectionPreview::IsZoomMenuFillChecked() const
{
	return ZoomMode == ETSZoomMode::Fill;
}

FText STG_SelectionPreview::HandleOutputText() const
{
	return FText::FromString(SpecifiedOutputName);
}

void STG_SelectionPreview::HandleOutputChanged(FString OuputName)
{
	SpecifiedOutputName = OuputName;
	
	UTG_EdGraphNode* Node = SelectedNode;

	if (GetIsLockedNode())
	{
		Node = LockedNode;
	}

	if (!Node)
	{
		return;
	}

	auto Graph = Cast<UTG_EdGraph>(Node->GetGraph());
	check(Graph);
	const UTG_EdGraphSchema* Schema = Cast<const UTG_EdGraphSchema>(Node->GetSchema());
	auto Pins = Node->GetAllPins();
	auto OutPin = Node->GetNode()->GetOutputPin(FName(OuputName));

	for (auto Pin : Pins)
	{
		auto TSPin = Schema->GetTGPinFromEdPin(Pin);
		if (TSPin->GetId() == OutPin->GetId())
		{
			Graph->PinSelectionManager.UpdateSelection(Pin);
		}
	}
}

FReply STG_SelectionPreview::OnLockClick()
{
	IsLocked = !IsLocked;
	if (IsLocked)
	{
		LockedNode = SelectedNode;
	}
	else
	{
		LockedNode = nullptr;
		//Need to update the preview because to show the output of current selected node
		UpdatePreview();
	}

	return FReply::Handled();
}
void STG_SelectionPreview::OnLockChanged(const ECheckBoxState NewCheckState)
{
	IsLocked = NewCheckState == ECheckBoxState::Checked;
	if (IsLocked)
	{
		LockedNode = SelectedNode;
	}
	else
	{
		LockedNode = nullptr;
		//Need to update the preview because to show the output of current selected node
		UpdatePreview();
	}
}

const FSlateBrush* STG_SelectionPreview::OnGetLockButtonImageResource() const
{
	if (IsLocked)
	{
		return FAppStyle::GetBrush(TEXT("PropertyWindow.Locked"));
	}
	else
	{
		return FAppStyle::GetBrush(TEXT("PropertyWindow.Unlocked"));
	}
}

ECheckBoxState STG_SelectionPreview::GetCheckBoxState() const
{
	if (ShowTileView)
	{
		return ECheckBoxState::Checked;
	}
	else
	{
		return ECheckBoxState::Unchecked;
	}
}

void STG_SelectionPreview::OnCheckBoxStateChanged(ECheckBoxState NewState)
{
	ShowTileView = NewState == ECheckBoxState::Checked;
	Margin = FMargin(0, 0, 0, 0);
	if (ShowTileView)
	{
		Margin = FMargin(2, 2, 2, 2);
	}

	UpdatePreview();
}

void STG_SelectionPreview::UpdatePreview()
{
	OnSelectionChanged(SelectedNode);
}

void STG_SelectionPreview::ConstructBlobView(BlobPtr InBlob)
{
	TextureViewport = SNew(STG_TexturePreviewViewport, SharedThis(this))
	.OnMouseHover(this,&STG_SelectionPreview::OnTexturePreviewMouseHover);
	
	VerticalBox->AddSlot()
	[
		SAssignNew(TileViewScaleBox, SScaleBox)
		.Stretch(EStretch::Fill)
		[
			TextureViewport.ToSharedRef()
		]
	];

	VerticalBox->AddSlot()
	.Padding(2,5,2,5)
	.AutoHeight()
	.HAlign(HAlign_Fill)
	[
		SNew( STextBlock)
		.Text(this, &STG_SelectionPreview::GetOutputDetailsText)
		.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
		.Justification(ETextJustify::Center)
		.AutoWrapText(true)
	];
}

FText STG_SelectionPreview::GetOutputDetailsText() const
{
	return FText::FromString(OutputDetailsText + "\n" + PixelInfo);
}

void STG_SelectionPreview::OnTexturePreviewMouseHover()
{
	UpdatePixelInfo();
}

void STG_SelectionPreview::UpdatePixelInfo()
{
	FVector2D MousePosition;
	if (TextureViewport->GetMousePosition(MousePosition) && PreviewBlob)
	{
		int32 DisplayWidth, DisplayHeight, DisplayDepth, DisplayArraySize;
		FVector2D TextureCoordinates;
		
		CalculateTextureDimensions(DisplayWidth, DisplayHeight, DisplayDepth, DisplayArraySize, false);

		float ScrollX = TextureViewport->GetHorizontalScrollBar()->DistanceFromTop();
		float ScrollY = TextureViewport->GetVerticalScrollBar()->DistanceFromTop();

		int32 ViewportSizeX = TextureViewport->GetViewport()->GetSizeXY().X;
		int32 ViewportSizeY = TextureViewport->GetViewport()->GetSizeXY().Y;
		
		float MaxScrollX = 1.0 - ((float)ViewportSizeX / DisplayWidth);
		float MaxScrollY = 1.0 - ((float)ViewportSizeY / DisplayHeight);

		//when the Display size is not equal to the Viewport size we will use the difference to offset the Tex Cords
		int32 OffsetX = ((ViewportSizeX - DisplayWidth)) ;
		int32 OffsetY = ((ViewportSizeY - DisplayHeight ));

		//When zoomed out the image is centered so we need to offset the tex cords to move them to center
		TextureCoordinates.X = (MousePosition.X - OffsetX * 0.5) / DisplayWidth;
		TextureCoordinates.Y = (MousePosition.Y - OffsetY * 0.5) / DisplayHeight;

		//When zoomed in Pan the values according to scroll bar position X and Y
		if (MaxScrollX > 0)
		{
			TextureCoordinates.X = (MousePosition.X - OffsetX * (ScrollX / MaxScrollX)) / DisplayWidth;
		}

		if (MaxScrollY > 0)
		{
			TextureCoordinates.Y = (MousePosition.Y - OffsetY * (ScrollY / MaxScrollY)) / DisplayHeight;
		}

		//No need to update the values when out of bound (0-1)
		if (TextureCoordinates.X >= 0 && TextureCoordinates.X <= 1 &&
			TextureCoordinates.Y >= 0 && TextureCoordinates.Y <= 1)
		{
			int Width = PreviewBlob->GetWidth();
			int Height = PreviewBlob->GetHeight();

			// Calculate the pixel position
			int32 X = FMath::Clamp(FMath::RoundToInt(TextureCoordinates.X * Width), 0, Width - 1);
			int32 Y = FMath::Clamp(FMath::RoundToInt(TextureCoordinates.Y * Height), 0, Height - 1);

			//PreviewBlob
			DeviceBufferRef Buffer = PreviewBlob->GetBufferRef();

			if (Buffer && Buffer.IsValid())
			{
				Buffer->Raw()
					.then([=, this](RawBufferPtr raw)
					{
						FLinearColor LinearColor = TextureHelper::GetPixelValueFromRaw(raw, Width, Height, X, Y);
						FString ColorString = raw->GetDescriptor().Format == BufferFormat::Byte ? LinearColor.ToFColorSRGB().ToString() : LinearColor.ToString();   
						PixelInfo = FString::Printf(TEXT("XY: (%d, %d), RGBA: %s"), X, Y, *ColorString);
					});
			}
		}
	}
}

FString STG_SelectionPreview::BufferToString(BufferDescriptor Desc)
{
	FString FormatString = Desc.FormatToString(Desc.Format);
	FString sRGB = Desc.bIsSRGB ? FString(TEXT("true")) : FString(TEXT("false"));
	FString Channel = TextureHelper::GetChannelsTextFromItemsPerPoint(Desc.ItemsPerPoint);

	FString BufferString = "Format: " + Channel + "_" + FormatString + ", ";
	BufferString += "Resolution: " + FString::FromInt(Desc.Width) + "x" + FString::FromInt(Desc.Height) + ", ";
	BufferString += "sRGB: " + sRGB;

	return BufferString;
}

UTG_EdGraphNode* STG_SelectionPreview::GetSelectedNode()
{
	if (IsLocked)
	{
		return LockedNode;
	}

	return SelectedNode;
}

void STG_SelectionPreview::OnSelectedNodeDeleted()
{
	SelectedNode = nullptr;
	LockedNode = nullptr;
	IsLocked = false;
	OnSelectionChanged(nullptr);
}

void STG_SelectionPreview::OnSelectionChanged(UTG_EdGraphNode* InNode)
{
	TextureViewport->SetViewportClientClearColor(FLinearColor::Black);
	BlobPtr Blob = nullptr;
	auto Heading = FText::FromString(TEXT("No output to preview"));
	UTG_EdGraphNode* Node = InNode;
	FString OutputText = "Output";

	//If the selection is updated by user and the preview is locked, we refresh 
	//the preview for locked node
	if (GetIsLockedNode())
	{
		Node = LockedNode;
		OutputText = SpecifiedOutputName;
	}

	if (Node)
	{
		const UTG_Node* TGNode = Node->GetNode();
		TArray<FTG_Texture> OutTextures;
		MaxOutputs = TGNode->GetAllOutputValues(OutTextures);

		//If we get the selected Pin from Node fine otherwise we get the
		//Pin by name . It is possible to not get selected pin from node
		//As of pin can get changes while our Node is locked so for that we find pin by name
		auto SelectedPin = Node->GetSelectedPin();
		if(!SelectedPin)
		{
			SelectedPin = Node->FindPinByPredicate([this](UEdGraphPin* Pin) { return Pin->Direction == EGPD_Output && Pin->GetName() == SpecifiedOutputName; });
		}

		int OutputIndex = 0;

		//If the selected node is same as already selected node or locked node
		// we tend to keep the selected pin
		if (UseSpecifiedOutput(Node) && !OutTextures.IsEmpty())
		{
			if (SelectedPin)
			{
				SpecifiedOutputName = SelectedPin->GetName();
				OutputIndex = GetSelectedPinIndex(Node, SelectedPin);
			}
			else
			{
				SpecifiedOutputName = OutputText;
			}

			Blob = OutTextures[OutputIndex].RasterBlob;
		}
		//If selected node is not same then we try to find the pin with same name other wise 
		//set the first avalible pin output as previw
		else if (OutTextures.Num() > 0)
		{
			if (SelectedPin)
			{
				SpecifiedOutputName = SelectedPin->GetName();
				OutputIndex = GetSelectedPinIndex(Node, SelectedPin);
				Blob = OutTextures[OutputIndex].RasterBlob;
			}
			else
			{
				Blob = OutTextures[0].RasterBlob;
				SpecifiedOutputName = OutputText;
			}
		}

		//Updating the Dropdown text
		//OutputTextBlock->SetText(FText::FromString(SpecifiedOutputName));

		//Set the heading disabled in UI for now
		//Check if Blob exist try to set the texture for preview from blob
		if (Blob != nullptr)
		{

			Heading = FText::FromString(Node->GetNode()->GetNodeName().ToString());
		
			//Check if the blob is tiled we need to combine it and then set the texture for preview
			//As for new we do not support multiple tiles in viewport
			if (Blob->IsTiled())
			{
				Blob->OnFinalise()
					.then([=, this]()
					{
						// OnFinalise can sometimes occur after the editor is closed and thus can potentially
						// deallocate all corresponding slate objects
						if (DoesSharedInstanceExist())
						{
							TiledBlobPtr BlobTiled = std::static_pointer_cast<TiledBlob>(Blob);
							return BlobTiled->CombineTiles(false, false);
						}
						return static_cast<AsyncBufferResultPtr>(cti::make_ready_continuable<BufferResultPtr>(std::make_shared<BufferResult>()));
						
					})
					.then([this, Blob](BufferResultPtr BufferPtr)
					{
						if (DoesSharedInstanceExist())
						{
							SetTexture(Blob);

							// Set Texture Properties
							SetTextureProperties();
							UpdateOutputDetailsText(Blob);
						}
					});
			}
			else
			{
				Blob->OnFinalise()
					.then([this, Blob]()
					{
						if (DoesSharedInstanceExist())
						{
							// Set Texture
							SetTexture(Blob);
							// Set Texture Properties
							SetTextureProperties();
							UpdateOutputDetailsText(Blob);
						}
					});
			}
		}
		else
		{
			// check here if pin is of color type, we simply output color
			const UTG_EdGraphSchema* Schema = Cast<const UTG_EdGraphSchema>(Node->GetSchema());
			UTG_Pin* TGPin;
			if (SelectedPin)
			{
				TGPin= Schema->GetTGPinFromEdPin(SelectedPin);
			}
			else
			{
				TGPin = TGNode->GetOutputPinAt(0);
			}
			 
			if (TGPin && TGPin->IsArgColor())
			{
				FLinearColor ColorValue;
				check (TGPin->GetValue(ColorValue));
				const FColor SRGBColor = ColorValue.ToFColorSRGB();
				TextureViewport->SetViewportClientClearColor(SRGBColor.ReinterpretAsLinear());
			}
			
			// Calling SetTexture here with null to set Texture to null
			// this will make sure the ViewportClient exits after
			// clearing to the expected color
			SetTexture(nullptr);
			UpdateOutputDetailsText(TGPin);
		}
	}
	else
	{
		//Null blob will set the black preview
		SetTexture(Blob);
		ResetOutputDetailsText();
	}

	//Update the output selection value when clicked on Graph
	if (!Node && !GetIsLockedNode())
	{
		SpecifiedOutputName = OutputText;
		//OutputTextBlock->SetText(FText::FromString(SpecifiedOutputName));
	}
	//HeadingText->SetText(Heading);
	SelectedNode = InNode;
	PreviewBlob = Blob;
	
	OnBlobSelectionChanged.ExecuteIfBound(Blob);
}

void STG_SelectionPreview::UpdateOutputDetailsText(BlobPtr InBlob)
{
	if (InBlob)
	{
		DeviceBufferPtr Buffer = InBlob->GetBufferRef().GetPtr();
		// Hopefully found a device buffer if not early exit
		if (!Buffer)
		{
			ResetOutputDetailsText();
			return;
		}

		OutputDetailsText = BufferToString(Buffer->Descriptor());
	}
	else
	{
		ResetOutputDetailsText();
	}
}

void STG_SelectionPreview::UpdateOutputDetailsText(UTG_Pin* Pin)
{
	if (Pin == nullptr)
	{
		ResetOutputDetailsText();
	}
	else
	{
		if (Pin->IsArgColor())
		{
			FLinearColor ColorValue;
			check(Pin->GetValue(ColorValue));
			const FColor SRGBColor = ColorValue.ToFColorSRGB();
			OutputDetailsText = "Color Value : " + SRGBColor.ToString();
			PixelInfo = "";
		}
		else if (Pin->IsArgScalar())
		{
			float ScalarValue = 0.0;
			check(Pin->GetValue(ScalarValue));
			OutputDetailsText = "Scalar Value : " + FString::Printf(TEXT("%0.3f"), ScalarValue);
			PixelInfo = "";
		}
		else if (Pin->IsArgVector())
		{
			FVector4f VectorValue(0,0,0,0);
			check(Pin->GetValue(VectorValue));
			OutputDetailsText = "Vector Value : " + VectorValue.ToString();
			PixelInfo = "";
		}
		else
		{
			ResetOutputDetailsText();
		}
	}
}

void STG_SelectionPreview::ResetOutputDetailsText()
{
	OutputDetailsText = "";
	PixelInfo = "";
}

int STG_SelectionPreview::GetSelectedPinIndex(UTG_EdGraphNode* InNode,const UEdGraphPin* SelectedPin)
{
	const UTG_EdGraphSchema* Schema = Cast<const UTG_EdGraphSchema>(InNode->GetSchema());
	auto TSPin = Schema->GetTGPinFromEdPin(SelectedPin);

	int OutputIndex = 0;
	auto OutPinIds = InNode->GetNode()->GetOutputPinIds();
	for (auto Pin : OutPinIds)
	{
		if (Pin == TSPin->GetId())
		{
			return OutputIndex;
		}
		OutputIndex++;
	}

	return 0;
}

bool STG_SelectionPreview::UseSpecifiedOutput(UTG_EdGraphNode* Node)
{
	return (Node == SelectedNode || Node == LockedNode);
}

UTexture* STG_SelectionPreview::GetTexture() const
{
	return Texture;
}

void STG_SelectionPreview::SetTexture(BlobPtr InBlob)
{
	Texture = nullptr;
	IsSingleChannel = false;
	bSRGB = false;
	if (!InBlob)
	{
		return;
	}

	DeviceBufferPtr Buffer = InBlob->GetBufferRef().GetPtr();
	// Hopefully found a device buffer if not early exit
	if (!Buffer)
	{
		return;
	}
	
	IsSingleChannel = Buffer->Descriptor().ItemsPerPoint == 1;
	bSRGB = Buffer->Descriptor().bIsSRGB;

	Texture = GetTextureFromBuffer(Buffer);
}

UTexture* STG_SelectionPreview::GetTextureFromBuffer(DeviceBufferPtr Buffer)
{
	UTexture* OutTexture = nullptr;
	auto FXBuffer = std::static_pointer_cast<DeviceBuffer_FX>(Buffer);
	if (FXBuffer)
	{
		if (!FXBuffer->IsNull())
		{
			auto Tex = FXBuffer->GetTexture();
			check(Tex);
			OutTexture = Tex->GetTexture();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Buffer is not an FXBuffer"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("BlobTexture failed to find the buffer ?"));
	}

	return OutTexture;
}

void STG_SelectionPreview::SetTextureProperties()
{

	UTexture* TextureToUse = GetTexture();

	const int32 SurfaceWidth = TextureToUse->GetSurfaceWidth();
	const int32 SurfaceHeight = TextureToUse->GetSurfaceHeight();
	const int32 SurfaceDepth = TextureToUse->GetSurfaceDepth();
	
	const FStreamableRenderResourceState SRRState = TextureToUse->GetStreamableResourceState();
	const int32 ActualMipBias = SRRState.IsValid() ? (SRRState.ResidentFirstLODIdx() + SRRState.AssetLODBias) : TextureToUse->GetCachedLODBias();
	// Editor dimensions (takes user specified mip setting into account)
	const int32 MipLevel = ActualMipBias + FMath::Max(0/*GetMipLevel()*/, 0);
	PreviewEffectiveTextureWidth = SurfaceWidth ? FMath::Max(SurfaceWidth >> MipLevel, 1) : 0;
	PreviewEffectiveTextureHeight = SurfaceHeight ? FMath::Max(SurfaceHeight >> MipLevel, 1) : 0;
	const int32 PreviewEffectiveTextureDepth = SurfaceDepth ? FMath::Max(SurfaceDepth >> MipLevel, 1) : 0;

	ExposureBias = 0;
}

void STG_SelectionPreview::CalculateTextureDimensions(int32& OutWidth, int32& OutHeight, int32& OutDepth, int32& OutArraySize, bool bInIncludeBorderSize) const
{
	UTexture* TextureToUse = GetTexture();

	if (!PreviewEffectiveTextureWidth || !PreviewEffectiveTextureHeight || TextureToUse == nullptr)
	{
		OutWidth = 0;
		OutHeight = 0;
		OutDepth = 0;
		OutArraySize = 0;
		return;
	}

	OutWidth = TextureToUse->GetSurfaceWidth();
	OutHeight = TextureToUse->GetSurfaceHeight();
	OutDepth = TextureToUse->GetSurfaceDepth();
	OutArraySize = 0;
	const int32 BorderSize = 1;

	if (ZoomMode != ETSZoomMode::Custom)
	{
		const int32 MaxWidth = FMath::Max(TextureViewport->GetViewport()->GetSizeXY().X - 2 * BorderSize, 0);
		const int32 MaxHeight = FMath::Max(TextureViewport->GetViewport()->GetSizeXY().Y - 2 * BorderSize, 0);

		if (MaxWidth * PreviewEffectiveTextureHeight < MaxHeight * PreviewEffectiveTextureWidth)
		{
			OutWidth = MaxWidth;
			OutHeight = FMath::DivideAndRoundNearest(OutWidth * PreviewEffectiveTextureHeight, PreviewEffectiveTextureWidth);
		}
		else
		{
			OutHeight = MaxHeight;
			OutWidth = FMath::DivideAndRoundNearest(OutHeight * PreviewEffectiveTextureWidth, PreviewEffectiveTextureHeight);
		}

		// If fit, then we only want to scale down
		// So if our natural dimensions are smaller than the viewport, we can just use those
		if (ZoomMode == ETSZoomMode::Fit && (PreviewEffectiveTextureWidth < OutWidth || PreviewEffectiveTextureHeight < OutHeight))
		{
			OutWidth = PreviewEffectiveTextureWidth;
			OutHeight = PreviewEffectiveTextureHeight;
		}
	}
	else
	{
		OutWidth = PreviewEffectiveTextureWidth * Zoom;
		OutHeight = PreviewEffectiveTextureHeight * Zoom;
	}

	if (bInIncludeBorderSize)
	{
		OutWidth += 2 * BorderSize;
		OutHeight += 2 * BorderSize;
	}
}

ESimpleElementBlendMode STG_SelectionPreview::GetColourChannelBlendMode()
{
	UTexture* TextureToUse = GetTexture();

	if (TextureToUse && (TextureToUse->CompressionSettings == TC_Grayscale || TextureToUse->CompressionSettings == TC_Alpha))
	{
		return SE_BLEND_Opaque;
	}

	// Add the red, green, blue, alpha and desaturation flags to the enum to identify the chosen filters
	uint32 Result = (uint32)SE_BLEND_RGBA_MASK_START;

	if (IsSingleChannel)
	{
		Result += RGBAButtons->GetIsRChannel() ? (1 << 0) : 0;
		Result += RGBAButtons->GetIsRChannel() ? (1 << 1) : 0;
		Result += RGBAButtons->GetIsRChannel() ? (1 << 2) : 0;
	}
	else
	{
		Result += RGBAButtons->GetIsRChannel() ? (1 << 0) : 0;
		Result += RGBAButtons->GetIsGChannel() ? (1 << 1) : 0;
		Result += RGBAButtons->GetIsBChannel() ? (1 << 2) : 0;
		Result += RGBAButtons->GetIsAChannel() ? (1 << 3) : 0;
	}
	// If we only have one color channel active, enable color desaturation by default
	/*const int32 NumColorChannelsActive = (bIsRedChannel ? 1 : 0) + (bIsGreenChannel ? 1 : 0) + (bIsBlueChannel ? 1 : 0);
	const bool bIsDesaturationLocal = (IsSingleChannel == 1);
	Result += bIsDesaturationLocal ? (1 << 4) : 0;*/

	return (ESimpleElementBlendMode)Result;
}

void STG_SelectionPreview::ZoomIn()
{
	// mouse wheel zoom
	const double CurrentZoom = CalculateDisplayedZoomLevel();
	SetCustomZoomLevel(CurrentZoom * ZoomFactor);
}


void STG_SelectionPreview::ZoomOut()
{
	const double CurrentZoom = CalculateDisplayedZoomLevel();
	SetCustomZoomLevel(CurrentZoom / ZoomFactor);
}

double STG_SelectionPreview::CalculateDisplayedZoomLevel() const
{
	// Avoid calculating dimensions if we're custom anyway
	if (ZoomMode == ETSZoomMode::Custom)
	{
		return Zoom;
	}

	int32 DisplayWidth, DisplayHeight, DisplayDepth, DisplayArraySize;
	CalculateTextureDimensions(DisplayWidth, DisplayHeight, DisplayDepth, DisplayArraySize, false);
	if (PreviewEffectiveTextureHeight != 0)
	{
		return (double)DisplayHeight / PreviewEffectiveTextureHeight;
	}
	else if (PreviewEffectiveTextureWidth != 0)
	{
		return (double)DisplayWidth / PreviewEffectiveTextureWidth;
	}
	else
	{
		return 0;
	}
}

void STG_SelectionPreview::SetCustomZoomLevel(double ZoomValue)
{
	// snap to discrete steps so that if we are nearly at 1.0 or 2.0, we hit them exactly:
	//ZoomValue = FMath::GridSnap(ZoomValue, MinZoom/4.0);

	double LogZoom = log2(ZoomValue);
	// the mouse wheel zoom is quantized on ZoomFactorLogSteps
	//	but that's too chunky for the drag slider, give it more steps, but on the same quantization grid
	double QuantizationSteps = ZoomFactorLogSteps * 2.0;
	double LogZoomQuantized = (1.0 / QuantizationSteps) * FMath::RoundToInt(QuantizationSteps * LogZoom);
	ZoomValue = pow(2.0, LogZoomQuantized);

	ZoomValue = FMath::Clamp(ZoomValue, MinZoom, MaxZoom);

	// set member variable "Zoom"
	Zoom = ZoomValue;

	// For now we also want to be in custom mode whenever this is changed
	ZoomMode = ETSZoomMode::Custom;
}

double STG_SelectionPreview::GetCustomZoomLevel() const
{
	return Zoom;
}
