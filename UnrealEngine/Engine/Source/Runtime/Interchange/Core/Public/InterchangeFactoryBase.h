// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeResultsContainer.h"
#include "Nodes/InterchangeFactoryBaseNode.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"

#include "InterchangeFactoryBase.generated.h"

class ULevel;
class UInterchangeBaseNodeContainer;
class UInterchangePipelineBase;
class UInterchangeSourceData;
class UInterchangeTranslatorBase;

UENUM(BlueprintType)
enum class EInterchangeFactoryAssetType : uint8
{
	None = 0,
	Textures,
	Materials,
	Meshes,
	Animations,
	Physics
};

/**
 * Asset factory implementation:
 * 
 * The first 3 steps use the interchange factory node to import/re-import the UObject
 * 
 * 1. BeginImportAsset_GameThread - Create the asset UObject. You can also import source data (retrieve payloads) and setup properties on the game thread.
 * 2. ImportAsset_Async - Import source data (retrieve payloads) and setup properties asynchronously on any thread.
 * 3. EndImportAsset_GameThread - Anything you need to do on the game thread to finalize the import source data and setup properties. (Conflict resolution that need UI)
 * 
 * The last two steps are helpful to change the imported/re-imported UObject before and after the PostEditChange (render data build) is call on the asset.
 * 
 * 4. SetupObject_GameThread - Do any UObject setup required before the build (before PostEditChange), the UObject dependencies should exist and have all the source data and properties imported.
 * 5. FinalizeObject_GameThread - Do any final UObject setup after the build (after PostEditChange). Note that the build of an asset can be asynchronous and not finish yet.
 * 
 * 
 * Scene factory implementation
 * 
 * 1. ImportSceneObject_GameThread - Create an actor in a level.
 */

UCLASS(BlueprintType, Blueprintable, Abstract, MinimalAPI)
class UInterchangeFactoryBase : public UObject
{
	GENERATED_BODY()
public:

	/**
	 * return the UClass this factory can create.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Factory")
	virtual UClass* GetFactoryClass() const
	{
		return nullptr;
	}

	/**
	 * return the asset type this factory can create.
	 */
	virtual EInterchangeFactoryAssetType GetFactoryAssetType() { return EInterchangeFactoryAssetType::None; }

	/**
	 * Parameters to pass to CreateAsset function
	 */
	struct FImportAssetObjectParams
	{
		/** The package where to create the asset, if null it will put it in the transient package */
		UObject* Parent = nullptr;

		/** The name we want to give to the asset we will create */
		FString AssetName = FString();

		/** The base node that describe how to create the asset */
		UInterchangeFactoryBaseNode* AssetNode = nullptr;

		/** The translator is use to retrieve the PayLoad data in case the factory need it */
		const UInterchangeTranslatorBase* Translator = nullptr;

		/** The source data, mainly use to set the asset import data file. TODO: we have to refactor UAssetImportData, the source data should be the base class for this now */
		const UInterchangeSourceData* SourceData = nullptr;

		/** The node container associate with the current source index */
		const UInterchangeBaseNodeContainer* NodeContainer = nullptr;

		/**
		 * If when we try to create the package we found out the asset already exist, this field will contain
		 * the asset we want to re-import. The re-import should just change the source data and not any asset settings.
		 */
		UObject* ReimportObject = nullptr;
	};

	struct FImportAssetResult
	{
		//If the factory set this to true, the interchange task import object should skip this asset
		//Asset can be skip if it already exist and its not a re-import. Example, if we import a static mesh and we find an existing material.
		//We don't want to override the material in this case since the UE material often contains game play graph.
		bool bIsFactorySkipAsset = false;
		//Return the UObject import or re-import by the factory, leave it to nullptr if there was an error.
		UObject* ImportedObject = nullptr;
	};

	UE_DEPRECATED(5.3, "This function is replaced by BeginImportAsset_GameThread.")
	virtual UObject* ImportAssetObject_GameThread(const FImportAssetObjectParams& Arguments)
	{
		return nullptr;
	}

	/**
	 * Override it to import/re-import source data and configure the properties synchronously.
	 * Create the asset package on the game thread since its not thread safe.
	 *
	 * @param Arguments - The structure containing all necessary arguments to import the UObject and the package, see the structure definition for the documentation.
	 * @return the FImportAssetResult, see the structure to access the documentation.
	 *
	 * @Note Mandatory to override this function to create the asset UObject package. Not mandatory for level actor, use CreateSceneObject instead.
	 */
	virtual FImportAssetResult BeginImportAsset_GameThread(const FImportAssetObjectParams& Arguments)
	{
		FImportAssetResult ImportAssetResult;
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		ImportAssetResult.ImportedObject = ImportAssetObject_GameThread(Arguments);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		return ImportAssetResult;
	}

	UE_DEPRECATED(5.3, "This function is replaced by ImportAsset_Async.")
	virtual UObject* ImportAssetObject_Async(const FImportAssetObjectParams& Arguments)
	{
		//By default simply return the UObject created by BeginImportAsset_GameThread that was store in the asset node.
		FSoftObjectPath ReferenceObject;
		if (Arguments.AssetNode && Arguments.AssetNode->GetCustomReferenceObject(ReferenceObject))
		{
			return ReferenceObject.TryLoad();
		}
		return nullptr;
	}

