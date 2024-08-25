// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialStats.h"
#include "MaterialStatsGrid.h"
#include "SMaterialEditorStatsWidget.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SScrollBox.h"
#include "HAL/PlatformApplicationMisc.h"
#include "MaterialEditorActions.h"
#include "Materials/MaterialInstance.h"
#include "IMaterialEditor.h"
#include "MaterialEditorSettings.h"
#include "ShaderCompiler.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Modules/ModuleManager.h"
#include "MessageLogModule.h"
#include "Interfaces/IShaderFormat.h"

#define LOCTEXT_NAMESPACE "MaterialStats"

const FName FMaterialStats::StatsTabId(TEXT("MaterialStats_Grid"));
const FName FMaterialStats::OldStatsTabId(TEXT("OldMaterialStats_Grid"));
const FName FMaterialStats::HLSLCodeTabId(TEXT("MaterialStats_HLSLCode"));

/***********************************************************************************************************************/
/*begin FShaderPlatformSettings functions*/

FShaderPlatformSettings::FShaderPlatformSettings(
	const EPlatformCategoryType _PlatformType,
	const EShaderPlatform _ShaderPlatformID,
	const FName _Name,
	const bool _bAllowPresenceInGrid,
	const bool _bAllowCodeView,
	const FString& _Description,
	const bool bAlwaysOn
	)
	: PlatformType(_PlatformType)
	, PlatformShaderID(_ShaderPlatformID)
	, PlatformName(_Name)
	, PlatformDescription(_Description)
	, bAlwaysOn(bAlwaysOn)
	, bAllowCodeView(_bAllowCodeView)
	, bAllowPresenceInGrid(_bAllowPresenceInGrid)
{
	PlatformNameID = *FMaterialStatsUtils::ShaderPlatformTypeName(PlatformShaderID);
}

void FShaderPlatformSettings::ClearResources()
{
	TArray<TRefCountPtr<FMaterial>> MaterialsToDeleteOnRenderThread;

	// free material resources
	for (int32 i = 0; i < EMaterialQualityLevel::Num; ++i)
	{
		for (int32 InstanceIndex = 0; InstanceIndex < PlatformData[i].Instances.Num(); ++InstanceIndex)
		{
			auto& Instance = PlatformData[i].Instances[InstanceIndex];
			FMaterialResourceStats* Resource = Instance.MaterialResourcesStats;
			if (Resource != nullptr)
			{
				if (Resource->PrepareDestroy_GameThread())
				{
					MaterialsToDeleteOnRenderThread.Add(Resource);
				}
				else
				{
					delete Resource;
				}
			}

			Instance.ArrShaderEntries.Empty();
		}

		PlatformData[i].Instances.Empty();
	}

	FMaterial::DeleteMaterialsOnRenderThread(MaterialsToDeleteOnRenderThread);
}

FText FShaderPlatformSettings::GetSelectedShaderViewComboText(EMaterialQualityLevel::Type QualityLevel, const int32 InstanceIndex) const
{
	const auto& Instance = PlatformData[QualityLevel].Instances[InstanceIndex];
	if (Instance.ArrShaderEntries.Num() == 0)
	{
		return FText::FromString(TEXT("-Compiling-Shaders-"));
	}

	return FText::FromString(Instance.ComboBoxSelectedEntry.Text);
}

void FShaderPlatformSettings::OnShaderViewComboSelectionChanged(TSharedPtr<FMaterialShaderEntry> Item, EMaterialQualityLevel::Type QualityType, const int32 InstanceIndex)
{
	if (Item.IsValid())
	{
		auto& Instance = PlatformData[QualityType].Instances[InstanceIndex];
		Instance.ComboBoxSelectedEntry = *Item.Get();
		Instance.bUpdateShaderCode = true;
	}
}

FText FShaderPlatformSettings::GetShaderCode(const EMaterialQualityLevel::Type QualityType, const int32 InstanceIndex)
{
	auto& Instance = PlatformData[QualityType].Instances[InstanceIndex];
	// if there were no change to the material return the cached shader code
	if (!Instance.bUpdateShaderCode)
	{
		return Instance.ShaderCode;
	}

	Instance.ShaderCode = LOCTEXT("ShaderCodeMsg", "Shader code compiling or not available!");

	FMaterialResource *Resource = Instance.MaterialResourcesStats;
	const FMaterialShaderMap* MaterialShaderMap = Resource->GetGameThreadShaderMap();

	const bool bCompilationFinished = Resource->IsCompilationFinished() && (MaterialShaderMap != nullptr);

	// check if shader compilation is done and extract shader code
	if (bCompilationFinished)
	{
		TMap<FShaderId, TShaderRef<FShader>> ShaderMap;
		MaterialShaderMap->GetShaderList(ShaderMap);

		const FShaderId& ShaderId = Instance.ComboBoxSelectedEntry.ShaderId;

		const auto Entry = ShaderMap.Find(ShaderId);
		if (Entry != nullptr)
		{
			const TShaderRef<FShader>& Shader = *Entry;
			const FMemoryImageString* ShaderSource = MaterialShaderMap->GetShaderSource(Shader.GetVertexFactoryType(), Shader.GetType(), ShaderId.PermutationId);
			if (ShaderSource != nullptr)
			{
				Instance.bUpdateShaderCode = false;
				Instance.ShaderCode = FText::FromString(*ShaderSource);
			}
		}
	}

	return Instance.ShaderCode;
}

void FShaderPlatformSettings::AllocateMaterialResources()
{
	ClearResources();

	const ERHIFeatureLevel::Type TargetFeatureLevel = GetMaxSupportedFeatureLevel(PlatformShaderID);

	for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
	{
		auto& Data = PlatformData[QualityLevelIndex].Instances.AddDefaulted_GetRef();

		Data.MaterialResourcesStats = new FMaterialResourceStats();
		Data.MaterialResourcesStats->SetMaterial(Material, MaterialInstance, TargetFeatureLevel, (EMaterialQualityLevel::Type)QualityLevelIndex);

		Data.ShaderStatsInfo.Reset();

		Data.bNeedShaderRecompilation = true;

		for (int32 i = 0; i < DerivedMaterialInstances.Num(); ++i)
		{
			auto& Data2 = PlatformData[QualityLevelIndex].Instances.AddDefaulted_GetRef();

			Data2.MaterialResourcesStats = new FMaterialResourceStats();
			Data2.MaterialResourcesStats->SetMaterial(DerivedMaterialInstances[i]->GetBaseMaterial(), DerivedMaterialInstances[i], TargetFeatureLevel, (EMaterialQualityLevel::Type)QualityLevelIndex);

			Data2.ShaderStatsInfo.Reset();

			Data2.bNeedShaderRecompilation = true;
		}

		// ensure we start compiling shaders soon due to bNeedShaderRecompilation being set above
		PlatformData[QualityLevelIndex].LastTimeCompilationRequested = 0.0;
	}
}

