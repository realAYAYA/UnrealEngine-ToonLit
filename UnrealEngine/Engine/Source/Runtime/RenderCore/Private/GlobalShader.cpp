// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GlobalShader.cpp: Global shader implementation.
=============================================================================*/

#include "GlobalShader.h"

#include "Interfaces/ITargetPlatform.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryWriter.h"
#include "Misc/CoreMisc.h"
#include "Misc/ConfigCacheIni.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Containers/StaticBitArray.h"
#include "ShaderCodeLibrary.h"

/** The global shader map. */
FGlobalShaderMap* GGlobalShaderMap[SP_NumPlatforms] = {};

IMPLEMENT_TYPE_LAYOUT(FGlobalShader);
IMPLEMENT_TYPE_LAYOUT(FGlobalShaderMapContent);

IMPLEMENT_SHADER_TYPE(,FNULLPS,TEXT("/Engine/Private/NullPixelShader.usf"),TEXT("Main"),SF_Pixel);

/** A per-platform map of global shader defines that come from config .ini */
class FGlobalShaderConfigDefines
{
public:

	static void ApplyConfigDefines(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FName ShaderFormat;
		if (const FPlatformInfo* Platform = GetPlatformInfoAndErrorCheck(Parameters.Platform, ShaderFormat))
		{
			if (const TArray<FDefine>* ShaderDefines = Platform->ShaderDefines.Find(Parameters.GlobalShaderName))
			{			
				for (const FDefine& Define : *ShaderDefines)
				{
					if (Define.IsRelevant(ShaderFormat, Parameters.PermutationId))
					{
						OutEnvironment.SetDefineAndCompileArgument(*Define.Name, Define.Value);
					}
				}
			}
		}
	}

	static void AppendKeyString(FString& KeyString, FName ShaderName, int32 PermutationID, FName IniPlatform, EShaderPlatform ShaderPlatform)
	{
		FName ShaderFormat;
		if (const FPlatformInfo* Platform = GetPlatformInfoAndErrorCheck(ShaderPlatform, ShaderFormat))
		{
			if (const TArray<FDefine>* ShaderDefines = Platform->ShaderDefines.Find(ShaderName))
			{
				TArray<FDefine> FilteredDefines;
				for (const FDefine& Define : *ShaderDefines)
				{
					if (Define.IsRelevant(ShaderFormat, PermutationID))
					{
						FilteredDefines.Add(Define);
					}
				}

				if (FilteredDefines.Num() > 0)
				{
					// Sort and hash the applicable defines
					FilteredDefines.Sort();

					FSHA1 HashState;
					for (const FDefine& Define : FilteredDefines)
					{
						HashState.UpdateWithString(*Define.Name, Define.Name.Len());
						HashState.UpdateWithString(*Define.Value, Define.Value.Len());

						const uint32 ShaderFormatHash = GetTypeHash(Define.ShaderFormat);
						HashState.Update((const uint8*)&ShaderFormatHash, sizeof(ShaderFormatHash));

						HashState.Update((const uint8*)&Define.PermutationID, sizeof(Define.PermutationID));
					}

					KeyString += HashState.Finalize().ToString();
				}
			}
		}
	}

private:
	struct FDefine
	{
		FString Name;
		FString Value;
		FName ShaderFormat = NAME_None;
		int32 PermutationID = INDEX_NONE;

		bool IsRelevant(FName InShaderFormat, int32 InPermutationID) const
		{
			if (ShaderFormat != NAME_None && ShaderFormat != InShaderFormat)
			{
				// This define is inappropriate for the target shader format
				return false;
			}

			if (PermutationID != INDEX_NONE && PermutationID != InPermutationID)
			{
				// This define is inappropriate for the permutation being compiled
				return false;
			}

			return true;
		}

		int32 Compare(const FDefine& Other) const
		{
			int Cmp = Name.Compare(Other.Name, ESearchCase::IgnoreCase);
			if (Cmp == 0)
			{
				Cmp = Value.Compare(Other.Value, ESearchCase::CaseSensitive);
				if (Cmp == 0)
				{
					Cmp = ShaderFormat.Compare(Other.ShaderFormat);
					if (Cmp == 0)
					{
						Cmp = Other.PermutationID - PermutationID;
					}
				}
			}

			return Cmp;
		}

