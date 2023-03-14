// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <atomic>

#include "CoreMinimal.h"

#include "Async/TaskGraphInterfaces.h"
#include "Containers/Queue.h"
#include "Delegates/DelegateCombinations.h"
#include "HAL/Thread.h"
#include "HAL/ThreadSafeBool.h"
#include "InterchangePipelineConfigurationBase.h"
#include "InterchangeResultsContainer.h"
#include "InterchangeSourceData.h"
#include "InterchangeTranslatorBase.h"
#include "InterchangeWriterBase.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Templates/Tuple.h"
#include "UObject/Package.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/StrongObjectPtr.h"
#include "Containers/Ticker.h"

#include "InterchangeManager.generated.h"

class FAsyncTaskNotification;
class UInterchangeFactoryBase;
class UInterchangeFactoryBaseNode;
class UInterchangePipelineBase;

/** Some utilities delegates for the automation of interchange */
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnObjectImportDoneDynamic, UObject*, Object);
DECLARE_DELEGATE_OneParam(FOnObjectImportDoneNative, UObject*);

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnImportDoneDynamic, const TArray<UObject*>&, Objects);
DECLARE_DELEGATE_OneParam(FOnImportDoneNative, const TArray<UObject*>&);

namespace UE
{
	namespace Interchange
	{
		class INTERCHANGEENGINE_API FScopedInterchangeImportEnableState
		{
		public:
			explicit FScopedInterchangeImportEnableState(const bool bScopeValue);
			~FScopedInterchangeImportEnableState();
		private:
			bool bOriginalInterchangeImportEnableState;
		};

		class INTERCHANGEENGINE_API FScopedSourceData
		{
		public:
			explicit FScopedSourceData(const FString& Filename);
			UInterchangeSourceData* GetSourceData() const;
		private:
			TStrongObjectPtr<UInterchangeSourceData> SourceDataPtr = nullptr;
		};

		class INTERCHANGEENGINE_API FScopedTranslator
		{
		public:
			explicit FScopedTranslator(const UInterchangeSourceData* SourceData);
			UInterchangeTranslatorBase* GetTranslator();

		private:
			TStrongObjectPtr<UInterchangeTranslatorBase> ScopedTranslatorPtr = nullptr;
		};

		enum class EImportType : uint8
		{
			ImportType_None,
			ImportType_Asset,
			ImportType_Scene
		};

		struct FImportAsyncHelperData
		{
			//True if the import process is unattended. We cannot show UI  if the import is automated
			bool bIsAutomated = false;

			//We can import assets or full scene
			EImportType ImportType = EImportType::ImportType_None;

			//True if we are reimporting assets or scene
			UObject* ReimportObject = nullptr;
		};

		class INTERCHANGEENGINE_API FImportResult : protected FGCObject
		{
		public:
			FImportResult();

			FImportResult(FImportResult&&) = delete;
			FImportResult& operator=(FImportResult&&) = delete;

			FImportResult(const FImportResult&) = delete;
			FImportResult& operator=(const FImportResult&) = delete;

			virtual ~FImportResult() = default;

		public:
			enum class EStatus
			{
				Invalid,
				InProgress,
				Done
			};

			EStatus GetStatus() const;

			bool IsValid() const;

			void SetInProgress();
			void SetDone();
			void WaitUntilDone();

			// Assets are only made available once they have been completely imported (passed through the entire import pipeline)
			// While the status isn't EStatus::Done, the list can grow between subsequent calls.
			// FAssetImportResult holds a reference to the assets so that they aren't garbage collected.
			const TArray< UObject* >& GetImportedObjects() const;

			// Helper to get the first asset of a certain class. Use when expecting a single asset of that class to be imported since the order isn't deterministic.
			UObject* GetFirstAssetOfClass(UClass* InClass) const;

			// Return the results of this asset import operation
			UInterchangeResultsContainer* GetResults() const { return Results; }

			// Adds an asset to the list of imported assets.
			void AddImportedObject(UObject* ImportedObject);