void FShaderPlatformSettings::SetMaterial(UMaterial *InBaseMaterial, UMaterialInstance *InBaseMaterialInstance, const TArray<TObjectPtr<UMaterialInstance>>& InDerivedMaterialInstances)
{
	bool bReallocate = false;

	if (InBaseMaterial != nullptr && InBaseMaterialInstance == nullptr && Material != InBaseMaterial)
	{
		Material = InBaseMaterial;
		MaterialInstance = nullptr;
		bReallocate = true;
	}

	if (InBaseMaterial == nullptr && InBaseMaterialInstance != nullptr && MaterialInstance != InBaseMaterialInstance)
	{
		Material = InBaseMaterialInstance->GetMaterial();
		MaterialInstance = InBaseMaterialInstance;
		bReallocate = true;
	}

	if (DerivedMaterialInstances != InDerivedMaterialInstances)
	{
		// TODO avoid copy
		DerivedMaterialInstances = InDerivedMaterialInstances;
		bReallocate = true;
	}

	if (bReallocate)
	{
		AllocateMaterialResources();
	}
}

bool FShaderPlatformSettings::CheckShaders(bool bIgnoreCooldown)
{
	if (Material == nullptr)
	{
		return false;
	}

	bool bRefreshStatUI = false;

	// don't refresh stats too often
	const double kMinimumTimeBetweenCompilationsSeconds = 1.5;

	const double CurrentTime = FPlatformTime::Seconds();

	// check and triggers shader recompilation if needed
	for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
	{
		auto &Data = PlatformData[QualityLevelIndex];

		const bool bNeedsShaders = (bPresentInGrid && Data.bExtractStats) || Data.bExtractCode;
		const bool bCooledDown = bIgnoreCooldown || (CurrentTime - Data.LastTimeCompilationRequested) > kMinimumTimeBetweenCompilationsSeconds;

		bool bTriggeredCompilation = false;

		for (int32 InstanceIndex = 0; InstanceIndex < Data.Instances.Num(); ++InstanceIndex)
		{
			auto& Instance = Data.Instances[InstanceIndex]; 

			if (Instance.bNeedShaderRecompilation && bNeedsShaders)
			{
				bRefreshStatUI = true;

				// reset even if we don't immediately recompile to improve UI feedback time
				Instance.ShaderStatsInfo.Reset();

				if (bCooledDown)
				{
					Instance.MaterialResourcesStats->CancelCompilation();

					Material->UpdateCachedExpressionData();

					if (MaterialInstance != nullptr)
					{
						MaterialInstance->UpdateCachedData();
					}

					TMap<FName, TArray<FMaterialStatsUtils::FRepresentativeShaderInfo>> ShaderTypeNamesAndDescriptions;
					FMaterialStatsUtils::GetRepresentativeShaderTypesAndDescriptions(ShaderTypeNamesAndDescriptions, Instance.MaterialResourcesStats);

					TArray<const FVertexFactoryType*> VFTypes;
					TArray<const FShaderPipelineType*> PipelineTypes;
					TArray<const FShaderType*> ShaderTypes;

					for (auto& DescriptionPair : ShaderTypeNamesAndDescriptions)
					{
						const FVertexFactoryType* VFType = FindVertexFactoryType(DescriptionPair.Key);
						check(VFType);

						auto& DescriptionArray = DescriptionPair.Value;
						for (const FMaterialStatsUtils::FRepresentativeShaderInfo& ShaderInfo : DescriptionArray)
						{
							const FShaderType* ShaderType = FindShaderTypeByName(ShaderInfo.ShaderName);

							// in compile only setting we only care if derived MI's are compiling at all
							// so we only take one shader combination not to slow down material stats window too much
							// it's likely sufficient to treat first shader as most complex, otherwise we need to refactor FindShaderTypeByName
							if (Instance.bOnlyCompileMostComplexShader && VFTypes.Num() >= 1)
							{
								break;
							}

							if (ShaderType && VFType)
							{
								VFTypes.Add(VFType);
								ShaderTypes.Add(ShaderType);
								PipelineTypes.Add(nullptr);
							}
						}
					}

					// Prepare the resource for compilation, but don't compile the completed shader map.
					const bool bSuccess = Instance.MaterialResourcesStats->CacheShaders(PlatformShaderID, EMaterialShaderPrecompileMode::None);

					if (bSuccess)
					{
						// Compile just the types we want.
						Instance.MaterialResourcesStats->CacheGivenTypes(PlatformShaderID, VFTypes, PipelineTypes, ShaderTypes);
					}

					Instance.bCompilingShaders = true;
					Instance.bUpdateShaderCode = true;
					Instance.bNeedShaderRecompilation = false;

					bTriggeredCompilation = true;
				}
			}
		}

		if (bTriggeredCompilation)
		{
			Data.LastTimeCompilationRequested = CurrentTime;
		}
	}

	return bRefreshStatUI;
}

bool FShaderPlatformSettings::Update()
{
	bool bRetValue = CheckShaders(false);

	// if a shader compilation has been requested check if completed and extract shader names needed by code viewer combo-box
	for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; ++QualityLevelIndex)
	{
		auto &Data = PlatformData[QualityLevelIndex];

		for (int32 InstanceIndex = 0; InstanceIndex < Data.Instances.Num(); ++InstanceIndex)
		{
			auto& Instance = Data.Instances[InstanceIndex]; 

			if (Instance.bCompilingShaders)
			{
				FMaterialResource* Resource = Instance.MaterialResourcesStats;

				// if compilation is complete extract the list of compiled shader names
				const bool bCompilationFinished = Resource->IsCompilationFinished();
				if (bCompilationFinished)
				{
					Instance.bCompilingShaders = false;
					Instance.bUpdateShaderCode = true;

					const FMaterialShaderMap* MaterialShaderMap = Resource->GetGameThreadShaderMap();
					if (MaterialShaderMap != nullptr)
					{
						TMap<FShaderId, TShaderRef<FShader>> ShaderMap;
						MaterialShaderMap->GetShaderList(ShaderMap);

						Instance.ArrShaderEntries.Empty();
						for (const auto& Entry : ShaderMap)
						{
							FShaderType* EntryShader = Entry.Value.GetType();
							FVertexFactoryType* VertexFactory = Entry.Value.GetVertexFactoryType();
							FString ShaderName = FString::Printf(TEXT("%s/%s"), VertexFactory ? VertexFactory->GetName() : TEXT("NullVF"), EntryShader->GetName());
							Instance.ArrShaderEntries.Add(MakeShareable(new FMaterialShaderEntry{ Entry.Key, ShaderName }));
						}

						if (Instance.ArrShaderEntries.Num() > 0)
						{
							Instance.ArrShaderEntries.Sort([](const TSharedPtr<FMaterialShaderEntry>& First, const TSharedPtr<FMaterialShaderEntry>& Second)
								{
									return First->Text < Second->Text;
								});

							Instance.ComboBoxSelectedEntry = *Instance.ArrShaderEntries[0];
						}
					}

					FMaterialStatsUtils::ExtractMatertialStatsInfo(PlatformShaderID, Instance.ShaderStatsInfo, Resource);

					Instance.bNeedToWarnAboutCompilationErrors = Resource->GetCompileErrors().Num() > 0;

					bRetValue = true;
				}
			}
		}
	}

	return bRetValue;
}

