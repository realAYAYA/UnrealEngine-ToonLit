// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeIREE.h"

#ifdef WITH_NNE_RUNTIME_IREE

#if WITH_EDITOR
#include "Containers/StringConv.h"
#include "DerivedDataCache.h"
#include "DerivedDataRequestOwner.h"
#include "HAL/PlatformFileManager.h"
#include "IO/IoHash.h"
#include "Memory/SharedBuffer.h"
#include "Misc/FileHelper.h"
#endif // WITH_EDITOR

#include "EngineAnalytics.h"
#include "HAL/Platform.h"
#include "Interfaces/ITargetPlatform.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "NNE.h"
#include "NNERuntimeIREECompiler.h"
#include "NNERuntimeIREEModel.h"
#include "NNERuntimeIREEMetaData.h"
#include "NNEModelData.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace UE::NNERuntimeIREE::CPU::Private
{
#if WITH_EDITOR
	inline UE::DerivedData::FCacheKey CreateCacheKey(const FString& RequestId)
	{
		return { UE::DerivedData::FCacheBucket(TEXT("NNEModelData")), FIoHash::HashBuffer(MakeMemoryView(StringCast<UTF8CHAR>(*RequestId))) };
	}

	inline void PutIntoDDC(const FString& RequestId, const FSharedBuffer& Data)
	{
		check(!Data.IsNull());
		check(Data.GetSize() > 0);

		TArray<UE::DerivedData::FCachePutValueRequest> Requests;
		Requests.SetNum(1);

		Requests[0].Name = FString("Put-") + RequestId;
		Requests[0].Key = CreateCacheKey(RequestId);
		Requests[0].Value = UE::DerivedData::FValue::Compress(Data);

		UE::DerivedData::FRequestOwner BlockingPutOwner(UE::DerivedData::EPriority::Blocking);
		UE::DerivedData::GetCache().PutValue(Requests, BlockingPutOwner);
		BlockingPutOwner.Wait();
	}

	inline FSharedBuffer GetFromDDC(const FString& RequestId)
	{
		TArray<UE::DerivedData::FCacheGetValueRequest> Requests;
		Requests.SetNum(1);

		Requests[0].Name = FString("Get-") + RequestId;
		Requests[0].Key = CreateCacheKey(RequestId);

		FSharedBuffer Result;
		UE::DerivedData::FRequestOwner BlockingGetOwner(UE::DerivedData::EPriority::Blocking);
		UE::DerivedData::GetCache().GetValue(Requests, BlockingGetOwner, [&Result](UE::DerivedData::FCacheGetValueResponse&& Response)
			{
				if (Response.Value.HasData() && Response.Value.GetRawSize() > sizeof(uint32))
				{
					Result = Response.Value.GetData().Decompress();
				}
			});
		BlockingGetOwner.Wait();
		return Result;
	}
#endif // WITH_EDITOR

	FString GetModelDataIdentifier(const FString& RuntimeName, const FGuid& Guid, const FString& FileIdString, const FString& PlatformName, const FString& Architecture)
	{
		return RuntimeName + "-" + Guid.ToString(EGuidFormats::Digits) + "-" + FString::FromInt(UNNERuntimeIREECpu::Version) + "-" + FileIdString + "-" + PlatformName + (!Architecture.IsEmpty() ? ("-" + Architecture) : "");
	}

	FString GetIntermediateModelDirPath(const FString& PlatformName, const FString& ModelName)
	{
		return FPaths::Combine("Intermediate", "Build", PlatformName, UE_PLUGIN_NAME, ModelName);
	}

	FString GetStagedModelDirPath(const FString& PlatformName)
	{
		return FPaths::Combine("Saved", "Cooked", PlatformName, "Engine", "Plugins", UE_PLUGIN_NAME, "Binaries");
	}

	FString GetPackagedModelDirPath(const FString& PlatformName)
	{
		FString PlatformNameShort = PlatformName.Equals("Windows") ? "Win64" : PlatformName;
		return FPaths::Combine("Binaries", PlatformNameShort, UE_PLUGIN_NAME);
	}
} // UE::NNERuntimeIREE::CPU::Private