			// Callback when the status switches to done.
			void OnDone(TFunction< void(FImportResult&) > Callback);

			// Internal delegates (use the FImportAssetParameters when calling the interchange import functions to set those)
			FOnObjectImportDoneDynamic OnObjectDone;
			FOnObjectImportDoneNative OnObjectDoneNative;

			FOnImportDoneDynamic OnImportDone;
			FOnImportDoneNative OnImportDoneNative;

		protected:
			/* FGCObject interface */
			virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
			virtual FString GetReferencerName() const override
			{
				return TEXT("UE::Interchange::FImportResult");
			}

		private:
			std::atomic< EStatus > ImportStatus;

			TArray< UObject* > ImportedObjects;
			mutable FRWLock ImportedObjectsRWLock;
			UInterchangeResultsContainer* Results;

			FGraphEventRef GraphEvent; // WaitUntilDone waits for this event to be triggered.

			TFunction< void(FImportResult&) > DoneCallback;
		};

		using FAssetImportResultRef = TSharedRef< FImportResult, ESPMode::ThreadSafe >;
		using FSceneImportResultRef = TSharedRef< FImportResult, ESPMode::ThreadSafe >;
		using FAssetImportResultPtr = TSharedPtr< FImportResult, ESPMode::ThreadSafe >;
		using FSceneImportResultPtr = TSharedPtr< FImportResult, ESPMode::ThreadSafe >;

		class FImportAsyncHelper : protected FGCObject
		{
		public:
			FImportAsyncHelper();

			~FImportAsyncHelper()
			{
				CleanUp();
			}

			/* FGCObject interface */
			virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
			virtual FString GetReferencerName() const override
			{
				return TEXT("UE::Interchange::FImportAsyncHelper");
			}

			/** Unique id for this asynchelper. */
			int32 UniqueId;

			//The following Arrays are per source data
			TArray<TStrongObjectPtr<UInterchangeBaseNodeContainer>> BaseNodeContainers;
			TArray<UInterchangeSourceData*> SourceDatas;
			TArray<UInterchangeTranslatorBase*> Translators;

			//Pipelines array is not per source data
			TArray<UInterchangePipelineBase*> Pipelines;
			//The original pipelines asset to save in the asset reimport data. The original pipeline can restore python class member value.
			//Python class instanced assets cannot be saved, so we have to serialize in json the data to restore it when we do a re-import.
			TArray<UObject*> OriginalPipelines;

			TArray<FGraphEventRef> TranslatorTasks;
			TArray<FGraphEventRef> PipelinePreImportTasks;
			TArray<FGraphEventRef> PipelinePostImportTasks;
			FGraphEventRef ParsingTask;
			TArray<FGraphEventRef> CreatePackageTasks;
			TArray<FGraphEventRef> CreateAssetTasks;
			TArray<FGraphEventRef> SceneTasks;

			FGraphEventRef PreAsyncCompletionTask;
			FGraphEventRef PreCompletionTask;
			FGraphEventRef CompletionTask;

			//Create package map, Key is package name. We cannot create package asynchronously so we have to create a game thread task to do this
			FCriticalSection CreatedPackagesLock;
			TMap<FString, UPackage*> CreatedPackages;

			// Created factories map, Key is factory node UID
			FCriticalSection CreatedFactoriesLock;
			TMap<FString, UInterchangeFactoryBase*> CreatedFactories;

			struct FImportedObjectInfo
			{
				UObject* ImportedObject = nullptr; // The object that was imported
				UInterchangeFactoryBase* Factory = nullptr; //The factory that created the imported object
				UInterchangeFactoryBaseNode* FactoryNode = nullptr; //The node that describes the object
				bool bIsReimport;
			};

			FCriticalSection ImportedAssetsPerSourceIndexLock;
			TMap<int32, TArray<FImportedObjectInfo>> ImportedAssetsPerSourceIndex;

			FCriticalSection ImportedSceneObjectsPerSourceIndexLock;
			TMap<int32, TArray<FImportedObjectInfo>> ImportedSceneObjectsPerSourceIndex;

