// Copyright Epic Games, Inc. All Rights Reserved.

/**
* Adapted from SGraphNodeK2Var.cpp
*/

#include "SPCGEditorGraphVarNode.h"

#include "PCGEditorGraphNodeReroute.h"

#include "GraphEditorSettings.h"
#include "SCommentBubble.h"
#include "SGraphNode.h"
#include "SlotBase.h"
#include "EdGraph/EdGraphNode.h"
#include "TutorialMetaData.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"

class SWidget;
struct FSlateBrush;

void SPCGEditorGraphVarNode::Construct(const FArguments& InArgs, UPCGEditorGraphNodeBase* InNode)
{
	SPCGEditorGraphNode::Construct(SPCGEditorGraphNode::FArguments{}, InNode);
}

FSlateColor SPCGEditorGraphVarNode::GetVariableColor() const
{
	return GraphNode->GetNodeTitleColor();
}

void SPCGEditorGraphVarNode::UpdateGraphNode()
{
	InputPins.Empty();
	OutputPins.Empty();

	// Reset variables that are going to be exposed, in case we are refreshing an already setup node.
	RightNodeBox.Reset();
	LeftNodeBox.Reset();

	FMargin ContentAreaMargin = FMargin(0.0f, 4.0f);

	SetupErrorReporting();

	const bool bNeedsTitle = (Cast<UPCGEditorGraphNodeNamedRerouteBase>(GraphNode) != nullptr);

	TSharedPtr<SWidget> TitleArea = SNullWidget::NullWidget.ToSharedPtr();

	if (bNeedsTitle)
	{
		const bool bNeedsPadding = (Cast<UPCGEditorGraphNodeNamedRerouteUsage>(GraphNode) != nullptr);
		FMargin CustomPadding = FMargin(Settings->PaddingLeftOfOutput, Settings->PaddingAbovePin, 0, Settings->PaddingBelowPin);

		TSharedPtr<SNodeTitle> NodeTitle = SNew(SNodeTitle, GraphNode);
		SAssignNew(TitleArea, SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.Padding(bNeedsPadding ? CustomPadding : FMargin())
			[
				CreateTitleWidget(NodeTitle)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				NodeTitle.ToSharedRef()
			];
	}

	// Setup a meta tag for this node
	FGraphNodeMetaData TagMeta(TEXT("Graphnode"));
	PopulateMetaTag(&TagMeta);

	//             ________________
	//            | (>) L |  R (>) |
	//            | (>) E |  I (>) |
	//            | (>) F |  G (>) |
	//            | (>) T |  H (>) |
	//            |       |  T (>) |
	//            |_______|________|
	//
	this->ContentScale.Bind( this, &SGraphNode::GetContentScale );
	this->GetOrAddSlot( ENodeZone::Center )
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		[
			SNew(SOverlay)
			.AddMetaData<FGraphNodeMetaData>(TagMeta)
			+ SOverlay::Slot()
			[
				SNew(SImage)
				.Image( FAppStyle::GetBrush("Graph.VarNode.Body") )
			]
			+ SOverlay::Slot()
			.VAlign(VAlign_Top)
			[
				SNew(SImage)
				.Image( FAppStyle::GetBrush("Graph.VarNode.ColorSpill") )
				.ColorAndOpacity( this, &SPCGEditorGraphVarNode::GetVariableColor )
			]
			+ SOverlay::Slot()
			[
				SNew(SImage)
				.Image( FAppStyle::GetBrush("Graph.VarNode.Gloss") )
			]
			+ SOverlay::Slot()
			.Padding( ContentAreaMargin )
			[
				// NODE CONTENT AREA
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.FillWidth(1.0f)
				.Padding( FMargin(2,0) )
				[
					// LEFT
					SAssignNew(LeftNodeBox, SVerticalBox)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					TitleArea.ToSharedRef()
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.Padding( FMargin(2,0) )
				[
					// RIGHT
					SAssignNew(RightNodeBox, SVerticalBox)
				]
			]
		]
		+SVerticalBox::Slot()
		.VAlign(VAlign_Top)
		.AutoHeight() 
		.Padding( FMargin(5.0f, 1.0f) )
		[
			ErrorReporting->AsWidget()
		]
	];

	float VerticalPaddingAmount = 0.0f;

	if (VerticalPaddingAmount > 0.0f)
	{
		LeftNodeBox->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			SNew(SSpacer).Size(FVector2D(0.0f, VerticalPaddingAmount))
		];

		RightNodeBox->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			SNew(SSpacer).Size(FVector2D(0.0f, VerticalPaddingAmount))
		];
	}
	// Create comment bubble
	TSharedPtr<SCommentBubble> CommentBubble;
	const FSlateColor CommentColor = GetDefault<UGraphEditorSettings>()->DefaultCommentNodeTitleColor;

	SAssignNew( CommentBubble, SCommentBubble )
	.GraphNode( GraphNode )
	.Text( this, &SGraphNode::GetNodeComment )
	.OnTextCommitted( this, &SGraphNode::OnCommentTextCommitted )
	.ColorAndOpacity( CommentColor )
	.AllowPinning( true )
	.EnableTitleBarBubble( true )
	.EnableBubbleCtrls( true )
	.GraphLOD( this, &SGraphNode::GetCurrentLOD )
	.IsGraphNodeHovered( this, &SGraphNode::IsHovered );

	GetOrAddSlot( ENodeZone::TopCenter )
	.SlotOffset( TAttribute<FVector2D>( CommentBubble.Get(), &SCommentBubble::GetOffset ))
	.SlotSize( TAttribute<FVector2D>( CommentBubble.Get(), &SCommentBubble::GetSize ))
	.AllowScaling( TAttribute<bool>( CommentBubble.Get(), &SCommentBubble::IsScalingAllowed ))
	.VAlign( VAlign_Top )
	[
		CommentBubble.ToSharedRef()
	];

	// Create widgets for each of the real pins
	CreatePinWidgets();
}

const FSlateBrush* SPCGEditorGraphVarNode::GetShadowBrush(bool bSelected) const
{
	return bSelected ? FAppStyle::GetBrush(TEXT("Graph.VarNode.ShadowSelected")) : FAppStyle::GetBrush(TEXT("Graph.VarNode.Shadow"));
}

void SPCGEditorGraphVarNode::GetDiffHighlightBrushes(const FSlateBrush*& BackgroundOut, const FSlateBrush*& ForegroundOut) const
{
	BackgroundOut = FAppStyle::GetBrush(TEXT("Graph.VarNode.DiffHighlight"));
	ForegroundOut = FAppStyle::GetBrush(TEXT("Graph.VarNode.DiffHighlightShading"));
}