FGuid UNNERuntimeIREECpu::GUID = FGuid((int32)'I', (int32)'C', (int32)'P', (int32)'U');
int32 UNNERuntimeIREECpu::Version = 0x00000003;

FString UNNERuntimeIREECpu::GetRuntimeName() const
{
	return TEXT("NNERuntimeIREECpu");
}

UNNERuntimeIREECpu::ECanCreateModelDataStatus UNNERuntimeIREECpu::CanCreateModelData(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const
{
#if WITH_EDITOR
	return FileType.Compare(TEXT("mlir"), ESearchCase::IgnoreCase) == 0 ? ECanCreateModelDataStatus::Ok : ECanCreateModelDataStatus::FailFileIdNotSupported;
#else
	return ECanCreateModelDataStatus::Fail;
#endif // WITH_EDITOR
}

TSharedPtr<UE::NNE::FSharedModelData> UNNERuntimeIREECpu::CreateModelData(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform)
{
#if WITH_EDITOR
	using namespace UE::NNERuntimeIREE::CPU::Private;

	FString TargetPlatformName = TargetPlatform ? TargetPlatform->IniPlatformName() : UGameplayStatics::GetPlatformName();
	if (CanCreateModelData(FileType, FileData, AdditionalFileData, FileId, TargetPlatform) != ECanCreateModelDataStatus::Ok)
	{
		UE_LOG(LogNNE, Warning, TEXT("UNNERuntimeIREECpu cannot create the model data with id %s (Filetype: %s) for platform %s"), *FileId.ToString(EGuidFormats::Digits).ToLower(), *FileType, *TargetPlatformName);
		return TSharedPtr<UE::NNE::FSharedModelData>();
	}

	FConfigFile ConfigFile;
	FString ConfigFilePath;
	GetUpdatedPlatformConfig(TargetPlatformName, ConfigFile, ConfigFilePath);
	if (ConfigFile.Dirty)
	{
		UE_LOG(LogNNE, Warning, TEXT("UNNERuntimeIREECpu could not find the required settings in config file %s. Please make the file writeable and re-start the editor or manually add the required staging settings or models will not work in packaged builds for platform %s!"), *ConfigFilePath, *TargetPlatformName);
	}

	TUniquePtr<UE::NNERuntimeIREE::CPU::FCompiler> Compiler = UE::NNERuntimeIREE::CPU::FCompiler::Make(TargetPlatformName);
	if (!Compiler.IsValid())
	{
		UE_LOG(LogNNE, Warning, TEXT("UNNERuntimeIREECpu failed to create a compiler to compile for platform %s"), *TargetPlatformName);
		return TSharedPtr<UE::NNE::FSharedModelData>();
	}
	
	FString FileIdString = FileId.ToString(EGuidFormats::Digits).ToLower();
	FString IntermediateDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), GetIntermediateModelDirPath(TargetPlatformName, FileIdString)));
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.DeleteDirectoryRecursively(*IntermediateDir);
	PlatformFile.CreateDirectoryTree(*IntermediateDir);

	TArray<UE::NNERuntimeIREE::CPU::FCompilerResult> CompilerResults;
	UNNERuntimeIREEModuleMetaData* CompilerModuleMetaData = NewObject<UNNERuntimeIREEModuleMetaData>();
	FString StagingDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), GetStagedModelDirPath(TargetPlatformName)));
	if (!Compiler->CompileMlir(FileData, FileIdString, IntermediateDir, StagingDir, CompilerResults, CompilerModuleMetaData))
	{
		UE_LOG(LogNNE, Warning, TEXT("UNNERuntimeIREECpu failed to compile model %s"), *FileIdString);
		return TSharedPtr<UE::NNE::FSharedModelData>();
	}

	for (int32 i = 0; i < CompilerResults.Num(); i++)
	{
		TArray<uint8> SharedLibData;
		FString StagedSharedLibPath = FPaths::Combine(StagingDir, CompilerResults[i].RelativeDirPath, CompilerResults[i].SharedLibraryFileName);
		if (!FFileHelper::LoadFileToArray(SharedLibData, *StagedSharedLibPath) || SharedLibData.IsEmpty())
		{
			UE_LOG(LogNNE, Warning, TEXT("UNNERuntimeIREECpu could not read the shared library \"%s\""), *StagedSharedLibPath);
			return TSharedPtr<UE::NNE::FSharedModelData>();
		}
		FSharedBuffer SharedLibBuffer = MakeSharedBufferFromArray(MoveTemp(SharedLibData));
		PutIntoDDC(UE::NNERuntimeIREE::CPU::Private::GetModelDataIdentifier(GetRuntimeName(), UNNERuntimeIREECpu::GUID, FileIdString, TargetPlatformName, CompilerResults[i].Architecture) + "-lib", SharedLibBuffer);

		TArray<uint8> VmfbData;
		FString StagedVmfbPath = FPaths::Combine(StagingDir, CompilerResults[i].RelativeDirPath, CompilerResults[i].VmfbFileName);
		if (!FFileHelper::LoadFileToArray(VmfbData, *StagedVmfbPath) || VmfbData.IsEmpty())
		{
			UE_LOG(LogNNE, Warning, TEXT("UNNERuntimeIREECpu could not read the vmfb data \"%s\""), *StagedVmfbPath);
			return TSharedPtr<UE::NNE::FSharedModelData>();
		}
		FSharedBuffer VmfbBuffer = MakeSharedBufferFromArray(MoveTemp(VmfbData));
		PutIntoDDC(UE::NNERuntimeIREE::CPU::Private::GetModelDataIdentifier(GetRuntimeName(), UNNERuntimeIREECpu::GUID, FileIdString, TargetPlatformName, CompilerResults[i].Architecture) + "-vmfb", VmfbBuffer);
	}

	TArray<uint8> ResultData;
	FMemoryWriter Writer(ResultData);
	Writer << UNNERuntimeIREECpu::GUID;
	Writer << UNNERuntimeIREECpu::Version;
	FGuid FileIdCopy = FileId;
	Writer << FileIdCopy;

	TArray<uint8> ModuleMetaData;
	if (AdditionalFileData.Contains("IREEModuleMetaData"))
	{
		ModuleMetaData = AdditionalFileData["IREEModuleMetaData"];
	}
	if (ModuleMetaData.IsEmpty())
	{
		FMemoryWriter ObjectWriter(ModuleMetaData);
		CompilerModuleMetaData->Serialize(ObjectWriter);
	}
	Writer << ModuleMetaData;

	int32 NumArchitectures = CompilerResults.Num();
	Writer << NumArchitectures;
	for (int32 i = 0; i < NumArchitectures; i++)
	{
		Writer << CompilerResults[i].Architecture;
		Writer << CompilerResults[i].RelativeDirPath;
		Writer << CompilerResults[i].SharedLibraryFileName;
		Writer << CompilerResults[i].VmfbFileName;
		Writer << CompilerResults[i].SharedLibraryEntryPointName;
	}

	return MakeShared<UE::NNE::FSharedModelData>(MakeSharedBufferFromArray(MoveTemp(ResultData)), 0);
