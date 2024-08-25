// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Set.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "DataLayerAction.h"
#include "Delegates/Delegate.h"
#include "EditorSubsystem.h"
#include "Engine/World.h"
#include "IActorEditorContextClient.h"
#include "Stats/Stats2.h"
#include "Templates/SharedPointer.h"
#include "Templates/UnrealTemplate.h"
#include "Tickable.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealNames.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "WorldPartition/DataLayer/ActorDataLayer.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"

#include "DataLayerEditorSubsystem.generated.h"

class AActor;
class AWorldDataLayers;
class FDataLayersBroadcast;
class FLevelEditorViewportClient;
class FSubsystemCollectionBase;
class SWidget;
class UDEPRECATED_DataLayer;
class UDataLayerAsset;
class UEditorEngine;
class ULevel;
class UObject;
class UWorld;
class UWorldPartition;
class UExternalDataLayerAsset;
struct FLevelEditorDragDropWorldSurrogateReferencingObject;
struct FFrame;
enum class EExternalDataLayerRegistrationState : uint8;
template<typename TItemType> class IFilter;

USTRUCT(BlueprintType)
struct DATALAYEREDITOR_API FDataLayerCreationParameters
{
	GENERATED_USTRUCT_BODY()

	FDataLayerCreationParameters();

	// Required. Will assign the asset to the created instance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Layer")
	TObjectPtr<UDataLayerAsset> DataLayerAsset;

	// Optional. Will default at the level WorldDataLayers if unset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Layer")
	TObjectPtr<AWorldDataLayers> WorldDataLayers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Layer")
	bool bIsPrivate;
};

