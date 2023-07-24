// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "Logging/TokenizedMessage.h"
#include "MetasoundEditorGraphNode.h"
#include "MetasoundEditorGraph.h"

class UMetasoundEditorGraphNode;


namespace Metasound
{
	namespace Editor
	{
		struct METASOUNDEDITOR_API FGraphNodeValidationResult
		{
			FGraphNodeValidationResult(UMetasoundEditorGraphNode& InNode);

			// Whether or not validation operations on result have
			// dirtied the associated Node (Validation doesn't mark
			// node as dirtied to avoid resave being required at
			// asset level)
			bool GetHasDirtiedNode() const;

			UMetasoundEditorGraphNode& GetNodeChecked() const;

			// Whether associated node is in invalid state, i.e.
			// may fail to build or may result in undefined behavior.
			bool GetIsInvalid() const;

			void SetMessage(EMessageSeverity::Type InSeverity, const FString& InMessage);
			void SetPinOrphaned(UEdGraphPin& Pin, bool bIsOrphaned);

		private:
			// Node associated with validation result
			UMetasoundEditorGraphNode* Node = nullptr;

			// Whether validation made changes to the node and is now in a dirty state
			bool bHasDirtiedNode = false;
		};

		struct METASOUNDEDITOR_API FGraphValidationResults
		{
			TArray<FGraphNodeValidationResult> NodeResults;

			// Results corresponding with node validation
			const TArray<FGraphNodeValidationResult>& GetResults() const;

			// Returns highest message severity of validated nodes
			EMessageSeverity::Type GetHighestMessageSeverity() const;
		};
	} // namespace Editor
} // namespace Metasound
