// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IMetasoundEditor.h"
#include "AudioMeterStyle.h"
#include "EdGraph/EdGraphNode.h"
#include "EditorUndoClient.h"
#include "Framework/Commands/UICommandList.h"
#include "GraphEditor.h"
#include "IDetailsView.h"
#include "Logging/TokenizedMessage.h"
#include "Math/UnrealMathUtility.h"
#include "MetasoundEditorGraphConnectionManager.h"
#include "MetasoundEditorGraphNode.h"
#include "MetasoundEditorMeter.h"
#include "MetasoundFrontend.h"
#include "MetasoundFrontendController.h"
#include "Meter.h"
#include "Misc/NotifyHook.h"
#include "SAudioMeter.h"
#include "SGraphActionMenu.h"
#include "SGraphPanel.h"
#include "SMetasoundPalette.h"
#include "Sound/AudioBus.h"
#include "Textures/SlateIcon.h"
#include "TickableEditorObject.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkitHost.h"
#include "UObject/GCObject.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SPanel.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"

#include "MetasoundEditor.generated.h"


// Forward Declarations
class FTabManager;
class SDockableTab;
class SGraphEditor;
class SMetasoundPalette;
class FSlateRect;
class IDetailsView;
class IToolkitHost;
class SVerticalBox;
class UAudioComponent;
class UEdGraphNode;
class UMetaSoundPatch;
class UMetasoundEditorGraph;
class UMetasoundEditorGraphNode;

struct FGraphActionNode;
struct FMeterResults;
struct FPropertyChangedEvent;


// Simple class for the interfaces details tab to keep track of its corresponding Metasound 
UCLASS(MinimalAPI)
class UMetasoundInterfacesView : public UObject
{
	GENERATED_BODY()
public:
	UMetasoundInterfacesView() = default;

	void SetMetasound(UObject* InMetasound) { Metasound = InMetasound; }
	const TWeakObjectPtr<UObject> GetMetasound() { return Metasound; }
private:
	TWeakObjectPtr<UObject> Metasound;
};

namespace Metasound
{
	namespace Editor
	{
		// Forward Declarations
		class FMetasoundGraphMemberSchemaAction;

		/* Enums to use when grouping the members in the list panel. Enum order dictates visible order. */
		enum class ENodeSection : uint8
		{
			None,
			Inputs,
			Outputs,
			Variables,

			COUNT
		};

		class FEditor : public IMetasoundEditor, public FGCObject, public FNotifyHook, public FEditorUndoClient, public FTickableEditorObject
		{
		public:
			static const FName EditorName;

			FEditor()
				: GraphConnectionManager(MakeUnique<FGraphConnectionManager>())
			{
			}

			virtual ~FEditor();

			virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
			virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;

			double GetPlayTime() const;
			TSharedPtr<SGraphEditor> GetGraphEditor() const;

			/** Edits the specified Metasound object */
			void InitMetasoundEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UObject* ObjectToEdit);

			UAudioComponent* GetAudioComponent() const;

			/** IMetasoundEditor interface */
			virtual UObject* GetMetasoundObject() const override;
			virtual void SetSelection(const TArray<UObject*>& SelectedObjects) override;
			virtual bool GetBoundsForSelectedNodes(FSlateRect& Rect, float Padding) override;

			/** IToolkit interface */
			virtual FName GetToolkitFName() const override;
			virtual FText GetBaseToolkitName() const override;
			virtual FString GetWorldCentricTabPrefix() const override;
			virtual FLinearColor GetWorldCentricTabColorScale() const override;
			virtual const FSlateBrush* GetDefaultTabIcon() const override;
			virtual FLinearColor GetDefaultTabColor() const override;

			/** IAssetEditorInstance interface */
			virtual FName GetEditorName() const override;

			virtual FString GetDocumentationLink() const override
			{
				return FString(TEXT("working-with-audio/sound-sources/meta-sounds"));
			}

