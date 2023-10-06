// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Map.h"
#include "Interfaces/MetasoundFrontendInterfaceRegistry.h"
#include "MetasoundFrontendDocumentCacheInterface.h"
#include "MetasoundDocumentInterface.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendDocumentModifyDelegates.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundFrontendTransform.h"
#include "MetasoundVertex.h"
#include "Templates/Function.h"
#include "UObject/ScriptInterface.h"

#include "MetasoundFrontendDocumentBuilder.generated.h"


// Forward Declarations
class FMetasoundAssetBase;


namespace Metasound::Frontend
{
	using FFinalizeNodeFunctionRef = TFunctionRef<void(FMetasoundFrontendNode&, const Metasound::Frontend::FNodeRegistryKey&)>;

	enum class EInvalidEdgeReason : uint8
	{
		None = 0,
		MismatchedAccessType,
		MismatchedDataType,
		MissingInput,
		MissingOutput,
		COUNT
	};

	METASOUNDFRONTEND_API FString LexToString(const EInvalidEdgeReason& InReason);

	struct METASOUNDFRONTEND_API FNamedEdge
	{
		const FGuid OutputNodeID;
		const FName OutputName;
		const FGuid InputNodeID;
		const FName InputName;

		friend bool operator==(const FNamedEdge& InLHS, const FNamedEdge& InRHS)
		{
			return InLHS.OutputNodeID == InRHS.OutputNodeID
				&& InLHS.OutputName == InRHS.OutputName
				&& InLHS.InputNodeID == InRHS.InputNodeID
				&& InLHS.InputName == InRHS.InputName;
		}

		friend bool operator!=(const FNamedEdge& InLHS, const FNamedEdge& InRHS)
		{
			return !(InLHS == InRHS);
		}

		friend FORCEINLINE uint32 GetTypeHash(const FNamedEdge& InBinding)
		{
			const int32 NameHash = HashCombineFast(GetTypeHash(InBinding.OutputName), GetTypeHash(InBinding.InputName));
			const int32 GuidHash = HashCombineFast(GetTypeHash(InBinding.OutputNodeID), GetTypeHash(InBinding.InputNodeID));
			return HashCombineFast(NameHash, GuidHash);
		}
	};

	struct METASOUNDFRONTEND_API FModifyInterfaceOptions
	{
		FModifyInterfaceOptions(const TArray<FMetasoundFrontendInterface>& InInterfacesToRemove, const TArray<FMetasoundFrontendInterface>& InInterfacesToAdd);
		FModifyInterfaceOptions(TArray<FMetasoundFrontendInterface>&& InInterfacesToRemove, TArray<FMetasoundFrontendInterface>&& InInterfacesToAdd);
		FModifyInterfaceOptions(const TArray<FMetasoundFrontendVersion>& InInterfaceVersionsToRemove, const TArray<FMetasoundFrontendVersion>& InInterfaceVersionsToAdd);

		TArray<FMetasoundFrontendInterface> InterfacesToRemove;
		TArray<FMetasoundFrontendInterface> InterfacesToAdd;

		// Function used to determine if an old of a removed interface
		// and new member of an added interface are considered equal and
		// to be swapped, retaining preexisting connections (and locations
		// if in editor and 'SetDefaultNodeLocations' option is set)
		TFunction<bool(FName, FName)> NamePairingFunction;

#if WITH_EDITORONLY_DATA
		bool bSetDefaultNodeLocations = true;
#endif // WITH_EDITORONLY_DATA
	};
} // namespace Metasound::Frontend


UCLASS()
class METASOUNDFRONTEND_API UMetaSoundBuilderDocument : public UObject, public IMetaSoundDocumentInterface
{
	GENERATED_BODY()

public:
	// Returns the document
	virtual const FMetasoundFrontendDocument& GetDocument() const override
	{
		return Document;
	}

	// Base MetaSoundClass that document is published to.
	virtual const UClass& GetBaseMetaSoundUClass() const final override
	{
		checkf(MetaSoundUClass, TEXT("BaseMetaSoundUClass must be set upon creation of UMetaSoundBuilderDocument instance"));
		return *MetaSoundUClass;
	}

private:
	// Sets the base MetaSound UClass (to be called immediately following UObject construction)
	void SetBaseMetaSoundUClass(const UClass& InMetaSoundClass)
	{
		MetaSoundUClass = &InMetaSoundClass;
	}