bool FShaderPlatformSettings::CachePendingShaders()
{
	const bool bNeedsGridRefresh = CheckShaders(true);

	FString MaterialName;
	TArray<FMaterial*> MaterialsToCompile;
	for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
	{
		for (int32 InstanceIndex = 0; InstanceIndex < PlatformData[QualityLevelIndex].Instances.Num(); ++InstanceIndex)
		{
			auto& Instance = PlatformData[QualityLevelIndex].Instances[InstanceIndex]; 

			if (Instance.bCompilingShaders)
			{
				if (MaterialName.IsEmpty())
				{
					MaterialName = Instance.MaterialResourcesStats->GetFriendlyName(); 
				}

				MaterialsToCompile.Add(Instance.MaterialResourcesStats);
			}
		}
	}

	FMaterial::FinishCompilation(*MaterialName, MaterialsToCompile);

	return bNeedsGridRefresh;
}

/*end FShaderPlatformSettings functions*/
/***********************************************************************************************************************/

/***********************************************************************************************************************/
/*begin FMaterialStats functions*/

FMaterialStats::~FMaterialStats()
{
	SaveSettings();
}

void FMaterialStats::Initialize(IMaterialEditor* InMaterialEditor, const bool bShowMaterialInstancesMenu, const bool bAllowIgnoringCompilationErrors)
{
	MaterialEditor = InMaterialEditor;

	StatsGrid = MakeShareable(new FMaterialStatsGrid(AsShared()));

	BuildShaderPlatformDB(bAllowIgnoringCompilationErrors);

	LoadSettings(bAllowIgnoringCompilationErrors);

	StatsGrid->BuildGrid();

	GridStatsWidget = SNew(SMaterialEditorStatsWidget)
		.MaterialStatsWPtr(SharedThis(this))
		.ShowMaterialInstancesMenu(bShowMaterialInstancesMenu)
		.AllowIgnoringCompilationErrors(bAllowIgnoringCompilationErrors);

	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	FMessageLogInitializationOptions LogOptions;
	// Show Pages so that user is never allowed to clear log messages
	LogOptions.bShowPages = false;
	LogOptions.bShowFilters = false; //TODO - Provide custom filters? E.g. "Critical Errors" vs "Errors" needed for materials?
	LogOptions.bAllowClear = false;
	LogOptions.MaxPageCount = 1;
	OldStatsListing = MessageLogModule.CreateLogListing("MaterialEditorStats", LogOptions);
	OldStatsWidget = MessageLogModule.CreateLogListingWidget(OldStatsListing.ToSharedRef());

	auto ToolkitCommands = MaterialEditor->GetToolkitCommands();
	const FMaterialEditorCommands& Commands = FMaterialEditorCommands::Get();

	ToolkitCommands->MapAction(
		Commands.TogglePlatformStats,
		FExecuteAction::CreateSP(this, &FMaterialStats::ToggleStats),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMaterialStats::IsShowingStats));

	ToolkitCommands->MapAction(
		Commands.ToggleMaterialStats,
		FExecuteAction::CreateSP(this, &FMaterialStats::ToggleOldStats),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMaterialStats::IsShowingOldStats));
}

void FMaterialStats::LoadSettings(const bool bAllowIgnoringCompilationErrors)
{
	Options = NewObject<UMaterialStatsOptions>();

	for (const auto& PlatformEntry : ShaderPlatformStatsDB)
	{
		const EShaderPlatform PlatformID = PlatformEntry.Key;
		const bool bPresentInGrid = !!Options->bPlatformUsed[PlatformID];

		auto Platform = PlatformEntry.Value;
		Platform->SetPresentInGrid(bPresentInGrid);
	}

	for (int32 i = 0; i < EMaterialQualityLevel::Num; ++i)
	{
		const bool bUsed = !!Options->bMaterialQualityUsed[i];
		bArrStatsQualitySelector[(EMaterialQualityLevel::Type)i] = bUsed || bArrStatsQualitySelectorAlwaysOn[(EMaterialQualityLevel::Type)i];

		for (const auto& PlatformEntry : ShaderPlatformStatsDB)
		{
			TSharedPtr<FShaderPlatformSettings> SomePlatform = PlatformEntry.Value;
			if (SomePlatform.IsValid())
			{
				SomePlatform->SetExtractStatsFlag((EMaterialQualityLevel::Type)i, bUsed);
			}
		}
	}

	MaterialStatsDerivedMIOption = Options->MaterialStatsDerivedMIOption;

	// force compilation of derived instances if we're not allowed to ignore compilation errors
	if (!bAllowIgnoringCompilationErrors && (MaterialStatsDerivedMIOption == EMaterialStatsDerivedMIOption::Ignore || MaterialStatsDerivedMIOption == EMaterialStatsDerivedMIOption::InvalidOrMax))
	{
		MaterialStatsDerivedMIOption = EMaterialStatsDerivedMIOption::CompileOnly;
	}
}

void FMaterialStats::SaveSettings()
{
	for (const auto& PlatformEntry : ShaderPlatformStatsDB)
	{
		const EShaderPlatform PlatformID = PlatformEntry.Key;
		const auto Platform = PlatformEntry.Value;

		const bool bPresentInGrid = Platform->IsPresentInGrid();
		Options->bPlatformUsed[PlatformID] = bPresentInGrid ? 1 : 0;
	}

	for (int32 i = 0; i < EMaterialQualityLevel::Num; ++i)
	{
		const bool bQualityPresent = GetStatsQualityFlag((EMaterialQualityLevel::Type)i);

		Options->bMaterialQualityUsed[i] = bQualityPresent ? 1 : 0;
	}

	Options->MaterialStatsDerivedMIOption = MaterialStatsDerivedMIOption;

	Options->SaveConfig();
}

void FMaterialStats::SetShowStats(const bool bValue)
{
	bShowStats = bValue;

	// open/close stats tab
	DisplayStatsGrid(bShowStats);

	GetGridStatsWidget()->RequestRefresh();
}

void FMaterialStats::SetShowOldStats(const bool bValue)
{
	bShowOldStats = bValue;

	// open/close stats tab
	DisplayOldStats(bShowOldStats);
}

void FMaterialStats::ToggleStats()
{
	// Toggle the showing of material stats each time the user presses the show stats button
	SetShowStats(!bShowStats);
}

void FMaterialStats::ToggleOldStats()
{
	// Toggle the showing of material stats each time the user presses the show stats button
	SetShowOldStats(!bShowOldStats);
}

void FMaterialStats::DisplayOldStats(const bool bShow)
{
	if (bShow)
	{
		MaterialEditor->GetTabManager()->TryInvokeTab(OldStatsTabId);
	}
	else if (!bShowOldStats && OldStatsTab.IsValid())
	{
		OldStatsTab.Pin()->RequestCloseTab();
	}
}

void FMaterialStats::DisplayStatsGrid(const bool bShow)
{
	if (bShow)
	{
		MaterialEditor->GetTabManager()->TryInvokeTab(StatsTabId);
	}
	else if (!bShowStats && StatsTab.IsValid())
	{
		StatsTab.Pin()->RequestCloseTab();
	}
}

void FMaterialStats::RefreshStatsGrid()
{
	GetGridStatsWidget()->RequestRefresh();
}

