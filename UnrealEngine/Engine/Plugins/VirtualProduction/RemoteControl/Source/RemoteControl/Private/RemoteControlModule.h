// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IRemoteControlModule.h"
#include "AssetRegistry/AssetData.h"
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Factories/IRemoteControlMaskingFactory.h"

class IRemoteControlInterceptionFeatureProcessor;

/**
 * Implementation of the RemoteControl interface
 */
PRAGMA_DISABLE_DEPRECATION_WARNINGS

class FRemoteControlModule : public IRemoteControlModule
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

	//~ Begin IRemoteControlModule
	virtual FOnPresetRegistered& OnPresetRegistered() override;
	virtual FOnPresetUnregistered& OnPresetUnregistered() override;
	virtual bool RegisterPreset(FName Name, URemoteControlPreset* Preset) override;
	virtual void UnregisterPreset(FName Name) override;
	virtual bool RegisterEmbeddedPreset(URemoteControlPreset* Preset, bool bReplaceExisting) override;
	virtual void UnregisterEmbeddedPreset(FName Name) override;
	virtual void UnregisterEmbeddedPreset(URemoteControlPreset* Preset) override;
	virtual void PerformMasking(const TSharedRef<FRCMaskingOperation>& InMaskingOperation) override;
	virtual void RegisterMaskingFactoryForType(UScriptStruct* RemoteControlPropertyType, const TSharedPtr<IRemoteControlMaskingFactory>& InMaskingFactory) override;
	virtual void UnregisterMaskingFactoryForType(UScriptStruct* RemoteControlPropertyType) override;
	virtual bool SupportsMasking(const FProperty* InProperty) const override;
	virtual bool ResolveCall(const FString& ObjectPath, const FString& FunctionName, FRCCallReference& OutCallRef, FString* OutErrorText) override;
	virtual bool InvokeCall(FRCCall& InCall, ERCPayloadType InPayloadType = ERCPayloadType::Json, const TArray<uint8>& InInterceptPayload = TArray<uint8>()) override;
	virtual bool ResolveObject(ERCAccess AccessType, const FString& ObjectPath, const FString& PropertyName, FRCObjectReference& OutObjectRef, FString* OutErrorText = nullptr) override;
	virtual bool ResolveObjectProperty(ERCAccess AccessType, UObject* Object, FRCFieldPathInfo PropertyPath, FRCObjectReference& OutObjectRef, FString* OutErrorText = nullptr) override;
	virtual bool GetObjectProperties(const FRCObjectReference& ObjectAccess, IStructSerializerBackend& Backend) override;
	virtual bool SetObjectProperties(const FRCObjectReference& ObjectAccess, IStructDeserializerBackend& Backend, ERCPayloadType InPayloadType, const TArray<uint8>& InPayload, ERCModifyOperation Operation) override;
	virtual bool ResetObjectProperties(const FRCObjectReference& ObjectAccess, const bool bAllowIntercept) override;
	virtual bool SetPresetController(const FName PresetName, const FName ControllerName, IStructDeserializerBackend& Backend, const TArray<uint8>& InPayload, const bool bAllowIntercept) override;
	virtual bool SetPresetController(const FName PresetName, class URCVirtualPropertyBase* VirtualProperty, IStructDeserializerBackend& Backend, const TArray<uint8>& InPayload, const bool bAllowIntercept) override;
	virtual TOptional<FExposedFunction> ResolvePresetFunction(const FResolvePresetFieldArgs& Args) const override;
	virtual TOptional<FExposedProperty> ResolvePresetProperty(const FResolvePresetFieldArgs& Args) const override;
	virtual URemoteControlPreset* ResolvePreset(FName PresetName) const override;
	virtual URemoteControlPreset* ResolvePreset(const FGuid& PresetId) const override;
	virtual URemoteControlPreset* CreateTransientPreset() override;
	virtual bool DestroyTransientPreset(FName PresetName) override;
	virtual bool DestroyTransientPreset(const FGuid& PresetId) override;
	virtual bool IsPresetTransient(FName PresetName) const override;
	virtual bool IsPresetTransient(const FGuid& PresetId) const override;
	virtual void GetPresets(TArray<TSoftObjectPtr<URemoteControlPreset>>& OutPresets) const override;
	virtual void GetPresetAssets(TArray<FAssetData>& OutPresetAssets, bool bIncludeTransient = true) const override;
	virtual void GetEmbeddedPresets(TArray<TWeakObjectPtr<URemoteControlPreset>>& OutEmbeddedPresets) const override;
	virtual const TMap<FName, FEntityMetadataInitializer>& GetDefaultMetadataInitializers() const override;
	virtual bool RegisterDefaultEntityMetadata(FName MetadataKey, FEntityMetadataInitializer MetadataInitializer) override;
	virtual void UnregisterDefaultEntityMetadata(FName MetadataKey) override;
	virtual bool PropertySupportsRawModificationWithoutEditor(FProperty* Property, UClass* OwnerClass = nullptr) const override;
	virtual void RegisterEntityFactory( const FName InFactoryName, const TSharedRef<IRemoteControlPropertyFactory>& InFactory) override;
	virtual void UnregisterEntityFactory( const FName InFactoryName ) override;
	virtual FGuid BeginManualEditorTransaction(const FText& InDescription, uint32 TypeHash) override;
	virtual int32 EndManualEditorTransaction(const FGuid& TransactionId) override;
	virtual const TMap<FName, TSharedPtr<IRemoteControlPropertyFactory>>& GetEntityFactories() const override { return EntityFactories; };
	//~ End IRemoteControlModule

