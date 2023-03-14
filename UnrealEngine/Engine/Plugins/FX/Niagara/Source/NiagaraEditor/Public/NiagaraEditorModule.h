// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleInterface.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "NiagaraTypes.h"
#include "INiagaraCompiler.h"
#include "AssetTypeCategories.h"
#include "NiagaraPerfBaseline.h"
#include "NiagaraDebuggerCommon.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraEditorModule.generated.h"

class IAssetTools;
class IAssetTypeActions;
class INiagaraEditorTypeUtilities;
class UNiagaraSettings;
class USequencerSettings;
class UNiagaraStackViewModel;
class UNiagaraStackEntry;
class UNiagaraStackIssue;
class FNiagaraSystemViewModel;
class FNiagaraScriptMergeManager;
class FNiagaraCompileOptions;
class FNiagaraCompileRequestDataBase;
class FNiagaraCompileRequestDuplicateDataBase;
class UMovieSceneNiagaraParameterTrack;
struct IConsoleCommand;
class INiagaraEditorOnlyDataUtilities;
class FNiagaraEditorCommands;
struct FNiagaraScriptHighlight;
class FNiagaraClipboard;
class UNiagaraScratchPadViewModel;
class FHlslNiagaraCompiler;
class FNiagaraComponentBroker;
class INiagaraStackObjectIssueGenerator;
class UNiagaraEffectType;
class SNiagaraBaselineViewport;
class FParticlePerfStatsListener_NiagaraBaselineComparisonRender;
class FNiagaraDebugger;
class UNiagaraParameterDefinitions;
class UNiagaraReservedParametersManager;
class FNiagaraGraphDataCache;
class UNiagaraParameterCollection;

DECLARE_STATS_GROUP(TEXT("Niagara Editor"), STATGROUP_NiagaraEditor, STATCAT_Advanced);

extern NIAGARAEDITOR_API int32 GbShowNiagaraDeveloperWindows;
extern NIAGARAEDITOR_API int32 GbPreloadSelectablePluginAssetsOnDemand;

/* Defines methods for allowing external modules to supply widgets to the core editor module. */
class NIAGARAEDITOR_API INiagaraEditorWidgetProvider
{
public:
	virtual TSharedRef<SWidget> CreateStackView(UNiagaraStackViewModel& StackViewModel) const = 0;
	virtual TSharedRef<SWidget> CreateSystemOverview(TSharedRef<FNiagaraSystemViewModel> SystemViewModel, const FAssetData& EditedAsset) const = 0;
	virtual TSharedRef<SWidget> CreateStackIssueIcon(UNiagaraStackViewModel& StackViewModel, UNiagaraStackEntry& StackEntry) const = 0;

	virtual TSharedRef<SWidget> CreateScriptScratchPadManager(UNiagaraScratchPadViewModel& ScriptScratchPadViewModel) const = 0;
	virtual TSharedRef<SWidget> CreateCurveOverview(TSharedRef<FNiagaraSystemViewModel> SystemViewModel) const = 0;
	virtual FLinearColor GetColorForExecutionCategory(FName ExecutionCategory) const = 0;
};

/* Wrapper struct for tracking parameters that are reserved by parameter definitions assets. */
USTRUCT()
struct FReservedParameter
{
	GENERATED_BODY()

public:
	FReservedParameter()
		: Parameter(FNiagaraVariable())
		, ReservingDefinitionsAsset(nullptr)
	{};

	FReservedParameter(const FNiagaraVariableBase& InParameter, const UNiagaraParameterDefinitions* InReservingDefinitionsAsset)
		: Parameter(InParameter)
		, ReservingDefinitionsAsset(const_cast<UNiagaraParameterDefinitions*>(InReservingDefinitionsAsset))
	{};

	bool operator== (const FReservedParameter& Other) const { return Parameter == Other.GetParameter() && ReservingDefinitionsAsset == Other.GetReservingDefinitionsAsset(); };

	const FNiagaraVariableBase& GetParameter() const { return Parameter; };
	const UNiagaraParameterDefinitions* GetReservingDefinitionsAsset() const { return ReservingDefinitionsAsset; };

private:
	UPROPERTY(transient)
	FNiagaraVariableBase Parameter;
		 
