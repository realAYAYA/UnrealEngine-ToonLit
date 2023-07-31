// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetasoundActionMenu.h"

#include "Framework/Application/SlateApplication.h"
#include "EdGraphSchema_K2.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "Styling/AppStyle.h"
#include "IDocumentation.h"
#include "MetasoundEditorGraphBuilder.h"
#include "MetasoundEditorGraphSchema.h"
#include "MetasoundEditorModule.h"
#include "MetasoundEditorSettings.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendRegistries.h"
#include "SGraphActionMenu.h"
#include "SMetasoundPalette.h"
#include "SSubobjectEditor.h"
#include "Styling/SlateStyleRegistry.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SExpanderArrow.h"

#define LOCTEXT_NAMESPACE "MetaSoundEditor"


namespace Metasound
{
	namespace Editor
	{
		void SMetasoundActionMenuExpanderArrow::Construct(const FArguments& InArgs, const FCustomExpanderData& ActionMenuData)
		{
			OwnerRowPtr = ActionMenuData.TableRow;
			IndentAmount = InArgs._IndentAmount;
			ActionPtr = ActionMenuData.RowAction;

			if (ActionPtr.IsValid())
			{
				ChildSlot
				.Padding(TAttribute<FMargin>(this, &SMetasoundActionMenuExpanderArrow::GetCustomIndentPadding))
				[
					SNullWidget::NullWidget
					// TODO: Add favorites support
					// SNew(SMetasoundActionFavoriteToggle, ActionMenuData)
				];
			}
			else
			{
				SExpanderArrow::FArguments SuperArgs;
				SuperArgs._IndentAmount = InArgs._IndentAmount;

				SExpanderArrow::Construct(SuperArgs, ActionMenuData.TableRow);
			}
		}

		FMargin SMetasoundActionMenuExpanderArrow::GetCustomIndentPadding() const
		{
			return SExpanderArrow::GetExpanderPadding();
		}

		SMetasoundActionMenu::~SMetasoundActionMenu()
		{
			OnClosedCallback.ExecuteIfBound();
			OnCloseReasonCallback.ExecuteIfBound(bActionExecuted, false, !DraggedFromPins.IsEmpty());
		}

