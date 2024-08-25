// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/CookShadersCommandlet.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "GlobalShader.h"
#include "HAL/FileManager.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "SceneTypes.h"
#include "ShaderCompiler.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogCookShadersCommandlet, Log, All);

static const TCHAR* GlobalName = TEXT("Global");
static const TCHAR* NiagaraName = TEXT("Niagara");

// Examples
// UnrealEditor-Cmd.exe <proj> -run=CookShaders -targetPlatform=<platform> -infoFile=D:\ShaderSymbols\ShaderSymbols.info -ShaderSymbolsExport=D:\ShaderSymbols\Out -filter=Mannequin
// UnrealEditor-Cmd.exe <proj> -run=CookShaders -targetPlatform=<platform> -infoFile=D:\ShaderSymbols\ShaderSymbols.info -ShaderSymbolsExport=D:\ShaderSymbols\Out -filter=00FB89F127D2DC10 -noglobals
// UnrealEditor-Cmd.exe <proj> -run=CookShaders -targetPlatform=<platform> -ShaderSymbolsExport=D:\ShaderSymbols\Out -material=M_UI_Base_BordersAndButtons
//
// Use -dpcvars="r.Shaders.Symbols=1" to force on symbols writing from the commandline, you can also edit the appropriate [Platform]Engine.ini and uncomment or add "r.Shaders.Symbols=1", especially if you want symbols enabled longer term for that specific platform.

namespace CookShadersCommandlet {

	// ShaderSymbols.info files will have a series of lines like the following, where the specifics of hash and extension are platform specific
	// hash0.extension Global/FTonemapCS/2233
	// hash1.extension M_Material_Name_ad9c64900150ee77/Default/FLocalVertexFactory/TBasePassPSFNoLightMapPolicy/0
	// hash2.extension NS_Niagara_System_Name/Emitted/ParticleGPUComputeScript/FNiagaraShader/0
	//
	// FInfoRecord will contain a deconstructed version of a single line from this file
	struct FInfoRecord
	{
		FString Hash;
		FString Type;
		FString Name;
		EMaterialQualityLevel::Type Quality;
		FString Emitter;
		FString Shader;
		FString VertexFactory;
		FString Pipeline;
		int32 Permutation;
	};

	// Commandlet can't get to similar list in MaterialShared.cpp as the accessors are just externs
	FName MaterialQualityLevelNames[] =
	{
		FName(TEXT("Low")),
		FName(TEXT("High")),
		FName(TEXT("Medium")),
		FName(TEXT("Epic")),
		FName(TEXT("Num"))
	};
	static_assert(UE_ARRAY_COUNT(MaterialQualityLevelNames) == EMaterialQualityLevel::Num + 1, "Missing entry from material quality level names.");