void FMaterialStats::BuildShaderPlatformDB(const bool bAllowIgnoringCompilationErrors)
{
	// set High quality level as always on if we're not allowed to ignore compilation errors in UI
	// this will not allow to disable this particular quality level 
	if (!bAllowIgnoringCompilationErrors)
	{
		bArrStatsQualitySelectorAlwaysOn[EMaterialQualityLevel::High] = true;
		bArrStatsQualitySelector[EMaterialQualityLevel::High] = true;
	}

#if PLATFORM_WINDOWS
	// DirectX
	AddShaderPlatform(EPlatformCategoryType::Desktop, SP_PCD3D_SM5, TEXT("DirectX SM5"), true, TEXT("Desktop, DirectX, Shader Model 5"));
	AddShaderPlatform(EPlatformCategoryType::Desktop, SP_PCD3D_SM6, TEXT("DirectX SM6"), true, TEXT("Desktop, DirectX, Shader Model 6"), !bAllowIgnoringCompilationErrors);
	AddShaderPlatform(EPlatformCategoryType::Desktop, SP_PCD3D_ES3_1, TEXT("DirectX  Mobile"), true, TEXT("Desktop, DirectX, Mobile"), !bAllowIgnoringCompilationErrors);
#endif

	// Vulkan
	AddShaderPlatform(EPlatformCategoryType::Desktop, SP_VULKAN_SM5, TEXT("Vulkan SM5"), true, TEXT("Desktop, Vulkan, Shader Model 5"));
	AddShaderPlatform(EPlatformCategoryType::Desktop, SP_VULKAN_SM6, TEXT("Vulkan SM6"), true, TEXT("Desktop, Vulkan, Shader Model 6"));
	AddShaderPlatform(EPlatformCategoryType::Desktop, SP_VULKAN_PCES3_1, TEXT("Vulkan Mobile"), true, TEXT("Desktop, Vulkan, Mobile"));

	// Android
	AddShaderPlatform(EPlatformCategoryType::Android, SP_OPENGL_ES3_1_ANDROID, TEXT("Android GLES Mobile"), true, TEXT("Android, OpenGLES Mobile"));
	AddShaderPlatform(EPlatformCategoryType::Android, SP_VULKAN_ES3_1_ANDROID, TEXT("Android Vulkan Mobile"), true, TEXT("Android, Vulkan Mobile"));
	AddShaderPlatform(EPlatformCategoryType::Android, SP_VULKAN_SM5_ANDROID, TEXT("Android Vulkan SM5"), true, TEXT("Android, Vulkan SM5"));

#if PLATFORM_MAC
	// macOS
	AddShaderPlatform(EPlatformCategoryType::Desktop, SP_METAL_SM5, TEXT("Metal SM5"), true, TEXT("macOS, Metal, Shader Model 5"));
	AddShaderPlatform(EPlatformCategoryType::Desktop, SP_METAL_SM6, TEXT("Metal SM6"), true, TEXT("macOS, Metal, Shader Model 6"), !bAllowIgnoringCompilationErrors);
	AddShaderPlatform(EPlatformCategoryType::Desktop, SP_METAL_MRT_MAC, TEXT("Metal SM5 (MRT)"), true, TEXT("macOS, Metal, Shader Model 5"));
#endif

	// iOS
	AddShaderPlatform(EPlatformCategoryType::IOS, SP_METAL, TEXT("Metal"), true, TEXT("iOS, Metal, Mobile"));
	AddShaderPlatform(EPlatformCategoryType::IOS, SP_METAL_MRT, TEXT("Metal MRT"), true, TEXT("iOS, Metal, Shader Model 5"));
	AddShaderPlatform(EPlatformCategoryType::IOS, SP_METAL_SIM, TEXT("Metal Simulator"), true, TEXT("iOS, Metal, Mobile"));

	ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();

	// Add platform extensions
	for (int32 StaticPlatform = SP_StaticPlatform_First; StaticPlatform <= SP_StaticPlatform_Last; ++StaticPlatform)
	{
		EShaderPlatform ShaderPlatform = (EShaderPlatform)StaticPlatform;
		if (FDataDrivenShaderPlatformInfo::IsValid(ShaderPlatform))
		{
			bool bIsConsole = FDataDrivenShaderPlatformInfo::GetIsConsole(ShaderPlatform);

			const FName ShaderFormat = LegacyShaderPlatformToShaderFormat(ShaderPlatform);
			if (TPM.FindShaderFormat(ShaderFormat) != nullptr)
			{
				const FName PlatformName = ShaderPlatformToPlatformName(ShaderPlatform);
				AddShaderPlatform(bIsConsole ? EPlatformCategoryType::Console : EPlatformCategoryType::Desktop, ShaderPlatform, PlatformName, true, PlatformName.ToString());
			}
		}
	}

}

TSharedPtr<FShaderPlatformSettings> FMaterialStats::AddShaderPlatform(const EPlatformCategoryType PlatformType, const EShaderPlatform PlatformID, const FName PlatformName, const bool bAllowCodeView, const FString& Description, const bool bAlwaysOn)
{
	if (!FDataDrivenShaderPlatformInfo::IsValid(PlatformID))
	{
		return TSharedPtr<FShaderPlatformSettings>();
	}

	ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
	const IShaderFormat* ShaderFormat = TPM.FindShaderFormat(LegacyShaderPlatformToShaderFormat(PlatformID));
	const bool bAllowPresenceInGrid = ShaderFormat ? ShaderFormat->CanCompileBinaryShaders() : false;

	TSharedPtr<FShaderPlatformSettings> PlatformPtr = MakeShareable(new FShaderPlatformSettings(PlatformType, PlatformID, PlatformName, bAllowPresenceInGrid, bAllowCodeView, Description, bAlwaysOn));
	ShaderPlatformStatsDB.Add(PlatformID, PlatformPtr);

	auto& ArrayPlatforms = PlatformTypeDB.FindOrAdd(PlatformType);
	ArrayPlatforms.Add(PlatformPtr);

	for (int32 i = 0; i < EMaterialQualityLevel::Num; ++i)
	{
		PlatformPtr->SetExtractStatsFlag((EMaterialQualityLevel::Type)i, bAlwaysOn && bArrStatsQualitySelectorAlwaysOn[i]);
	}

	return PlatformPtr;
}

void FMaterialStats::SignalMaterialChanged()
{
	ExtractHLSLCode();

	for (auto Entry : ShaderPlatformStatsDB)
	{
		for (int32 i = 0; i < EMaterialQualityLevel::Num; ++i)
			Entry.Value->SetNeedShaderCompilation((EMaterialQualityLevel::Type)i, true, MaterialStatsDerivedMIOption == EMaterialStatsDerivedMIOption::CompileOnly);
	}
}

bool FMaterialStats::SwitchShaderPlatformUseStats(const EShaderPlatform PlatformID)
{
	bool bRetValue = false;

	auto* Item = ShaderPlatformStatsDB.Find(PlatformID);
	if (Item != nullptr)
	{
		bRetValue = (*Item)->FlipPresentInGrid();

		GetStatsGrid()->OnAddOrRemovePlatform(*Item);
		SaveSettings();
	}

	return bRetValue;
}

