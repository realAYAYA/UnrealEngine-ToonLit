// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MetasoundAccessPtr.h"
#include "MetasoundAssetManager.h"
#include "MetasoundFrontend.h"
#include "MetasoundFrontendController.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendDocumentAccessPtr.h"
#include "MetasoundFrontendRegistryKey.h"
#include "MetasoundGraph.h"
#include "MetasoundLog.h"
#include "MetasoundParameterTransmitter.h"
#include "MetasoundVertex.h"
#include "Templates/SharedPointer.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/WeakObjectPtrTemplates.h"


// Forward Declarations
class UEdGraph;
struct FMetaSoundFrontendDocumentBuilder;
class IConsoleVariable;
typedef TMulticastDelegate<void(IConsoleVariable*), FDefaultDelegateUserPolicy> FConsoleVariableMulticastDelegate;

namespace Metasound
{
	class FGraph;
	namespace Frontend
	{
		// Forward Declarations
		class IInterfaceRegistryEntry;

		METASOUNDFRONTEND_API TRange<float> GetBlockRateClampRange();
		METASOUNDFRONTEND_API float GetBlockRateOverride();
		METASOUNDFRONTEND_API FConsoleVariableMulticastDelegate& GetBlockRateOverrideChangedDelegate();

		METASOUNDFRONTEND_API TRange<int32> GetSampleRateClampRange();
		METASOUNDFRONTEND_API int32 GetSampleRateOverride();
		METASOUNDFRONTEND_API FConsoleVariableMulticastDelegate& GetSampleRateOverrideChangedDelegate();
		class FProxyDataCache;

	} // namespace Frontend
} // namespace Metasound


/** FMetasoundAssetBase is intended to be a mix-in subclass for UObjects which utilize
 * Metasound assets.  It provides consistent access to FMetasoundFrontendDocuments, control
 * over the FMetasoundFrontendClassInterface of the FMetasoundFrontendDocument.  It also enables the UObject
 * to be utilized by a host of other engine tools built to support MetaSounds.
 */
class METASOUNDFRONTEND_API FMetasoundAssetBase
{
public:
	static const FString FileExtension;

	FMetasoundAssetBase() = default;
	virtual ~FMetasoundAssetBase() = default;

#if WITH_EDITORONLY_DATA
	virtual FText GetDisplayName() const = 0;

	// Returns the graph associated with this Metasound. Graph is required to be referenced on
	// Metasound UObject for editor serialization purposes.
	// @return Editor graph associated with this metasound uobject.
	virtual UEdGraph* GetGraph() = 0;
	virtual const UEdGraph* GetGraph() const = 0;
	virtual UEdGraph& GetGraphChecked() = 0;
	virtual const UEdGraph& GetGraphChecked() const = 0;

	// Sets the graph associated with this Metasound. Graph is required to be referenced on
	// Metasound UObject for editor serialization purposes.
	// @param Editor graph associated with this metasound object.
	virtual void SetGraph(UEdGraph* InGraph) = 0;

	// Only required for editor builds. Adds metadata to properties available when the object is
	// not loaded for use by the Asset Registry.
	virtual void SetRegistryAssetClassInfo(const Metasound::Frontend::FNodeClassInfo& InClassInfo) = 0;
#endif // WITH_EDITORONLY_DATA

	// Called when the interface is changed, presenting the opportunity for
	// any reflected object data to be updated based on the new interface.
	// Returns whether or not any edits were made.
	virtual bool ConformObjectDataToInterfaces() = 0;

	// Registers the root graph of the given asset with the MetaSound Frontend.
	virtual void RegisterGraphWithFrontend(Metasound::Frontend::FMetaSoundAssetRegistrationOptions InRegistrationOptions = Metasound::Frontend::FMetaSoundAssetRegistrationOptions());

	// Unregisters the root graph of the given asset with the MetaSound Frontend.
	void UnregisterGraphWithFrontend();

	// Cooks this MetaSound and recursively checks and cooks referenced MetaSounds if necessary. Cook includes autoupdating and resolving the document, which is then registered with the MetaSound Frontend.
	void CookMetaSound();

