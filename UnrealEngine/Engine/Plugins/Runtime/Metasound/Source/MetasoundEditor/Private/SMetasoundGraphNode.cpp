// Copyright Epic Games, Inc. All Rights Reserved.
#include "SMetasoundGraphNode.h"

#include "AudioParameterControllerInterface.h"
#include "Components/AudioComponent.h"
#include "Styling/AppStyle.h"
#include "GraphEditorSettings.h"
#include "IAudioParameterTransmitter.h"
#include "IDocumentation.h"
#include "KismetPins/SGraphPinBool.h"
#include "KismetPins/SGraphPinExec.h"
#include "KismetPins/SGraphPinInteger.h"
#include "KismetPins/SGraphPinNum.h"
#include "KismetPins/SGraphPinObject.h"
#include "KismetPins/SGraphPinString.h"
#include "MetasoundEditorGraph.h"
#include "MetasoundEditorGraphBuilder.h"
#include "MetasoundEditorGraphInputNode.h"
#include "MetasoundEditorGraphMemberDefaults.h"
#include "MetasoundEditorGraphNode.h"
#include "MetasoundEditorGraphSchema.h"
#include "MetasoundEditorModule.h"
#include "MetasoundFrontendArchetypeRegistry.h"
#include "MetasoundFrontendNodeTemplateRegistry.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundTrace.h"
#include "MetasoundTrigger.h"
#include "NodeFactory.h"
#include "NodeTemplates/MetasoundFrontendNodeTemplateReroute.h"
#include "PropertyCustomizationHelpers.h"
#include "SAudioRadialSlider.h"
#include "SAudioSlider.h"
#include "SCommentBubble.h"
#include "ScopedTransaction.h"
#include "SGraphNode.h"
#include "SGraphPinComboBox.h"
#include "SLevelOfDetailBranchNode.h"
#include "SMetasoundGraphEnumPin.h"
#include "SMetasoundGraphPin.h"
#include "SMetasoundPinValueInspector.h"
#include "SPinTypeSelector.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateStyleRegistry.h"
#include "TutorialMetaData.h"
#include "UObject/ScriptInterface.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

#define LOCTEXT_NAMESPACE "MetasoundEditor"


namespace Metasound
{
	namespace Editor
	{
		SMetaSoundGraphNode::~SMetaSoundGraphNode()
		{
			UMetasoundEditorGraphNode& Node = GetMetaSoundNode();
			if (UMetasoundEditorGraphMemberNode* MemberNode = Cast<UMetasoundEditorGraphMemberNode>(&Node))
			{
				if (UMetasoundEditorGraphMember* GraphMember = MemberNode->GetMember())
				{
					if (UMetasoundEditorGraphMemberDefaultFloat* DefaultFloat = Cast<UMetasoundEditorGraphMemberDefaultFloat>(GraphMember->GetLiteral()))
					{
						DefaultFloat->OnDefaultValueChanged.Remove(InputSliderOnValueChangedDelegateHandle);
						DefaultFloat->OnRangeChanged.Remove(InputSliderOnRangeChangedDelegateHandle);
					}
				}
			}

			if (bIsInputWidgetTransacting)
			{
				GEditor->EndTransaction();
				UE_LOG(LogMetaSound, Warning, TEXT("Unmatched MetaSound editor widget transaction."));
			}
		}

		bool SMetaSoundGraphNode::IsVariableAccessor() const
		{
			return ClassType == EMetasoundFrontendClassType::VariableAccessor
				|| ClassType == EMetasoundFrontendClassType::VariableDeferredAccessor;
		}

		bool SMetaSoundGraphNode::IsVariableMutator() const
		{
			return ClassType == EMetasoundFrontendClassType::VariableMutator;
		}

		const FSlateBrush* SMetaSoundGraphNode::GetShadowBrush(bool bSelected) const
		{
			if (IsVariableAccessor() || IsVariableMutator())
			{
				return bSelected ? FAppStyle::GetBrush(TEXT("Graph.VarNode.ShadowSelected")) : FAppStyle::GetBrush(TEXT("Graph.VarNode.Shadow"));
			}

			return SGraphNode::GetShadowBrush(bSelected);
		}

		void SMetaSoundGraphNode::Construct(const FArguments& InArgs, class UEdGraphNode* InNode)
		{
			GraphNode = InNode;
			Frontend::FConstNodeHandle NodeHandle = GetMetaSoundNode().GetConstNodeHandle();
			ClassType = NodeHandle->GetClassMetadata().GetType();

			SetCursor(EMouseCursor::CardinalCross);
			UpdateGraphNode();
		}

