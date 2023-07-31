// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeManager.h"

#include "AssetRegistry/AssetDataTagMap.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Async/Async.h"
#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "EngineAnalytics.h"
#include "Framework/Notifications/NotificationManager.h"
#include "InterchangeAssetImportData.h"
#include "InterchangeBlueprintPipelineBase.h"
#include "InterchangeFactoryBase.h"
#include "InterchangeEngineLogPrivate.h"
#include "InterchangeProjectSettings.h"
#include "InterchangePythonPipelineBase.h"
#include "InterchangeSourceData.h"
#include "InterchangeTranslatorBase.h"
#include "InterchangeWriterBase.h"
#include "Internationalization/Internationalization.h"
#include "Misc/App.h"
#include "Misc/AssertionMacros.h"
#include "Misc/AsyncTaskNotification.h"
#include "Misc/MessageDialog.h"
#include "Misc/ScopedSlowTask.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "PackageUtils/PackageUtils.h"
#include "Tasks/InterchangeTaskParsing.h"
#include "Tasks/InterchangeTaskPipeline.h"
#include "Tasks/InterchangeTaskTranslator.h"
#include "UObject/Class.h"
#include "UObject/GarbageCollection.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ReflectedTypeAccessors.h"
#include "UObject/UObjectIterator.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/Notifications/SNotificationList.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeManager)

static bool GInterchangeImportEnable = true;
static FAutoConsoleVariableRef CCvarInterchangeImportEnable(
	TEXT("Interchange.FeatureFlags.Import.Enable"),
	GInterchangeImportEnable,
	TEXT("Whether Interchange import is enabled."),
	ECVF_Default);

namespace UE::Interchange::Private
{
	const FLogCategoryBase* GetLogInterchangePtr()
	{
#if NO_LOGGING
		return nullptr;
#else
		return &LogInterchangeEngine;
#endif
	}

	FInterchangeImportSettings& GetMutableImportSettings(UInterchangeProjectSettings& InterchangeProjectSettings, EImportType ImportType)
	{
		return const_cast<FInterchangeImportSettings&>(FInterchangeProjectSettingsUtils::GetImportSettings(InterchangeProjectSettings, ImportType == EImportType::ImportType_Scene));
	}

	void FillPipelineAnalyticData(UInterchangePipelineBase* Pipeline, const int32 UniqueId, const FString& ParentPipeline)
	{
		if (!FEngineAnalytics::IsAvailable())
		{
			return;
		}
		
		int32 PortFlags = 0;
		UClass* Class = Pipeline->GetClass();
		FString PipelineChainName = ParentPipeline.IsEmpty() ? Pipeline->GetName() : ParentPipeline + TEXT(".") + Pipeline->GetName();

		TArray<FAnalyticsEventAttribute> PipelineAttribs;
		PipelineAttribs.Add(FAnalyticsEventAttribute(TEXT("UniqueId"), UniqueId));
		PipelineAttribs.Add(FAnalyticsEventAttribute(TEXT("Name"), PipelineChainName));
		PipelineAttribs.Add(FAnalyticsEventAttribute(TEXT("Class"), Class->GetName()));

		for (FProperty* Property = Class->PropertyLink; Property; Property = Property->PropertyLinkNext)
		{
			if (Property->HasAnyPropertyFlags(CPF_Transient))
			{
				continue;
			}

			if (Property->GetFName() == UInterchangePipelineBase::GetPropertiesStatesPropertyName())
			{
				continue;
			}

			if (Property->GetFName() == UInterchangePipelineBase::GetResultsPropertyName())
			{
				continue;
			}

			const FString PropertyName = Property->GetName();
			FObjectProperty* SubObject = CastField<FObjectProperty>(Property);
			UInterchangePipelineBase* SubPipeline = SubObject ? Cast<UInterchangePipelineBase>(SubObject->GetObjectPropertyValue_InContainer(Pipeline)) : nullptr;
			//Add the category name to the key
			FString CategoryName = FString();
#if WITH_EDITORONLY_DATA
			CategoryName = Property->GetMetaData("Category");
			if (!SubPipeline && CategoryName.IsEmpty())
			{
				//In Editor do not add property with no category
				continue;
			}
			CategoryName.ReplaceCharInline(TEXT('.'), TEXT('_'));
			CategoryName.RemoveSpacesInline();
			CategoryName = TEXT(".") + CategoryName;
#endif

			
			if (FArrayProperty* Array = CastField<FArrayProperty>(Property))
			{
				FScriptArrayHelper_InContainer ArrayHelper(Array, Pipeline);
				for (int32 i = 0; i < ArrayHelper.Num(); i++)
				{
					FString	Buffer;
					Array->Inner->ExportTextItem_Direct(Buffer, ArrayHelper.GetRawPtr(i), ArrayHelper.GetRawPtr(i), Pipeline, PortFlags);
					
					PipelineAttribs.Add(FAnalyticsEventAttribute(FString::Printf(TEXT("Property%s.%s"), *CategoryName, *PropertyName), Buffer));
				}
			}
			else if (SubPipeline)
			{
				// Save the settings if the referenced pipeline is a subobject of ours
				if (SubPipeline->IsInOuter(Pipeline))
				{
					//Go recursive with subObject, like if they are part of the same object
					FillPipelineAnalyticData(SubPipeline, UniqueId, PipelineChainName);
				}
			}
			else
			{
				for (int32 Index = 0; Index < Property->ArrayDim; Index++)
				{
					FString PropertyIndexName = PropertyName;
					if (Property->ArrayDim != 1)
					{
						PropertyIndexName += TEXT("[") + FString::FromInt(Index) + TEXT("]");
					}

					FString	Value;
					Property->ExportText_InContainer(Index, Value, Pipeline, Pipeline, Pipeline, PortFlags);
					PipelineAttribs.Add(FAnalyticsEventAttribute(FString::Printf(TEXT("Property%s.%s"), *CategoryName, *PropertyIndexName), Value));
				}
			}
		}
		
		FString EventString = TEXT("Interchange.Usage.Import.Pipeline");
		FEngineAnalytics::GetProvider().RecordEvent(EventString, PipelineAttribs);
	}
}

UE::Interchange::FScopedInterchangeImportEnableState::FScopedInterchangeImportEnableState(const bool bScopeValue)
{
	bOriginalInterchangeImportEnableState = CCvarInterchangeImportEnable->GetBool();
	CCvarInterchangeImportEnable->Set(bScopeValue);
}

UE::Interchange::FScopedInterchangeImportEnableState::~FScopedInterchangeImportEnableState()
{
	CCvarInterchangeImportEnable->Set(bOriginalInterchangeImportEnableState);
}

UE::Interchange::FScopedSourceData::FScopedSourceData(const FString& Filename)
{
	//Found the translator
	SourceDataPtr = TStrongObjectPtr<UInterchangeSourceData>(UInterchangeManager::GetInterchangeManager().CreateSourceData(Filename));
	check(SourceDataPtr.IsValid());
}

UInterchangeSourceData* UE::Interchange::FScopedSourceData::GetSourceData() const
{
	return SourceDataPtr.Get();
}

UE::Interchange::FScopedTranslator::FScopedTranslator(const UInterchangeSourceData* SourceData)
{
	//Found the translator
	ScopedTranslatorPtr = TStrongObjectPtr<UInterchangeTranslatorBase>(UInterchangeManager::GetInterchangeManager().GetTranslatorForSourceData(SourceData));
}

UInterchangeTranslatorBase* UE::Interchange::FScopedTranslator::GetTranslator()
{
	return ScopedTranslatorPtr.Get();
}

UE::Interchange::FImportAsyncHelper::FImportAsyncHelper()
	: AssetImportResult(MakeShared<FImportResult>())
	, SceneImportResult(MakeShared<FImportResult>())
{
	bCancel = false;
}

void UE::Interchange::FImportAsyncHelper::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(SourceDatas);
	Collector.AddReferencedObjects(Translators);
	Collector.AddReferencedObjects(Pipelines);
	Collector.AddReferencedObjects(CreatedFactories);
}

