// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "EdGraph/EdGraphPin.h"
#include "Styling/AppStyle.h"
#include "KismetPins/SGraphPinBool.h"
#include "KismetPins/SGraphPinNum.h"
#include "KismetPins/SGraphPinInteger.h"
#include "KismetPins/SGraphPinObject.h"
#include "KismetPins/SGraphPinString.h"
#include "MetasoundEditorGraph.h"
#include "MetasoundEditorGraphBuilder.h"
#include "MetasoundEditorGraphMemberDefaults.h"
#include "MetasoundEditorGraphNode.h"
#include "MetasoundFrontendController.h"
#include "SGraphPin.h"
#include "SGraphNodeKnot.h"
#include "SMetasoundPinValueInspector.h"
#include "SPinTypeSelector.h"
#include "Styling/SlateStyleRegistry.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SWidget.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateBrush.h"

#define LOCTEXT_NAMESPACE "MetasoundEditor"


namespace Metasound
{
	namespace Editor
	{
		template <typename ParentPinType>
		class TMetasoundGraphPin : public ParentPinType
		{
			TSharedPtr<SMetasoundPinValueInspector> PinInspector;

		protected:
			static TWeakPtr<FPinValueInspectorTooltip> OpenPinInspector(UEdGraphPin& InPin, TSharedPtr<SMetasoundPinValueInspector>& OutPinInspector)
			{
				TSharedPtr<SMetasoundPinValueInspector> NewPinInspector = SNew(SMetasoundPinValueInspector);
				TWeakPtr<FPinValueInspectorTooltip> NewTooltip = FPinValueInspectorTooltip::SummonTooltip(&InPin, NewPinInspector);
				if (NewTooltip.IsValid())
				{
					OutPinInspector = NewPinInspector;
					return NewTooltip;
				}

				return nullptr;
			}

			static void UpdatePinInspector(
				UEdGraphPin& InPin,
				const bool bIsHoveringPin,
				TSharedPtr<SMetasoundPinValueInspector>& OutPinInspector,
				TWeakPtr<FPinValueInspectorTooltip>& OutInspectorTooltip,
				TFunctionRef<void(FVector2D&)> InGetTooltipLocation)
			{
				const bool bCanInspectPin = FGraphBuilder::CanInspectPin(&InPin);
				if (bIsHoveringPin && bCanInspectPin)
				{
					if (OutPinInspector.IsValid())
					{
						const UEdGraphPin* InspectedPin = OutPinInspector->GetPinRef().Get();
						if (InspectedPin == &InPin)
						{
							OutPinInspector->UpdateMessage();
						}
					}
					else
					{
						OutInspectorTooltip = OpenPinInspector(InPin, OutPinInspector);
						TSharedPtr<FPinValueInspectorTooltip> NewTooltip = OutInspectorTooltip.Pin();
						if (NewTooltip.IsValid())
						{
							FVector2D TooltipLocation;
							InGetTooltipLocation(TooltipLocation);
							NewTooltip->MoveTooltip(TooltipLocation);
						}
					}
				}
				else if (OutPinInspector.IsValid())
				{
					TSharedPtr<FPinValueInspectorTooltip> InspectorTooltip = OutInspectorTooltip.Pin();
					if (InspectorTooltip.IsValid())
					{
						if (InspectorTooltip->TooltipCanClose())
						{
							constexpr bool bForceDismiss = true;
							InspectorTooltip->TryDismissTooltip(bForceDismiss);
							OutInspectorTooltip.Reset();
							OutPinInspector.Reset();
						}
					}
					else
					{
						OutPinInspector.Reset();
					}
				}
			}

			void CacheAccessType()
			{
				AccessType = EMetasoundFrontendVertexAccessType::Unset;

				if (const UEdGraphPin* Pin = ParentPinType::GetPinObj())
				{
					if (const UMetasoundEditorGraphNode* Node = Cast<UMetasoundEditorGraphNode>(Pin->GetOwningNode()))
					{
						if (const UMetasoundEditorGraphMemberNode* MemberNode = Cast<UMetasoundEditorGraphMemberNode>(Node))
						{
							if (const UMetasoundEditorGraphVertex* Vertex = Cast<UMetasoundEditorGraphVertex>(MemberNode->GetMember()))
							{
								AccessType = Vertex->GetVertexAccessType();
							}
						}
						else if (const UMetasoundEditorGraphExternalNode* ExternalNode = Cast<UMetasoundEditorGraphExternalNode>(Node))
						{
							if (Pin->Direction == EGPD_Input)
							{
								Frontend::FInputHandle InputHandle = FGraphBuilder::GetInputHandleFromPin(Pin);
								AccessType = InputHandle->GetVertexAccessType();
							}
							else if (Pin->Direction == EGPD_Output)
							{
								Frontend::FOutputHandle OutputHandle = FGraphBuilder::GetOutputHandleFromPin(Pin);
								AccessType = OutputHandle->GetVertexAccessType();
							}
						}
					}
				}
			}