			FImportAsyncHelperData TaskData;

			FAssetImportResultRef AssetImportResult;
			FSceneImportResultRef SceneImportResult;
			
			//If we cancel the tasks, we set this boolean to true
			std::atomic<bool> bCancel;

			void SendAnalyticImportEndData();
			void ReleaseTranslatorsSource();
			void InitCancel();
			void CancelAndWaitUntilDoneSynchronously();

			void CleanUp();
		};

		void SanitizeObjectPath(FString& ObjectPath);

		void SanitizeObjectName(FString& ObjectName);

		/* This function take an asset representing a pipeline and generate a UInterchangePipelineBase asset. */
		INTERCHANGEENGINE_API UInterchangePipelineBase* GeneratePipelineInstance(const FSoftObjectPath& PipelineInstance);

	} //ns interchange
} //ns UE

USTRUCT(BlueprintType)
struct FImportAssetParameters
{
	GENERATED_USTRUCT_BODY()

	// If the import is a reimport for a specific asset set the asset to reimport here
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interchange|ImportAsset")
	TObjectPtr<UObject> ReimportAsset = nullptr;

	// If we are doing a reimport, set the source index here. Some asset have more then one source file when they import partial part of there content.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interchange|ImportAsset")
	int32 ReimportSourceIndex = INDEX_NONE;

	// Tell interchange that import is automated and it shouldn't present a model window
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interchange|ImportAsset")
	bool bIsAutomated = false;

	// Adding some override will tell interchange to use the specific custom set pipelines instead of letting the user or the system chose
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interchange|ImportAsset")
	TArray<TObjectPtr<UInterchangePipelineBase>> OverridePipelines;

	/* Delegates used track the imported objects */

	// This is called each time an asset is imported or reimported from the import call
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interchange|ImportAsset", meta=(PinHiddenByDefault))
	FOnObjectImportDoneDynamic OnAssetDone;
	FOnObjectImportDoneNative OnAssetDoneNative;

	// This is called when all the assets where imported from the source data
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interchange|ImportAsset", meta=(PinHiddenByDefault))
	FOnImportDoneDynamic OnAssetsImportDone;
	FOnImportDoneNative OnAssetsImportDoneNative;

	// This is called each time an object in the scene is imported or reimported from the import call
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interchange|ImportAsset", meta=(PinHiddenByDefault))
	FOnObjectImportDoneDynamic OnSceneObjectDone;
	FOnObjectImportDoneNative OnSceneObjectDoneNative;

	// This is called when all the scene object where imported from the source data
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interchange|ImportAsset", meta=(PinHiddenByDefault))
	FOnImportDoneDynamic OnSceneImportDone;
	FOnImportDoneNative OnSceneImportDoneNative;
};

UCLASS(Transient, BlueprintType)
class INTERCHANGEENGINE_API UInterchangeManager : public UObject
{
	GENERATED_BODY()
public:

	/**
	 * Return the interchange manager singleton pointer.
	 *
	 * @note - We need to return a pointer to have a valid Blueprint callable function
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Import Manager")
	static UInterchangeManager* GetInterchangeManagerScripted()
	{
		return &GetInterchangeManager();
	}

	/** Return the interchange manager singleton.*/
	static UInterchangeManager& GetInterchangeManager();

	/** Return the CVar which make interchange enable or not.*/
	static bool IsInterchangeImportEnabled();

	/** delegate type fired when new assets have been imported. Note: InCreatedObject can be NULL if import failed. Params: UFactory* InFactory, UObject* InCreatedObject */
	DECLARE_MULTICAST_DELEGATE_OneParam(FInterchangeOnAssetPostImport, UObject*);
	/** delegate type fired when new assets have been reimported. Note: InCreatedObject can be NULL if import failed. Params: UObject* InCreatedObject */
	DECLARE_MULTICAST_DELEGATE_OneParam(FInterchangeOnAssetPostReimport, UObject*);
	/** delegate type fired when import results in an error */
	DECLARE_MULTICAST_DELEGATE_OneParam(FInterchangeOnBatchImportComplete, TStrongObjectPtr<UInterchangeResultsContainer>);