void FMaterialStats::SetStatusQualityFlag(const EMaterialQualityLevel::Type QualityLevel, const bool bValue)
{
	check(QualityLevel < EMaterialQualityLevel::Num);

	bArrStatsQualitySelector[QualityLevel] = bValue || bArrStatsQualitySelectorAlwaysOn[QualityLevel];

	for (const auto& PlatformEntry : ShaderPlatformStatsDB)
	{
		TSharedPtr<FShaderPlatformSettings> SomePlatform = PlatformEntry.Value;
		if (SomePlatform.IsValid())
		{
			SomePlatform->SetExtractStatsFlag(QualityLevel, bValue);
		}
	}

	SaveSettings();
}

bool FMaterialStats::SwitchStatsQualityFlag(EMaterialQualityLevel::Type Quality)
{
	check(Quality < EMaterialQualityLevel::Num);

	const bool bValue = !bArrStatsQualitySelector[Quality];
	SetStatusQualityFlag(Quality, bValue);

	return bValue;
}

void FMaterialStats::SetMaterialStatsDerivedMIOption(const EMaterialStatsDerivedMIOption value)
{
	MaterialStatsDerivedMIOption = value;

	if (MaterialEditor != nullptr)
	{
		MaterialEditor->RefreshStatsMaterials();
	}

	SaveSettings();
}

void FMaterialStats::SetShaderPlatformUseCodeView(const EShaderPlatform PlatformID, const EMaterialQualityLevel::Type Quality, const bool bValue)
{
	auto* Item = ShaderPlatformStatsDB.Find(PlatformID);
	if (Item != nullptr)
	{
		(*Item)->SetCodeViewNeeded(Quality, bValue);
	}
}

FName FMaterialStats::GetPlatformName(const EShaderPlatform InEnumValue) const
{
	FName PlatformName = NAME_None;

	auto* Entry = ShaderPlatformStatsDB.Find(InEnumValue);
	if (Entry != nullptr && Entry->IsValid())
	{
		PlatformName = (*Entry)->GetPlatformName();
	}

	return PlatformName;
}

EShaderPlatform FMaterialStats::GetShaderPlatformID(const FName InName) const
{
	for (auto Entry : ShaderPlatformStatsDB)
	{
		if (Entry.Value.Get()->GetPlatformName() == InName)
		{
			return Entry.Key;
		}
	}

	return SP_NumPlatforms;
}

TSharedPtr<FShaderPlatformSettings> FMaterialStats::GetPlatformSettings(const EShaderPlatform PlatformID)
{
	auto* Entry = ShaderPlatformStatsDB.Find(PlatformID);
	if (Entry == nullptr)
	{
		return TSharedPtr<FShaderPlatformSettings>(nullptr);
	}

	return *Entry;
}

TSharedPtr<FShaderPlatformSettings> FMaterialStats::GetPlatformSettings(const FName PlatformName)
{
	const EShaderPlatform PlatformID = GetShaderPlatformID(PlatformName);

	return GetPlatformSettings(PlatformID);
}

FText FMaterialStats::GetShaderCode(const EShaderPlatform PlatformID, const EMaterialQualityLevel::Type QualityType, const int32 InstanceIndex)
{
	auto* Entry = ShaderPlatformStatsDB.Find(PlatformID);
	if (Entry == nullptr)
	{
		return FText::FromString(TEXT("Shader code compiling or not available!"));
	}

	return (*Entry)->GetShaderCode(QualityType, InstanceIndex);
}

void FMaterialStats::Update()
{
	const bool bNeedsUpdate = IsShowingStats() || IsCodeViewWindowActive();

	// don't update materials if we don't need to verify derived MI's
	// this is to preserve old behaviour of speeding up material editor by hiding platform stats
	if (!bNeedsUpdate && !GetProvideDerivedMIFlag())
	{
		return;
	}
	
	// otherwise always update material compilation info so we can check if shaders are compiling at material editor save
	for (const auto& Entry : ShaderPlatformStatsDB)
	{
		auto PlatformStats = Entry.Value;
		bNeedsGridRefresh |= PlatformStats->Update();
	}

	if (!bNeedsUpdate)
	{
		return;
	}

	if (bNeedsGridRefresh)
	{
		// in compile only mode the amount of columns might change if some of MI's fail to compile 
		if (GetMaterialStatsDerivedMIOption() == EMaterialStatsDerivedMIOption::CompileOnly)
		{
			auto GridPtr = GetGridStatsWidget();
			if (GridPtr.IsValid())
			{
				GridPtr->OnColumnNumChanged();
			}
		}
		else
		{
			GetStatsGrid()->OnShaderChanged();
		}

		// update any dependent tabs
		RefreshDependentTabs.Broadcast();

		bNeedsGridRefresh = false;
	}

	ComputeGridWarnings();
}

void FMaterialStats::CacheAndCompilePendingShaders()
{
	for (const auto& Entry : ShaderPlatformStatsDB)
	{
		auto PlatformStats = Entry.Value;
		bNeedsGridRefresh |= PlatformStats->CachePendingShaders();
	}
}

void FMaterialStats::SetMaterial(UMaterial *InMaterial, const TArray<TObjectPtr<UMaterialInstance>>& InDerivedMaterialInstances)
{
	if (MaterialInterface != InMaterial || DerivedMaterialInstances != InDerivedMaterialInstances)
	{
		MaterialInterface = InMaterial;
		DerivedMaterialInstances = InDerivedMaterialInstances;

		for (const auto& Entry : ShaderPlatformStatsDB)
		{
			auto& Platform = Entry.Value;
			if (Platform.IsValid())
			{
				Platform->SetMaterial(InMaterial, nullptr, DerivedMaterialInstances);
			}
		}

		auto GridPtr = GetGridStatsWidget();
		if (GridPtr.IsValid())
		{
			GridPtr->OnColumnNumChanged();
		}
	}
}

void FMaterialStats::SetMaterial(UMaterialInstance *InMaterialInstance)
{
	if (MaterialInterface != InMaterialInstance || DerivedMaterialInstances.Num() > 0)
	{
		MaterialInterface = InMaterialInstance;
		DerivedMaterialInstances.Empty();

		for (const auto& Entry : ShaderPlatformStatsDB)
		{
			auto& Platform = Entry.Value;
			if (Platform.IsValid())
			{
				Platform->SetMaterial(nullptr, InMaterialInstance, DerivedMaterialInstances);
			}
		}

		auto GridPtr = GetGridStatsWidget();
		if (GridPtr.IsValid())
		{
			GridPtr->OnColumnNumChanged();
		}
	}
}

bool FMaterialStats::AnyNewCompilationErrors(const int32 StartingFromInstanceIndex)
{
	bool bNewCompilationErrors = false;

	for (const auto& Entry : ShaderPlatformStatsDB)
	{
		auto& Platform = Entry.Value;
		if (Platform.IsValid())
		{
			for (int32 QualityLevel = 0; QualityLevel < EMaterialQualityLevel::Num; ++QualityLevel)
			{
				for (int32 InstanceIndex = StartingFromInstanceIndex; InstanceIndex < Platform->GetPlatformData(static_cast<EMaterialQualityLevel::Type>(QualityLevel)).Instances.Num(); ++InstanceIndex)
				{
					auto &Data = Platform->GetInstanceData(static_cast<EMaterialQualityLevel::Type>(QualityLevel), InstanceIndex);
					bNewCompilationErrors |= Data.bNeedToWarnAboutCompilationErrors;
					Data.bNeedToWarnAboutCompilationErrors = false;
				}
			}
		}
	}

	return bNewCompilationErrors;
}

