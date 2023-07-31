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
#include "Preferences/MaterialStatsOptions.h"
#include "MaterialEditorSettings.h"
#include "ShaderCompiler.h"

#include "Modules/ModuleManager.h"
#include "MessageLogModule.h"

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
	const FString& _Description)
	:
	PlatformType(_PlatformType),
	PlatformShaderID(_ShaderPlatformID),
	PlatformName(_Name),
	PlatformDescription(_Description),
	bAllowCodeView(_bAllowCodeView),
	bAllowPresenceInGrid(_bAllowPresenceInGrid)
{
	PlatformNameID = *FMaterialStatsUtils::ShaderPlatformTypeName(PlatformShaderID);
}

void FShaderPlatformSettings::ClearResources()
{
	TArray<TRefCountPtr<FMaterial>> MaterialsToDeleteOnRenderThread;

	// free material resources
	for (int32 i = 0; i < EMaterialQualityLevel::Num; ++i)
	{
		FMaterialResourceStats* Resource = PlatformData[i].MaterialResourcesStats;
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
			PlatformData[i].MaterialResourcesStats = nullptr;
		}

		PlatformData[i].ArrShaderEntries.Empty();

		PlatformData[i].bCompilingShaders = false;
		PlatformData[i].bNeedShaderRecompilation = true;
	}

	FMaterial::DeleteMaterialsOnRenderThread(MaterialsToDeleteOnRenderThread);
}

FText FShaderPlatformSettings::GetSelectedShaderViewComboText(EMaterialQualityLevel::Type QualityLevel) const
{
	if (PlatformData[QualityLevel].ArrShaderEntries.Num() == 0)
	{
		return FText::FromString(TEXT("-Compiling-Shaders-"));
	}

	return FText::FromString(PlatformData[QualityLevel].ComboBoxSelectedEntry.Text);
}

void FShaderPlatformSettings::OnShaderViewComboSelectionChanged(TSharedPtr<FMaterialShaderEntry> Item, EMaterialQualityLevel::Type QualityType)
{
	if (Item.IsValid())
	{
		PlatformData[QualityType].ComboBoxSelectedEntry = *Item.Get();
		PlatformData[QualityType].bUpdateShaderCode = true;
	}
}

FText FShaderPlatformSettings::GetShaderCode(const EMaterialQualityLevel::Type QualityType)
{
	// if there were no change to the material return the cached shader code
	if (!PlatformData[QualityType].bUpdateShaderCode)
	{
		return PlatformData[QualityType].ShaderCode;
	}

	PlatformData[QualityType].ShaderCode = LOCTEXT("ShaderCodeMsg", "Shader code compiling or not available!");

	FMaterialResource *Resource = PlatformData[QualityType].MaterialResourcesStats;
	const FMaterialShaderMap* MaterialShaderMap = Resource->GetGameThreadShaderMap();

	const bool bCompilationFinished = Resource->IsCompilationFinished() && (MaterialShaderMap != nullptr);

	// check if shader compilation is done and extract shader code
	if (bCompilationFinished)
	{
		TMap<FShaderId, TShaderRef<FShader>> ShaderMap;
		MaterialShaderMap->GetShaderList(ShaderMap);

		const FShaderId& ShaderId = PlatformData[QualityType].ComboBoxSelectedEntry.ShaderId;

		const auto Entry = ShaderMap.Find(ShaderId);
		if (Entry != nullptr)
		{
			const TShaderRef<FShader>& Shader = *Entry;

			static FName NAME_NullVF(TEXT("NullVF"));
			FVertexFactoryType* VertexFactory = Shader.GetVertexFactoryType();
			const FName VertexFactoryName = VertexFactory ? VertexFactory->GetFName() : NAME_NullVF;
			const FName ShaderTypeName = Shader.GetType()->GetFName();
			const FMemoryImageString* ShaderSource = MaterialShaderMap->GetShaderSource(VertexFactoryName, ShaderTypeName);
			if (ShaderSource != nullptr)
			{
				PlatformData[QualityType].bUpdateShaderCode = false;
				PlatformData[QualityType].ShaderCode = FText::FromString(*ShaderSource);
			}
		}
	}

	return PlatformData[QualityType].ShaderCode;
}