		void SMetaSoundGraphNode::ExecuteTrigger(UMetasoundEditorGraphMemberDefaultLiteral& Literal)
		{
			UMetasoundEditorGraphMember* Member = Cast<UMetasoundEditorGraphMember>(Literal.GetOuter());
			if (!ensure(Member))
			{
				return;
			}

			if (UMetasoundEditorGraph* Graph = Member->GetOwningGraph())
			{
				if (!Graph->IsPreviewing())
				{
					TSharedPtr<FEditor> MetaSoundEditor = FGraphBuilder::GetEditorForMetasound(Graph->GetMetasoundChecked());
					if (!MetaSoundEditor.IsValid())
					{
						return;
					}
					MetaSoundEditor->Play();
				}
			}

			if (UAudioComponent* PreviewComponent = GEditor->GetPreviewAudioComponent())
			{
				PreviewComponent->SetTriggerParameter(Member->GetMemberName());
			}
		}

		TAttribute<EVisibility> SMetaSoundGraphNode::GetSimulationVisibilityAttribute() const
		{
			return TAttribute<EVisibility>::CreateLambda([this]()
			{
				using namespace Frontend;

				if (const UMetasoundEditorGraphMemberNode* Node = Cast<UMetasoundEditorGraphMemberNode>(&GetMetaSoundNode()))
				{
					if (const UMetasoundEditorGraphVertex* Vertex = Cast<UMetasoundEditorGraphVertex>(Node->GetMember()))
					{
						if (const UMetasoundEditorGraph* Graph = Vertex->GetOwningGraph())
						{
							if (!Graph->IsPreviewing())
							{
								return EVisibility::Hidden;
							}
						}

						// Don't enable trigger simulation widget if its a trigger provided by an interface
						// that does not support transmission.
						const FInterfaceRegistryKey Key = GetInterfaceRegistryKey(Vertex->GetInterfaceVersion());
						const IInterfaceRegistryEntry* Entry = IInterfaceRegistry::Get().FindInterfaceRegistryEntry(Key);
						if (Entry && Entry->GetRouterName() != Audio::IParameterTransmitter::RouterName)
						{
							return EVisibility::Hidden;
						}
						else if (const UMetasoundEditorGraphMemberDefaultLiteral* Literal = Vertex->GetLiteral())
						{
							if (!Literal)
							{
								return EVisibility::Hidden;
							}
						}
					}
				}

				return EVisibility::Visible;
			});
		}

		TSharedRef<SWidget> SMetaSoundGraphNode::CreateTriggerSimulationWidget(UMetasoundEditorGraphMemberDefaultLiteral& InputLiteral, TAttribute<EVisibility>&& InVisibility, TAttribute<bool>&& InEnablement, const FText* InToolTip)
		{
			const FText ToolTip = InToolTip
				? *InToolTip
				: LOCTEXT("MetasoundGraphNode_TriggerTestToolTip", "Executes trigger if currently previewing MetaSound.");

			TSharedPtr<SButton> SimulationButton;
			TSharedRef<SWidget> SimulationWidget = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(2.0f, 0.0f, 0.0f, 0.0f)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SAssignNew(SimulationButton, SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked_Lambda([LiteralPtr = TWeakObjectPtr<UMetasoundEditorGraphMemberDefaultLiteral>(&InputLiteral)]()
				{
					if (LiteralPtr.IsValid())
					{
						ExecuteTrigger(*LiteralPtr.Get());
					}
					return FReply::Handled();
				})
				.ToolTipText(ToolTip)
				.ForegroundColor(FSlateColor::UseForeground())
				.ContentPadding(0)
				.IsFocusable(false)
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.CircleArrowDown"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
				.Visibility(MoveTemp(InVisibility))
			];

			SimulationButton->SetEnabled(MoveTemp(InEnablement));

			return SimulationWidget;
		}

		void SMetaSoundGraphNode::CreateInputSideAddButton(TSharedPtr<SVerticalBox> InputBox)
		{
			TSharedRef<SWidget> AddPinButton = AddPinButtonContent(
				LOCTEXT("MetasoundGraphNode_AddPinInputButton", "Add Input"),
				LOCTEXT("MetasoundGraphNode_AddPinInputButton_Tooltip", "Add an input to the parent Metasound node.")
			);

			FMargin AddPinPadding = Settings->GetOutputPinPadding();
			AddPinPadding.Top += 6.0f;

			InputBox->AddSlot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.Padding(AddPinPadding)
			[
				AddPinButton
			];
		}

