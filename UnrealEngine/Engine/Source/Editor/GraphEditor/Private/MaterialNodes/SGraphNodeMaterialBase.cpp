// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialNodes/SGraphNodeMaterialBase.h"

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Containers/Array.h"
#include "Containers/EnumAsByte.h"
#include "Containers/UnrealString.h"
#include "CoreGlobals.h"
#include "Delegates/Delegate.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/World.h"
#include "GenericPlatform/ICursor.h"
#include "GraphEditor.h"
#include "GraphEditorSettings.h"
#include "HAL/PlatformCrt.h"
#include "Layout/Geometry.h"
#include "Layout/Margin.h"
#include "Layout/SlateRect.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "MaterialGraph/MaterialGraphSchema.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionBreakMaterialAttributes.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionSubstrate.h"
#include "Materials/MaterialFunction.h"
#include "Math/Color.h"
#include "Math/IntPoint.h"
#include "Math/IntRect.h"
#include "Misc/App.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "Misc/Guid.h"
#include "Misc/Optional.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "RenderingThread.h"
#include "SGraphPanel.h"
#include "SGraphPin.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Styling/ISlateStyle.h"
#include "Templates/Casts.h"
#include "TutorialMetaData.h"
#include "Types/SlateEnums.h"
#include "Types/SlateStructs.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectPtr.h"
#include "UnrealClient.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SViewport.h"
#include "Widgets/Text/STextBlock.h"
#include "Rendering/SubstrateMaterialShared.h"
#include "SGraphSubstrateMaterial.h"
#include "MaterialShared.h"

class FWidgetStyle;
class SWidget;
struct FSlateBrush;

static const FName NAME_Pin_NotConnectable("Graph.Pin.Dummy");
static const FSlateBrush* CacheImg_Pin_NotConnectable = nullptr;

/**
* Simple representation of the backbuffer that the preview canvas renders to
* This class may only be accessed from the render thread
*/
class FSlateMaterialPreviewRenderTarget : public FRenderTarget
{
public:
	/** FRenderTarget interface */
	virtual FIntPoint GetSizeXY() const
	{
		return ClippingRect.Size();
	}

	/** Sets the texture that this target renders to */
	void SetRenderTargetTexture( FTexture2DRHIRef& InRHIRef )
	{
		RenderTargetTextureRHI = InRHIRef;
	}

	/** Clears the render target texture */
	void ClearRenderTargetTexture()
	{
		RenderTargetTextureRHI.SafeRelease();
	}

	/** Sets the viewport rect for the render target */
	void SetViewRect( const FIntRect& InViewRect ) 
	{ 
		ViewRect = InViewRect;
	}

	/** Gets the viewport rect for the render target */
	const FIntRect& GetViewRect() const 
	{
		return ViewRect;
	}

	/** Sets the clipping rect for the render target */
	void SetClippingRect( const FIntRect& InClippingRect ) 
	{ 
		ClippingRect = InClippingRect;
	}

	/** Gets the clipping rect for the render target */
	const FIntRect& GetClippingRect() const 
	{
		return ClippingRect;
	}

private:
	FIntRect ViewRect;
	FIntRect ClippingRect;
};

/*-----------------------------------------------------------------------------
   FPreviewViewport
-----------------------------------------------------------------------------*/

FPreviewViewport::FPreviewViewport(class UMaterialGraphNode* InNode)
	: MaterialNode(InNode)
	, PreviewElement( new FPreviewElement )
{
	if (MaterialNode)
	{
		MaterialNode->InvalidatePreviewMaterialDelegate.BindRaw(this, &FPreviewViewport::UpdatePreviewNodeRenderProxy);
	}
}

FPreviewViewport::~FPreviewViewport()
{
	if (MaterialNode)
	{
		MaterialNode->InvalidatePreviewMaterialDelegate.Unbind();
	}
	// Pass the preview element to the render thread so that it's deleted after it's shown for the last time
	ENQUEUE_RENDER_COMMAND(SafeDeletePreviewElement)(
		[PreviewElement = PreviewElement](FRHICommandListImmediate& RHICmdList) mutable
		{
			PreviewElement.Reset();
		}
	);
}

