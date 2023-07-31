// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/NiagaraDumpBytecodeCommandlet.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "CollectionManagerModule.h"
#include "CollectionManagerTypes.h"
#include "HAL/FileManager.h"
#include "ICollectionManager.h"
#include "Misc/OutputDeviceArchiveWrapper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "NiagaraSystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraDumpBytecodeCommandlet)

DEFINE_LOG_CATEGORY_STATIC(LogNiagaraDumpBytecodeCommandlet, Log, All);

UNiagaraDumpByteCodeCommandlet::UNiagaraDumpByteCodeCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UNiagaraDumpByteCodeCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;

	ParseCommandLine(*Params, Tokens, Switches);

	if (!FParse::Value(*Params, TEXT("AuditOutputFolder="), AuditOutputFolder))
	{
		// No output folder specified. Use the default folder.
		AuditOutputFolder = FPaths::ProjectSavedDir() / TEXT("Audit");
	}

	// Add a timestamp to the folder
	AuditOutputFolder /= FDateTime::Now().ToString();

	FParse::Value(*Params, TEXT("FilterCollection="), FilterCollection);

	// Package Paths
	FString PackagePathsString;
	if (FParse::Value(*Params, TEXT("PackagePaths="), PackagePathsString, false))
	{
		TArray<FString> PackagePathsStrings;
		PackagePathsString.ParseIntoArray(PackagePathsStrings, TEXT(","));
		for (const FString& v : PackagePathsStrings)
		{
			PackagePaths.Add(FName(v));
		}
	}

	if (PackagePaths.Num() == 0)
	{
		PackagePaths.Add(FName(TEXT("/Game")));
	}

	if (Switches.Contains("COOKED"))
	{
		ForceBakedRapidIteration = true;
		ForceAttributeTrimming = true;
	}

	if (Switches.Contains("BAKED"))
	{
		ForceBakedRapidIteration = true;
	}

	if (Switches.Contains("TRIMMED"))
	{
		ForceAttributeTrimming = true;
	}

	ProcessNiagaraScripts();

	return 0;
}