	UPROPERTY(transient)
	TObjectPtr<UNiagaraParameterDefinitions> ReservingDefinitionsAsset;
};

USTRUCT()
struct NIAGARAEDITOR_API FNiagaraRendererCreationInfo
{
	DECLARE_DELEGATE_RetVal_OneParam(UNiagaraRendererProperties*, FRendererFactory, UObject* OuterEmitter);

	GENERATED_BODY()

	FNiagaraRendererCreationInfo() = default;
	FNiagaraRendererCreationInfo(FText InDisplayName, const FTopLevelAssetPath& InRendererClassPath, FRendererFactory InFactory) : DisplayName(InDisplayName), RendererClassPath(InRendererClassPath), RendererFactory(InFactory)
	{}

	FNiagaraRendererCreationInfo(FText InDisplayName, FText InDescription, const FTopLevelAssetPath& InRendererClassPath, FRendererFactory InFactory) : DisplayName(InDisplayName), Description(InDescription), RendererClassPath(InRendererClassPath), RendererFactory(InFactory)
	{}

	UPROPERTY()
	FText DisplayName;

	UPROPERTY()
	FText Description;

	UPROPERTY()
	FTopLevelAssetPath RendererClassPath;
	
	FRendererFactory RendererFactory;
};

FORCEINLINE uint32 GetTypeHash(const FReservedParameter& ReservedParameter) { return GetTypeHash(ReservedParameter.GetParameter().GetName()); };

/** Niagara Editor module */
class FNiagaraEditorModule : public IModuleInterface,
	public IHasMenuExtensibility, public IHasToolBarExtensibility, public FGCObject
{
public:
	DECLARE_DELEGATE_RetVal_OneParam(UMovieSceneNiagaraParameterTrack*, FOnCreateMovieSceneTrackForParameter, FNiagaraVariable);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnCheckScriptToolkitsShouldFocusGraphElement, const FNiagaraScriptIDAndGraphFocusInfo*);