		bool operator==(const FDefine& Other) const
		{
			return Name.Equals(Other.Name, ESearchCase::IgnoreCase)
				&& Value == Other.Value
				&& ShaderFormat == Other.ShaderFormat
				&& PermutationID == Other.PermutationID;
		}

		bool operator!=(const FDefine& Other) const
		{
			return !(*this == Other);
		}

		bool operator<(const FDefine& Other) const
		{
			return Compare(Other) < 0;
		}
	};

	using FShaderDefines = TMap<FName, TArray<FDefine>>;

	struct FPlatformInfo
	{
		FName Name;
		bool bInitializedFromConfig = false;
		FShaderDefines ShaderDefines;
	};
	
	static TMap<FName, FPlatformInfo> ConfigDefines;
	static TStaticBitArray<EShaderPlatform::SP_NumPlatforms> ErrorCheckedPlatforms;

	static const FPlatformInfo* GetPlatformInfoAndErrorCheck(EShaderPlatform ShaderPlatform, FName& OutShaderFormat)
	{
		OutShaderFormat = LegacyShaderPlatformToShaderFormat(ShaderPlatform);

		// Search for all target platforms that support this shader platform
		TArray<FName, TInlineAllocator<16>> IniPlatforms;
		for(const ITargetPlatform* TP : GetTargetPlatformManagerRef().GetTargetPlatforms())
		{
			check(TP);
			TArray<FName> PlatformShaderFormats;
			TP->GetAllPossibleShaderFormats(PlatformShaderFormats);
			if (PlatformShaderFormats.Contains(OutShaderFormat))
			{
				IniPlatforms.AddUnique(FName(TP->IniPlatformName()));
			}
		}
		
		if (IniPlatforms.Num() == 0)
		{
			// No found platforms that support this shader format
			return nullptr;
		}
		
		// first add all platforms to populate the map :
		for (int32 PlatformIndex = 0; PlatformIndex < IniPlatforms.Num(); ++PlatformIndex)
		{
			FPlatformInfo& Platform = ConfigDefines.FindOrAdd(IniPlatforms[PlatformIndex]);

			if (!Platform.bInitializedFromConfig)
			{
				InitializePlatform(Platform, IniPlatforms[PlatformIndex]);
				check( Platform.bInitializedFromConfig );
			}
		}

		if (!ErrorCheckedPlatforms[ShaderPlatform])
		{
			ErrorCheckedPlatforms[ShaderPlatform] = true;

			if (IniPlatforms.Num() > 1)
			{
				// This shader platform is shared by multiple target platforms that can be configured independently. We need to make sure all config defines
				// match up, and that no platform-specific defines exist that might introduce shader compiler output that diverges between target platforms
								
				// pointer to Platform0 is safe to hold now because all are added first :
				const FPlatformInfo * Platform0 = ConfigDefines.Find(IniPlatforms[0]);
				check( Platform0 != nullptr );

				for (int32 PlatformIndex = 1; PlatformIndex < IniPlatforms.Num(); ++PlatformIndex)
				{
					const FPlatformInfo * OtherPlatform = ConfigDefines.Find(IniPlatforms[PlatformIndex]);
					check( OtherPlatform != nullptr );

					ErrorCheckPlatformsForShaderFormat(*Platform0, *OtherPlatform, OutShaderFormat);
				}
			}
		}
		
		// reget the pointer to IniPlatforms[0] after all Map adds are done, to return out :
		// note returned pointer is not safe if any more adds are done
		const FPlatformInfo * Platform = ConfigDefines.Find(IniPlatforms[0]);

		return Platform;
	}