void UNiagaraDumpByteCodeCommandlet::ProcessNiagaraScripts()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	AssetRegistry.SearchAllAssets(true);

	FARFilter Filter;
	Filter.PackagePaths = PackagePaths;
	Filter.bRecursivePaths = true;

	Filter.ClassPaths.Add(UNiagaraSystem::StaticClass()->GetClassPathName());
	if (!FilterCollection.IsEmpty())
	{
		FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
		CollectionManagerModule.Get().GetObjectsInCollection(FName(*FilterCollection), ECollectionShareType::CST_All, Filter.SoftObjectPaths, ECollectionRecursionFlags::SelfAndChildren);
	}

	TArray<FAssetData> AssetList;
	AssetRegistry.GetAssets(Filter, AssetList);

	const static UEnum* UsageEnum = StaticEnum<ENiagaraScriptUsage>();

	const double StartProcessNiagaraSystemsTime = FPlatformTime::Seconds();

	//  Iterate over all scripts
	const FString DevelopersFolder = FPackageName::FilenameToLongPackageName(FPaths::GameDevelopersDir().LeftChop(1));
	FString LastPackageName = TEXT("");
	UPackage* CurrentPackage = nullptr;
	for (const FAssetData& AssetIt : AssetList)
	{
		const FString SystemName = AssetIt.GetObjectPathString();
		const FString PackageName = AssetIt.PackageName.ToString();

		if (PackageName.StartsWith(DevelopersFolder))
		{
			// Skip developer folders
			continue;
		}

		if (PackageName != LastPackageName)
		{
			UPackage* Package = ::LoadPackage(nullptr, *PackageName, LOAD_None);
			if (Package != nullptr)
			{
				LastPackageName = PackageName;
				Package->FullyLoad();
				CurrentPackage = Package;
			}
			else
			{
				UE_LOG(LogNiagaraDumpBytecodeCommandlet, Warning, TEXT("Failed to load package %s processing %s"), *PackageName, *SystemName);
				CurrentPackage = nullptr;
			}
		}

		const FString ShorterSystemName = AssetIt.AssetName.ToString();
		UNiagaraSystem* NiagaraSystem = FindObject<UNiagaraSystem>(CurrentPackage, *ShorterSystemName);
		if (NiagaraSystem == nullptr)
		{
			UE_LOG(LogNiagaraDumpBytecodeCommandlet, Warning, TEXT("Failed to load Niagara system %s"), *SystemName);
			continue;
		}

		NiagaraSystem->WaitForCompilationComplete();

		if (ForceBakedRapidIteration || ForceAttributeTrimming)
		{
			if (ForceBakedRapidIteration)
			{
				NiagaraSystem->SetBakeOutRapidIterationOnCook(true);
			}
			if (ForceAttributeTrimming)
			{
				NiagaraSystem->SetTrimAttributesOnCook(true);
			}

			NiagaraSystem->RequestCompile(true);
			NiagaraSystem->WaitForCompilationComplete(true);
		}

		if (!NiagaraSystem->IsValid())
		{
			UE_LOG(LogNiagaraDumpBytecodeCommandlet, Warning, TEXT("Loaded system was Invalid! %s"), *SystemName);
		}

		const FString SystemPathName = NiagaraSystem->GetPathName();
		const FString HashedPathName = FString::Printf(TEXT("%08x"), GetTypeHash(SystemPathName));

		IFileManager::Get().MakeDirectory(*(AuditOutputFolder / HashedPathName));
		DumpByteCode(NiagaraSystem->GetSystemSpawnScript(), SystemPathName, HashedPathName, TEXT("SystemSpawnScript.txt"));
		DumpByteCode(NiagaraSystem->GetSystemUpdateScript(), SystemPathName, HashedPathName, TEXT("SystemUpdateScript.txt"));
		
		for (const auto& EmitterHandle : NiagaraSystem->GetEmitterHandles())
		{
			if (!EmitterHandle.GetIsEnabled())
			{
				continue;
			}

			if (FVersionedNiagaraEmitterData* Emitter = EmitterHandle.GetEmitterData())
			{
				if (Emitter->SimTarget == ENiagaraSimTarget::CPUSim)
				{
					const FString EmitterName = EmitterHandle.GetUniqueInstanceName();

					TArray<UNiagaraScript*> EmitterScripts;
					Emitter->GetScripts(EmitterScripts);

					IFileManager::Get().MakeDirectory(*(HashedPathName / EmitterName));

					for (const auto* EmitterScript : EmitterScripts)
					{
						DumpByteCode(EmitterScript, SystemPathName, HashedPathName, EmitterName / UsageEnum->GetNameStringByValue(static_cast<int64>(EmitterScript->GetUsage())) + TEXT(".txt"));
					}
				}
			}
		}
	}

	// sort the data alphabetically (based on MetaData.FullName
	ScriptMetaData.StableSort([](const FScriptMetaData& Lhs, const FScriptMetaData& Rhs)
	{
		return Lhs.FullName.Compare(Rhs.FullName, ESearchCase::IgnoreCase) < 0;
	});

	// create the xml for the meta data
	{
		const FString MetaDataFileName = AuditOutputFolder / TEXT("NiagaraScripts.xml");
		TUniquePtr<FArchive> FileArchive(IFileManager::Get().CreateDebugFileWriter(*MetaDataFileName));
		TUniquePtr<FOutputDeviceArchiveWrapper> OutputStream(new FOutputDeviceArchiveWrapper(FileArchive.Get()));

		OutputStream->Log(TEXT("<?xml version='1.0' ?>"));
		OutputStream->Log(TEXT("<Scripts>"));
		for (const auto& MetaData : ScriptMetaData)
		{
			OutputStream->Log(TEXT("\t<Script>"));
			OutputStream->Logf(TEXT("\t\t<Hash>%s</Hash>"), *MetaData.SystemHash);
			OutputStream->Logf(TEXT("\t\t<Name>%s</Name>"), *MetaData.FullName);
			OutputStream->Logf(TEXT("\t\t<OpCount>%d</OpCount>"), MetaData.OpCount);
			OutputStream->Logf(TEXT("\t\t<RegisterCount>%d</RegisterCount>"), MetaData.RegisterCount);
			OutputStream->Logf(TEXT("\t\t<ConstantCount>%d</ConstantCount>"), MetaData.ConstantCount);
			OutputStream->Logf(TEXT("\t\t<AttributeCount>%d</AttributeCount>"), MetaData.AttributeCount);
			OutputStream->Log(TEXT("\t</Script>"));
		}
		OutputStream->Log(TEXT("</Scripts>"));
	}

	// create the csv for the meta data
	{
		const FString MetaDataFileName = AuditOutputFolder / TEXT("NiagaraScripts.csv");
		TUniquePtr<FArchive> FileArchive(IFileManager::Get().CreateDebugFileWriter(*MetaDataFileName));
		TUniquePtr<FOutputDeviceArchiveWrapper> OutputStream(new FOutputDeviceArchiveWrapper(FileArchive.Get()));

		OutputStream->Log(TEXT("Hash, Name, OpCount, RegisteredCount, ConstantCount, AttributeCount"));
		for (const auto& MetaData : ScriptMetaData)
		{
			OutputStream->Logf(TEXT("%s, %s, %d, %d, %d, %d"),
				*MetaData.SystemHash,
				*MetaData.FullName,
				MetaData.OpCount,
				MetaData.RegisterCount,
				MetaData.ConstantCount,
				MetaData.AttributeCount);
		}
	}

	// Probably don't need to do this, but just in case we have any 'hanging' packages 
	// and more processing steps are added later, let's clean up everything...
	::CollectGarbage(RF_NoFlags);

	double ProcessNiagaraSystemsTime = FPlatformTime::Seconds() - StartProcessNiagaraSystemsTime;
	UE_LOG(LogNiagaraDumpBytecodeCommandlet, Log, TEXT("Took %5.3f seconds to process referenced Niagara systems..."), ProcessNiagaraSystemsTime);
}