		void SMetasoundActionMenu::Construct(const FArguments& InArgs)
		{
			using namespace Metasound::Editor;
			using namespace Metasound::Frontend;

			Graph = InArgs._Graph;
			DraggedFromPins = InArgs._DraggedFromPins;
			NewNodePosition = InArgs._NewNodePosition;
			OnClosedCallback = InArgs._OnClosedCallback;
			bAutoExpandActionMenu = InArgs._AutoExpandActionMenu;
			OnCloseReasonCallback = InArgs._OnCloseReason;

			FSlateColor TypeColor;
			const FSlateBrush* PinBrush = nullptr;
			if (!DraggedFromPins.IsEmpty())
			{
				if (const UEdGraphPin* Pin = DraggedFromPins[0])
				{
					FDataTypeRegistryInfo RegistryInfo;

					const IMetasoundEditorModule& EditorModule = FModuleManager::GetModuleChecked<IMetasoundEditorModule>("MetaSoundEditor");
					if (Pin->Direction == EGPD_Input)
					{
						FInputHandle InputHandle = FGraphBuilder::GetInputHandleFromPin(Pin);
						IDataTypeRegistry::Get().GetDataTypeInfo(InputHandle->GetDataType(), RegistryInfo);
					}
					else
					{
						FOutputHandle OutputHandle = FGraphBuilder::GetOutputHandleFromPin(Pin);
						IDataTypeRegistry::Get().GetDataTypeInfo(OutputHandle->GetDataType(), RegistryInfo);
					}

					TypeColor = FGraphBuilder::GetPinCategoryColor(Pin->PinType);

					if (RegistryInfo.bIsArrayType)
					{
						PinBrush = FAppStyle::GetBrush("Graph.ArrayPin.Connected");
					}
					else if (Pin->PinType.PinCategory == FGraphBuilder::PinCategoryTrigger)
					{
						if (const ISlateStyle* MetasoundStyle = FSlateStyleRegistry::FindSlateStyle("MetaSoundStyle"))
						{
							PinBrush = MetasoundStyle->GetBrush(TEXT("MetasoundEditor.Graph.TriggerPin.Connected"));
						}
					}

					if (!PinBrush)
					{
						PinBrush = FAppStyle::GetBrush("Graph.Pin.Connected");
					}
				}
			}

			SBorder::Construct(SBorder::FArguments()
				.BorderImage(FAppStyle::GetBrush("Menu.Background"))
				.Padding(5)
				[
					SNew(SBox)
					.WidthOverride(400)
					.HeightOverride(400)
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(2, 2, 2, 5)
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(0, 0, 5, 0)
							[
								SNew(SImage)
								.ColorAndOpacity(TypeColor)
								.Visibility_Lambda([this]()
								{
									return DraggedFromPins.IsEmpty() ? EVisibility::Hidden : EVisibility::Visible;
								})
								.Image(PinBrush)
							]
							+SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text_Lambda([this]()
								{
									if (DraggedFromPins.IsEmpty())
									{
										return LOCTEXT("ContextText", "All MetaSound Node Classes");
									}

									UEdGraphPin* Pin = DraggedFromPins[0];

									if (DraggedFromPins[0]->Direction == EGPD_Input)
									{
										FInputHandle InputHandle = FGraphBuilder::GetInputHandleFromPin(Pin);
										return FText::Format(LOCTEXT("ContextTypeFilteredText_Output", "Classes with output of type '{0}'"), FText::FromName(InputHandle->GetDataType()));
									}
									else
									{
										FOutputHandle OutputHandle = FGraphBuilder::GetOutputHandleFromPin(Pin);
										return FText::Format(LOCTEXT("ContextTypeFilteredText_Input", "Classes with input of type '{0}'"), FText::FromName(OutputHandle->GetDataType()));
									}
								})
								// TODO: Move to Metasound Style
								.Font(FAppStyle::GetFontStyle("BlueprintEditor.ActionMenu.ContextDescriptionFont"))
								.ToolTip(IDocumentation::Get()->CreateToolTip(
									LOCTEXT("ActionMenuContextTextTooltip", "Describes the current context of the action list"),
									nullptr,
									TEXT("Shared/Editors/MetasoundEditor"),
									TEXT("MetasoundActionMenuContextText")))
								.WrapTextAt(280)
							]
						]
						+SVerticalBox::Slot()
						[
							SAssignNew(GraphActionMenu, SGraphActionMenu)
								.OnActionSelected(this, &SMetasoundActionMenu::OnActionSelected)
								.OnCreateWidgetForAction(SGraphActionMenu::FOnCreateWidgetForAction::CreateSP(this, &SMetasoundActionMenu::OnCreateWidgetForAction))
								.OnCollectAllActions(this, &SMetasoundActionMenu::CollectAllActions)
								.OnCreateCustomRowExpander_Lambda([](const FCustomExpanderData& InCustomExpanderData)
								{
									return SNew(SMetasoundActionMenuExpanderArrow, InCustomExpanderData);
								})
								.DraggedFromPins(DraggedFromPins)
								.GraphObj(Graph)
								.AlphaSortItems(true)
								.bAllowPreselectedItemActivation(true)
						]
					]
				]
			);
		}

		void SMetasoundActionMenu::CollectAllActions(FGraphActionListBuilderBase& OutAllActions)
		{
			if (!Graph)
			{
				return;
			}

			FGraphContextMenuBuilder MenuBuilder(Graph);
			if (!DraggedFromPins.IsEmpty())
			{
				MenuBuilder.FromPin = DraggedFromPins[0];
			}

			// Cannot call GetGraphContextActions() during serialization and GC due to its use of FindObject()
			if(!GIsSavingPackage && !IsGarbageCollecting())
			{
				if (const UMetasoundEditorGraphSchema* Schema = GetDefault<UMetasoundEditorGraphSchema>())
				{
					Schema->GetGraphContextActions(MenuBuilder);
				}
			}

			OutAllActions.Append(MenuBuilder);
		}

		TSharedRef<SEditableTextBox> SMetasoundActionMenu::GetFilterTextBox()
		{
			return GraphActionMenu->GetFilterTextBox();
		}

		TSharedRef<SWidget> SMetasoundActionMenu::OnCreateWidgetForAction(FCreateWidgetForActionData* const InCreateData)
		{
			using namespace Metasound;
			using namespace Metasound::Frontend;

			check(InCreateData);
			InCreateData->bHandleMouseButtonDown = false;

			const FSlateBrush* IconBrush = nullptr;
			FLinearColor IconColor;
			TSharedPtr<FMetasoundGraphSchemaAction> Action = StaticCastSharedPtr<FMetasoundGraphSchemaAction>(InCreateData->Action);
			if (Action.IsValid())
			{
				IconBrush = Action->GetIconBrush();
				IconColor = Action->GetIconColor();
			}

			TSharedPtr<SHorizontalBox> WidgetBox = SNew(SHorizontalBox);
			if (IconBrush)
			{
				WidgetBox->AddSlot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(5, 0, 0, 0)
					[
						SNew(SImage)
						.ColorAndOpacity(IconColor)
						.Image(IconBrush)
					];
			}

			WidgetBox->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 0, 0)
				[
					SNew(SGraphPaletteItem, InCreateData)
				];

			return WidgetBox->AsShared();
		}

		void SMetasoundActionMenu::OnActionSelected(const TArray<TSharedPtr<FEdGraphSchemaAction>>& SelectedAction, ESelectInfo::Type InSelectionType)
		{
			if (!Graph)
			{
				return;
			}

			if (InSelectionType != ESelectInfo::OnMouseClick  && InSelectionType != ESelectInfo::OnKeyPress && !SelectedAction.IsEmpty())
			{
				return;
			}

			for (const TSharedPtr<FEdGraphSchemaAction>& Action : SelectedAction)
			{
				if (Action.IsValid() && Graph)
				{
					if (!bActionExecuted && (Action->GetTypeId() != FEdGraphSchemaAction_Dummy::StaticGetTypeId()))
					{
						FSlateApplication::Get().DismissAllMenus();
						bActionExecuted = true;
					}

					if (UEdGraphNode* ResultNode = Action->PerformAction(Graph, DraggedFromPins, NewNodePosition))
					{
						NewNodePosition += Metasound::Frontend::DisplayStyle::NodeLayout::DefaultOffsetX;
					}
				}
			}
		}
	} // namespace Editor
} // namespace Metasound
#undef LOCTEXT_NAMESPACE