TSharedRef<class SDockTab> FMaterialStats::SpawnTab_HLSLCode(const class FSpawnTabArgs& Args)
{
	auto CodeViewUtility =
		SNew(SVerticalBox)
		// Copy Button
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew(SButton)
				.Text(LOCTEXT("CopyHLSLButton", "Copy"))
				.ToolTipText(LOCTEXT("CopyHLSLButtonToolTip", "Copies all HLSL code to the clipboard."))
				.ContentPadding(3)
				.OnClicked_Lambda
				(
					[Code = &HLSLCode]()
					{
						FPlatformApplicationMisc::ClipboardCopy(**Code);
						return FReply::Handled();
					}
				)
			]
		]
		// Separator
		+ SVerticalBox::Slot()
		.FillHeight(1)
		[
			SNew(SSeparator)
		];

	auto CodeView =
		SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(5)
		[
			SNew(STextBlock)
			.Text_Lambda([Code = &HLSLCode]() { return FText::FromString(*Code); })
		];


	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("HLSLCodeTitle", "HLSL Code"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				CodeViewUtility
			]
			+ SVerticalBox::Slot()
			.FillHeight(1)
			[
				CodeView
			]
		];

	HLSLTab = SpawnedTab;

	ExtractHLSLCode();

	return SpawnedTab;
}

TSharedRef<SDockTab> FMaterialStats::SpawnTab_ShaderCode(const FSpawnTabArgs& Args, const EShaderPlatform PlatformID, const EMaterialQualityLevel::Type QualityLevel, const int32 InstanceIndex)
{
	SetShaderPlatformUseCodeView(PlatformID, QualityLevel, true);

	const FString PlatformName = GetPlatformName(PlatformID).ToString();
	const FString FullPlatformName = PlatformName + TEXT(" -- ") + FMaterialStatsUtils::MaterialQualityToString(QualityLevel);

	TSharedPtr<FShaderPlatformSettings> PlatformPtr = GetPlatformSettings(PlatformID);
	check(PlatformPtr.IsValid());

	const TArray<TSharedPtr<FMaterialShaderEntry>> *ShaderEntriesPtr = PlatformPtr->GetShaderEntries(QualityLevel, InstanceIndex);

	if (ShaderEntriesPtr == nullptr)
	{
		// SComboBox needs a valid pointer otherwise follow up SetItemsSource will fail.
		static TArray<TSharedPtr<FMaterialShaderEntry>> EmptyShaderEntriesArray = {};
		ShaderEntriesPtr = &EmptyShaderEntriesArray;
	}

	ensure(ShaderEntriesPtr != nullptr);
	TSharedRef<SComboBox<TSharedPtr<FMaterialShaderEntry>>> ShaderBox = SNew(SComboBox<TSharedPtr<FMaterialShaderEntry>>)
		.OptionsSource(ShaderEntriesPtr)
		.OnGenerateWidget_Lambda([](TSharedPtr<FMaterialShaderEntry> Value) { return SNew(STextBlock).Text(FText::FromString(Value->Text)); })
		.OnSelectionChanged_Lambda([PlatformPtr, QualityLevel, InstanceIndex](TSharedPtr<FMaterialShaderEntry> Item, ESelectInfo::Type SelectInfo) { PlatformPtr->OnShaderViewComboSelectionChanged(Item, QualityLevel, InstanceIndex); })
		[
			SNew(STextBlock)
			.Text_Lambda([PlatformPtr, QualityLevel, InstanceIndex]() { return PlatformPtr->GetSelectedShaderViewComboText(QualityLevel, InstanceIndex); })
		];

	// Refresh ShaderBox when shaders are updated
	FDelegateHandle ShaderBoxUpdater = RefreshDependentTabs.AddLambda([PlatformPtrWeakPtr = PlatformPtr.ToWeakPtr(), ShaderBoxWeakPtr = ShaderBox.ToWeakPtr(), QualityLevel, InstanceIndex]()
	{
		TSharedPtr<FShaderPlatformSettings> PlatformPtr = PlatformPtrWeakPtr.Pin();
		TSharedPtr<SComboBox<TSharedPtr<FMaterialShaderEntry>>> ShaderBox = ShaderBoxWeakPtr.Pin();
		if (PlatformPtr.IsValid() && ShaderBox.IsValid())
		{
			ShaderBox->SetItemsSource(PlatformPtr->GetShaderEntries(QualityLevel, InstanceIndex));
		}
	});

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.OnTabClosed_Lambda([MaterialStatsWeakPtr = this->AsWeak(), ShaderBoxUpdater](TSharedRef<SDockTab> Tab)
		{
			TSharedPtr<FMaterialStats> MaterialStats = MaterialStatsWeakPtr.Pin();
			if (MaterialStats.IsValid() && ShaderBoxUpdater.IsValid())
			{
				MaterialStats->RefreshDependentTabs.Remove(ShaderBoxUpdater);
			}
		})
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SVerticalBox)
				// Copy Button
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2.0f, 0.0f)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					[
						SNew(SButton)
						.Text(LOCTEXT("CopyShaderCodeButton", "Copy"))
						.ToolTipText(LOCTEXT("CopyShaderCodeButtonToolTip", "Copies all shader code to the clipboard."))
						.ContentPadding(3)
						.OnClicked_Lambda
						(
							[MaterialStats = TWeakPtr<FMaterialStats>(SharedThis(this)), PlatformID, QualityLevel, InstanceIndex]()
							{
								auto StatsPtr = MaterialStats.Pin();
								if (StatsPtr.IsValid())
								{
									FText ShaderCode = StatsPtr->GetShaderCode(PlatformID, QualityLevel, InstanceIndex);
									FPlatformApplicationMisc::ClipboardCopy(*ShaderCode.ToString());
									return FReply::Handled();
								}

								return FReply::Unhandled();
							}
						)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2.0f, 2.0f)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					[
						SNew(STextBlock)
						.Text(FText::FromString(FullPlatformName))
					]
				]
				// Separator
				+ SVerticalBox::Slot()
				.FillHeight(1)
				[
					SNew(SSeparator)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				ShaderBox
			]
			+ SVerticalBox::Slot()
			.FillHeight(1)
			[
				PlatformPtr->GetShaderViewerScrollBox(QualityLevel).ToSharedRef()
			]
		];

	PlatformPtr->SetCodeViewerTab(QualityLevel, SpawnedTab);

	SpawnedTab->SetLabel(FText::FromString(PlatformName));

	return SpawnedTab;
}

FName FMaterialStats::MakeTabName(const EPlatformCategoryType PlatformType, const EShaderPlatform ShaderPlatformType, const EMaterialQualityLevel::Type QualityLevel, const int32 InstanceIndex)
{
	const FString TabName = FMaterialStatsUtils::GetPlatformTypeName(PlatformType) + FMaterialStatsUtils::ShaderPlatformTypeName(ShaderPlatformType) + FMaterialStatsUtils::MaterialQualityToString(QualityLevel) + FString::FromInt(InstanceIndex);

	return FName(*TabName);
}