void FShaderPlatformSettings::AllocateMaterialResources()
{
	ClearResources();

	const ERHIFeatureLevel::Type TargetFeatureLevel = GetMaxSupportedFeatureLevel(PlatformShaderID);

	for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
	{
		PlatformData[QualityLevelIndex].MaterialResourcesStats = new FMaterialResourceStats();
		PlatformData[QualityLevelIndex].MaterialResourcesStats->SetMaterial(Material, MaterialInstance, (ERHIFeatureLevel::Type)TargetFeatureLevel, (EMaterialQualityLevel::Type)QualityLevelIndex);
	}
}

void FShaderPlatformSettings::SetMaterial(UMaterial* InMaterial)
{
	// if this is a different material, clear away the old one's resources and compile new shaders
	if (Material != InMaterial)
	{
		Material = InMaterial;
		MaterialInstance = nullptr;

		AllocateMaterialResources();
	}
}

void FShaderPlatformSettings::SetMaterial(UMaterialInstance* InMaterialInstance)
{
	if (MaterialInstance != InMaterialInstance)
	{
		MaterialInstance = InMaterialInstance;
		Material = MaterialInstance->GetMaterial();

		AllocateMaterialResources();
	}
}

bool FShaderPlatformSettings::CheckShaders()
{
	bool bRetValue = false;

	// prevent stats 
	const double kMinimumTimeBetweenCompilationsSeconds = 5.0;

	if (Material != nullptr)
	{
		// check and triggers shader recompilation if needed
		for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
		{
			auto &Data = PlatformData[QualityLevelIndex];
			const bool bNeedsShaders = (bPresentInGrid && Data.bExtractStats) || Data.bExtractCode;
			const double CurrentTime = FPlatformTime::Seconds();
			if (Data.bNeedShaderRecompilation && bNeedsShaders && (CurrentTime - Data.LastTimeCompilationRequested) > kMinimumTimeBetweenCompilationsSeconds)
			{
				Data.MaterialResourcesStats->CancelCompilation();

				Material->UpdateCachedExpressionData();

				if (MaterialInstance != nullptr)
				{
					MaterialInstance->UpdateCachedData();
				}

				Data.LastTimeCompilationRequested = CurrentTime;

				TMap<FName, TArray<FMaterialStatsUtils::FRepresentativeShaderInfo>> ShaderTypeNamesAndDescriptions;
				FMaterialStatsUtils::GetRepresentativeShaderTypesAndDescriptions(ShaderTypeNamesAndDescriptions, Data.MaterialResourcesStats);

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
						if (ShaderType && VFType)
						{
							VFTypes.Add(VFType);
							ShaderTypes.Add(ShaderType);
							PipelineTypes.Add(nullptr);
						}
					}
				}

				// Prepare the resource for compilation, but don't compile the completed shader map.
				const bool bSuccess = Data.MaterialResourcesStats->CacheShaders(PlatformShaderID, EMaterialShaderPrecompileMode::None);

				if (bSuccess)
				{
					// Compile just the types we want.
					Data.MaterialResourcesStats->CacheGivenTypes(PlatformShaderID, VFTypes, PipelineTypes, ShaderTypes);
				}

				Data.bCompilingShaders = true;
				Data.bUpdateShaderCode = true;
				Data.bNeedShaderRecompilation = false;

				Data.ShaderStatsInfo.Reset();

				bRetValue = true;
			}
		}
	}

	return bRetValue;
}

