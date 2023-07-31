// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundEditor.h"

#include "Algo/AnyOf.h"
#include "AudioDevice.h"
#include "AudioMeterStyle.h"
#include "Components/AudioComponent.h"
#include "DetailLayoutBuilder.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphUtilities.h"
#include "Editor.h"
#include "Styling/AppStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GenericPlatform/GenericApplication.h"
#include "GraphEditor.h"
#include "GraphEditorActions.h"
#include "GraphEditorDragDropAction.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IAudioExtensionPlugin.h"
#include "IDetailsView.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Logging/TokenizedMessage.h"
#include "Metasound.h"
#include "MetasoundEditorCommands.h"
#include "MetasoundEditorGraph.h"
#include "MetasoundEditorGraphBuilder.h"
#include "MetasoundEditorGraphInputNode.h"
#include "MetasoundEditorGraphSchema.h"
#include "MetasoundEditorGraphValidation.h"
#include "MetasoundEditorModule.h"
#include "MetasoundEditorSettings.h"
#include "MetasoundEditorTabFactory.h"
#include "MetasoundFrontend.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundFrontendSearchEngine.h"
#include "MetasoundFrontendTransform.h"
#include "MetasoundLog.h"
#include "MetasoundUObjectRegistry.h"
#include "Misc/Attribute.h"
#include "Modules/ModuleManager.h"
#include "NodeTemplates/MetasoundFrontendNodeTemplateReroute.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "SMetasoundActionMenu.h"
#include "SMetasoundPalette.h"
#include "SNodePanel.h"
#include "Stats/Stats.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWindow.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetasoundEditor)

struct FGraphActionNode;

#define LOCTEXT_NAMESPACE "MetaSoundEditor"


namespace Metasound
{
	namespace Editor
	{
		static const TArray<FText> NodeSectionNames
		{
			LOCTEXT("NodeSectionName_Invalid", "INVALID"),
			LOCTEXT("NodeSectionName_Inputs", "Inputs"),
			LOCTEXT("NodeSectionName_Outputs", "Outputs"),
			LOCTEXT("NodeSectionName_Variables", "Variables")
		};

		class FMetasoundGraphMemberSchemaAction : public FEdGraphSchemaAction
		{
		public:
			UEdGraph* Graph = nullptr;
			FGuid MemberID;

			FMetasoundGraphMemberSchemaAction()
				: FEdGraphSchemaAction()
			{
			}

			FMetasoundGraphMemberSchemaAction(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping, const ENodeSection InSectionID)
				: FEdGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), InGrouping, FText(), static_cast<int32>(InSectionID))
			{
			}

			FMetasoundAssetBase& GetMetasoundAssetChecked() const
			{
				UObject* Object = CastChecked<UMetasoundEditorGraph>(Graph)->GetMetasound();
				FMetasoundAssetBase* MetasoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(Object);
				check(MetasoundAsset);
				return *MetasoundAsset;
			}

			UMetasoundEditorGraphMember* GetGraphMember() const
			{
				UMetasoundEditorGraph* MetasoundGraph = CastChecked<UMetasoundEditorGraph>(Graph);
				return MetasoundGraph->FindMember(MemberID);
			}

			FName GetMemberName() const
			{
				if (UMetasoundEditorGraphMember* Member = GetGraphMember())
				{
					return Member->GetMemberName();
				}
				return NAME_None;
			}

			Frontend::FGraphHandle GetGraphHandle() const
			{
				return GetMetasoundAssetChecked().GetRootGraphHandle();
			}

			// FEdGraphSchemaAction interface
			virtual bool IsParentable() const override
			{
				return true;
			}

			virtual void MovePersistentItemToCategory(const FText& NewCategoryName) override
			{
				checkNoEntry();
			}

			virtual int32 GetReorderIndexInContainer() const override
			{
				TArray<Frontend::FConstNodeHandle> InputHandles = GetGraphHandle()->GetConstInputNodes();
				return InputHandles.IndexOfByPredicate([=](const Frontend::FConstNodeHandle& NodeHandle)
				{
					return NodeHandle->GetID() == MemberID;
				});
			}