	static void InitializePlatform(FPlatformInfo& Platform, FName PlatformName)
	{
		if (FConfigCacheIni* ConfigCache = FConfigCacheIni::ForPlatform(PlatformName))
		{
			TArray<FString> DefineStrings;
			ConfigCache->GetArray(TEXT("GlobalShaderDefines"), TEXT("Definitions"), DefineStrings, GEngineIni);

			for (const FString& DefineString : DefineStrings)
			{				
				FString ShaderName, DefineName;
				if (FParse::Value(*DefineString, TEXT("Shader="), ShaderName) &&
					FParse::Value(*DefineString, TEXT("Define="), DefineName))
				{
					FName ShaderFormat = NAME_None;
					FParse::Value(*DefineString, TEXT("ShaderFormat="), ShaderFormat);

					int32 PermutationID = INDEX_NONE;
					FParse::Value(*DefineString, TEXT("PermutationID="), PermutationID);

					FString DefineValue;
					int32 EqualsIndex = INDEX_NONE;
					if (DefineName.FindChar(TCHAR('='), EqualsIndex))
					{
						DefineValue = DefineName.Mid(EqualsIndex + 1);
						DefineName.MidInline(0, EqualsIndex, EAllowShrinking::No);
					}

					const FShaderType* ShaderType = FindShaderTypeByName(ShaderName);
					if (ShaderType == nullptr || ShaderType->GetGlobalShaderType() == nullptr)
					{
						// This global shader doesn't actually exist
						UE_LOG(LogShaders, Warning, TEXT("Global shader definition '%s' found in engine config for global shader '%s', which does not exist"), *DefineName, *ShaderName);
						continue;
					}

					TArray<FDefine>& ShaderDefines = Platform.ShaderDefines.FindOrAdd(FName(ShaderName));
					FDefine* Define = ShaderDefines.FindByPredicate(
						[&](const FDefine& Define)
						{
							return Define.Name.Equals(DefineName, ESearchCase::IgnoreCase) &&
								Define.ShaderFormat == ShaderFormat &&
								Define.PermutationID == PermutationID;
						});

					if (!Define)
					{
						Define = &ShaderDefines.AddDefaulted_GetRef();
						Define->Name = DefineName;
						Define->ShaderFormat = ShaderFormat;
						Define->PermutationID = PermutationID;
					}

					Define->Value = DefineValue;
				}
			}			
		}		

		Platform.Name = PlatformName;
		Platform.bInitializedFromConfig = true;
	}

	static void ErrorCheckPlatformsForShaderFormat(const FPlatformInfo& PlatformA, const FPlatformInfo& PlatformB, FName ShaderFormat)
	{
		auto FilterShaderDefines = [ShaderFormat](const FShaderDefines& ShaderDefines) -> FShaderDefines
		{
			FShaderDefines FilteredShaderDefines;

			for (const auto& ShaderIt : ShaderDefines)
			{
				TArray<FDefine> FilteredDefines = ShaderIt.Value.FilterByPredicate(
					[ShaderFormat](const FDefine& Define)
					{
						return Define.ShaderFormat == NAME_None || Define.ShaderFormat == ShaderFormat;
					});

				if (FilteredDefines.Num() > 0)
				{
					// Sort the filtered defines for every shader for the purposes of comparison in error checking
					FilteredDefines.Sort();
					FilteredShaderDefines.Add(ShaderIt.Key, MoveTemp(FilteredDefines));
				}
			}

			return MoveTemp(FilteredShaderDefines);
		};

		FShaderDefines FilteredDefinesA = FilterShaderDefines(PlatformA.ShaderDefines);
		FShaderDefines FilteredDefinesB = FilterShaderDefines(PlatformB.ShaderDefines);

		TSet<FName> ShadersWithError;

		// Check if there are any keys in A that aren't in B, and vice versa
		{
			TSet<FName> ShaderNamesA, ShaderNamesB;
			FilteredDefinesA.GetKeys(ShaderNamesA);
			FilteredDefinesB.GetKeys(ShaderNamesB);

			ShadersWithError.Append(ShaderNamesA.Difference(ShaderNamesB));
			ShadersWithError.Append(ShaderNamesB.Difference(ShaderNamesA));
		}

		// Check if the defines (that are relevant to this shader format) are different between A or B
		for (const auto& ShaderIt : FilteredDefinesA)
		{
			const TArray<FDefine>& DefinesA = ShaderIt.Value;
			const TArray<FDefine>* DefinesB = FilteredDefinesB.Find(ShaderIt.Key);

			if (DefinesB && DefinesA != *DefinesB)
			{
				ShadersWithError.Add(ShaderIt.Key);
			}
		}

		if (ShadersWithError.Num() > 0)
		{
			FString ShaderListString = *FString::JoinBy(ShadersWithError, TEXT("\n    "), [](const FName& A) { return A.ToString(); });
			UE_LOG(LogShaders, Fatal, TEXT("It has been detected that one or more global shaders are configured differently between platform '%s' and '%s' for ")
				TEXT("shader format '%s'. This is unsupported, and could result in data mismatches in the Derived Data Cache. Please check that all entries ")
				TEXT("of '[GlobalShaderDefines]` in the various engine config .ini files are the same for these shaders on all platforms that use this shader ")
				TEXT("model.\n\nGlobal shaders with errors:\n    %s\n"),
				*PlatformA.Name.ToString(), *PlatformB.Name.ToString(), *ShaderFormat.ToString(), *ShaderListString);
		}
	}
};