bool FShaderPlatformSettings::Update()
{
	bool bRetValue = CheckShaders();

	// if a shader compilation has been requested check if completed and extract shader names needed by code viewer combo-box
	for (int32 i = 0; i < EMaterialQualityLevel::Num; ++i)
	{
		auto& QualityItem = PlatformData[i];
		if (QualityItem.bCompilingShaders)
		{
			FMaterialResource* Resource = QualityItem.MaterialResourcesStats;

			// if compilation is complete extract the list of compiled shader names
			const bool bCompilationFinished = Resource->IsCompilationFinished();
			if (bCompilationFinished)
			{
				QualityItem.bCompilingShaders = false;
				QualityItem.bUpdateShaderCode = true;

				const FMaterialShaderMap* MaterialShaderMap = Resource->GetGameThreadShaderMap();
				if (MaterialShaderMap != nullptr)
				{
					TMap<FShaderId, TShaderRef<FShader>> ShaderMap;
					MaterialShaderMap->GetShaderList(ShaderMap);

					QualityItem.ArrShaderEntries.Empty();
					for (const auto& Entry : ShaderMap)
					{
						FShaderType* EntryShader = Entry.Value.GetType();
						FVertexFactoryType* VertexFactory = Entry.Value.GetVertexFactoryType();
						FString ShaderName = FString::Printf(TEXT("%s/%s"), VertexFactory ? VertexFactory->GetName() : TEXT("NullVF"), EntryShader->GetName());
						QualityItem.ArrShaderEntries.Add(MakeShareable(new FMaterialShaderEntry{ Entry.Key, ShaderName }));
					}

					if (QualityItem.ArrShaderEntries.Num() > 0)
					{
						QualityItem.ArrShaderEntries.Sort([](const TSharedPtr<FMaterialShaderEntry>& First, const TSharedPtr<FMaterialShaderEntry>& Second)
							{
								return First->Text < Second->Text;
							});

						QualityItem.ComboBoxSelectedEntry = *QualityItem.ArrShaderEntries[0];
					}
				}

				FMaterialStatsUtils::ExtractMatertialStatsInfo(PlatformShaderID, QualityItem.ShaderStatsInfo, Resource);

				bRetValue = true;
			}
		}
	}

	return bRetValue;
}

/*end FShaderPlatformSettings functions*/
/***********************************************************************************************************************/

/***********************************************************************************************************************/
/*begin FMaterialStats functions*/

FMaterialStats::~FMaterialStats()
{
	SaveSettings();
}