#else
	return TSharedPtr<UE::NNE::FSharedModelData>();
#endif // WITH_EDITOR
}

FString UNNERuntimeIREECpu::GetModelDataIdentifier(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const
{
	// Leave architecture blank as there is only one model data for all architectures of a given platform, only the vmfb and shared lib are different
	FString PlatformName = TargetPlatform ? TargetPlatform->IniPlatformName() : UGameplayStatics::GetPlatformName();
	return UE::NNERuntimeIREE::CPU::Private::GetModelDataIdentifier(GetRuntimeName(), UNNERuntimeIREECpu::GUID, FileId.ToString(EGuidFormats::Digits), PlatformName, "");
}

UNNERuntimeIREECpu::ECanCreateModelCPUStatus UNNERuntimeIREECpu::CanCreateModelCPU(const TObjectPtr<UNNEModelData> ModelData) const
{
	check(ModelData != nullptr);

	TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName());
	if (!SharedData.IsValid())
	{
		return ECanCreateModelCPUStatus::Fail;
	}

	TConstArrayView<uint8> SharedDataView = SharedData->GetView();
	int32 GuidSize = sizeof(UNNERuntimeIREECpu::GUID);
	int32 VersionSize = sizeof(UNNERuntimeIREECpu::Version);
	if (SharedDataView.Num() <= GuidSize + VersionSize)
	{
		return ECanCreateModelCPUStatus::Fail;
	}

	bool bResult = FGenericPlatformMemory::Memcmp(&(SharedDataView[0]), &(UNNERuntimeIREECpu::GUID), GuidSize) == 0;
	bResult &= FGenericPlatformMemory::Memcmp(&(SharedDataView[GuidSize]), &(UNNERuntimeIREECpu::Version), VersionSize) == 0;

	return bResult ? ECanCreateModelCPUStatus::Ok : ECanCreateModelCPUStatus::Fail;
}