			void CacheNodeOffset(const FGeometry& AllottedGeometry)
			{
				const FVector2D UnscaledPosition = ParentPinType::OwnerNodePtr.Pin()->GetUnscaledPosition();
				ParentPinType::CachedNodeOffset = FVector2D(AllottedGeometry.AbsolutePosition) / AllottedGeometry.Scale - UnscaledPosition;
				ParentPinType::CachedNodeOffset.Y += AllottedGeometry.Size.Y * 0.5f;
			}

			EMetasoundFrontendVertexAccessType AccessType = EMetasoundFrontendVertexAccessType::Unset;

		public:
			SLATE_BEGIN_ARGS(TMetasoundGraphPin<ParentPinType>)
			{
			}
			SLATE_END_ARGS()

			virtual ~TMetasoundGraphPin() = default;

			Frontend::FConstInputHandle GetConstInputHandle() const
			{
				using namespace Frontend;

				const bool bIsInput = (ParentPinType::GetDirection() == EGPD_Input);
				if (bIsInput)
				{
					if (const UEdGraphPin* Pin = ParentPinType::GetPinObj())
					{
						if (const UMetasoundEditorGraphNode* MetasoundEditorNode = Cast<UMetasoundEditorGraphNode>(Pin->GetOwningNode()))
						{
							FConstNodeHandle NodeHandle = MetasoundEditorNode->GetConstNodeHandle();
							return NodeHandle->GetConstInputWithVertexName(Pin->GetFName());
						}
					}
				}

				return IInputController::GetInvalidHandle();
			}

			Frontend::FInputHandle GetInputHandle()
			{
				using namespace Frontend;

				const bool bIsInput = (ParentPinType::GetDirection() == EGPD_Input);
				if (bIsInput)
				{
					if (UEdGraphPin* Pin = ParentPinType::GetPinObj())
					{
						if (UMetasoundEditorGraphNode* MetasoundEditorNode = Cast<UMetasoundEditorGraphNode>(Pin->GetOwningNode()))
						{
							FNodeHandle NodeHandle = MetasoundEditorNode->GetNodeHandle();
							return NodeHandle->GetInputWithVertexName(Pin->GetFName());
						}
					}
				}

				return IInputController::GetInvalidHandle();
			}

			bool ShowDefaultValueWidget() const
			{
				UEdGraphPin* Pin = ParentPinType::GetPinObj();
				if (!Pin)
				{
					return true;
				}

				UMetasoundEditorGraphMemberNode* Node = Cast<UMetasoundEditorGraphMemberNode>(Pin->GetOwningNode());
				if (!Node)
				{
					return true;
				}

				UMetasoundEditorGraphMember* Member = Node->GetMember();
				if (!Member)
				{
					return true;
				}

				UMetasoundEditorGraphMemberDefaultFloat* DefaultFloat = Cast<UMetasoundEditorGraphMemberDefaultFloat>(Member->GetLiteral());
				if (!DefaultFloat)
				{
					return true;
				}

				return DefaultFloat->WidgetType == EMetasoundMemberDefaultWidget::None;
			}

			virtual TSharedRef<SWidget> GetDefaultValueWidget() override
			{
				using namespace Frontend;
				TSharedRef<SWidget> DefaultWidget = ParentPinType::GetDefaultValueWidget();

				if (!ShowDefaultValueWidget())
				{
					return SNullWidget::NullWidget;
				}

				// For now, arrays do not support literals.
				// TODO: Support array literals by displaying
				// default literals (non-array too) in inspector window.
				FConstInputHandle InputHandle = GetConstInputHandle();
				if (!InputHandle->IsValid() || ParentPinType::IsArray())
				{
					return DefaultWidget;
				}

				return SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						DefaultWidget
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
						.ToolTipText(LOCTEXT("ResetToClassDefaultToolTip", "Reset to class default"))
						.ButtonStyle(FAppStyle::Get(), TEXT("NoBorder"))
						.ContentPadding(0.0f)
						.Visibility(TAttribute<EVisibility>::Create([this]
						{
							using namespace Frontend;
							if (!ParentPinType::IsConnected())
							{
								FConstInputHandle InputHandle = GetConstInputHandle();
								if (const FMetasoundFrontendLiteral* Literal = InputHandle->GetLiteral())
								{
									const bool bIsDefaultConstructed = Literal->GetType() == EMetasoundFrontendLiteralType::None;
									const bool bIsTriggerDataType = InputHandle->GetDataType() == GetMetasoundDataTypeName<FTrigger>();
									if (!bIsDefaultConstructed && !bIsTriggerDataType)
									{
										return EVisibility::Visible;
									}
								}
							}

							return EVisibility::Collapsed;
						}))
						.OnClicked(FOnClicked::CreateLambda([this]()
						{
							using namespace Frontend;

							if (UEdGraphPin* Pin = ParentPinType::GetPinObj())
							{
								if (UMetasoundEditorGraphNode* Node = Cast<UMetasoundEditorGraphNode>(Pin->GetOwningNode()))
								{
									if (UMetasoundEditorGraph* MetaSoundGraph = CastChecked<UMetasoundEditorGraph>(Node->GetGraph()))
									{
										UObject& MetaSound = MetaSoundGraph->GetMetasoundChecked();

										{
											const FScopedTransaction Transaction(LOCTEXT("MetaSoundEditorResetToClassDefault", "Reset to Class Default"));
											MetaSound.Modify();
											MetaSoundGraph->Modify();

											if (UMetasoundEditorGraphMemberNode* MemberNode = Cast<UMetasoundEditorGraphMemberNode>(Node))
											{
												UMetasoundEditorGraphMember* Member = MemberNode->GetMember();
												if (ensure(Member))
												{
													Member->ResetToClassDefault();
													MetaSoundGraph->GetModifyContext().AddMemberIDsModified({ Member->GetMemberID() });
												}
												else
												{
													MetaSoundGraph->GetModifyContext().SetDocumentModified();
												}
											}
											else
											{
												FInputHandle InputHandle = GetInputHandle();
												InputHandle->ClearLiteral();
												MetaSoundGraph->GetModifyContext().SetDocumentModified();
											}
										}
									}
								}
							}

							return FReply::Handled();
						}))
						.Content()
						[
							SNew(SImage)
							.Image(FAppStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
						]
					];
			}