			/** FGCObject interface */
			virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
			virtual FString GetReferencerName() const override
			{
				return TEXT("Metasound::Editor::FEditor");
			}

			/** FEditorUndoClient Interface */
			virtual void PostUndo(bool bSuccess) override;
			virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }

			/** FTickableEditorObject Interface */
			virtual void Tick(float DeltaTime) override;
			virtual TStatId GetStatId() const override;
			virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }

			virtual void Play() override;
			virtual void Stop() override;

			virtual bool IsPlaying() const override;

			/** Whether pasting the currently selected nodes is permissible */
			bool CanPasteNodes();

			/** Duplicates the selected node(s) in the graph */
			void DuplicateNodes();

			/** Forces all UX pertaining to the root graph's details panel to be refreshed. */
			void RefreshDetails();
			
			/** Pastes node(s) from the clipboard to the graph */
			void PasteNodes(const FVector2D* InLocation = nullptr);
			void PasteNodes(const FVector2D* InLocation, const FText& InTransactionText);

			/** Returns Graph Connection Manager associated with this editor */
			FGraphConnectionManager& GetConnectionManager();
			const FGraphConnectionManager& GetConnectionManager() const;

			/** Forces all UX pertaining to the root graph's interface to be refreshed, returning the first selected member. */
			UMetasoundEditorGraphMember* RefreshGraphMemberMenu();

			/** Forces refresh of interfaces list. */
			void RefreshInterfaces();

			/** Updates selected node classes to highest class found in the MetaSound Class Registry. */
			void UpdateSelectedNodeClasses();

			/* Whether the displayed graph is marked as editable */
			bool IsGraphEditable() const;

			void ClearSelectionAndSelectNode(UEdGraphNode* Node);

			int32 GetNumNodesSelected() const
			{
				return MetasoundGraphEditor->GetSelectedNodes().Num();
			}

			/** Creates analyzers */
			void CreateAnalyzers();

			void BuildTransport(FToolBarBuilder& InToolBarBuilder);

			/** Destroys analyzers */
			void DestroyAnalyzers();

			/** Jumps to the given nodes on the graph (templated to support arrays of various MetaSound graph node types) */
			template<typename TNodeType = UMetasoundEditorGraphNode>
			void JumpToNodes(const TArray<TNodeType*>& InNodes)
			{
				if (!MetasoundGraphEditor.IsValid())
				{
					return;
				}

				MetasoundGraphEditor->ClearSelectionSet();
				const UMetasoundEditorGraph& Graph = GetMetaSoundGraphChecked();
				if (!InNodes.IsEmpty())
				{
					if (SGraphPanel* GraphPanel = MetasoundGraphEditor->GetGraphPanel())
					{
						FVector2D BottomLeft = { TNumericLimits<float>::Max(), TNumericLimits<float>::Max() };
						FVector2D TopRight = { TNumericLimits<float>::Min(), TNumericLimits<float>::Min() };
						for (TNodeType* Node : InNodes)
						{
							if (!Node || Node->GetGraph() != &Graph)
							{
								continue;
							}

							constexpr bool bSelected = true;
							MetasoundGraphEditor->SetNodeSelection(Node, bSelected);
							BottomLeft.X = FMath::Min(BottomLeft.X, Node->NodePosX);
							BottomLeft.Y = FMath::Min(BottomLeft.Y, Node->NodePosY);
							TopRight.X = FMath::Max(TopRight.X, Node->NodePosX + Node->EstimateNodeWidth());
							TopRight.Y = FMath::Max(TopRight.Y, Node->NodePosY);
						}

						GraphPanel->JumpToRect(BottomLeft, TopRight);
					}
				}
			}

		protected:
			// Callbacks for action tree
			bool CanRenameOnActionNode(TWeakPtr<FGraphActionNode> InSelectedNode) const;
			bool CanAddNewElementToSection(int32 InSectionID) const;
			void CollectAllActions(FGraphActionListBuilderBase& OutAllActions);
			void CollectStaticSections(TArray<int32>& StaticSectionIDs);
			TSharedRef<SWidget> CreateAddButton(int32 InSectionID, FText AddNewText, FName MetaDataTag);
			FText GetFilterText() const;
			bool HandleActionMatchesName(FEdGraphSchemaAction* InAction, const FName& InName) const;
			FReply OnActionDragged(const TArray<TSharedPtr<FEdGraphSchemaAction>>& InActions, const FPointerEvent& MouseEvent);
			void OnMemberActionDoubleClicked(const TArray< TSharedPtr<FEdGraphSchemaAction> >& InActions);
			FActionMenuContent OnCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed);
			void OnActionSelected(const TArray<TSharedPtr<FEdGraphSchemaAction>>& InActions, ESelectInfo::Type InSelectionType);
			FReply OnAddButtonClickedOnSection(int32 InSectionID);
			TSharedRef<SWidget> OnGetMenuSectionWidget(TSharedRef<SWidget> RowWidget, int32 InSectionID);
			FText GetSectionTitle(ENodeSection InSection) const;
			FText OnGetSectionTitle(int32 InSectionID);
			TSharedRef<SWidget> OnCreateWidgetForAction(struct FCreateWidgetForActionData* const InCreateData);
			TSharedPtr<SWidget> OnContextMenuOpening();

			/** Called when the selection changes in the GraphEditor */
			void OnSelectedNodesChanged(const TSet<UObject*>& NewSelection);

			FGraphAppearanceInfo GetGraphAppearance() const;

			UMetasoundEditorGraph& GetMetaSoundGraphChecked();

			FText GetGraphStatusDescription() const;
			const FSlateIcon& GetPlayIcon() const;
			const FSlateIcon& GetStopIcon() const;

			/**
			 * Called when a node's title is committed for a rename
			 *
			 * @param	NewText				New title text
			 * @param	CommitInfo			How text was committed
			 * @param	NodeBeingChanged	The node being changed
			 */
			void OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged);

			/** Deletes from the Metasound Menu (i.e. input or output) if in focus, or the currently selected nodes if the graph editor is in focus. */
			void DeleteSelected();

			void DeleteInterfaceItem(TSharedPtr<FMetasoundGraphMemberSchemaAction> ActionToDelete);

			/** Delete the currently selected nodes */
			void DeleteSelectedNodes();

			/** Delete the currently selected interface items */
			void DeleteSelectedInterfaceItems();

			/** Cut the currently selected nodes */
			void CutSelectedNodes();

			/** Copy the currently selected nodes */
			void CopySelectedNodes() const;

			/** Whether copying the currently selected node(s) is permissible */
			bool CanCopyNodes() const;

			/** Whether or not the currently selected node(s) can be duplicated */
			bool CanDuplicateNodes() const;

			/** Whether the currently selected node(s) can be deleted */
			bool CanDeleteNodes() const;

			/** Whether the currently selected interface item(s) can be deleted */
			bool CanDeleteInterfaceItems() const;

			/** Whether at least one of the currently selected node(s) can be renamed. */
			bool CanRenameSelectedNodes() const;

			/** Rename selected node (currently applies to comments and member nodes). */
			void RenameSelectedNode();

			/** Whether at least one of the currently selected interface item(s) can be renamed. */
			bool CanRenameSelectedInterfaceItems() const;

			/** Rename selected interface item. */
			void RenameSelectedInterfaceItem();

			/** Whether there are nodes to jump to for the currently selected interface item. */
			bool CanJumpToNodesForSelectedInterfaceItem() const;

			/** Jumps to the nodes corresponding to the first valid currently selected interface item. */
			void JumpToNodesForSelectedInterfaceItem();

			/** Called to undo the last action */
			void UndoGraphAction();

			/** Called to redo the last undone action */
			void RedoGraphAction();

			void RefreshEditorContext();

		private:
			void RemoveInvalidSelection();

			void SetPreviewID(uint32 InPreviewID);

			/** FNotifyHook interface */
			virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;

			/** Creates all internal widgets for the tabs to point at */
			void CreateInternalWidgets();

			/** Builds the toolbar widget for the Metasound editor */
			void ExtendToolbar();

			/** Binds new graph commands to delegates */
			void BindGraphCommands();

			FSlateIcon GetImportStatusImage() const;

			FSlateIcon GetExportStatusImage() const;

			FSlateIcon GetSettingsImage() const;

			// TODO: Move import/export out of editor and into import/export asset actions
			void Import();
			void Export();

			/** Toolbar command methods */
			void ExecuteNode();

			/** Whether we can play the current selection of nodes */
			bool CanExecuteNode() const;

			/** Either play the Metasound or stop currently playing sound */
			void TogglePlayback();

			/** Executes specified node (If supported) */
			void ExecuteNode(UEdGraphNode* Node);

			/** Sync the content browser to the current selection of nodes */
			void SyncInBrowser();

			/** Converts the MetaSound from a preset to a fully modifiable MetaSound. */
			void ConvertFromPreset();

			/** Show the Metasound object's Source settings in the Details panel */
			void EditSourceSettings();

			/** Show the Metasound object's settings in the Details panel */
			void EditMetasoundSettings();

			/** Add an input to the currently selected node */
			void AddInput();

			/** Whether we can add an input to the currently selected node */
			bool CanAddInput() const;

			/* Create comment node on graph */
			void OnCreateComment();

			/** Create new graph editor widget */
			void CreateGraphEditorWidget();

			TSharedPtr<SWidget> BuildAnalyzerWidget() const;

			void EditObjectSettings();

			void NotifyAssetPrimeComplete();
			void NotifyAssetPrimeInProgress();
			void NotifyDocumentVersioned();
			void NotifyNodePasteFailure_MultipleVariableSetters();
			void NotifyNodePasteFailure_ReferenceLoop();

			/** List of open tool panels; used to ensure only one exists at any one time */
			TMap<FName, TWeakPtr<SDockableTab>> SpawnedToolPanels;

			/** New Graph Editor */
			TSharedPtr<SGraphEditor> MetasoundGraphEditor;

			/** Details tab */
			TSharedPtr<IDetailsView> MetasoundDetails;

			/** Interfaces tab */
			TSharedPtr<IDetailsView> InterfacesDetails;
			TStrongObjectPtr<UMetasoundInterfacesView> InterfacesView;

			/** Metasound graph members menu */
			TSharedPtr<SGraphActionMenu> GraphMembersMenu;

			/** Meter used in the analyzer tab for auditioning preview output. */
			TSharedPtr<FEditorMeter> OutputMeter;

			/** Palette of Node types */
			TSharedPtr<SMetasoundPalette> Palette;

			/** Widget showing playtime that overlays the graph when previewing */
			TSharedPtr<STextBlock> PlayTimeWidget;
			double PlayTime = 0.0;

			TUniquePtr<FGraphConnectionManager> GraphConnectionManager;

			/** Command list for this editor */
			TSharedPtr<FUICommandList> GraphEditorCommands;

			/** The Metasound asset being edited */
			UObject* Metasound = nullptr;

			/** Whether or not metasound being edited is valid */
			bool bPassedValidation = true;

			/** Text content used when either duplicating or pasting from clipboard (avoids deserializing twice) */
			FString NodeTextToPaste;

			/** Boolean state for when selection change handle should not respond due to selection state
			  * being manually applied in code */
			bool bManuallyClearingGraphSelection = false;

			/** Whether or not registry is currently being primed due to assets still loading */
			bool bPrimingRegistry = false;

			int32 HighestMessageSeverity = EMessageSeverity::Info;
		};
	} // namespace Editor
} // namespace Metasound
