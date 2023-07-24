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
 * UObject asset factory implementation:
 * 1. ImportAssetObject_GameThread - Create the asset UObject, and you can also import source data and setup properties synchronously.
 * 2. ImportAssetObject_Async - Import source data and setup properties asynchronously.
 * 3. SetupObject_GameThread - Do any UObject setup required before the build (before PostEditChange), the UObject dependencies should exist and have all the source data and properties imported.
 * 4. FinalizeObject_GameThread - Do any final UObject setup after the build (after PostEditChange)
 * 
 * Actor factory implementation
 * 1. ImportSceneObject_GameThread - Create an actor in a level.
 */

UCLASS(BlueprintType, Blueprintable, Abstract)
class INTERCHANGECORE_API UInterchangeFactoryBase : public UObject
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

	UE_DEPRECATED(5.2, "This function is replaced by ImportAssetObject_GameThread.")
	virtual UObject* CreateEmptyAsset(const FImportAssetObjectParams& Arguments)
	{
		return nullptr;
	}
	
	/**
	 * Create the asset UObject. You can optionally import source data (payload data) and configure the properties synchronously.
	 *
	 * @param Arguments - The structure containing all necessary arguments to import the UObject and the package, see the structure definition for the documentation.
	 * @return the imported UObject or nullptr if there is an error.
	 *
	 * @Note Mandatory to override this function to create the asset UObject. Not mandatory for level actor, use CreateSceneObject instead.
	 */
	virtual UObject* ImportAssetObject_GameThread(const FImportAssetObjectParams& Arguments)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return CreateEmptyAsset(Arguments);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	UE_DEPRECATED(5.2, "This function is replaced by ImportAssetObject_Async.")
	virtual UObject* CreateAsset(const FImportAssetObjectParams& Arguments)
	{
		//By default simply return the UObject created by ImportAssetObject_GameThread that was store in the asset node.
		FSoftObjectPath ReferenceObject;
		if (Arguments.AssetNode->GetCustomReferenceObject(ReferenceObject))
		{
			return ReferenceObject.TryLoad();
		}
		return nullptr;
	}
	/**
	 * Override it to setup the UObject source data (payload data) and configure the properties asynchronously. Do not create the asset UObject asynchronously.
	 *
	 * @param Arguments - The structure containing all necessary data to imported the UObject, see the structure definition for the documentation.
	 * @return the imported UObject or nullptr if there is an error.
	 */
	virtual UObject* ImportAssetObject_Async(const FImportAssetObjectParams& Arguments)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return CreateAsset(Arguments);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
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
	};

	UE_DEPRECATED(5.2, "This function is replaced by ImportSceneObject_GameThread.")
	virtual UObject* CreateSceneObject(const FImportSceneObjectsParams& Arguments)
	{
		return nullptr;
	}

	/**
	 * Creates the scene object from a Scene Node data.
	 *
	 * @param Arguments - The structure containing all necessary arguments, see the structure definition for the documentation.
	 * @return The scene object or nullptr if the operation was unsuccessful.
	 */
	virtual UObject* ImportSceneObject_GameThread(const FImportSceneObjectsParams& Arguments)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return CreateSceneObject(Arguments);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
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

	UE_DEPRECATED(5.2, "This function is replaced by SetupObject_GameThread.")
	virtual void PreImportPreCompletedCallback(const FSetupObjectParams& Arguments)
	{
		check(IsInGameThread());
	}

	/*
	 * Do any UObject setup required before the build (before PostEditChange) and after all dependency UObject have been imported.
	 * @note - This function is called when starting the pre completion task (before PostEditChange is called for the asset).
	 */
	virtual void SetupObject_GameThread(const FSetupObjectParams& Arguments)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		PreImportPreCompletedCallback(Arguments);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
	
	UE_DEPRECATED(5.2, "This function is replace by FinalizeObject_GameThread.")
	virtual void PostImportPreCompletedCallback(const FSetupObjectParams& Arguments)
	{
		check(IsInGameThread());
	}

	/*
	 * Do any final UObject setup after the build (after PostEditChange)
	 * @note - This function is called at the end of the pre completion task (after PostEditChange is called for the asset).
	 */
	virtual void FinalizeObject_GameThread(const FSetupObjectParams& Arguments)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		PostImportPreCompletedCallback(Arguments);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
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
