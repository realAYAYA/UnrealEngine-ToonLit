// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Internationalization/Text.h"
#include "Interfaces/MetasoundFrontendInterfaceRegistry.h"
#include "MetasoundAssetBase.h"
#include "MetasoundFrontendController.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundVertex.h"
#include "Templates/Function.h"


// Forward Declarations
struct FMetaSoundFrontendDocumentBuilder;


namespace Metasound::Frontend
{
	namespace DocumentTransform
	{
#if WITH_EDITOR
		using FGetNodeDisplayNameProjection = TFunction<FText(const FNodeHandle&)>;
		using FGetNodeDisplayNameProjectionRef = TFunctionRef<FText(const FNodeHandle&)>;

		METASOUNDFRONTEND_API void RegisterNodeDisplayNameProjection(FGetNodeDisplayNameProjection&& InNameProjection);
		METASOUNDFRONTEND_API FGetNodeDisplayNameProjectionRef GetNodeDisplayNameProjection();
#endif // WITH_EDITOR
	}

	/** Interface for transforms applied to documents. */
	class METASOUNDFRONTEND_API IDocumentTransform
	{
	public:
		virtual ~IDocumentTransform() = default;

		/** Return true if InDocument was modified, false otherwise. */
		virtual bool Transform(FDocumentHandle InDocument) const = 0;

		/** Return true if InDocument was modified, false otherwise.
			* This function is soft deprecated.  It is not pure virtual
			* to grandfather in old transform implementation. Old transforms
			* should be deprecated and rewritten to use the Controller-less
			* API in the interest of better performance and simplicity.
			*/
		virtual bool Transform(FMetasoundFrontendDocument& InOutDocument) const;
	};

	/** Interface for transforms applied to a graph. */
	class METASOUNDFRONTEND_API IGraphTransform
	{
	public:
		virtual ~IGraphTransform() = default;

		UE_DEPRECATED(5.3, "Deprecated: unused function which encouraged breaking const behavior of transform state.")
		virtual FMetasoundFrontendDocument& GetOwningDocument() const { static FMetasoundFrontendDocument BaseDoc; return BaseDoc; };

		/** Return true if the graph was modified, false otherwise. */
		virtual bool Transform(FMetasoundFrontendGraph& InOutGraph) const = 0;
	};

	/** Interface for transforming a node. */
	class METASOUNDFRONTEND_API INodeTransform
	{
	public:
		virtual ~INodeTransform() = default;

		UE_DEPRECATED(5.3, "Deprecated: unused function which encouraged breaking const behavior of transform state.")
		virtual FMetasoundFrontendDocument& GetOwningDocument() const { static FMetasoundFrontendDocument BaseDoc; return BaseDoc; };

		UE_DEPRECATED(5.3, "Deprecated: unused function which encouraged breaking const behavior of transform state.")
		virtual FMetasoundFrontendGraph& GetOwningGraph() const { static FMetasoundFrontendGraph BaseGraph; return BaseGraph; }

		/** Return true if the node was modified, false otherwise. */
		virtual bool Transform(const FGuid& InNodeID, FMetaSoundFrontendDocumentBuilder& OutBuilder) const;

		UE_DEPRECATED(5.4, "Use transform which provides builder and const node representation to apply node transform to mutate underlying builder document")
		virtual bool Transform(FMetasoundFrontendNode& InOutNode) const;
	};

	/** Adds or swaps document members (inputs, outputs) and removing any document members where necessary and adding those missing. */
	class METASOUNDFRONTEND_API FModifyRootGraphInterfaces : public IDocumentTransform
	{
	public:
		FModifyRootGraphInterfaces(const TArray<FMetasoundFrontendInterface>& InInterfacesToRemove, const TArray<FMetasoundFrontendInterface>& InInterfacesToAdd);
		FModifyRootGraphInterfaces(const TArray<FMetasoundFrontendVersion>& InInterfaceVersionsToRemove, const TArray<FMetasoundFrontendVersion>& InInterfaceVersionsToAdd);

#if WITH_EDITOR
		// Whether or not to propagate node locations to new members. Setting to false
		// results in members not having a default physical location in the editor graph.
		void SetDefaultNodeLocations(bool bInSetDefaultNodeLocations);
#endif // WITH_EDITOR