UCLASS()
class DATALAYEREDITOR_API UDataLayerEditorSubsystem final : public UEditorSubsystem, public IActorEditorContextClient, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UDataLayerEditorSubsystem();

	static UDataLayerEditorSubsystem* Get();

	typedef IFilter<const TWeakObjectPtr<AActor>&> FActorFilter;

	/**
	 *	Prepares for use
	 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/**
	 *	Internal cleanup
	 */
	virtual void Deinitialize() override;

	/**
	 *	Destructor
	 */
	virtual ~UDataLayerEditorSubsystem() {}

	//~ Begin FTickableGameObject interface
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual bool IsTickableInEditor() const { return true; }
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsAllowedToTick() const override;
	virtual void Tick(float DeltaTime) override;
	TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UDataLayerEditorSubsystem, STATGROUP_Tickables); }
	//~ End FTickableGameObject interface

	//~ Begin UObject overrides.
	virtual void BeginDestroy() override;
	// ~End UObject overrides.

	//~ Begin IActorEditorContextClient interface
	virtual void OnExecuteActorEditorContextAction(UWorld* InWorld, const EActorEditorContextAction& InType, AActor* InActor = nullptr) override;
	virtual bool GetActorEditorContextDisplayInfo(UWorld* InWorld, FActorEditorContextClientDisplayInfo& OutDiplayInfo) const override;
	virtual bool CanResetContext(UWorld* InWorld) const override { return true; };
	virtual TSharedRef<SWidget> GetActorEditorContextWidget(UWorld* InWorld) const override;
	virtual FOnActorEditorContextClientChanged& GetOnActorEditorContextClientChanged() override { return ActorEditorContextClientChanged; }
	//~ End IActorEditorContextClient interface
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	void AddToActorEditorContext(UDataLayerInstance* InDataLayerInstance);
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	void RemoveFromActorEditorContext(UDataLayerInstance* InDataLayerInstance);
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	bool SetActorEditorContextCurrentExternalDataLayer(const UExternalDataLayerAsset* InExternalDataLayerAsset);
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	const UExternalDataLayerAsset* GetActorEditorContextCurrentExternalDataLayer() const;

	TArray<const UDataLayerInstance*> GetDataLayerInstances(const TArray<const UDataLayerAsset*> DataLayerAssets) const;

	template<class DataLayerType, typename ...Args>
	DataLayerType* CreateDataLayerInstance(AWorldDataLayers* WorldDataLayers, Args&&... InArgs);

	/* Broadcasts whenever one or more DataLayers are modified
	 *
	 * Actions
	 * Add    : The specified ChangedDataLayer is a newly created UDataLayerInstance
	 * Modify : The specified ChangedDataLayer was just modified, if ChangedDataLayer is invalid then multiple DataLayers were modified.
	 *          ChangedProperty specifies what field on the UDataLayerInstance was changed, if NAME_None then multiple fields were changed
	 * Delete : A DataLayer was deleted
	 * Rename : The specified ChangedDataLayer was just renamed
	 * Reset  : A large amount of changes have occurred to a number of DataLayers.
	 */
	DECLARE_EVENT_ThreeParams(UDataLayerEditorSubsystem, FOnDataLayerChanged, const EDataLayerAction /*Action*/, const TWeakObjectPtr<const UDataLayerInstance>& /*ChangedDataLayer*/, const FName& /*ChangedProperty*/);
	FOnDataLayerChanged& OnDataLayerChanged() { return DataLayerChanged; }

	/** Broadcasts whenever one or more Actors changed UDataLayerInstances*/
	DECLARE_EVENT_OneParam(UDataLayerEditorSubsystem, FOnActorDataLayersChanged, const TWeakObjectPtr<AActor>& /*ChangedActor*/);
	FOnActorDataLayersChanged& OnActorDataLayersChanged() { return ActorDataLayersChanged; }

	/** Broadcasts whenever one or more DataLayers editor loading state changed */
	DECLARE_EVENT_OneParam(UDataLayerEditorSubsystem, FOnActorDataLayersEditorLoadingStateChanged, bool /*bIsFromUserChange*/);
	FOnActorDataLayersEditorLoadingStateChanged& OnActorDataLayersEditorLoadingStateChanged() { return DataLayerEditorLoadingStateChanged; }

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Operations on an individual actor.

	/**
	 *	Checks to see if the specified actor is in an appropriate state to interact with DataLayers
	 *
	 *	@param	Actor	The actor to validate
	 */
	UE_DEPRECATED(5.4, "Use IsActorValidForDataLayerInstances instead.")
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	bool IsActorValidForDataLayer(AActor* Actor);

	/**
	 *	Checks to see if the specified actor is in an appropriate state to interact with DataLayers
	 *
	 *	@param	Actor				The actor to validate
	 *  @param  DataLayerInstances	The data layers used to do the validation
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	bool IsActorValidForDataLayerInstances(AActor* Actor, const TArray<UDataLayerInstance*>& DataLayerInstances);

	/**
	 * Adds the actor to the DataLayer.
	 *
	 * @param	Actor		The actor to add to the DataLayer
	 * @param	DataLayer	The DataLayer to add the actor to
	 * @return				true if the actor was added.  false is returned if the actor already belongs to the DataLayer.
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	bool AddActorToDataLayer(AActor* Actor, UDataLayerInstance* DataLayer);

	/**
	 * Adds the provided actor to the DataLayers.
	 *
	 * @param	Actor		The actor to add to the provided DataLayers
	 * @param	DataLayers	A valid list of DataLayers.
	 * @return				true if the actor was added to at least one of the provided DataLayers.
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	bool AddActorToDataLayers(AActor* Actor, const TArray<UDataLayerInstance*>& DataLayers);

	/**
	 * Removes an actor from the specified DataLayer.
	 *
	 * @param	Actor			The actor to remove from the provided DataLayer
	 * @param	DataLayerToRemove	The DataLayer to remove the actor from
	 * @return					true if the actor was removed from the DataLayer.  false is returned if the actor already belonged to the DataLayer.
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	bool RemoveActorFromDataLayer(AActor* Actor, UDataLayerInstance* DataLayerToRemove);

	/**
	 * Removes the provided actor from the DataLayers.
	 *
	 * @param	Actor		The actor to remove from the provided DataLayers
	 * @param	DataLayers	A valid list of DataLayers.
	 * @return				true if the actor was removed from at least one of the provided DataLayers.
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	bool RemoveActorFromDataLayers(AActor* Actor, const TArray<UDataLayerInstance*>& DataLayers);

	/**
	 * Removes an actor from all DataLayers.
	 *
	 * @param	Actor			The actor to modify
	 * @return					true if the actor was changed.
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	bool RemoveActorFromAllDataLayers(AActor* Actor);
	
	/**
	 * Removes an actor from all DataLayers.
	 *
	 * @param	Actor			The actors to modify
	 * @return					true if any actor was changed.
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	bool RemoveActorsFromAllDataLayers(const TArray<AActor*>& Actors);

	/////////////////////////////////////////////////
	// Operations on multiple actors.

	/**
	 * Add the actors to the DataLayer
	 *
	 * @param	Actors		The actors to add to the DataLayer
	 * @param	DataLayer	The DataLayer to add to
	 * @return				true if at least one actor was added to the DataLayer.  false is returned if all the actors already belonged to the DataLayer.
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	bool AddActorsToDataLayer(const TArray<AActor*>& Actors, UDataLayerInstance* DataLayer);

	/**
	 * Add the actors to the DataLayers
	 *
	 * @param	Actors		The actors to add to the DataLayers
	 * @param	DataLayers	A valid list of DataLayers.
	 * @return				true if at least one actor was added to at least one DataLayer.  false is returned if all the actors already belonged to all specified DataLayers.
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	bool AddActorsToDataLayers(const TArray<AActor*>& Actors, const TArray<UDataLayerInstance*>& DataLayers);

	/**
	 * Removes the actors from the specified DataLayer.
	 *
	 * @param	Actors			The actors to remove from the provided DataLayer
	 * @param	DataLayerToRemove	The DataLayer to remove the actors from
	 * @return					true if at least one actor was removed from the DataLayer.  false is returned if all the actors already belonged to the DataLayer.
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	bool RemoveActorsFromDataLayer(const TArray<AActor*>& Actors, UDataLayerInstance* DataLayer);

	/**
	 * Remove the actors from the DataLayers
	 *
	 * @param	Actors		The actors to remove to the DataLayers
	 * @param	DataLayers	A valid list of DataLayers.
	 * @return				true if at least one actor was removed from at least one DataLayer.  false is returned if none of the actors belonged to any of the specified DataLayers.
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	bool RemoveActorsFromDataLayers(const TArray<AActor*>& Actors, const TArray<UDataLayerInstance*>& DataLayers);

	/////////////////////////////////////////////////
	// Operations on selected actors.

	/**
	 * Adds selected actors to the DataLayer.
	 *
	 * @param	DataLayer	A DataLayer.
	 * @return				true if at least one actor was added.  false is returned if all selected actors already belong to the DataLayer.
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	bool AddSelectedActorsToDataLayer(UDataLayerInstance* DataLayer);

	/**
	 * Adds selected actors to the DataLayers.
	 *
	 * @param	DataLayers	A valid list of DataLayers.
	 * @return				true if at least one actor was added.  false is returned if all selected actors already belong to the DataLayers.
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	bool AddSelectedActorsToDataLayers(const TArray<UDataLayerInstance*>& DataLayers);

	/**
	 * Removes the selected actors from the DataLayer.
	 *
	 * @param	DataLayer	A DataLayer.
	 * @return				true if at least one actor was added.  false is returned if all selected actors already belong to the DataLayer.
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	bool RemoveSelectedActorsFromDataLayer(UDataLayerInstance* DataLayer);

	/**
	 * Removes selected actors from the DataLayers.
	 *
	 * @param	DataLayers	A valid list of DataLayers.
	 * @return				true if at least one actor was removed.
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	bool RemoveSelectedActorsFromDataLayers(const TArray<UDataLayerInstance*>& DataLayers);


	/////////////////////////////////////////////////
	// Operations on actors in DataLayers
	
	/**
	 * Selects/de-selects actors belonging to the DataLayer.
	 *
	 * @param	DataLayer						A valid DataLayer.
	 * @param	bSelect							If true actors are selected; if false, actors are deselected.
	 * @param	bNotify							If true the Editor is notified of the selection change; if false, the Editor will not be notified.
	 * @param	bSelectEvenIfHidden	[optional]	If true even hidden actors will be selected; if false, hidden actors won't be selected.
	 * @return									true if at least one actor was selected/deselected.
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	bool SelectActorsInDataLayer(UDataLayerInstance* DataLayer, const bool bSelect, const bool bNotify, const bool bSelectEvenIfHidden = false);

	/**
	 * Selects/de-selects actors belonging to the DataLayer.
	 *
	 * @param	DataLayer						A valid DataLayer.
	 * @param	bSelect							If true actors are selected; if false, actors are deselected.
	 * @param	bNotify							If true the Editor is notified of the selection change; if false, the Editor will not be notified.
	 * @param	bSelectEvenIfHidden	[optional]	If true even hidden actors will be selected; if false, hidden actors won't be selected.
	 * @param	Filter	[optional]				Actor that don't pass the specified filter restrictions won't be selected.
	 * @return									true if at least one actor was selected/deselected.
	 */
	bool SelectActorsInDataLayer(UDataLayerInstance* DataLayer, const bool bSelect, const bool bNotify, const bool bSelectEvenIfHidden, const TSharedPtr<FActorFilter>& Filter);

	/**
	 * Selects/de-selects actors belonging to the DataLayers.
	 *
	 * @param	DataLayers						A valid list of DataLayers.
	 * @param	bSelect							If true actors are selected; if false, actors are deselected.
	 * @param	bNotify							If true the Editor is notified of the selection change; if false, the Editor will not be notified
	 * @param	bSelectEvenIfHidden	[optional]	If true even hidden actors will be selected; if false, hidden actors won't be selected.
	 * @return									true if at least one actor was selected/deselected.
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	bool SelectActorsInDataLayers(const TArray<UDataLayerInstance*>& DataLayers, const bool bSelect, const bool bNotify, const bool bSelectEvenIfHidden = false);

	/**
	 * Selects/de-selects actors belonging to the DataLayers.
	 *
	 * @param	DataLayers						A valid list of DataLayers.
	 * @param	bSelect							If true actors are selected; if false, actors are deselected.
	 * @param	bNotify							If true the Editor is notified of the selection change; if false, the Editor will not be notified
	 * @param	bSelectEvenIfHidden	[optional]	If true even hidden actors will be selected; if false, hidden actors won't be selected.
	 * @param	Filter	[optional]				Actor that don't pass the specified filter restrictions won't be selected.
	 * @return									true if at least one actor was selected/deselected.
	 */
	bool SelectActorsInDataLayers(const TArray<UDataLayerInstance*>& DataLayers, const bool bSelect, const bool bNotify, const bool bSelectEvenIfHidden, const TSharedPtr<FActorFilter>& Filter);


	/////////////////////////////////////////////////
	// Operations on actor viewport visibility regarding DataLayers



	/**
	 * Updates the provided actors visibility in the viewports
	 *
	 * @param	Actor						Actor to update
	 * @param	bOutSelectionChanged [OUT]	Whether the Editors selection changed
	 * @param	bOutActorModified [OUT]		Whether the actor was modified
	 * @param	bNotifySelectionChange		If true the Editor is notified of the selection change; if false, the Editor will not be notified
	 * @param	bRedrawViewports			If true the viewports will be redrawn; if false, they will not
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	bool UpdateActorVisibility(AActor* Actor, bool& bOutSelectionChanged, bool& bOutActorModified, const bool bNotifySelectionChange, const bool bRedrawViewports);

	/**
	 * Updates the visibility of all actors in the viewports
	 *
	 * @param	bNotifySelectionChange		If true the Editor is notified of the selection change; if false, the Editor will not be notified
	 * @param	bRedrawViewports			If true the viewports will be redrawn; if false, they will not
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	bool UpdateAllActorsVisibility(const bool bNotifySelectionChange, const bool bRedrawViewports);


	/////////////////////////////////////////////////
	// Operations on DataLayers

	/**
	 *	Appends all the actors associated with the specified DataLayer.
	 *
	 *	@param	DataLayer			The DataLayer to find actors for.
	 *	@param	InOutActors			The list to append the found actors to.
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	void AppendActorsFromDataLayer(UDataLayerInstance* DataLayer, TArray<AActor*>& InOutActors) const;

	/**
	 *	Appends all the actors associated with the specified DataLayer.
	 *
	 *	@param	DataLayer			The DataLayer to find actors for.
	 *	@param	InOutActors			The list to append the found actors to.
	 *  @param	Filter	[optional]	Actor that don't pass the specified filter restrictions won't be selected.
	 */
	void AppendActorsFromDataLayer(UDataLayerInstance* DataLayer, TArray<AActor*>& InOutActors, const TSharedPtr<FActorFilter>& Filter) const;
	
	/**
	 *	Appends all the actors associated with ANY of the specified DataLayers.
	 *
	 *	@param	DataLayers			The DataLayers to find actors for.
	 *	@param	InOutActors			The list to append the found actors to.
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	void AppendActorsFromDataLayers(const TArray<UDataLayerInstance*>& DataLayers, TArray<AActor*>& InOutActors) const;

	/**
	 *	Appends all the actors associated with ANY of the specified DataLayers.
	 *
	 *	@param	DataLayers			The DataLayers to find actors for.
	 *	@param	InOutActors			The list to append the found actors to.
	 *  @param	Filter	[optional]	Actor that don't pass the specified filter restrictions won't be selected.
	 */
	void AppendActorsFromDataLayers(const TArray<UDataLayerInstance*>& DataLayers, TArray<AActor*>& InOutActors, const TSharedPtr<FActorFilter>& Filter) const;

	/**
	 *	Gets all the actors associated with the specified DataLayer. Analog to AppendActorsFromDataLayer but it returns rather than appends the actors.
	 *
	 *	@param	DataLayer			The DataLayer to find actors for.
	 *	@return						The list to assign the found actors to.
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	TArray<AActor*> GetActorsFromDataLayer(UDataLayerInstance* DataLayer) const;

	/**
	 *	Gets all the actors associated with the specified DataLayer. Analog to AppendActorsFromDataLayer but it returns rather than appends the actors.
	 *
	 *	@param	DataLayer			The DataLayer to find actors for.
	 *  @param	Filter	[optional]	Actor that don't pass the specified filter restrictions won't be selected.
	 *	@return						The list to assign the found actors to.
	 */
	TArray<AActor*> GetActorsFromDataLayer(UDataLayerInstance* DataLayer, const TSharedPtr<FActorFilter>& Filter) const;

	/**
	 *	Gets all the actors associated with ANY of the specified DataLayers. Analog to AppendActorsFromDataLayers but it returns rather than appends the actors.
	 *
	 *	@param	DataLayers			The DataLayers to find actors for.
	 *	@return						The list to assign the found actors to.
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	TArray<AActor*> GetActorsFromDataLayers(const TArray<UDataLayerInstance*>& DataLayers) const;

	/**
	 *	Gets all the actors associated with ANY of the specified DataLayers. Analog to AppendActorsFromDataLayers but it returns rather than appends the actors.
	 *
	 *	@param	DataLayers			The DataLayers to find actors for.
	 *  @param	Filter	[optional]	Actor that don't pass the specified filter restrictions won't be selected.
	 *	@return						The list to assign the found actors to.
	 */
	TArray<AActor*> GetActorsFromDataLayers(const TArray<UDataLayerInstance*>& DataLayers, const TSharedPtr<FActorFilter>& Filter) const;

	/**
	 * Changes the DataLayer's visibility to the provided state
	 *
	 * @param	DataLayer	The DataLayer to affect.
	 * @param	bIsVisible	If true the DataLayer will be visible; if false, the DataLayer will not be visible.
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	void SetDataLayerVisibility(UDataLayerInstance* DataLayer, const bool bIsVisible);

	/**
	 * Changes visibility of the DataLayers to the provided state
	 *
	 * @param	DataLayers	The DataLayers to affect
	 * @param	bIsVisible	If true the DataLayers will be visible; if false, the DataLayers will not be visible
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	void SetDataLayersVisibility(const TArray<UDataLayerInstance*>& DataLayers, const bool bIsVisible);

	/**
	 * Toggles the DataLayer's visibility
	 *
	 * @param DataLayer	The DataLayer to affect
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	void ToggleDataLayerVisibility(UDataLayerInstance* DataLayer);

	/**
	 * Toggles the visibility of all of the DataLayers
	 *
	 * @param	DataLayers	The DataLayers to affect
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	void ToggleDataLayersVisibility(const TArray<UDataLayerInstance*>& DataLayers);

	/**
	 * Changes the DataLayer's IsLoadedInEditor flag to the provided state
	 *
	 * @param	DataLayer						The DataLayer to affect.
	 * @param	bIsLoadedInEditor				The new value of the flag IsLoadedInEditor.
	 *											If True, the Editor loading will consider this DataLayer to load or not an Actor part of this DataLayer.
	 *											An Actor will not be loaded in the Editor if all its DataLayers are not LoadedInEditor.
	 * @param	bIsFromUserChange				If this change originates from a user change or not.
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	bool SetDataLayerIsLoadedInEditor(UDataLayerInstance* DataLayer, const bool bIsLoadedInEditor, const bool bIsFromUserChange);

	/**
	 * Changes the IsLoadedInEditor flag of the DataLayers to the provided state
	 *
	 * @param	DataLayers						The DataLayers to affect
	 * @param	bIsLoadedInEditor				The new value of the flag IsLoadedInEditor.
	 *											If True, the Editor loading will consider this DataLayer to load or not an Actor part of this DataLayer.
	 *											An Actor will not be loaded in the Editor if all its DataLayers are not LoadedInEditor.
	 * @param	bIsFromUserChange				If this change originates from a user change or not.
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	bool SetDataLayersIsLoadedInEditor(const TArray<UDataLayerInstance*>& DataLayers, const bool bIsLoadedInEditor, const bool bIsFromUserChange);

	/**
	 * Toggles the DataLayer's IsLoadedInEditor flag
	 *
	 * @param	DataLayer						The DataLayer to affect
	 * @param	bIsFromUserChange				If this change originates from a user change or not.
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	bool ToggleDataLayerIsLoadedInEditor(UDataLayerInstance* DataLayer, const bool bIsFromUserChange);

	/**
	 * Toggles the IsLoadedInEditor flag of all of the DataLayers
	 *
	 * @param	DataLayers						The DataLayers to affect
	 * @param	bIsFromUserChange				If this change originates from a user change or not.
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	bool ToggleDataLayersIsLoadedInEditor(const TArray<UDataLayerInstance*>& DataLayers, const bool bIsFromUserChange);

	/**
	 * Set the visibility of all DataLayers to true
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	void MakeAllDataLayersVisible();

	/**
	 * Gets the UDataLayerInstance associated to the DataLayerAsset
	 *
	 * @param	DataLayerAsset	The DataLayerAsset associated to the UDataLayerInstance
	 * @return					The UDataLayerInstance of the provided DataLayerAsset
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	UDataLayerInstance* GetDataLayerInstance(const UDataLayerAsset* DataLayerAsset) const;

	/**
	 * Gets the UDataLayerInstances associated to the each DataLayerAssets
	 *
	 * @param	DataLayerAssets	The array of DataLayerAssets associated to UDataLayerInstances
	 * @return					The array of UDataLayerInstances corresponding to a DataLayerAsset in the DataLayerAssets array
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	TArray<UDataLayerInstance*> GetDataLayerInstances(const TArray<UDataLayerAsset*> DataLayerAssets) const;

	/**
	 * Gets the UDataLayerInstance Object of the DataLayer name
	 *
	 * @param	DataLayerName	The name of the DataLayer whose UDataLayerInstance Object is returned
	 * @return					The UDataLayerInstance Object of the provided DataLayer name
	 */
	UDataLayerInstance* GetDataLayerInstance(const FName& DataLayerInstanceName) const;

	/**
	 * Gets all known DataLayers and appends them to the provided array
	 *
	 * @param OutDataLayers[OUT] Output array to store all known DataLayers
	 */
	void AddAllDataLayersTo(TArray<TWeakObjectPtr<UDataLayerInstance>>& OutDataLayers) const;

	/**
	 * Creates a UDataLayerInstance Object
	 *
	 * @param	Parameters The Data Layer Instance creation parameters
	 * @return	The newly created UDataLayerInstance Object
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	UDataLayerInstance* CreateDataLayerInstance(const FDataLayerCreationParameters& Parameters);

	/**
	 * Sets a Parent DataLayer for a specified DataLayer
	 * 
	 *  @param DataLayer		The child DataLayer.
	 *  @param ParentDataLayer	The parent DataLayer.
	 * 
	 *  @return	true if succeeded, false if failed.
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	bool SetParentDataLayer(UDataLayerInstance* DataLayer, UDataLayerInstance* ParentDataLayer);

	/**
	 * Deletes all of the provided DataLayers
	 *
	 * @param DataLayersToDelete	A valid list of DataLayer.
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	void DeleteDataLayers(const TArray<UDataLayerInstance*>& DataLayersToDelete);

	/**
	 * Deletes the provided DataLayer
	 *
	 * @param DataLayerToDelete		A valid DataLayer
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	void DeleteDataLayer(UDataLayerInstance* DataLayerToDelete);

	/**
	 * Returns all Data Layers
	 */
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	TArray<UDataLayerInstance*> GetAllDataLayers();

	/**
	* Resets user override settings of all DataLayers
	*/
	bool ResetUserSettings();

	/*
	 * Returns whether the DataLayer contains one of more actors that are part of the editor selection.
	 */
	bool DoesDataLayerContainSelectedActors(const UDataLayerInstance* DataLayer) const { return GetSelectedDataLayersFromEditorSelection().Contains(DataLayer); }

	/**
	* Whether there are any deprecated DataLayerInstance found
	*/
	bool HasDeprecatedDataLayers() const;

	/**
	 * Assign new unique short name to DataLayerInstance if it supports it. 
	 */
	bool SetDataLayerShortName(UDataLayerInstance* InDataLayerInstance, const FString& InNewShortName);

	//~ Begin Deprecated

	PRAGMA_DISABLE_DEPRECATION_WARNINGS

	UE_DEPRECATED(5.0, "Per-view Data Layer visibility was removed.")
	UFUNCTION(BlueprintCallable, Category = DataLayers, meta = (DeprecatedFunction, DeprecationMessage = "Per-view Data Layer visibility was removed."))
	void UpdateAllViewVisibility(UDEPRECATED_DataLayer* DataLayerThatChanged) {}

	UE_DEPRECATED(5.0, "Per-view Data Layer visibility was removed.")
	void UpdatePerViewVisibility(FLevelEditorViewportClient* ViewportClient, UDEPRECATED_DataLayer* DataLayerThatChanged = nullptr) {}

	UE_DEPRECATED(5.0, "Per-view Data Layer visibility was removed.")
	void UpdateActorViewVisibility(FLevelEditorViewportClient* ViewportClient, AActor* Actor, const bool bReregisterIfDirty = true) {}

	UE_DEPRECATED(5.0, "Per-view Data Layer visibility was removed.")
	UFUNCTION(BlueprintCallable, Category = DataLayers, meta = (DeprecatedFunction, DeprecationMessage = "Per-view Data Layer visibility was removed."))
	void UpdateActorAllViewsVisibility(AActor* Actor) {}
		
	UE_DEPRECATED(5.0, "Use SetDataLayerIsLoadedInEditor() instead.")
	UFUNCTION(BlueprintCallable, Category = DataLayers, meta = (DeprecatedFunction, DeprecationMessage = "Use SetDataLayerIsLoadedInEditor instead"))
	bool SetDataLayerIsDynamicallyLoadedInEditor(UDEPRECATED_DataLayer* DataLayer, const bool bIsLoadedInEditor, const bool bIsFromUserChange) { return false; }

	UE_DEPRECATED(5.0, "Use SetDataLayersIsLoadedInEditor() instead.")
	UFUNCTION(BlueprintCallable, Category = DataLayers, meta = (DeprecatedFunction, DeprecationMessage = "Use SetDataLayersIsLoadedInEditor instead"))
	bool SetDataLayersIsDynamicallyLoadedInEditor(const TArray<UDEPRECATED_DataLayer*>& DataLayers, const bool bIsLoadedInEditor, const bool bIsFromUserChange) { return false;  }

	UE_DEPRECATED(5.0, "Use ToggleDataLayerIsLoadedInEditor() instead.")
	UFUNCTION(BlueprintCallable, Category = DataLayers, meta = (DeprecatedFunction, DeprecationMessage = "Use ToggleDataLayerIsLoadedInEditor instead"))
	bool ToggleDataLayerIsDynamicallyLoadedInEditor(UDEPRECATED_DataLayer* DataLayer, const bool bIsFromUserChange) { return false; }

	UE_DEPRECATED(5.0, "Use ToggleDataLayersIsLoadedInEditor() instead.")
	UFUNCTION(BlueprintCallable, Category = DataLayers, meta = (DeprecatedFunction, DeprecationMessage = "Use ToggleDataLayersIsLoadedInEditor instead"))
	bool ToggleDataLayersIsDynamicallyLoadedInEditor(const TArray<UDEPRECATED_DataLayer*>& DataLayers, const bool bIsFromUserChange) { return false; }

	PRAGMA_ENABLE_DEPRECATION_WARNINGS
			
	UE_DEPRECATED(5.1, "Use GetDataLayerInstance instead")
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	UDataLayerInstance* GetDataLayer(const FActorDataLayer& ActorDataLayer) const;

	UE_DEPRECATED(5.1, "Use GetDataLayerInstance instead")
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	UDataLayerInstance* GetDataLayerFromLabel(const FName& DataLayerLabel) const;

	UE_DEPRECATED(5.1, "Renaming is not permitted anymore")
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	bool RenameDataLayer(UDataLayerInstance* DataLayer, const FName& NewDataLayerLabel);

	UE_DEPRECATED(5.1, "Use CreateDataLayerInstance with FDataLayerCreationParameters instead")
	UFUNCTION(BlueprintCallable, Category = DataLayers)
	UDataLayerInstance* CreateDataLayer(UDataLayerInstance* ParentDataLayer = nullptr) { return nullptr; }

	//~ End Deprecated