public:
	FNiagaraEditorModule();

	// IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Get the instance of this module. */
	NIAGARAEDITOR_API static FNiagaraEditorModule& Get();

	/** Start the compilation of the specified script. */
	virtual int32 CompileScript(const FNiagaraCompileRequestDataBase* InCompileRequest, const FNiagaraCompileRequestDuplicateDataBase* InCompileRequestDuplicate, const FNiagaraCompileOptions& InCompileOptions);
	virtual TSharedPtr<FNiagaraVMExecutableData> GetCompilationResult(int32 JobID, bool bWait);

	TSharedPtr<FNiagaraCompileRequestDataBase, ESPMode::ThreadSafe> Precompile(UObject* InObj, FGuid Version);
	TSharedPtr<FNiagaraCompileRequestDuplicateDataBase, ESPMode::ThreadSafe> PrecompileDuplicate(
		const FNiagaraCompileRequestDataBase* OwningSystemRequestData,
		UNiagaraSystem* OwningSystem,
		UNiagaraEmitter* OwningEmitter,
		UNiagaraScript* TargetScript,
		FGuid TargetVersion);
	TSharedPtr<FNiagaraGraphCachedDataBase, ESPMode::ThreadSafe> CacheGraphTraversal(const UObject* Obj, FGuid Version);


	/** Gets the extensibility managers for outside entities to extend static mesh editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() override {return MenuExtensibilityManager;}
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() override {return ToolBarExtensibilityManager;}

	/** Registers niagara editor type utilities for a specific type. */
	void RegisterTypeUtilities(FNiagaraTypeDefinition Type, TSharedRef<INiagaraEditorTypeUtilities, ESPMode::ThreadSafe> EditorUtilities);

	/** Register/unregister niagara editor settings. */
	void RegisterSettings();
	void UnregisterSettings();

	/** Gets Niagara editor type utilities for a specific type if there are any registered. */
	TSharedPtr<INiagaraEditorTypeUtilities, ESPMode::ThreadSafe> NIAGARAEDITOR_API GetTypeUtilities(const FNiagaraTypeDefinition& Type);

	static EAssetTypeCategories::Type GetAssetCategory() { return NiagaraAssetCategory; }

	NIAGARAEDITOR_API void RegisterWidgetProvider(TSharedRef<INiagaraEditorWidgetProvider> InWidgetProvider);
	NIAGARAEDITOR_API void UnregisterWidgetProvider(TSharedRef<INiagaraEditorWidgetProvider> InWidgetProvider);

	TSharedRef<INiagaraEditorWidgetProvider> GetWidgetProvider() const;

	TSharedRef<FNiagaraScriptMergeManager> GetScriptMergeManager() const;

	// Object pooling methods used to prevent unnecessary object allocation during compiles
	NIAGARAEDITOR_API UObject* GetPooledDuplicateObject(UObject* Source, EFieldIteratorFlags::SuperClassFlags CopySuperProperties = EFieldIteratorFlags::ExcludeSuper);
	NIAGARAEDITOR_API void ReleaseObjectToPool(UObject* Obj);
	NIAGARAEDITOR_API void ClearObjectPool();

	/** Registers a new renderer creation delegate with the display name it's going to use for the UI. */
	NIAGARAEDITOR_API void RegisterRendererCreationInfo(FNiagaraRendererCreationInfo RendererCreationInfo);
	NIAGARAEDITOR_API const TArray<FNiagaraRendererCreationInfo>& GetRendererCreationInfos() const { return RendererCreationInfo; }
	
	void RegisterParameterTrackCreatorForType(const UScriptStruct& StructType, FOnCreateMovieSceneTrackForParameter CreateTrack);
	void UnregisterParameterTrackCreatorForType(const UScriptStruct& StructType);
	bool CanCreateParameterTrackForType(const UScriptStruct& StructType);
	UMovieSceneNiagaraParameterTrack* CreateParameterTrackForType(const UScriptStruct& StructType, FNiagaraVariable Parameter);

	/** Niagara Editor app identifier string */
	static const FName NiagaraEditorAppIdentifier;

	/** The tab color scale for niagara editors. */
	static const FLinearColor WorldCentricTabColorScale;

	/** Get the niagara UI commands. */
	NIAGARAEDITOR_API const class FNiagaraEditorCommands& Commands();

	FOnCheckScriptToolkitsShouldFocusGraphElement& GetOnScriptToolkitsShouldFocusGraphElement() { return OnCheckScriptToolkitsShouldFocusGraphElement; };

	NIAGARAEDITOR_API TSharedPtr<FNiagaraSystemViewModel> GetExistingViewModelForSystem(UNiagaraSystem* InSystem);

	NIAGARAEDITOR_API const FNiagaraEditorCommands& GetCommands() const;

	void InvalidateCachedScriptAssetData();
	
	const TArray<UNiagaraScript*>& GetCachedTypeConversionScripts() const;

	NIAGARAEDITOR_API FNiagaraClipboard& GetClipboard() const;

	template<typename T>
	void EnqueueObjectForDeferredDestruction(TSharedRef<T> InObjectToDestruct)
	{
		TDeferredDestructionContainer<T>* ObjectInContainer = new TDeferredDestructionContainer<T>(InObjectToDestruct);
		EnqueueObjectForDeferredDestructionInternal(ObjectInContainer);
	}

	FORCEINLINE INiagaraStackObjectIssueGenerator* FindStackObjectIssueGenerator(FName StructName)
	{
		if (INiagaraStackObjectIssueGenerator** FoundGenerator = StackIssueGenerators.Find(StructName))
		{
			return *FoundGenerator;
		}
		return nullptr;
	}

#if WITH_NIAGARA_DEBUGGER
	TSharedPtr<FNiagaraDebugger> GetDebugger(){ return Debugger; }