	virtual FMetasoundFrontendDocument& GetDocument() override
	{
		return Document;
	}

	UPROPERTY(Transient)
	FMetasoundFrontendDocument Document;

	UPROPERTY(Transient)
	TObjectPtr<const UClass> MetaSoundUClass = nullptr;

	friend struct FMetaSoundFrontendDocumentBuilder;
	friend class UMetaSoundBuilderBase;
};

// Builder used to support dynamically generating MetaSound documents at runtime. Builder contains caches that speed up
// common search and modification operations on a given document, which may result in slower performance on construction,
// but faster manipulation of its managed document.  The builder's managed copy of a document is expected to not be modified
// by any external system to avoid cache becoming stale.
USTRUCT()
struct METASOUNDFRONTEND_API FMetaSoundFrontendDocumentBuilder
{
	GENERATED_BODY()

public:
	// Exists only to make UObject reflection happy.  Should never be
	// used directly as builder interface (and optionally delegates) should be specified on construction.
	FMetaSoundFrontendDocumentBuilder();
	FMetaSoundFrontendDocumentBuilder(TScriptInterface<IMetaSoundDocumentInterface> InDocumentInterface);
	FMetaSoundFrontendDocumentBuilder(TScriptInterface<IMetaSoundDocumentInterface> InDocumentInterface, TSharedRef<Metasound::Frontend::FDocumentModifyDelegates> InDocumentDelegates);

	const FMetasoundFrontendClass* AddDependency(const FMetasoundFrontendClass& InClass);
	void AddEdge(FMetasoundFrontendEdge&& InNewEdge);
	bool AddNamedEdges(const TSet<Metasound::Frontend::FNamedEdge>& ConnectionsToMake, TArray<const FMetasoundFrontendEdge*>* OutEdgesCreated = nullptr, bool bReplaceExistingConnections = true);
	bool AddEdgesByNodeClassInterfaceBindings(const FGuid& InFromNodeID, const FGuid& InToNodeID, bool bReplaceExistingConnections = true);
	bool AddEdgesFromMatchingInterfaceNodeOutputsToGraphOutputs(const FGuid& InNodeID, TArray<const FMetasoundFrontendEdge*>& OutEdgesCreated, bool bReplaceExistingConnections = true);
	bool AddEdgesFromMatchingInterfaceNodeInputsToGraphInputs(const FGuid& InNodeID, TArray<const FMetasoundFrontendEdge*>& OutEdgesCreated, bool bReplaceExistingConnections = true);
	const FMetasoundFrontendNode* AddGraphInput(const FMetasoundFrontendClassInput& InClassInput);
	const FMetasoundFrontendNode* AddGraphOutput(const FMetasoundFrontendClassOutput& InClassOutput);
	bool AddInterface(FName InterfaceName);

	const FMetasoundFrontendNode* AddGraphNode(const FMetasoundFrontendGraphClass& InClass, FGuid InNodeID = FGuid::NewGuid());
	const FMetasoundFrontendNode* AddNodeByClassName(const FMetasoundFrontendClassName& InClassName, int32 InMajorVersion, FGuid InNodeID = FGuid::NewGuid());

	// Returns whether or not the given edge can be added, which requires that its input
	// is not already connected and the edge is valid (see function 'IsValidEdge').
	bool CanAddEdge(const FMetasoundFrontendEdge& InEdge) const;

	void ClearGraph();
	bool ContainsEdge(const FMetasoundFrontendEdge& InEdge) const;
	bool ContainsNode(const FGuid& InNodeID) const;

	bool ConvertFromPreset();
	bool ConvertToPreset(const FMetasoundFrontendDocument& InReferencedDocument);

	static bool FindDeclaredInterfaces(const FMetasoundFrontendDocument& InDocument, TArray<const Metasound::Frontend::IInterfaceRegistryEntry*>& OutInterfaces);
	bool FindDeclaredInterfaces(TArray<const Metasound::Frontend::IInterfaceRegistryEntry*>& OutInterfaces) const;

