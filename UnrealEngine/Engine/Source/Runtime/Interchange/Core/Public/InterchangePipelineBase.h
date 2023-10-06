// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "InterchangeResultsContainer.h"
#include "InterchangeSourceData.h"
#include "Misc/AssertionMacros.h"
#include "Misc/OptionalFwd.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Nodes/InterchangeFactoryBaseNode.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "InterchangePipelineBase.generated.h"

class FText;
class UClass;
class UInterchangeBaseNodeContainer;
class UInterchangeResult;
class UInterchangeSourceData;
struct FFrame;

UENUM(BlueprintType)
enum class EInterchangePipelineTask : uint8
{
	PostTranslator,
	PostFactory,
	PostImport,
	Export
};

UENUM()
enum class EInterchangePipelineContext : uint8
{
	None, //Default pipeline instance we refer in the project settings pipeline stack. This context should allow editing of the properties states
	AssetImport,
	AssetReimport,
	SceneImport,
	SceneReimport,
	AssetCustomLODImport, //The import for custom LOD is there because we use a copy of the asset import data pipeline stack.
	AssetCustomLODReimport,
	AssetAlternateSkinningImport, //The import for custom LOD is there because we use a copy of the asset import data pipeline stack.
	AssetAlternateSkinningReimport,
};

USTRUCT(BlueprintType)
struct FInterchangePipelinePropertyStatePerContext
{
	GENERATED_BODY()

	/** If true, the property is visible. */
	UPROPERTY(EditAnywhere, Category = "Property States")
	bool bVisible = true;
};

USTRUCT(BlueprintType)
struct FInterchangePipelinePropertyStates
{
	GENERATED_BODY()

	/** Return true if the property is locked. */
	bool IsPropertyLocked() const
	{
		return bLocked;
	}
	
	void SetPropertyLocked(const bool bLockValue)
	{
		bLocked = bLockValue;
	}
	
	/** Return true if the property is visible for the specified context. */
	bool IsPropertyVisible(const bool bIsReimportContext) const
	{
		return bIsReimportContext ? ReimportStates.bVisible : ImportStates.bVisible;
	}

	void SetPropertyVisible(const bool bIsReimportContext, const bool bVisibleValue)
	{
		bIsReimportContext ? ReimportStates.bVisible = bVisibleValue : ImportStates.bVisible = bVisibleValue;
	}

	/** If true, the property is locked. */
	UPROPERTY(EditAnywhere, Category = "Property States")
	bool bLocked = false;

	/** The property states for the import context */
	UPROPERTY(EditAnywhere, Category = "Context Properties States")
	FInterchangePipelinePropertyStatePerContext ImportStates;

	/** The property states for the reimport context */
	UPROPERTY(EditAnywhere, Category = "Context Properties States")
	FInterchangePipelinePropertyStatePerContext ReimportStates;
};

UCLASS(BlueprintType, Blueprintable, editinlinenew, Abstract, MinimalAPI)
class UInterchangePipelineBase : public UObject
{
	GENERATED_BODY()

public:

	UE_DEPRECATED(5.2, "This function is replace by ScriptedExecutePipeline.")
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interchange | Pipeline")
	INTERCHANGECORE_API void ScriptedExecutePreImportPipeline(UInterchangeBaseNodeContainer* BaseNodeContainer, const TArray<UInterchangeSourceData*>& SourceDatas);

	UE_DEPRECATED(5.2, "This internal public function is deprecated and not replace by any.")
	void ScriptedExecutePreImportPipeline_Implementation(UInterchangeBaseNodeContainer* BaseNodeContainer, const TArray<UInterchangeSourceData*>& SourceDatas)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		//By default we call the virtual import pipeline execution
		ExecutePreImportPipeline(BaseNodeContainer, SourceDatas);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	/**
	 * ScriptedExecutePipeline, is call after the translation and before we parse the graph to call the factory.
	 * This is where factory node should be created by the pipeline.
	 * Each factory node represent an unreal asset create that will be create by an interchange factory.
	 * @note - the FTaskPipeline is calling this function not the virtual one that is call by the default implementation.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interchange | Pipeline")
	INTERCHANGECORE_API void ScriptedExecutePipeline(UInterchangeBaseNodeContainer* BaseNodeContainer, const TArray<UInterchangeSourceData*>& SourceDatas);
	/** The default implementation (call if the blueprint do not have any implementation) will call the virtual ExecutePipeline */
	void ScriptedExecutePipeline_Implementation(UInterchangeBaseNodeContainer* BaseNodeContainer, const TArray<UInterchangeSourceData*>& SourceDatas)
	{
		//By default we call the virtual import pipeline execution
		ExecutePipeline(BaseNodeContainer, SourceDatas);
	}

