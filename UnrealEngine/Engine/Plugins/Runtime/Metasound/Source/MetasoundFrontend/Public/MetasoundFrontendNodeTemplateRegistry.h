// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundFrontendController.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendTransform.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "UObject/NoExportTypes.h"


// Forward Declarations
struct FMetaSoundFrontendDocumentBuilder;


namespace Metasound::Frontend
{
	/**
		* Base interface for a node template, which acts in place of frontend node class and respective instance(s).
		* Instances are preprocessed, allowing for custom graph manipulation prior to generating a respective runtime
		* graph operator representation.
		*/
	class METASOUNDFRONTEND_API INodeTemplate
	{
	public:
		virtual ~INodeTemplate() = default;

		// Returns note template class name.
		virtual const FMetasoundFrontendClassName& GetClassName() const = 0;

		UE_DEPRECATED(5.4, "Use version that does not provide a preprocessed document")
		virtual TUniquePtr<INodeTransform> GenerateNodeTransform(FMetasoundFrontendDocument& InDocument) const { return nullptr; }

		// Generates node transform that is used to preprocess nodes.
		virtual TUniquePtr<INodeTransform> GenerateNodeTransform() const { return nullptr; }

		// Returns the class definition for the given node class template.
		virtual const FMetasoundFrontendClass& GetFrontendClass() const = 0;

		// Returns access type of the given input within the provided builder's document
		virtual EMetasoundFrontendVertexAccessType GetNodeInputAccessType(const FMetaSoundFrontendDocumentBuilder& InBuilder, const FGuid& InNodeID, const FGuid& InVertexID) const = 0;

		// Returns access type of the given output within the provided builder's document
		virtual EMetasoundFrontendVertexAccessType GetNodeOutputAccessType(const FMetaSoundFrontendDocumentBuilder& InBuilder, const FGuid& InNodeID, const FGuid& InVertexID) const = 0;

		UE_DEPRECATED(5.4, "Use version number or classname instead")
		virtual const FMetasoundFrontendVersion& GetVersion() const { const static FMetasoundFrontendVersion NullVersion; return NullVersion; }

		// Returns note template class version.
		virtual const FMetasoundFrontendVersionNumber& GetVersionNumber() const = 0;

#if WITH_EDITOR
		// Returns whether or not the given node template has the necessary
		// required connections to be preprocessed (editor only).
		virtual bool HasRequiredConnections(FConstNodeHandle InNodeHandle, FString* OutMessage = nullptr) const = 0;
#endif // WITH_EDITOR

		// Returns whether template can dynamically assign a node's input access type (as opposed to it being assigned on the class input definition)
		virtual bool IsInputAccessTypeDynamic() const = 0;

		// Returns whether template can dynamically assign a node's output's access type (as opposed to it being assigned on the class output definition)
		virtual bool IsOutputAccessTypeDynamic() const = 0;

		// Given the provided node interface, returns whether or not it conforms to an expected format
		// that can be successfully manipulated by a generated node template transform.
		virtual bool IsValidNodeInterface(const FMetasoundFrontendNodeInterface& InNodeInterface) const = 0;
	};

	class METASOUNDFRONTEND_API FNodeTemplateBase : public INodeTemplate
	{
		virtual bool IsValidNodeInterface(const FMetasoundFrontendNodeInterface& InNodeInterface) const override;
		virtual bool IsInputAccessTypeDynamic() const override;
		virtual bool IsOutputAccessTypeDynamic() const override;
		virtual EMetasoundFrontendVertexAccessType GetNodeInputAccessType(const FMetaSoundFrontendDocumentBuilder& InBuilder, const FGuid& InNodeID, const FGuid& InVertexID) const override;
		virtual EMetasoundFrontendVertexAccessType GetNodeOutputAccessType(const FMetaSoundFrontendDocumentBuilder& InBuilder, const FGuid& InNodeID, const FGuid& InVertexID) const override;
	};

	class METASOUNDFRONTEND_API INodeTemplateRegistry
	{
	public:
		// Returns singleton template registry.
		static INodeTemplateRegistry& Get();

		virtual ~INodeTemplateRegistry() = default;

		// Find a template with the given key. Returns null if entry not found with given key.
		virtual const INodeTemplate* FindTemplate(const FNodeRegistryKey& InKey) const = 0;
	};

	// Register & Unregister are not publicly accessible implementation as the API
	// is in beta and, currently, only to be used by internal implementation (ex. reroute nodes).
	void RegisterNodeTemplate(TUniquePtr<INodeTemplate>&& InTemplate);

	UE_DEPRECATED(5.4, "Use version that provides class name and version instead")
	void UnregisterNodeTemplate(const FMetasoundFrontendVersion& InNodeTemplateVersion);

	void UnregisterNodeTemplate(const FMetasoundFrontendClassName& InClassName, const FMetasoundFrontendVersionNumber& InTemplateVersion);
} // namespace Metasound::Frontend