void FMaterialStats::BuildViewShaderCodeMenus()
{
	auto TabManager = MaterialEditor->GetTabManager();
	auto ParentCategoryRef = MaterialEditor->GetWorkspaceMenuCategory();

	TSharedPtr<FWorkspaceItem> PlatformGroupMenuItem = ParentCategoryRef->AddGroup(LOCTEXT("ViewShaderCodePlatformsGroupMenu", "Shader Code"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "MaterialEditor.Tabs.HLSLCode"));

	// add hlsl code viewer tab
	TabManager->RegisterTabSpawner( HLSLCodeTabId, FOnSpawnTab::CreateSP(this, &FMaterialStats::SpawnTab_HLSLCode))
		.SetDisplayName( LOCTEXT("HLSLCodeTab", "HLSL Code") )
		.SetGroup( PlatformGroupMenuItem.ToSharedRef() )
		.SetIcon( FSlateIcon(FAppStyle::GetAppStyleSetName(), "MaterialEditor.Tabs.HLSLCode") );

	const int32 InstanceIndex = 0; // only work on base material

	for (auto MapEntry : PlatformTypeDB)
	{
		const EPlatformCategoryType PlatformType = MapEntry.Key;

		const FString PlatformName = FMaterialStatsUtils::GetPlatformTypeName(PlatformType);
		TSharedPtr<FWorkspaceItem> PlatformMenuItem = PlatformGroupMenuItem->AddGroup(FText::FromString(PlatformName), FSlateIcon(FAppStyle::GetAppStyleSetName(), "MaterialEditor.Tabs.HLSLCode"));

		const auto& ArrShaderPlatforms = MapEntry.Value;

		for (int32 i = 0; i < ArrShaderPlatforms.Num(); ++i)
		{
			const auto& PlatformPtr = ArrShaderPlatforms[i];

			if (!PlatformPtr.IsValid())
			{
				continue;
			}

			const EShaderPlatform PlatformID = PlatformPtr->GetPlatformShaderType();

			if (PlatformID == SP_NumPlatforms || !PlatformPtr->IsCodeViewAllowed())
			{
				continue;
			}

			const FString ShaderPlatformName = PlatformPtr->GetPlatformName().ToString();
			TSharedPtr<FWorkspaceItem> ShaderPlatformMenuItem = PlatformMenuItem->AddGroup(FText::FromString(ShaderPlatformName), FSlateIcon(FAppStyle::GetAppStyleSetName(), "MaterialEditor.Tabs.HLSLCode"));

			for (int32 q = 0; q < EMaterialQualityLevel::Num; ++q)
			{
				const EMaterialQualityLevel::Type QualityLevel = (EMaterialQualityLevel::Type)q;

				const FString MaterialQualityName = FMaterialStatsUtils::MaterialQualityToString(QualityLevel);
				const FName TabName = MakeTabName(PlatformType, PlatformID, QualityLevel, InstanceIndex);

				TabManager->RegisterTabSpawner(TabName, FOnSpawnTab::CreateSP(this, &FMaterialStats::SpawnTab_ShaderCode, PlatformID, QualityLevel, InstanceIndex))
					.SetGroup(ShaderPlatformMenuItem.ToSharedRef())
					.SetDisplayName(FText::FromString(MaterialQualityName));

				auto CodeScrollBox = SNew(SScrollBox)
					+ SScrollBox::Slot().Padding(5)
					[
						SNew(STextBlock)
						.Text_Lambda([MaterialStats = TWeakPtr<FMaterialStats>(SharedThis(this)), PlatformID, QualityLevel]()
						{
							auto StatsPtr = MaterialStats.Pin();
							if (StatsPtr.IsValid())
							{
								return StatsPtr->GetShaderCode(PlatformID, QualityLevel, InstanceIndex);
							}

							return FText::FromString(TEXT("Error reading shader code!"));
						})
					];

				PlatformPtr->GetPlatformData(QualityLevel).CodeScrollBox = CodeScrollBox;
			}
		}
	}
}

bool FMaterialStats::IsCodeViewWindowActive() const
{
	for (const auto& MapEntry : ShaderPlatformStatsDB)
	{
		TSharedPtr<FShaderPlatformSettings> PlatformPtr = MapEntry.Value;

		for (int q = 0; q < EMaterialQualityLevel::Num; ++q)
		{
			if (PlatformPtr->GetCodeViewerTab((EMaterialQualityLevel::Type)q).IsValid())
			{
				return true;
			}
		}
	}

	return false;
}

TSharedRef<SDockTab> FMaterialStats::SpawnTab_Stats(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == StatsTabId);

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("Platform Stats", "Platform Stats"))
		.OnTabClosed_Lambda([&bShowStats = bShowStats](TSharedRef<SDockTab>) { bShowStats = false; })
		[
			SNew(SBox)
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("MaterialStats")))
			[
				GetGridStatsWidget().ToSharedRef()
			]
		];

	StatsTab = SpawnedTab;

	// like so because the material editor will automatically restore this tab if it was still opened when the editor shuts down
	bShowStats = true;

	return SpawnedTab;
}

TSharedRef<SDockTab> FMaterialStats::SpawnTab_OldStats(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == OldStatsTabId);

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("Stats", "Stats"))
		.OnTabClosed_Lambda([&bShowStats = bShowOldStats](TSharedRef<SDockTab>) { bShowStats = false; })
		[
			SNew(SBox)
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("MaterialStats")))
			[
				OldStatsWidget.ToSharedRef()
			]
		];

	OldStatsTab = SpawnedTab;

	// like so because the material editor will automatically restore this tab if it was still opened when the editor shuts down
	bShowOldStats = true;

	return SpawnedTab;
}

void FMaterialStats::BuildStatsTab()
{
	const auto ParentCategoryRef = MaterialEditor->GetWorkspaceMenuCategory();
	auto TabManager = MaterialEditor->GetTabManager();

	TabManager->RegisterTabSpawner(StatsTabId, FOnSpawnTab::CreateSP(this, &FMaterialStats::SpawnTab_Stats))
		.SetDisplayName(LOCTEXT("StatsTab", "Platform Stats"))
		.SetGroup(ParentCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::Get().GetStyleSetName(),"MaterialEditor.TogglePlatformStats.Tab"));
}

void FMaterialStats::BuildOldStatsTab()
{
	const auto ParentCategoryRef = MaterialEditor->GetWorkspaceMenuCategory();
	auto TabManager = MaterialEditor->GetTabManager();

	TabManager->RegisterTabSpawner(OldStatsTabId, FOnSpawnTab::CreateSP(this, &FMaterialStats::SpawnTab_OldStats))
		.SetDisplayName(LOCTEXT("OldStatsTab", "Stats"))
		.SetGroup(ParentCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::Get().GetStyleSetName(), "MaterialEditor.ToggleMaterialStats.Tab"));
}

void FMaterialStats::RegisterTabs()
{
	BuildStatsTab();
	BuildOldStatsTab();
	BuildViewShaderCodeMenus();
}

