// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MetasoundFrontendController.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendDocumentBuilder.h"
#include "MetasoundFrontendNodeTemplateRegistry.h"
#include "MetasoundFrontendTransform.h"
#include "Templates/SharedPointer.h"
#include "UObject/NoExportTypes.h"


namespace Metasound::Frontend
{
	class METASOUNDFRONTEND_API FRerouteNodeTemplate : public INodeTemplate
	{
	public:
		static const FMetasoundFrontendClassName ClassName;

		static const FMetasoundFrontendVersionNumber VersionNumber;

		static const FNodeRegistryKey& GetRegistryKey();

		static FMetasoundFrontendNodeInterface CreateNodeInterfaceFromDataType(FName InDataType);

		virtual ~FRerouteNodeTemplate() = default;

		virtual const FMetasoundFrontendClassName& GetClassName() const override;

		UE_DEPRECATED(5.4, "Use version that does not require mutating a provided PreprocessedDocument")
		virtual TUniquePtr<INodeTransform> GenerateNodeTransform(FMetasoundFrontendDocument& InPreprocessedDocument) const override;

		virtual TUniquePtr<INodeTransform> GenerateNodeTransform() const override;
		virtual const FMetasoundFrontendClass& GetFrontendClass() const override;
		virtual EMetasoundFrontendVertexAccessType GetNodeInputAccessType(const FMetaSoundFrontendDocumentBuilder& InBuilder, const FGuid& InNodeID, const FGuid& InVertexID) const override;
		virtual EMetasoundFrontendVertexAccessType GetNodeOutputAccessType(const FMetaSoundFrontendDocumentBuilder& InBuilder, const FGuid& InNodeID, const FGuid& InVertexID) const override;
		virtual const FMetasoundFrontendVersionNumber& GetVersionNumber() const override;
		virtual bool IsInputAccessTypeDynamic() const override;
		virtual bool IsOutputAccessTypeDynamic() const override;
		virtual bool IsValidNodeInterface(const FMetasoundFrontendNodeInterface& InNodeInterface) const override;

#if WITH_EDITOR
		virtual bool HasRequiredConnections(FConstNodeHandle InNodeHandle, FString* OutMessage = nullptr) const override;
#endif // WITH_EDITOR
	};
} // namespace Metasound::Frontend