void FMaterialStats::Initialize(IMaterialEditor* InMaterialEditor)
{
	MaterialEditor = InMaterialEditor;

	StatsGrid = MakeShareable(new FMaterialStatsGrid(AsShared()));

	BuildShaderPlatformDB();

	LoadSettings();

	StatsGrid->BuildGrid();

	GridStatsWidget = SNew(SMaterialEditorStatsWidget)
		.MaterialStatsWPtr(SharedThis(this));

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

void FMaterialStats::LoadSettings()
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
		bArrStatsQualitySelector[(EMaterialQualityLevel::Type)i] = bUsed;

		for (const auto& PlatformEntry : ShaderPlatformStatsDB)
		{
			TSharedPtr<FShaderPlatformSettings> SomePlatform = PlatformEntry.Value;
			if (SomePlatform.IsValid())
			{
				SomePlatform->SetExtractStatsQualityLevel((EMaterialQualityLevel::Type)i, bUsed);
			}
		}
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

void FMaterialStats::BuildShaderPlatformDB()
{
	bool bCanCompileMacDesktopSPs = (PLATFORM_MAC != 0);
	bool bCanCompileMacMobileSPs = (PLATFORM_MAC != 0) || (PLATFORM_WINDOWS != 0);

#if PLATFORM_WINDOWS
	// DirectX
	AddShaderPlatform(EPlatformCategoryType::Desktop, SP_PCD3D_SM5, TEXT("DirectX SM5"), true, true, TEXT("Desktop, DirectX, Shader Model 5"));
	AddShaderPlatform(EPlatformCategoryType::Desktop, SP_PCD3D_SM6, TEXT("DirectX SM6"), true, true, TEXT("Desktop, DirectX, Shader Model 6"));
	AddShaderPlatform(EPlatformCategoryType::Desktop, SP_PCD3D_ES3_1, TEXT("DirectX ES 3.1"), true, true, TEXT("Desktop, DirectX, ES 3.1"));

	// Desktop is the closest fit for Hololens (for now)
	AddShaderPlatform(EPlatformCategoryType::Desktop, SP_D3D_ES3_1_HOLOLENS, TEXT("DirectX ES 3.1 Hololens"), true, true, TEXT("Hololens, DirectX, ES 3.1"));
#endif

	// Vulkan
	AddShaderPlatform(EPlatformCategoryType::Desktop, SP_VULKAN_SM5, TEXT("Vulkan SM5"), true, true, TEXT("Desktop, Vulkan, Shader Model 5"));

	// Android
	AddShaderPlatform(EPlatformCategoryType::Android, SP_OPENGL_ES3_1_ANDROID, TEXT("Android GLES 3.1"), true, true, TEXT("Android, OpenGLES 3.1"));
	AddShaderPlatform(EPlatformCategoryType::Android, SP_VULKAN_ES3_1_ANDROID, TEXT("Android Vulkan"), true, true, TEXT("Android, Vulkan"));
	AddShaderPlatform(EPlatformCategoryType::Android, SP_VULKAN_SM5_ANDROID, TEXT("Android Vulkan SM5"), true, true, TEXT("Android, Vulkan SM5"));

	// Apple
	AddShaderPlatform(EPlatformCategoryType::Desktop, SP_METAL_SM5, TEXT("Metal SM5"), bCanCompileMacDesktopSPs, true, TEXT("macOS, Metal, Shader Model 5"));
	AddShaderPlatform(EPlatformCategoryType::Desktop, SP_METAL_MRT_MAC, TEXT("Metal SM5 (MRT)"), bCanCompileMacDesktopSPs, true, TEXT("macOS, Metal, Shader Model 5"));
	AddShaderPlatform(EPlatformCategoryType::IOS, SP_METAL, TEXT("Metal"), bCanCompileMacMobileSPs, true, TEXT("iOS, Metal, Mobile"));
	AddShaderPlatform(EPlatformCategoryType::IOS, SP_METAL_MRT, TEXT("Metal MRT"), bCanCompileMacMobileSPs, true, TEXT("iOS, Metal, Shader Model 5"));

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
				AddShaderPlatform(bIsConsole ? EPlatformCategoryType::Console : EPlatformCategoryType::Desktop, ShaderPlatform, PlatformName, true, true, PlatformName.ToString());
			}
		}
	}

}

TSharedPtr<FShaderPlatformSettings> FMaterialStats::AddShaderPlatform(const EPlatformCategoryType PlatformType, const EShaderPlatform PlatformID, const FName PlatformName,
	const bool bAllowPresenceInGrid, const bool bAllowCodeView, const FString& Description)
{
	if (!FDataDrivenShaderPlatformInfo::IsValid(PlatformID))
	{
		return TSharedPtr<FShaderPlatformSettings>();
	}

	TSharedPtr<FShaderPlatformSettings> PlatformPtr = MakeShareable(new FShaderPlatformSettings(PlatformType, PlatformID, PlatformName, bAllowPresenceInGrid, bAllowCodeView, Description));
	ShaderPlatformStatsDB.Add(PlatformID, PlatformPtr);

	auto& ArrayPlatforms = PlatformTypeDB.FindOrAdd(PlatformType);
	ArrayPlatforms.Add(PlatformPtr);

	for (int32 i = 0; i < EMaterialQualityLevel::Num; ++i)
	{
		PlatformPtr->SetExtractStatsFlag((EMaterialQualityLevel::Type)i, bArrStatsQualitySelector[i]);
	}

	return PlatformPtr;
}