			virtual const FSlateBrush* GetPinIcon() const override
			{
				const bool bIsConnected = ParentPinType::IsConnected();

				// Is constructor pin 
				if (AccessType == EMetasoundFrontendVertexAccessType::Value)
				{
					if (const ISlateStyle* MetasoundStyle = FSlateStyleRegistry::FindSlateStyle("MetaSoundStyle"))
					{
						if (ParentPinType::IsArray())
						{
							return bIsConnected ? MetasoundStyle->GetBrush(TEXT("MetasoundEditor.Graph.ConstructorPinArray")) :
								MetasoundStyle->GetBrush(TEXT("MetasoundEditor.Graph.ConstructorPinArrayDisconnected"));
						}
						else
						{
							return bIsConnected ? MetasoundStyle->GetBrush(TEXT("MetasoundEditor.Graph.ConstructorPin")) :
								MetasoundStyle->GetBrush(TEXT("MetasoundEditor.Graph.ConstructorPinDisconnected"));
						}
					}
				}
				return SGraphPin::GetPinIcon();
			}

			virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
			{
				CacheNodeOffset(AllottedGeometry);

				if (UEdGraphPin* GraphPin = ParentPinType::GetPinObj())
				{
					const bool bIsHoveringPin = ParentPinType::IsHovered();
					UpdatePinInspector(*GraphPin, bIsHoveringPin, PinInspector, ParentPinType::ValueInspectorTooltip,
						[this](FVector2D& OutTooltipLocation)
						{
							ParentPinType::GetInteractiveTooltipLocation(OutTooltipLocation);
						});
				}
			}
		};

		class SMetasoundGraphPin : public TMetasoundGraphPin<SGraphPin>
		{
		public:
			virtual ~SMetasoundGraphPin() = default;

			void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
			{
				SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
				CacheAccessType();
			}
		};

		class SMetasoundGraphPinBool : public TMetasoundGraphPin<SGraphPinBool>
		{
		public:
			virtual ~SMetasoundGraphPinBool() = default;

			void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
			{
				SGraphPinBool::Construct(SGraphPinBool::FArguments(), InGraphPinObj);
				CacheAccessType();
			}
		};

		class SMetasoundGraphPinFloat : public TMetasoundGraphPin<SGraphPinNum<float>>
		{
		public:
			virtual ~SMetasoundGraphPinFloat() = default;

			void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
			{
				SGraphPinNum<float>::Construct(SGraphPinNum<float>::FArguments(), InGraphPinObj);
				CacheAccessType();
			}
		};

		class SMetasoundGraphPinInteger : public TMetasoundGraphPin<SGraphPinInteger>
		{
		public:
			virtual ~SMetasoundGraphPinInteger() = default;

			void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
			{
				SGraphPinInteger::Construct(SGraphPinInteger::FArguments(), InGraphPinObj);
				CacheAccessType();
			}
		};

		class SMetasoundGraphPinObject : public TMetasoundGraphPin<SGraphPinObject>
		{
		public:
			virtual ~SMetasoundGraphPinObject() = default;

			void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
			{
				SGraphPinObject::Construct(SGraphPinObject::FArguments(), InGraphPinObj);
				CacheAccessType();
			}
		};

		class SMetasoundGraphPinString : public TMetasoundGraphPin<SGraphPinString>
		{
		public:
			virtual ~SMetasoundGraphPinString() = default;

			void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
			{
				SGraphPinString::Construct(SGraphPinString::FArguments(), InGraphPinObj);
				CacheAccessType();
			}
		};

		class SMetaSoundGraphPinKnot : public TMetasoundGraphPin<SGraphPinKnot>
		{
		public:
			virtual ~SMetaSoundGraphPinKnot() = default;

			void Construct(const FArguments& InArgs, UEdGraphPin* InPin);

			virtual FSlateColor GetPinColor() const override;

			virtual const FSlateBrush* GetPinIcon() const override;

		protected:
			void CacheHasRequiredConnections();

			bool bHasRequiredConnections = false;
		};
	} // namespace Editor
} // namespace Metasound
#undef LOCTEXT_NAMESPACE