void UE::Interchange::FImportAsyncHelper::SendAnalyticImportEndData()
{
	if (!FEngineAnalytics::IsAvailable())
	{
		return;
	}

	TArray<FAnalyticsEventAttribute> Attribs;
	//Set the unique id of this import
	Attribs.Add(FAnalyticsEventAttribute(TEXT("UniqueId"), UniqueId));
	Attribs.Add(FAnalyticsEventAttribute(TEXT("IsCanceled"), bCancel));
	if (bCancel)
	{
		return;
	}

	int32 ImportedObjectCount = 0;
	for (const TPair<int32, TArray<FImportedObjectInfo>>& SourceIndexAndImportedAssets : ImportedAssetsPerSourceIndex)
	{
		ImportedObjectCount += SourceIndexAndImportedAssets.Value.Num();
	}

	for (const TPair<int32, TArray<FImportedObjectInfo>>& SourceIndexAndImportedScneObjects : ImportedSceneObjectsPerSourceIndex)
	{
		ImportedObjectCount += SourceIndexAndImportedScneObjects.Value.Num();
	}

	Attribs.Add(FAnalyticsEventAttribute(TEXT("ImportObjectCount"), ImportedObjectCount));

	//Report any warning or error message
	TArray<FString> WarningMessages;
	TArray<FString> ErrorMessages;
	auto CollectResultContainer = [&WarningMessages, &ErrorMessages](const UInterchangeResultsContainer* ResultContainer)
	{
		TArray<UInterchangeResult*> InterchangeResults = ResultContainer->GetResults();
		
		for (const UInterchangeResult* InterchangeResult : InterchangeResults)
		{
			switch (InterchangeResult->GetResultType() )
			{
			case EInterchangeResultType::Success:
				break;
			case EInterchangeResultType::Warning:
				WarningMessages.Add(TEXT("{") + InterchangeResult->GetText().ToString() + TEXT("}"));
				break;
			case EInterchangeResultType::Error:
				ErrorMessages.Add(TEXT("{") + InterchangeResult->GetText().ToString() + TEXT("}"));
				break;
			}
		}
		
	};

	if (const UInterchangeResultsContainer* ResultContainer = AssetImportResult->GetResults())
	{
		CollectResultContainer(ResultContainer);
	}
	if (const UInterchangeResultsContainer* ResultContainer = SceneImportResult->GetResults())
	{
		CollectResultContainer(ResultContainer);
	}

	if (WarningMessages.Num() > 0)
	{
		Attribs.Add(FAnalyticsEventAttribute(TEXT("WarningMessages"), WarningMessages));
	}
	if (ErrorMessages.Num() > 0)
	{
		Attribs.Add(FAnalyticsEventAttribute(TEXT("ErrorMessages"), ErrorMessages));
	}

	FString EventString = TEXT("Interchange.Usage.ImportResult");
	FEngineAnalytics::GetProvider().RecordEvent(EventString, Attribs);
}

void UE::Interchange::FImportAsyncHelper::ReleaseTranslatorsSource()
{
	for (UInterchangeTranslatorBase* BaseTranslator : Translators)
	{
		if (BaseTranslator)
		{
			BaseTranslator->ReleaseSource();
		}
	}
}

void UE::Interchange::FImportAsyncHelper::InitCancel()
{
	bCancel = true;
	ReleaseTranslatorsSource();
}

void UE::Interchange::FImportAsyncHelper::CancelAndWaitUntilDoneSynchronously()
{
	bCancel = true;

	FGraphEventArray TasksToComplete;

	TasksToComplete.Append(TranslatorTasks);
	TasksToComplete.Append(PipelinePreImportTasks);
	
	if (ParsingTask.GetReference())
	{
		TasksToComplete.Add(ParsingTask);
	}

	TasksToComplete.Append(CreatePackageTasks);
	TasksToComplete.Append(CreateAssetTasks);
	TasksToComplete.Append(SceneTasks);
	TasksToComplete.Append(PipelinePostImportTasks);

	if (PreAsyncCompletionTask.GetReference())
	{
		TasksToComplete.Add(PreAsyncCompletionTask);
	}
	if (PreCompletionTask.GetReference())
	{
		TasksToComplete.Add(PreCompletionTask);
	}
	if (CompletionTask.GetReference())
	{
		//Completion task will make sure any created asset before canceling will be mark for delete
		TasksToComplete.Add(CompletionTask);
	}

	//Block until all task are completed, it should be fast since bCancel is true
	if (TasksToComplete.Num())
	{
		FTaskGraphInterface::Get().WaitUntilTasksComplete(TasksToComplete, ENamedThreads::GameThread);
	}

	AssetImportResult->SetDone();
	SceneImportResult->SetDone();
}

void UE::Interchange::FImportAsyncHelper::CleanUp()
{
	//Release the graph
	BaseNodeContainers.Empty();

	for (UInterchangeSourceData* SourceData : SourceDatas)
	{
		if (SourceData)
		{
			SourceData->RemoveFromRoot();
			SourceData->MarkAsGarbage();
		}
	}
	SourceDatas.Empty();

	for (UInterchangeTranslatorBase* Translator : Translators)
	{
		if(Translator)
		{
			Translator->ImportFinish();
			Translator->RemoveFromRoot();
			Translator->MarkAsGarbage();
		}
	}
	Translators.Empty();

	for (UInterchangePipelineBase* Pipeline : Pipelines)
	{
		if(Pipeline)
		{
			Pipeline->RemoveFromRoot();
			Pipeline->MarkAsGarbage();
		}
	}
	Pipelines.Empty();

	for (const TPair<FString, UInterchangeFactoryBase*>& FactoryKeyAndValue : CreatedFactories)
	{
		if (FactoryKeyAndValue.Value)
		{
			FactoryKeyAndValue.Value->RemoveFromRoot();
			FactoryKeyAndValue.Value->MarkAsGarbage();
		}
	}
	CreatedFactories.Empty();
}

UE::Interchange::FImportResult::FImportResult()
	: ImportStatus(EStatus::Invalid)
{
	Results = NewObject<UInterchangeResultsContainer>(GetTransientPackage());
}

UE::Interchange::FImportResult::EStatus UE::Interchange::FImportResult::GetStatus() const
{
	return ImportStatus;
}

bool UE::Interchange::FImportResult::IsValid() const
{
	return GetStatus() != EStatus::Invalid;
}

void UE::Interchange::FImportResult::SetInProgress()
{
	EStatus ExpectedStatus = EStatus::Invalid;
	if (ImportStatus.compare_exchange_strong(ExpectedStatus, EStatus::InProgress))
	{
		GraphEvent = FGraphEvent::CreateGraphEvent();
	}
}

void UE::Interchange::FImportResult::SetDone()
{
	SetInProgress(); // Make sure we always pass through the InProgress state

	EStatus ExpectedStatus = EStatus::InProgress;
	if (ImportStatus.compare_exchange_strong(ExpectedStatus, EStatus::Done))
	{
		if (DoneCallback)
		{
			DoneCallback(*this);
		}

		TArray<UObject*> Objects = GetImportedObjects();

		if (IsInGameThread())
		{
			OnImportDoneNative.ExecuteIfBound(Objects);
			OnImportDone.ExecuteIfBound(Objects);
		}
		else
		{
			TArray<TWeakObjectPtr<UObject>> WeakObjects;
			WeakObjects.Reserve(Objects.Num());
			for (UObject* Object : Objects)
			{
				WeakObjects.Emplace(Object);
			}

			// call the callbacks on the game thread
			Async(EAsyncExecution::TaskGraphMainThread, [InWeakObjects = MoveTemp(WeakObjects), ImportDoneNative = OnImportDoneNative, ImportDone = OnImportDone]()
			{
				TArray<UObject*> ValidObjects;
				ValidObjects.Reserve(InWeakObjects.Num());

				for (const TWeakObjectPtr<UObject>& WeakObject : InWeakObjects)
				{
					if (UObject* ValidObject = WeakObject.Get())
					{
						ValidObjects.Add(ValidObject);
					}
				}

				ImportDoneNative.ExecuteIfBound(ValidObjects);
				ImportDone.ExecuteIfBound(ValidObjects);
			});
		}


		GraphEvent->DispatchSubsequents();
	}
}

void UE::Interchange::FImportResult::WaitUntilDone()
{
	if (ImportStatus == EStatus::InProgress)
	{
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(GraphEvent);
	}
}

const TArray< UObject* >& UE::Interchange::FImportResult::GetImportedObjects() const
{
	FReadScopeLock ReadScopeLock(ImportedObjectsRWLock);
	return ImportedObjects;
}

UObject* UE::Interchange::FImportResult::GetFirstAssetOfClass(UClass* InClass) const
{
	UObject* Asset = nullptr;

	FReadScopeLock ReadScopeLock(ImportedObjectsRWLock);
	for (UObject* ImportedAsset : ImportedObjects)
	{
		if (ImportedAsset->IsA(InClass))
		{
			Asset = ImportedAsset;
			break;
		}
	}

	return Asset;
}

void UE::Interchange::FImportResult::AddImportedObject(UObject* ImportedObject)
{
	{
		FWriteScopeLock WriteScopeLock(ImportedObjectsRWLock);
		ImportedObjects.Add(ImportedObject);
	}

	if (IsInGameThread())
	{
		OnObjectDoneNative.ExecuteIfBound(ImportedObject);
		OnObjectDone.ExecuteIfBound(ImportedObject);
	}
	else
	{
		// call the callbacks on the game thread
		Async(EAsyncExecution::TaskGraphMainThread, [WeakImportedObject = TWeakObjectPtr<UObject>(ImportedObject), ObjectDoneNative = OnObjectDoneNative, ObjectDone = OnObjectDone] ()
			{
				if (UObject* ImportedObjectInGameThread = WeakImportedObject.Get())
				{
					ObjectDoneNative.ExecuteIfBound(ImportedObjectInGameThread);
					ObjectDone.ExecuteIfBound(ImportedObjectInGameThread);
				}
			});
	}
}