void FPreviewViewport::OnDrawViewport( const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, class FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled )
{
	FSlateRect SlateCanvasRect = AllottedGeometry.GetLayoutBoundingRect();
	FSlateRect ClippedCanvasRect = SlateCanvasRect.IntersectionWith(MyCullingRect);

	FIntRect CanvasRect(
		FMath::TruncToInt( FMath::Max(0.0f, SlateCanvasRect.Left) ),
		FMath::TruncToInt( FMath::Max(0.0f, SlateCanvasRect.Top) ),
		FMath::TruncToInt( FMath::Max(0.0f, SlateCanvasRect.Right) ), 
		FMath::TruncToInt( FMath::Max(0.0f, SlateCanvasRect.Bottom) ) );

	FIntRect ClippingRect(
		FMath::TruncToInt( FMath::Max(0.0f, ClippedCanvasRect.Left) ),
		FMath::TruncToInt( FMath::Max(0.0f, ClippedCanvasRect.Top) ),
		FMath::TruncToInt( FMath::Max(0.0f, ClippedCanvasRect.Right) ), 
		FMath::TruncToInt( FMath::Max(0.0f, ClippedCanvasRect.Bottom) ) );

	bool bIsRealtime = MaterialNode->RealtimeDelegate.IsBound() ? MaterialNode->RealtimeDelegate.Execute() : false;

	if (PreviewElement->BeginRenderingCanvas(CanvasRect, ClippingRect, MaterialNode, bIsRealtime))
	{
		// Draw above everything else
		uint32 PreviewLayer = LayerId+1;
		FSlateDrawElement::MakeCustom( OutDrawElements, PreviewLayer, PreviewElement );
	}
}

FIntPoint FPreviewViewport::GetSize() const
{
	return FIntPoint(96,96);
}

void FPreviewViewport::UpdatePreviewNodeRenderProxy()
{
	if (PreviewElement.IsValid())
	{
		PreviewElement->UpdateExpressionPreview(MaterialNode);
	}
}

/////////////////////////////////////////////////////
// FPreviewElement

FPreviewElement::FPreviewElement()
	: RenderTarget(new FSlateMaterialPreviewRenderTarget)
	, ExpressionPreview(nullptr)
	, bIsRealtime(false)
{
}

FPreviewElement::~FPreviewElement()
{
	delete RenderTarget;
}

bool FPreviewElement::BeginRenderingCanvas( const FIntRect& InCanvasRect, const FIntRect& InClippingRect, UMaterialGraphNode* InGraphNode, bool bInIsRealtime )
{
	if(InCanvasRect.Size().X > 0 && InCanvasRect.Size().Y > 0 && InClippingRect.Size().X > 0 && InClippingRect.Size().Y > 0 && InGraphNode != NULL)
	{
		/**
		 * Struct to contain all info that needs to be passed to the render thread
		 */
		struct FPreviewRenderInfo
		{
			/** Size of the Canvas tile */
			FIntRect CanvasRect;
			/** How to clip the canvas tile */
			FIntRect ClippingRect;
			/** Render proxy for the expression preview */
			FMaterialRenderProxy* RenderProxy;
			/** Whether preview is using realtime values */
			bool bIsRealtime;
		};

		FPreviewRenderInfo RenderInfo;
		RenderInfo.CanvasRect = InCanvasRect;
		RenderInfo.ClippingRect = InClippingRect;
		RenderInfo.RenderProxy = InGraphNode->GetExpressionPreview();
		RenderInfo.bIsRealtime = bInIsRealtime;

		FPreviewElement* PreviewElement = this;
		ENQUEUE_RENDER_COMMAND(BeginRenderingPreviewCanvas)(
			[PreviewElement, RenderInfo](FRHICommandListImmediate& RHICmdList)
			{
				PreviewElement->RenderTarget->SetViewRect(RenderInfo.CanvasRect);
				PreviewElement->RenderTarget->SetClippingRect(RenderInfo.ClippingRect);
				PreviewElement->ExpressionPreview = RenderInfo.RenderProxy;
				PreviewElement->bIsRealtime = RenderInfo.bIsRealtime;
			}
			);
		return true;
	}

	return false;
}