private:
	/** Refreshes Editor related visuals like location Gizmo for relevant properties (like Location of a SceneComponent) */
	void RefreshEditorPostSetObjectProperties(const FRCObjectReference& ObjectAccess);

	/** Cache all presets in the project for the ResolvePreset function. */
	void CachePresets() const;
	
	//~ Asset registry callbacks
	void OnAssetAdded(const FAssetData& AssetData);
	void OnAssetRemoved(const FAssetData& AssetData);
	void OnAssetRenamed(const FAssetData& AssetData, const FString&);
	void OnEmbeddedPresetRenamed(URemoteControlPreset* Preset);

	/** Destroy a transient preset using an object reference. */
	bool DestroyTransientPreset(URemoteControlPreset* Preset);

	/** Determines if a property modification should use a setter or default to deserializing directly onto an object. */
	static bool PropertyModificationShouldUseSetter(UObject* Object, FProperty* Property);

	/**
	 * Deserialize data for a non-EQUAL modification request and apply the operation to the resulting data.
	 * 
	 * @param ObjectAccess Data about the object/property for which modification was requested.
	 * @param Backend Deserialization backend for the modification request.
	 * @param Operation Type of operation to apply to the value.
	 * @param OutData Buffer used to deserialize data from the backend and contain the result value.
	 * @return True if the data was successfully deserialized and modified.
	 */
	static bool DeserializeDeltaModificationData(const FRCObjectReference& ObjectAccess, IStructDeserializerBackend& Backend, ERCModifyOperation Operation, TArray<uint8>& OutData);

	/**
	 * Register(s) masking factories of supported types.
	 */
	void RegisterMaskingFactories();

#if WITH_EDITOR
	/**
	 * End the ongoing modification if one exists and is a mismatch for the new object to edit.
	 * @param TypeHash The type hash of the object we want to modify after this check.
	 */
	void EndOngoingModificationIfMismatched(uint32 TypeHash);

	/** Finalize an ongoing change, triggering post edit change on the tracked object. */
	void TestOrFinalizeOngoingChange(bool bForceEndChange = false);

	// End ongoing change on map preload.
	void HandleMapPreLoad(const FString& MapName);
	
	/** Callback to handle registering delegates once the engine has finished its startup. */
	void HandleEnginePostInit();

	//~ Register/Unregister editor delegates.
	void RegisterEditorDelegates();
	void UnregisterEditorDelegates();

#endif // WITH_EDITOR

private:
	/** Cache of preset names to preset assets */
	mutable TMap<FName, TArray<FAssetData>> CachedPresetsByName;

	/** Cache of ids to preset names. */
	mutable TMap<FGuid, FName> CachedPresetNamesById;

	/**
	 * Listed of hosted (non-asset based) presets. 
	 * We can assume that all hosted presets are "active," so the list should be relatively short.
	 **/
	TMap<FName, TWeakObjectPtr<URemoteControlPreset>> EmbeddedPresets;

	/** Temporary presets that aren't saved as assets or directly visible to the editor's user. */
	TSet<FAssetData> TransientPresets;

	/** Index of the next created transient preset, to avoid naming collisions. */
	uint32 NextTransientPresetIndex = 0;

	/** Map of registered default metadata initializers. */
	TMap<FName, FEntityMetadataInitializer> DefaultMetadataInitializers;

	/** Delegate for preset registration */
	FOnPresetRegistered OnPresetRegisteredDelegate;

	/** Delegate for preset unregistration */
	FOnPresetUnregistered OnPresetUnregisteredDelegate;

	/** RC Processor feature instance */
	TUniquePtr<IRemoteControlInterceptionFeatureProcessor> RCIProcessor;

#if WITH_EDITOR
	/** Flags for a given RC change. */
	struct FOngoingChange
	{
		FOngoingChange(FRCObjectReference InReference);
		FOngoingChange(FRCCallReference InReference);
		
		friend uint32 GetTypeHash(const FOngoingChange& Modification)
		{
			if (Modification.Reference.IsType<FRCObjectReference>())
			{
				return GetTypeHash(Modification.Reference.Get<FRCObjectReference>());
			}
			else
			{
				return GetTypeHash(Modification.Reference.Get<FRCCallReference>());
			}
		}

		/** Reference to either the property we're modifying or the function we're calling. */
		TVariant<FRCObjectReference, FRCCallReference> Reference;
		/** Whether this change was triggered with a transaction or not. */
		bool bHasStartedTransaction = false;
		/** Whether this change was triggered since the last tick of OngoingChangeTimer. */
		bool bWasTriggeredSinceLastPass = true;
	};
	
	/** Ongoing change that needs to end its transaction and call PostEditChange in the case of a property modification. */
	TOptional<FOngoingChange> OngoingModification;

	/** Handle to the timer that that ends the ongoing change in regards to PostEditChange and transactions. */
	FTimerHandle OngoingChangeTimer;

	/** Delay before we check if a modification is no longer ongoing. */
	static constexpr float SecondsBetweenOngoingChangeCheck = 0.2f;

#endif // WITH_EDITOR

	/** Map of the factories which is responsible for the Remote Control property creation */
	TMap<FName, TSharedPtr<IRemoteControlPropertyFactory>> EntityFactories;

	/** Map of the factories which is responsible for the Remote Control property masking. */
	TMap<TWeakObjectPtr<UScriptStruct>, TSharedPtr<IRemoteControlMaskingFactory>> MaskingFactories;

	/** Holds the set of active masking operations. */
	TSet<TSharedPtr<FRCMaskingOperation>> ActiveMaskingOperations;
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS

IMPLEMENT_MODULE(FRemoteControlModule, RemoteControl);