	// Sets/overwrites the root class metadata
	UE_DEPRECATED(5.3, "Directly setting graph class Metadata is no longer be supported. Use the FMetaSoundFrontendDocumentBuilder to modify class data.")
	virtual void SetMetadata(FMetasoundFrontendClassMetadata& InMetadata);

#if WITH_EDITOR
	// Rebuild dependent asset classes
	void RebuildReferencedAssetClasses();
#endif // WITH_EDITOR

	// Returns the interface entries declared by the given asset's document from the InterfaceRegistry.
	UE_DEPRECATED(5.3, "Use static FMetaSoundFrontendDocumentBuilder 'FindDeclaredInterfaces instead.")
	bool GetDeclaredInterfaces(TArray<const Metasound::Frontend::IInterfaceRegistryEntry*>& OutInterfaces) const;

	// Returns whether an interface with the given version is declared by the given asset's document.
	bool IsInterfaceDeclared(const FMetasoundFrontendVersion& InVersion) const;

	// Gets the asset class info.
	UE_DEPRECATED(5.4, "NodeClassInfo can be constructed directly from document's root graph & asset's path and requires no specialized virtual getter.")
	virtual Metasound::Frontend::FNodeClassInfo GetAssetClassInfo() const;

	// Returns all the class keys of this asset's referenced assets
	virtual const TSet<FString>& GetReferencedAssetClassKeys() const = 0;

	// Returns set of class references set call to serialize in the editor
	// Used at runtime load register referenced classes.
	virtual TArray<FMetasoundAssetBase*> GetReferencedAssets() = 0;

	// Return all dependent asset paths to load asynchronously
	virtual const TSet<FSoftObjectPath>& GetAsyncReferencedAssetClassPaths() const = 0;

	// Called when async assets have finished loading.
	virtual void OnAsyncReferencedAssetsLoaded(const TArray<FMetasoundAssetBase*>& InAsyncReferences) = 0;


	bool AddingReferenceCausesLoop(const FSoftObjectPath& InReferencePath) const;
	bool IsReferencedAsset(const FMetasoundAssetBase& InAssetToCheck) const;

	UE_DEPRECATED(5.3, "ConvertFromPreset moved to FMetaSoundFrontendDocumentBuilder.")
	void ConvertFromPreset();

	bool IsRegistered() const;

	// Imports data from a JSON string directly
	bool ImportFromJSON(const FString& InJSON);

	// Imports the asset from a JSON file at provided path
	bool ImportFromJSONAsset(const FString& InAbsolutePath);

	// Returns handle for the root metasound graph of this asset.
	Metasound::Frontend::FDocumentHandle GetDocumentHandle();
	Metasound::Frontend::FConstDocumentHandle GetDocumentHandle() const;

	// Returns handle for the root metasound graph of this asset.
	Metasound::Frontend::FGraphHandle GetRootGraphHandle();
	Metasound::Frontend::FConstGraphHandle GetRootGraphHandle() const;

	// Overwrites the existing document. If the document's interface is not supported,
	// the FMetasoundAssetBase be while queried for a new one using `GetPreferredInterface`.
	void SetDocument(const FMetasoundFrontendDocument& InDocument, bool bMarkDirty = true);
	void SetDocument(FMetasoundFrontendDocument&& InDocument, bool bMarkDirty = true);

	FMetasoundFrontendDocument& GetDocumentChecked();
	const FMetasoundFrontendDocument& GetDocumentChecked() const;

	const Metasound::Frontend::FGraphRegistryKey& GetGraphRegistryKey() const;

	UE_DEPRECATED(5.4, "Use GetGraphRegistryKey instead.")
	const Metasound::Frontend::FNodeRegistryKey& GetRegistryKey() const;

	UE_DEPRECATED(5.3, "AddDefaultInterfaces is included in now applied via FMetaSoundFrontendDocumentBuilder::InitDocument and no longer directly supported via this function.")
	void AddDefaultInterfaces();

	bool VersionAsset();

#if WITH_EDITOR
	/*
	 * Caches transient metadata (class & vertex) found in the registry
	 * that is not necessary for serialization or core graph generation.
	 *
	 * @return - Whether class was found in the registry & data was cached successfully.
	 */
	void CacheRegistryMetadata();

	FMetasoundFrontendDocumentModifyContext& GetModifyContext();
	const FMetasoundFrontendDocumentModifyContext& GetModifyContext() const;
#endif // WITH_EDITOR