void FPreviewElement::UpdateExpressionPreview(UMaterialGraphNode* MaterialNode)
{
	FPreviewElement* PreviewElement = this;
	FMaterialRenderProxy* InRenderProxy = MaterialNode ? MaterialNode->GetExpressionPreview() : nullptr;
	ENQUEUE_RENDER_COMMAND(UpdatePreviewNodeRenderProxy)(
		[PreviewElement, InRenderProxy](FRHICommandListImmediate& RHICmdList)
		{
			PreviewElement->ExpressionPreview = InRenderProxy;
		}
	);
}

void FPreviewElement::Draw_RenderThread(FRHICommandListImmediate& RHICmdList, const void* InWindowBackBuffer, const FSlateCustomDrawParams& Params)
{
	if(ExpressionPreview)
	{
		RenderTarget->SetRenderTargetTexture(*(FTexture2DRHIRef*)InWindowBackBuffer);
		{
			// Check realtime mode for whether to pass current time to canvas
			double CurrentTime = bIsRealtime ? (FApp::GetCurrentTime() - GStartTime) : 0.0;
			float DeltaTime = bIsRealtime ? FApp::GetDeltaTime() : 0.0f;

			FCanvas Canvas(RenderTarget, NULL, FGameTime::CreateUndilated(CurrentTime, DeltaTime), GMaxRHIFeatureLevel);
			{
				Canvas.SetAllowedModes(0);
				Canvas.SetRenderTargetRect(RenderTarget->GetViewRect());
				Canvas.SetRenderTargetScissorRect(RenderTarget->GetClippingRect());

				FCanvasTileItem TileItem(FVector2D::ZeroVector, ExpressionPreview, RenderTarget->GetSizeXY());
				Canvas.DrawItem(TileItem);
			}
			Canvas.Flush_RenderThread(RHICmdList, true);
		}
		RenderTarget->ClearRenderTargetTexture();
		RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
	}
}

/////////////////////////////////////////////////////
// SGraphNodeMaterialBase

void SGraphNodeMaterialBase::Construct(const FArguments& InArgs, UMaterialGraphNode* InNode)
{
	this->GraphNode = InNode;
	this->MaterialNode = InNode;

	this->SetCursor(EMouseCursor::CardinalCross);

	this->UpdateGraphNode();
}

void SGraphNodeMaterialBase::CreatePinWidgets()
{
	// Create Pin widgets for each of the pins.
	for( int32 PinIndex=0; PinIndex < GraphNode->Pins.Num(); ++PinIndex )
	{
		UEdGraphPin* CurPin = GraphNode->Pins[PinIndex];

		bool bHideNoConnectionPins = false;

		if (OwnerGraphPanelPtr.IsValid())
		{
			bHideNoConnectionPins = OwnerGraphPanelPtr.Pin()->GetPinVisibility() == SGraphEditor::Pin_HideNoConnection;
		}

		const bool bPinHasConections = CurPin->LinkedTo.Num() > 0;

		bool bPinDesiresToBeHidden = CurPin->bHidden || (bHideNoConnectionPins && !bPinHasConections); 

		UMaterialGraph* MaterialGraph = CastChecked<UMaterialGraph>(GraphNode->GetGraph());
		if (MaterialNode && MaterialNode->MaterialExpression && MaterialGraph->MaterialFunction == nullptr && !MaterialGraph->MaterialInputs.IsEmpty())
		{
			bool bIsAMakeAttrNode = MaterialNode->MaterialExpression->IsA(UMaterialExpressionMakeMaterialAttributes::StaticClass());
			bool bIsABreakAttrNode = MaterialNode->MaterialExpression->IsA(UMaterialExpressionBreakMaterialAttributes::StaticClass());
		
			if ((bIsABreakAttrNode && CurPin->Direction == EGPD_Output) || (bIsAMakeAttrNode && CurPin->Direction == EGPD_Input))
			{
				if (CurPin->PinType.PinCategory != UMaterialGraphSchema::PC_Exec)
				{
					bPinDesiresToBeHidden |= !MaterialGraph->MaterialInputs[CurPin->SourceIndex].IsVisiblePin(MaterialGraph->Material, true);
				}
			}
		}

		if (!bPinDesiresToBeHidden)
		{
			TSharedPtr<SGraphPin> NewPin = CreatePinWidget(CurPin);
			check(NewPin.IsValid());

			// Assign an custom icon to not connectible pins
			if (CurPin->bNotConnectable)
			{
				if (!CacheImg_Pin_NotConnectable)
				{
					CacheImg_Pin_NotConnectable = FAppStyle::Get().GetBrush(NAME_Pin_NotConnectable);
				}
				NewPin->SetCustomPinIcon(CacheImg_Pin_NotConnectable, CacheImg_Pin_NotConnectable);
			}

			// Override pin color for Substrate node
			if (Substrate::IsSubstrateEnabled())
			{
				FSubstrateWidget::GetPinColor(NewPin, MaterialNode);
			}

			this->AddPin(NewPin.ToSharedRef());
		}
	}
}

