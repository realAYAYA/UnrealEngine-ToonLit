// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Delegates/DelegateCombinations.h"
#include "MetasoundFrontendDocument.h"
#include "Templates/SharedPointer.h"


DECLARE_MULTICAST_DELEGATE_OneParam(FOnMetaSoundFrontendDocumentMutateArray, int32 /* Index */);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMetaSoundFrontendDocumentMutateInterfaceArray, const FMetasoundFrontendInterface& /* Interface */);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMetaSoundFrontendDocumentRemoveSwappingArray, int32 /* Index */, int32 /* LastIndex */);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMetaSoundFrontendDocumentRenameClass, const int32 /* Index */, const FMetasoundFrontendClassName& /* NewName */);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnMetaSoundFrontendDocumentMutateNodeInputLiteralArray, int32 /* NodeIndex */, int32 /* VertexIndex */, int32 /* LiteralIndex */);


namespace Metasound::Frontend
{
	struct METASOUNDFRONTEND_API FInterfaceModifyDelegates
	{
		FOnMetaSoundFrontendDocumentMutateInterfaceArray OnInterfaceAdded;
		FOnMetaSoundFrontendDocumentMutateInterfaceArray OnRemovingInterface;

		FOnMetaSoundFrontendDocumentMutateArray OnInputAdded;
		FOnMetaSoundFrontendDocumentMutateArray OnInputDefaultChanged;
		FOnMetaSoundFrontendDocumentMutateArray OnRemovingInput;

		FOnMetaSoundFrontendDocumentMutateArray OnOutputAdded;
		FOnMetaSoundFrontendDocumentMutateArray OnRemovingOutput;
	};

	struct METASOUNDFRONTEND_API FNodeModifyDelegates
	{
		FOnMetaSoundFrontendDocumentMutateArray OnNodeAdded;
		FOnMetaSoundFrontendDocumentRemoveSwappingArray OnRemoveSwappingNode;

		FOnMetaSoundFrontendDocumentMutateNodeInputLiteralArray OnNodeInputLiteralSet;
		FOnMetaSoundFrontendDocumentMutateNodeInputLiteralArray OnRemovingNodeInputLiteral;
	};

	struct METASOUNDFRONTEND_API FEdgeModifyDelegates
	{
		FOnMetaSoundFrontendDocumentMutateArray OnEdgeAdded;
		FOnMetaSoundFrontendDocumentRemoveSwappingArray OnRemoveSwappingEdge;
	};

	struct METASOUNDFRONTEND_API FDocumentModifyDelegates : TSharedFromThis<FDocumentModifyDelegates>
	{
		FOnMetaSoundFrontendDocumentMutateArray OnDependencyAdded;
		FOnMetaSoundFrontendDocumentRemoveSwappingArray OnRemoveSwappingDependency;
		FOnMetaSoundFrontendDocumentRenameClass OnRenamingDependencyClass;

		FInterfaceModifyDelegates InterfaceDelegates;
		FNodeModifyDelegates NodeDelegates;
		FEdgeModifyDelegates EdgeDelegates;
	};
} // namespace Metasound::Frontend