void UE::Interchange::FImportResult::OnDone(TFunction< void(FImportResult&) > Callback)
{
	DoneCallback = Callback;
}

void UE::Interchange::FImportResult::AddReferencedObjects(FReferenceCollector& Collector)
{
	FReadScopeLock ReadScopeLock(ImportedObjectsRWLock);
	Collector.AddReferencedObjects(ImportedObjects);
	Collector.AddReferencedObject(Results);
}

void UE::Interchange::SanitizeObjectPath(FString& ObjectPath)
{
	const TCHAR* InvalidChar = INVALID_OBJECTPATH_CHARACTERS;
	while (*InvalidChar)
	{
		ObjectPath.ReplaceCharInline(*InvalidChar, TCHAR('_'), ESearchCase::CaseSensitive);
		++InvalidChar;
	}
}

void UE::Interchange::SanitizeObjectName(FString& ObjectName)
{
	const TCHAR* InvalidChar = INVALID_OBJECTNAME_CHARACTERS;
	while (*InvalidChar)
	{
		ObjectName.ReplaceCharInline(*InvalidChar, TCHAR('_'), ESearchCase::CaseSensitive);
		++InvalidChar;
	}
}

UInterchangePipelineBase* UE::Interchange::GeneratePipelineInstance(const FSoftObjectPath& PipelineInstance)
{
	UObject* ReferenceInstance = PipelineInstance.TryLoad();
	if (!ReferenceInstance)
	{
		return nullptr;
	}
	UInterchangePipelineBase* GeneratedPipeline = nullptr;
	if (const UInterchangeBlueprintPipelineBase* BlueprintPipeline = Cast<UInterchangeBlueprintPipelineBase>(ReferenceInstance))
	{
		if (BlueprintPipeline->GeneratedClass.Get())
		{
			GeneratedPipeline = NewObject<UInterchangePipelineBase>(GetTransientPackage(), BlueprintPipeline->GeneratedClass);
		}
		else
		{
			//Log an error because we cannot load the python class, maybe the python script was not loaded
			UE_LOG(LogInterchangeEngine, Error, TEXT("Cannot generate a pipeline instance because the blueprint %s do not have a valid generated class."), *PipelineInstance.GetWithoutSubPath().ToString());
		}
	}
	else if (const UInterchangePythonPipelineAsset* PythonPipeline = Cast<UInterchangePythonPipelineAsset>(ReferenceInstance))
	{
		if (PythonPipeline->GeneratedPipeline)
		{
			GeneratedPipeline = DuplicateObject<UInterchangePipelineBase>(PythonPipeline->GeneratedPipeline.Get(), GetTransientPackage());
		}
		else
		{
			//Log an error because we cannot load the python class, maybe the python script was not loaded
			UE_LOG(LogInterchangeEngine, Error, TEXT("Cannot generate a pipeline instance because the Python pipeline asset %s do not have a valid generated pipeline instance."), *PipelineInstance.GetWithoutSubPath().ToString());
		}
	}
	else if (const UInterchangePipelineBase* DefaultPipeline = Cast<UInterchangePipelineBase>(ReferenceInstance))
	{
		GeneratedPipeline = DuplicateObject<UInterchangePipelineBase>(DefaultPipeline, GetTransientPackage());
	}
	else
	{
		UE_LOG(LogInterchangeEngine, Error, TEXT("Cannot generate a pipeline instance because the pipeline asset %s type is unknown."), *PipelineInstance.GetWithoutSubPath().ToString());
	}

	// Make sure that the instance does not carry over standalone and public flags as they are not actual assets to be persisted
	GeneratedPipeline->ClearFlags(EObjectFlags::RF_Standalone|EObjectFlags::RF_Public);

	return GeneratedPipeline;
}

UInterchangeManager& UInterchangeManager::GetInterchangeManager()
{
	static TStrongObjectPtr<UInterchangeManager> InterchangeManager = nullptr;
	
	//This boolean will be true after we delete the singleton
	static bool InterchangeManagerScopeOfLifeEnded = false;

	if (!InterchangeManager.IsValid())
	{
		//We cannot create a TStrongObjectPtr outside of the main thread, we also need a valid Transient package
		check(IsInGameThread() && GetTransientPackage());

		//Avoid hard crash if someone call the manager after we delete it, but send a callstack to the crash manager
		ensure(!InterchangeManagerScopeOfLifeEnded);

		InterchangeManager = TStrongObjectPtr<UInterchangeManager>(NewObject<UInterchangeManager>(GetTransientPackage(), NAME_None, EObjectFlags::RF_NoFlags));
		
		//We cancel any running task when we pre exit the engine
		FCoreDelegates::OnEnginePreExit.AddLambda([]()
		{
			//In editor the user cannot exit the editor if the interchange manager has active task.
			//But if we are not running the editor its possible to get here, so block the main thread until all
			//cancel tasks are done.
			if(GIsEditor)
			{
				ensure(InterchangeManager->ImportTasks.Num() == 0);
			}
			else
			{
				InterchangeManager->CancelAllTasksSynchronously();
			}
			//Task should have been cancel in the Engine pre exit callback
			ensure(InterchangeManager->ImportTasks.Num() == 0);
			InterchangeManager->OnPreDestroyInterchangeManager.Broadcast();
			//Release the InterchangeManager object
			InterchangeManager.Reset();
			InterchangeManagerScopeOfLifeEnded = true;
		});
	}

	//When we get here we should be valid
	check(InterchangeManager.IsValid());

	return *(InterchangeManager.Get());
}

bool UInterchangeManager::RegisterTranslator(const UClass* TranslatorClass)
{
	if (!TranslatorClass)
	{
		return false;
	}

	RegisteredTranslatorsClass.Add(TranslatorClass);
	return true;
}

bool UInterchangeManager::RegisterFactory(const UClass* FactoryClass)
{
	if (!FactoryClass || !FactoryClass->IsChildOf<UInterchangeFactoryBase>())
	{
		return false;
	}

	UClass* ClassToMake = FactoryClass->GetDefaultObject<UInterchangeFactoryBase>()->GetFactoryClass();
	if (ClassToMake)
	{
		if (!RegisteredFactoryClasses.Contains(ClassToMake))
		{
			RegisteredFactoryClasses.Add(ClassToMake, FactoryClass);
		}

		return true;
	}

	return false;
}

bool UInterchangeManager::RegisterWriter(const UClass* WriterClass)
{
#if WITH_EDITOR
	if (!WriterClass)
	{
		return false;
	}

	if (RegisteredWriters.Contains(WriterClass))
	{
		return true;
	}
	UInterchangeWriterBase* WriterToRegister = NewObject<UInterchangeWriterBase>(GetTransientPackage(), WriterClass, NAME_None);
	if (!WriterToRegister)
	{
		return false;
	}
	RegisteredWriters.Add(WriterClass, WriterToRegister);
#endif
	return true;
}

TArray<FString> UInterchangeManager::GetSupportedFormats(const EInterchangeTranslatorType ForTranslatorType) const
{
	TArray<FString> FileExtensions;
	if (!IsInterchangeImportEnabled())
	{
		return FileExtensions;
	}

	for (const UClass* TranslatorClass : RegisteredTranslatorsClass)
	{
		const UInterchangeTranslatorBase* TranslatorBaseCDO = TranslatorClass->GetDefaultObject<UInterchangeTranslatorBase>();

		if (EnumHasAllFlags(TranslatorBaseCDO->GetTranslatorType(), ForTranslatorType))
		{
			FileExtensions.Append(TranslatorBaseCDO->GetSupportedFormats());
		}
	}

	return FileExtensions;
}

TArray<FString> UInterchangeManager::GetSupportedAssetTypeFormats(const EInterchangeTranslatorAssetType ForTranslatorAssetType) const
{
	TArray<FString> FileExtensions;
	if (!IsInterchangeImportEnabled())
	{
		return FileExtensions;
	}

	for (const UClass* TranslatorClass : RegisteredTranslatorsClass)
	{
		const UInterchangeTranslatorBase* TranslatorBaseCDO = TranslatorClass->GetDefaultObject<UInterchangeTranslatorBase>();

		if (TranslatorBaseCDO->DoesSupportAssetType(ForTranslatorAssetType))
		{
			FileExtensions.Append(TranslatorBaseCDO->GetSupportedFormats());
		}
	}

	return FileExtensions;
}