	/**
	 * ScriptedExecutePostFactoryPipeline is call after the factory create the unreal asset but before calling PostEditChange.
	 * @note - the FTaskPreCompletion task is calling this function not the virtual one that is call by the default implementation.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interchange | Pipeline")
	INTERCHANGECORE_API void ScriptedExecutePostFactoryPipeline(const UInterchangeBaseNodeContainer* BaseNodeContainer, const FString& FactoryNodeKey, UObject* CreatedAsset, bool bIsAReimport);
	/** The default implementation (call if the blueprint do not have any implementation) will call the virtual ExecutePostFactoryPipeline */
	void ScriptedExecutePostFactoryPipeline_Implementation(const UInterchangeBaseNodeContainer* BaseNodeContainer, const FString& FactoryNodeKey, UObject* CreatedAsset, bool bIsAReimport)
	{
		//By default we call the virtual import pipeline execution
		ExecutePostFactoryPipeline(BaseNodeContainer, FactoryNodeKey, CreatedAsset, bIsAReimport);
	}
	/**
	 * ScriptedExecutePostImportPipeline is call after we completely import an asset, PostEditChange is already called.
	 * This can be useful if you need an asset build data to finish the setup of another asset.
	 * @example - PhysicsAsset need skeletal mesh render data to be build properly.
	 * @note - the FTaskPipelinePostImport is calling this function not the virtual one that is call by the default implementation.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interchange | Pipeline")
	INTERCHANGECORE_API void ScriptedExecutePostImportPipeline(const UInterchangeBaseNodeContainer* BaseNodeContainer, const FString& FactoryNodeKey, UObject* CreatedAsset, bool bIsAReimport);
	/** The default implementation (call if the blueprint do not have any implementation) will call the virtual ExecutePostImportPipeline */
	void ScriptedExecutePostImportPipeline_Implementation(const UInterchangeBaseNodeContainer* BaseNodeContainer, const FString& FactoryNodeKey, UObject* CreatedAsset, bool bIsAReimport)
	{
		//By default we call the virtual import pipeline execution
		ExecutePostImportPipeline(BaseNodeContainer, FactoryNodeKey, CreatedAsset, bIsAReimport);
	}

	/**
	 * Non virtual helper to allow blueprint to implement event base function let the interchange know if it can run asynchronously.
	 * the Interchange manager is calling this function not the virtual one that is call by the default implementation.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interchange | Pipeline")
	INTERCHANGECORE_API void ScriptedExecuteExportPipeline(UInterchangeBaseNodeContainer* BaseNodeContainer);

	/** The default implementation (call if the blueprint do not have any implementation) will call the virtual ExecuteExportPipeline */
	void ScriptedExecuteExportPipeline_Implementation(UInterchangeBaseNodeContainer* BaseNodeContainer)
	{
		ExecuteExportPipeline(BaseNodeContainer);
	}

	/**
	 * Non virtual helper to allow blueprint to implement event base function let the interchange know if it can run asynchronously.
	 * the Interchange manager is calling this function not the virtual one that is call by the default implementation.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interchange | Pipeline")
	INTERCHANGECORE_API bool ScriptedCanExecuteOnAnyThread(EInterchangePipelineTask PipelineTask);

	/** The default implementation (call if the blueprint do not have any implementation) will call the virtual CanExecuteAsync */
	bool ScriptedCanExecuteOnAnyThread_Implementation(EInterchangePipelineTask PipelineTask)
	{
		return CanExecuteOnAnyThread(PipelineTask);
	}

	/**
	 * Non virtual helper to allow blueprint to implement event base function.
	 * the Interchange framework is calling this function not the virtual one that is called by the default implementation.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interchange | Pipeline")
	INTERCHANGECORE_API void ScriptedSetReimportSourceIndex(UClass* ReimportObjectClass, const int32 SourceFileIndex);

	/** The default implementation (call if the blueprint do not have any implementation) will call the virtual SetReimportContentFromSourceIndex */
	void ScriptedSetReimportSourceIndex_Implementation(UClass* ReimportObjectClass, const int32 SourceFileIndex)
	{
		SetReimportSourceIndex(ReimportObjectClass, SourceFileIndex);
	}

	/**
	 * Non scripted class should return false here, we have the default to true because scripted class cannot override
	 * this function since it can be call in a asynchronous thread, which python cannot be executed.
	 *
	 * We cannot call ScriptedCanExecuteOnAnyThread for a scripted python pipeline from the task parsing async thread.
	 * This function allow us to not call it and force the ScriptedExecutePostImportPipeline to execute on the game thread.
	 */
	virtual bool IsScripted()
	{
		return true;
	}

	INTERCHANGECORE_API void LoadSettings(const FName PipelineStackName);