TMap<FName, FGlobalShaderConfigDefines::FPlatformInfo> FGlobalShaderConfigDefines::ConfigDefines;
TStaticBitArray<EShaderPlatform::SP_NumPlatforms> FGlobalShaderConfigDefines::ErrorCheckedPlatforms;

/** Used to identify the global shader map in compile queues. */
const int32 GlobalShaderMapId = 0;

FGlobalShaderMapId::FGlobalShaderMapId(EShaderPlatform Platform, const ITargetPlatform* TargetPlatform)
{
	if (TargetPlatform == nullptr)
	{
		TargetPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
		check(TargetPlatform);
	}

	ShaderPlatform = Platform;
	IniPlatformName = FName(TargetPlatform->IniPlatformName());

	LayoutParams.InitializeForPlatform(TargetPlatform);
	const EShaderPermutationFlags PermutationFlags = GetShaderPermutationFlags(LayoutParams);
	TArray<FShaderType*> ShaderTypes;
	TArray<const FShaderPipelineType*> ShaderPipelineTypes;

	for (TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList()); ShaderTypeIt; ShaderTypeIt.Next())
	{
		FGlobalShaderType* GlobalShaderType = ShaderTypeIt->GetGlobalShaderType();
		if (!GlobalShaderType)
		{
			continue;
		}

		bool bList = false;
		for (int32 PermutationId = 0; PermutationId < GlobalShaderType->GetPermutationCount(); PermutationId++)
		{
			if (GlobalShaderType->ShouldCompilePermutation(Platform, PermutationId, PermutationFlags))
			{
				bList = true;
				break;
			}
		}

		if (bList)
		{
			ShaderTypes.Add(GlobalShaderType);
		}
	}

	for (TLinkedList<FShaderPipelineType*>::TIterator ShaderPipelineIt(FShaderPipelineType::GetTypeList()); ShaderPipelineIt; ShaderPipelineIt.Next())
	{
		const FShaderPipelineType* Pipeline = *ShaderPipelineIt;
		if (Pipeline->IsGlobalTypePipeline())
		{
			int32 NumStagesNeeded = 0;
			auto& StageTypes = Pipeline->GetStages();
			for (const FShaderType* Shader : StageTypes)
			{
				const FGlobalShaderType* GlobalShaderType = Shader->GetGlobalShaderType();
				if (GlobalShaderType->ShouldCompilePermutation(Platform, /* PermutationId = */ 0, PermutationFlags))
				{
					++NumStagesNeeded;
				}
				else
				{
					break;
				}
			}

			if (NumStagesNeeded == StageTypes.Num())
			{
				ShaderPipelineTypes.Add(Pipeline);
			}
		}
	}

	// Individual shader dependencies
	ShaderTypes.Sort(FCompareShaderTypes());
	for (int32 TypeIndex = 0; TypeIndex < ShaderTypes.Num(); TypeIndex++)
	{
		FShaderType* ShaderType = ShaderTypes[TypeIndex];
		FShaderTypeDependency Dependency(ShaderType, Platform);

		const TCHAR* ShaderFilename = ShaderType->GetShaderFilename();

		TArray<FShaderTypeDependency>& Dependencies = ShaderFilenameToDependenciesMap.FindOrAdd(ShaderFilename);

		Dependencies.Add(Dependency);
	}

	// Shader pipeline dependencies
	ShaderPipelineTypes.Sort(FCompareShaderPipelineNameTypes());
	for (int32 TypeIndex = 0; TypeIndex < ShaderPipelineTypes.Num(); TypeIndex++)
	{
		const FShaderPipelineType* Pipeline = ShaderPipelineTypes[TypeIndex];
		FShaderPipelineTypeDependency Dependency(Pipeline, Platform);
		ShaderPipelineTypeDependencies.Add(Dependency);
	}
}