TSharedPtr<UE::NNE::IModelCPU> UNNERuntimeIREECpu::CreateModelCPU(const TObjectPtr<UNNEModelData> ModelData)
{
	check(ModelData != nullptr);

	using namespace UE::NNERuntimeIREE::CPU::Private;

	if (CanCreateModelCPU(ModelData) != ECanCreateModelCPUStatus::Ok)
	{
		UE_LOG(LogNNE, Warning, TEXT("UNNERuntimeIREECpu cannot create a model from the model data with id %s"), *ModelData->GetFileId().ToString(EGuidFormats::Digits));
		return TSharedPtr<UE::NNE::IModelCPU>();
	}

	FString CurrentArchitecture = "";
#if PLATFORM_CPU_X86_FAMILY
	CurrentArchitecture = "x86_64";
#elif PLATFORM_CPU_ARM_FAMILY
	CurrentArchitecture = "arm64";
#endif

	TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName());
	check(SharedData.IsValid());
	TConstArrayView<uint8> SharedDataView = SharedData->GetView();
	FMemoryReaderView Reader(SharedDataView);
	FGuid DataGuid = FGuid();
	Reader << DataGuid;
	int32 VersionVersion = 0;
	Reader << VersionVersion;
	FGuid FileId = FGuid();
	Reader << FileId;

	TArray<uint8> ModuleDataArray;
	Reader << ModuleDataArray;
	if (ModuleDataArray.IsEmpty())
	{
		UE_LOG(LogNNE, Warning, TEXT("UNNERuntimeIREECpu failed to find any module meta data, please reimport the original model"));
		return TSharedPtr<UE::NNE::IModelCPU>();
	}
	UNNERuntimeIREEModuleMetaData* ModuleMetaData = NewObject<UNNERuntimeIREEModuleMetaData>();
	FMemoryReaderView ObjectReader(ModuleDataArray);
	ModuleMetaData->Serialize(ObjectReader);
	if (ModuleMetaData->FunctionMetaData.IsEmpty())
	{
		UE_LOG(LogNNE, Warning, TEXT("UNNERuntimeIREECpu failed to parse the module meta data, please reimport the original model"));
		return TSharedPtr<UE::NNE::IModelCPU>();
	}

	int32 NumArchitectures = 0;
	Reader << NumArchitectures;

	bool bFound = false;
	FString Architecture = "";
	FString RelativeDirPath = "";
	FString SharedLibraryFileName = "";
	FString VmfbFileName = "";
	FString SharedLibraryEntryPointName = "";
	for (int32 i = 0; i < NumArchitectures; i++)
	{
		FString TmpArchitecture = "";
		Reader << TmpArchitecture;
		FString TmpRelativeDirPath = "";
		Reader << TmpRelativeDirPath;
		FString TmpSharedLibraryFileName = "";
		Reader << TmpSharedLibraryFileName;
		FString TmpVmfbFileName = "";
		Reader << TmpVmfbFileName;
		FString TmpSharedLibraryEntryPointName = "";
		Reader << TmpSharedLibraryEntryPointName;

		if (TmpArchitecture.IsEmpty() && !bFound)
		{
			Architecture = TmpArchitecture;
			RelativeDirPath = TmpRelativeDirPath;
			SharedLibraryFileName = TmpSharedLibraryFileName;
			VmfbFileName = TmpVmfbFileName;
			SharedLibraryEntryPointName = TmpSharedLibraryEntryPointName;
			bFound = true;
		}
		else if (TmpArchitecture.Equals(CurrentArchitecture))
		{
			Architecture = TmpArchitecture;
			RelativeDirPath = TmpRelativeDirPath;
			SharedLibraryFileName = TmpSharedLibraryFileName;
			VmfbFileName = TmpVmfbFileName;
			SharedLibraryEntryPointName = TmpSharedLibraryEntryPointName;
			bFound = true;
		}
	}
	if (!bFound)
	{
		UE_LOG(LogNNE, Warning, TEXT("UNNERuntimeIREECpu failed to find a matching architecture for \'%s\'"), *CurrentArchitecture);
		return TSharedPtr<UE::NNE::IModelCPU>();
	}

	FString FileIdString = FileId.ToString(EGuidFormats::Digits).ToLower();