	// Calls the outermost package and marks it dirty.
	bool MarkMetasoundDocumentDirty() const;

	struct FSendInfoAndVertexName
	{
		Metasound::FMetaSoundParameterTransmitter::FSendInfo SendInfo;
		Metasound::FVertexName VertexName;
	};

	// Returns the owning asset responsible for transactions applied to MetaSound
	virtual UObject* GetOwningAsset() = 0;

	// Returns the owning asset responsible for transactions applied to MetaSound
	virtual const UObject* GetOwningAsset() const = 0;

	FString GetOwningAssetName() const;

protected:
	void OnNotifyBeginDestroy();

#if WITH_EDITOR
	virtual void SetReferencedAssetClasses(TSet<Metasound::Frontend::IMetaSoundAssetManager::FAssetInfo>&& InAssetClasses) = 0;
#endif

	// Get information for communicating asynchronously with MetaSound running instance.
	UE_DEPRECATED(5.3, "MetaSounds no longer communicate using FSendInfo.")
	TArray<FSendInfoAndVertexName> GetSendInfos(uint64 InInstanceID) const;

#if WITH_EDITORONLY_DATA
	FText GetDisplayName(FString&& InTypeName) const;
#endif // WITH_EDITORONLY_DATA

	// Returns an access pointer to the document.
	virtual Metasound::Frontend::FDocumentAccessPtr GetDocumentAccessPtr() = 0;

	// Returns an access pointer to the document.
	virtual Metasound::Frontend::FConstDocumentAccessPtr GetDocumentConstAccessPtr() const = 0;

	// Waits for a graph to be registered in the scenario where a graph is registered on an async task.
	//
	// When a graph is registered, the underlying IMetaSoundDocumentInterface may be accessed on an
	// async tasks. If modifications need to be made to the IMetaSoundDocumentInterface, callers should
	// wait for the inflight graph registration to complete by calling this method.

protected:
	
	// Container for runtime data of MetaSound graph.
	struct FRuntimeData_DEPRECATED
	{
		// Current ID of graph.
		FGuid ChangeID;

		// Array of inputs which can be set for construction. 
		TArray<FMetasoundFrontendClassInput> PublicInputs;

		// Array of inputs which can be transmitted to.
		TArray<FMetasoundFrontendClassInput> TransmittableInputs;

		// Core graph.
		TSharedPtr<Metasound::FGraph, ESPMode::ThreadSafe> Graph;
	};

	struct UE_DEPRECATED(5.4, "FRuntimeData is no longer used to store runtime graphs and inputs. Runtime graphs are stored in the node registry. Runtime inputs are stored on the UMetaSoundSoruce") FRuntimeData : FRuntimeData_DEPRECATED
	{
	};

	// Returns the cached runtime data.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.4, "Access to graph and public inputs has moved. Use the node registry to access the graph and GetPublicClassInputs() to access public inputs")
	const FRuntimeData& GetRuntimeData() const;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Returns all public class inputs.  This is a potentially expensive.
	// Prefer accessing public class inputs using CacheRuntimeData.
	TArray<FMetasoundFrontendClassInput> GetPublicClassInputs() const;

	bool AutoUpdate(bool bInLogWarningsOnDroppedConnection);
	void CookReferencedMetaSounds();

	// Ensures all referenced graph classes are registered (or re-registers depending on options).
	void RegisterAssetDependencies(const Metasound::Frontend::FMetaSoundAssetRegistrationOptions & InRegistrationOptions);

	UE_DEPRECATED(5.4, "Template node transformation moved to private implementation. A MetaSound asset will likely never have the function Process(...). Without a Process function, you cannot have preprocessing.")
	TSharedPtr<FMetasoundFrontendDocument> PreprocessDocument();

private:
#if WITH_EDITORONLY_DATA
	void UpdateAssetRegistry();
#endif

	// Returns true if the IMetaSoundDocumentInterface is currently has an active builder.
	//
	// If true, calls to register the graph will be performed synchronously in order to avoid
	// race conditions with an active builder.
	virtual bool IsBuilderActive() const = 0;

	Metasound::Frontend::FGraphRegistryKey GraphRegistryKey;
};