		void SMetaSoundGraphNode::CreateOutputSideAddButton(TSharedPtr<SVerticalBox> OutputBox)
		{
			TSharedRef<SWidget> AddPinButton = AddPinButtonContent(
				LOCTEXT("MetasoundGraphNode_AddPinOutputButton", "Add Output"),
				LOCTEXT("MetasoundGraphNode_AddPinOutputButton_Tooltip", "Add an output to the parent Metasound node.")
			);

			FMargin AddPinPadding = Settings->GetOutputPinPadding();
			AddPinPadding.Top += 6.0f;

			OutputBox->AddSlot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.Padding(AddPinPadding)
			[
				AddPinButton
			];
		}

		UMetasoundEditorGraphNode& SMetaSoundGraphNode::GetMetaSoundNode()
		{
			return *CastChecked<UMetasoundEditorGraphNode>(GraphNode);
		}

		const UMetasoundEditorGraphNode& SMetaSoundGraphNode::GetMetaSoundNode() const
		{
			check(GraphNode);
			return *Cast<UMetasoundEditorGraphNode>(GraphNode);
		}

		TSharedPtr<SGraphPin> SMetaSoundGraphNode::CreatePinWidget(UEdGraphPin* InPin) const
		{
			using namespace Frontend;

			TSharedPtr<SGraphPin> PinWidget;

			if (const UMetasoundEditorGraphSchema* GraphSchema = Cast<const UMetasoundEditorGraphSchema>(InPin->GetSchema()))
			{
				// Don't show default value field for container types
				if (InPin->PinType.ContainerType != EPinContainerType::None)
				{
					PinWidget = SNew(SMetasoundGraphPin, InPin);
				}

				else if (InPin->PinType.PinCategory == FGraphBuilder::PinCategoryAudio)
				{
					PinWidget = SNew(SMetasoundGraphPin, InPin);
				}

				else if (InPin->PinType.PinCategory == FGraphBuilder::PinCategoryBoolean)
				{
					PinWidget = SNew(SMetasoundGraphPinBool, InPin);
				}
				
				else if (InPin->PinType.PinCategory == FGraphBuilder::PinCategoryFloat
					|| InPin->PinType.PinCategory == FGraphBuilder::PinCategoryTime)
				{
					PinWidget = SNew(SMetasoundGraphPinFloat, InPin);
				}

				else if (InPin->PinType.PinCategory == FGraphBuilder::PinCategoryInt32)
				{
					if (SMetasoundGraphEnumPin::FindEnumInterfaceFromPin(InPin))
					{
						PinWidget = SNew(SMetasoundGraphEnumPin, InPin);
					}
					else
					{
						PinWidget = SNew(SMetasoundGraphPinInteger, InPin);
					}
				}

				else if (InPin->PinType.PinCategory == FGraphBuilder::PinCategoryObject)
				{
					PinWidget = SNew(SMetasoundGraphPinObject, InPin);
				}

				else if (InPin->PinType.PinCategory == FGraphBuilder::PinCategoryString)
				{
					PinWidget = SNew(SMetasoundGraphPinString, InPin);
				}

				else if (InPin->PinType.PinCategory == FGraphBuilder::PinCategoryTrigger)
				{
					PinWidget = SNew(SMetasoundGraphPin, InPin);

					const FSlateBrush& PinConnectedBrush = Editor::Style::GetSlateBrushSafe("MetasoundEditor.Graph.TriggerPin.Connected");
					const FSlateBrush& PinDisconnectedBrush = Editor::Style::GetSlateBrushSafe("MetasoundEditor.Graph.TriggerPin.Disconnected");
					PinWidget->SetCustomPinIcon(&PinConnectedBrush, &PinDisconnectedBrush);
				}
			}

			if (!PinWidget.IsValid())
			{
				PinWidget = SNew(SMetasoundGraphPin, InPin);
			}

			return PinWidget;
		}

		void SMetaSoundGraphNode::CreateStandardPinWidget(UEdGraphPin* InPin)
		{
			const bool bShowPin = ShouldPinBeHidden(InPin);
			if (bShowPin)
			{
				TSharedPtr<SGraphPin> NewPin = CreatePinWidget(InPin);
				check(NewPin.IsValid());

				Frontend::FConstNodeHandle NodeHandle = GetMetaSoundNode().GetConstNodeHandle();
				if (InPin->Direction == EGPD_Input)
				{
					if (!NodeHandle->GetClassStyle().Display.bShowInputNames)
					{
						NewPin->SetShowLabel(false);
					}
				}
				else if (InPin->Direction == EGPD_Output)
				{
					if (!NodeHandle->GetClassStyle().Display.bShowOutputNames)
					{
						NewPin->SetShowLabel(false);
					}
				}

				AddPin(NewPin.ToSharedRef());
			}
		}