#if WITH_EDITOR
	FString SharedLibraryDirPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), GetIntermediateModelDirPath(UGameplayStatics::GetPlatformName(), FileIdString), RelativeDirPath));
#else
	FString SharedLibraryDirPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), GetPackagedModelDirPath(UGameplayStatics::GetPlatformName()), RelativeDirPath));
#endif // WITH_EDITOR

#if WITH_EDITOR
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	FString SharedLibraryFilePath = FPaths::Combine(SharedLibraryDirPath, SharedLibraryFileName);
	if (!PlatformFile.FileExists(*SharedLibraryFilePath))
	{
		FSharedBuffer SharedBuffer = GetFromDDC(UE::NNERuntimeIREE::CPU::Private::GetModelDataIdentifier(GetRuntimeName(), UNNERuntimeIREECpu::GUID, FileIdString, UGameplayStatics::GetPlatformName(), Architecture) + "-lib");
		if (SharedBuffer.GetSize() <= 0)
		{
			UE_LOG(LogNNE, Warning, TEXT("UNNERuntimeIREECpu could not fetch the shared library %s from DDC"), *SharedLibraryFileName);
			return TSharedPtr<UE::NNE::IModelCPU>();
		}
		PlatformFile.CreateDirectoryTree(*SharedLibraryDirPath);
		FFileHelper::SaveArrayToFile(TConstArrayView<uint8>((uint8*)SharedBuffer.GetData(), SharedBuffer.GetSize()), *SharedLibraryFilePath);
	}

	FString VmfbFilePath = FPaths::Combine(SharedLibraryDirPath, VmfbFileName);
	if (!PlatformFile.FileExists(*VmfbFilePath))
	{
		FSharedBuffer SharedBuffer = GetFromDDC(UE::NNERuntimeIREE::CPU::Private::GetModelDataIdentifier(GetRuntimeName(), UNNERuntimeIREECpu::GUID, FileIdString, UGameplayStatics::GetPlatformName(), Architecture) + "-vmfb");
		if (SharedBuffer.GetSize() <= 0)
		{
			UE_LOG(LogNNE, Warning, TEXT("UNNERuntimeIREECpu could not fetch the vmfb %s from DDC"), *VmfbFileName);
			return TSharedPtr<UE::NNE::IModelCPU>();
		}
		PlatformFile.CreateDirectoryTree(*SharedLibraryDirPath);
		FFileHelper::SaveArrayToFile(TConstArrayView<uint8>((uint8*)SharedBuffer.GetData(), SharedBuffer.GetSize()), *VmfbFilePath);
	}
