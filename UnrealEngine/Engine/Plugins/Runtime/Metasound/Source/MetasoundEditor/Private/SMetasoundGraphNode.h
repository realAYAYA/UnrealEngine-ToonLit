// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once


#include "Input/Reply.h"
#include "Layout/Visibility.h"
#include "MetasoundFrontendDocument.h"
#include "Misc/Attribute.h"
#include "SGraphNode.h"
#include "SGraphNodeKnot.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SOverlay.h"


// Forward Declarations
class SAudioInputWidget;
class SGraphPin;
class SVerticalBox;
class UMetasoundEditorGraphMember;
class UMetasoundEditorGraphMemberDefaultLiteral;
class UMetasoundEditorGraphMemberNode;
class UMetasoundEditorGraphNode;


namespace Metasound
{
	namespace Editor
	{
		class SMetaSoundGraphNode : public SGraphNode
		{
			public:
			SLATE_BEGIN_ARGS(SMetaSoundGraphNode)
			{
			}

			SLATE_END_ARGS()
			virtual ~SMetaSoundGraphNode();

			void Construct(const FArguments& InArgs, class UEdGraphNode* InNode);

		protected:
			bool IsVariableAccessor() const;
			bool IsVariableMutator() const;

			// SGraphNode Interface
			virtual const FSlateBrush* GetShadowBrush(bool bSelected) const override;
			virtual void CreateInputSideAddButton(TSharedPtr<SVerticalBox> InputBox) override;
			virtual void CreateOutputSideAddButton(TSharedPtr<SVerticalBox> OutputBox) override;
			virtual TSharedPtr<SGraphPin> CreatePinWidget(UEdGraphPin* InPin) const override;
			virtual void CreateStandardPinWidget(UEdGraphPin* InPin) override;
			virtual TSharedRef<SWidget> CreateNodeContentArea() override;
			virtual TSharedRef<SWidget> CreateTitleWidget(TSharedPtr<SNodeTitle> NodeTitle) override;
			virtual const FSlateBrush* GetNodeBodyBrush() const override;
			virtual TSharedRef<SWidget> CreateTitleRightWidget() override;
			virtual EVisibility IsAddPinButtonVisible() const override;
			virtual FReply OnAddPin() override;
			virtual void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty = true) override;
			virtual void SetDefaultTitleAreaWidget(TSharedRef<SOverlay> DefaultTitleAreaWidget) override;
			virtual void GetOverlayBrushes(bool bSelected, const FVector2D WidgetSize, TArray<FOverlayBrushInfo>& Brushes) const override;

			FLinearColor GetNodeTitleColorOverride() const;

			FName GetLiteralDataType() const;
			UMetasoundEditorGraphNode& GetMetaSoundNode();
			const UMetasoundEditorGraphNode& GetMetaSoundNode() const;

		public:
			static void ExecuteTrigger(UMetasoundEditorGraphMemberDefaultLiteral& Literal);
			static TSharedRef<SWidget> CreateTriggerSimulationWidget(UMetasoundEditorGraphMemberDefaultLiteral& Literal, TAttribute<EVisibility>&& InVisibility, TAttribute<bool>&& InEnablement, const FText* InToolTip = nullptr);

		private:
			// If this node represents a graph member node, returns corresponding member.
			UMetasoundEditorGraphMember* GetMetaSoundMember();

			// If this node represents a graph member node, returns cast node.
			UMetasoundEditorGraphMemberNode* GetMetaSoundMemberNode();

			TAttribute<EVisibility> GetSimulationVisibilityAttribute() const;

			// Slider widget for float input
			TSharedPtr<SAudioInputWidget> InputWidget;
			// Handle for on value changed delegate for input slider 
			FDelegateHandle InputSliderOnValueChangedDelegateHandle;
			// Handle for on input slider range changed  
			FDelegateHandle InputSliderOnRangeChangedDelegateHandle;

			// Whether the input widget is currently transacting 
			// for keeping track of transaction state across delegates to only commit transaction on value commit
			bool bIsInputWidgetTransacting = false;

			EMetasoundFrontendClassType ClassType;
		};

		class SMetaSoundGraphNodeKnot : public SGraphNodeKnot
		{
		public:
			SLATE_BEGIN_ARGS(SMetaSoundGraphNode)
			{
			}

			SLATE_END_ARGS()

			void Construct(const FArguments& InArgs, class UEdGraphNode* InNode);

			virtual TSharedPtr<SGraphPin> CreatePinWidget(UEdGraphPin* Pin) const override;
			virtual void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty) override;

			UMetasoundEditorGraphNode& GetMetaSoundNode();
			const UMetasoundEditorGraphNode& GetMetaSoundNode() const;
		};
	} // namespace Editor
} // namespace Metasound