	INTERCHANGECORE_API void SaveSettings(const FName PipelineStackName);

	/**
	 * This function is called before showing the import dialog it is not called doing a re-import.
	 */
	virtual void PreDialogCleanup(const FName PipelineStackName) {}

	/**
	 * This function should return true if all the pipeline settings are in a valid state to start the import.
	 * The Interchange Pipeline Configuration Dialog will call this to know if the Import button can be enable.
	 */
	virtual bool IsSettingsAreValid(TOptional<FText>& OutInvalidReason) const
	{
		return true;
	}

	/**
	 * This function is call when before we show the pipeline dialog. Pipeline that override it can change the existing settings according to the re-import type.
	 * The function is also call when we import or re-import custom LOD and alternate skinning.
	 * 
	 * @Note - The function will set the context of the pipeline
	 * @Param ReimportType - Tell pipeline what re-import type the user want to achieve.
	 * @Param ReimportAsset - This is an optional parameter which is set when re-importing an asset.
	 */
	INTERCHANGECORE_API virtual void AdjustSettingsForContext(EInterchangePipelineContext ReimportType, TObjectPtr<UObject> ReimportAsset);
	INTERCHANGECORE_API virtual void AdjustSettingsFromCache();

	/** Transfer the source pipeline adjust settings to this pipeline. */
	INTERCHANGECORE_API void TransferAdjustSettings(UInterchangePipelineBase* SourcePipeline);

	/**
	 * This function is used to add the given message object directly into the results for this operation.
	 */
	template <typename T>
	T* AddMessage() const
	{
		check(Results != nullptr);
		T* Item = Results->Add<T>();
		return Item;
	}

	void AddMessage(UInterchangeResult* Item) const
	{
		check(Results != nullptr);
		Results->Add(Item);
	}
	
	void SetResultsContainer(UInterchangeResultsContainer* InResults)
	{
		Results = InResults;
	}

	/**
	 * Return a const property states pointer. Return nullptr if the property do not exist
	 */
	INTERCHANGECORE_API const FInterchangePipelinePropertyStates* GetPropertyStates(const FName PropertyPath) const;

	/**
	 * Return a mutable property states pointer. Return nullptr if the property do not exist
	 */
	INTERCHANGECORE_API FInterchangePipelinePropertyStates* GetMutablePropertyStates(const FName PropertyPath);

	/**
	 * Return true if the property has valid states, false if no states was set for the property
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Pipeline")
	INTERCHANGECORE_API bool DoesPropertyStatesExist(const FName PropertyPath) const;

	/**
	 * Return a mutable property states Reference. Add the property states if it doesnt exist.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Pipeline")
	INTERCHANGECORE_API FInterchangePipelinePropertyStates& FindOrAddPropertyStates(const FName PropertyPath);

	/**
	 * Return the property name of the properties states map
	 */
	static INTERCHANGECORE_API FName GetPropertiesStatesPropertyName();

	static INTERCHANGECORE_API FName GetResultsPropertyName();

	bool CanEditPropertiesStates() { return bAllowPropertyStatesEdition; }
	bool IsReimportContext() { return bIsReimportContext; }

#if WITH_EDITOR
	/*
	 * FName or FString properties can have a dynamic set of possible values. When displaying 
	 * a pipeline property in the property editor, this function will be call for all FString and FName
	 * properties and will generate a combo box with the PossibleValues if the function return true.
	 */
	virtual bool GetPropertyPossibleValues(const FName PropertyPath, TArray<FString>& PossibleValues)
	{
		return false;
	}
#endif //WITH_EDITOR

	/*
	 * Return true if the pipeline is not a sub pipeline. Stand Alone Pipeline can have some extra properties.
	 * The customize detail of the pipeline will call this function to show or hide those property.
	 * The pipeline itself can call the function to know if a property must be use or not.
	 * Example: the override asset name property
	 */
	bool IsStandAlonePipeline()
	{
		return this == GetMostPipelineOuter();
	};

	/*
	* Update/Re-set the TWeakObjectPtrs to point to their original TObjectPtr
	* Works based on Variable naming.
	*/
	INTERCHANGECORE_API void UpdateWeakObjectPtrs();

	/** Begin UObject overrides*/
	INTERCHANGECORE_API virtual void PostDuplicate(bool bDuplicateForPIE) override;
	/** End UObject overrides */

	/*
	 * If it return true it mean we want to save this pipeline in the asset import data.
	 * We will reuse this pipeline when re-importing the asset.
	 * If false its probably a debug helper pipeline we do not want to save into assets
	 */
	INTERCHANGECORE_API virtual bool SupportReimport() const { return true; }

protected:

	UE_DEPRECATED(5.2, "This function is replace by ExecutePipeline.")
	virtual void ExecutePreImportPipeline(UInterchangeBaseNodeContainer* BaseNodeContainer, const TArray<UInterchangeSourceData*>& SourceDatas)
	{
	}

	/**
	 * This function is called after the translation is done. It should create any factory node representing unreal assets to be imported by the factories
	 */
	virtual void ExecutePipeline(UInterchangeBaseNodeContainer* BaseNodeContainer, const TArray<UInterchangeSourceData*>& SourceDatas)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		ExecutePreImportPipeline(BaseNodeContainer, SourceDatas);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	/**
	 * This function is called after the factory create an asset, but before PostEditChange is called.
	 * It is always call on the game thread, code inside this function should not wait on other thread. (do not stall the game thread)
	 * This is the place to do any change to the unreal asset before PostEditChange is call.
	 */
	virtual void ExecutePostFactoryPipeline(const UInterchangeBaseNodeContainer* BaseNodeContainer, const FString& NodeKey, UObject* CreatedAsset, bool bIsAReimport)
	{
	}

	/**
	 * This function is called after the unreal asset is completely import and PostEditChange was called.
	 * @Note: Some unreal assets have asynchronous build, its possible they are still compiling.
	 */
	virtual void ExecutePostImportPipeline(const UInterchangeBaseNodeContainer* BaseNodeContainer, const FString& NodeKey, UObject* CreatedAsset, bool bIsAReimport)
	{
	}

	/**
	 * This function tell the interchange manager if we can execute this pipeline in async mode. If it return false, the ScriptedExecuteImportPipeline
	 * will be call on the main thread (GameThread), if true it will be run in a background thread and possibly in parallel. If there is multiple
	 * import process in same time.
	 *
	 */
	virtual bool CanExecuteOnAnyThread(EInterchangePipelineTask PipelineTask)
	{
		return true;
	}

	virtual void SetReimportSourceIndex(UClass* ReimportObjectClass, const int32 SourceFileIndex)
	{
	}

	/** This function can modify the BaseNodeContainer to create a pipeline that will set/validate the graph nodes hierarchy and options.*/
	virtual void ExecuteExportPipeline(UInterchangeBaseNodeContainer* BaseNodeContainer)
	{
	}

	INTERCHANGECORE_API void LoadSettingsInternal(const FName PipelineStackName, const FString& ConfigFilename, TMap<FName, FInterchangePipelinePropertyStates>& ParentPropertiesStates);

	INTERCHANGECORE_API void SaveSettingsInternal(const FName PipelineStackName, const FString& ConfigFilename);

	INTERCHANGECORE_API UInterchangePipelineBase* GetMostPipelineOuter() const;

	static INTERCHANGECORE_API void InternalToggleVisibilityPropertiesOfMetaDataValue(UInterchangePipelineBase* OuterMostPipeline, UInterchangePipelineBase* Pipeline, bool bDoTransientSubPipeline, const FString& MetaDataKey, const FString& MetaDataValue, const bool bVisibilityState);
	static INTERCHANGECORE_API void HidePropertiesOfCategory(UInterchangePipelineBase* OuterMostPipeline, UInterchangePipelineBase* Pipeline, const FString& HideCategoryName, bool bDoTransientSubPipeline = false);
	static INTERCHANGECORE_API void HidePropertiesOfSubCategory(UInterchangePipelineBase* OuterMostPipeline, UInterchangePipelineBase* Pipeline, const FString& HideSubCategoryName, bool bDoTransientSubPipeline = false);
	
	/**
	 * If true, the property editor for this pipeline instance will allow properties states edition.
	 * If false, the property editor for this pipeline instance will apply the properties states.
	 *
	 * Note: If you open in the content browser a pipeline asset you will be able to edit the properties states.
	 *       If you import a file with interchange, the import dialog will apply properties states.
	 */
	bool bAllowPropertyStatesEdition = true;

	/**
	 * If true, this pipeline instance is use for reimport.
	 * If false, this pipeline instance is use for import.
	 *
	 * Note: This context must be set by the owner instancing this pipeline. This context will be use to hide or not some properties.
	 */
	bool bIsReimportContext = false;

	UPROPERTY()
	TObjectPtr<UInterchangeResultsContainer> Results;

	/**
	 * Map of property path and lock status. Any properties that have a true lock status will be readonly when showing the import dialog.
	 * Use the API to Get and Set the properties states
	*/
	UPROPERTY()
	TMap<FName, FInterchangePipelinePropertyStates> PropertiesStates;

	mutable TMap<FName, FInterchangePipelinePropertyStates> CachePropertiesStates;
	mutable EInterchangePipelineContext CachePipelineContext = EInterchangePipelineContext::None;
	mutable TWeakObjectPtr<UObject> CacheReimportObject = nullptr;
};