	const FMetasoundFrontendClass* FindDependency(const FGuid& InClassID) const;
	const FMetasoundFrontendClass* FindDependency(const FMetasoundFrontendClassMetadata& InMetadata) const;

	bool FindInterfaceInputNodes(FName InterfaceName, TArray<const FMetasoundFrontendNode*>& OutInputs) const;
	bool FindInterfaceOutputNodes(FName InterfaceName, TArray<const FMetasoundFrontendNode*>& OutOutputs) const;

	const FMetasoundFrontendClassInput* FindGraphInput(FName InputName) const;
	const FMetasoundFrontendNode* FindGraphInputNode(FName InputName) const;
	const FMetasoundFrontendClassOutput* FindGraphOutput(FName OutputName) const;
	const FMetasoundFrontendNode* FindGraphOutputNode(FName OutputName) const;

	const FMetasoundFrontendNode* FindNode(const FGuid& InNodeID) const;

	const FMetasoundFrontendVertex* FindNodeInput(const FGuid& InNodeID, const FGuid& InVertexID) const;
	const FMetasoundFrontendVertex* FindNodeInput(const FGuid& InNodeID, FName InVertexName) const;
	TArray<const FMetasoundFrontendVertex*> FindNodeInputs(const FGuid& InNodeID, FName TypeName = FName()) const;
	TArray<const FMetasoundFrontendVertex*> FindNodeInputsConnectedToNodeOutput(const FGuid& InOutputNodeID, const FGuid& InOutputVertexID, TArray<const FMetasoundFrontendNode*>* ConnectedInputNodes = nullptr) const;

	const FMetasoundFrontendVertex* FindNodeOutput(const FGuid& InNodeID, const FGuid& InVertexID) const;
	const FMetasoundFrontendVertex* FindNodeOutput(const FGuid& InNodeID, FName InVertexName) const;
	TArray<const FMetasoundFrontendVertex*> FindNodeOutputs(const FGuid& InNodeID, FName TypeName = FName()) const;
	const FMetasoundFrontendVertex* FindNodeOutputConnectedToNodeInput(const FGuid& InInputNodeID, const FGuid& InInputVertexID, const FMetasoundFrontendNode** ConnectedOutputNode = nullptr) const;

	const FMetasoundFrontendDocument& GetDocument() const;
	const Metasound::Frontend::FDocumentModifyDelegates& GetDocumentDelegates() const;
	const IMetaSoundDocumentInterface& GetDocumentInterface() const;
	const FMetasoundFrontendLiteral* GetNodeInputClassDefault(const FGuid& InNodeID, const FGuid& InVertexID) const;
	const FMetasoundFrontendLiteral* GetNodeInputDefault(const FGuid& InNodeID, const FGuid& InVertexID) const;

	// Initializes GraphClass Metadata, optionally resetting the version back to 1.0 and/or creating a unique class name if a name is not provided.
	static void InitGraphClassMetadata(FMetasoundFrontendClassMetadata& InOutMetadata, bool bResetVersion = false, const FMetasoundFrontendClassName* NewClassName = nullptr);
	void InitDocument();
	void InitNodeLocations();

	bool IsDependencyReferenced(const FGuid& InClassID) const;
	bool IsNodeInputConnected(const FGuid& InNodeID, const FGuid& InVertexID) const;
	bool IsNodeOutputConnected(const FGuid& InNodeID, const FGuid& InVertexID) const;

	bool IsInterfaceDeclared(FName InInterfaceName) const;
	bool IsInterfaceDeclared(const FMetasoundFrontendVersion& InInterfaceVersion) const;
	bool IsPreset() const;

	// Returns whether or not the given edge is valid (i.e. represents an input and output that equate in data and access types) or malformed.
	// Note that this does not return whether or not the given edge exists, but rather if it could be legally applied to the given edge vertices.
	Metasound::Frontend::EInvalidEdgeReason IsValidEdge(const FMetasoundFrontendEdge& InEdge) const;

	bool ModifyInterfaces(Metasound::Frontend::FModifyInterfaceOptions&& InOptions);