#endif // WITH_EDITOR

	TSharedPtr<UE::NNE::IModelCPU> Model = UE::NNERuntimeIREE::CPU::FModel::Make(SharedLibraryDirPath, SharedLibraryFileName, VmfbFileName, SharedLibraryEntryPointName, *ModuleMetaData);
	if (!Model.IsValid())
	{
		UE_LOG(LogNNE, Warning, TEXT("UNNERuntimeIREECpu could not initialize the model created from model data with id %s"), *FileIdString);
		return TSharedPtr<UE::NNE::IModelCPU>();
	}

	if (FEngineAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> Attributes = MakeAnalyticsEventAttributeArray(
			TEXT("PlatformName"), UGameplayStatics::GetPlatformName(),
			TEXT("HashedRuntimeName"), FMD5::HashAnsiString(*GetRuntimeName()),
			TEXT("ModelDataSize"), SharedDataView.Num()
		);
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("NeuralNetworkEngine.CreateModel"), Attributes);
	}

	return Model;
}

void UNNERuntimeIREECpu::GetUpdatedPlatformConfig(const FString& PlatformName, FConfigFile& ConfigFile, FString& ConfigFilePath)
{ 
	FString ConfigFolderPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir());
	ConfigFilePath = FPaths::Combine(ConfigFolderPath, PlatformName, PlatformName + "Game.ini");

	ConfigFile.Read(ConfigFilePath);

	FString StagingPath = FString("/") + UE::NNERuntimeIREE::CPU::Private::GetStagedModelDirPath(PlatformName);
	FString PackagingPath = FString("/") + UE::NNERuntimeIREE::CPU::Private::GetPackagedModelDirPath(PlatformName);

	ConfigFile.AddUniqueToSection(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("+DirectoriesToAlwaysStageAsNonUFS"), FString("(Path=\"..") + StagingPath + FString("\")"));
	ConfigFile.AddUniqueToSection(TEXT("Staging"), TEXT("+RemapDirectories"), FString("(From=\"") + FApp::GetProjectName() + StagingPath + FString("\", To=\"") + FApp::GetProjectName() + PackagingPath + FString("\")"));
	ConfigFile.AddUniqueToSection(TEXT("Staging"), TEXT("+AllowedDirectories"), FApp::GetProjectName() + PackagingPath);
}

FString UNNERuntimeIREEGpu::GetRuntimeName() const
{
	return TEXT("");
}

