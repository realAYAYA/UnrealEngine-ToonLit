// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundFrontendController.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendTransform.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "UObject/NoExportTypes.h"


namespace Metasound
{
	namespace Frontend
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

			// Generates node transform that can cache document state for use when individually preprocessing nodes.
			virtual TUniquePtr<INodeTransform> GenerateNodeTransform(FMetasoundFrontendDocument& InDocument) const = 0;

			// Returns the class definition for the given node class template.
			virtual const FMetasoundFrontendClass& GetFrontendClass() const = 0;

			// Returns the version of the given node class template.
			virtual const FMetasoundFrontendVersion& GetVersion() const = 0;

			// Given the provided node interface, returns whether or not it conforms to an expected format
			// that can be successfully manipulated by a generated node template preprocessor.
			virtual bool IsValidNodeInterface(const FMetasoundFrontendNodeInterface& InNodeInterface) const = 0;

#if WITH_EDITOR
			// Returns whether or not the given node template has the necessary
			// required connections to be preprocessed (editor only).
			virtual bool HasRequiredConnections(FConstNodeHandle InNodeHandle) const = 0;
#endif // WITH_EDITOR
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
		void UnregisterNodeTemplate(const FMetasoundFrontendVersion& InNodeTemplateVersion);
	} // namespace Frontend
} // namespace Metasound
