// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MetasoundAccessPtr.h"
#include "MetasoundAssetManager.h"
#include "MetasoundFrontend.h"
#include "MetasoundFrontendController.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendDocumentAccessPtr.h"
#include "MetasoundGraph.h"
#include "MetasoundLog.h"
#include "MetasoundParameterTransmitter.h"
#include "MetasoundVertex.h"
#include "Templates/SharedPointer.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/WeakObjectPtrTemplates.h"

// Forward Declarations
class UEdGraph;

namespace Metasound
{
	namespace Frontend
	{
		// Forward Declarations
		class IInterfaceRegistryEntry;

		METASOUNDFRONTEND_API float GetDefaultBlockRate();
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
	void RegisterGraphWithFrontend(Metasound::Frontend::FMetaSoundAssetRegistrationOptions InRegistrationOptions = Metasound::Frontend::FMetaSoundAssetRegistrationOptions());

	// Unregisters the root graph of the given asset with the MetaSound Frontend.
	void UnregisterGraphWithFrontend();

	// Sets/overwrites the root class metadata
	virtual void SetMetadata(FMetasoundFrontendClassMetadata& InMetadata);

#if WITH_EDITOR
	// Rebuild dependent asset classes
	void RebuildReferencedAssetClasses();
#endif // WITH_EDITOR

	// Returns the interface entries declared by the given asset's document from the InterfaceRegistry.
	bool GetDeclaredInterfaces(TArray<const Metasound::Frontend::IInterfaceRegistryEntry*>& OutInterfaces) const;

	// Returns whether an interface with the given version is declared by the given asset's document.
	bool IsInterfaceDeclared(const FMetasoundFrontendVersion& InVersion) const;

	// Gets the asset class info.
	virtual Metasound::Frontend::FNodeClassInfo GetAssetClassInfo() const = 0;

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
	void SetDocument(const FMetasoundFrontendDocument& InDocument);

	FMetasoundFrontendDocument& GetDocumentChecked();
	const FMetasoundFrontendDocument& GetDocumentChecked() const;

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
#if WITH_EDITOR
	virtual void SetReferencedAssetClasses(TSet<Metasound::Frontend::IMetaSoundAssetManager::FAssetInfo>&& InAssetClasses) = 0;
#endif

	// Get information for communicating asynchronously with MetaSound running instance.
	TArray<FSendInfoAndVertexName> GetSendInfos(uint64 InInstanceID) const;

#if WITH_EDITORONLY_DATA
	FText GetDisplayName(FString&& InTypeName) const;
#endif // WITH_EDITORONLY_DATA

	// Returns an access pointer to the document.
	virtual Metasound::Frontend::FDocumentAccessPtr GetDocument() = 0;

	// Returns an access pointer to the document.
	virtual Metasound::Frontend::FConstDocumentAccessPtr GetDocument() const = 0;

protected:
	// Container for runtime data of MetaSound graph.
	struct FRuntimeData
	{
		// Current ID of graph.
		FGuid ChangeID;

		// Array of inputs which can be set for construction. 
		TArray<FMetasoundFrontendClassInput> PublicInputs;

		// Array of inputs which can be transmitted to.
		TArray<FMetasoundFrontendClassInput> TransmittableInputs;

		// Core graph.
		TSharedPtr<Metasound::IGraph, ESPMode::ThreadSafe> Graph;
	};

	// Returns the cached runtime data.
	const FRuntimeData& GetRuntimeData() const;

	// Returns all public class inputs.  This is a potentially expensive.
	// Prefer accessing public class inputs using CacheRuntimeData.
	TArray<FMetasoundFrontendClassInput> GetPublicClassInputs() const;


	bool AutoUpdate(bool bInLogWarningsOnDroppedConnection);
	void RegisterAssetDependencies(const Metasound::Frontend::FMetaSoundAssetRegistrationOptions& InRegistrationOptions);
	TSharedPtr<FMetasoundFrontendDocument> PreprocessDocument();
private:
#if WITH_EDITORONLY_DATA
	void UpdateAssetRegistry();
#endif
	// Returns the cached runtime data. Call updates cached data if out-of-date.
	const FRuntimeData& CacheRuntimeData(const FMetasoundFrontendDocument& InPreprocessedDoc);

	Metasound::Frontend::FNodeRegistryKey RegistryKey;

	// Cache ID is used to determine whether CachedRuntimeData is out-of-date.
	FGuid CurrentCachedRuntimeDataChangeID;
	FRuntimeData CachedRuntimeData;

	TSharedPtr<Metasound::IGraph, ESPMode::ThreadSafe> BuildMetasoundDocument(const FMetasoundFrontendDocument& InPreprocessDoc, const TSet<FName>& InTransmittableInputNames) const;
	Metasound::FSendAddress CreateSendAddress(uint64 InInstanceID, const Metasound::FVertexName& InVertexName, const FName& InDataTypeName) const;
	Metasound::Frontend::FNodeHandle AddInputPinForSendAddress(const Metasound::FMetaSoundParameterTransmitter::FSendInfo& InSendInfo, Metasound::Frontend::FGraphHandle InGraph) const;
};