void FGlobalShaderMapId::AppendKeyString(FString& KeyString, const TArray<FShaderTypeDependency>& Dependencies) const
{
#if WITH_EDITOR

	LayoutParams.AppendKeyString(KeyString);

	{
		const FSHAHash LayoutHash = GetShaderTypeLayoutHash(StaticGetTypeLayoutDesc<FGlobalShaderMapContent>(), LayoutParams);
		KeyString.AppendChar('_');
		LayoutHash.AppendString(KeyString);
		KeyString.AppendChar('_');
	}

	AppendKeyStringShaderDependencies(Dependencies, ShaderPipelineTypeDependencies, TConstArrayView<FVertexFactoryTypeDependency>(), LayoutParams, KeyString);

	// Extra logic, just for Global shaders
	for (const FShaderTypeDependency& ShaderTypeDependency : Dependencies)
	{
		const FShaderType* ShaderType = FindShaderTypeByName(ShaderTypeDependency.ShaderTypeName);
		// Add the config define hash, if any defines exist in config
		FGlobalShaderConfigDefines::AppendKeyString(KeyString, ShaderType->GetFName(), ShaderTypeDependency.PermutationId, IniPlatformName, ShaderPlatform);
	}

#endif // WITH_EDITOR
}

bool FGlobalShaderMapId::WithEditorOnly() const
{
	return LayoutParams.WithEditorOnly();
}

bool FGlobalShaderType::ShouldCompilePipeline(const FShaderPipelineType* ShaderPipelineType, EShaderPlatform Platform, EShaderPermutationFlags Flags)
{
	for (const FShaderType* ShaderType : ShaderPipelineType->GetStages())
	{
		const FGlobalShaderPermutationParameters Parameters(ShaderType->GetFName(), Platform, kUniqueShaderPermutationId, Flags);
		checkSlow(ShaderType->GetGlobalShaderType());
		if (!ShaderType->ShouldCompilePermutation(Parameters))
		{
			return false;
		}
	}
	return true;
}

FGlobalShader::FGlobalShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
:	FShader(Initializer)
{}

#if WITH_EDITOR
void FGlobalShaderType::SetupCompileEnvironment(EShaderPlatform Platform, int32 PermutationId, EShaderPermutationFlags Flags, FShaderCompilerEnvironment& Environment) const
{
	FGlobalShaderPermutationParameters Parameters(GetFName(), Platform, PermutationId, Flags);

	// Modify the compilation environment based on platform.
	FGlobalShaderConfigDefines::ApplyConfigDefines(Parameters, Environment);

	// Allow the shader type to modify its compile environment.
	ModifyCompilationEnvironment(Parameters, Environment);
}
#endif // WITH_EDITOR

void BackupGlobalShaderMap(FGlobalShaderBackupData& OutGlobalShaderBackup)
{
#if 0
	for (int32 i = (int32)ERHIFeatureLevel::ES2_REMOVED; i < (int32)ERHIFeatureLevel::Num; ++i)
	{
		EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform((ERHIFeatureLevel::Type)i);
		if (ShaderPlatform < EShaderPlatform::SP_NumPlatforms && GGlobalShaderMap[ShaderPlatform] != nullptr)
		{
			TUniquePtr<TArray<uint8>> ShaderData = MakeUnique<TArray<uint8>>();
			FMemoryWriter Ar(*ShaderData);
			GGlobalShaderMap[ShaderPlatform]->SerializeInline(Ar, true, true, false, nullptr);
			//GGlobalShaderMap[ShaderPlatform]->RegisterSerializedShaders(false);
			GGlobalShaderMap[ShaderPlatform]->Empty();
			OutGlobalShaderBackup.FeatureLevelShaderData[i] = MoveTemp(ShaderData);
		}
	}

	// Remove cached references to global shaders
	for (TLinkedList<FGlobalBoundShaderStateResource*>::TIterator It(FGlobalBoundShaderStateResource::GetGlobalBoundShaderStateList()); It; It.Next())
	{
		BeginUpdateResourceRHI(*It);
	}
#endif
	check(0);
}