TArray<FString> UInterchangeManager::GetSupportedFormatsForObject(const UObject* Object) const
{
	TArray<FString> FileExtensions;
	if (!IsInterchangeImportEnabled())
	{
		return FileExtensions;
	}

	const UClass* RegisteredFactoryClass = GetRegisteredFactoryClass(Object->GetClass());
	if (!RegisteredFactoryClass)
	{
		return FileExtensions;
	}

	UInterchangeFactoryBase* Factory = RegisteredFactoryClass->GetDefaultObject<UInterchangeFactoryBase>();
	TArray<FString> TempFilenames;
	//GetSourceFilenames verify we have a valid UInterchangeAssetImportData for this Object
	//This ensure we do not allow re-import
	if (!Factory->GetSourceFilenames(Object, TempFilenames))
	{
		return FileExtensions;
	}

	switch (Factory->GetFactoryAssetType())
	{
	case EInterchangeFactoryAssetType::Animations:
		FileExtensions = GetSupportedAssetTypeFormats(EInterchangeTranslatorAssetType::Animations);
		break;
	case EInterchangeFactoryAssetType::Materials:
		FileExtensions = GetSupportedAssetTypeFormats(EInterchangeTranslatorAssetType::Materials);
		break;
	case EInterchangeFactoryAssetType::Meshes:
	case EInterchangeFactoryAssetType::Physics:
		FileExtensions = GetSupportedAssetTypeFormats(EInterchangeTranslatorAssetType::Meshes);
		break;
	case EInterchangeFactoryAssetType::Textures:
		FileExtensions = GetSupportedAssetTypeFormats(EInterchangeTranslatorAssetType::Textures);
		break;
	case EInterchangeFactoryAssetType::None: //Actor factories return None
		FileExtensions = GetSupportedFormats(EInterchangeTranslatorType::Actors);
		break;
	}

	return FileExtensions;
}

bool UInterchangeManager::CanTranslateSourceData(const UInterchangeSourceData* SourceData) const
{
	if (!IsInterchangeImportEnabled())
	{
		return false;
	}

	return GetTranslatorForSourceData(SourceData) != nullptr;
}

bool UInterchangeManager::CanReimport(const UObject* Object, TArray<FString>& OutFilenames) const
{
	if (!IsInterchangeImportEnabled())
	{
		return false;
	}

	const UClass* RegisteredFactoryClass = GetRegisteredFactoryClass(Object->GetClass());
	if (!RegisteredFactoryClass)
	{
		return false;
	}

	UInterchangeFactoryBase* Factory = RegisteredFactoryClass->GetDefaultObject<UInterchangeFactoryBase>();
	if(!Factory->GetSourceFilenames(Object, OutFilenames))
	{
		return false;
	}

	for (const FString& Filename : OutFilenames)
	{
		UE::Interchange::FScopedSourceData ScopedSourceData(Filename);

		if (CanTranslateSourceData(ScopedSourceData.GetSourceData()))
		{
			return true;
		}
	}

	OutFilenames.Empty();
	return false;
}

void UInterchangeManager::StartQueuedTasks(bool bCancelAllTasks /*= false*/)
{
	TRACE_CPUPROFILER_EVENT_SCOPE("UInterchangeManager::StartQueuedTasks")
	ensure(IsInterchangeImportEnabled());
	if (!ensure(IsInGameThread()))
	{
		//Do not crash but we will not start any queued tasks if we are not in the game thread
		return;
	}

	auto UpdateNotification = [this]()
	{
		if (Notification.IsValid())
		{
			int32 ImportTaskNumber = ImportTasks.Num() + QueueTaskCount;
			FString ImportTaskNumberStr = TEXT(" (") + FString::FromInt(ImportTaskNumber) + TEXT(")");
			Notification->SetProgressText(FText::FromString(ImportTaskNumberStr));
		}
		else
		{
			FText TitleText = NSLOCTEXT("Interchange", "Asynchronous_import_start", "Importing");
			FAsyncTaskNotificationConfig NotificationConfig;
			NotificationConfig.bIsHeadless = false;
			NotificationConfig.bKeepOpenOnFailure = true;
			NotificationConfig.TitleText = TitleText;
			NotificationConfig.LogCategory = UE::Interchange::Private::GetLogInterchangePtr();
			NotificationConfig.bCanCancel.Set(true);
			NotificationConfig.bKeepOpenOnFailure.Set(true);

			Notification = MakeShared<FAsyncTaskNotification>(NotificationConfig);
			Notification->SetNotificationState(FAsyncNotificationStateData(TitleText, FText::GetEmpty(), EAsyncTaskNotificationState::Pending));
		}
	};

	//We need to leave some free task in the pool to avoid deadlock.
	//Each import can use 2 tasks in same time if the build of the asset ddc use the same task pool (i.e. staticmesh, skeletalmesh, texture...)
	const int32 PoolWorkerThreadCount = FTaskGraphInterface::Get().GetNumWorkerThreads()/2;
	const int32 MaxNumWorker = FMath::Max(PoolWorkerThreadCount, 1);
	while (!QueuedTasks.IsEmpty() && (ImportTasks.Num() < MaxNumWorker || bCancelAllTasks))
	{
		FQueuedTaskData QueuedTaskData;
		if (QueuedTasks.Dequeue(QueuedTaskData))
		{
			QueueTaskCount = FMath::Clamp(QueueTaskCount-1, 0, MAX_int32);
			check(QueuedTaskData.AsyncHelper.IsValid());

			int32 AsyncHelperIndex = ImportTasks.Add(QueuedTaskData.AsyncHelper);
			SetActiveMode(true);
			//Update the asynchronous notification
			UpdateNotification();

			TWeakPtr<UE::Interchange::FImportAsyncHelper, ESPMode::ThreadSafe> WeakAsyncHelper = QueuedTaskData.AsyncHelper;
			
			if (bCancelAllTasks)
			{
				QueuedTaskData.AsyncHelper->InitCancel();
			}

			//Create/Start import tasks
			FGraphEventArray PipelinePrerequistes;
			check(QueuedTaskData.AsyncHelper->Translators.Num() == QueuedTaskData.AsyncHelper->SourceDatas.Num());
			for (int32 SourceDataIndex = 0; SourceDataIndex < QueuedTaskData.AsyncHelper->SourceDatas.Num(); ++SourceDataIndex)
			{
				int32 TranslatorTaskIndex = QueuedTaskData.AsyncHelper->TranslatorTasks.Add(TGraphTask<UE::Interchange::FTaskTranslator>::CreateTask().ConstructAndDispatchWhenReady(SourceDataIndex, WeakAsyncHelper));
				PipelinePrerequistes.Add(QueuedTaskData.AsyncHelper->TranslatorTasks[TranslatorTaskIndex]);
			}

			FGraphEventArray GraphParsingPrerequistes;
			for (int32 GraphPipelineIndex = 0; GraphPipelineIndex < QueuedTaskData.AsyncHelper->Pipelines.Num(); ++GraphPipelineIndex)
			{
				UInterchangePipelineBase* GraphPipeline = QueuedTaskData.AsyncHelper->Pipelines[GraphPipelineIndex];
				TWeakObjectPtr<UInterchangePipelineBase> WeakPipelinePtr = GraphPipeline;
				int32 GraphPipelineTaskIndex = INDEX_NONE;
				GraphPipelineTaskIndex = QueuedTaskData.AsyncHelper->PipelinePreImportTasks.Add(TGraphTask<UE::Interchange::FTaskPipelinePreImport>::CreateTask(&PipelinePrerequistes).ConstructAndDispatchWhenReady(WeakPipelinePtr, WeakAsyncHelper));
				//Ensure we run the pipeline in the same order we create the task, since pipeline modify the node container, its important that its not process in parallel, Adding the one we start to the prerequisites
				//is the way to go here
				PipelinePrerequistes.Add(QueuedTaskData.AsyncHelper->PipelinePreImportTasks[GraphPipelineTaskIndex]);

				//Add pipeline to the graph parsing prerequisites
				GraphParsingPrerequistes.Add(QueuedTaskData.AsyncHelper->PipelinePreImportTasks[GraphPipelineTaskIndex]);
			}

			if (GraphParsingPrerequistes.Num() > 0)
			{
				QueuedTaskData.AsyncHelper->ParsingTask = TGraphTask<UE::Interchange::FTaskParsing>::CreateTask(&GraphParsingPrerequistes).ConstructAndDispatchWhenReady(this, QueuedTaskData.PackageBasePath, WeakAsyncHelper);
			}
			else
			{
				//Fallback on the translator pipeline prerequisites (translator must be done if there is no pipeline)
				QueuedTaskData.AsyncHelper->ParsingTask = TGraphTask<UE::Interchange::FTaskParsing>::CreateTask(&PipelinePrerequistes).ConstructAndDispatchWhenReady(this, QueuedTaskData.PackageBasePath, WeakAsyncHelper);
			}

			//The graph parsing task will create the FCreateAssetTask that will run after them, the FAssetImportTask will call the appropriate Post asset import pipeline when the asset is completed
		}
	}

	if (!QueuedTasks.IsEmpty())
	{
		//Make sure any task we add is count in the task to do, even if we cannot start it
		UpdateNotification();
	}
}