	bool RemoveDependency(const FGuid& InClassID);
	bool RemoveDependency(EMetasoundFrontendClassType ClassType, const FMetasoundFrontendClassName& InClassName, const FMetasoundFrontendVersionNumber& InClassVersionNumber);
	bool RemoveEdge(const FMetasoundFrontendEdge& EdgeToRemove);
	bool RemoveEdgesByNodeClassInterfaceBindings(const FGuid& InOutputNodeID, const FGuid& InInputNodeID);
	bool RemoveEdgesFromNodeOutput(const FGuid& InNodeID, const FGuid& InVertexID);
	bool RemoveEdgeToNodeInput(const FGuid& InNodeID, const FGuid& InVertexID);
	bool RemoveGraphInput(FName InInputName);
	bool RemoveGraphOutput(FName InOutputName);
	bool RemoveInterface(FName InName);
	bool RemoveNamedEdges(const TSet<Metasound::Frontend::FNamedEdge>& InNamedEdgesToRemove, TArray<FMetasoundFrontendEdge>* OutRemovedEdges = nullptr);
	bool RemoveNode(const FGuid& InNodeID);
	bool RemoveNodeInputDefault(const FGuid& InNodeID, const FGuid& InVertexID);
	bool RemoveUnusedDependencies();
	bool RenameRootGraphClass(const FMetasoundFrontendClassName& InName);

#if WITH_EDITOR
	// Primarily used by editor transaction stack to avoid corruption when object is
	// changed outside of the builder API. Generally discouraged for direct use otherwise
	// as it can be expensive and builder API should manage internal cache state directly.
	void ReloadCache();

	void SetAuthor(const FString& InAuthor);

	// Sets the editor-only node location of a node with the given ID to the provided location.
	// Returns true if the node was found and the location was updated, false if not.
	bool SetNodeLocation(const FGuid& InNodeID, const FVector2D& InLocation);
#endif // WITH_EDITOR

	bool SetGraphInputDefault(FName InputName, const FMetasoundFrontendLiteral& InDefaultLiteral);
	bool SetNodeInputDefault(const FGuid& InNodeID, const FGuid& InVertexID, const FMetasoundFrontendLiteral& InLiteral);

	bool SwapGraphInput(const FMetasoundFrontendClassVertex& InExistingInputVertex, const FMetasoundFrontendClassVertex& NewInputVertex);
	bool SwapGraphOutput(const FMetasoundFrontendClassVertex& InExistingOutputVertex, const FMetasoundFrontendClassVertex& NewOutputVertex);
	bool UpdateDependencyClassNames(const TMap<FMetasoundFrontendClassName, FMetasoundFrontendClassName>& OldToNewReferencedClassNames);

private:
	using FFinalizeNodeFunctionRef = TFunctionRef<void(FMetasoundFrontendNode&, const Metasound::Frontend::FNodeRegistryKey&)>;

	const FTopLevelAssetPath GetBuilderClassPath() const;
	FMetasoundFrontendDocument& GetDocument();

	FMetasoundFrontendNode* AddNodeInternal(const FMetasoundFrontendClassMetadata& InClassMetadata, Metasound::Frontend::FFinalizeNodeFunctionRef FinalizeNode, FGuid InNodeID = FGuid::NewGuid());

	const TSet<FMetasoundFrontendVersion>* FindNodeClassInterfaces(const FGuid& InNodeID) const;
	const FMetasoundFrontendClassInput* FindNodeInputClassInput(const FGuid& InNodeID, const FGuid& InVertexID) const;
	const FMetasoundFrontendClassOutput* FindNodeOutputClassOutput(const FGuid& InNodeID, const FGuid& InVertexID) const;

	void ReloadCacheInternal();

	bool SetGraphInputInheritsDefault(FName InName, bool bInputInheritsDefault);

	UPROPERTY(Transient)
	TScriptInterface<IMetaSoundDocumentInterface> DocumentInterface;

	TSharedPtr<Metasound::Frontend::IDocumentCache> DocumentCache;

	// SharedRef to struct of delegates fired when mutating document.  These are shared to safely support copying
	// FrontendDocumentBuilders.  Copying is typically dissuaded as you can still have disparate builders that are
	// not operating on the same cache; using a shared builder registered within the MetaSoundBuilderSubsystem
	// in MetaSoundEngine is recommended.
	TSharedRef<Metasound::Frontend::FDocumentModifyDelegates> DocumentDelegates;
};