void RestoreGlobalShaderMap(const FGlobalShaderBackupData& GlobalShaderBackup)
{
#if 0
	for (int32 i = (int32)ERHIFeatureLevel::ES2_REMOVED; i < (int32)ERHIFeatureLevel::Num; ++i)
	{
		EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform((ERHIFeatureLevel::Type)i);		
		if (GlobalShaderBackup.FeatureLevelShaderData[i] != nullptr
			&& ShaderPlatform < EShaderPlatform::SP_NumPlatforms
			&& GGlobalShaderMap[ShaderPlatform] != nullptr)
		{
			FMemoryReader Ar(*GlobalShaderBackup.FeatureLevelShaderData[i]);
			GGlobalShaderMap[ShaderPlatform]->SerializeInline(Ar, true, true, false, nullptr);
			//GGlobalShaderMap[ShaderPlatform]->RegisterSerializedShaders(false);
		}
	}
#endif
	check(0);
}


FGlobalShaderMap* GetGlobalShaderMap(EShaderPlatform Platform)
{
	// If the global shader map hasn't been created yet
	check(GGlobalShaderMap[Platform]);
	return GGlobalShaderMap[Platform];
}

FGlobalShaderMapSection* FGlobalShaderMapSection::CreateFromArchive(FArchive& Ar)
{
	FGlobalShaderMapSection* Section = new FGlobalShaderMapSection();
	if (Section->Serialize(Ar))
	{
		return Section;
	}
	delete Section;
	return nullptr;
}

bool FGlobalShaderMapSection::Serialize(FArchive& Ar)
{
	return Super::Serialize(Ar, true, false);
}

TShaderRef<FShader> FGlobalShaderMapSection::GetShader(FShaderType* ShaderType, int32 PermutationId) const
{
	FShader* Shader = GetContent()->GetShader(ShaderType, PermutationId);
	return Shader ? TShaderRef<FShader>(Shader, *this) : TShaderRef<FShader>();
}

FShaderPipelineRef FGlobalShaderMapSection::GetShaderPipeline(const FShaderPipelineType* PipelineType) const
{
	FShaderPipeline* Pipeline = GetContent()->GetShaderPipeline(PipelineType);
	return Pipeline ? FShaderPipelineRef(Pipeline, *this) : FShaderPipelineRef();
}

FGlobalShaderMap::FGlobalShaderMap(EShaderPlatform InPlatform)
	: Platform(InPlatform)
{
}

FGlobalShaderMap::~FGlobalShaderMap()
{
	ReleaseAllSections();
}

TShaderRef<FShader> FGlobalShaderMap::GetShader(FShaderType* ShaderType, int32 PermutationId) const
{
	FGlobalShaderMapSection* const* Section = SectionMap.Find(ShaderType->GetHashedShaderFilename());
	return Section ? (*Section)->GetShader(ShaderType, PermutationId) : TShaderRef<FShader>();
}

FShaderPipelineRef FGlobalShaderMap::GetShaderPipeline(const FShaderPipelineType* ShaderPipelineType) const
{
	FGlobalShaderMapSection* const* Section = SectionMap.Find(ShaderPipelineType->GetHashedPrimaryShaderFilename());
	return Section ? (*Section)->GetShaderPipeline(ShaderPipelineType) : FShaderPipelineRef();
}

void FGlobalShaderMap::BeginCreateAllShaders()
{
	for (const auto& It : SectionMap)
	{
		It.Value->GetResource()->BeginCreateAllShaders();
	}
}

#if WITH_EDITOR
void FGlobalShaderMap::GetOutdatedTypes(TArray<const FShaderType*>& OutdatedShaderTypes, TArray<const FShaderPipelineType*>& OutdatedShaderPipelineTypes, TArray<const FVertexFactoryType*>& OutdatedFactoryTypes) const
{
	for (const auto& It : SectionMap)
	{
		It.Value->GetOutdatedTypes(OutdatedShaderTypes, OutdatedShaderPipelineTypes, OutdatedFactoryTypes);
	}
}