	// Delegates used to register and unregister

	FInterchangeOnAssetPostImport OnAssetPostImport;
	FInterchangeOnAssetPostReimport OnAssetPostReimport;
	FInterchangeOnBatchImportComplete OnBatchImportComplete;
	// Called when before the application is exiting.
	FSimpleMulticastDelegate OnPreDestroyInterchangeManager;

	/**
	 * Any translator must register to the manager
	 * @Param Translator - The UClass of the translator you want to register
	 * @return true if the translator class can be register false otherwise.
	 *
	 * @Note if you register multiple time the same class it will return true for every call
	 * @Note The order in which the translators are registered will be the same as the order used to select a translator to import a file
	 */
	bool RegisterTranslator(const UClass* TranslatorClass);

	/**
	 * Any factory must register to the manager
	 * @Param Factory - The UClass of the factory you want to register
	 * @return true if the factory class can be register false otherwise.
	 *
	 * @Note if you register multiple time the same class it will return true for every call
	 */
	bool RegisterFactory(const UClass* Factory);

	/**
	 * Any writer must register to the manager
	 * @Param Writer - The UClass of the writer you want to register
	 * @return true if the writer class can be register false otherwise.
	 *
	 * @Note if you register multiple time the same class it will return true for every call
	 */
	bool RegisterWriter(const UClass* Writer);

	/**
	 * Returns the list of supported formats for a given translator type.
	 */
	TArray<FString> GetSupportedFormats(const EInterchangeTranslatorType ForTranslatorType) const;

	/**
	 * Returns the list of formats supporting the specified translator asset type.
	 */
	TArray<FString> GetSupportedAssetTypeFormats(const EInterchangeTranslatorAssetType ForTranslatorAssetType) const;

	/**
	 * Returns the list of supported formats for a given Object.
	 */
	TArray<FString> GetSupportedFormatsForObject(const UObject* Object) const;

	/**
	 * Look if there is a registered translator for this source data.
	 * This allow us to by pass the original asset tools system to import supported asset.
	 * @Param SourceData - The source data input we want to translate to Uod
	 * @return True if there is a registered translator that can handle handle this source data, false otherwise.
	 */
	bool CanTranslateSourceData(const UInterchangeSourceData* SourceData) const;

	/**
	 * Returns true if Interchange can create that type of assets and is able to translate its source file.
	 * @Param Object - The object we want to reimport.
	 * @Param OutFilenames - An array that is filled with the object's source filenames if the operation is successful.
	 */
	bool CanReimport(const UObject* Object, TArray<FString>& OutFilenames) const;