#endif

	const TArray<TWeakObjectPtr<UNiagaraParameterDefinitions>>& GetCachedParameterDefinitionsAssets();

	NIAGARAEDITOR_API void GetTargetSystemAndEmitterForDataInterface(UNiagaraDataInterface* InDataInterface, UNiagaraSystem*& OutOwningSystem, FVersionedNiagaraEmitter& OutOwningEmitter);
	NIAGARAEDITOR_API void GetDataInterfaceFeedbackSafe(UNiagaraDataInterface* InDataInterface, TArray<FNiagaraDataInterfaceError>& OutErrors, TArray<FNiagaraDataInterfaceFeedback>& Warnings, TArray<FNiagaraDataInterfaceFeedback>& Info);

	NIAGARAEDITOR_API void EnsureReservedDefinitionUnique(FGuid& UniqueId);

	FNiagaraGraphDataCache& GetGraphDataCache() const { return *GraphDataCache.Get(); }

	NIAGARAEDITOR_API UNiagaraParameterCollection* FindCollectionForVariable(const FString& VariableName);

	void PreloadSelectablePluginAssetsByClass(UClass* InClass);

private:
	class FDeferredDestructionContainerBase
	{
	public:
		virtual ~FDeferredDestructionContainerBase()
		{
		}
	};

	template<typename T>
	class TDeferredDestructionContainer : public FDeferredDestructionContainerBase
	{
	public:
		TDeferredDestructionContainer(TSharedRef<const T> InObjectToDestruct)
			: ObjectToDestuct(InObjectToDestruct)
		{
		}

		virtual ~TDeferredDestructionContainer()
		{
			ObjectToDestuct.Reset();
		}

		TSharedPtr<const T> ObjectToDestuct;
	};

	template<class AssetType>
	class TAssetPreloadCache
	{
	public:
		void RefreshCache(bool bAllowLoading)
		{
			const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			TArray<FAssetData> AssetData;
			AssetRegistryModule.GetRegistry().GetAssetsByClass(AssetType::StaticClass()->GetClassPathName(), AssetData);

			CachedAssets.Reset(AssetData.Num());
			for (const FAssetData& AssetDatum : AssetData)
			{
				if (AssetDatum.IsAssetLoaded() || (bAllowLoading && FPackageName::GetPackageMountPoint(AssetDatum.PackageName.ToString()) != NAME_None))
				{
					if (AssetType* Asset = Cast<AssetType>(AssetDatum.GetAsset()))
					{
						CachedAssets.Add(MakeWeakObjectPtr(Asset));
					}
				}
			}
		};

		const TArray<TWeakObjectPtr<AssetType>>& Get() const { return CachedAssets; };

	private:
		TArray<TWeakObjectPtr<AssetType>> CachedAssets;
	};

	void RegisterDefaultRendererFactories();
	void RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action);
	void OnNiagaraSettingsChangedEvent(const FName& PropertyName, const UNiagaraSettings* Settings);
	void OnPreGarbageCollection();
	void OnExecParticleInvoked(const TCHAR* InStr);
	void OnPostEngineInit();
	void OnDeviceProfileManagerUpdated();
	void OnPreviewPlatformChanged();
	void OnPreExit();
	void PostGarbageCollect();

	/** FGCObject interface */
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	virtual FString GetReferencerName() const override
	{
		return "FNiagaraEditorModule";
	}

	void TestCompileScriptFromConsole(const TArray<FString>& Arguments);
	void ReinitializeStyle();

	void EnqueueObjectForDeferredDestructionInternal(FDeferredDestructionContainerBase* InObjectToDestruct);

	bool DeferredDestructObjects(float InDeltaTime);

	void RegisterStackIssueGenerator(FName StructName, INiagaraStackObjectIssueGenerator* Generator)
	{
		StackIssueGenerators.Add(StructName) = Generator;
	}

private:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;

	/** All created asset type actions.  Cached here so that we can unregister it during shutdown. */
	TArray< TSharedPtr<IAssetTypeActions> > CreatedAssetTypeActions;

	FCriticalSection TypeEditorsCS;
	TMap<FNiagaraTypeDefinition, TSharedRef<INiagaraEditorTypeUtilities, ESPMode::ThreadSafe>> TypeToEditorUtilitiesMap;
	TSharedPtr<INiagaraEditorTypeUtilities, ESPMode::ThreadSafe> EnumTypeUtilities;

	static EAssetTypeCategories::Type NiagaraAssetCategory;

	FDelegateHandle CreateEmitterTrackEditorHandle;
	FDelegateHandle CreateSystemTrackEditorHandle;

	FDelegateHandle CreateBoolParameterTrackEditorHandle;
	FDelegateHandle CreateFloatParameterTrackEditorHandle;
	FDelegateHandle CreateIntegerParameterTrackEditorHandle;
	FDelegateHandle CreateVectorParameterTrackEditorHandle;
	FDelegateHandle CreateColorParameterTrackEditorHandle;

	FDelegateHandle ScriptCompilerHandle;
	FDelegateHandle CompileResultHandle;
	FDelegateHandle PrecompilerHandle;
	FDelegateHandle PrecompileDuplicatorHandle;
	FDelegateHandle GraphCacheTraversalHandle;

	FDelegateHandle DeviceProfileManagerUpdatedHandle;

	FDelegateHandle PreviewPlatformChangedHandle;

	USequencerSettings* SequencerSettings;

	TSharedPtr<INiagaraEditorWidgetProvider> WidgetProvider;

	TSharedPtr<FNiagaraScriptMergeManager> ScriptMergeManager;

	TSharedPtr<INiagaraEditorOnlyDataUtilities> EditorOnlyDataUtilities;

	TSharedPtr<FNiagaraComponentBroker> NiagaraComponentBroker;

	TMap<const UScriptStruct*, FOnCreateMovieSceneTrackForParameter> TypeToParameterTrackCreatorMap;

	IConsoleCommand* TestCompileScriptCommand;
	IConsoleCommand* DumpRapidIterationParametersForAsset;
	IConsoleCommand* PreventSystemRecompileCommand;
	IConsoleCommand* PreventAllSystemRecompilesCommand;
	IConsoleCommand* UpgradeAllNiagaraAssetsCommand;
	IConsoleCommand* DumpCompileIdDataForAssetCommand;
	IConsoleCommand* LoadAllSystemsInFolderCommand;

	FOnCheckScriptToolkitsShouldFocusGraphElement OnCheckScriptToolkitsShouldFocusGraphElement;

	mutable TOptional<TArray<FNiagaraScriptHighlight>> CachedScriptAssetHighlights;
	mutable TOptional<TArray<UNiagaraScript*>> TypeConversionScriptCache;

	bool bThumbnailRenderersRegistered;

	TSharedRef<FNiagaraClipboard> Clipboard;

	IConsoleCommand* ReinitializeStyleCommand;

	TMap<int32, TSharedPtr<FHlslNiagaraCompiler>> ActiveCompilations;

	TArray<TSharedRef<const FDeferredDestructionContainerBase>> EnqueuedForDeferredDestruction;

	TMap<FName, INiagaraStackObjectIssueGenerator*> StackIssueGenerators;

	TMap<UClass*, TArray<UObject*>> ObjectPool;

	TArray<FNiagaraRendererCreationInfo> RendererCreationInfo;

#if NIAGARA_PERF_BASELINES
	void GeneratePerfBaselines(TArray<UNiagaraEffectType*>& BaselinesToGenerate);
	void OnPerfBaselineWindowClosed(const TSharedRef<SWindow>& ClosedWindow);

	/** Viewport used when generating the performance baselines for Niagara systems. */
	TSharedPtr<SNiagaraBaselineViewport> BaselineViewport;
#endif

#if WITH_NIAGARA_DEBUGGER
	TSharedPtr<FNiagaraDebugger> Debugger;
#endif

	UNiagaraReservedParametersManager* ReservedParametersManagerSingleton;

	// Set of Parameter Definitions Ids
	TSet<FGuid> ReservedDefinitionIds;

	TUniquePtr<FNiagaraGraphDataCache> GraphDataCache;

	TAssetPreloadCache<UNiagaraParameterCollection> ParameterCollectionAssetCache;
	TAssetPreloadCache<UNiagaraParameterDefinitions> ParameterDefinitionsAssetCache;

	TArray<UClass*> PluginAssetClassesPreloaded;
};