void FGlobalShaderMap::SaveShaderStableKeys(EShaderPlatform TargetShaderPlatform)
{
	FStableShaderKeyAndValue SaveKeyVal;
	for (const auto& It : SectionMap)
	{
		It.Value->SaveShaderStableKeys(TargetShaderPlatform, SaveKeyVal);
	}
}

#endif // WITH_EDITOR

bool FGlobalShaderMap::IsEmpty() const
{
	for (const auto& It : SectionMap)
	{
		if (!It.Value->GetContent()->IsEmpty())
		{
			return false;
		}
	}
	return true;
}

bool FGlobalShaderMap::IsComplete(const ITargetPlatform* TargetPlatform) const
{
	// TODO: store these in the shadermap before it's start to be compiled?
	FPlatformTypeLayoutParameters LayoutParams;
	LayoutParams.InitializeForPlatform(TargetPlatform);
	const EShaderPermutationFlags PermutationFlags = GetShaderPermutationFlags(LayoutParams);

	FGlobalShaderMapId ShaderMapId(Platform, TargetPlatform);

	// traverse all global shader types
	for (TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList()); ShaderTypeIt; ShaderTypeIt.Next())
	{
		FGlobalShaderType* GlobalShaderType = ShaderTypeIt->GetGlobalShaderType();
		if (!GlobalShaderType)
		{
			continue;
		}

		int32 PermutationCountToCompile = 0;
		for (int32 PermutationId = 0; PermutationId < GlobalShaderType->GetPermutationCount(); PermutationId++)
		{
			if (GlobalShaderType->ShouldCompilePermutation(Platform, PermutationId, PermutationFlags)
				&& !HasShader(GlobalShaderType, PermutationId))
			{
				return false;
			}
		}
	}

	// traverse all pipelines. Note that there's no ShouldCompile call for them. Materials instead test individual stages, but it leads to another problems
	// like including the standalone types even if they are not going to be used. This code follows VerifyGlobalShaders() logic that includes all global pipelines unconditionally.
	for (TLinkedList<FShaderPipelineType*>::TIterator ShaderPipelineIt(FShaderPipelineType::GetTypeList()); ShaderPipelineIt; ShaderPipelineIt.Next())
	{
		const FShaderPipelineType* Pipeline = *ShaderPipelineIt;
		if (Pipeline->IsGlobalTypePipeline()
			&& FGlobalShaderType::ShouldCompilePipeline(Pipeline, Platform, PermutationFlags)
			&& !HasShaderPipeline(Pipeline))
		{
			return false;
		}
	}

	return true;
}

void FGlobalShaderMap::Empty()
{
	for (const auto& It : SectionMap)
	{
		It.Value->GetMutableContent()->Empty();
	}
}

void FGlobalShaderMap::ReleaseAllSections()
{
	for (auto& It : SectionMap)
	{
		delete It.Value;
	}
	SectionMap.Empty();
}

FShader* FGlobalShaderMap::FindOrAddShader(const FShaderType* ShaderType, int32 PermutationId, FShader* Shader)
{
	const FHashedName HashedFilename(ShaderType->GetHashedShaderFilename());
	FGlobalShaderMapSection*& Section = SectionMap.FindOrAdd(HashedFilename);
	if (!Section)
	{
		Section = new FGlobalShaderMapSection(Platform, HashedFilename);
	}
	return Section->GetMutableContent()->FindOrAddShader(ShaderType->GetHashedName(), PermutationId, Shader);
}

FShaderPipeline* FGlobalShaderMap::FindOrAddShaderPipeline(const FShaderPipelineType* ShaderPipelineType, FShaderPipeline* ShaderPipeline)
{
	FGlobalShaderMapSection*& Section = SectionMap.FindOrAdd(ShaderPipelineType->GetHashedPrimaryShaderFilename());
	if (!Section)
	{
		Section = new FGlobalShaderMapSection(Platform, ShaderPipelineType->GetHashedPrimaryShaderFilename());
	}
	return Section->GetMutableContent()->FindOrAddShaderPipeline(ShaderPipeline);
}