void FMaterialStats::SignalMaterialChanged()
{
	ExtractHLSLCode();

	for (auto Entry : ShaderPlatformStatsDB)
	{
		for (int32 i = 0; i < EMaterialQualityLevel::Num; ++i)
			Entry.Value->SetNeedShaderCompilation((EMaterialQualityLevel::Type)i, true);
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

	bArrStatsQualitySelector[QualityLevel] = bValue;

	for (const auto& PlatformEntry : ShaderPlatformStatsDB)
	{
		TSharedPtr<FShaderPlatformSettings> SomePlatform = PlatformEntry.Value;
		if (SomePlatform.IsValid())
		{
			SomePlatform->SetExtractStatsQualityLevel(QualityLevel, bValue);
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

FText FMaterialStats::GetShaderCode(const EShaderPlatform PlatformID, const EMaterialQualityLevel::Type QualityType)
{
	auto* Entry = ShaderPlatformStatsDB.Find(PlatformID);
	if (Entry == nullptr)
	{
		return FText::FromString(TEXT("Shader code compiling or not available!"));
	}

	return (*Entry)->GetShaderCode(QualityType);
}

void FMaterialStats::Update()
{
	const bool bNeedsUpdate = IsShowingStats() || IsCodeViewWindowActive();
	if (!bNeedsUpdate)
	{
		return;
	}

	bool bInfoChanged = false;

	for (const auto& Entry : ShaderPlatformStatsDB)
	{
		auto PlatformStats = Entry.Value;
		bInfoChanged |= PlatformStats->Update();
	}

	if (bInfoChanged)
	{
		GetStatsGrid()->OnShaderChanged();
	}

	ComputeGridWarnings();
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

TSharedRef<SDockTab> FMaterialStats::SpawnTab_ShaderCode(const FSpawnTabArgs& Args, const EShaderPlatform PlatformID, const EMaterialQualityLevel::Type QualityLevel)
{
	SetShaderPlatformUseCodeView(PlatformID, QualityLevel, true);

	const FString PlatformName = GetPlatformName(PlatformID).ToString();
	const FString FullPlatformName = PlatformName + TEXT(" -- ") + FMaterialStatsUtils::MaterialQualityToString(QualityLevel);

	TSharedPtr<FShaderPlatformSettings> PlatformPtr = GetPlatformSettings(PlatformID);
	check(PlatformPtr.IsValid());

	TSharedRef<SComboBox<TSharedPtr<FMaterialShaderEntry>>> ShaderBox = SNew(SComboBox<TSharedPtr<FMaterialShaderEntry>>)
		.OptionsSource(PlatformPtr->GetShaderEntries(QualityLevel))
		.OnGenerateWidget_Lambda([](TSharedPtr<FMaterialShaderEntry> Value) { return SNew(STextBlock).Text(FText::FromString(Value->Text)); })
		.OnSelectionChanged_Lambda([PlatformPtr, QualityLevel](TSharedPtr<FMaterialShaderEntry> Item, ESelectInfo::Type SelectInfo) { PlatformPtr->OnShaderViewComboSelectionChanged(Item, QualityLevel); })
		[
			SNew(STextBlock)
			.Text_Lambda([PlatformPtr, QualityLevel]() { return PlatformPtr->GetSelectedShaderViewComboText(QualityLevel); })
		];

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
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
							[MaterialStats = TWeakPtr<FMaterialStats>(SharedThis(this)), PlatformID, QualityLevel]()
							{
								auto StatsPtr = MaterialStats.Pin();
								if (StatsPtr.IsValid())
								{
									FText ShaderCode = StatsPtr->GetShaderCode(PlatformID, QualityLevel);
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

FName FMaterialStats::MakeTabName(const EPlatformCategoryType PlatformType, const EShaderPlatform ShaderPlatformType, const EMaterialQualityLevel::Type QualityLevel)
{
	const FString TabName = FMaterialStatsUtils::GetPlatformTypeName(PlatformType) + FMaterialStatsUtils::ShaderPlatformTypeName(ShaderPlatformType) + FMaterialStatsUtils::MaterialQualityToString(QualityLevel);

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

	for (auto MapEntry : PlatformTypeDB)
	{
		const EPlatformCategoryType PlatformType = MapEntry.Key;

		const FString PlatformName = FMaterialStatsUtils::GetPlatformTypeName(PlatformType);
		TSharedPtr<FWorkspaceItem> PlatformMenuItem = PlatformGroupMenuItem->AddGroup(FText::FromString(PlatformName), FSlateIcon(FAppStyle::GetAppStyleSetName(), "MaterialEditor.Tabs.HLSLCode"));

		TArray<TSharedPtr<FShaderPlatformSettings>>& ArrShaderPlatforms = MapEntry.Value;

		for (int32 i = 0; i < ArrShaderPlatforms.Num(); ++i)
		{
			TSharedPtr<FShaderPlatformSettings> PlatformPtr = ArrShaderPlatforms[i];

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
				const FName TabName = MakeTabName(PlatformType, PlatformID, QualityLevel);

				TabManager->RegisterTabSpawner(TabName, FOnSpawnTab::CreateSP<FMaterialStats, EShaderPlatform, EMaterialQualityLevel::Type>(this, &FMaterialStats::SpawnTab_ShaderCode, PlatformID, QualityLevel))
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
								return StatsPtr->GetShaderCode(PlatformID, QualityLevel);
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

	FString TabName = FString::Printf(TEXT(""), *GetMaterialName().ToString());

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
				const FName TabName = MakeTabName(PlatformType, ShaderPlatformID, (EMaterialQualityLevel::Type)q);

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

	int32 Warnings = 0;
	TArray<EShaderPlatform> CompilerWarnings;
	TArray<FString> WarningMessages;

	bool bAnyQualityPresent = false;
	for (int32 q = 0; q < EMaterialQualityLevel::Num; ++q)
	{
		auto Quality = static_cast<EMaterialQualityLevel::Type>(q);

		bAnyQualityPresent |= GetStatsQualityFlag(Quality);
	}

	if (!bAnyQualityPresent)
	{
		Warnings |= WarningNoQuality;
		WarningMessages.Emplace("No material quality selected. Please use the 'Settings' button and choose the desired quality level to be analyzed.");
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
					CompilerWarnings.Add(ShaderPlatformType);
					auto WarningString = FString::Printf(TEXT("Platform %s needs an offline shader compiler to extract instruction count. Please check 'Editor Preferences' -> 'Content Editors' -> 'Material Editor' for additional settings."), *PlatformPtr->GetPlatformName().ToString());
					WarningMessages.Add(MoveTemp(WarningString));
				}
			}
		}
	}

	if (!bAnyPlatformPresent)
	{
		Warnings |= WarningNoPlatform;
		WarningMessages.Emplace("No platform selected. Please use the 'Settings' button and choose desired platform to be analyzed.");
	}

	bool bRefreshWarnings = (Warnings != LastGenericWarning) || (CompilerWarnings.Num() != LastMissingCompilerWarnings.Num());
	if (!bRefreshWarnings)
	{
		for (int32 i = 0; i < CompilerWarnings.Num(); ++i)
		{
			if (CompilerWarnings[i] != LastMissingCompilerWarnings[i])
			{
				bRefreshWarnings = true;
				break;
			}
		}
	}

	if (bRefreshWarnings)
	{
		LastGenericWarning = Warnings;
		LastMissingCompilerWarnings = CompilerWarnings;

		GridPtr->ClearWarningMessages();

		for (int32 i = 0; i < WarningMessages.Num(); ++i)
		{
			GridPtr->AddWarningMessage(WarningMessages[i]);
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