bool UInterchangeManager::ImportAsset(const FString& ContentPath, const UInterchangeSourceData* SourceData, const FImportAssetParameters& ImportAssetParameters)
{
	UE::Interchange::FAssetImportResultRef InterchangeResult = ImportAssetAsync(ContentPath, SourceData, ImportAssetParameters);
	InterchangeResult->WaitUntilDone();
	return InterchangeResult->IsValid();
}

UE::Interchange::FAssetImportResultRef UInterchangeManager::ImportAssetAsync(const FString& ContentPath, const UInterchangeSourceData* SourceData, const FImportAssetParameters& ImportAssetParameters)
{
	return ImportInternal(ContentPath, SourceData, ImportAssetParameters, UE::Interchange::EImportType::ImportType_Asset).Get<0>();
}

bool UInterchangeManager::ImportScene(const FString& ContentPath, const UInterchangeSourceData* SourceData, const FImportAssetParameters& ImportAssetParameters)
{
	using namespace UE::Interchange;
	TTuple<FAssetImportResultRef, FSceneImportResultRef> ImportResults = ImportInternal(ContentPath, SourceData, ImportAssetParameters, UE::Interchange::EImportType::ImportType_Scene);
	
	ImportResults.Get<0>()->WaitUntilDone();
	ImportResults.Get<1>()->WaitUntilDone();

	return ImportResults.Get<0>()->IsValid() && ImportResults.Get<1>()->IsValid();
}

TTuple<UE::Interchange::FAssetImportResultRef, UE::Interchange::FSceneImportResultRef>
UInterchangeManager::ImportSceneAsync(const FString& ContentPath, const UInterchangeSourceData* SourceData, const FImportAssetParameters& ImportAssetParameters)
{
	return ImportInternal(ContentPath, SourceData, ImportAssetParameters, UE::Interchange::EImportType::ImportType_Scene);
}