void SGraphNodeMaterialBase::MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty)
{
	SGraphNode::MoveTo(NewPosition, NodeFilter, bMarkDirty);

	MaterialNode->MaterialExpression->MaterialExpressionEditorX = MaterialNode->NodePosX;
	MaterialNode->MaterialExpression->MaterialExpressionEditorY = MaterialNode->NodePosY;
	MaterialNode->MaterialExpression->MarkPackageDirty();
	MaterialNode->MaterialDirtyDelegate.ExecuteIfBound();
}

void SGraphNodeMaterialBase::AddPin( const TSharedRef<SGraphPin>& PinToAdd )
{
	PinToAdd->SetOwner( SharedThis(this) );

	// Set visibility on advanced view pins
	const UEdGraphPin* PinObj = PinToAdd->GetPinObj();
	const bool bAdvancedParameter = (PinObj != nullptr) && PinObj->bAdvancedView;
	if (bAdvancedParameter)
	{
		PinToAdd->SetVisibility(TAttribute<EVisibility>(PinToAdd, &SGraphPin::IsPinVisibleAsAdvanced));
	}

	if (PinToAdd->GetDirection() == EEdGraphPinDirection::EGPD_Input)
	{
		FMargin Padding = Settings->GetInputPinPadding();
		Padding.Left *= 0.5f;
		Padding.Right = 0.0f;

		LeftNodeBox->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(Padding)
		[
			PinToAdd
		];
		InputPins.Add(PinToAdd);
	}
	else // Direction == EEdGraphPinDirection::EGPD_Output
	{
		FMargin Padding = Settings->GetOutputPinPadding();
		Padding.Left = 0.0f;
		Padding.Right *= 0.5f;

		RightNodeBox->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(Padding)
		[
			PinToAdd
		];
		OutputPins.Add(PinToAdd);
	}
}