		// Override function used to match removed members with added members, allowing
		// transform to preserve connections made between removed interface members & new interface members
		// that may be related but not be named the same.
		void SetNamePairingFunction(const TFunction<bool(FName, FName)>& InNamePairingFunction);

		virtual bool Transform(FDocumentHandle InDocument) const override;
		virtual bool Transform(FMetasoundFrontendDocument& InOutDocument) const override;

	private:
		bool AddMissingVertices(FGraphHandle GraphHandle) const;
		void Init(const TFunction<bool(FName, FName)>* InNamePairingFunction = nullptr);
		bool SwapPairedVertices(FGraphHandle GraphHandle) const;
		bool RemoveUnsupportedVertices(FGraphHandle GraphHandle) const;
		bool UpdateInterfacesInternal(FDocumentHandle DocumentHandle) const;

#if WITH_EDITORONLY_DATA
		void UpdateAddedVertexNodePositions(FGraphHandle GraphHandle) const;

		bool bSetDefaultNodeLocations = true;
#endif // WITH_EDITORONLY_DATA

		TArray<FMetasoundFrontendInterface> InterfacesToRemove;
		TArray<FMetasoundFrontendInterface> InterfacesToAdd;

		using FVertexPair = TTuple<FMetasoundFrontendClassVertex, FMetasoundFrontendClassVertex>;
		TArray<FVertexPair> PairedInputs;
		TArray<FVertexPair> PairedOutputs;

		struct FInputData
		{
			FMetasoundFrontendClassInput Input;
			const FMetasoundFrontendInterface* InputInterface = nullptr;
		};

		struct FOutputData
		{
			FMetasoundFrontendClassOutput Output;
			const FMetasoundFrontendInterface* OutputInterface = nullptr;
		};

		TArray<FInputData> InputsToAdd;
		TArray<FMetasoundFrontendClassInput> InputsToRemove;
		TArray<FOutputData> OutputsToAdd;
		TArray<FMetasoundFrontendClassOutput> OutputsToRemove;

	};

	/** Updates document's given interface to the most recent version. */
	class METASOUNDFRONTEND_API FUpdateRootGraphInterface : public IDocumentTransform
	{
	public:
		FUpdateRootGraphInterface(const FMetasoundFrontendVersion& InInterfaceVersion, const FString& InOwningAssetName=FString(TEXT("Unknown")))
			: InterfaceVersion(InInterfaceVersion)
			, OwningAssetName(InOwningAssetName)
		{
		}

		bool Transform(FDocumentHandle InDocument) const override;

	private:
		void GetUpdatePathForDocument(const FMetasoundFrontendVersion& InCurrentVersion, const FMetasoundFrontendVersion& InTargetVersion, TArray<const IInterfaceRegistryEntry*>& OutUpgradePath) const;
		bool UpdateDocumentInterface(const TArray<const IInterfaceRegistryEntry*>& InUpgradePath, FDocumentHandle InDocument) const;

		FMetasoundFrontendVersion InterfaceVersion;
		FString OwningAssetName;
	};

	/** Completely rebuilds the graph connecting a preset's inputs to the reference
		* document's root graph. It maintains previously set input values entered upon 
		* the presets wrapping graph. */
	class METASOUNDFRONTEND_API FRebuildPresetRootGraph : public IDocumentTransform
	{
	public:
		/** Create transform.
			* @param InReferenceDocument - The document containing the wrapped MetaSound graph.
			*/
		FRebuildPresetRootGraph(FConstDocumentHandle InReferencedDocument)
			: ReferencedDocument(InReferencedDocument)
		{
		}