	/**
	 * Override it to import/re-import the UObject source data and configure the properties asynchronously. Do not create the asset UObject asynchronously.
	 * Helpful to get all the payload in parallel or do any long import task
	 *
	 * @param Arguments - The structure containing all necessary data to imported the UObject, see the structure definition for the documentation.
	 * @return the FImportAssetResult, see the structure to access the documentation.
	 */
	virtual FImportAssetResult ImportAsset_Async(const FImportAssetObjectParams& Arguments)
	{
		FImportAssetResult ImportAssetResult;
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		ImportAssetResult.ImportedObject = ImportAssetObject_Async(Arguments);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		return ImportAssetResult;
	}

	/**
	 * Override it to end import/re-import on the game thread. Helpful if you need to display ui (i.e. re-import material conflict resolution) or if you
	 * need to do anything not thread safe to complete the import/re-import.
	 *
	 * @param Arguments - The structure containing all necessary data to imported the UObject, see the structure definition for the documentation.
	 * @return the FImportAssetResult, see the structure to access the documentation.
	 */
	virtual FImportAssetResult EndImportAsset_GameThread(const FImportAssetObjectParams& Arguments)
	{
		FImportAssetResult ImportAssetResult;
		//By default simply return the UObject created by BeginImportAssetObject_GameThread that was store in the asset node.
		FSoftObjectPath ReferenceObject;
		if (Arguments.AssetNode && Arguments.AssetNode->GetCustomReferenceObject(ReferenceObject))
		{
			ImportAssetResult.ImportedObject = ReferenceObject.TryLoad();
		}
		return ImportAssetResult;
	}

	/**
	 * Parameters to pass to SpawnActor function
	 */
	struct FImportSceneObjectsParams
	{
		/** The level in which to create the scene objects */
		ULevel* Level = nullptr;

		/** The name we want to give to the actor that we will create */
		FString ObjectName;

		/** The base node that describe how to create the asset */
		UInterchangeFactoryBaseNode* FactoryNode = nullptr;

		/** The node container associated with the current source index */
		const UInterchangeBaseNodeContainer* NodeContainer = nullptr;
	
		/**
		 * If not null, the factory must perform a reimport of the scene node
		 */
		UObject* ReimportObject = nullptr;
	
		/**
		 * Factory base node associated with the reimported scene node
		 */
		const UInterchangeFactoryBaseNode* ReimportFactoryNode = nullptr;
	};

	/**
	 * Creates the scene object from a Scene Node data.
	 *
	 * @param Arguments - The structure containing all necessary arguments, see the structure definition for the documentation.
	 * @return The scene object or nullptr if the operation was unsuccessful.
	 */
	virtual UObject* ImportSceneObject_GameThread(const FImportSceneObjectsParams& Arguments)
	{
		return nullptr;
	}

	/** Call when the user cancel the operation. */
	virtual void Cancel() {}

	/**
	 * Parameters to pass to CreateAsset function
	 */
	struct FSetupObjectParams
	{
		/** The source data, mainly use to set the asset import data file. TODO: we have to refactor UAssetImportData, the source data should be the base class for this now */
		const UInterchangeSourceData* SourceData = nullptr;
		UInterchangeFactoryBaseNode* FactoryNode = nullptr;


		/** The UObject  we want to execute code on*/
		UObject* ImportedObject = nullptr;
		FString NodeUniqueID;
		UInterchangeBaseNodeContainer* NodeContainer = nullptr;
		TArray<UInterchangePipelineBase*> Pipelines;
		TArray<UObject*> OriginalPipelines;
		
		bool bIsReimport  = false;
;
	};

	/*
	 * Do any UObject setup required before the build (before PostEditChange) and after all dependency UObject have been imported.
	 * @note - This function is called when starting the pre completion task (before PostEditChange is called for the asset).
	 */
	virtual void SetupObject_GameThread(const FSetupObjectParams& Arguments)
	{
		check(IsInGameThread());
	}
	
	/*
	 * Do any final UObject setup after the build (after PostEditChange)
	 * @note - This function is called at the end of the pre completion task (after PostEditChange is called for the asset).
	 */
	virtual void FinalizeObject_GameThread(const FSetupObjectParams& Arguments)
	{
		check(IsInGameThread());
	}

	/**
	 * Fills the OutSourceFilenames array with the list of source files contained in the asset source data.
	 * Returns true if the operation was successful.
	 */
	virtual bool GetSourceFilenames(const UObject* Object, TArray<FString>& OutSourceFilenames) const
	{
		return false;
	}

	/**
	 * Sets the object's source at the specified index to the given SourceFileName
	 */
	virtual bool SetSourceFilename(const UObject* Object, const FString& SourceFilename, int32 SourceIndex) const
	{
		return false;
	}

	/**
	 * Set the object's reimport source at the specified index value.
	 */
	virtual bool SetReimportSourceIndex(const UObject* Object, int32 SourceIndex) const
	{
		return false;
	}

	/**
	 * This function is used to add the given message object directly into the results for this operation.
	 */
	template <typename T>
	T* AddMessage()
	{
		check(Results != nullptr);
		T* Item = Results->Add<T>();
		return Item;
	}


	void AddMessage(UInterchangeResult* Item)
	{
		check(Results != nullptr);
		Results->Add(Item);
	}
	

	void SetResultsContainer(UInterchangeResultsContainer* InResults)
	{
		Results = InResults;
	}


	UPROPERTY()
	TObjectPtr<UInterchangeResultsContainer> Results;
};