UNNERuntimeIREEGpu::ECanCreateModelDataStatus UNNERuntimeIREEGpu::CanCreateModelData(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const
{
#if WITH_EDITOR
	return FileType.Compare(TEXT("mlir"), ESearchCase::IgnoreCase) == 0 ? ECanCreateModelDataStatus::Ok : ECanCreateModelDataStatus::FailFileIdNotSupported;
#else
	return ECanCreateModelDataStatus::Fail;
#endif // WITH_EDITOR
}

TSharedPtr<UE::NNE::FSharedModelData> UNNERuntimeIREEGpu::CreateModelData(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform)
{
	return TSharedPtr<UE::NNE::FSharedModelData>();
}

FString UNNERuntimeIREEGpu::GetModelDataIdentifier(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const
{
	FString PlatformName = TargetPlatform ? TargetPlatform->IniPlatformName() : UGameplayStatics::GetPlatformName();
	return UE::NNERuntimeIREE::CPU::Private::GetModelDataIdentifier(GetRuntimeName(), GetGUID(), FileId.ToString(EGuidFormats::Digits), PlatformName, "");
}

UNNERuntimeIREEGpu::ECanCreateModelGPUStatus UNNERuntimeIREEGpu::CanCreateModelGPU(const TObjectPtr<UNNEModelData> ModelData) const
{
	check(ModelData != nullptr);

	TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName());
	if (!SharedData.IsValid())
	{
		return ECanCreateModelGPUStatus::Fail;
	}

	TConstArrayView<uint8> SharedDataView = SharedData->GetView();
	FGuid Guid = GetGUID();
	int32 Version = GetVersion();
	int32 GuidSize = sizeof(Guid);
	int32 VersionSize = sizeof(Version);
	if (SharedDataView.Num() <= GuidSize + VersionSize)
	{
		return ECanCreateModelGPUStatus::Fail;
	}

	bool bResult = FGenericPlatformMemory::Memcmp(&(SharedDataView[0]), &(Guid), GuidSize) == 0;
	bResult &= FGenericPlatformMemory::Memcmp(&(SharedDataView[GuidSize]), &(Version), VersionSize) == 0;

	return bResult ? ECanCreateModelGPUStatus::Ok : ECanCreateModelGPUStatus::Fail;
}

TSharedPtr<UE::NNE::IModelGPU> UNNERuntimeIREEGpu::CreateModelGPU(const TObjectPtr<UNNEModelData> ModelData)
{
	check(ModelData != nullptr);

	if (CanCreateModelGPU(ModelData) != ECanCreateModelGPUStatus::Ok)
	{
		UE_LOG(LogNNE, Warning, TEXT("UNNERuntimeIREEGpu cannot create a model from the model data with id %s"), *ModelData->GetFileId().ToString(EGuidFormats::Digits));
		return TSharedPtr<UE::NNE::IModelGPU>();
	}

	check(ModelData->GetModelData(GetRuntimeName()).IsValid());

	UE::NNE::IModelGPU* IModel = nullptr;
	TConstArrayView<uint8> SharedDataView = ModelData->GetModelData(GetRuntimeName())->GetView();

	if (FEngineAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> Attributes = MakeAnalyticsEventAttributeArray(
			TEXT("PlatformName"), UGameplayStatics::GetPlatformName(),
			TEXT("HashedRuntimeName"), FMD5::HashAnsiString(*GetRuntimeName()),
			TEXT("ModelDataSize"), SharedDataView.Num()
		);
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("NeuralNetworkEngine.CreateModel"), Attributes);
	}

	return TSharedPtr<UE::NNE::IModelGPU>(IModel);
}

bool UNNERuntimeIREEGpu::IsAvailable() const
{
	return false;
}

FGuid UNNERuntimeIREEGpu::GetGUID() const
{
	return FGuid();
}

int32 UNNERuntimeIREEGpu::GetVersion() const
{
	return 0;
}

FGuid UNNERuntimeIREECuda::GUID = FGuid((int32)'I', (int32)'G', (int32)'C', (int32)'U');
int32 UNNERuntimeIREECuda::Version = 0x00000001;

FString UNNERuntimeIREECuda::GetRuntimeName() const
{
	return TEXT("NNERuntimeIREECuda");
}

bool UNNERuntimeIREECuda::IsAvailable() const
{
	return false;
}

FGuid UNNERuntimeIREECuda::GetGUID() const
{
	return GUID;
}

int32 UNNERuntimeIREECuda::GetVersion() const
{
	return Version;
}

FGuid UNNERuntimeIREEVulkan::GUID = FGuid((int32)'I', (int32)'G', (int32)'V', (int32)'U');
int32 UNNERuntimeIREEVulkan::Version = 0x00000001;

FString UNNERuntimeIREEVulkan::GetRuntimeName() const
{
	return TEXT("NNERuntimeIREEVulkan");
}

bool UNNERuntimeIREEVulkan::IsAvailable() const
{
	return false;
}

FGuid UNNERuntimeIREEVulkan::GetGUID() const
{
	return GUID;
}

int32 UNNERuntimeIREEVulkan::GetVersion() const
{
	return Version;
}

FGuid UNNERuntimeIREERdg::GUID = FGuid((int32)'I', (int32)'R', (int32)'D', (int32)'G');
int32 UNNERuntimeIREERdg::Version = 0x00000001;