	bool LoadAndParse(const FString& Path, const FString& Filter, TArray<FInfoRecord>& OutInfo)
	{
		IFileManager& FileManager = IFileManager::Get();
		TUniquePtr<FArchive> Reader = TUniquePtr<FArchive>(FileManager.CreateFileReader(*Path));
		if (Reader.IsValid())
		{
			int64 Size = Reader->TotalSize();
			TArray<uint8> RawData;
			RawData.AddUninitialized(Size);
			Reader->Serialize(RawData.GetData(), Size);
			Reader->Close();

			TArray<FString> Lines;
			FString(StringCast<TCHAR>(reinterpret_cast<const ANSICHAR*>(RawData.GetData())).Get()).ParseIntoArrayLines(Lines);

			for (const FString& Line : Lines)
			{
				int32 Space;
				Line.FindChar(TEXT(' '), Space);
				if (Space != INDEX_NONE)
				{
					FString HashString = Line.Left(Space);
					FString DataString = Line.Right(Line.Len() - Space - 1);

					// add to our list if it passes the filter
					if (Filter.IsEmpty() || HashString.Contains(Filter) || DataString.Contains(Filter))
					{
						FInfoRecord Record;
						Record.Hash = HashString;

						TArray<FString> Substrings;
						DataString.ParseIntoArray(Substrings, TEXT("/"));

						// need to have 3 or more parts
						if (Substrings.Num() >= 3)
						{
							// always ends in a shader/permutation
							Record.Permutation = FCString::Atoi(*Substrings[Substrings.Num() - 1]);
							Record.Shader = Substrings[Substrings.Num() - 2];

							// check for Niagara
							if (Record.Shader == TEXT("FNiagaraShader"))
							{
								Record.Type = NiagaraName;
								Record.Name = Substrings[0];
								Record.Emitter = Substrings[1];
							}
							else
							{
								// either material or global, we need to reconstruct the name
								TArray<FString> NameParts;
								Substrings[0].ParseIntoArray(NameParts, TEXT("_"));
								if (NameParts.Num() == 1)
								{
									// probably "Global"
									Record.Name = Substrings[0];
								}
								else
								{
									// probably "M_Name_MoreName_UIDNUM"
									FString UID = NameParts[NameParts.Num() - 1];
									Record.Name = Substrings[0].Left(Substrings[0].Len() - UID.Len() - 1);
								}

								if (Record.Name == GlobalName)
								{
									Record.Type = GlobalName;
									Record.Shader = Substrings[1];
									Record.Name = Record.Shader;
								}
								else
								{
									Record.Type = TEXT("Material");

									// default is Num
									Record.Quality = EMaterialQualityLevel::Num;
									FName QualityName(Substrings[1]);
									for (int32 q = 0; q < EMaterialQualityLevel::Num; ++q)
									{
										auto QualityLevel = static_cast<EMaterialQualityLevel::Type>(q);

										if (MaterialQualityLevelNames[q] == QualityName)
										{
											Record.Quality = QualityLevel;
											break;
										}
									}

									// if it has 5 or more parts, Num-3 is the vertex factory
									if (Substrings.Num() >= 5)
									{
										Record.VertexFactory = Substrings[Substrings.Num() - 3];
									}

									// if it has 6 parts, Num-4 is the pipeline
									if (Substrings.Num() == 6)
									{
										Record.Pipeline = Substrings[Substrings.Num() - 4];
									}
								}
							}

							OutInfo.Emplace(Record);
						}
					}
				}
			}

			return true;
		}

		return false;
	}
};

using namespace CookShadersCommandlet;