private:

	/**
	 *	Synchronizes an newly created Actor's DataLayers with the DataLayer system
	 *
	 *	@param	Actor	The actor to initialize
	 */
	void InitializeNewActorDataLayers(AActor* Actor);

	/**
	 * Find and return the selected actors.
	 *
	 * @return				The selected AActor's as a TArray.
	 */
	TArray<AActor*> GetSelectedActors() const;

	/**
	 * Get the current UWorld object.
	 *
	 * @return						The UWorld* object
	 */
	UWorld* GetWorld() const; // Fallback to GWorld

	/** Delegate handler for FEditorDelegates::MapChange. It internally calls DataLayerChanged.Broadcast. */
	void EditorMapChange();

	/** Delegate handler for FEditorDelegates::RefreshDataLayerBrowser. It internally calls UpdateAllActorsVisibility to refresh the actors of each DataLayer. */
	void EditorRefreshDataLayerBrowser();

	/** Delegate handler for FEditorDelegates::PostUndoRedo. It internally calls DataLayerChanged.Broadcast and UpdateAllActorsVisibility to refresh the actors of each DataLayer. */
	void PostUndoRedo();

	void UpdateRegisteredWorldDelegates();
	void OnWorldPartitionInitialized(UWorldPartition* InWorldPartition);
	void OnWorldPartitionUninitialized(UWorldPartition* InWorldPartition);
	void OnLoadedActorAddedToLevel(AActor& InActor);
	void OnDataLayerEditorLoadingStateChanged(bool bIsFromUserChange);

	bool SetDataLayerIsLoadedInEditorInternal(UDataLayerInstance* DataLayer, const bool bIsLoadedInEditor, const bool bIsFromUserChange);

	bool PassDataLayersFilter(UWorld* World, const FWorldPartitionHandle& ActorHandle);

	void BroadcastActorDataLayersChanged(const TWeakObjectPtr<AActor>& ChangedActor);
	void BroadcastDataLayerChanged(const EDataLayerAction Action, const TWeakObjectPtr<const UDataLayerInstance>& ChangedDataLayer, const FName& ChangedProperty);
	void BroadcastDataLayerEditorLoadingStateChanged(bool bIsFromUserChange);
	void OnSelectionChanged();
	void RebuildSelectedDataLayersFromEditorSelection();
	const TSet<TWeakObjectPtr<const UDataLayerInstance>>& GetSelectedDataLayersFromEditorSelection() const;

	bool UpdateAllActorsVisibility(const bool bNotifySelectionChange, const bool bRedrawViewports, ULevel* InLevel);

	bool IsActorValidForDataLayerForClasses(AActor* Actor, const TSet<TSubclassOf<UDataLayerInstance>>& DataLayerInstanceClasses);

	// External Data Layer methods
	void OnActorPreSpawnInitialization(AActor* Actor);
	static const UExternalDataLayerAsset* GetReferencingWorldSurrogateObjectForObject(UWorld* ReferencingWorld, const FSoftObjectPath& ObjectPath);
	TUniquePtr<FLevelEditorDragDropWorldSurrogateReferencingObject> OnLevelEditorDragDropWorldSurrogateReferencingObject(UWorld* ReferencingWorld, const FSoftObjectPath& Object);
	void OnExternalDataLayerAssetRegistrationStateChanged(const UExternalDataLayerAsset* ExternalDataLayerAsset, EExternalDataLayerRegistrationState OldState, EExternalDataLayerRegistrationState NewState);

	/** Contains Data Layers that contain actors that are part of the editor selection */
	mutable TSet<TWeakObjectPtr<const UDataLayerInstance>> SelectedDataLayersFromEditorSelection;

	/** Internal flag to know if SelectedDataLayersFromEditorSelection needs to be rebuilt. */
	mutable bool bRebuildSelectedDataLayersFromEditorSelection;

	/** When true, next Tick will call BroadcastDataLayerChanged */
	bool bAsyncBroadcastDataLayerChanged;

	/** When true, next Tick will call UpdateAllActorsVisibility */
	bool bAsyncUpdateAllActorsVisibility;

	/** When true, next Tick will force invalid editing viewports */
	bool bAsyncInvalidateViewports;

	/** Fires whenever one or more DataLayer changes */
	FOnDataLayerChanged DataLayerChanged;

	/**	Fires whenever one or more actor DataLayer changes */
	FOnActorDataLayersChanged ActorDataLayersChanged;

	/** Fires whenever one or more DataLayer editor loading state changed */
	FOnActorDataLayersEditorLoadingStateChanged DataLayerEditorLoadingStateChanged;

	FDelegateHandle OnActorDataLayersEditorLoadingStateChangedEngineBridgeHandle;

	/** Auxiliary class that sets the callback functions for multiple delegates */
	TSharedPtr<class FDataLayersBroadcast> DataLayersBroadcast;

	/** Delegate used to notify changes to ActorEditorContextSubsystem */
	FOnActorEditorContextClientChanged ActorEditorContextClientChanged;

	/** Last World to have registered world delegates */
	TWeakObjectPtr<UWorld> LastRegisteredWorldDelegates;

	/** Delegate handle for world's AddOnActorPreSpawnInitialization */
	FDelegateHandle OnActorPreSpawnInitializationDelegate;

	friend class FDataLayersBroadcast;
	friend struct FExternalDataLayerWorldSurrogateReferencingObject;
};

template<class DataLayerInstanceType, typename ...Args>
DataLayerInstanceType* UDataLayerEditorSubsystem::CreateDataLayerInstance(AWorldDataLayers* WorldDataLayers, Args&&... InArgs)
{
	UDataLayerInstance* NewDataLayer = nullptr;

	if (WorldDataLayers)
	{
		NewDataLayer = WorldDataLayers->CreateDataLayer<DataLayerInstanceType>(Forward<Args>(InArgs)...);
	}

	if (NewDataLayer != nullptr)
	{
		BroadcastDataLayerChanged(EDataLayerAction::Add, NewDataLayer, NAME_None);
	}

	return CastChecked<DataLayerInstanceType>(NewDataLayer);
}