FString UNNERuntimeIREERdg::GetRuntimeName() const
{
	return TEXT("NNERuntimeIREERdg");
}

UNNERuntimeIREERdg::ECanCreateModelDataStatus UNNERuntimeIREERdg::CanCreateModelData(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const
{
#if WITH_EDITOR
	return	FileType.Compare(TEXT("mlir"), ESearchCase::IgnoreCase) == 0 ? ECanCreateModelDataStatus::Ok : ECanCreateModelDataStatus::FailFileIdNotSupported;
#else
	return ECanCreateModelDataStatus::Fail;
#endif // WITH_EDITOR
}

TSharedPtr<UE::NNE::FSharedModelData> UNNERuntimeIREERdg::CreateModelData(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform)
{
	return TSharedPtr<UE::NNE::FSharedModelData>();
}

FString UNNERuntimeIREERdg::GetModelDataIdentifier(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const
{
	FString PlatformName = TargetPlatform ? TargetPlatform->IniPlatformName() : UGameplayStatics::GetPlatformName();
	return UE::NNERuntimeIREE::CPU::Private::GetModelDataIdentifier(GetRuntimeName(), UNNERuntimeIREERdg::GUID, FileId.ToString(EGuidFormats::Digits), PlatformName, "");
}

UNNERuntimeIREERdg::ECanCreateModelRDGStatus UNNERuntimeIREERdg::CanCreateModelRDG(const TObjectPtr<UNNEModelData> ModelData) const
{
	check(ModelData != nullptr);

	TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName());
	if (!SharedData.IsValid())
	{
		return ECanCreateModelRDGStatus::Fail;
	}

	TConstArrayView<uint8> SharedDataView = SharedData->GetView();
	int32 GuidSize = sizeof(UNNERuntimeIREERdg::GUID);
	int32 VersionSize = sizeof(UNNERuntimeIREERdg::Version);
	if (SharedDataView.Num() <= GuidSize + VersionSize)
	{
		return ECanCreateModelRDGStatus::Fail;
	}

	bool bResult = FGenericPlatformMemory::Memcmp(&(SharedDataView[0]), &(UNNERuntimeIREERdg::GUID), GuidSize) == 0;
	bResult &= FGenericPlatformMemory::Memcmp(&(SharedDataView[GuidSize]), &(UNNERuntimeIREERdg::Version), VersionSize) == 0;

	return bResult ? ECanCreateModelRDGStatus::Ok : ECanCreateModelRDGStatus::Fail;
}

TSharedPtr<UE::NNE::IModelRDG> UNNERuntimeIREERdg::CreateModelRDG(const TObjectPtr<UNNEModelData> ModelData)
{
	check(ModelData != nullptr);

	if (CanCreateModelRDG(ModelData) != ECanCreateModelRDGStatus::Ok)
	{
		UE_LOG(LogNNE, Warning, TEXT("UNNERuntimeIREERdg cannot create a model from the model data with id %s"), *ModelData->GetFileId().ToString(EGuidFormats::Digits));
		return TSharedPtr<UE::NNE::IModelRDG>();
	}

	check(ModelData->GetModelData(GetRuntimeName()).IsValid());

	UE::NNE::IModelRDG* IModel = nullptr;
	TConstArrayView<uint8> SharedDataView = ModelData->GetModelData(GetRuntimeName())->GetView();

	if (FEngineAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> Attributes = MakeAnalyticsEventAttributeArray(
			TEXT("PlatformName"), UGameplayStatics::GetPlatformName(),
			TEXT("HashedRuntimeName"), FMD5::HashAnsiString(*GetRuntimeName()),
			TEXT("ModelDataSize"), SharedDataView.Num()
		);
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("NeuralNetworkEngine.CreateModel"), Attributes);
	}

	return TSharedPtr<UE::NNE::IModelRDG>(IModel);
}

bool UNNERuntimeIREERdg::IsAvailable() const
{
	return false;
}

#endif // WITH_NNE_RUNTIME_IREE