void FMaterialStats::UnregisterTabs()
{
	auto TabManager = MaterialEditor->GetTabManager();
	
	const int32 InstanceIndex = 0;

	for (auto MapEntry : PlatformTypeDB)
	{
		const EPlatformCategoryType PlatformType = MapEntry.Key;
		TArray<TSharedPtr<FShaderPlatformSettings>>& ArrShaderPlatforms = MapEntry.Value;

		for (int32 i = 0; i < ArrShaderPlatforms.Num(); ++i)
		{
			TSharedPtr<FShaderPlatformSettings> PlatformPtr = ArrShaderPlatforms[i];

			if (!PlatformPtr.IsValid())
			{
				continue;
			}

			const EShaderPlatform ShaderPlatformID = PlatformPtr->GetPlatformShaderType();

			for (int32 q = 0; q < EMaterialQualityLevel::Num; ++q)
			{
				const FName TabName = MakeTabName(PlatformType, ShaderPlatformID, (EMaterialQualityLevel::Type)q, InstanceIndex);

				TabManager->UnregisterTabSpawner(TabName);
			}
		}
	}

	TabManager->UnregisterTabSpawner(StatsTabId);
	TabManager->UnregisterTabSpawner(OldStatsTabId);
	TabManager->UnregisterTabSpawner(HLSLCodeTabId);
}

void FMaterialStats::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Options);
}

void FMaterialStats::ComputeGridWarnings()
{
	auto GridPtr = GetGridStatsWidget();
	if (!GridPtr.IsValid())
	{
		return;
	}

	// no need to update warnings that often
	const double kMinimumTimeBetweenWarningsRefreshSeconds = 0.1;
	const double CurrentTime = FPlatformTime::Seconds();
	const bool bCooledDown = (CurrentTime - LastGridMessagesUpdate) > kMinimumTimeBetweenWarningsRefreshSeconds;

	if (!bCooledDown)
	{
		return;
	}

	LastGridMessagesUpdate = CurrentTime;

	TArray<FString> Messages;
	TArray<bool> IsErrorMessage;

	bool bAnyQualityPresent = false;
	for (int32 q = 0; q < EMaterialQualityLevel::Num; ++q)
	{
		auto Quality = static_cast<EMaterialQualityLevel::Type>(q);
		bAnyQualityPresent |= GetStatsQualityFlag(Quality);
	}

	if (!bAnyQualityPresent)
	{
		Messages.Emplace("No material quality selected. Please use the 'Settings' button and choose the desired quality level to be analyzed.");
		IsErrorMessage.Add(false);
	}

	for (const auto& Entry : ShaderPlatformStatsDB)
	{
		auto& Platform = Entry.Value;
		if (Platform.IsValid())
		{
			for (int32 QualityLevel = 0; QualityLevel < EMaterialQualityLevel::Num; ++QualityLevel)
			{
				for (int32 InstanceIndex = 0; InstanceIndex < Platform->GetPlatformData(static_cast<EMaterialQualityLevel::Type>(QualityLevel)).Instances.Num(); ++InstanceIndex)
				{
					auto& Data = Platform->GetInstanceData(static_cast<EMaterialQualityLevel::Type>(QualityLevel), InstanceIndex);
					auto& Errors = Data.MaterialResourcesStats->GetCompileErrors();
					for (int32 ErrorIndex = 0; ErrorIndex < Errors.Num(); ++ErrorIndex)
					{
						const auto& PlatformName = Platform->GetPlatformName().ToString();
						const auto& AssetName = GetMaterialName(InstanceIndex);
						const FString QualityName = FMaterialStatsUtils::MaterialQualityToShortString((EMaterialQualityLevel::Type)QualityLevel);

						FString Message = FString::Printf(TEXT("[%s/%s:%s] %s"), *AssetName, *PlatformName, *QualityName, *Errors[ErrorIndex]);
						Messages.Add(Message);
						IsErrorMessage.Add(true);
					}
				}
			}
		}
	}

	bool bAnyPlatformPresent = false;
	bool bPlatformsNeedOfflineCompiler = false;

	const auto& PlatformList = GetPlatformsDB();
	for (auto Pair : PlatformList)
	{
		auto& PlatformPtr = Pair.Value;

		if (PlatformPtr->IsPresentInGrid())
		{
			bAnyPlatformPresent = true;

			auto ShaderPlatformType = PlatformPtr->GetPlatformShaderType();
			bool bNeedsOfflineCompiler = FMaterialStatsUtils::PlatformNeedsOfflineCompiler(ShaderPlatformType);
			if (bNeedsOfflineCompiler)
			{
				bool bCompilerAvailable = FMaterialStatsUtils::IsPlatformOfflineCompilerAvailable(ShaderPlatformType);

				if (!bCompilerAvailable)
				{
					auto WarningString = FString::Printf(TEXT("Platform %s needs an offline shader compiler to extract instruction count. Please check 'Editor Preferences' -> 'Content Editors' -> 'Material Editor' for additional settings."), *PlatformPtr->GetPlatformName().ToString());
					Messages.Add(MoveTemp(WarningString));
					IsErrorMessage.Add(false);
				}
			}
		}
	}

	if (!bAnyPlatformPresent)
	{
		Messages.Emplace("No platform selected. Please use the 'Settings' button and choose desired platform to be analyzed.");
		IsErrorMessage.Add(false);
	}

	if (Messages != LastGridMessages)
	{
		LastGridMessages = Messages;

		GridPtr->ClearMessages();

		check(Messages.Num() == IsErrorMessage.Num());
		for (int32 i = 0; i < Messages.Num(); ++i)
		{
			GridPtr->AddMessage(Messages[i], IsErrorMessage[i]);
		}
	}
}

void FMaterialStats::ExtractHLSLCode()
{
#define MARKTAG TEXT("/*MARK_")
#define MARKTAGLEN 7

	HLSLCode = TEXT("");

 	if (!HLSLTab.IsValid())
	{
		return;
	}

	FString MarkupSource;
	if (MaterialInterface != nullptr && MaterialInterface->GetMaterialResource(GMaxRHIFeatureLevel)->GetMaterialExpressionSource(MarkupSource))
	{
		// Remove line-feeds and leave just CRs so the character counts match the selection ranges.
		MarkupSource.ReplaceInline(TEXT("\r"), TEXT(""));

		// Improve formatting: Convert tab to 4 spaces since STextBlock (currently) doesn't show tab characters
		MarkupSource.ReplaceInline(TEXT("\t"), TEXT("    "));

		// Extract highlight ranges from markup tags

		// Make a copy so we can insert null terminators.
		TCHAR* MarkupSourceCopy = new TCHAR[MarkupSource.Len() + 1];
		FCString::Strcpy(MarkupSourceCopy, MarkupSource.Len() + 1, *MarkupSource);

		TCHAR* Ptr = MarkupSourceCopy;
		while (Ptr && *Ptr != '\0')
		{
			TCHAR* NextTag = FCString::Strstr(Ptr, MARKTAG);
			if (!NextTag)
			{
				// No more tags, so we're done!
				HLSLCode += Ptr;
				break;
			}

			// Copy the text up to the tag.
			*NextTag = '\0';
			HLSLCode += Ptr;

			// Advance past the markup tag to see what type it is (beginning or end)
			NextTag += MARKTAGLEN;
			int32 TagNumber = FCString::Atoi(NextTag + 1);
			Ptr = FCString::Strstr(NextTag, TEXT("*/")) + 2;
		}

		delete[] MarkupSourceCopy;
	}
}

#undef LOCTEXT_NAMESPACE

/*end FMaterialStats functions*/
/***********************************************************************************************************************/