			virtual bool ReorderToBeforeAction(TSharedRef<FEdGraphSchemaAction> OtherAction) override
			{
				// TODO: Implement reordering
				checkNoEntry();

				return false;
			}
		};

		class FMetaSoundDragDropMemberAction : public FGraphSchemaActionDragDropAction
		{
			TSharedPtr<FEditor> Editor;
			TWeakObjectPtr<UMetasoundEditorGraphMember> GraphMember;

		public:
			FMetaSoundDragDropMemberAction(TSharedPtr<FEditor> InEditor, UMetasoundEditorGraphMember* InGraphMember)
				: Editor(InEditor)
				, GraphMember(InGraphMember)
			{
				CursorDecoratorWindow = SWindow::MakeCursorDecorator();
				constexpr bool bShowImmediately = false;
				FSlateApplication::Get().AddWindow(CursorDecoratorWindow.ToSharedRef(), bShowImmediately);
			}

			DRAG_DROP_OPERATOR_TYPE(FMetaSoundDragDropMemberAction, FGraphSchemaActionDragDropAction)

			virtual FReply DroppedOnPanel(const TSharedRef<SWidget>& InPanel, FVector2D InScreenPosition, FVector2D InGraphPosition, UEdGraph& InGraph) override
			{
				using namespace Frontend;

				if (!GraphMember.IsValid() || &InGraph != GraphMember->GetOwningGraph())
				{
					return FReply::Unhandled();
				}

				UMetasoundEditorGraph* MetasoundGraph = CastChecked<UMetasoundEditorGraph>(&InGraph);
				UObject& ParentMetasound = MetasoundGraph->GetMetasoundChecked();

				if (UMetasoundEditorGraphInput* Input = Cast<UMetasoundEditorGraphInput>(GraphMember.Get()))
				{
					const FScopedTransaction Transaction(LOCTEXT("DropAddNewInputNode", "Drop New MetaSound Input Node"));
					ParentMetasound.Modify();
					MetasoundGraph->Modify();
					Input->Modify();

					if (UMetasoundEditorGraphInputNode* NewGraphNode = FGraphBuilder::AddInputNode(ParentMetasound, Input->GetNodeHandle(), InGraphPosition))
					{
						NewGraphNode->Modify();
						FGraphBuilder::RegisterGraphWithFrontend(ParentMetasound);
						TSharedPtr<FEditor> MetasoundEditor = FGraphBuilder::GetEditorForGraph(*MetasoundGraph);
						if (MetasoundEditor.IsValid())
						{
							MetasoundEditor->ClearSelectionAndSelectNode(NewGraphNode);
						}
						return FReply::Handled();
					}
				}

				if (UMetasoundEditorGraphOutput* Output = Cast<UMetasoundEditorGraphOutput>(GraphMember.Get()))
				{
					TArray<UMetasoundEditorGraphMemberNode*> Nodes = Output->GetNodes();
					if (Nodes.IsEmpty())
					{
						const FScopedTransaction Transaction(LOCTEXT("DropAddNewOutputNode", "Drop New MetaSound Output Node"));
						ParentMetasound.Modify();
						MetasoundGraph->Modify();
						Output->Modify();

						FNodeHandle OutputHandle = Output->GetNodeHandle();
						if (UMetasoundEditorGraphOutputNode* NewGraphNode = FGraphBuilder::AddOutputNode(ParentMetasound, OutputHandle, InGraphPosition))
						{
							NewGraphNode->Modify();
							FGraphBuilder::RegisterGraphWithFrontend(ParentMetasound);
							TSharedPtr<FEditor> MetasoundEditor = FGraphBuilder::GetEditorForGraph(*MetasoundGraph);
							if (MetasoundEditor.IsValid())
							{
								MetasoundEditor->ClearSelectionAndSelectNode(NewGraphNode);
							}
							return FReply::Handled();
						}
					}
					else
					{
						if (Editor.IsValid())
						{
							Editor->JumpToNodes(Nodes);
							return FReply::Handled();
						}
					}
				}

				if (UMetasoundEditorGraphVariable* Variable = Cast<UMetasoundEditorGraphVariable>(GraphMember.Get()))
				{
					const FScopedTransaction Transaction(LOCTEXT("DropAddNewVariableNode", "Drop New MetaSound Variable Node"));
					ParentMetasound.Modify();
					MetasoundGraph->Modify();
					Variable->Modify();

					FVariableHandle VariableHandle = Variable->GetVariableHandle();
					FMetasoundFrontendClass VariableClass;

					const bool bMakeOrJumpToMutator = FSlateApplication::Get().GetModifierKeys().AreModifersDown(EModifierKey::Shift);
					if (bMakeOrJumpToMutator)
					{
						FConstNodeHandle MutatorNodeHandle = Variable->GetConstVariableHandle()->FindMutatorNode();
						if (MutatorNodeHandle->IsValid())
						{
							if (Editor.IsValid())
							{
								auto IsMutatorNode = [&MutatorNodeHandle](const UMetasoundEditorGraphMemberNode* Node)
								{
									return Node->GetNodeID() == MutatorNodeHandle->GetID();
								};
								TArray<UMetasoundEditorGraphMemberNode*> Nodes = Variable->GetNodes();
								if (UMetasoundEditorGraphMemberNode** MutatorNode = Nodes.FindByPredicate(IsMutatorNode))
								{
									check(*MutatorNode);
									Editor->JumpToNodes<UMetasoundEditorGraphMemberNode>({ *MutatorNode });
									return FReply::Handled();
								}
							}
						}
						else
						{
							ensure(IDataTypeRegistry::Get().GetFrontendVariableMutatorClass(Variable->GetDataType(), VariableClass));
						}
					}
					else
					{
						const bool bJumpToGetters = FSlateApplication::Get().GetModifierKeys().AreModifersDown(EModifierKey::Control);
						if (bJumpToGetters)
						{
							TArray<UMetasoundEditorGraphMemberNode*> Nodes = Variable->GetNodes();
							for (int32 i = Nodes.Num() - 1; i >= 0; --i)
							{
								const UMetasoundEditorGraphVariableNode* VariableNode = CastChecked<UMetasoundEditorGraphVariableNode>(Nodes[i]);
								const EMetasoundFrontendClassType ClassType = VariableNode->GetClassType();
								if (ClassType != EMetasoundFrontendClassType::VariableAccessor
									&& ClassType != EMetasoundFrontendClassType::VariableDeferredAccessor)
								{
									constexpr bool bAllowShrinking = false;
									Nodes.RemoveAtSwap(i, 1, bAllowShrinking);
								}
							}
							Editor->JumpToNodes(Nodes);
							return FReply::Handled();
						}
						else
						{
							const bool bMakeGetDeferred = FSlateApplication::Get().GetModifierKeys().AreModifersDown(EModifierKey::Alt);
							if (bMakeGetDeferred)
							{
								ensure(IDataTypeRegistry::Get().GetFrontendVariableDeferredAccessorClass(Variable->GetDataType(), VariableClass));
							}
							else
							{
								ensure(IDataTypeRegistry::Get().GetFrontendVariableAccessorClass(Variable->GetDataType(), VariableClass));
							}
						}
					}

					const FNodeClassName ClassName = VariableClass.Metadata.GetClassName().ToNodeClassName();
					FNodeHandle NodeHandle = FGraphBuilder::AddVariableNodeHandle(ParentMetasound, Variable->GetVariableID(), ClassName);
					if (UMetasoundEditorGraphVariableNode* NewGraphNode = FGraphBuilder::AddVariableNode(ParentMetasound, NodeHandle, InGraphPosition))
					{
						NewGraphNode->Modify();
						FGraphBuilder::RegisterGraphWithFrontend(ParentMetasound);
						TSharedPtr<FEditor> MetasoundEditor = FGraphBuilder::GetEditorForGraph(*MetasoundGraph);
						if (MetasoundEditor.IsValid())
						{
							MetasoundEditor->ClearSelectionAndSelectNode(NewGraphNode);
						}
						return FReply::Handled();
					}
				}

				return FReply::Unhandled();
			}
			virtual FReply DroppedOnNode(FVector2D ScreenPosition, FVector2D GraphPosition) override { return FReply::Unhandled(); }
			virtual FReply DroppedOnPin(FVector2D ScreenPosition, FVector2D GraphPosition) override { return FReply::Unhandled(); }
			virtual FReply DroppedOnAction(TSharedRef<FEdGraphSchemaAction> Action) { return FReply::Unhandled(); }
			virtual FReply DroppedOnCategory(FText Category) override { return FReply::Unhandled(); }
			virtual void HoverTargetChanged() override
			{
				using namespace Frontend;

				bDropTargetValid = false;

				const FSlateBrush* PrimarySymbol = nullptr;
				const FSlateBrush* SecondarySymbol = nullptr;
				FSlateColor PrimaryColor;
				FSlateColor SecondaryColor;
				GetDefaultStatusSymbol(PrimarySymbol, PrimaryColor, SecondarySymbol, SecondaryColor);

				FText Message;
				if (GraphMember.IsValid())
				{
					UMetasoundEditorGraph* OwningGraph = GraphMember->GetOwningGraph();
					Message = GraphMember->GetDisplayName();
					if (GetHoveredGraph() && OwningGraph)
					{
						if (GetHoveredGraph() == OwningGraph)
						{
							FConstDocumentHandle DocumentHandle= OwningGraph->GetDocumentHandle();
							const FMetasoundFrontendGraphClass& RootGraphClass = DocumentHandle->GetRootGraphClass();
							const bool bIsPreset = RootGraphClass.PresetOptions.bIsPreset;

							if (bIsPreset)
							{
								Message = FText::Format(LOCTEXT("DropTargetFailIsPreset", "'{0}': Graph is Preset"), GraphMember->GetDisplayName());
							}
							else if (UMetasoundEditorGraphInput* Input = Cast<UMetasoundEditorGraphInput>(GraphMember.Get()))
							{
								bDropTargetValid = true;

								Style::GetSlateBrushSafe("MetasoundEditor.Graph.Node.Class.Input");
								SecondarySymbol = nullptr;

								if (const UMetasoundEditorSettings* EditorSettings = GetDefault<UMetasoundEditorSettings>())
								{
									PrimaryColor = EditorSettings->InputNodeTitleColor;
									SecondaryColor = EditorSettings->InputNodeTitleColor;
								}
							}
							else if (UMetasoundEditorGraphOutput* Output = Cast<UMetasoundEditorGraphOutput>(GraphMember.Get()))
							{
								bDropTargetValid = true;

								if (!Output->GetNodes().IsEmpty())
								{
									PrimarySymbol = FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.ShowNode"));
									SecondarySymbol = nullptr;
									Message = FText::Format(LOCTEXT("DropTargetShowOutput", "Show '{0}' (One per graph)"), GraphMember->GetDisplayName());
								}
								else
								{
									if (const ISlateStyle* MetasoundStyle = FSlateStyleRegistry::FindSlateStyle("MetaSoundStyle"))
									{
										PrimarySymbol = MetasoundStyle->GetBrush("MetasoundEditor.Graph.Node.Class.Output");
										SecondarySymbol = nullptr;
									}

									if (const UMetasoundEditorSettings* EditorSettings = GetDefault<UMetasoundEditorSettings>())
									{
										PrimaryColor = EditorSettings->OutputNodeTitleColor;
										SecondaryColor = EditorSettings->OutputNodeTitleColor;
									}
								}
							}
							else if (UMetasoundEditorGraphVariable* Variable = Cast<UMetasoundEditorGraphVariable>(GraphMember.Get()))
							{
								bDropTargetValid = true;

								PrimarySymbol = FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.ShowNode"));

								if (const ISlateStyle* MetasoundStyle = FSlateStyleRegistry::FindSlateStyle("MetaSoundStyle"))
								{
									PrimarySymbol = MetasoundStyle->GetBrush("MetasoundEditor.Graph.Node.Class.Variable");
									SecondarySymbol = nullptr;
								}

								if (const UMetasoundEditorSettings* EditorSettings = GetDefault<UMetasoundEditorSettings>())
								{
									PrimaryColor = EditorSettings->VariableNodeTitleColor;
									SecondaryColor = EditorSettings->VariableNodeTitleColor;
								}

								const FText DisplayName = GraphMember->GetDisplayName();
								const FText GetterToolTip = FText::Format(LOCTEXT("DropTargetGetterVariableToolTipFormat", "{0}\nAdd:\n* Get (Drop)\n* Get Delayed (Alt+Drop)\n"), DisplayName);
								static const FText GetJumpToToolTip = LOCTEXT("JumpToGettersToolTip", "Get (Ctrl+Drop)");
								static const FText AddOrJumpToSetToolTip = LOCTEXT("AddOrJumpToSetToolTip", "");
								FConstNodeHandle MutatorNodeHandle = Variable->GetConstVariableHandle()->FindMutatorNode();
								if (MutatorNodeHandle->IsValid())
								{
									Message = FText::Format(LOCTEXT("DropTargetVariableJumpToFormat", "{0}\nJump To:\n* {1}\n* Set (Shift+Drop, One per graph)"), GetterToolTip, GetJumpToToolTip);
								}
								else
								{
									TArray<FConstNodeHandle> AccessorNodeHandles = Variable->GetConstVariableHandle()->FindAccessorNodes();

									if (AccessorNodeHandles.IsEmpty())
									{
										Message = FText::Format(LOCTEXT("DropTargetVariableAddSetGetFormat", "{0}* Set (Shift+Drop)"), GetterToolTip);
									}
									else
									{
										Message = FText::Format(LOCTEXT("DropTargetVariableAddSetJumpToGetFormat", "{0}* Set (Shift+Drop)\n\nJump To:\n* {1}"), GetterToolTip, GetJumpToToolTip);
									}
								}
							}
						}
						else
						{
							Message = FText::Format(LOCTEXT("DropTargetFailNotParentGraph", "'{0}': Graph is not parent of member."), GraphMember->GetDisplayName());
						}
					}
				}

				SetSimpleFeedbackMessage(PrimarySymbol, PrimaryColor, Message, SecondarySymbol, SecondaryColor);
			}
		};


		class SMetaSoundGraphPaletteItem : public SGraphPaletteItem
		{
		private:
			TSharedPtr<FMetasoundGraphMemberSchemaAction> MetasoundAction;
			FMetasoundFrontendVersion InterfaceVersion;

		public:
			SLATE_BEGIN_ARGS(SMetaSoundGraphPaletteItem)
			{
			}

			SLATE_END_ARGS()

			void Construct(const FArguments& InArgs, FCreateWidgetForActionData* const InCreateData)
			{
				TSharedPtr<FEdGraphSchemaAction> Action = InCreateData->Action;
				MetasoundAction = StaticCastSharedPtr<FMetasoundGraphMemberSchemaAction>(Action);

				if (UMetasoundEditorGraphVertex* GraphVertex = Cast<UMetasoundEditorGraphVertex>(MetasoundAction->GetGraphMember()))
				{
					InterfaceVersion = GraphVertex->GetInterfaceVersion();
				}

				SGraphPaletteItem::Construct(SGraphPaletteItem::FArguments(), InCreateData);
			}

		protected:
			virtual void OnNameTextCommitted(const FText& InNewText, ETextCommit::Type InTextCommit) override
			{
				using namespace Frontend;

				if (InterfaceVersion.IsValid())
				{
					return;
				}

				if (MetasoundAction.IsValid())
				{
					if (UMetasoundEditorGraphMember* GraphMember = MetasoundAction->GetGraphMember())
					{
						const FText TransactionLabel = FText::Format(LOCTEXT("Rename Graph Member", "Set MetaSound {0}'s Name"), GraphMember->GetGraphMemberLabel());
						const FScopedTransaction Transaction(TransactionLabel);

						constexpr bool bPostTransaction = false;
						GraphMember->SetDisplayName(FText::GetEmpty(), bPostTransaction);
						GraphMember->SetMemberName(FName(*InNewText.ToString()), bPostTransaction);
					}
				}
			}

			virtual TSharedRef<SWidget> CreateTextSlotWidget(FCreateWidgetForActionData* const InCreateData, TAttribute<bool> bIsReadOnly) override
			{
				TSharedRef<SWidget> TextWidget = SGraphPaletteItem::CreateTextSlotWidget(InCreateData, bIsReadOnly);

				bool bIsConstructorPin = false;

				const FSlateBrush* IconBrush = nullptr;
				const FVector2D IconSize16 = FVector2D(16.0f, 16.0f);
				FSlateColor IconColor = FSlateColor::UseForeground();

				const bool bIsInterfaceMember = InterfaceVersion.IsValid();
				const FSlateBrush* InterfaceIconBrush = bIsInterfaceMember ? FAppStyle::GetBrush("Icons.Lock") : FStyleDefaults::GetNoBrush();

				if (TSharedPtr<FMetasoundGraphMemberSchemaAction> GraphMemberAction = StaticCastSharedPtr<FMetasoundGraphMemberSchemaAction>(InCreateData->Action))
				{

					UMetasoundEditorGraphMember* GraphMember = GraphMemberAction->GetGraphMember();
					if (const UMetasoundEditorGraphVertex* Vertex = Cast<UMetasoundEditorGraphVertex>(GraphMember))
					{
						EMetasoundFrontendVertexAccessType AccessType = Vertex->GetVertexAccessType();
						bIsConstructorPin = AccessType == EMetasoundFrontendVertexAccessType::Value;
					}
					FName DataTypeName = GraphMember->GetDataType(); 

					const IMetasoundEditorModule& EditorModule = FModuleManager::GetModuleChecked<IMetasoundEditorModule>("MetaSoundEditor");
					if (const FEdGraphPinType* PinType = EditorModule.FindPinType(DataTypeName))
					{
						if (const UMetasoundEditorGraphSchema* Schema = GetDefault<UMetasoundEditorGraphSchema>())
						{
							IconColor = Schema->GetPinTypeColor(*PinType);
						}
					}

					IconBrush = EditorModule.GetIconBrush(DataTypeName, bIsConstructorPin);
				}

				TSharedRef<SHorizontalBox> LayoutWidget = SNew(SHorizontalBox);
				LayoutWidget->AddSlot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(IconBrush)
					.ColorAndOpacity(IconColor)
					.DesiredSizeOverride(IconSize16)
				];

				if (bIsInterfaceMember)
				{
					LayoutWidget->AddSlot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(InterfaceIconBrush)
						.ToolTipText(bIsInterfaceMember ? FText::Format(LOCTEXT("InterfaceMemberToolTipFormat", "Cannot Add/Remove: Member of interface '{0}'"), FText::FromName(InterfaceVersion.Name)) : FText())
						.ColorAndOpacity(FSlateColor::UseForeground())
						.DesiredSizeOverride(IconSize16)
					];
				}
				
				LayoutWidget->AddSlot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(2, 0, 0, 0)
				[
					TextWidget
				];

				return LayoutWidget;
			}

			virtual bool OnNameTextVerifyChanged(const FText& InNewText, FText& OutErrorMessage) override
			{
				if (MetasoundAction.IsValid())
				{
					if (UMetasoundEditorGraphMember* GraphMember = MetasoundAction->GetGraphMember())
					{
						return GraphMember->CanRename(InNewText, OutErrorMessage);
					}
				}

				return false;
			}
		};

		const FName FEditor::EditorName = "MetaSoundEditor";

		void FEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
		{
			WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_MetasoundEditor", "MetaSound Editor"));
			auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

			FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

			InTabManager->RegisterTabSpawner(TabFactory::Names::GraphCanvas, FOnSpawnTab::CreateLambda([InPlayTimeWidget = PlayTimeWidget, InMetasoundGraphEditor = MetasoundGraphEditor](const FSpawnTabArgs& Args)
			{
				return TabFactory::CreateGraphCanvasTab(SNew(SOverlay)
					+ SOverlay::Slot()
					[
						InMetasoundGraphEditor.ToSharedRef()
					]
					+ SOverlay::Slot()
					[
						InPlayTimeWidget.ToSharedRef()
					]
					.Padding(5.0f, 5.0f)
				, Args);
			}))
			.SetDisplayName(LOCTEXT("GraphCanvasTab", "Viewport"))
			.SetGroup(WorkspaceMenuCategoryRef)
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.EventGraph_16x"));

			InTabManager->RegisterTabSpawner(TabFactory::Names::Details, FOnSpawnTab::CreateLambda([InMetasoundDetails = MetasoundDetails](const FSpawnTabArgs& Args)
			{
				return TabFactory::CreateDetailsTab(InMetasoundDetails, Args);
			}))
			.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
			.SetGroup(WorkspaceMenuCategoryRef)
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

			InTabManager->RegisterTabSpawner(TabFactory::Names::Members, FOnSpawnTab::CreateLambda([InGraphMembersMenu = GraphMembersMenu](const FSpawnTabArgs& Args)
			{
				return TabFactory::CreateMembersTab(InGraphMembersMenu, Args);
			}))
			.SetDisplayName(LOCTEXT("MembersTab", "Members"))
			.SetGroup(WorkspaceMenuCategoryRef)
			.SetIcon(FSlateIcon("MetaSoundStyle", "MetasoundEditor.Metasound.Icon"));

			InTabManager->RegisterTabSpawner(TabFactory::Names::Analyzers, FOnSpawnTab::CreateLambda([InAnalyzerWidget = BuildAnalyzerWidget()](const FSpawnTabArgs& Args)
			{
				return TabFactory::CreateAnalyzersTab(InAnalyzerWidget, Args);
			}))
			.SetDisplayName(LOCTEXT("AnalyzersTab", "Analyzers"))
			.SetGroup(WorkspaceMenuCategoryRef)
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Kismet.Tabs.Palette"));

			InTabManager->RegisterTabSpawner(TabFactory::Names::Interfaces, FOnSpawnTab::CreateLambda([InInterfacesDetails = InterfacesDetails](const FSpawnTabArgs& Args)
			{
				return TabFactory::CreateInterfacesTab(InInterfacesDetails, Args);
			}))
			.SetDisplayName(LOCTEXT("InterfacesTab", "Interfaces"))
			.SetGroup(WorkspaceMenuCategoryRef)
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Interface"));
		}

		void FEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
		{
			using namespace Metasound::Editor;

			FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

			InTabManager->UnregisterTabSpawner(TabFactory::Names::Analyzers);
			InTabManager->UnregisterTabSpawner(TabFactory::Names::GraphCanvas);
			InTabManager->UnregisterTabSpawner(TabFactory::Names::Details);
			InTabManager->UnregisterTabSpawner(TabFactory::Names::Members);
			InTabManager->UnregisterTabSpawner(TabFactory::Names::Interfaces);
		}

		TSharedPtr<SWidget> FEditor::BuildAnalyzerWidget() const
		{
			if (!OutputMeter.IsValid() || !OutputMeter->GetWidget().IsValid())
			{
				return SNullWidget::NullWidget->AsShared();
			}

			const ISlateStyle* MetaSoundStyle = FSlateStyleRegistry::FindSlateStyle("MetaSoundStyle");
			FLinearColor BackgroundColor = FLinearColor::Transparent;
			if (ensure(MetaSoundStyle))
			{
				BackgroundColor = MetaSoundStyle->GetColor("MetasoundEditor.Analyzers.BackgroundColor");
			}

			return SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SColorBlock)
				.Color(BackgroundColor)
			]
			+ SOverlay::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Fill)
				[
					OutputMeter->GetWidget().ToSharedRef()
				]
			];
		}

		bool FEditor::IsPlaying() const
		{
			if (Metasound)
			{
				if (const UAudioComponent* PreviewComponent = GEditor->GetPreviewAudioComponent())
				{
					if (PreviewComponent->IsPlaying())
					{
						if (const USoundBase* Sound = PreviewComponent->Sound)
						{
							return Sound->GetUniqueID() == Metasound->GetUniqueID();
						}
					}
				}
			}

			return false;
		}

		FEditor::~FEditor()
		{
			if (IsPlaying())
			{
				Stop();
			}

			GraphConnectionManager.Reset();
			InterfacesView.Reset();
			DestroyAnalyzers();
			check(GEditor);
			GEditor->UnregisterForUndo(this);
		}

		void FEditor::InitMetasoundEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UObject* ObjectToEdit)
		{
			using namespace Metasound::Frontend;

			check(ObjectToEdit);
			checkf(IMetasoundUObjectRegistry::Get().IsRegisteredClass(ObjectToEdit), TEXT("Object passed in was not registered as a valid metasound interface!"));

			IMetasoundEditorModule& MetaSoundEditorModule = FModuleManager::GetModuleChecked<IMetasoundEditorModule>("MetaSoundEditor");
			bPrimingRegistry = MetaSoundEditorModule.GetAssetRegistryPrimeStatus() <= EAssetPrimeStatus::InProgress;
			if (MetaSoundEditorModule.GetAssetRegistryPrimeStatus() < EAssetPrimeStatus::InProgress)
			{
				MetaSoundEditorModule.PrimeAssetRegistryAsync();
			}

			// Support undo/redo
			Metasound = ObjectToEdit;
			Metasound->SetFlags(RF_Transactional);

			GEditor->RegisterForUndo(this);

			FGraphEditorCommands::Register();
			FEditorCommands::Register();

			// If sound was already playing in the editor (ex. from ContentBrowser),
			// restart to synchronize visual state of editor (ex. volume meter analysis
			// via transient AudioBus, PlayTime, etc.). If playing, registration is not
			// required here as it will be handled in play call below after UI is initialized
			const bool bRestartSound = IsPlaying();
			if (!bRestartSound)
			{
				FGraphBuilder::RegisterGraphWithFrontend(*Metasound);
			}
			RefreshEditorContext();

			BindGraphCommands();
			CreateInternalWidgets();
			CreateAnalyzers();

			// Has to be run after widgets are initialized to properly display
			if (bPrimingRegistry)
			{
				NotifyAssetPrimeInProgress();
			}

			const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_MetasoundEditor_Layout_v10")
				->AddArea
				(
					FTabManager::NewPrimaryArea()
					->SetOrientation(Orient_Vertical)
					->Split(FTabManager::NewSplitter()
						->SetOrientation(Orient_Horizontal)
						->Split
						(
							FTabManager::NewSplitter()
							->SetSizeCoefficient(0.15f)
							->SetOrientation(Orient_Vertical)
							->Split
							(
								FTabManager::NewStack()
								->SetSizeCoefficient(0.25f)
								->SetHideTabWell(false)
								->AddTab(TabFactory::Names::Members, ETabState::OpenedTab)
							)
							->Split
							(
								FTabManager::NewStack()
								->SetSizeCoefficient(0.1f)
								->SetHideTabWell(true)
								->AddTab(TabFactory::Names::Interfaces, ETabState::OpenedTab)
							)
							->Split
							(
								FTabManager::NewStack()
								->SetSizeCoefficient(0.50f)
								->SetHideTabWell(false)
								->AddTab(TabFactory::Names::Details, ETabState::OpenedTab)
							)
						)
						->Split
						(
							FTabManager::NewStack()
							->SetSizeCoefficient(0.77f)
							->SetHideTabWell(true)
							->AddTab(TabFactory::Names::GraphCanvas, ETabState::OpenedTab)
						)
						->Split
						(
							FTabManager::NewStack()
							->SetSizeCoefficient(0.08f)
							->SetHideTabWell(true)
							->AddTab(TabFactory::Names::Analyzers, ETabState::OpenedTab)
						)
					)
				);

			constexpr bool bCreateDefaultStandaloneMenu = true;
			constexpr bool bCreateDefaultToolbar = true;
			constexpr bool bToolbarFocusable = false;
			constexpr bool bUseSmallToolbarIcons = true;
			FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, TEXT("MetasoundEditorApp"), StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectToEdit, bToolbarFocusable, bUseSmallToolbarIcons);

			ExtendToolbar();
			RegenerateMenusAndToolbars();

			NotifyDocumentVersioned();

			if (bRestartSound)
			{
				Play();
			}
		}

		UObject* FEditor::GetMetasoundObject() const
		{
			return Metasound;
		}

		void FEditor::SetSelection(const TArray<UObject*>& SelectedObjects)
		{
			if (GraphMembersMenu.IsValid())
			{
				// Only support menu selection of a single object until multiselect functionality is added 
				if (SelectedObjects.Num() == 1)
				{
					if (UMetasoundEditorGraphMember* Member = Cast<UMetasoundEditorGraphMember>(SelectedObjects[0]))
					{
						const FName ActionName = Member->GetMemberName();
						GraphMembersMenu->SelectItemByName(ActionName, ESelectInfo::Direct, static_cast<int32>(Member->GetSectionID()));
					}
				}
			}

			if (MetasoundDetails.IsValid())
			{
				MetasoundDetails->SetObjects(SelectedObjects);
				MetasoundDetails->HideFilterArea(false);
			}
		}

		bool FEditor::GetBoundsForSelectedNodes(FSlateRect& Rect, float Padding)
		{
			return MetasoundGraphEditor->GetBoundsForSelectedNodes(Rect, Padding);
		}

		FName FEditor::GetToolkitFName() const
		{
			return FEditor::EditorName;
		}

		FText FEditor::GetBaseToolkitName() const
		{
			return LOCTEXT("AppLabel", "MetaSound Editor");
		}

		FString FEditor::GetWorldCentricTabPrefix() const
		{
			return LOCTEXT("WorldCentricTabPrefix", "MetaSound ").ToString();
		}

		FLinearColor FEditor::GetWorldCentricTabColorScale() const
		{
			if (const ISlateStyle* MetasoundStyle = FSlateStyleRegistry::FindSlateStyle("MetaSoundStyle"))
			{
				if (UMetaSoundSource* MetaSoundSource = Cast<UMetaSoundSource>(Metasound))
				{
					return MetasoundStyle->GetColor("MetaSoundSource.Color");
				}

				if (UMetaSoundPatch* MetaSoundPatch = Cast<UMetaSoundPatch>(Metasound))
				{
					return MetasoundStyle->GetColor("MetaSoundPatch.Color");
				}
			}

			return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f);
		}

		const FSlateBrush* FEditor::GetDefaultTabIcon() const
		{
			FString IconName = TEXT("MetasoundEditor");
			if (IsPlaying())
			{
				IconName += TEXT(".Play");
			}
			else
			{
				if (UMetaSoundSource* MetaSoundSource = Cast<UMetaSoundSource>(Metasound))
				{
					IconName += TEXT(".MetasoundSource");
				}
				else if (UMetaSoundPatch* MetaSoundPatch = Cast<UMetaSoundPatch>(Metasound))
				{
					IconName += TEXT(".MetasoundPatch");
				}

				const FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(Metasound);
				check(MetaSoundAsset);
				if (MetaSoundAsset->GetDocumentChecked().RootGraph.PresetOptions.bIsPreset)
				{
					IconName += TEXT(".Preset");
				}

				IconName += TEXT(".Icon");
			}

			return &Style::GetSlateBrushSafe(FName(*IconName));
		}

		FLinearColor FEditor::GetDefaultTabColor() const
		{
			if (IsPlaying())
			{
				if (const ISlateStyle* MetasoundStyle = FSlateStyleRegistry::FindSlateStyle("MetaSoundStyle"))
				{
					if (UMetaSoundSource* MetaSoundSource = Cast<UMetaSoundSource>(Metasound))
					{
						return MetasoundStyle->GetColor("MetaSoundSource.Color");
					}

					if (UMetaSoundPatch* MetaSoundPatch = Cast<UMetaSoundPatch>(Metasound))
					{
						return MetasoundStyle->GetColor("MetaSoundPatch.Color");
					}
				}
			}

			return FAssetEditorToolkit::GetDefaultTabColor();
		}

		FName FEditor::GetEditorName() const 
		{
			return FEditor::EditorName;
		}

		void FEditor::AddReferencedObjects(FReferenceCollector& Collector)
		{
			Collector.AddReferencedObject(Metasound);
		}

		void FEditor::PostUndo(bool bSuccess)
		{
			if (MetasoundGraphEditor.IsValid())
			{
				MetasoundGraphEditor->ClearSelectionSet();
				MetasoundGraphEditor->NotifyGraphChanged();
			}

			FSlateApplication::Get().DismissAllMenus();
		}

		void FEditor::NotifyAssetPrimeInProgress()
		{
			if (MetasoundGraphEditor.IsValid())
			{
				FNotificationInfo Info(LOCTEXT("MetaSoundScanInProgressNotificationText", "Registering MetaSound Assets..."));
				Info.SubText = LOCTEXT("MetaSoundScanInProgressNotificationSubText", "Class selector results may be incomplete");
				Info.bUseThrobber = true;
				Info.bFireAndForget = true;
				Info.bUseSuccessFailIcons = false;
				Info.ExpireDuration = 3.0f;
				Info.FadeOutDuration = 1.0f;

				MetasoundGraphEditor->AddNotification(Info, false /* bSuccess */);
			}
		}

		void FEditor::NotifyAssetPrimeComplete()
		{
			if (MetasoundGraphEditor.IsValid())
			{
				FNotificationInfo Info(LOCTEXT("MetaSoundScanInProgressNotification", "MetaSound Asset Registration Complete"));
				Info.bFireAndForget = true;
				Info.bUseSuccessFailIcons = true;
				Info.ExpireDuration = 3.0f;
				Info.FadeOutDuration = 1.0f;

				MetasoundGraphEditor->AddNotification(Info, true /* bSuccess */);
			}
		}

		void FEditor::NotifyDocumentVersioned()
		{
			if (MetasoundGraphEditor.IsValid())
			{
				UMetasoundEditorGraph& MetaSoundGraph = GetMetaSoundGraphChecked();
				const bool bVersionedOnLoad = MetaSoundGraph.GetVersionedOnLoad();
				if (bVersionedOnLoad)
				{
					MetaSoundGraph.ClearVersionedOnLoad();
					const FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(Metasound);
					check(MetaSoundAsset);

					const FString VersionString = MetaSoundAsset->GetDocumentChecked().Metadata.Version.Number.ToString();
					FText Msg = FText::Format(LOCTEXT("MetaSoundDocumentVersioned", "Document versioned to '{0}' on load."), FText::FromString(VersionString));
					FNotificationInfo Info(Msg);
					Info.bFireAndForget = true;
					Info.bUseSuccessFailIcons = false;
					Info.ExpireDuration = 5.0f;

					MetasoundGraphEditor->AddNotification(Info, false /* bSuccess */);

					MetaSoundAsset->MarkMetasoundDocumentDirty();
				}
			}
		}

		void FEditor::NotifyNodePasteFailure_MultipleVariableSetters()
		{
			FNotificationInfo Info(LOCTEXT("NodePasteFailed_MultipleVariableSetters", "Node(s) not pasted: Only one variable setter node possible per graph."));
			Info.bFireAndForget = true;
			Info.bUseSuccessFailIcons = false;
			Info.ExpireDuration = 5.0f;

			MetasoundGraphEditor->AddNotification(Info, false /* bSuccess */);
		}

		void FEditor::NotifyNodePasteFailure_ReferenceLoop()
		{
			FNotificationInfo Info(LOCTEXT("NodePasteFailed_ReferenceLoop", "Node(s) not pasted: Nodes would create asset reference cycle."));
			Info.bFireAndForget = true;
			Info.bUseSuccessFailIcons = false;
			Info.ExpireDuration = 5.0f;

			MetasoundGraphEditor->AddNotification(Info, false /* bSuccess */);
		}

		void FEditor::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
		{
			if (MetasoundGraphEditor.IsValid() && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
			{
				// If a property change event occurs outside of the metasound UEdGraph and results in the metasound document changing,
				// then the document and the UEdGraph need to be synchronized. There may be a better trigger for this call to reduce
				// the number of times the graph is synchronized.
				if (Metasound)
				{
					if (FMetasoundAssetBase* Asset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(Metasound))
					{
						Asset->GetModifyContext().SetDocumentModified();
					}
				}
			}
		}

		void FEditor::CreateInternalWidgets()
		{
			CreateGraphEditorWidget();

			FDetailsViewArgs Args;
			Args.bHideSelectionTip = true;
			Args.NotifyHook = this;

			SAssignNew(GraphMembersMenu, SGraphActionMenu, false)
				.AlphaSortItems(true)
				.OnActionDoubleClicked(this, &FEditor::OnMemberActionDoubleClicked)
				.OnActionDragged(this, &FEditor::OnActionDragged)
				.OnActionMatchesName(this, &FEditor::HandleActionMatchesName)
				.OnActionSelected(this, &FEditor::OnActionSelected)
// 				.OnCategoryTextCommitted(this, &FEditor::OnCategoryNameCommitted)
				.OnCollectAllActions(this, &FEditor::CollectAllActions)
				.OnCollectStaticSections(this, &FEditor::CollectStaticSections)
 				.OnContextMenuOpening(this, &FEditor::OnContextMenuOpening)
				.OnCreateWidgetForAction(this, &FEditor::OnCreateWidgetForAction)
  				.OnCanRenameSelectedAction(this, &FEditor::CanRenameOnActionNode)
				.OnGetFilterText(this, &FEditor::GetFilterText)
				.OnGetSectionTitle(this, &FEditor::OnGetSectionTitle)
				.OnGetSectionWidget(this, &FEditor::OnGetMenuSectionWidget)
				.OnCreateCustomRowExpander_Lambda([](const FCustomExpanderData& InCustomExpanderData)
				{
					return SNew(SMetasoundActionMenuExpanderArrow, InCustomExpanderData);
				})
				.UseSectionStyling(true);

			FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
			MetasoundDetails = PropertyModule.CreateDetailView(Args);
			InterfacesDetails = PropertyModule.CreateDetailView(Args);
			if (InterfacesDetails.IsValid())
			{
				InterfacesView = TStrongObjectPtr(NewObject<UMetasoundInterfacesView>());
				InterfacesView->SetMetasound(Metasound);
				const TArray<UObject*> InterfacesViewObj{ InterfacesView.Get() };

				InterfacesDetails->SetObjects(InterfacesViewObj);
				InterfacesDetails->HideFilterArea(true);
			}

			Palette = SNew(SMetasoundPalette);
		}

		// TODO: Tie in rename on GraphActionMenu.  For now, just renameable via field in details
		bool FEditor::CanRenameOnActionNode(TWeakPtr<FGraphActionNode> InSelectedNode) const
		{
			return false;
		}

		void FEditor::CreateAnalyzers()
		{
			if (UMetaSoundSource* MetaSoundSource = Cast<UMetaSoundSource>(Metasound))
			{
				if (!OutputMeter.IsValid())
				{
					OutputMeter = MakeShared<FEditorMeter>();
				}
				OutputMeter->Init(MetaSoundSource->NumChannels);
			}
			else
			{
				OutputMeter.Reset();
			}
		}

		void FEditor::DestroyAnalyzers()
		{
			if (OutputMeter.IsValid())
			{
				OutputMeter->Teardown();
			}
		}

		void FEditor::ExtendToolbar()
		{
			TSharedPtr<FExtender> ToolbarExtender = MakeShared<FExtender>();
			ToolbarExtender->AddToolBarExtension
			(
				"Asset",
				EExtensionHook::After,
				GetToolkitCommands(),
				FToolBarExtensionDelegate::CreateLambda([this](FToolBarBuilder& ToolbarBuilder)
				{
					// TODO: Clean-up json importer/exporter and re-enable this
 					ToolbarBuilder.BeginSection("Utilities");
 					{
// 						ToolbarBuilder.AddToolBarButton
// 						(
// 							FEditorCommands::Get().Import,
// 							NAME_None,
// 							TAttribute<FText>(),
// 							TAttribute<FText>(),
// 							TAttribute<FSlateIcon>::Create([this]() { return GetImportStatusImage(); }),
// 							"ImportMetasound"
// 						);
// 
// 						ToolbarBuilder.AddToolBarButton
// 						(
// 							FEditorCommands::Get().Export,
// 							NAME_None,
// 							TAttribute<FText>(),
// 							TAttribute<FText>(),
// 							TAttribute<FSlateIcon>::Create([this]() { return GetExportStatusImage(); }),
// 							"ExportMetasound"
// 						);

						if (!IsGraphEditable())
						{
							ToolbarBuilder.AddToolBarButton
 							(
 								FEditorCommands::Get().ConvertFromPreset,
 								NAME_None,
 								TAttribute<FText>(),
 								TAttribute<FText>(),
 								TAttribute<FSlateIcon>::Create([this]() { return GetExportStatusImage(); }),
 								"ConvertFromPreset"
							);
						}
 					}
 					ToolbarBuilder.EndSection();

					ToolbarBuilder.BeginSection("Settings");
					{
						if (Metasound->IsA<USoundBase>())
						{
							ToolbarBuilder.AddToolBarButton(
								FEditorCommands::Get().EditSourceSettings,
								NAME_None,
								TAttribute<FText>(),
								TAttribute<FText>(),
								TAttribute<FSlateIcon>::Create([this]() { return GetSettingsImage(); }),
								"EditSourceSettings"
							);
						}

						ToolbarBuilder.AddToolBarButton(
							FEditorCommands::Get().EditMetasoundSettings,
							NAME_None,
							TAttribute<FText>(),
							TAttribute<FText>(),
							TAttribute<FSlateIcon>::Create([this]() { return GetSettingsImage(); }),
							"EditMetasoundSettings"
						);
					}
					ToolbarBuilder.EndSection();

					if (Metasound->IsA<USoundBase>())
					{
						ToolbarBuilder.BeginSection("Audition");
						{
							ToolbarBuilder.BeginStyleOverride("Toolbar.BackplateLeft");
							{
								ToolbarBuilder.AddToolBarButton(
									FEditorCommands::Get().Play,
									NAME_None,
									TAttribute<FText>(),
									TAttribute<FText>::Create([this] { return GetGraphStatusDescription(); }),
									TAttribute<FSlateIcon>::Create([this]() { return GetPlayIcon(); })
								);
							}
							ToolbarBuilder.EndStyleOverride();

							ToolbarBuilder.BeginStyleOverride("Toolbar.BackplateRight");
							{
								ToolbarBuilder.AddToolBarButton(
									FEditorCommands::Get().Stop,
									NAME_None,
									TAttribute<FText>(),
									TAttribute<FText>(),
									TAttribute<FSlateIcon>::Create([this]() { return GetStopIcon(); })
								);
							}
							ToolbarBuilder.EndStyleOverride();
						}
						ToolbarBuilder.EndSection();
					}
				})
			);

			AddToolbarExtender(ToolbarExtender);
		}

		FSlateIcon FEditor::GetImportStatusImage() const
		{
			const FName IconName = "MetasoundEditor.Import";
			return FSlateIcon("MetaSoundStyle", IconName);
		}

		FSlateIcon FEditor::GetSettingsImage() const
		{
			const FName IconName = "MetasoundEditor.Settings";
			return FSlateIcon("MetaSoundStyle", IconName);
		}

		FSlateIcon FEditor::GetExportStatusImage() const
		{
			FName IconName = "MetasoundEditor.Export";
			if (!bPassedValidation)
			{
				IconName = "MetasoundEditor.ExportError";
			}

			return FSlateIcon("MetaSoundStyle", IconName);
		}

		void FEditor::BindGraphCommands()
		{
			const FEditorCommands& Commands = FEditorCommands::Get();

			ToolkitCommands->MapAction(
				Commands.Play,
				FExecuteAction::CreateSP(this, &FEditor::Play));

			ToolkitCommands->MapAction(
				Commands.Stop,
				FExecuteAction::CreateSP(this, &FEditor::Stop));

			ToolkitCommands->MapAction(
				Commands.Import,
				FExecuteAction::CreateSP(this, &FEditor::Import));

			ToolkitCommands->MapAction(
				Commands.Export,
				FExecuteAction::CreateSP(this, &FEditor::Export));

			ToolkitCommands->MapAction(
				Commands.TogglePlayback,
				FExecuteAction::CreateSP(this, &FEditor::TogglePlayback));

			ToolkitCommands->MapAction(
				FGenericCommands::Get().Undo,
				FExecuteAction::CreateSP(this, &FEditor::UndoGraphAction));

			ToolkitCommands->MapAction(
				FGenericCommands::Get().Redo,
				FExecuteAction::CreateSP(this, &FEditor::RedoGraphAction));

			ToolkitCommands->MapAction(
				Commands.EditMetasoundSettings,
				FExecuteAction::CreateSP(this, &FEditor::EditMetasoundSettings));

			ToolkitCommands->MapAction(
				Commands.EditSourceSettings,
				FExecuteAction::CreateSP(this, &FEditor::EditSourceSettings));

			ToolkitCommands->MapAction(
				Commands.ConvertFromPreset,
				FExecuteAction::CreateSP(this, &FEditor::ConvertFromPreset));

			ToolkitCommands->MapAction(FGenericCommands::Get().Delete,
				FExecuteAction::CreateSP(this, &FEditor::DeleteSelectedInterfaceItems),
				FCanExecuteAction::CreateSP(this, &FEditor::CanDeleteInterfaceItems));

			ToolkitCommands->MapAction(FGenericCommands::Get().Rename,
				FExecuteAction::CreateSP(this, &FEditor::RenameSelectedInterfaceItem),
				FCanExecuteAction::CreateSP(this, &FEditor::CanRenameSelectedInterfaceItems));

			ToolkitCommands->MapAction(
				FEditorCommands::Get().UpdateNodeClass,
				FExecuteAction::CreateSP(this, &FEditor::UpdateSelectedNodeClasses));
		}

		void FEditor::Import()
		{
			// TODO: Prompt OFD and provide path from user
			const FString InputPath = FPaths::ProjectIntermediateDir() / TEXT("MetaSounds") + FPaths::ChangeExtension(Metasound->GetPathName(), FMetasoundAssetBase::FileExtension);
			
			// TODO: use the same directory as the currently open MetaSound
			const FString OutputPath = FString("/Game/ImportedMetaSound/GeneratedMetaSound");

			FMetasoundFrontendDocument MetasoundDoc;

			if (Frontend::ImportJSONAssetToMetasound(InputPath, MetasoundDoc))
			{
				TSet<UClass*> ImportClasses;

				for (const FMetasoundFrontendVersion& InterfaceVersion : MetasoundDoc.Interfaces)
				{
					TArray<UClass*> InterfaceClasses = IMetasoundUObjectRegistry::Get().FindSupportedInterfaceClasses(InterfaceVersion);
					ImportClasses.Append(MoveTemp(InterfaceClasses));
				}

				if (ImportClasses.Num() < 1)
				{
					TArray<FString> InterfaceNames;
					Algo::Transform(MetasoundDoc.Interfaces, InterfaceNames, [] (const FMetasoundFrontendVersion& InterfaceVersion) { return InterfaceVersion.ToString(); });
					UE_LOG(LogMetaSound, Warning, TEXT("Cannot create UObject from MetaSound document. No UClass supports interface(s) \"%s\""), *FString::Join(InterfaceNames, TEXT(",")));
				}
				else
				{
					UClass* AnyClass = nullptr;
					for (UClass* ImportClass : ImportClasses)
					{
						AnyClass = ImportClass;
						if (ImportClasses.Num() > 1)
						{
							// TODO: Modal dialog to give user choice of import type.
							TArray<FString> InterfaceNames;
							Algo::Transform(MetasoundDoc.Interfaces, InterfaceNames, [](const FMetasoundFrontendVersion& InterfaceVersion) { return InterfaceVersion.ToString(); });
							UE_LOG(LogMetaSound, Warning, TEXT("Duplicate UClass support interface(s) \"%s\" with UClass \"%s\""), *FString::Join(InterfaceNames, TEXT(",")), *ImportClass->GetName());
						}
					}

					IMetasoundUObjectRegistry::Get().NewObject(AnyClass, MetasoundDoc, OutputPath);
				}
			}
			else
			{
				UE_LOG(LogMetaSound, Warning, TEXT("Could not import MetaSound at path: %s"), *InputPath);
			}
		}

		void FEditor::Export()
		{
			FMetasoundAssetBase* MetasoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(Metasound);
			check(MetasoundAsset);

			static const FString MetasoundExtension(TEXT(".metasound"));

			// TODO: We could just make this an object.
			const FString Path = FPaths::ProjectSavedDir() / TEXT("MetaSounds") + FPaths::ChangeExtension(Metasound->GetPathName(), MetasoundExtension);
			MetasoundAsset->GetDocumentHandle()->ExportToJSONAsset(Path);
		}

		FText FEditor::GetGraphStatusDescription() const
		{
			switch (HighestMessageSeverity)
			{
				case EMessageSeverity::Error:
				{
					return LOCTEXT("MetaSoundPlayStateTooltip_Error", "MetaSound contains errors and cannot be played.");
				}

				case EMessageSeverity::PerformanceWarning:
				case EMessageSeverity::Warning:
				{
					return LOCTEXT("MetaSoundPlayStateTooltip_Warning", "MetaSound contains warnings and playback behavior may be undesired.");
				}
				break;

				case EMessageSeverity::Info:
				default:
				{
					return FEditorCommands::Get().Play->GetDescription();
				}
				break;
			}
		}

		const FSlateIcon& FEditor::GetPlayIcon() const
		{
			switch (HighestMessageSeverity)
			{
				case EMessageSeverity::Error:
				{
					static const FSlateIcon Icon = Style::CreateSlateIcon("MetasoundEditor.Play.Error");
					return Icon;
				}

				case EMessageSeverity::PerformanceWarning:
				case EMessageSeverity::Warning:
				{
					if (IsPlaying())
					{
						static const FSlateIcon Icon = Style::CreateSlateIcon("MetasoundEditor.Play.Active.Warning");
						return Icon;
					}
					else
					{
						static const FSlateIcon Icon = Style::CreateSlateIcon("MetasoundEditor.Play.Inactive.Warning");
						return Icon;
					}
				}
				break;

				case EMessageSeverity::Info:
				default:
				{
					if (IsPlaying())
					{
						static const FSlateIcon Icon = Style::CreateSlateIcon("MetasoundEditor.Play.Active.Valid");
						return Icon;
					}
					else
					{
						static const FSlateIcon Icon = Style::CreateSlateIcon("MetasoundEditor.Play.Inactive.Valid");
						return Icon;
					}
				}
				break;
			}
		}

		const FSlateIcon& FEditor::GetStopIcon() const
		{
			switch (HighestMessageSeverity)
			{
				case EMessageSeverity::Error:
				{
					static const FSlateIcon Icon = Style::CreateSlateIcon("MetasoundEditor.Stop.Disabled");
					return Icon;
				}
				break;

				case EMessageSeverity::PerformanceWarning:
				case EMessageSeverity::Warning:
				case EMessageSeverity::Info:
				default:
				{
					if (IsPlaying())
					{
						static const FSlateIcon Icon = Style::CreateSlateIcon("MetasoundEditor.Stop.Active");
						return Icon;
					}
					else
					{
						static const FSlateIcon Icon = Style::CreateSlateIcon("MetasoundEditor.Stop.Inactive");
						return Icon;
					}
				}
				break;
			}
		}

		void FEditor::Play()
		{
			if (USoundBase* MetasoundToPlay = Cast<USoundBase>(Metasound))
			{
				HighestMessageSeverity = GetMetaSoundGraphChecked().GetHighestMessageSeverity();
				if (HighestMessageSeverity == EMessageSeverity::Error)
				{
					return;
				}

				// Even though the MetaSoundSource will attempt to register via InitResources
				// later in this execution (and deeper in the stack), this call forces
				// re-registering to make sure everything is up-to-date.
				FGraphBuilder::RegisterGraphWithFrontend(*Metasound);

				// Set the send to the audio bus that is used for analyzing the metasound output
				check(GEditor);
				if (UAudioComponent* PreviewComp = GEditor->PlayPreviewSound(MetasoundToPlay))
				{
					PlayTime = 0.0;

					UObject* ParamInterfaceObject = PreviewComp;
					if (ensure(ParamInterfaceObject))
					{
						SetPreviewID(ParamInterfaceObject->GetUniqueID());
					}

					if (UAudioBus* AudioBus = OutputMeter->GetAudioBus())
					{
						PreviewComp->SetAudioBusSendPostEffect(AudioBus, 1.0f);
					}
				
					FMetasoundAssetBase* MetasoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(Metasound);
					check(MetasoundAsset);

					FAudioDevice* AudioDevice = PreviewComp->GetAudioDevice();
					check(AudioDevice);

					const FName& AudioBufferTypeName = GetMetasoundDataTypeName<FAudioBuffer>();
					const FSampleRate SampleRate = static_cast<FSampleRate>(AudioDevice->GetSampleRate());
					GraphConnectionManager = MakeUnique<FGraphConnectionManager>(*MetasoundAsset, *PreviewComp, SampleRate);
				}

				MetasoundGraphEditor->RegisterActiveTimer(0.0f,
					FWidgetActiveTimerDelegate::CreateLambda([this](double InCurrentTime, float InDeltaTime)
					{
						if (IsPlaying())
						{
							if (PlayTimeWidget.IsValid())
							{
								PlayTime += InDeltaTime;
								FString PlayTimeString = FTimespan::FromSeconds(PlayTime).ToString();

								// Remove leading '+'
								PlayTimeString.ReplaceInline(TEXT("+"), TEXT(""));
								PlayTimeWidget->SetText(FText::FromString(PlayTimeString));
							}
							return EActiveTimerReturnType::Continue;
						}
						else
						{
							SetPreviewID(INDEX_NONE);
							PlayTime = 0.0;
							PlayTimeWidget->SetText(FText::GetEmpty());
							GraphConnectionManager = MakeUnique<FGraphConnectionManager>();

							return EActiveTimerReturnType::Stop;
						}
					})
				);

				TSharedPtr<SAudioMeter> OutputMeterWidget = OutputMeter->GetWidget();
				if (OutputMeterWidget.IsValid())
				{
					if (!OutputMeterWidget->bIsActiveTimerRegistered)
					{
						OutputMeterWidget->RegisterActiveTimer(0.0f,
							FWidgetActiveTimerDelegate::CreateLambda([this](double InCurrentTime, float InDeltaTime)
							{
								if (IsPlaying())
								{
									return EActiveTimerReturnType::Continue;
								}
								else
								{
									if (OutputMeter->GetWidget().IsValid())
									{
										OutputMeter->GetWidget()->bIsActiveTimerRegistered = false;
									}
									return EActiveTimerReturnType::Stop;
								}
							})
						);
						OutputMeterWidget->bIsActiveTimerRegistered = true;
					}
				}
			}
		}

		void FEditor::SetPreviewID(uint32 InPreviewID)
		{
			if (!Metasound)
			{
				return;
			}

			GetMetaSoundGraphChecked().SetPreviewID(InPreviewID);
		}

		UMetasoundEditorGraph& FEditor::GetMetaSoundGraphChecked()
		{
			FMetasoundAssetBase* MetasoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(Metasound);
			check(MetasoundAsset);

			UEdGraph* Graph = MetasoundAsset->GetGraph();
			check(Graph);

			return *CastChecked<UMetasoundEditorGraph>(MetasoundAsset->GetGraph());
		}

		void FEditor::ExecuteNode()
		{
			const FGraphPanelSelectionSet SelectedNodes = MetasoundGraphEditor->GetSelectedNodes();
			for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
			{
				ExecuteNode(CastChecked<UEdGraphNode>(*NodeIt));
			}
		}

		bool FEditor::CanExecuteNode() const
		{
			return true;
		}

		double FEditor::GetPlayTime() const
		{
			return PlayTime;
		}

		TSharedPtr<SGraphEditor> FEditor::GetGraphEditor() const
		{
			return MetasoundGraphEditor;
		}

		void FEditor::Stop()
		{
			check(GEditor);
			GEditor->ResetPreviewAudioComponent();
			SetPreviewID(INDEX_NONE);
		}

		void FEditor::TogglePlayback()
		{
			check(GEditor);

			if (IsPlaying())
			{
				Stop();
			}
			else
			{
				Play();
			}
		}

		void FEditor::ExecuteNode(UEdGraphNode* InNode)
		{
			using namespace Metasound;
			using namespace Metasound::Frontend;

			if (!GEditor)
			{
				return;
			}

			if (UMetasoundEditorGraphExternalNode* ExternalNode = Cast<UMetasoundEditorGraphExternalNode>(InNode))
			{
				FConstNodeHandle NodeHandle = ExternalNode->GetConstNodeHandle();
				FNodeRegistryKey Key = FMetasoundFrontendRegistryContainer::Get()->GetRegistryKey(NodeHandle->GetClassMetadata());

				FNodeClassInfo ClassInfo;
				if (FMetasoundFrontendRegistryContainer::Get()->FindNodeClassInfoFromRegistered(Key, ClassInfo))
				{
					if (ClassInfo.AssetClassID.IsValid())
					{
						if (UObject* AssetObject = ClassInfo.LoadAsset())
						{
							GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(AssetObject);
						}
					}
				}
			}
		}

		void FEditor::EditObjectSettings()
		{
			if (GraphMembersMenu.IsValid())
			{
				GraphMembersMenu->SelectItemByName(FName());
			}

			if (MetasoundGraphEditor.IsValid())
			{
				bManuallyClearingGraphSelection = true;
				MetasoundGraphEditor->ClearSelectionSet();
				bManuallyClearingGraphSelection = false;
			}

			// Clear selection first to force refresh of customization
			// if swapping from one object-level edit mode to the other
			// (ex. Metasound Settings to General Settings)
			SetSelection({ });
			SetSelection({ Metasound });
		}

		void FEditor::ConvertFromPreset()
		{
			check(GEditor);

			if (Metasound)
			{
				FMetasoundAssetBase* MetasoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(Metasound);
				check(MetasoundAsset);
				MetasoundAsset->ConvertFromPreset();

				// Hack until toolbar is polished up & corner text properly dynamically updates
				if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
				{
					AssetEditorSubsystem->CloseAllEditorsForAsset(Metasound);
				}
			}
		}

		void FEditor::EditSourceSettings()
		{
			if (UMetasoundEditorSettings* EditorSettings = GetMutableDefault<UMetasoundEditorSettings>())
			{
				EditorSettings->DetailView = EMetasoundActiveDetailView::General;
			}

			EditObjectSettings();
		}

		void FEditor::EditMetasoundSettings()
		{
			if (UMetasoundEditorSettings* EditorSettings = GetMutableDefault<UMetasoundEditorSettings>())
			{
				EditorSettings->DetailView = EMetasoundActiveDetailView::Metasound;
			}

			EditObjectSettings();
		}

		void FEditor::SyncInBrowser()
		{
			TArray<UObject*> ObjectsToSync;

			const FGraphPanelSelectionSet SelectedNodes = MetasoundGraphEditor->GetSelectedNodes();
			for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
			{
				// TODO: Implement sync to referenced Metasound if selected node is a reference to another metasound
			}

			if (!ObjectsToSync.Num())
			{
				ObjectsToSync.Add(Metasound);
			}

			check(GEditor);
			GEditor->SyncBrowserToObjects(ObjectsToSync);
		}

		void FEditor::AddInput()
		{
		}

		bool FEditor::CanAddInput() const
		{
			return MetasoundGraphEditor->GetSelectedNodes().Num() == 1;
		}

		void FEditor::OnCreateComment()
		{
			if (MetasoundGraphEditor.IsValid())
			{
				if (UEdGraph* Graph = MetasoundGraphEditor->GetCurrentGraph())
				{
					FMetasoundGraphSchemaAction_NewComment CommentAction;
					CommentAction.PerformAction(Graph, nullptr, MetasoundGraphEditor->GetPasteLocation());
				}
			}
		}

		void FEditor::CreateGraphEditorWidget()
		{
			if (!GraphEditorCommands.IsValid())
			{
				GraphEditorCommands = MakeShared<FUICommandList>();

				GraphEditorCommands->MapAction(FEditorCommands::Get().BrowserSync,
					FExecuteAction::CreateSP(this, &FEditor::SyncInBrowser));

				GraphEditorCommands->MapAction(FEditorCommands::Get().EditMetasoundSettings,
					FExecuteAction::CreateSP(this, &FEditor::EditMetasoundSettings));

				if (Metasound->IsA<UMetaSoundSource>())
				{
					GraphEditorCommands->MapAction(FEditorCommands::Get().EditSourceSettings,
						FExecuteAction::CreateSP(this, &FEditor::EditSourceSettings));
				}

				GraphEditorCommands->MapAction(FEditorCommands::Get().AddInput,
					FExecuteAction::CreateSP(this, &FEditor::AddInput),
					FCanExecuteAction::CreateSP(this, &FEditor::CanAddInput));

				// Editing Commands
				GraphEditorCommands->MapAction(FGenericCommands::Get().SelectAll,
					FExecuteAction::CreateLambda([this]() { MetasoundGraphEditor->SelectAllNodes(); }));

				GraphEditorCommands->MapAction(FGenericCommands::Get().Copy,
					FExecuteAction::CreateSP(this, &FEditor::CopySelectedNodes),
					FCanExecuteAction::CreateSP(this, &FEditor::CanCopyNodes));

				GraphEditorCommands->MapAction(FGenericCommands::Get().Cut,
					FExecuteAction::CreateSP(this, &FEditor::CutSelectedNodes),
					FCanExecuteAction::CreateLambda([this]() { return CanCopyNodes() && CanDeleteNodes(); }));

				GraphEditorCommands->MapAction(FGenericCommands::Get().Paste,
					FExecuteAction::CreateLambda([this]() { PasteNodes(); }),
					FCanExecuteAction::CreateSP(this, &FEditor::CanPasteNodes));

				GraphEditorCommands->MapAction(FGenericCommands::Get().Delete,
					FExecuteAction::CreateSP(this, &FEditor::DeleteSelectedNodes),
					FCanExecuteAction::CreateLambda([this]() { return CanDeleteNodes(); }));

				GraphEditorCommands->MapAction(FGenericCommands::Get().Duplicate,
					FExecuteAction::CreateLambda([this] { DuplicateNodes(); }),
					FCanExecuteAction::CreateLambda([this]() { return CanDuplicateNodes(); }));

				GraphEditorCommands->MapAction(FGenericCommands::Get().Rename,
					FExecuteAction::CreateLambda([this] { RenameSelectedNode(); }),
					FCanExecuteAction::CreateLambda([this]() { return CanRenameSelectedNodes(); }));

				// Alignment Commands
				GraphEditorCommands->MapAction(FGraphEditorCommands::Get().AlignNodesTop,
					FExecuteAction::CreateLambda([this]() { MetasoundGraphEditor->OnAlignTop(); }));

				GraphEditorCommands->MapAction(FGraphEditorCommands::Get().AlignNodesMiddle,
					FExecuteAction::CreateLambda([this]() { MetasoundGraphEditor->OnAlignMiddle(); }));

				GraphEditorCommands->MapAction(FGraphEditorCommands::Get().AlignNodesBottom,
					FExecuteAction::CreateLambda([this]() { MetasoundGraphEditor->OnAlignBottom(); }));

				GraphEditorCommands->MapAction(FGraphEditorCommands::Get().AlignNodesLeft,
					FExecuteAction::CreateLambda([this]() { MetasoundGraphEditor->OnAlignLeft(); }));

				GraphEditorCommands->MapAction(FGraphEditorCommands::Get().AlignNodesCenter,
					FExecuteAction::CreateLambda([this]() { MetasoundGraphEditor->OnAlignCenter(); }));

				GraphEditorCommands->MapAction(FGraphEditorCommands::Get().AlignNodesRight,
					FExecuteAction::CreateLambda([this]() { MetasoundGraphEditor->OnAlignRight(); }));

				GraphEditorCommands->MapAction(FGraphEditorCommands::Get().StraightenConnections,
					FExecuteAction::CreateLambda([this]() { MetasoundGraphEditor->OnStraightenConnections(); }));

				// Distribution Commands
				GraphEditorCommands->MapAction(FGraphEditorCommands::Get().DistributeNodesHorizontally,
					FExecuteAction::CreateLambda([this]() { MetasoundGraphEditor->OnDistributeNodesH(); }));

				GraphEditorCommands->MapAction(FGraphEditorCommands::Get().DistributeNodesVertically,
					FExecuteAction::CreateLambda([this]() { MetasoundGraphEditor->OnDistributeNodesV(); }));

				// Node Commands
				GraphEditorCommands->MapAction(FGraphEditorCommands::Get().CreateComment,
					FExecuteAction::CreateSP(this, &FEditor::OnCreateComment));

				GraphEditorCommands->MapAction(FEditorCommands::Get().UpdateNodeClass,
					FExecuteAction::CreateSP(this, &FEditor::UpdateSelectedNodeClasses));
			}

			SGraphEditor::FGraphEditorEvents GraphEvents;
			GraphEvents.OnCreateActionMenu = SGraphEditor::FOnCreateActionMenu::CreateSP(this, &FEditor::OnCreateGraphActionMenu);
			GraphEvents.OnNodeDoubleClicked = FSingleNodeEvent::CreateSP(this, &FEditor::ExecuteNode);
			GraphEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FEditor::OnSelectedNodesChanged);
			GraphEvents.OnTextCommitted = FOnNodeTextCommitted::CreateSP(this, &FEditor::OnNodeTitleCommitted);

			FMetasoundAssetBase* MetasoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(Metasound);
			check(MetasoundAsset);

			SAssignNew(MetasoundGraphEditor, SGraphEditor)
				.AdditionalCommands(GraphEditorCommands)
				.Appearance(this, &FEditor::GetGraphAppearance)
				.AutoExpandActionMenu(true)
				.GraphEvents(GraphEvents)
				.GraphToEdit(MetasoundAsset->GetGraph())
				.IsEditable(this, &FEditor::IsGraphEditable)
				.ShowGraphStateOverlay(false);

			SAssignNew(PlayTimeWidget, STextBlock)
				.Visibility(EVisibility::HitTestInvisible)
				.TextStyle(FAppStyle::Get(), "Graph.ZoomText")
				.ColorAndOpacity(FLinearColor(1, 1, 1, 0.30f));
		}

		FGraphAppearanceInfo FEditor::GetGraphAppearance() const
		{
			FGraphAppearanceInfo AppearanceInfo;

			if (Metasound)
			{
				const FMetasoundAssetBase* MetasoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(Metasound);
				check(MetasoundAsset);
				AppearanceInfo.CornerText = MetasoundAsset->GetDisplayName();
			}

			return AppearanceInfo;
		}

		void FEditor::OnSelectedNodesChanged(const TSet<UObject*>& InSelectedNodes)
		{
			TArray<UObject*> Selection;
			for (UObject* NodeObject : InSelectedNodes)
			{
				if (UMetasoundEditorGraphInputNode* InputNode = Cast<UMetasoundEditorGraphInputNode>(NodeObject))
				{
					Selection.Add(InputNode->Input);
				}
				else if (UMetasoundEditorGraphOutputNode* OutputNode = Cast<UMetasoundEditorGraphOutputNode>(NodeObject))
				{
					Selection.Add(OutputNode->Output);
				}
				else if (UMetasoundEditorGraphVariableNode* VariableNode = Cast<UMetasoundEditorGraphVariableNode>(NodeObject))
				{
					Selection.Add(VariableNode->Variable);
				}
				else
				{
					Selection.Add(NodeObject);
				}
			}

			if (GraphMembersMenu.IsValid() && !bManuallyClearingGraphSelection)
			{
				GraphMembersMenu->SelectItemByName(FName());
			}
			SetSelection(Selection);
		}

		void FEditor::OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged)
		{
			if (NodeBeingChanged)
			{
				const FScopedTransaction Transaction(TEXT(""), LOCTEXT("RenameNode", "Rename Node"), NodeBeingChanged);
				NodeBeingChanged->Modify();
				NodeBeingChanged->OnRenameNode(NewText.ToString());
			}
		}

		void FEditor::DeleteInterfaceItem(TSharedPtr<FMetasoundGraphMemberSchemaAction> ActionToDelete)
		{
			using namespace Metasound::Frontend;
			check(Metasound);

			UMetasoundEditorGraphMember* GraphMember = ActionToDelete->GetGraphMember();
			if (ensure(GraphMember))
			{
				const FGuid MemberID = GraphMember->GetMemberID();
				UMetasoundEditorGraph& Graph = GetMetaSoundGraphChecked();
				UMetasoundEditorGraphMember* NextToSelect = Graph.FindAdjacentMember(*GraphMember);

				{
					const FScopedTransaction Transaction(LOCTEXT("MetaSoundEditorDeleteSelectedMember", "Delete MetaSound Graph Member"));
					Metasound->Modify();
					Graph.Modify();
					GraphMember->Modify();
					Graph.RemoveMember(*GraphMember);
				}

				if (NextToSelect)
				{
					if (GraphMembersMenu->SelectItemByName(NextToSelect->GetMemberName(), ESelectInfo::Direct, static_cast<int32>(NextToSelect->GetSectionID())))
					{
						const TArray<UObject*> GraphMembersToSelect { NextToSelect };
						SetSelection(GraphMembersToSelect);
					}
				}
				else
				{
					FMetasoundAssetBase* MetasoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(Metasound);
					check(MetasoundAsset);
					MetasoundAsset->GetModifyContext().AddMemberIDsModified({ MemberID });
				}
			}

			FGraphBuilder::RegisterGraphWithFrontend(*Metasound);
		}

		void FEditor::DeleteSelected()
		{
			using namespace Frontend;

			if (!IsGraphEditable())
			{
				return;
			}

			if (CanDeleteNodes())
			{
				DeleteSelectedNodes();
			}
			DeleteSelectedInterfaceItems();
		}

		void FEditor::DeleteSelectedNodes()
		{
			using namespace Metasound::Frontend;

			const FGraphPanelSelectionSet SelectedNodes = MetasoundGraphEditor->GetSelectedNodes();
			MetasoundGraphEditor->ClearSelectionSet();

			FMetasoundAssetBase* MetasoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(Metasound);
			check(MetasoundAsset);

			const FScopedTransaction Transaction(LOCTEXT("MetaSoundEditorDeleteSelectedNode2", "Delete Selected MetaSound Node(s)"));
			check(Metasound);
			Metasound->Modify();
			UEdGraph* Graph = MetasoundGraphEditor->GetCurrentGraph();
			check(Graph);
			Graph->Modify();
			for (UObject* NodeObj : SelectedNodes)
			{
				// Some nodes may not be metasound nodes (ex. comments and perhaps aliases eventually), but can be safely deleted.
				if (UEdGraphNode* Node = Cast<UEdGraphNode>(NodeObj))
				{
					if (!FGraphBuilder::DeleteNode(*Node))
					{
						MetasoundGraphEditor->SetNodeSelection(Node, true /* bSelect */);
					}
				}
			}
		}

		void FEditor::DeleteSelectedInterfaceItems()
		{
			if (!IsGraphEditable() || !GraphMembersMenu.IsValid())
			{
				return;
			}

			TArray<TSharedPtr<FEdGraphSchemaAction>> Actions;
			GraphMembersMenu->GetSelectedActions(Actions);
			if (Actions.IsEmpty())
			{
				return;
			}

			TSharedPtr<FMetasoundGraphMemberSchemaAction> ActionToDelete;
			for (const TSharedPtr<FEdGraphSchemaAction>& Action : Actions)
			{
				TSharedPtr<FMetasoundGraphMemberSchemaAction> MetasoundAction = StaticCastSharedPtr<FMetasoundGraphMemberSchemaAction>(Action);
				if (MetasoundAction.IsValid())
				{
					const UMetasoundEditorGraphMember* GraphMember = MetasoundAction->GetGraphMember();
					if (ensure(nullptr != GraphMember))
					{
						const FMetasoundFrontendVersion* InterfaceVersion = nullptr;
						if (const UMetasoundEditorGraphVertex* Vertex = Cast<UMetasoundEditorGraphVertex>(GraphMember))
						{
							InterfaceVersion = &Vertex->GetInterfaceVersion();
						}

						if (InterfaceVersion && InterfaceVersion->IsValid())
						{
							if (MetasoundGraphEditor.IsValid())
							{
								const FText Notification = FText::Format(LOCTEXT("CannotDeleteInterfaceMemberNotificationFormat", "Cannot delete individual member of interface '{0}'."), FText::FromName(InterfaceVersion->Name));
								FNotificationInfo Info(Notification);
								Info.bFireAndForget = true;
								Info.bUseSuccessFailIcons = false;
								Info.ExpireDuration = 5.0f;

								MetasoundGraphEditor->AddNotification(Info, false /* bSuccess */);
							}
						}
						else
						{
							ActionToDelete = MetasoundAction;
							if (ActionToDelete.IsValid())
							{
								DeleteInterfaceItem(ActionToDelete);
							}
						}
					}
				}
			}
		}

		void FEditor::CutSelectedNodes()
		{
			CopySelectedNodes();

			// Cache off the old selection
			const FGraphPanelSelectionSet OldSelectedNodes = MetasoundGraphEditor->GetSelectedNodes();

			// Clear the selection and only select the nodes that can be duplicated
			FGraphPanelSelectionSet RemainingNodes;
			MetasoundGraphEditor->ClearSelectionSet();

			for (FGraphPanelSelectionSet::TConstIterator SelectedIter(OldSelectedNodes); SelectedIter; ++SelectedIter)
			{
				UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
				if (Node && Node->CanUserDeleteNode())
				{
					MetasoundGraphEditor->SetNodeSelection(Node, true);
				}
				else
				{
					RemainingNodes.Add(Node);
				}
			}

			// Delete the deletable nodes
			DeleteSelectedNodes();

			// Clear deleted, and reselect remaining nodes from original selection
			MetasoundGraphEditor->ClearSelectionSet();
			for (UObject* RemainingNode : RemainingNodes)
			{
				if (UEdGraphNode* Node = Cast<UEdGraphNode>(RemainingNode))
				{
					MetasoundGraphEditor->SetNodeSelection(Node, true);
				}
			}
		}

		void FEditor::CopySelectedNodes() const
		{
			FString NodeString;
			const FGraphPanelSelectionSet SelectedNodes = MetasoundGraphEditor->GetSelectedNodes();
			FEdGraphUtilities::ExportNodesToText(SelectedNodes, NodeString);
			FPlatformApplicationMisc::ClipboardCopy(*NodeString);
		}

		bool FEditor::CanCopyNodes() const
		{
			// If any of the nodes can be duplicated then allow copying
			const FGraphPanelSelectionSet& SelectedNodes = MetasoundGraphEditor->GetSelectedNodes();
			for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
			{
				UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
				if (Node && Node->CanDuplicateNode())
				{
					return true;
				}
			}
			return false;
		}

		bool FEditor::CanDuplicateNodes() const
		{
			if (!IsGraphEditable())
			{
				return false;
			}

			// If any of the nodes can be duplicated then allow copying
			const FGraphPanelSelectionSet& SelectedNodes = MetasoundGraphEditor->GetSelectedNodes();
			for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
			{
				UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
				if (!Node || !Node->CanDuplicateNode())
				{
					return false;
				}
			}

			FString NodeString;
			FEdGraphUtilities::ExportNodesToText(SelectedNodes, NodeString);

			FMetasoundAssetBase* MetasoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(Metasound);
			check(MetasoundAsset);

			UEdGraph* Graph = MetasoundAsset->GetGraph();
			if (!Graph)
			{
				return false;
			}

			return FEdGraphUtilities::CanImportNodesFromText(Graph, NodeString);
		}

		bool FEditor::CanDeleteNodes() const
		{
			if (!IsGraphEditable())
			{
				return false;
			}

			if (MetasoundGraphEditor->GetSelectedNodes().IsEmpty())
			{
				return false;
			}

			const FGraphPanelSelectionSet& SelectedNodes = MetasoundGraphEditor->GetSelectedNodes();
			for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
			{
				UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
				if (Node && Node->CanUserDeleteNode())
				{
					return true;
				}
			}
			return false;
		}

		bool FEditor::CanDeleteInterfaceItems() const
		{
			if (!IsGraphEditable())
			{
				return false;
			}

			if (!GraphMembersMenu.IsValid())
			{
				return false;
			}

			TArray<TSharedPtr<FEdGraphSchemaAction>> Actions;
			GraphMembersMenu->GetSelectedActions(Actions);

			if (Actions.IsEmpty())
			{
				return false;
			}

			TSharedPtr<FMetasoundGraphMemberSchemaAction> ActionToDelete;
			for (const TSharedPtr<FEdGraphSchemaAction>& Action : Actions)
			{
				TSharedPtr<FMetasoundGraphMemberSchemaAction> MetasoundAction = StaticCastSharedPtr<FMetasoundGraphMemberSchemaAction>(Action);
				if (MetasoundAction.IsValid())
				{
					const UMetasoundEditorGraphMember* GraphMember = MetasoundAction->GetGraphMember();
					if (ensure(nullptr != GraphMember))
					{
						const FMetasoundFrontendVersion* InterfaceVersion = nullptr;
						if (const UMetasoundEditorGraphVertex* Vertex = Cast<UMetasoundEditorGraphVertex>(GraphMember))
						{
							InterfaceVersion = &Vertex->GetInterfaceVersion();
						}

						// Interface members cannot be deleted
						const bool bIsInterfaceMember = InterfaceVersion && InterfaceVersion->IsValid();
						if (!bIsInterfaceMember)
						{
							return true;
						}
					}
				}
			}
			return false;
		}

		void FEditor::DuplicateNodes()
		{
			const FGraphPanelSelectionSet SelectedNodes = MetasoundGraphEditor->GetSelectedNodes();
			FEdGraphUtilities::ExportNodesToText(SelectedNodes, NodeTextToPaste);
			PasteNodes(nullptr, LOCTEXT("MetaSoundEditorDuplicate", "Duplicate MetaSound Node(s)"));
		}

		void FEditor::PasteNodes(const FVector2D* InLocation)
		{
			PasteNodes(InLocation, LOCTEXT("MetaSoundEditorPaste", "Paste MetaSound Node(s)"));
		}

		void FEditor::PasteNodes(const FVector2D* InLocation, const FText& InTransactionText)
		{
			using namespace Frontend;

			FVector2D Location;
			if (InLocation)
			{
				Location = *InLocation;
			}
			else
			{
				check(MetasoundGraphEditor);
				Location = MetasoundGraphEditor->GetPasteLocation();
			}

			FMetasoundAssetBase* MetasoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(Metasound);
			check(MetasoundAsset);

			UMetasoundEditorGraph& Graph = GetMetaSoundGraphChecked();

			const FScopedTransaction Transaction(InTransactionText);
			Metasound->Modify();
			Graph.Modify();

			// Clear the selection set (newly pasted stuff will be selected)
			MetasoundGraphEditor->ClearSelectionSet();

			TSet<UEdGraphNode*> PastedGraphNodes;
			FEdGraphUtilities::ImportNodesFromText(&Graph, NodeTextToPaste, PastedGraphNodes);

			NodeTextToPaste.Empty();

			bool bNotifyReferenceLoop = false;
			bool bNotifyMultipleVariableSetters = false;

			TArray<UEdGraphNode*> NodesToRemove;
			for (UEdGraphNode* GraphNode : PastedGraphNodes)
			{
				GraphNode->CreateNewGuid();
				if (UMetasoundEditorGraphExternalNode* ExternalNode = Cast<UMetasoundEditorGraphExternalNode>(GraphNode))
				{
					FMetasoundFrontendClassMetadata LookupMetadata;
					LookupMetadata.SetClassName(ExternalNode->GetClassName());
					LookupMetadata.SetType(EMetasoundFrontendClassType::External);
					const FNodeRegistryKey PastedRegistryKey = NodeRegistryKey::CreateKey(LookupMetadata);
					if (const FSoftObjectPath* AssetPath = IMetaSoundAssetManager::GetChecked().FindObjectPathFromKey(PastedRegistryKey))
					{
						if (MetasoundAsset->AddingReferenceCausesLoop(*AssetPath))
						{
							FMetasoundFrontendClass MetaSoundClass;
							FMetasoundFrontendRegistryContainer::Get()->FindFrontendClassFromRegistered(PastedRegistryKey, MetaSoundClass);
							FString FriendlyClassName = MetaSoundClass.Metadata.GetDisplayName().ToString();
							if (FriendlyClassName.IsEmpty())
							{
								FriendlyClassName = MetaSoundClass.Metadata.GetClassName().ToString();
							}
							UE_LOG(LogMetaSound, Warning, TEXT("Failed to paste node with class '%s'.  Class would introduce cyclic asset dependency."), *FriendlyClassName);
							bNotifyReferenceLoop = true;
							NodesToRemove.Add(GraphNode);
						}
						else
						{
							FNodeHandle NewHandle = FGraphBuilder::AddNodeHandle(*Metasound, *ExternalNode);
							if (!NewHandle->IsValid())
							{
								NodesToRemove.Add(GraphNode);
							}
						}
					}
					else
					{
						FMetasoundFrontendClass FrontendClass;
						const FMetasoundFrontendClassName ClassName = ExternalNode->GetClassName();
						if (ClassName == FRerouteNodeTemplate::ClassName)
						{
							bool bIsValid = false;
							if (!ExternalNode->Pins.IsEmpty())
							{
								const FName DataType = FGraphBuilder::GetPinDataType(ExternalNode->Pins.Last());
								if (!DataType.IsNone())
								{
									const FNodeRegistryKey& RerouteTemplateKey = FRerouteNodeTemplate::GetRegistryKey();
									FMetasoundFrontendNodeInterface NodeInterface = FRerouteNodeTemplate::CreateNodeInterfaceFromDataType(DataType);

									FNodeHandle NodeHandle = MetasoundAsset->GetRootGraphHandle()->AddTemplateNode(RerouteTemplateKey, MoveTemp(NodeInterface));
									FGraphBuilder::InitGraphNode(NodeHandle, ExternalNode, *Metasound);
									bIsValid = NodeHandle->IsValid();
								}
							}

							if (!bIsValid)
							{
								NodesToRemove.Add(GraphNode);
							}
						}
						else
						{
							FNodeHandle NewHandle = FGraphBuilder::AddNodeHandle(*Metasound, *ExternalNode);
							if (!NewHandle->IsValid())
							{
								NodesToRemove.Add(GraphNode);
							}
						}
					}
				}
				else if (UMetasoundEditorGraphInputNode* InputNode = Cast<UMetasoundEditorGraphInputNode>(GraphNode))
				{
					if (!InputNode->Input || !Graph.ContainsInput(*InputNode->Input))
					{
						NodesToRemove.Add(GraphNode);
					}
				}
				else if (UMetasoundEditorGraphOutputNode* OutputNode = Cast<UMetasoundEditorGraphOutputNode>(GraphNode))
				{
					if (OutputNode->Output && Graph.ContainsOutput(*OutputNode->Output))
					{
						auto NodeMatches = [OutputNodeID = OutputNode->GetNodeID()](const TObjectPtr<UEdGraphNode>& EdNode)
						{
							if (UMetasoundEditorGraphOutputNode* OutputNode = Cast<UMetasoundEditorGraphOutputNode>(EdNode))
							{
								return OutputNodeID == OutputNode->GetNodeID();
							}
							return false;
						};

						// Can only have one output reference node
						if (Graph.Nodes.ContainsByPredicate(NodeMatches))
						{
							NodesToRemove.Add(GraphNode);
						}
					}
					else
					{
						NodesToRemove.Add(GraphNode);
					}
				}
				else if (UMetasoundEditorGraphVariableNode* VariableNode = Cast<UMetasoundEditorGraphVariableNode>(GraphNode))
				{
					// Can only have one setter node
					if (const UMetasoundEditorGraphVariable* Variable = VariableNode->Variable)
					{
						if (Graph.ContainsVariable(*Variable))
						{
							FConstVariableHandle VariableHandle = Variable->GetConstVariableHandle();
							if (VariableHandle->IsValid())
							{
								FConstNodeHandle VariableMutatorNodeHandle = VariableHandle->FindMutatorNode();
								if (VariableNode->GetNodeID() == VariableMutatorNodeHandle->GetID())
								{
									bNotifyMultipleVariableSetters = true;
									NodesToRemove.Add(GraphNode);
								}
								else
								{
									// Add new variable node
									const FNodeClassName NodeClassName = VariableNode->GetClassName().ToNodeClassName();
									FNodeHandle NodeHandle = FGraphBuilder::AddVariableNodeHandle(*Metasound, Variable->GetVariableID(), NodeClassName, VariableNode);
									if (!NodeHandle->IsValid())
									{
										NodesToRemove.Add(GraphNode);
									}
								}
							}
						}
						else
						{
							NodesToRemove.Add(GraphNode);
						}
					}
					else
					{
						NodesToRemove.Add(GraphNode);
					}
				}
				else if (!GraphNode->IsA<UEdGraphNode_Comment>())
				{
					checkNoEntry();
				}
			}

			// Remove nodes failed to import before attempting to connect/place
			// in frontend graph.
			for (UEdGraphNode* Node : NodesToRemove)
			{
				Graph.RemoveNode(Node);
				PastedGraphNodes.Remove(Node);
			}

			// Find average midpoint of nodes and offset subgraph accordingly
			FVector2D AvgNodePosition = FVector2D::ZeroVector;
			for (UEdGraphNode* Node : PastedGraphNodes)
			{
				AvgNodePosition.X += Node->NodePosX;
				AvgNodePosition.Y += Node->NodePosY;
			}

			if (!PastedGraphNodes.IsEmpty())
			{
				float InvNumNodes = 1.0f / PastedGraphNodes.Num();
				AvgNodePosition.X *= InvNumNodes;
				AvgNodePosition.Y *= InvNumNodes;
			}

			for (UEdGraphNode* GraphNode : PastedGraphNodes)
			{
				GraphNode->NodePosX = (GraphNode->NodePosX - AvgNodePosition.X) + Location.X;
				GraphNode->NodePosY = (GraphNode->NodePosY - AvgNodePosition.Y) + Location.Y;

				GraphNode->SnapToGrid(SNodePanel::GetSnapGridSize());
				if (UMetasoundEditorGraphNode* MetasoundGraphNode = Cast<UMetasoundEditorGraphNode>(GraphNode))
				{
					FNodeHandle NodeHandle = MetasoundGraphNode->GetNodeHandle();
					if (ensure(NodeHandle->IsValid()))
					{
						const FVector2D NewNodeLocation = FVector2D(GraphNode->NodePosX, GraphNode->NodePosY);
						FMetasoundFrontendNodeStyle NodeStyle = NodeHandle->GetNodeStyle();
						NodeStyle.Display.Locations.FindOrAdd(MetasoundGraphNode->NodeGuid) = NewNodeLocation;
						NodeHandle->SetNodeStyle(NodeStyle);
					}
				}
			}

			for (UEdGraphNode* GraphNode : PastedGraphNodes)
			{
				UMetasoundEditorGraphNode* MetasoundNode = Cast<UMetasoundEditorGraphNode>(GraphNode);
				if (!MetasoundNode)
				{
					continue;
				}

				for (UEdGraphPin* Pin : GraphNode->Pins)
				{
					if (Pin->Direction != EGPD_Input)
					{
						continue;
					}

					FInputHandle InputHandle = FGraphBuilder::GetInputHandleFromPin(Pin);
					if (InputHandle->IsValid() && InputHandle->GetDataType() != GetMetasoundDataTypeName<FTrigger>())
					{
						FMetasoundFrontendLiteral LiteralValue;
						if (FGraphBuilder::GetPinLiteral(*Pin, LiteralValue))
						{
							if (const FMetasoundFrontendLiteral* ClassDefault = InputHandle->GetClassDefaultLiteral())
							{
								// Check equivalence with class default and don't set if they are equal. Copied node
								// pin has no information to indicate whether or not the literal was already set.
								if (!LiteralValue.IsEqual(*ClassDefault))
								{
									InputHandle->SetLiteral(LiteralValue);
								}
							}
							else
							{
								InputHandle->SetLiteral(LiteralValue);
							}
						}
					}

					for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
					{
						if (UMetasoundEditorGraphNode* Node = Cast<UMetasoundEditorGraphNode>(LinkedPin->GetOwningNode()))
						{
							FGraphBuilder::ConnectNodes(*Pin, *LinkedPin, false /* bConnectEdPins */);
						}
					}
				}
			}

			// Select the newly pasted stuff
			for (UEdGraphNode* GraphNode : PastedGraphNodes)
			{
				MetasoundGraphEditor->SetNodeSelection(GraphNode, true);
			}


			if (bNotifyReferenceLoop)
			{
				NotifyNodePasteFailure_ReferenceLoop();
			}

			if (bNotifyMultipleVariableSetters)
			{
				NotifyNodePasteFailure_MultipleVariableSetters();
			}

			MetasoundGraphEditor->NotifyGraphChanged();
		}

		bool FEditor::CanRenameSelectedNodes() const
		{
			if (!IsGraphEditable())
			{
				return false;
			}

			const FGraphPanelSelectionSet& SelectedNodes = MetasoundGraphEditor->GetSelectedNodes();
			for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
			{
				// Node is directly renameable (comment nodes)
				const UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
				if (Node && Node->GetCanRenameNode())
				{
					return true;
				}

				// Renameable member nodes
				if (const UMetasoundEditorGraphMemberNode* MemberNode = Cast<UMetasoundEditorGraphMemberNode>(*SelectedIter))
				{
					if (const UMetasoundEditorGraphMember* Member = MemberNode->GetMember()) 
					{
						return Member->CanRename();
					}
				}
			}
			return false;
		}

		bool FEditor::CanRenameSelectedInterfaceItems() const
		{
			if (GraphMembersMenu.IsValid())
			{
				TArray<TSharedPtr<FEdGraphSchemaAction>> Actions;
				GraphMembersMenu->GetSelectedActions(Actions);

				if (!Actions.IsEmpty())
				{
					for (const TSharedPtr<FEdGraphSchemaAction>& Action : Actions)
					{
						TSharedPtr<FMetasoundGraphMemberSchemaAction> MetasoundAction = StaticCastSharedPtr<FMetasoundGraphMemberSchemaAction>(Action);
						if (MetasoundAction.IsValid())
						{
							if (const UMetasoundEditorGraphMember* GraphMember = MetasoundAction->GetGraphMember())
							{
								if (GraphMember->CanRename())
								{
									return true;
								}
							}
						}
					}
				}
			}
			return false;
		}

		FGraphConnectionManager& FEditor::GetConnectionManager()
		{
			return *GraphConnectionManager.Get();
		}

		const FGraphConnectionManager& FEditor::GetConnectionManager() const
		{
			return *GraphConnectionManager.Get();
		}

		UAudioComponent* FEditor::GetAudioComponent() const
		{
			// TODO: Instance for each editor
			if (IsPlaying())
			{
				return GEditor->GetPreviewAudioComponent();
			}

			return nullptr;
		}

		void FEditor::RenameSelectedNode()
		{
			const FGraphPanelSelectionSet& SelectedNodes = MetasoundGraphEditor->GetSelectedNodes();
			for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
			{
				// Node is directly renameable (comment nodes)
				UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
				if (Node && Node->GetCanRenameNode())
				{
					if (TSharedPtr<SGraphEditor> GraphEditor = GetGraphEditor())
					{
						GraphEditor->JumpToNode(Node, /*bRequestRename=*/true);
						return;
					}
				}

				// Renameable member nodes (inputs/outputs/variables)
				if (UMetasoundEditorGraphMemberNode* MemberNode = Cast<UMetasoundEditorGraphMemberNode>(*SelectedIter))
				{
					if (const UMetasoundEditorGraphMember* Member = MemberNode->GetMember()) 
					{
						GraphMembersMenu->SelectItemByName(Member->GetMemberName(), ESelectInfo::Direct, static_cast<int32>(Member->GetSectionID()));

						if (Member->OnRenameRequested.IsBound())
						{
							Member->OnRenameRequested.Broadcast();
						}
					}
				}
			}
		}

		void FEditor::RenameSelectedInterfaceItem()
		{
			if (GraphMembersMenu.IsValid())
			{
				TArray<TSharedPtr<FEdGraphSchemaAction>> Actions;
				GraphMembersMenu->GetSelectedActions(Actions);

				if (!Actions.IsEmpty())
				{
					for (const TSharedPtr<FEdGraphSchemaAction>& Action : Actions)
					{
						TSharedPtr<FMetasoundGraphMemberSchemaAction> MetasoundAction = StaticCastSharedPtr<FMetasoundGraphMemberSchemaAction>(Action);
						if (MetasoundAction.IsValid())
						{
							if (const UMetasoundEditorGraphMember* GraphMember = MetasoundAction->GetGraphMember())
							{
								if (GraphMember->CanRename())
								{
									if (GraphMember->OnRenameRequested.IsBound())
									{
										GraphMember->OnRenameRequested.Broadcast();
										return;
									}
								}
							}
						}
					}
				}
			}
		}

		void FEditor::RefreshDetails()
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::Editor::FEditor::RefreshDetails);

			using namespace Frontend;

			if (MetasoundDetails.IsValid())
			{
				MetasoundDetails->ForceRefresh();
			}
		}

		void FEditor::RefreshInterfaces()
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::Editor::FEditor::RefreshInterfaces);

			if (InterfacesDetails.IsValid())
			{
				InterfacesDetails->ForceRefresh();
			}
		}

		UMetasoundEditorGraphMember* FEditor::RefreshGraphMemberMenu()
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::Editor::FEditor::RefreshGraphMemberMenu);

			if (GraphMembersMenu.IsValid())
			{
				TArray<TSharedPtr<FEdGraphSchemaAction>> SelectedActions;
				GraphMembersMenu->GetSelectedActions(SelectedActions);

				GraphMembersMenu->RefreshAllActions(true /* bPreserveExpansion */, false /*bHandleOnSelectionEvent*/);

				for (const TSharedPtr<FEdGraphSchemaAction>& Action : SelectedActions)
				{
					TSharedPtr<FMetasoundGraphMemberSchemaAction> MetasoundAction = StaticCastSharedPtr<FMetasoundGraphMemberSchemaAction>(Action);
					if (MetasoundAction.IsValid())
					{
						if (UMetasoundEditorGraphMember* Member = MetasoundAction->GetGraphMember())
						{
							const FName ActionName = Member->GetMemberName();
							GraphMembersMenu->SelectItemByName(ActionName, ESelectInfo::Direct, Action->GetSectionID());
							return Member;
						}
					}
				}
			}
			return nullptr;
		}

		void FEditor::UpdateSelectedNodeClasses()
		{
			using namespace Metasound::Frontend;

			const FScopedTransaction Transaction(LOCTEXT("NodeVersionUpdate", "Update MetaSound Node(s) Class(es)"));
			check(Metasound);
			Metasound->Modify();

			UMetasoundEditorGraph& Graph = GetMetaSoundGraphChecked();
			Graph.Modify();

			bool bReplacedNodes = false;
			const FGraphPanelSelectionSet Selection = MetasoundGraphEditor->GetSelectedNodes();
			for (UObject* Object : Selection)
			{
				if (UMetasoundEditorGraphExternalNode* ExternalNode = Cast<UMetasoundEditorGraphExternalNode>(Object))
				{
					Metasound::Frontend::FNodeHandle NodeHandle = ExternalNode->GetNodeHandle();
					const FMetasoundFrontendClassMetadata& Metadata = NodeHandle->GetClassMetadata();

					// Check for new version
					FMetasoundFrontendVersionNumber HighestVersion = ExternalNode->FindHighestVersionInRegistry();
					const bool bHasNewVersion = HighestVersion.IsValid() && HighestVersion > Metadata.GetVersion();

					// Check for non-native classes
					const FNodeRegistryKey RegistryKey = NodeRegistryKey::CreateKey(Metadata);
					const bool bIsClassNative = FMetasoundFrontendRegistryContainer::Get()->IsNodeNative(RegistryKey);

					if (bHasNewVersion || !bIsClassNative)
					{
						// These are ignored here when updating as the user is actively
						// forcing an update.
						constexpr TArray<INodeController::FVertexNameAndType>* DisconnectedInputs = nullptr;
						constexpr TArray<INodeController::FVertexNameAndType>* DisconnectedOutputs = nullptr;

						FNodeHandle NewNode = NodeHandle->ReplaceWithVersion(HighestVersion, DisconnectedInputs, DisconnectedOutputs);
						bReplacedNodes = true;
					}
				}
			}

			if (bReplacedNodes)
			{
				FDocumentHandle DocumentHandle = Graph.GetDocumentHandle();
				DocumentHandle->RemoveUnreferencedDependencies();
				DocumentHandle->SynchronizeDependencyMetadata();
				Graph.GetModifyContext().SetDocumentModified();
			}
		}

		bool FEditor::CanPasteNodes()
		{
			if (!IsGraphEditable())
			{
				return false;
			}

			UMetasoundEditorGraph& Graph = GetMetaSoundGraphChecked();
			FPlatformApplicationMisc::ClipboardPaste(NodeTextToPaste);
			if (FEdGraphUtilities::CanImportNodesFromText(&Graph, NodeTextToPaste))
			{
				return true;
			}

			NodeTextToPaste.Empty();
			return false;
		}

		void FEditor::UndoGraphAction()
		{
			check(GEditor);
			GEditor->UndoTransaction();
		}

		void FEditor::RedoGraphAction()
		{
			// Clear selection, to avoid holding refs to nodes that go away
			MetasoundGraphEditor->ClearSelectionSet();

			check(GEditor);
			GEditor->RedoTransaction();
		}

		void FEditor::CollectAllActions(FGraphActionListBuilderBase& OutAllActions)
		{
			const FMetasoundAssetBase* MetasoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(Metasound);
			check(MetasoundAsset);

			UMetasoundEditorGraph& EdGraph = GetMetaSoundGraphChecked();
			Frontend::FConstGraphHandle FrontendGraph = MetasoundAsset->GetRootGraphHandle();

			auto GetMemberCategory = [](FName InFullCategoryName)
			{
				FName InterfaceName;
				FName MemberName;
				Audio::FParameterPath::SplitName(InFullCategoryName, InterfaceName, MemberName);

				if (InterfaceName.IsNone())
				{
					return FText::GetEmpty();
				}

				FString CategoryString = InterfaceName.ToString();
				CategoryString.ReplaceInline(*Audio::FParameterPath::NamespaceDelimiter, TEXT("|"));
				return FText::FromString(CategoryString);
			};

			FrontendGraph->IterateConstNodes([this, &GetMemberCategory, &EdGraph, ActionList = &OutAllActions](const Frontend::FConstNodeHandle& Input)
			{
				constexpr bool bIncludeNamespace = false;
				const FText Tooltip = Input->GetDescription();
				const FText MenuDesc = FGraphBuilder::GetDisplayName(*Input, bIncludeNamespace);
				const FGuid NodeID = Input->GetID();
				const FText Category = GetMemberCategory(Input->GetNodeName());

				TSharedPtr<FMetasoundGraphMemberSchemaAction> NewFuncAction = MakeShared<FMetasoundGraphMemberSchemaAction>(Category, MenuDesc, Tooltip, 1, ENodeSection::Inputs);
				NewFuncAction->Graph = &EdGraph;
				NewFuncAction->MemberID = NodeID;

				ActionList->AddAction(NewFuncAction);
			}, EMetasoundFrontendClassType::Input);

			FrontendGraph->IterateConstNodes([this, &GetMemberCategory, &EdGraph, ActionList = &OutAllActions](const Frontend::FConstNodeHandle& Output)
			{
				constexpr bool bIncludeNamespace = false;

				const FText Tooltip = Output->GetDescription();
				const FText MenuDesc = FGraphBuilder::GetDisplayName(*Output, bIncludeNamespace);
				const FGuid NodeID = Output->GetID();
				const FText Category = GetMemberCategory(Output->GetNodeName());

				TSharedPtr<FMetasoundGraphMemberSchemaAction> NewFuncAction = MakeShared<FMetasoundGraphMemberSchemaAction>(Category, MenuDesc, Tooltip, 1, ENodeSection::Outputs);
				NewFuncAction->Graph = &EdGraph;
				NewFuncAction->MemberID = Output->GetID();
				ActionList->AddAction(NewFuncAction);
			}, EMetasoundFrontendClassType::Output);

			TArray<Frontend::FConstVariableHandle> Variables = FrontendGraph->GetVariables();
			for (const Frontend::FConstVariableHandle& Variable : Variables)
			{
				const FText MenuDesc = FGraphBuilder::GetDisplayName(*Variable);
				const FGuid VariableID = Variable->GetID();
				const FText Category = GetMemberCategory(Variable->GetName());

				TSharedPtr<FMetasoundGraphMemberSchemaAction> NewFuncAction = MakeShared<FMetasoundGraphMemberSchemaAction>(Category, MenuDesc, FText::GetEmpty(), 1, ENodeSection::Variables);
				NewFuncAction->Graph = &EdGraph;
				NewFuncAction->MemberID = VariableID; 
				OutAllActions.AddAction(NewFuncAction);
			}

		}

		void FEditor::CollectStaticSections(TArray<int32>& StaticSectionIDs)
		{
			Frontend::FConstDocumentHandle DocumentHandle = GetMetaSoundGraphChecked().GetDocumentHandle();
			const FMetasoundFrontendGraphClass& RootGraphClass = DocumentHandle->GetRootGraphClass();
			const bool bIsPreset = RootGraphClass.PresetOptions.bIsPreset;

			for (int32 i = 0; i < static_cast<int32>(ENodeSection::COUNT); ++i)
			{
				if (static_cast<ENodeSection>(i) != ENodeSection::None)
				{
					// Presets do not have variables
					if (bIsPreset && static_cast<ENodeSection>(i) == ENodeSection::Variables)
					{
						continue;
					}
					StaticSectionIDs.Add(i);
				}
			}
		}

		bool FEditor::HandleActionMatchesName(FEdGraphSchemaAction* InAction, const FName& InName) const
		{
			if (FMetasoundGraphMemberSchemaAction* Action = static_cast<FMetasoundGraphMemberSchemaAction*>(InAction))
			{
				return InName == Action->GetMemberName();
			}

			return false;
		}

		FReply FEditor::OnActionDragged(const TArray<TSharedPtr<FEdGraphSchemaAction>>& InActions, const FPointerEvent& MouseEvent)
		{
			if (!MetasoundGraphEditor.IsValid() || InActions.IsEmpty())
			{
				return FReply::Unhandled();
			}

			TSharedPtr<FEdGraphSchemaAction> DragAction = InActions.Last();
			if (FMetasoundGraphMemberSchemaAction* MemberAction = static_cast<FMetasoundGraphMemberSchemaAction*>(DragAction.Get()))
			{
				if (UEdGraph* ActionGraph = MemberAction->Graph)
				{
					if (&GetMetaSoundGraphChecked() == ActionGraph)
					{
						TSharedPtr<FEditor> ThisEditor = StaticCastSharedRef<FEditor>(AsShared());
						return FReply::Handled().BeginDragDrop(MakeShared<FMetaSoundDragDropMemberAction>(ThisEditor, MemberAction->GetGraphMember()));
					}
				}
			}

			return FReply::Unhandled();
		}

		void FEditor::OnMemberActionDoubleClicked(const TArray<TSharedPtr<FEdGraphSchemaAction>>& InActions)
		{
			if (!MetasoundGraphEditor.IsValid() || InActions.IsEmpty())
			{
				return;
			}

			TSharedPtr<FMetasoundGraphMemberSchemaAction> MemberAction = StaticCastSharedPtr<FMetasoundGraphMemberSchemaAction>(InActions.Last());
			if (UMetasoundEditorGraphMember* Member = MemberAction->GetGraphMember())
			{
				JumpToNodes(Member->GetNodes());
			}
		}

		bool FEditor::CanJumpToNodesForSelectedInterfaceItem() const
		{
			if (!GraphMembersMenu.IsValid())
			{
				return false;
			}
			TArray<TSharedPtr<FEdGraphSchemaAction>> Actions;
			GraphMembersMenu->GetSelectedActions(Actions);

			if (!Actions.IsEmpty())
			{
				for (const TSharedPtr<FEdGraphSchemaAction>& Action : Actions)
				{
					TSharedPtr<FMetasoundGraphMemberSchemaAction> MetasoundAction = StaticCastSharedPtr<FMetasoundGraphMemberSchemaAction>(Action);
					if (MetasoundAction.IsValid())
					{
						if (const UMetasoundEditorGraphMember* GraphMember = MetasoundAction->GetGraphMember())
						{
							TArray<UMetasoundEditorGraphMemberNode*> Nodes = GraphMember->GetNodes();
							if (!Nodes.IsEmpty())
							{
								return true;
							}
						}
					}
				}
			}
			return false;
		}

		void FEditor::JumpToNodesForSelectedInterfaceItem()
		{
			if (GraphMembersMenu.IsValid())
			{
				TArray<TSharedPtr<FEdGraphSchemaAction>> Actions;
				GraphMembersMenu->GetSelectedActions(Actions);

				if (!Actions.IsEmpty())
				{
					for (const TSharedPtr<FEdGraphSchemaAction>& Action : Actions)
					{
						TSharedPtr<FMetasoundGraphMemberSchemaAction> MetasoundAction = StaticCastSharedPtr<FMetasoundGraphMemberSchemaAction>(Action);
						if (MetasoundAction.IsValid())
						{
							if (const UMetasoundEditorGraphMember* GraphMember = MetasoundAction->GetGraphMember())
							{
								JumpToNodes(GraphMember->GetNodes());
								return;
							}
						}
					}
				}
			}
		}

		FActionMenuContent FEditor::OnCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed)
		{
			TSharedRef<SMetasoundActionMenu> ActionMenu = SNew(SMetasoundActionMenu)
				.AutoExpandActionMenu(bAutoExpand)
				.Graph(&GetMetaSoundGraphChecked())
				.NewNodePosition(InNodePosition)
				.DraggedFromPins(InDraggedPins)
				.OnClosedCallback(InOnMenuClosed);
// 				.OnCloseReason(this, &FEditor::OnGraphActionMenuClosed);

			TSharedPtr<SWidget> FilterTextBox = StaticCastSharedRef<SWidget>(ActionMenu->GetFilterTextBox());
			return FActionMenuContent(StaticCastSharedRef<SWidget>(ActionMenu), FilterTextBox);
		}

		void FEditor::OnActionSelected(const TArray<TSharedPtr<FEdGraphSchemaAction>>& InActions, ESelectInfo::Type InSelectionType)
		{
			if (InSelectionType == ESelectInfo::OnMouseClick || InSelectionType == ESelectInfo::OnKeyPress || InSelectionType == ESelectInfo::OnNavigation || InActions.IsEmpty())
			{
				TArray<UObject*> SelectedObjects;
				for (const TSharedPtr<FEdGraphSchemaAction>& Action : InActions)
				{
					TSharedPtr<FMetasoundGraphMemberSchemaAction> MetasoundMemberAction = StaticCastSharedPtr<FMetasoundGraphMemberSchemaAction>(Action);
					if (MetasoundMemberAction.IsValid())
					{
						SelectedObjects.Add(MetasoundMemberAction->GetGraphMember());
					}
				}

				if (InSelectionType != ESelectInfo::Direct && !InActions.IsEmpty())
				{
					if (MetasoundGraphEditor.IsValid())
					{
						bManuallyClearingGraphSelection = true;
						MetasoundGraphEditor->ClearSelectionSet();
						bManuallyClearingGraphSelection = false;
					}
					SetSelection(SelectedObjects);
				}
			}
		}

		// TODO: Add ability to filter inputs/outputs in "MetaSound" Tab
		FText FEditor::GetFilterText() const
		{
			return FText::GetEmpty();
		}

		TSharedRef<SWidget> FEditor::OnCreateWidgetForAction(FCreateWidgetForActionData* const InCreateData)
		{
			return SNew(SMetaSoundGraphPaletteItem, InCreateData);
		}

		TSharedPtr<SWidget> FEditor::OnContextMenuOpening()
		{
			if (!GraphMembersMenu.IsValid())
			{
				return nullptr;
			}

			// Context menu should only open when graph members are selected
			TArray<TSharedPtr<FEdGraphSchemaAction>> Actions;
			GraphMembersMenu->GetSelectedActions(Actions);
			if (Actions.IsEmpty())
			{
				return nullptr;
			}

			FMenuBuilder MenuBuilder(true, ToolkitCommands);

			MenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete);
			MenuBuilder.AddMenuEntry(FGenericCommands::Get().Rename);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("JumpToNodesMenuEntry", "Jump to Node(s) in Graph"),
				LOCTEXT("JumpToNodesMenuEntryTooltip", "Jump to the corresponding node(s) in the MetaSound graph"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &FEditor::JumpToNodesForSelectedInterfaceItem), 
					FCanExecuteAction::CreateSP(this, &FEditor::CanJumpToNodesForSelectedInterfaceItem)));

			return MenuBuilder.MakeWidget();
		}

		void FEditor::RemoveInvalidSelection()
		{
			if (MetasoundDetails.IsValid())
			{
				const TArray<TWeakObjectPtr<UObject>>& Objects = MetasoundDetails->GetSelectedObjects();
				TArray<UObject*> NewSelection;

				TSet<const UMetasoundEditorGraphMember*> GraphMembers;
				GetMetaSoundGraphChecked().IterateMembers([&GraphMembers](UMetasoundEditorGraphMember& GraphMember) { GraphMembers.Add(&GraphMember); });

				for (const TWeakObjectPtr<UObject>& Object : Objects)
				{
					if (Object.IsValid())
					{
						if (const UMetasoundEditorGraphMember* Member = Cast<UMetasoundEditorGraphMember>(Object.Get()))
						{
							if (GraphMembers.Contains(Member))
							{
								NewSelection.Add(Object.Get());
							}
						}
						else
						{
							NewSelection.Add(Object.Get());
						}
					}
				}

				if (NewSelection.Num() != Objects.Num())
				{
					SetSelection(NewSelection);
				}
			}
		}

		void FEditor::Tick(float DeltaTime)
		{
			if (!Metasound)
			{
				return;
			}

			if (bPrimingRegistry)
			{
				IMetasoundEditorModule& MetaSoundEditorModule = FModuleManager::GetModuleChecked<IMetasoundEditorModule>("MetaSoundEditor");
				if (MetaSoundEditorModule.GetAssetRegistryPrimeStatus() == EAssetPrimeStatus::Complete)
				{
					bPrimingRegistry = false;
					NotifyAssetPrimeComplete();
				}
			}

			RefreshEditorContext();

			GraphConnectionManager->Update(DeltaTime);
		}

		void FEditor::RefreshEditorContext()
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::Editor::FEditor::RefreshEditorContext);
			const bool bSynchronizedGraph = FGraphBuilder::SynchronizeGraph(*Metasound);

			FMetasoundAssetBase* MetasoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(Metasound);
			check(MetasoundAsset);

			// Capture after synchronizing as the modification state may be modified therein
			const FMetasoundFrontendDocumentModifyContext& ModifyContext = MetasoundAsset->GetModifyContext();
			const bool bForceRefreshViews = ModifyContext.GetForceRefreshViews();
			const TSet<FName>& InterfacesModified = ModifyContext.GetInterfacesModified();
			const TSet<FGuid>& MembersModified = ModifyContext.GetMemberIDsModified();
			const TSet<FGuid>& NodesModified = ModifyContext.GetNodeIDsModified();
			if (bSynchronizedGraph || bForceRefreshViews || !InterfacesModified.IsEmpty() || !NodesModified.IsEmpty() || !MembersModified.IsEmpty())
			{
				FGraphValidationResults Results = FGraphBuilder::ValidateGraph(*Metasound);

				for (const FGraphNodeValidationResult& Result : Results.GetResults())
				{
					UMetasoundEditorGraphNode& Node = Result.GetNodeChecked();
					const bool bClassChanged = Node.ContainsClassChange();
					const FText Title = Node.GetCachedTitle();
					Node.CacheTitle();
					const bool bTitleUpdated = !Title.IdenticalTo(Node.GetCachedTitle());
					const bool bRefreshNode = NodesModified.Contains(Node.GetNodeID());
					if (Result.GetHasDirtiedNode() || bTitleUpdated || bClassChanged || bForceRefreshViews || bRefreshNode)
					{
						Node.SyncChangeIDs();
						if (MetasoundGraphEditor.IsValid())
						{
							MetasoundGraphEditor->RefreshNode(Node);
						}
					}
				}

				TArray<UObject*> Selection;

				if (!MembersModified.IsEmpty() || bForceRefreshViews)
				{
					UMetasoundEditorGraphMember* SelectedMember = RefreshGraphMemberMenu();

					// If no member was selected by an action (ex. undo/redo), select a modified member 
					if (!SelectedMember)
					{
						UMetasoundEditorGraph& Graph = GetMetaSoundGraphChecked();
						for (const FGuid& MemberGuid : MembersModified)
						{
							if (UObject* Member = Graph.FindMember(MemberGuid))
							{
								// Currently only one member can be selected at a time, so only first found is added
								Selection.Add(Member);
								break;
							}
						}
					}
				}

				// Only refresh details panel if
				// 1. Forcing refresh with modify context option
				// 2. The currently selected object(s) is/are modified.
				// 3. If the selection is changed via the modify context, it will automatically dirty & refresh via 'SetSelection' below
				if (bForceRefreshViews)
				{
					RefreshDetails();
				}
				else if (!NodesModified.IsEmpty() || !MembersModified.IsEmpty())
				{
					TArray<TWeakObjectPtr<UObject>> SelectedObjects = MetasoundDetails->GetSelectedObjects();
					const bool bShouldRefreshDetails = Algo::AnyOf(SelectedObjects, [&NodesModified, &MembersModified](const TWeakObjectPtr<UObject>& Obj)
					{
						if (const UMetasoundEditorGraphNode* Node = Cast<const UMetasoundEditorGraphNode>(Obj.Get()))
						{
							return NodesModified.Contains(Node->GetNodeID());
						}
						if (const UMetasoundEditorGraphMember* Member = Cast<const UMetasoundEditorGraphMember>(Obj.Get()))
						{
							return MembersModified.Contains(Member->GetMemberID());
						}
						return false;
					});
					if (bShouldRefreshDetails)
					{
						RefreshDetails();
					}

					// TODO: Because input editor nodes are not one-to-one, this can cause multi-selection
					// when not desired.  Once input alias templates are complete, this can be switched on.
					// For now, callsites are just directly setting selection via ClearSelectionAndSelectNode.
// 					MetasoundGraphEditor->ClearSelectionSet();
// 
// 					const UMetasoundEditorGraph& Graph = GetMetaSoundGraphChecked();
// 
// 					TArray<UObject*> NodeSelection;
// 					for (UEdGraphNode* Node : Graph.Nodes)
// 					{
// 						if (UMetasoundEditorGraphNode* MetaSoundNode = Cast<UMetasoundEditorGraphNode>(Node))
// 						{
// 							if (NodesModified.Contains(MetaSoundNode->GetNodeID()))
// 							{
// 								if (UMetasoundEditorGraphMemberNode* MemberNode = Cast<UMetasoundEditorGraphMemberNode>(Node))
// 								{
// 									NodeSelection.Add(MemberNode->GetMember());
// 								}
// 								else
// 								{
// 									NodeSelection.Add(Node);
// 								}
// 
// 								MetasoundGraphEditor->SetNodeSelection(Node, /*bSelect=*/true);
// 
// 							}
// 						}
// 					}
// 
// 					if (!NodeSelection.IsEmpty())
// 					{
// 						Selection = NodeSelection;
// 					}
				}

				if (!InterfacesModified.IsEmpty() || bForceRefreshViews)
				{
					RefreshInterfaces();
				}

				HighestMessageSeverity = GetMetaSoundGraphChecked().GetHighestMessageSeverity();

				// Modify data has been observed both from synchronization & by
				// updating views by this point, so full reset is completed here.
				MetasoundAsset->GetModifyContext().Reset();

				if (!Selection.IsEmpty())
				{
					SetSelection(Selection);

				}

				// Avoids details panel displaying
				// removed members in certain cases.
				RemoveInvalidSelection();
			}
		}

		TStatId FEditor::GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FMetasoundEditor, STATGROUP_Tickables);
		}

		FText FEditor::GetSectionTitle(ENodeSection InSection) const
		{
			const int32 SectionIndex = static_cast<int32>(InSection);
			if (ensure(NodeSectionNames.IsValidIndex(SectionIndex)))
			{
				return NodeSectionNames[SectionIndex];
			}

			return FText::GetEmpty();
		}

		FText FEditor::OnGetSectionTitle(int32 InSectionID)
		{
			if (ensure(NodeSectionNames.IsValidIndex(InSectionID)))
			{
				return NodeSectionNames[InSectionID];
			}

			return FText::GetEmpty();
		}

		bool FEditor::IsGraphEditable() const
		{
			if (!Metasound)
			{
				return false;
			}

			FMetasoundAssetBase* MetasoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(Metasound);
			check(MetasoundAsset);
			return MetasoundAsset->GetRootGraphHandle()->GetGraphStyle().bIsGraphEditable;
		}

		void FEditor::ClearSelectionAndSelectNode(UEdGraphNode* Node)
		{
			if (MetasoundGraphEditor.IsValid())
			{
				MetasoundGraphEditor->ClearSelectionSet();
				MetasoundGraphEditor->SetNodeSelection(Node, /*bSelect=*/true);
			}
		}

		TSharedRef<SWidget> FEditor::OnGetMenuSectionWidget(TSharedRef<SWidget> RowWidget, int32 InSectionID)
		{
			TWeakPtr<SWidget> WeakRowWidget = RowWidget;

			FText AddNewText;
			FName MetaDataTag;

			if (IsGraphEditable())
			{
				switch (static_cast<ENodeSection>(InSectionID))
				{
				case ENodeSection::Inputs:
				{
					AddNewText = LOCTEXT("AddNewInput", "Input");
					MetaDataTag = "AddNewInput";
					return CreateAddButton(InSectionID, AddNewText, MetaDataTag);
				}
				break;

				case ENodeSection::Outputs:
				{
					AddNewText = LOCTEXT("AddNewOutput", "Output");
					MetaDataTag = "AddNewOutput";
					return CreateAddButton(InSectionID, AddNewText, MetaDataTag);
				}
				break;
	
				case ENodeSection::Variables:
				{
					AddNewText = LOCTEXT("AddNewVariable", "Variable");
					MetaDataTag = "AddNewVariable";
					return CreateAddButton(InSectionID, AddNewText, MetaDataTag);
				}
				break;

				default:
					break;
				}
			}

			return SNullWidget::NullWidget;
		}

		bool FEditor::CanAddNewElementToSection(int32 InSectionID) const
		{
			return true;
		}

		FReply FEditor::OnAddButtonClickedOnSection(int32 InSectionID)
		{
			if (!Metasound)
			{
				return FReply::Unhandled();
			}

			UMetasoundEditorGraph& Graph = GetMetaSoundGraphChecked();

			TArray<TObjectPtr<UObject>> SelectedObjects;

			FName NameToSelect;
			switch (static_cast<ENodeSection>(InSectionID))
			{
				case ENodeSection::Inputs:
				{
					const FScopedTransaction Transaction(LOCTEXT("AddInputNode", "Add MetaSound Input"));
					Metasound->Modify();

					const FName DataTypeName = GetMetasoundDataTypeName<float>();
					FCreateNodeVertexParams VertexParams;
					VertexParams.DataType = DataTypeName;

					Frontend::FNodeHandle NodeHandle = FGraphBuilder::AddInputNodeHandle(*Metasound, VertexParams);
					if (ensure(NodeHandle->IsValid()))
					{
						NameToSelect = NodeHandle->GetNodeName();

						TObjectPtr<UMetasoundEditorGraphInput> Input = Graph.FindOrAddInput(NodeHandle);
						if (ensure(Input))
						{
							SelectedObjects.Add(Input);
						}
					}
				}
				break;

				case ENodeSection::Outputs:
				{
					const FScopedTransaction Transaction(TEXT(""), LOCTEXT("AddOutputNode", "Add MetaSound Output"), Metasound);
					Metasound->Modify();

					const FName DataTypeName = GetMetasoundDataTypeName<float>();
					FCreateNodeVertexParams VertexParams;
					VertexParams.DataType = DataTypeName;

					Frontend::FNodeHandle NodeHandle = FGraphBuilder::AddOutputNodeHandle(*Metasound, VertexParams);
					if (ensure(NodeHandle->IsValid()))
					{
						NameToSelect = NodeHandle->GetNodeName();

						TObjectPtr<UMetasoundEditorGraphOutput> Output = Graph.FindOrAddOutput(NodeHandle);
						if (ensure(Output))
						{
							SelectedObjects.Add(Output);
						}
					}
				}
				break;

				case ENodeSection::Variables:
				{
					const FScopedTransaction Transaction(TEXT(""), LOCTEXT("AddVariableNode", "Add MetaSound Variable"), Metasound);
					Metasound->Modify();

					const FName DataTypeName = GetMetasoundDataTypeName<float>();
					
					Frontend::FVariableHandle FrontendVariable = FGraphBuilder::AddVariableHandle(*Metasound, DataTypeName);
					if (ensure(FrontendVariable->IsValid()))
					{
						TObjectPtr<UMetasoundEditorGraphVariable> EditorVariable = Graph.FindOrAddVariable(FrontendVariable);
						if (ensure(EditorVariable))
						{
							SelectedObjects.Add(EditorVariable);
							NameToSelect = EditorVariable->GetMemberName();
						}
					}
				}
				break;

				default:
				return FReply::Unhandled();
			}

			FGraphBuilder::RegisterGraphWithFrontend(*Metasound);

			if (GraphMembersMenu.IsValid())
			{
				GraphMembersMenu->RefreshAllActions(/* bPreserveExpansion */ true);
				if (!NameToSelect.IsNone())
				{
					GraphMembersMenu->SelectItemByName(NameToSelect);
					SetSelection(SelectedObjects);
				}
			}
			return FReply::Handled();
		}

		TSharedRef<SWidget> FEditor::CreateAddButton(int32 InSectionID, FText AddNewText, FName MetaDataTag)
		{
			return
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked(this, &FEditor::OnAddButtonClickedOnSection, InSectionID)
				.IsEnabled(this, &FEditor::CanAddNewElementToSection, InSectionID)
				.ContentPadding(FMargin(1, 0))
				.AddMetaData<FTagMetaData>(FTagMetaData(MetaDataTag))
				.ToolTipText(AddNewText)
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.PlusCircle"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				];
		}
	}
}
#undef LOCTEXT_NAMESPACE