void FGlobalShaderMap::RemoveShaderTypePermutaion(const FShaderType* Type, int32 PermutationId)
{
	FGlobalShaderMapSection** Section = SectionMap.Find(Type->GetHashedShaderFilename());
	if (Section)
	{
		(*Section)->GetMutableContent()->RemoveShaderTypePermutaion(Type->GetHashedName(), PermutationId);
	}
}

void FGlobalShaderMap::RemoveShaderPipelineType(const FShaderPipelineType* ShaderPipelineType)
{
	FGlobalShaderMapSection** Section = SectionMap.Find(ShaderPipelineType->GetHashedPrimaryShaderFilename());
	if (Section)
	{
		(*Section)->GetMutableContent()->RemoveShaderPipelineType(ShaderPipelineType);
	}
}

void FGlobalShaderMap::AddSection(FGlobalShaderMapSection* InSection)
{
	check(InSection);
	const FGlobalShaderMapContent* Content = InSection->GetContent();
	const FHashedName& HashedFilename = Content->HashedSourceFilename;

	SectionMap.Add(HashedFilename, InSection);
}

FGlobalShaderMapSection* FGlobalShaderMap::FindSection(const FHashedName& HashedShaderFilename)
{
	FGlobalShaderMapSection* const* Section = SectionMap.Find(HashedShaderFilename);
	return Section ? *Section : nullptr;
}

FGlobalShaderMapSection* FGlobalShaderMap::FindOrAddSection(const FShaderType* ShaderType)
{
	const FHashedName HashedFilename(ShaderType->GetHashedShaderFilename());
	FGlobalShaderMapSection* Section = FindSection(HashedFilename);
	if(!Section)
	{
		Section = new FGlobalShaderMapSection(Platform, HashedFilename);
		AddSection(Section);
	}
	return Section;
}

void FGlobalShaderMap::SaveToGlobalArchive(FArchive& Ar)
{
	int32 NumSections = SectionMap.Num();
	Ar << NumSections;

	for (const auto& It : SectionMap)
	{
		It.Value->Serialize(Ar);
	}
}

void FGlobalShaderMap::LoadFromGlobalArchive(FArchive& Ar)
{
	int32 NumSections = 0;
	Ar << NumSections;

	for (int32 i = 0; i < NumSections; ++i)
	{
		FGlobalShaderMapSection* Section = FGlobalShaderMapSection::CreateFromArchive(Ar);
		if (Section)
		{
			AddSection(Section);
		}
		else
		{
			UE_LOG(LogShaders, Fatal, TEXT("Could not load section %d (of %d) of the global shadermap."), i, NumSections);
		}
	}
}

RENDERCORE_API ERecursiveShader GRequiredRecursiveShaders = ERecursiveShader::None;

void ForceInitGlobalShaderType(FShaderType& ShaderType)
{
	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	for (int32 Permutation = 0; Permutation < ShaderType.GetPermutationCount(); ++Permutation)
	{
		TShaderRef<FShader> ShaderRef = GlobalShaderMap->GetShader(&ShaderType, Permutation);

		if (ShaderRef.IsValid())
		{
			FShaderMapResource& MapResource = ShaderRef.GetResourceChecked();
			for (int32 Index = 0; Index < MapResource.GetNumShaders(); ++Index)
			{
				MapResource.GetShader(Index);
			}
		}
	}
}

RENDERCORE_API void CreateRecursiveShaders()
{
	ensureMsgf(!GRHISupportsMultithreadedShaderCreation, TEXT("CreateRecursiveShaders() is called while GRHISupportsMultithreadedShaderCreation is true. This is an unnecessary call."));
	ensureMsgf(IsInRenderingThread(), TEXT("CreateRecursiveShaders() is expected to be called from the render thread."));

	if (EnumHasAnyFlags(GRequiredRecursiveShaders, ERecursiveShader::Resolve))
	{
		extern void CreateResolveShaders();
		CreateResolveShaders();
	}

	if (EnumHasAnyFlags(GRequiredRecursiveShaders, ERecursiveShader::Clear))
	{
		extern void CreateClearReplacementShaders();
		CreateClearReplacementShaders();
	}

	if (EnumHasAnyFlags(GRequiredRecursiveShaders, ERecursiveShader::Null))
	{
		ForceInitGlobalShaderType<FNULLPS>();
	}
}