void UNiagaraDumpByteCodeCommandlet::DumpByteCode(const UNiagaraScript* Script, const FString& PathName, const FString& HashName, const FString& FilePath)
{
	if (!Script)
	{
		return;
	}

	const auto& ExecData = Script->GetVMExecutableData();

	FScriptMetaData& MetaData = ScriptMetaData.AddZeroed_GetRef();
	MetaData.SystemHash = HashName;
	MetaData.FullName = PathName / FilePath;
	MetaData.RegisterCount = ExecData.NumTempRegisters;
	MetaData.OpCount = ExecData.LastOpCount;
	MetaData.ConstantCount = ExecData.InternalParameters.GetTableSize() / 4;
	MetaData.AttributeCount = 0;

	for (const FNiagaraVariableBase& Var : ExecData.Attributes)
	{
		MetaData.AttributeCount += Var.GetType().GetSize() / 4;
	}

	//const static UEnum* VmOpEnum = StaticEnum<EVectorVMOp>();

	if (Script)
	{
		const FString FullFilePath = AuditOutputFolder / HashName / FilePath;

		TUniquePtr<FArchive> FileArchive(IFileManager::Get().CreateDebugFileWriter(*FullFilePath));
		if (!FileArchive)
		{
			UE_LOG(LogNiagaraDumpBytecodeCommandlet, Warning, TEXT("Failed to create output stream %s"), *FullFilePath);
		}

		TUniquePtr<FOutputDeviceArchiveWrapper> OutputStream(new FOutputDeviceArchiveWrapper(FileArchive.Get()));

		const auto& VMData = Script->GetVMExecutableData();

		TArray<FString> AssemblyLines;
		VMData.LastAssemblyTranslation.ParseIntoArrayLines(AssemblyLines, false);

		// we want to translate all instances of OP_X into the string from the VMVector enum
		for (FString& CurrentLine : AssemblyLines)
		{
			int OpStartIndex = CurrentLine.Find(TEXT("OP_"), ESearchCase::CaseSensitive);
			if (OpStartIndex != INDEX_NONE)
			{
				const int32 OpEndStartSearch = OpStartIndex + 3;
				int OpEndIndex = CurrentLine.Find(TEXT("("), ESearchCase::IgnoreCase, ESearchDir::FromStart, OpEndStartSearch);
				if (OpEndIndex == INDEX_NONE)
				{
					OpEndIndex = CurrentLine.Find(TEXT(";"), ESearchCase::IgnoreCase, ESearchDir::FromStart, OpEndStartSearch);
				}

				if (OpEndIndex != INDEX_NONE)
				{
					const FString OpIndexString = CurrentLine.Mid(OpEndStartSearch, OpEndIndex - OpEndStartSearch);
					int OpIndexValue = FCString::Atoi(*OpIndexString);

					FString LinePrefix = CurrentLine.Left(OpStartIndex);
					FString LineSuffix = CurrentLine.RightChop(OpEndIndex);

					
					CurrentLine = LinePrefix + TEXT(":") + VectorVM::GetOpName((EVectorVMOp)OpIndexValue) + LineSuffix;
				}
			}
			OutputStream->Log(CurrentLine);
		}
	}
}