TTuple<UE::Interchange::FAssetImportResultRef, UE::Interchange::FSceneImportResultRef>
UInterchangeManager::ImportInternal(const FString& ContentPath, const UInterchangeSourceData* SourceData, const FImportAssetParameters& ImportAssetParameters, const UE::Interchange::EImportType ImportType)
{
	TRACE_CPUPROFILER_EVENT_SCOPE("UInterchangeManager::ImportInternal")
	ensure(IsInterchangeImportEnabled());
	check(IsInGameThread());
	static int32 GeneratedUniqueID = 0;
	int32 UniqueId = ++GeneratedUniqueID;

	TArray<FAnalyticsEventAttribute> Attribs;
	auto PreReturn = [&Attribs]()
	{
		if (FEngineAnalytics::IsAvailable())
		{
			FString EventString = TEXT("Interchange.Usage.Import");
			FEngineAnalytics::GetProvider().RecordEvent(EventString, Attribs);
		}
	};

	auto EarlyExit = [&PreReturn]()
	{
		PreReturn();
		return TTuple<UE::Interchange::FAssetImportResultRef, UE::Interchange::FSceneImportResultRef>{ MakeShared< UE::Interchange::FImportResult, ESPMode::ThreadSafe >(), MakeShared< UE::Interchange::FImportResult, ESPMode::ThreadSafe >() };
	};

	if (FEngineAnalytics::IsAvailable())
	{
		Attribs.Add(FAnalyticsEventAttribute(TEXT("UniqueId"), UniqueId));
		Attribs.Add(FAnalyticsEventAttribute(TEXT("ImportType"), ImportType == UE::Interchange::EImportType::ImportType_Asset ? TEXT("Asset") : TEXT("Scene")));
		Attribs.Add(FAnalyticsEventAttribute(TEXT("Parameters.IsAutomated"), ImportAssetParameters.bIsAutomated));
		const bool bIsReimport = ImportAssetParameters.ReimportAsset != nullptr;
		Attribs.Add(FAnalyticsEventAttribute(TEXT("Parameters.IsReimport"), bIsReimport));
		if (ImportAssetParameters.ReimportAsset != nullptr)
		{
			Attribs.Add(FAnalyticsEventAttribute(TEXT("Parameters.ReimportSourceIndex"), ImportAssetParameters.ReimportSourceIndex));
		}
		const bool bIsPipelineOverride = ImportAssetParameters.OverridePipelines.Num() > 0;
		Attribs.Add(FAnalyticsEventAttribute(TEXT("Parameters.IsPipelineOverrided"), bIsPipelineOverride));
	}

	if (!ensure(IsInGameThread()))
	{
		//Import process can be started only in the game thread
		return EarlyExit();
	}

	if (!SourceData)
	{
		UE_LOG(LogInterchangeEngine, Error, TEXT("Cannot import file, the source data is invalid"));
		return EarlyExit();
	}
	
	if (FEngineAnalytics::IsAvailable())
	{
		Attribs.Add(FAnalyticsEventAttribute(TEXT("SourceExtension"), FPaths::GetExtension(SourceData->GetFilename())));
	}

	const bool bImportScene = ImportType == UE::Interchange::EImportType::ImportType_Scene;
	const FInterchangeImportSettings& InterchangeImportSettings = FInterchangeProjectSettingsUtils::GetDefaultImportSettings(bImportScene);
	
	if (InterchangeImportSettings.PipelineStacks.Num() == 0)
	{
		UE_LOG(LogInterchangeEngine, Error, TEXT("Cannot import file, there is no pipeline stack define for %s import type. File path %s"), bImportScene ? TEXT("scene") : TEXT("content"));
		return EarlyExit();
	}
	
	//Set a default pipeline stack if none is valid
	if (!InterchangeImportSettings.PipelineStacks.Contains(InterchangeImportSettings.DefaultPipelineStack))
	{
		FInterchangeImportSettings& MutableInterchangeImportSettings = FInterchangeProjectSettingsUtils::GetMutableDefaultImportSettings(bImportScene);
		TArray<FName> Keys;
		MutableInterchangeImportSettings.PipelineStacks.GetKeys(Keys);
		check(Keys.Num() > 0);
		MutableInterchangeImportSettings.DefaultPipelineStack = Keys[0];
	}

	UInterchangeAssetImportData* OriginalAssetImportData = nullptr;
	FString PackageBasePath = ContentPath;
	if(!ImportAssetParameters.ReimportAsset)
	{
		UE::Interchange::SanitizeObjectPath(PackageBasePath);
	}
	else
	{
		PackageBasePath = FPaths::GetPath(ImportAssetParameters.ReimportAsset->GetPathName());
		TArray<UObject*> SubObjects;
		GetObjectsWithOuter(ImportAssetParameters.ReimportAsset, SubObjects);
		for (UObject* SubObject : SubObjects)
		{
			OriginalAssetImportData = Cast<UInterchangeAssetImportData>(SubObject);
			if(OriginalAssetImportData)
			{
				break;
			}
		}
	}
	bool bImportAborted = false; // True when we're unable to go through with the import process

	//Create a task for every source data
	UE::Interchange::FImportAsyncHelperData TaskData;
	TaskData.bIsAutomated = ImportAssetParameters.bIsAutomated;
	TaskData.ImportType = ImportType;
	TaskData.ReimportObject = ImportAssetParameters.ReimportAsset;
	TSharedRef<UE::Interchange::FImportAsyncHelper, ESPMode::ThreadSafe> AsyncHelper = CreateAsyncHelper(TaskData, ImportAssetParameters);
	AsyncHelper->UniqueId = UniqueId;

	//Create a duplicate of the source data, we need to be multithread safe so we copy it to control the life cycle. The async helper will hold it and delete it when the import task will be completed.
	UInterchangeSourceData* DuplicateSourceData = Cast<UInterchangeSourceData>(StaticDuplicateObject(SourceData, GetTransientPackage()));
	//Array of source data to build one graph per source
	AsyncHelper->SourceDatas.Add(DuplicateSourceData);

	//Get all the translators for the source datas
	for (int32 SourceDataIndex = 0; SourceDataIndex < AsyncHelper->SourceDatas.Num(); ++SourceDataIndex)
	{
		ensure(AsyncHelper->Translators.Add(GetTranslatorForSourceData(AsyncHelper->SourceDatas[SourceDataIndex])) == SourceDataIndex);
	}

	//Create the node graphs for each source data (StrongObjectPtr has to be created on the main thread)
	for (int32 SourceDataIndex = 0; SourceDataIndex < AsyncHelper->SourceDatas.Num(); ++SourceDataIndex)
	{
		AsyncHelper->BaseNodeContainers.Add(TStrongObjectPtr<UInterchangeBaseNodeContainer>(NewObject<UInterchangeBaseNodeContainer>(GetTransientPackage(), NAME_None)));
		check(AsyncHelper->BaseNodeContainers[SourceDataIndex].IsValid());
	}

	
	UInterchangePipelineConfigurationBase* RegisteredPipelineConfiguration = nullptr;

	//In runtime we do not have any pipeline configurator
#if WITH_EDITORONLY_DATA
	TSoftClassPtr <UInterchangePipelineConfigurationBase> PipelineConfigurationDialogClass = InterchangeImportSettings.PipelineConfigurationDialogClass;

	if (PipelineConfigurationDialogClass.IsValid())
	{
		UClass* PipelineConfigurationClass = PipelineConfigurationDialogClass.LoadSynchronous();
		if (PipelineConfigurationClass)
		{
			RegisteredPipelineConfiguration = NewObject<UInterchangePipelineConfigurationBase>(GetTransientPackage(), PipelineConfigurationClass, NAME_None, RF_NoFlags);
		}
	}
#endif


	if ( ImportAssetParameters.OverridePipelines.Num() == 0 )
	{
		const bool bIsUnattended = FApp::IsUnattended() || GIsAutomationTesting || ImportAssetParameters.bIsAutomated;

#if WITH_EDITORONLY_DATA
		const bool bShowPipelineStacksConfigurationDialog = !bIsUnattended
															&& FInterchangeProjectSettingsUtils::ShouldShowPipelineStacksConfigurationDialog(bImportScene, *SourceData)
															&& !bImportAllWithDefault
															&& !bImportCanceled;
#else
		const bool bShowPipelineStacksConfigurationDialog = false;
#endif

		if (FEngineAnalytics::IsAvailable())
		{
			Attribs.Add(FAnalyticsEventAttribute(TEXT("ShowImportDialog"), bShowPipelineStacksConfigurationDialog));
		}

		const TMap<FName, FInterchangePipelineStack>& DefaultPipelineStacks = InterchangeImportSettings.PipelineStacks;

		//If we reimport we want to load the original pipeline and the original pipeline settings
		if (OriginalAssetImportData && OriginalAssetImportData->Pipelines.Num() > 0)
		{
			TArray<UInterchangePipelineBase*> PipelineStack;
			for (int32 PipelineIndex = 0; PipelineIndex < OriginalAssetImportData->Pipelines.Num(); ++PipelineIndex)
			{
				UInterchangePipelineBase* SourcePipeline = Cast<UInterchangePipelineBase>(OriginalAssetImportData->Pipelines[PipelineIndex]);
				if (!SourcePipeline)
				{
					if (UInterchangePythonPipelineAsset* PythonPipelineAsset = Cast<UInterchangePythonPipelineAsset>(OriginalAssetImportData->Pipelines[PipelineIndex]))
					{
						SourcePipeline = PythonPipelineAsset->GeneratedPipeline;
					}
				}
				if (SourcePipeline) //Its possible a pipeline doesnt exist anymore so it wont load into memory when we loading the outer asset
				{
					//Duplicate the pipeline saved in the asset import data
					UInterchangePipelineBase* GeneratedPipeline = Cast<UInterchangePipelineBase>(StaticDuplicateObject(SourcePipeline, GetTransientPackage()));
					// Make sure that the instance does not carry over standalone and public flags as they are not actual assets to be persisted
					GeneratedPipeline->ClearFlags(EObjectFlags::RF_Standalone|EObjectFlags::RF_Public);
					if (bImportScene)
					{
						GeneratedPipeline->AdjustSettingsForContext(EInterchangePipelineContext::SceneReimport, nullptr);
					}
					else
					{
						GeneratedPipeline->AdjustSettingsForContext(EInterchangePipelineContext::AssetReimport, ImportAssetParameters.ReimportAsset);
					}
					PipelineStack.Add(GeneratedPipeline);
				}
				else
				{
					//A pipeline was not loaded
					//Log something
				}
			}

			if (RegisteredPipelineConfiguration && bShowPipelineStacksConfigurationDialog && !bIsUnattended)
			{
				//Show the dialog, a plugin should have registered this dialog. We use a plugin to be able to use editor code when doing UI
				EInterchangePipelineConfigurationDialogResult DialogResult = RegisteredPipelineConfiguration->ScriptedShowReimportPipelineConfigurationDialog(PipelineStack, DuplicateSourceData);
				if (DialogResult == EInterchangePipelineConfigurationDialogResult::Cancel)
				{
					bImportCanceled = true;
				}
				if (DialogResult == EInterchangePipelineConfigurationDialogResult::ImportAll)
				{
					bImportAllWithDefault = true;
				}
			}

			// Simply move the existing pipeline
			AsyncHelper->Pipelines = MoveTemp(PipelineStack);

			//Fill the original pipeline array that will be save in the asset import data
			for (UInterchangePipelineBase* Pipeline : AsyncHelper->Pipelines)
			{
				if (UInterchangePythonPipelineBase* PythonPipeline = Cast<UInterchangePythonPipelineBase>(Pipeline))
				{
					UInterchangePythonPipelineAsset* PythonPipelineAsset = NewObject<UInterchangePythonPipelineAsset>(GetTransientPackage());
					PythonPipelineAsset->PythonClass = PythonPipeline->GetClass();
					PythonPipelineAsset->SetupFromPipeline(PythonPipeline);
					AsyncHelper->OriginalPipelines.Add(PythonPipelineAsset);
				}
				else
				{
					AsyncHelper->OriginalPipelines.Add(Pipeline);
				}
				UE::Interchange::Private::FillPipelineAnalyticData(Pipeline, UniqueId, FString());
			}
		}
		else
		{
			FName PipelineStackName = FInterchangeProjectSettingsUtils::GetDefaultPipelineStackName(bImportScene, *SourceData);
			if (RegisteredPipelineConfiguration && (bShowPipelineStacksConfigurationDialog || (!DefaultPipelineStacks.Contains(PipelineStackName) && !bIsUnattended)))
			{
				//Show the dialog, a plugin should have register this dialog. We use a plugin to be able to use editor code when doing UI
				EInterchangePipelineConfigurationDialogResult DialogResult = bImportScene ? RegisteredPipelineConfiguration->ScriptedShowScenePipelineConfigurationDialog(DuplicateSourceData) : RegisteredPipelineConfiguration->ScriptedShowPipelineConfigurationDialog(DuplicateSourceData);
				if (DialogResult == EInterchangePipelineConfigurationDialogResult::Cancel)
				{
					bImportCanceled = true;
				}
				if (DialogResult == EInterchangePipelineConfigurationDialogResult::ImportAll)
				{
					bImportAllWithDefault = true;
				}
			}

			if (!bImportCanceled)
			{
				if (!DefaultPipelineStacks.Contains(PipelineStackName))
				{
					if (DefaultPipelineStacks.Contains(FInterchangeProjectSettingsUtils::GetDefaultPipelineStackName(bImportScene, *SourceData)))
					{
						PipelineStackName = FInterchangeProjectSettingsUtils::GetDefaultPipelineStackName(bImportScene, *SourceData);
					}
					else
					{
						//Log an error, we cannot import asset without a valid pipeline, we will use the first available pipeline
						for (const TPair<FName, FInterchangePipelineStack>& PipelineStack : DefaultPipelineStacks)
						{
							PipelineStackName = PipelineStack.Key;
						}
					}
				}

				if (DefaultPipelineStacks.Contains(PipelineStackName))
				{
					if (FEngineAnalytics::IsAvailable())
					{
						Attribs.Add(FAnalyticsEventAttribute(TEXT("PipelineStackName"), PipelineStackName));
					}
					//use the default pipeline
					const FInterchangePipelineStack& PipelineStack = DefaultPipelineStacks.FindChecked(PipelineStackName);
					const TArray<FSoftObjectPath>& Pipelines = PipelineStack.Pipelines;
					for (int32 GraphPipelineIndex = 0; GraphPipelineIndex < Pipelines.Num(); ++GraphPipelineIndex)
					{
						if (Pipelines[GraphPipelineIndex].IsValid())
						{
							if(UInterchangePipelineBase* GeneratedPipeline = UE::Interchange::GeneratePipelineInstance(Pipelines[GraphPipelineIndex]))
							{
								GeneratedPipeline->AdjustSettingsForContext(bImportScene ? EInterchangePipelineContext::SceneImport : EInterchangePipelineContext::AssetImport, nullptr);

								if (FInterchangeProjectSettingsUtils::ShouldShowPipelineStacksConfigurationDialog(bImportScene, *SourceData))
								{
									//Load the settings for this pipeline
									//The dialog is saving the settings for all default pipelines
									GeneratedPipeline->LoadSettings(PipelineStackName);
								}
					
								UE::Interchange::Private::FillPipelineAnalyticData(GeneratedPipeline, UniqueId, FString());

								AsyncHelper->Pipelines.Add(GeneratedPipeline);

								//We need to save the python pipeline asset because we cannot save an asset created with a python class
								if (UInterchangePythonPipelineAsset* OriginalPipeline = Cast<UInterchangePythonPipelineAsset>(Pipelines[GraphPipelineIndex].TryLoad()))
								{
									if (UInterchangePythonPipelineAsset* DuplicatedPythonPipelineAsset = DuplicateObject<UInterchangePythonPipelineAsset>(OriginalPipeline, GetTransientPackage()))
									{
										DuplicatedPythonPipelineAsset->SetupFromPipeline(Cast<UInterchangePythonPipelineBase>(GeneratedPipeline));
										AsyncHelper->OriginalPipelines.Add(DuplicatedPythonPipelineAsset);
									}
								}
								else
								{
									AsyncHelper->OriginalPipelines.Add(GeneratedPipeline);
								}
							}
						}
					}
				}
				else
				{
					//Log an error, we cannot import asset without a valid pipeline, there is no pipeline stack defined
					bImportAborted = true;
				}
			}
		}
	}
	else
	{
		for (int32 GraphPipelineIndex = 0; GraphPipelineIndex < ImportAssetParameters.OverridePipelines.Num(); ++GraphPipelineIndex)
		{
			// Duplicate the override pipelines to protect the scripted users form making race conditions
			UInterchangePipelineBase* GeneratedPipeline = DuplicateObject<UInterchangePipelineBase>(ImportAssetParameters.OverridePipelines[GraphPipelineIndex], GetTransientPackage());
			if (OriginalAssetImportData != nullptr)
			{
				GeneratedPipeline->AdjustSettingsForContext(bImportScene ? EInterchangePipelineContext::SceneImport : EInterchangePipelineContext::AssetImport, ImportAssetParameters.ReimportAsset);
			}
			else
			{
				GeneratedPipeline->AdjustSettingsForContext(bImportScene ? EInterchangePipelineContext::SceneImport : EInterchangePipelineContext::AssetImport, nullptr);
			}
			AsyncHelper->Pipelines.Add(GeneratedPipeline);
			AsyncHelper->OriginalPipelines.Add(GeneratedPipeline);
			UE::Interchange::Private::FillPipelineAnalyticData(GeneratedPipeline, UniqueId, FString());
		}
	}

	//Cancel the import do not queue task
	if (bImportCanceled || bImportAborted)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			Attribs.Add(FAnalyticsEventAttribute(TEXT("Canceled"), bImportCanceled));
		}

		AsyncHelper->InitCancel();
		AsyncHelper->CleanUp();
	}

	//Queue the task cancel or not, we need to return a valid asset import result
	FQueuedTaskData QueuedTaskData;
	QueuedTaskData.AsyncHelper = AsyncHelper;
	QueuedTaskData.PackageBasePath = PackageBasePath;
	QueuedTasks.Enqueue(QueuedTaskData);
	QueueTaskCount = FMath::Clamp(QueueTaskCount + 1, 0, MAX_int32);

	StartQueuedTasks();

	PreReturn();
	return TTuple<UE::Interchange::FAssetImportResultRef, UE::Interchange::FSceneImportResultRef>{ AsyncHelper->AssetImportResult, AsyncHelper->SceneImportResult };
}