UCookShadersCommandlet::UCookShadersCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UCookShadersCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);

	// Display help
	if (Switches.Contains("help"))
	{
		UE_LOG(LogCookShadersCommandlet, Log, TEXT("CookShadersCommandlet"));
		UE_LOG(LogCookShadersCommandlet, Log, TEXT("Cook shaders based upon the options, ideal for generating pdbs for shaders you need"));
		UE_LOG(LogCookShadersCommandlet, Log, TEXT("Options:"));
		UE_LOG(LogCookShadersCommandlet, Log, TEXT(" Required: -targetPlatform=<platform>     (Which target platform do you want results, e.g. WindowsClient, etc."));
		UE_LOG(LogCookShadersCommandlet, Log, TEXT(" Required: -ShaderSymbolsExport=<path>    (Set shader symbols output location."));
		UE_LOG(LogCookShadersCommandlet, Log, TEXT(" Optional: -infoFile=<path>               (Path to ShaderSymbols.info file you want to find shaders from."));
		UE_LOG(LogCookShadersCommandlet, Log, TEXT(" Optional: -filter=<string>               (Recommended! Filter to shaders with <string> in their hash or info data, requires -infoFile)."));
		UE_LOG(LogCookShadersCommandlet, Log, TEXT(" Optional: -material=<string>             (Cook this material if you don't have a .info file, can be Global for global shaders)."));
		UE_LOG(LogCookShadersCommandlet, Log, TEXT(" Optional: -noglobals                     (Don't do global shaders, even if they match the filter.)"));
		UE_LOG(LogCookShadersCommandlet, Log, TEXT(" Optional: -nomaterialinstances           (Don't do material instances, this search is currently slow.)"));
		return 0;
	}

	// Setup
	FString Filter;
	FParse::Value(*Params, TEXT("filter="), Filter, true);
	FString MaterialString;
	FParse::Value(*Params, TEXT("material="), MaterialString, true);
	FString InfoFilePath;
	FParse::Value(*Params, TEXT("infoFile="), InfoFilePath, true);
	FString ExportPath;
	FParse::Value(*Params, TEXT("ShaderSymbolsExport="), ExportPath, true);
	const bool bNoGlobals = Switches.Contains(TEXT("noglobals"));
	const bool bNoMaterialInstances = Switches.Contains(TEXT("nomaterialinstances"));

	TArray<FInfoRecord> Info;

	// Check to see if we want Globals specifically
	TSet<FString> GlobalsToFind;
	if (!MaterialString.IsEmpty())
	{
		if (MaterialString == GlobalName)
		{
			// we don't have a way to specify global shader name, or compile one specifically
			GlobalsToFind.Add(GlobalName);
			MaterialString = TEXT("");
		}
	}

	// Load info file if requested
	if (!InfoFilePath.IsEmpty())
	{
		if (!LoadAndParse(InfoFilePath, Filter, Info))
		{
			UE_LOG(LogCookShadersCommandlet, Log, TEXT("Unabled to read / parse info file '%s'"), *InfoFilePath);
			return 0;
		}
	}

	// Pre-process the info we have, separating out individual requests
	TArray<FODSCRequestPayload> IndividualRequests;
	for (const auto& I : Info)
	{
		if (I.Type == GlobalName)
		{
			GlobalsToFind.Add(I.Name);
		}
		else if (I.Type == TEXT("Material"))
		{
			TArray<FString> ShaderTypeNames;
			ShaderTypeNames.Add(I.Shader);
			IndividualRequests.Add(FODSCRequestPayload(
				EShaderPlatform::SP_NumPlatforms, ERHIFeatureLevel::Num,
				I.Quality, I.Name, I.VertexFactory, I.Pipeline, ShaderTypeNames, I.Permutation, I.Hash));
		}
	}

	// Load asset lists
	UE_LOG(LogCookShadersCommandlet, Display, TEXT("Loading Asset Registry..."));
	IAssetRegistry& AssetRegistry = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	AssetRegistry.SearchAllAssets(true);

	TArray<FAssetData> MaterialList;
	TArray<FAssetData> MaterialInstanceList;
	if (!AssetRegistry.IsLoadingAssets())
	{
		if (!MaterialString.IsEmpty() || !IndividualRequests.IsEmpty())
		{
			AssetRegistry.GetAssetsByClass(UMaterial::StaticClass()->GetClassPathName(), MaterialList, true);
			AssetRegistry.GetAssetsByClass(UMaterialInstance::StaticClass()->GetClassPathName(), MaterialInstanceList, true);
		}
	}

	// Locate full paths for the materials we have individual requests for
	TArray<UMaterialInterface*> MaterialsToFindInstancesOf;
	for (auto& I : IndividualRequests)
	{
		for (const FAssetData& It : MaterialList)
		{
			if (It.AssetName == I.MaterialName)
			{
				UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *It.GetObjectPathString());
				MaterialsToFindInstancesOf.Add(Material);
				I.MaterialName = Material->GetPathName();
				break;
			}
		}
	}

	// Locate full paths for the materials which match the command line material param
	TArray<FString> MaterialsRequested;
	if (!MaterialString.IsEmpty())
	{
		for (const FAssetData& It : MaterialList)
		{
			FString AssetString = It.AssetName.ToString();
			if (AssetString.Contains(MaterialString))
			{
				UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *It.GetObjectPathString());
				MaterialsRequested.Add(Material->GetPathName());
				MaterialsToFindInstancesOf.Add(Material);
			}
		}
	}

	// Iterate material instances, need to find instances which depend upon the list of base materials
	if (!bNoMaterialInstances && MaterialsToFindInstancesOf.Num())
	{
		int32 Count = 0;
		double StartTime = FPlatformTime::Seconds();
		TArray<FODSCRequestPayload> InstancedRequests;
		for (const FAssetData& It : MaterialInstanceList)
		{
			Count++;
			if (FPlatformTime::Seconds() > StartTime + 10.0)
			{
				StartTime = FPlatformTime::Seconds();
				UE_LOG(LogCookShadersCommandlet, Display, TEXT("MaterialInstances %d/%d..."), Count, MaterialInstanceList.Num());
			}

			UMaterialInterface* MaterialInstance = LoadObject<UMaterialInterface>(nullptr, *It.GetObjectPathString());
			if (MaterialInstance)
			{
				TArray<FODSCRequestPayload>::TConstIterator RequestIt(IndividualRequests);
				for (auto* Material : MaterialsToFindInstancesOf)
				{
					if (MaterialInstance->IsDependent(Material))
					{
						if (IndividualRequests.IsEmpty())
						{
							// We are matching a set of materials, simply add to the list
							MaterialsRequested.Add(MaterialInstance->GetPathName());
						}
						else if (RequestIt)
						{
							// Make a new individual request, same as base with instance material name
							FODSCRequestPayload Req(*RequestIt);
							Req.MaterialName = MaterialInstance->GetPathName();
							InstancedRequests.Add(Req);
						}
					}

					// if we have individual base requests, go to next one
					if (RequestIt)
					{
						++RequestIt;
					}
				}
			}
		}

		IndividualRequests.Append(InstancedRequests);
	}

	// Did we find anything to do? 
	if (MaterialsRequested.IsEmpty() && GlobalsToFind.IsEmpty() && IndividualRequests.IsEmpty())
	{
		UE_LOG(LogCookShadersCommandlet, Display, TEXT("Couldn't find anything to process!"));
		return 0;
	}

	// Iterate over the active platforms
	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	const TArray<ITargetPlatform*>& Platforms = TPM->GetActiveTargetPlatforms();
	for (int32 Index = 0; Index < Platforms.Num(); Index++)
	{
		TArray<FName> DesiredShaderFormats;
		Platforms[Index]->GetAllTargetedShaderFormats(DesiredShaderFormats);

		for (int32 FormatIndex = 0; FormatIndex < DesiredShaderFormats.Num(); FormatIndex++)
		{
			const auto* Platform = Platforms[Index];
			const EShaderPlatform ShaderPlatform = ShaderFormatToLegacyShaderPlatform(DesiredShaderFormats[FormatIndex]);
			FString ShaderPlatformName = LexToString(ShaderPlatform);
			FString PlatformName = Platform->PlatformName();
			ERHIFeatureLevel::Type FeatureLevel = GetMaxSupportedFeatureLevel(ShaderPlatform);

			UE_LOG(LogCookShadersCommandlet, Log, TEXT("Working on %s %s"), *PlatformName, *ShaderPlatformName);

			// Setup
			TArray<uint8> OutGlobalShaderMap;
			TArray<uint8> OutMeshMaterialMaps;
			TArray<FString> OutModifiedFiles;
			FString OutputDir;
			FShaderRecompileData Arguments(PlatformName, ShaderPlatform, ODSCRecompileCommand::None, &OutModifiedFiles, &OutMeshMaterialMaps, &OutGlobalShaderMap);

			// Cook individual requests
			if (!IndividualRequests.IsEmpty())
			{
				UE_LOG(LogCookShadersCommandlet, Display, TEXT("Cooking Individual Shaders..."));

				// Adjust our requests for the current Platform and Feature Level and run them
				for (auto& I : IndividualRequests)
				{
					I.ShaderPlatform = ShaderPlatform;
					I.FeatureLevel = FeatureLevel;
				}
				Arguments.ShadersToRecompile = IndividualRequests;
				RecompileShadersForRemote(Arguments, OutputDir);
			}

			// Cook global shaders unless disabled
			if (!bNoGlobals && !GlobalsToFind.IsEmpty())
			{
				// GlobalsToFind has the list of global shaders we are interested in, although we can only compile all globals today
				UE_LOG(LogCookShadersCommandlet, Display, TEXT("Cooking Global Shaders..."));
				Arguments.CommandType = ODSCRecompileCommand::Global;
				RecompileShadersForRemote(Arguments, OutputDir);
			}

			// Cook materials
			if (!MaterialsRequested.IsEmpty())
			{
				UE_LOG(LogCookShadersCommandlet, Display, TEXT("Cooking Materials..."));
				Arguments.CommandType = ODSCRecompileCommand::Material;
				Arguments.MaterialsToLoad = MaterialsRequested;
				RecompileShadersForRemote(Arguments, OutputDir);
			}
		}
	}

	UE_LOG(LogCookShadersCommandlet, Display, TEXT("Done CookShadersCommandlet"));
	return 0;
}