		TSharedRef<SWidget> SMetaSoundGraphNode::CreateTitleWidget(TSharedPtr<SNodeTitle> NodeTitle)
		{
			Frontend::FConstNodeHandle NodeHandle = GetMetaSoundNode().GetConstNodeHandle();
			if (!NodeHandle->GetClassStyle().Display.bShowName)
			{
				return SNullWidget::NullWidget;
			}

			TSharedPtr<SHorizontalBox> TitleBoxWidget = SNew(SHorizontalBox);

			FSlateIcon NodeIcon = GetMetaSoundNode().GetNodeTitleIcon();
			if (const FSlateBrush* IconBrush = NodeIcon.GetIcon())
			{
				if (IconBrush != FStyleDefaults::GetNoBrush())
				{
					TSharedPtr<SImage> Image;
					TitleBoxWidget->AddSlot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Right)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.Padding(0.0f, 0.0f, 4.0f, 0.0f)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Right)
						[
							SAssignNew(Image, SImage)
						]
					];
					Image->SetColorAndOpacity(TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(GetNodeTitleColorOverride()); }));
					Image->SetImage(IconBrush);
				}
			}

			TitleBoxWidget->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SGraphNode::CreateTitleWidget(NodeTitle)
			];

			InlineEditableText->SetColorAndOpacity(TAttribute<FLinearColor>::Create(TAttribute<FLinearColor>::FGetter::CreateSP(this, &SMetaSoundGraphNode::GetNodeTitleColorOverride)));

			return TitleBoxWidget.ToSharedRef();
		}

		void SMetaSoundGraphNode::GetOverlayBrushes(bool bSelected, const FVector2D WidgetSize, TArray<FOverlayBrushInfo>& Brushes) const
		{
			FName CornerIcon = GetMetaSoundNode().GetCornerIcon();
			if (CornerIcon != NAME_None)
			{

				if (const FSlateBrush* Brush = FAppStyle::GetBrush(CornerIcon))
				{
					FOverlayBrushInfo OverlayInfo = { Brush };

					// Logic copied from SGraphNodeK2Base
					OverlayInfo.OverlayOffset.X = (WidgetSize.X - (OverlayInfo.Brush->ImageSize.X / 2.f)) - 3.f;
					OverlayInfo.OverlayOffset.Y = (OverlayInfo.Brush->ImageSize.Y / -2.f) + 2.f;
					Brushes.Add(MoveTemp(OverlayInfo));
				}
			}
		}

		FLinearColor SMetaSoundGraphNode::GetNodeTitleColorOverride() const
		{
			FLinearColor ReturnTitleColor = GraphNode->IsDeprecated() ? FLinearColor::Red : GetNodeObj()->GetNodeTitleColor();

			if (!GraphNode->IsNodeEnabled() || GraphNode->IsDisplayAsDisabledForced() || GraphNode->IsNodeUnrelated())
			{
				ReturnTitleColor *= FLinearColor(0.5f, 0.5f, 0.5f, 0.4f);
			}
			else
			{
				ReturnTitleColor.A = FadeCurve.GetLerp();
			}

			return ReturnTitleColor;
		}

		void SMetaSoundGraphNode::SetDefaultTitleAreaWidget(TSharedRef<SOverlay> DefaultTitleAreaWidget)
		{
			SGraphNode::SetDefaultTitleAreaWidget(DefaultTitleAreaWidget);

			Metasound::Frontend::FNodeHandle NodeHandle = GetMetaSoundNode().GetNodeHandle();
			if (NodeHandle->GetClassStyle().Display.bShowName)
			{
				DefaultTitleAreaWidget->ClearChildren();
				TSharedPtr<SNodeTitle> NodeTitle = SNew(SNodeTitle, GraphNode);

				DefaultTitleAreaWidget->AddSlot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Fill)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("NoBorder"))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.AutoHeight()
								.HAlign(HAlign_Center)
								[
									CreateTitleWidget(NodeTitle)
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									NodeTitle.ToSharedRef()
								]
							]
						]
					]
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.Padding(0, 0, 5, 0)
					.AutoWidth()
					[
						CreateTitleRightWidget()
					]
				];

				DefaultTitleAreaWidget->AddSlot()
				.VAlign(VAlign_Top)
				[
					SNew(SBorder)
					.Visibility(EVisibility::HitTestInvisible)
					.BorderImage( FAppStyle::GetBrush( "Graph.Node.TitleHighlight" ) )
					.BorderBackgroundColor( this, &SGraphNode::GetNodeTitleIconColor )
					[
						SNew(SSpacer)
						.Size(FVector2D(20,20))
					]
				];

			}
			else
			{
				DefaultTitleAreaWidget->SetVisibility(EVisibility::Collapsed);
			}
		}

		void SMetaSoundGraphNode::MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty)
		{
			SGraphNode::MoveTo(NewPosition, NodeFilter, bMarkDirty);

			UMetasoundEditorGraphNode& Node = GetMetaSoundNode();
			Node.GetMetasoundChecked().Modify();
			Node.SetNodeLocation(NewPosition);
		}

		const FSlateBrush* SMetaSoundGraphNode::GetNodeBodyBrush() const
		{
			// TODO: Add tweak & add custom bodies
			if (GraphNode)
			{
				switch (ClassType)
				{
					case EMetasoundFrontendClassType::Variable:
					case EMetasoundFrontendClassType::VariableAccessor:
					case EMetasoundFrontendClassType::VariableDeferredAccessor:
					case EMetasoundFrontendClassType::VariableMutator:
					{
						return FAppStyle::GetBrush("Graph.VarNode.Body");
					}
					break;

					case EMetasoundFrontendClassType::Input:
					case EMetasoundFrontendClassType::Output:
					default:
					{
					}
					break;
				}
			}

			return FAppStyle::GetBrush("Graph.Node.Body");
		}

		EVisibility SMetaSoundGraphNode::IsAddPinButtonVisible() const
		{
			EVisibility DefaultVisibility = SGraphNode::IsAddPinButtonVisible();
			if (DefaultVisibility == EVisibility::Visible)
			{
				if (!GetMetaSoundNode().CanAddInputPin())
				{
					return EVisibility::Collapsed;
				}
			}

			return DefaultVisibility;
		}

		FReply SMetaSoundGraphNode::OnAddPin()
		{
			GetMetaSoundNode().CreateInputPin();

			return FReply::Handled();
		}

		FName SMetaSoundGraphNode::GetLiteralDataType() const
		{
			using namespace Frontend;

			FName TypeName;

			// Just take last type.  If more than one, all types are the same.
			const UMetasoundEditorGraphNode& Node = GetMetaSoundNode();
			Node.GetNodeHandle()->IterateConstOutputs([InTypeName = &TypeName](FConstOutputHandle OutputHandle)
			{
				*InTypeName = OutputHandle->GetDataType();
			});

			return TypeName;
		}

		TSharedRef<SWidget> SMetaSoundGraphNode::CreateTitleRightWidget()
		{
			using namespace Frontend;

			const FName TypeName = GetLiteralDataType();
			if (TypeName == Metasound::GetMetasoundDataTypeName<Metasound::FTrigger>())
			{
				if (UMetasoundEditorGraphMemberNode* Node = Cast<UMetasoundEditorGraphMemberNode>(&GetMetaSoundNode()))
				{
					if (UMetasoundEditorGraphInput* Input = Cast<UMetasoundEditorGraphInput>(Node->GetMember()))
					{
						if (UMetasoundEditorGraphMemberDefaultLiteral* Literal = Input->GetLiteral())
						{
							TAttribute<EVisibility> SimVisibility = GetSimulationVisibilityAttribute();
							TAttribute<bool> SimEnablement = true;
							return CreateTriggerSimulationWidget(*Literal, MoveTemp(SimVisibility), MoveTemp(SimEnablement));
						}
					}
				}
			}

			return SGraphNode::CreateTitleRightWidget();
		}

		UMetasoundEditorGraphMember* SMetaSoundGraphNode::GetMetaSoundMember()
		{
			if (UMetasoundEditorGraphMemberNode* MemberNode = GetMetaSoundMemberNode())
			{
				return MemberNode->GetMember();
			}

			return nullptr;
		}

		UMetasoundEditorGraphMemberNode* SMetaSoundGraphNode::GetMetaSoundMemberNode()
		{
			return Cast<UMetasoundEditorGraphMemberNode>(&GetMetaSoundNode());
		}

		TSharedRef<SWidget> SMetaSoundGraphNode::CreateNodeContentArea()
		{
			using namespace Frontend;

			FConstNodeHandle NodeHandle = GetMetaSoundNode().GetConstNodeHandle();
			const FMetasoundFrontendClassStyleDisplay& StyleDisplay = NodeHandle->GetClassStyle().Display;
			TSharedPtr<SHorizontalBox> ContentBox = SNew(SHorizontalBox);
			TSharedPtr<SWidget> OuterContentBox; // currently only used for input float nodes to accommodate the input widget

			// If editable float input node and not constructor input, check if custom widget required
			bool bShowInputWidget = false;
			if (UMetasoundEditorGraphInput* GraphMember = Cast<UMetasoundEditorGraphInput>(GetMetaSoundMember()))
			{
				const UMetasoundEditorGraph* OwningGraph = GraphMember->GetOwningGraph();
				if (OwningGraph && OwningGraph->IsEditable() && GraphMember->GetVertexAccessType() == EMetasoundFrontendVertexAccessType::Reference)
				{
					UMetasoundEditorGraphMemberDefaultFloat* DefaultFloat = Cast<UMetasoundEditorGraphMemberDefaultFloat>(GraphMember->GetLiteral());
					if (DefaultFloat && DefaultFloat->WidgetType != EMetasoundMemberDefaultWidget::None)
					{
						constexpr float WidgetPadding = 3.0f;
						static const FVector2D SliderDesiredSizeVertical = FVector2D(30.0f, 250.0f);
						static const FVector2D RadialSliderDesiredSize = FVector2D(56.0f, 87.0f);

						bShowInputWidget = true;

						auto OnValueChangedLambda = [DefaultFloat, GraphMember, this](float Value)
						{
							if (InputWidget.IsValid())
							{
								if (!bIsInputWidgetTransacting)
								{
									GEditor->BeginTransaction(LOCTEXT("MetasoundGraphNode_MetasoundSetInputDefault", "Set MetaSound Input Default"));
									bIsInputWidgetTransacting = true;
								}
								GraphMember->GetOwningGraph()->GetMetasound()->Modify();
								DefaultFloat->Modify();

								constexpr bool bPostTransaction = true;
								float Output = InputWidget->GetOutputValue(Value);
								DefaultFloat->SetDefault(Output);
								GraphMember->UpdateFrontendDefaultLiteral(bPostTransaction);
							}
						};

						auto OnValueCommittedLambda = [DefaultFloat, GraphMember, this](float Value)
						{
							if (InputWidget.IsValid())
							{
								bool bPostTransaction = false;
								float Output = InputWidget->GetOutputValue(Value);
								DefaultFloat->SetDefault(Output);

								if (bIsInputWidgetTransacting)
								{
									GEditor->EndTransaction();
									bIsInputWidgetTransacting = false;
								}
								else
								{
									bPostTransaction = true;
									UE_LOG(LogMetaSound, Warning, TEXT("Unmatched MetaSound editor widget transaction."));
								}

								GraphMember->UpdateFrontendDefaultLiteral(bPostTransaction);

								if (UMetasoundEditorGraph* Graph = GraphMember->GetOwningGraph())
								{
									Graph->GetModifyContext().AddMemberIDsModified({ GraphMember->GetMemberID() });
								}
							}
						};

						if (DefaultFloat->WidgetType == EMetasoundMemberDefaultWidget::Slider)
						{
							// Create slider 
							if (DefaultFloat->WidgetValueType == EMetasoundMemberDefaultWidgetValueType::Frequency)
							{
								SAssignNew(InputWidget, SAudioFrequencySlider)
									.OnValueChanged_Lambda(OnValueChangedLambda)
									.OnValueCommitted_Lambda(OnValueCommittedLambda);
							}
							else if (DefaultFloat->WidgetValueType == EMetasoundMemberDefaultWidgetValueType::Volume)
							{
								SAssignNew(InputWidget, SAudioVolumeSlider)
									.OnValueChanged_Lambda(OnValueChangedLambda)
									.OnValueCommitted_Lambda(OnValueCommittedLambda);
								StaticCastSharedPtr<SAudioVolumeSlider>(InputWidget)->SetUseLinearOutput(DefaultFloat->VolumeWidgetUseLinearOutput);
							}
							else
							{
								SAssignNew(InputWidget, SAudioSlider)
									.OnValueChanged_Lambda(OnValueChangedLambda)
									.OnValueCommitted_Lambda(OnValueCommittedLambda);
								InputWidget->SetShowUnitsText(false);
							}
							// Slider layout 
							if (DefaultFloat->WidgetOrientation == Orient_Vertical)
							{
								SAssignNew(OuterContentBox, SVerticalBox)
									+ SVerticalBox::Slot()
									.HAlign(HAlign_Right)
									.VAlign(VAlign_Center)
									.AutoHeight()
									[
										ContentBox.ToSharedRef()
									]
								+ SVerticalBox::Slot()
									.HAlign(HAlign_Fill)
									.VAlign(VAlign_Top)
									.Padding(WidgetPadding, 0.0f, WidgetPadding, WidgetPadding)
									.AutoHeight()
									[
										InputWidget.ToSharedRef()
									];
								InputWidget->SetDesiredSizeOverride(SliderDesiredSizeVertical);
							}
							else // horizontal orientation
							{
								UMetasoundEditorGraphMemberNode* MemberNode = GetMetaSoundMemberNode();
								TSharedPtr<SWidget> Slot1;
								TSharedPtr<SWidget> Slot2;
								if (MemberNode->IsA<UMetasoundEditorGraphInputNode>())
								{
									Slot1 = InputWidget;
									Slot2 = ContentBox;
								}
								else
								{
									Slot1 = ContentBox;
									Slot2 = InputWidget;
								}

								SAssignNew(OuterContentBox, SHorizontalBox)
									+ SHorizontalBox::Slot()
									.HAlign(HAlign_Fill)
									.VAlign(VAlign_Center)
									.Padding(WidgetPadding, 0.0f, WidgetPadding, 0.0f)
									.AutoWidth()
									[
										Slot1.ToSharedRef()
									]
								+ SHorizontalBox::Slot()
									.HAlign(HAlign_Center)
									.VAlign(VAlign_Fill)
									.AutoWidth()
									[
										Slot2.ToSharedRef()
									];
								InputWidget->SetDesiredSizeOverride(FVector2D(SliderDesiredSizeVertical.Y, SliderDesiredSizeVertical.X));
							}
							// safe downcast because the ptr was just assigned above 
							StaticCastSharedPtr<SAudioSliderBase>(InputWidget)->SetOrientation(DefaultFloat->WidgetOrientation);
						}
						else if (DefaultFloat->WidgetType == EMetasoundMemberDefaultWidget::RadialSlider)
						{
							auto OnRadialSliderMouseCaptureBeginLambda = [this]()
							{
								if (!bIsInputWidgetTransacting)
								{
									GEditor->BeginTransaction(LOCTEXT("MetasoundSetRadialSliderInputDefault", "Set MetaSound Input Default"));
									bIsInputWidgetTransacting = true;
								}
							};

							auto OnRadialSliderMouseCaptureEndLambda = [this]()
							{
								if (bIsInputWidgetTransacting)
								{
									GEditor->EndTransaction();
									bIsInputWidgetTransacting = false;
								}
								else
								{
									UE_LOG(LogMetaSound, Warning, TEXT("Unmatched MetaSound editor widget transaction."));
								}
							};

							// Create slider 
							if (DefaultFloat->WidgetValueType == EMetasoundMemberDefaultWidgetValueType::Frequency)
							{
								SAssignNew(InputWidget, SAudioFrequencyRadialSlider)
									.OnValueChanged_Lambda(OnValueChangedLambda)
									.OnMouseCaptureBegin_Lambda(OnRadialSliderMouseCaptureBeginLambda)
									.OnMouseCaptureEnd_Lambda(OnRadialSliderMouseCaptureEndLambda);
							}
							else if (DefaultFloat->WidgetValueType == EMetasoundMemberDefaultWidgetValueType::Volume)
							{
								SAssignNew(InputWidget, SAudioVolumeRadialSlider)
									.OnValueChanged_Lambda(OnValueChangedLambda)
									.OnMouseCaptureBegin_Lambda(OnRadialSliderMouseCaptureBeginLambda)
									.OnMouseCaptureEnd_Lambda(OnRadialSliderMouseCaptureEndLambda);
								StaticCastSharedPtr<SAudioVolumeRadialSlider>(InputWidget)->SetUseLinearOutput(DefaultFloat->VolumeWidgetUseLinearOutput);
							}
							else
							{
								SAssignNew(InputWidget, SAudioRadialSlider)
									.OnValueChanged_Lambda(OnValueChangedLambda)
									.OnMouseCaptureBegin_Lambda(OnRadialSliderMouseCaptureBeginLambda)
									.OnMouseCaptureEnd_Lambda(OnRadialSliderMouseCaptureEndLambda);
								InputWidget->SetShowUnitsText(false);
							}
							// Only vertical layout for radial slider
							SAssignNew(OuterContentBox, SVerticalBox)
								+ SVerticalBox::Slot()
								.HAlign(HAlign_Right)
								.VAlign(VAlign_Center)
								.AutoHeight()
								[
									ContentBox.ToSharedRef()
								]
							+ SVerticalBox::Slot()
								.HAlign(HAlign_Fill)
								.VAlign(VAlign_Top)
								.Padding(WidgetPadding, 0.0f, WidgetPadding, WidgetPadding)
								.AutoHeight()
								[
									InputWidget.ToSharedRef()
								];
							InputWidget->SetDesiredSizeOverride(RadialSliderDesiredSize);
						}

						InputWidget->SetOutputRange(DefaultFloat->GetRange());
						InputWidget->SetUnitsTextReadOnly(true);
						InputWidget->SetSliderValue(InputWidget->GetSliderValue(DefaultFloat->GetDefault()));
						InputWidget->SetVisibility(TAttribute<EVisibility>::Create([this]()
						{
							if (UMetasoundEditorGraphMemberNode* Node = GetMetaSoundMemberNode())
							{
								return Node->EnableInteractWidgets() ? EVisibility::Visible : EVisibility::Collapsed;
							}
							return EVisibility::Collapsed;
						}));

						// Setup & clear delegate if necessary (ex. if was just saved)
						if (InputSliderOnValueChangedDelegateHandle.IsValid())
						{
							DefaultFloat->OnDefaultValueChanged.Remove(InputSliderOnValueChangedDelegateHandle);
							InputSliderOnValueChangedDelegateHandle.Reset();
						}

						InputSliderOnValueChangedDelegateHandle = DefaultFloat->OnDefaultValueChanged.AddLambda([Widget = InputWidget](float Value)
						{
							if (Widget.IsValid())
							{
								const float SliderValue = Widget->GetSliderValue(Value);
								Widget->SetSliderValue(SliderValue);
							}
						});

						if (InputSliderOnRangeChangedDelegateHandle.IsValid())
						{
							DefaultFloat->OnRangeChanged.Remove(InputSliderOnRangeChangedDelegateHandle);
							InputSliderOnRangeChangedDelegateHandle.Reset();
						}

						InputSliderOnRangeChangedDelegateHandle = DefaultFloat->OnRangeChanged.AddLambda([Widget = InputWidget](FVector2D Range)
						{
							if (Widget.IsValid())
							{
								Widget->SetOutputRange(Range);
							}
						});
					}
				}
			}
	
			// Gives more space for user to grab a bit easier as variables do not have any title area nor icon
			const float GrabPadding = IsVariableMutator() ? 28.0f : 0.0f;

			const EVerticalAlignment PinNodeAlignInput = (!StyleDisplay.bShowInputNames && NodeHandle->GetNumInputs() == 1) ? VAlign_Center : VAlign_Top;
			ContentBox->AddSlot()
			.HAlign(HAlign_Left)
			.VAlign(PinNodeAlignInput)
			.FillWidth(1.0f)
			.Padding(0.0f, 0.0f, GrabPadding, 0.0f)
			[
				SAssignNew(LeftNodeBox, SVerticalBox)
			];

			if (!StyleDisplay.ImageName.IsNone())
			{
				const FSlateBrush& ImageBrush = Metasound::Editor::Style::GetSlateBrushSafe(StyleDisplay.ImageName);
				ContentBox->AddSlot()
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(&ImageBrush)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.DesiredSizeOverride(FVector2D(20, 20))
				];
			}

			const EVerticalAlignment PinNodeAlignOutput = (!StyleDisplay.bShowInputNames && NodeHandle->GetNumOutputs() == 1) ? VAlign_Center : VAlign_Top;
			ContentBox->AddSlot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.VAlign(PinNodeAlignOutput)
				.Padding(GrabPadding, 0.0f, 0.0f, 0.0f)
				[
					SAssignNew(RightNodeBox, SVerticalBox)
				];

			return SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("NoBorder"))
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.Padding(FMargin(0,3))
				[
					(bShowInputWidget ? OuterContentBox : ContentBox).ToSharedRef()
				];
		}

		TSharedPtr<SGraphPin> SMetaSoundGraphNodeKnot::CreatePinWidget(UEdGraphPin* Pin) const
		{
			return SNew(SMetaSoundGraphPinKnot, Pin);
		}

		void SMetaSoundGraphNodeKnot::Construct(const FArguments& InArgs, class UEdGraphNode* InNode)
		{
			GraphNode = InNode;
			SetCursor(EMouseCursor::CardinalCross);
			UpdateGraphNode();
		}

		void SMetaSoundGraphNodeKnot::MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty)
		{
			SGraphNode::MoveTo(NewPosition, NodeFilter, bMarkDirty);

			UMetasoundEditorGraphNode& Node = GetMetaSoundNode();
			Node.GetMetasoundChecked().Modify();
			Node.SetNodeLocation(NewPosition);
		}

		UMetasoundEditorGraphNode& SMetaSoundGraphNodeKnot::GetMetaSoundNode()
		{
			return *CastChecked<UMetasoundEditorGraphNode>(GraphNode);
		}

		const UMetasoundEditorGraphNode& SMetaSoundGraphNodeKnot::GetMetaSoundNode() const
		{
			check(GraphNode);
			return *Cast<UMetasoundEditorGraphNode>(GraphNode);
		}
	} // namespace Editor
} // namespace Metasound
#undef LOCTEXT_NAMESPACE // MetasoundEditor