bool UInterchangeManager::IsInterchangeImportEnabled()
{
	return CCvarInterchangeImportEnable->GetBool();
}

bool UInterchangeManager::ExportAsset(const UObject* Asset, bool bIsAutomated)
{
	return false;
}

bool UInterchangeManager::ExportScene(const UObject* World, bool bIsAutomated)
{
	return false;
}

UInterchangeSourceData* UInterchangeManager::CreateSourceData(const FString& InFileName)
{
	UInterchangeSourceData* SourceDataAsset = NewObject<UInterchangeSourceData>(GetTransientPackage(), NAME_None);
	if(!InFileName.IsEmpty())
	{
		SourceDataAsset->SetFilename(InFileName);
	}
	return SourceDataAsset;
}

const UClass* UInterchangeManager::GetRegisteredFactoryClass(const UClass* ClassToMake) const
{
	const UClass* BestClassToMake = nullptr;
	const UClass* Result = nullptr;

	for (const auto& Kvp : RegisteredFactoryClasses)
	{
		if (ClassToMake->IsChildOf(Kvp.Key))
		{
			// Find the factory which handles the most derived registered type 
			if (BestClassToMake == nullptr || Kvp.Key->IsChildOf(BestClassToMake))
			{
				BestClassToMake = Kvp.Key.Get();
				Result = Kvp.Value.Get();
			}
		}
	}
	return Result;
}

TSharedRef<UE::Interchange::FImportAsyncHelper, ESPMode::ThreadSafe> UInterchangeManager::CreateAsyncHelper(const UE::Interchange::FImportAsyncHelperData& Data, const FImportAssetParameters& ImportAssetParameters)
{
	TSharedRef<UE::Interchange::FImportAsyncHelper, ESPMode::ThreadSafe> AsyncHelper = MakeShared<UE::Interchange::FImportAsyncHelper, ESPMode::ThreadSafe>();
	//Copy the task data
	AsyncHelper->TaskData = Data;
	
	AsyncHelper->AssetImportResult->OnObjectDone = ImportAssetParameters.OnAssetDone;
	AsyncHelper->AssetImportResult->OnObjectDoneNative = ImportAssetParameters.OnAssetDoneNative;
	AsyncHelper->AssetImportResult->OnImportDone = ImportAssetParameters.OnAssetsImportDone;
	AsyncHelper->AssetImportResult->OnImportDoneNative = ImportAssetParameters.OnAssetsImportDoneNative;

	AsyncHelper->SceneImportResult->OnObjectDone = ImportAssetParameters.OnSceneObjectDone;
	AsyncHelper->SceneImportResult->OnObjectDoneNative = ImportAssetParameters.OnSceneObjectDoneNative;
	AsyncHelper->SceneImportResult->OnImportDone = ImportAssetParameters.OnSceneImportDone;
	AsyncHelper->SceneImportResult->OnImportDoneNative = ImportAssetParameters.OnSceneImportDoneNative;

	AsyncHelper->AssetImportResult->SetInProgress();

	return AsyncHelper;
}

void UInterchangeManager::ReleaseAsyncHelper(TWeakPtr<UE::Interchange::FImportAsyncHelper, ESPMode::ThreadSafe> AsyncHelper)
{
	using namespace UE::Interchange;

	check(AsyncHelper.IsValid());
	
	constexpr bool bLogWarningsAndErrors = true;

	bool bSucceeded = false;
	{
		TSharedPtr<FImportAsyncHelper> AsyncHelperPtr = AsyncHelper.Pin();
		
		auto ForEachResult = [&bSucceeded, bLogWarningsAndErrors](TArray<UInterchangeResult*>&& Results)
		{
			if (!bSucceeded || bLogWarningsAndErrors)
			{
				for (UInterchangeResult* Result : Results)
				{
					if (!Result)
					{
						continue;
					}

					if (Result->GetResultType() == EInterchangeResultType::Success)
					{
						bSucceeded = true;
					}
					else if (bLogWarningsAndErrors)
					{
						switch (Result->GetResultType())
						{
						case EInterchangeResultType::Warning:
							UE_LOG(LogInterchangeEngine, Warning, TEXT("%s"), *Result->ToJson());
							break;
						case EInterchangeResultType::Error:
							UE_LOG(LogInterchangeEngine, Error, TEXT("%s"), *Result->ToJson());
							break;
						}
					}
				}
			}
		};

		ForEachResult(AsyncHelperPtr->AssetImportResult->GetResults()->GetResults());
		ForEachResult(AsyncHelperPtr->SceneImportResult->GetResults()->GetResults());
	}

	ImportTasks.RemoveSingle(AsyncHelper.Pin());
	//Make sure the async helper is destroy, if not destroy its because we are canceling the import and we still have a shared ptr on it
	{
		TSharedPtr<UE::Interchange::FImportAsyncHelper, ESPMode::ThreadSafe> AsyncHelperSharedPtr = AsyncHelper.Pin();
		check(!AsyncHelperSharedPtr.IsValid() || AsyncHelperSharedPtr->bCancel);
	}

	int32 ImportTaskNumber = ImportTasks.Num();
	FString ImportTaskNumberStr = TEXT(" (") + FString::FromInt(ImportTaskNumber) + TEXT(")");
	if (ImportTaskNumber == 0)
	{
		SetActiveMode(false);

		if (Notification.IsValid())
		{
			FText TitleText;
			if (bImportCanceled)
			{
				TitleText = NSLOCTEXT("Interchange", "Asynchronous_import_canceled", "Import Canceled");
				bSucceeded = true; // Mark the "cancelation" as a success so that the notification goes away
			}
			else
			{
				TitleText = NSLOCTEXT("Interchange", "Asynchronous_import_end", "Import Done");
			}

			Notification->SetComplete(TitleText, FText::GetEmpty(), bSucceeded);
			Notification = nullptr; //This should delete the notification
		}

		bImportAllWithDefault = false;
		bImportCanceled = false;
	}
	else if(Notification.IsValid())
	{
		Notification->SetProgressText(FText::FromString(ImportTaskNumberStr));
	}

	//Start some task if there is some waiting
	StartQueuedTasks();
}