void SGraphNodeMaterialBase::CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox)
{
	if (GraphNode && MainBox.IsValid())
	{
		// Count the number of visible input pins on the left
		int32 LeftPinCount = 0;
		if (GraphNode->AdvancedPinDisplay == ENodeAdvancedPins::Hidden)
		{
			// Advanced view pins are hidden so exclude them from the pin count
			for (int32 i = 0; i < InputPins.Num(); ++i)
			{
				const UEdGraphPin* PinObj = InputPins[i]->GetPinObj();
				if (!PinObj->bAdvancedView)
				{
					LeftPinCount++;
				}
			}
		}
		else
		{
			LeftPinCount = InputPins.Num();
		}

		int32 RightPinCount = OutputPins.Num();

		const float NegativeHPad = FMath::Max<float>(-Settings->PaddingTowardsNodeEdge, 0.0f);
		const float ExtraPad = 0.0f;

		// Place preview widget based on where the least pins are
		if ((LeftPinCount < RightPinCount) || (RightPinCount == 0))
		{
			LeftNodeBox->AddSlot()
			.Padding(FMargin(NegativeHPad + ExtraPad, 0.0f, 0.0f, 0.0f))
			.AutoHeight()
			.HAlign(HAlign_Left)
			[
				CreatePreviewWidget()
			];
		}
		else if (LeftPinCount > RightPinCount)
		{
			RightNodeBox->AddSlot()
			.Padding(FMargin(NegativeHPad + ExtraPad, 0.0f, 0.0f, 0.0f))
			.AutoHeight()
			.HAlign(HAlign_Right)
			[
				CreatePreviewWidget()
			];
		}
		else
		{
			MainBox->AddSlot()
			.Padding(Settings->GetNonPinNodeBodyPadding())
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					CreatePreviewWidget()
				]
			];
		}
	}

	// Preview of Substrate nodes topology
	if (Substrate::IsSubstrateEnabled() && MaterialNode)
	{
		TArray<FGuid> Guids;
		if (MaterialNode->MaterialExpression->IsA(UMaterialExpressionSubstrateBSDF::StaticClass()))
		{
			const UMaterialExpression* SubstrateExpression = (const UMaterialExpression*)MaterialNode->MaterialExpression;
			Guids.Add(SubstrateExpression->MaterialExpressionGuid);
		}
		else if (MaterialNode->MaterialExpression->IsA(UMaterialExpressionMaterialFunctionCall::StaticClass()))
		{
			UMaterialExpressionMaterialFunctionCall* FunctionCall = (UMaterialExpressionMaterialFunctionCall*)MaterialNode->MaterialExpression;
			const uint32 OutputCount = FunctionCall->FunctionOutputs.Num();
			for (uint32 OutputIndex = 0; OutputIndex < OutputCount; ++OutputIndex)
			{
				if (FunctionCall->IsResultSubstrateMaterial(OutputIndex))
				{
					FSubstrateMaterialInfo SubstrateMaterialInfo(true/*bGatherGuids*/);
					FunctionCall->GatherSubstrateMaterialInfo(SubstrateMaterialInfo, OutputIndex);
					Guids = SubstrateMaterialInfo.GetGuids();
					break;
				}
			}
		}

		if (Guids.Num() > 0)
		{
			if (const UMaterialExpression* SubstrateExpression = (const UMaterialExpression*)MaterialNode->MaterialExpression)
			{		
				if (UMaterial* MaterialForStats = SubstrateExpression->Material)
				{
					if (const FMaterialResource* MaterialResource = MaterialForStats->GetMaterialResource(GMaxRHIFeatureLevel))
					{
						if (FMaterialShaderMap* ShaderMap = MaterialResource->GetGameThreadShaderMap())
						{
							const FSubstrateMaterialCompilationOutput& CompilationOutput = ShaderMap->GetSubstrateMaterialCompilationOutput();
							MainBox->AddSlot()
							.Padding(Settings->GetNonPinNodeBodyPadding())
							.AutoHeight()
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Center)
								[							
									FSubstrateWidget::ProcessOperator(CompilationOutput, Guids)
								]
							];
						}
					}
	
				}
			}
		}
	}
}

void SGraphNodeMaterialBase::SetDefaultTitleAreaWidget(TSharedRef<SOverlay> DefaultTitleAreaWidget)
{
	if (!MaterialNode->MaterialExpression->bHidePreviewWindow)
	{
		DefaultTitleAreaWidget->AddSlot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(FMargin(5))
		[
			SNew(SCheckBox)
			.OnCheckStateChanged( this, &SGraphNodeMaterialBase::OnExpressionPreviewChanged )
			.IsChecked( IsExpressionPreviewChecked() )
			.Cursor(EMouseCursor::Default)
			.Style(FAppStyle::Get(), "Graph.Node.AdvancedView")
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SImage)
					. Image(GetExpressionPreviewArrow())
				]
			]
		];
	}
}

TSharedRef<SWidget> SGraphNodeMaterialBase::CreateNodeContentArea()
{
	// NODE CONTENT AREA
	return SNew(SBorder)
		.BorderImage( FAppStyle::GetBrush("NoBorder") )
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.FillWidth(1.0f)
			[
				// LEFT
				SAssignNew(LeftNodeBox, SVerticalBox)
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			[
				// RIGHT
				SAssignNew(RightNodeBox, SVerticalBox)
			]
		];
}

void SGraphNodeMaterialBase::OnAdvancedViewChanged(const ECheckBoxState NewCheckedState)
{
	SGraphNode::OnAdvancedViewChanged(NewCheckedState);

	// Update the graph node so that the preview is recreated to update its position
	UpdateGraphNode();
}

