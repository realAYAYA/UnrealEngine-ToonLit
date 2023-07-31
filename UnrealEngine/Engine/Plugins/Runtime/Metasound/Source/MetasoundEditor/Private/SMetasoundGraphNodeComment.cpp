// Copyright Epic Games, Inc. All Rights Reserved.
#include "SMetasoundGraphNodeComment.h"

#include "EdGraphNode_Comment.h"
#include "MetasoundEditorGraphNode.h"

namespace Metasound
{
	namespace Editor
	{
		void SMetasoundGraphNodeComment::MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty)
		{
			SGraphNodeComment::MoveTo(NewPosition, NodeFilter, bMarkDirty);

			// Update Frontend node positions for unselected nodes that are dragged along with the comment box
			// partially copied from SGraphNodeComment::MoveTo
			// Don't drag note content if either of the shift keys are down.
			FModifierKeysState KeysState = FSlateApplication::Get().GetModifierKeys();
			if (!KeysState.IsShiftDown())
			{
				UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(GraphNode);
				if (CommentNode && CommentNode->MoveMode == ECommentBoxMode::GroupMovement)
				{
					FVector2D PositionDelta = NewPosition - GetPosition();

					// Now update any nodes which are touching the comment but *not* selected
					// Selected nodes will be moved as part of the normal selection code
					TSharedPtr< SGraphPanel > Panel = GetOwnerPanel();
					for (FCommentNodeSet::TConstIterator NodeIt(CommentNode->GetNodesUnderComment()); NodeIt; ++NodeIt)
					{
						if (UMetasoundEditorGraphNode* MetasoundGraphNode = Cast<UMetasoundEditorGraphNode>(*NodeIt))
						{
							FVector2D MetasoundNodePosition = FVector2D(MetasoundGraphNode->NodePosX, MetasoundGraphNode->NodePosY);
							MetasoundNodePosition += PositionDelta;

							MetasoundGraphNode->GetMetasoundChecked().Modify();
							MetasoundGraphNode->UpdateFrontendNodeLocation(MetasoundNodePosition);
						}
					}
				}
			}
		}

	} // namespace Editor
} // namespace Metasound