UInterchangeTranslatorBase* UInterchangeManager::GetTranslatorForSourceData(const UInterchangeSourceData* SourceData) const
{
	// Find the translator
	for (const UClass* TranslatorClass : RegisteredTranslatorsClass)
	{
		if (TranslatorClass->GetDefaultObject<UInterchangeTranslatorBase>()->CanImportSourceData(SourceData))
		{
			UInterchangeTranslatorBase* SourceDataTranslator = NewObject<UInterchangeTranslatorBase>(GetTransientPackage(), TranslatorClass, NAME_None);
			SourceDataTranslator->SourceData = SourceData;
			return SourceDataTranslator;
		}
	}
	return nullptr;
}

bool UInterchangeManager::IsInterchangeActive()
{
	return bIsActive;
}

bool UInterchangeManager::WarnIfInterchangeIsActive()
{
	if (!bIsActive)
	{
		return false;
	}
	//Tell the user they have to cancel the import before closing the editor
	FNotificationInfo Info(NSLOCTEXT("InterchangeManager", "WarnCannotProceed", "An import process is currently underway! Please cancel it to proceed!"));
	Info.ExpireDuration = 5.0f;
	TSharedPtr<SNotificationItem> WarnNotification = FSlateNotificationManager::Get().AddNotification(Info);
	if (WarnNotification.IsValid())
	{
		WarnNotification->SetCompletionState(SNotificationItem::CS_Fail);
	}
	return true;
}

bool UInterchangeManager::CanTranslateSourceDataWithPayloadInterface(const UInterchangeSourceData* SourceData, const UClass* PayloadInterfaceClass) const
{
	if (GetTranslatorSupportingPayloadInterfaceForSourceData(SourceData, PayloadInterfaceClass))
	{
		return true;
	}
	return false;
}

UInterchangeTranslatorBase* UInterchangeManager::GetTranslatorSupportingPayloadInterfaceForSourceData(const UInterchangeSourceData* SourceData, const UClass* PayloadInterfaceClass) const
{
	// Find the translator
	for (const UClass* TranslatorClass : RegisteredTranslatorsClass)
	{
		if (TranslatorClass->ImplementsInterface(PayloadInterfaceClass) &&
			TranslatorClass->GetDefaultObject<UInterchangeTranslatorBase>()->CanImportSourceData(SourceData))
		{
			UInterchangeTranslatorBase* SourceDataTranslator = NewObject<UInterchangeTranslatorBase>(GetTransientPackage(), TranslatorClass, NAME_None);
			SourceDataTranslator->SourceData = SourceData;
			return SourceDataTranslator;
		}
	}
	return nullptr;
}

bool UInterchangeManager::IsAttended()
{
	if (FApp::IsGame())
	{
		return false;
	}
	if (FApp::IsUnattended())
	{
		return false;
	}
	return true;
}

void UInterchangeManager::FindPipelineCandidate(TArray<UClass*>& PipelineCandidates)
{
	//Find in memory pipeline class
	for (TObjectIterator< UClass > ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		// Only interested in native C++ classes
// 		if (!Class->IsNative())
// 		{
// 			continue;
// 		}
		// Ignore deprecated
		if (Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			continue;
		}

		// Check this class is a subclass of Base and not the base itself
		if (Class == UInterchangePipelineBase::StaticClass() || !Class->IsChildOf(UInterchangePipelineBase::StaticClass()))
		{
			continue;
		}

		//We found a candidate
		PipelineCandidates.AddUnique(Class);
	}

	//Blueprint and python script discoverability is available only if we compile with the engine
	// Load the asset registry module
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked< FAssetRegistryModule >(FName("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray< FString > ContentPaths;
	ContentPaths.Add(TEXT("/Game"));
	//TODO do we have an other alternative, this call is synchronous and will wait unitl the registry database have finish the initial scan. If there is a lot of asset it can take multiple second the first time we call it.
	AssetRegistry.ScanPathsSynchronous(ContentPaths);

	FTopLevelAssetPath BaseClassName = UInterchangePipelineBase::StaticClass()->GetClassPathName();

	// Use the asset registry to get the set of all class names deriving from Base
	TSet< FTopLevelAssetPath > DerivedNames;
	{
		TArray< FTopLevelAssetPath > BaseNames;
		BaseNames.Add(BaseClassName);

		TSet< FTopLevelAssetPath > Excluded;
		AssetRegistry.GetDerivedClassNames(BaseNames, Excluded, DerivedNames);
	}

	FARFilter Filter;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	Filter.bRecursivePaths = true;

	TArray< FAssetData > AssetList;
	AssetRegistry.GetAssets(Filter, AssetList);

	// Iterate over retrieved blueprint assets
	for (const FAssetData& Asset : AssetList)
	{
		//Only get the asset with the native parent class using UInterchangePipelineBase
		FAssetDataTagMapSharedView::FFindTagResult GeneratedClassPath = Asset.TagsAndValues.FindTag(TEXT("GeneratedClass"));
		if (GeneratedClassPath.IsSet())
		{
			// Convert path to just the name part
			const FTopLevelAssetPath ClassObjectPath(FPackageName::ExportTextPathToObjectPath(*GeneratedClassPath.GetValue()));

			// Check if this class is in the derived set
			if (!DerivedNames.Contains(ClassObjectPath))
			{
				continue;
			}

			UBlueprint* Blueprint = Cast<UBlueprint>(Asset.GetAsset());
			check(Blueprint);
			check(Blueprint->ParentClass == UInterchangePipelineBase::StaticClass());
			PipelineCandidates.AddUnique(Blueprint->GeneratedClass);
		}
	}
}

void UInterchangeManager::CancelAllTasks()
{
	check(IsInGameThread());

	//Cancel the queued tasks, we cannot simply not do them since, there is some promise objects
	//to setup in the completion task
	const bool bCancelAllTasks = true;
	StartQueuedTasks(bCancelAllTasks);

	//Set the cancel state on all running tasks
	int32 ImportTaskCount = ImportTasks.Num();
	for (int32 TaskIndex = 0; TaskIndex < ImportTaskCount; ++TaskIndex)
	{
		TSharedPtr<UE::Interchange::FImportAsyncHelper, ESPMode::ThreadSafe> AsyncHelper = ImportTasks[TaskIndex];
		if (AsyncHelper.IsValid())
		{
			AsyncHelper->InitCancel();
		}
	}
	//Tasks should all finish quite fast now
};

void UInterchangeManager::CancelAllTasksSynchronously()
{
	//Start the cancel process by cancelling all current task
	CancelAllTasks();

	//Now wait for each task to be completed on the main thread
	while (ImportTasks.Num() > 0)
	{
		int32 ImportTaskCount = ImportTasks.Num();
		TSharedPtr<UE::Interchange::FImportAsyncHelper, ESPMode::ThreadSafe> AsyncHelper = ImportTasks[0];
		if (AsyncHelper.IsValid())
		{
			//Cancel any on going interchange activity this is blocking but necessary.
			AsyncHelper->CancelAndWaitUntilDoneSynchronously();
			ensure(ImportTaskCount > ImportTasks.Num());
			TWeakPtr<UE::Interchange::FImportAsyncHelper, ESPMode::ThreadSafe> WeakAsyncHelper = AsyncHelper;
			//free the async helper
			AsyncHelper = nullptr;
			//We verify that the weak pointer is invalid after releasing the async helper
			ensure(!WeakAsyncHelper.IsValid());
		}
	}
}

void UInterchangeManager::SetActiveMode(bool IsActive)
{
	if (bIsActive == IsActive)
	{
		return;
	}

	bIsActive = IsActive;
	if (bIsActive)
	{
		ensure(!NotificationTickHandle.IsValid());
		NotificationTickHandle = FTSTicker::GetCoreTicker().AddTicker(TEXT("InterchangeManagerTickHandle"), 0.1f, [this](float)
		{
			if (Notification.IsValid() && Notification->GetPromptAction() == EAsyncTaskNotificationPromptAction::Cancel)
			{
				CancelAllTasks();
			}
			return true;
		});
	}
	else
	{
		FTSTicker::GetCoreTicker().RemoveTicker(NotificationTickHandle);
		NotificationTickHandle.Reset();
	}
}