	/**
	 * Call this to start an import asset process, the caller must specify a source data.
	 * This import process can import many different asset, but all in the game content.
	 *
	 * @Param ContentPath - The content path where to import the assets
	 * @Param SourceData - The source data input we want to translate
	 * @param ImportAssetParameters - All import asset parameter we need to pass to the import asset function
	 * @return true if the import succeed, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Import Manager")
	bool ImportAsset(const FString& ContentPath, const UInterchangeSourceData* SourceData, const FImportAssetParameters& ImportAssetParameters);
	UE::Interchange::FAssetImportResultRef ImportAssetAsync(const FString& ContentPath, const UInterchangeSourceData* SourceData, const FImportAssetParameters& ImportAssetParameters);

	/**
	 * Call this to start an import scene process, the caller must specify a source data.
	 * This import process can import many different asset and there transform (USceneComponent) and store the result in a blueprint and add the blueprint to the level.
	 *
	 * @Param ContentPath - The content path where to import the assets
	 * @Param SourceData - The source data input we want to translate, this object will be duplicate to allow multithread safe operations
	 * @param ImportAssetParameters - All import asset parameter we need to pass to the import asset function
	 * @return true if the import succeed, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Import Manager")
	bool ImportScene(const FString& ContentPath, const UInterchangeSourceData* SourceData, const FImportAssetParameters& ImportAssetParameters);

	TTuple<UE::Interchange::FAssetImportResultRef, UE::Interchange::FSceneImportResultRef>
	ImportSceneAsync(const FString& ContentPath, const UInterchangeSourceData* SourceData, const FImportAssetParameters& ImportAssetParameters);

	/**
	 * Call this to start an export asset process, the caller must specify a source data.
	 * 
	 * @Param SourceData - The source data output 
	 * @Param bIsAutomated - If true the exporter will not show any UI or dialog
	 * @return true if the import succeed, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Export Manager")
	bool ExportAsset(const UObject* Asset, bool bIsAutomated = false);

	/**
	 * Call this to start an export scene process, the caller must specify a source data
	 * This import process can import many different asset and there transform (USceneComponent) and store the result in a blueprint and add the blueprint to the level.
	 * @Param SourceData - The source data input we want to translate
	 * @Param bIsAutomated - If true the import asset will not show any UI or dialog
	 * @return true if the import succeed, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Export Manager")
	bool ExportScene(const UObject* World, bool bIsAutomated = false);

	/*
	* Script helper to create a source data object pointing on a file on disk
	* @Param InFilename: Specify a file on disk
	* @return: A new UInterchangeSourceData.
	*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Import Manager")
	static UInterchangeSourceData* CreateSourceData(const FString& InFileName);

	/**
	* Script helper to get a registered factory for a specified class
	* @Param FactoryClass: The class we search a registerd factory
	* @return: if found, we return the factory class that is registered. Return NULL if nothing found.
	*/
	UFUNCTION(BlueprintCallable, Category = "Interchange | Import Manager")
	const UClass* GetRegisteredFactoryClass(const UClass* ClassToMake) const;

	/**
	 * Return an FImportAsynHelper pointer. The pointer is deleted when ReleaseAsyncHelper is call.
	 * @param Data - The data we want to pass to the different import tasks
	 */
	TSharedRef<UE::Interchange::FImportAsyncHelper, ESPMode::ThreadSafe> CreateAsyncHelper(const UE::Interchange::FImportAsyncHelperData& Data, const FImportAssetParameters& ImportAssetParameters);

	/** Delete the specified AsyncHelper and remove it from the array that was holding it. */
	void ReleaseAsyncHelper(TWeakPtr<UE::Interchange::FImportAsyncHelper, ESPMode::ThreadSafe> AsyncHelper);

	/*
	 * Return the first translator that can translate the source data.
	 * @Param SourceData - The source data for which we search a translator.
	 * @return return a matching translator or nullptr if there is no match.
	 */
	UInterchangeTranslatorBase* GetTranslatorForSourceData(const UInterchangeSourceData* SourceData) const;

	/**
	 * Return true if the Interchange is active (importing or exporting), return false otherwise.
	 */
	bool IsInterchangeActive();

	/**
	 * Return false if the Interchange is not active (importing or exporting).
	 * If the interchange is active, it will display a notification to let the user know they can cancel the asynchronous import/export
	 * to be able to complete the operation they want to do. (The exit editor operation is calling this)
	 */
	bool WarnIfInterchangeIsActive();

	/**
	 * Look if there is a translator registered that can translate the source data with the specified PayloadInterface
	 * @Param SourceData - The source data input we want to translate to Uod
	 * @return true if the source data can be translated using the specified PayloadInterface, false otherwise.
	 */
	bool CanTranslateSourceDataWithPayloadInterface(const UInterchangeSourceData* SourceData, const UClass* PayloadInterfaceClass) const;

	/*
	 * Return the first translator that can translate the source data with the specified PayloadInterface.
	 * @Param SourceData - The source data for which we search a translator.
	 * @Param PayloadInterfaceClass - The PayloadInterface that the translator must implement.
	 * @return return a matching translator implementing the specified PayloadInterface or nullptr if there is no match.
	 */
	UInterchangeTranslatorBase* GetTranslatorSupportingPayloadInterfaceForSourceData(const UInterchangeSourceData* SourceData, const UClass* PayloadInterfaceClass) const;

protected:

	/** Return true if we can show some UI */
	static bool IsAttended();

	/*
	 * Find all Pipeline candidate (c++, blueprint and python).
	 * @Param SourceData - The source data for which we search a translator.
	 * @return return a matching translator or nullptr if there is no match.
	 */
	void FindPipelineCandidate(TArray<UClass*>& PipelineCandidates);

	/**
	 * This function cancel all task and finish them has fast as possible.
	 * We use this if the user cancel the work or if the editor is exiting.
	 * @note - This is a blocking call until the tasks are completed.
	 */
	void CancelAllTasks();

	/**
	 * Same has CancelAllTasks, but will wait all task are done before exiting the function
	 */
	void CancelAllTasksSynchronously();

	/**
	 * If we set the mode to active we will setup the timer and add the thread that will block the GC.
	 * If the we set the mode to inactive we will remove the timer and finish the thread that block the GC.
	 */
	void SetActiveMode(bool IsActive);

	/**
	 * Start task until we reach the taskgraph worker number.
	 * @param bCancelAllTasks - If true we will start all task but with the cancel state set, so task will complete fast and call the completion task
	 */
	void StartQueuedTasks(bool bCancelAllTasks = false);

	/**
	 * Called by the public Import functions
	 */
	TTuple<UE::Interchange::FAssetImportResultRef, UE::Interchange::FSceneImportResultRef>
	ImportInternal(const FString& ContentPath, const UInterchangeSourceData* SourceData, const FImportAssetParameters& ImportAssetParameters, const UE::Interchange::EImportType ImportType);

private:
	struct FQueuedTaskData
	{
		FString PackageBasePath;
		TSharedPtr<UE::Interchange::FImportAsyncHelper, ESPMode::ThreadSafe> AsyncHelper;
	};
	
	//Queue all incomming tasks if there is more started task then we have cores
	TQueue<FQueuedTaskData> QueuedTasks;
	int32 QueueTaskCount = 0;

	//By using pointer, there is no issue if the array get resize
	TArray<TSharedPtr<UE::Interchange::FImportAsyncHelper, ESPMode::ThreadSafe> > ImportTasks;

	TSharedPtr<FAsyncTaskNotification> Notification = nullptr;
	FTSTicker::FDelegateHandle NotificationTickHandle;

	// Caching the registered translator classes to avoid double registration fast
	UPROPERTY()
	TSet<TObjectPtr<const UClass>> RegisteredTranslatorsClass;

	//The manager will create translator at every import, translator must be able to retrieve payload information when the factory ask for it.
	//The translator stored has value is only use to know if we can use this type of translator.
//	UPROPERTY()
//	TArray<TObjectPtr<UInterchangeTranslatorBase>> RegisteredTranslators;
	
	//The manager will create only one pipeline per type
//	UPROPERTY()
//	TMap<TObjectPtr<const UClass>, TObjectPtr<UInterchangePipelineBase> > RegisteredPipelines;

	//The manager will create only one factory per type
	UPROPERTY()
	TMap<TObjectPtr<const UClass>, TObjectPtr<const UClass> > RegisteredFactoryClasses;

	//The manager will create only one writer per type
	UPROPERTY()
	TMap<TObjectPtr<const UClass>, TObjectPtr<UInterchangeWriterBase> > RegisteredWriters;

	//If interchange is currently importing we have a timer to watch the cancel and we block GC 
	FThreadSafeBool bIsActive = false;

	//If the user wants to use the same import pipeline stack for all the queue task
	//This boolean is reset to false when the ImportTasks array is empty.
	bool bImportAllWithDefault = false;

	//Indicates that the import process was canceled by the user.
	//This boolean is reset to false when the ImportTasks array is empty.
	bool bImportCanceled = false;

	friend class UE::Interchange::FScopedTranslator;
};