		FRebuildPresetRootGraph(const FMetasoundFrontendDocument& InReferencedDocument);

		virtual bool Transform(FDocumentHandle InDocument) const override;
		virtual bool Transform(FMetasoundFrontendDocument& InOutDocument) const override;

	private:

		// Get the class inputs needed for this preset. Input literals set on 
		// the preset graph will be used if they are set and are marked as inheriting
		// the default from the referenced graph.
		TArray<FMetasoundFrontendClassInput> GenerateRequiredClassInputs(FDocumentHandle& InDocumentHandle, const FConstGraphHandle& InParentGraph, TSet<FName>& OutInputsInheritingDefault) const;

		// Get the class Outputs needed for this preset.
		TArray<FMetasoundFrontendClassOutput> GenerateRequiredClassOutputs(FDocumentHandle& InDocumentHandle, const FConstGraphHandle& InParentGraph) const;

		// Add inputs to parent graph and connect to wrapped graph node.
		void AddAndConnectInputs(const TArray<FMetasoundFrontendClassInput>& InClassInputs, FGraphHandle& InParentGraphHandle, FNodeHandle& InReferencedNode) const;

		// Add outputs to parent graph and connect to wrapped graph node.
		void AddAndConnectOutputs(const TArray<FMetasoundFrontendClassOutput>& InClassOutputs, FGraphHandle& InParentGraphHandle, FNodeHandle& InReferencedNode) const;

		FConstDocumentHandle ReferencedDocument = IDocumentController::GetInvalidHandle();
	};

	/** Automatically updates all nodes and respective dependencies in graph where
		* newer versions exist in the loaded MetaSound Class Node Registry.
		*/
	class METASOUNDFRONTEND_API FAutoUpdateRootGraph : public IDocumentTransform
	{
	public:
		/** Construct an AutoUpdate transform
			*
			* @param InDebugAssetPath - Asset path used for debug logs on warnings and errors.
			* @param bInLogWarningOnDroppedConnections - If true, warnings will be logged if a node update results in a dropped connection.
			*/
		FAutoUpdateRootGraph(FString&& InDebugAssetPath, bool bInLogWarningOnDroppedConnection)
			: DebugAssetPath(MoveTemp(InDebugAssetPath))
			, bLogWarningOnDroppedConnection(bInLogWarningOnDroppedConnection)
		{
		}

		bool Transform(FDocumentHandle InDocument) const override;

	private:
		const FString DebugAssetPath;
		bool bLogWarningOnDroppedConnection;
	};

	/** Sets the document's graph class, optionally updating the namespace and variant. */
	class METASOUNDFRONTEND_API FRenameRootGraphClass : public IDocumentTransform
	{
		const FMetasoundFrontendClassName NewClassName;

	public:
		/* Generates and assigns the document's root graph class a unique name using the provided guid as the name field and
		 * (optionally) namespace and variant.
		 * Returns true if transform succeeded, false if not.
		*/
		static bool Generate(FDocumentHandle InDocument, const FGuid& InGuid, const FName Namespace = { }, const FName Variant = { })
		{
			const FMetasoundFrontendClassName GeneratedClassName = { Namespace, *InGuid.ToString(), Variant };
			return FRenameRootGraphClass(GeneratedClassName).Transform(InDocument);
		}

		static bool Generate(FMetasoundFrontendDocument& InDocument, const FGuid& InGuid, const FName Namespace = { }, const FName Variant = { })
		{
			const FMetasoundFrontendClassName GeneratedClassName = { Namespace, *InGuid.ToString(), Variant };
			return FRenameRootGraphClass(GeneratedClassName).Transform(InDocument);
		}

		FRenameRootGraphClass(const FMetasoundFrontendClassName InClassName)
			: NewClassName(InClassName)
		{
		}

		bool Transform(FDocumentHandle InDocument) const override;
		bool Transform(FMetasoundFrontendDocument& InOutDocument) const override;
	};
} // Metasound::Frontend