TSharedRef<SWidget> SGraphNodeMaterialBase::CreatePreviewWidget()
{
	PreviewViewport.Reset();

	// if this node should currently show a preview
	if (!MaterialNode->MaterialExpression->bHidePreviewWindow && !MaterialNode->MaterialExpression->bCollapsed)
	{
		const float ExpressionPreviewSize = 106.0f;
		const float CentralPadding = 5.0f;

		TSharedPtr<SViewport> ViewportWidget = 
			SNew( SViewport )
			.RenderDirectlyToWindow(true)
			.EnableGammaCorrection(false);

		PreviewViewport = MakeShareable(new FPreviewViewport(MaterialNode));

		// The viewport widget needs an interface so it knows what should render
		ViewportWidget->SetViewportInterface( PreviewViewport.ToSharedRef() );

		return SNew(SBox)
			.WidthOverride(ExpressionPreviewSize)
			.HeightOverride(ExpressionPreviewSize)
			.MaxAspectRatio(1.0f)
			.MaxDesiredHeight(ExpressionPreviewSize)
			.Visibility(ExpressionPreviewVisibility())
			[
				SNew(SBorder)
				.Padding(CentralPadding)
				.BorderImage( FAppStyle::GetBrush("NoBorder") )
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						ViewportWidget.ToSharedRef()
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Bottom)
					.Padding(0.0f,0.0f,8.0f,0.0f)
					[
						SNew(STextBlock)
						.ShadowColorAndOpacity(FLinearColor::Black)
						.ShadowOffset(FVector2D(1.0f, 1.0f))
						.Text(this, &SGraphNodeMaterialBase::ExpressionPreviewOverlayText)
					]
				]
			];
	}

	return SNullWidget::NullWidget;
}

FText SGraphNodeMaterialBase::ExpressionPreviewOverlayText() const
{
	UMaterialExpression* MaterialExpression = MaterialNode->MaterialExpression;
	return MaterialNode->MaterialExpression->GetPreviewOverlayText();
}

EVisibility SGraphNodeMaterialBase::ExpressionPreviewVisibility() const
{
	UMaterialExpression* MaterialExpression = MaterialNode->MaterialExpression;
	const bool bShowPreview = !MaterialExpression->bHidePreviewWindow && !MaterialExpression->bCollapsed;
	return bShowPreview ? EVisibility::Visible : EVisibility::Collapsed;
}

void SGraphNodeMaterialBase::OnExpressionPreviewChanged( const ECheckBoxState NewCheckedState )
{
	UMaterialExpression* MaterialExpression = MaterialNode->MaterialExpression;
	const bool bCollapsed = (NewCheckedState != ECheckBoxState::Checked);
	if (MaterialExpression->bCollapsed != bCollapsed)
	{
		UMaterialGraph* MaterialGraph = CastChecked<UMaterialGraph>(MaterialNode->GetGraph());
		MaterialGraph->ToggleCollapsedDelegate.ExecuteIfBound(MaterialExpression);

		// Update the graph node so that preview viewport is created
		UpdateGraphNode();
	}
}

ECheckBoxState SGraphNodeMaterialBase::IsExpressionPreviewChecked() const
{
	return MaterialNode->MaterialExpression->bCollapsed ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
}

const FSlateBrush* SGraphNodeMaterialBase::GetExpressionPreviewArrow() const
{
	return FAppStyle::GetBrush(MaterialNode->MaterialExpression->bCollapsed ? TEXT("Icons.ChevronDown") : TEXT("Icons.ChevronUp"));
}

void SGraphNodeMaterialBase::PopulateMetaTag(FGraphNodeMetaData* TagMeta) const
{
	if (GraphNode != nullptr)
	{
		UMaterialGraph* OuterGraph = MaterialNode->GetTypedOuter<UMaterialGraph>();
		if ((OuterGraph != nullptr) && (MaterialNode->MaterialExpression != nullptr) )
		{
			TagMeta->OuterName = OuterGraph->OriginalMaterialFullName;
			TagMeta->GUID = MaterialNode->MaterialExpression->MaterialExpressionGuid;
			TagMeta->Tag = FName(*FString::Printf(TEXT("MaterialExprNode_%s_%s"), *TagMeta->OuterName, *TagMeta->GUID.ToString()));
		}
		TagMeta->FriendlyName = FString::Printf(TEXT("%s expression node in %s"), *GraphNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString(), *TagMeta->OuterName);		
	}
}
