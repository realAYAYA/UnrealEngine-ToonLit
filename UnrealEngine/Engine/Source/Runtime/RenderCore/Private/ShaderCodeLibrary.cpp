// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
ShaderCodeLibrary.cpp: Bound shader state cache implementation.
=============================================================================*/

#include "ShaderCodeLibrary.h"

#include "Algo/Replace.h"
#include "Async/ParallelFor.h"
#include "Containers/HashTable.h"
#include "Containers/Set.h"
#include "Containers/StringView.h"
#include "FileCache/FileCache.h"
#include "HAL/FileManager.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformSplash.h" // IWYU pragma: keep
#include "Hash/CityHash.h"
#include "Interfaces/IPluginManager.h"
#include "Internationalization/Regex.h"
#include "Math/UnitConversion.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/SecureHash.h"
#include "Misc/StringBuilder.h"
#include "PipelineFileCache.h"
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "RenderingThread.h"
#include "Shader.h"
#include "ShaderCodeArchive.h"
#include "ShaderPipelineCache.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "String/ParseTokens.h"
#include "IO/IoChunkId.h"
#include "IO/IoDispatcher.h"

#if WITH_EDITORONLY_DATA
#include "Interfaces/IShaderFormat.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#endif

#if WITH_EDITOR
#include "PipelineCacheUtilities.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "Serialization/CompactBinarySerialization.h"
#include "Serialization/CompactBinaryWriter.h"
#include "RHIStrings.h"
#endif

// allow introspection (e.g. dumping the contents) for easier debugging
#define UE_SHADERLIB_WITH_INTROSPECTION			!UE_BUILD_SHIPPING

// Enabled by default for all configurations, runtime check should prevent chuck discovery in non dev builds
#define UE_SHADERLIB_SUPPORT_CHUNK_DISCOVERY	(1)

DEFINE_LOG_CATEGORY(LogShaderLibrary);

static uint32 GShaderCodeArchiveVersion = 2;
static uint32 GShaderPipelineArchiveVersion = 1;

static FString ShaderExtension = TEXT(".ushaderbytecode");
static FString ShaderAssetInfoExtension = TEXT(".assetinfo.json");
static FString StableExtension = TEXT(".shk");
static FString PipelineExtension = TEXT(".ushaderpipelines");

static FString ShaderCodePatternStr = TEXT("^ShaderCode\\-(\\w*)\\-([\\w\\-]*)\\.ushaderbytecode$");
static FString ShaderArchivePatternStr = TEXT("^ShaderArchive\\-(\\w*)\\-([\\w\\-]*)\\.ushaderbytecode$");
static FString StableInfoPatternStr = TEXT("^ShaderStableInfo\\-(\\w*)\\-([\\w\\-]*)\\.shk$");

int32 GShaderCodeLibrarySeparateLoadingCache = 0;
static FAutoConsoleVariableRef CVarShaderCodeLibrarySeparateLoadingCache(
	TEXT("r.ShaderCodeLibrary.SeparateLoadingCache"),
	GShaderCodeLibrarySeparateLoadingCache,
	TEXT("if > 0, each shader code library has it's own loading cache."),
	ECVF_Default
);

class FShaderLibraryInstance;
namespace UE
{
	namespace ShaderLibrary
	{
		namespace Private
		{
			int32 GProduceExtendedStats = 1;
			static FAutoConsoleVariableRef CVarShaderLibraryProduceExtendedStats(
				TEXT("r.ShaderLibrary.PrintExtendedStats"),
				GProduceExtendedStats,
				TEXT("if != 0, shader library will produce extended stats, including the textual representation"),
				ECVF_Default
			);

			static FDelegateHandle OnPakFileMountedDelegateHandle;
			static FDelegateHandle OnPluginMountedDelegateHandle;
			static FDelegateHandle OnPluginUnmountedDelegateHandle;

			static TSet<FString> PluginsToIgnoreOnMount;

			/** Helper function shared between the cooker and runtime */
			FString GetShaderLibraryNameForChunk(FString const& BaseName, int32 ChunkId)
			{
				if (ChunkId == INDEX_NONE)
				{
					return BaseName;
				}
				return FString::Printf(TEXT("%s_Chunk%d"), *BaseName, ChunkId);
			}

			bool IsRunningWithPakFile()
			{
				static const bool bRunningWithPakFile =
					(FPlatformFileManager::Get().FindPlatformFile(TEXT("PakFile")) != nullptr);
				return bRunningWithPakFile;
			}
			bool IsRunningWithIoStore()
			{
				static const bool bRunningWithIoStore =
					FIoDispatcher::IsInitialized()
					&& FIoDispatcher::Get().DoesChunkExist(CreateIoChunkId(0, 0, EIoChunkType::ScriptObjects));
				return bRunningWithIoStore;
			}
			bool IsRunningWithZenStore()
			{
				static const bool bRunningWithZenStore =
					FPlatformFileManager::Get().FindPlatformFile(TEXT("StorageServer")) != nullptr;
				return bRunningWithZenStore;
			}
			bool ShouldLookForLooseCookedChunks()
			{
				return IsRunningWithZenStore() || !IsRunningWithIoStore();
			}

			// [RCL] TODO 2020-11-20: Separate runtime and editor-only code (tracked as UE-103486)
			/** Descriptor used to pass the pak file information to the library as we cannot store IPakFile ref */
			struct FMountedPakFileInfo
			{
				/** Pak filename (external OS filename) */
				FString PakFilename;
				/** In-game path for the pak content */
				FString MountPoint;
				/** Chunk ID */
				int32 ChunkId;

				// this constructor is used for chunks that we have not yet possibly seen
				FMountedPakFileInfo(int32 InChunkId)
					: PakFilename(TEXT("Fake")),
					ChunkId(InChunkId)
				{
				}

#if UE_SHADERLIB_SUPPORT_CHUNK_DISCOVERY
				FMountedPakFileInfo(const FString& InMountPoint, int32 InChunkId)
					: PakFilename(TEXT("Fake")),
					MountPoint(InMountPoint),
					ChunkId(InChunkId)
				{
				}
#endif // UE_SHADERLIB_SUPPORT_CHUNK_DISCOVERY

				FMountedPakFileInfo(const IPakFile& PakFile)
					: PakFilename(PakFile.PakGetPakFilename()),
					MountPoint(PakFile.PakGetMountPoint()),
					ChunkId(PakFile.PakGetPakchunkIndex())
				{}

				FString ToString() const
				{
					return FString::Printf(TEXT("ChunkID:%d Root:%s File:%s"), ChunkId, *MountPoint, *PakFilename);
				}

				friend uint32 GetTypeHash(const FMountedPakFileInfo& InData)
				{
					return HashCombine(HashCombine(GetTypeHash(InData.PakFilename), GetTypeHash(InData.MountPoint)), ::GetTypeHash(InData.ChunkId));
				}

				friend bool operator==(const FMountedPakFileInfo& A, const FMountedPakFileInfo& B)
				{
					return A.PakFilename == B.PakFilename && A.MountPoint == B.MountPoint && A.ChunkId == B.ChunkId;
				}

				/** Holds a set of all known paks that can be added very early. Each library on Open will traverse that list. */
				static TSet<FMountedPakFileInfo> KnownPakFiles;

				/** Guards access to the list of known pak files*/
				static FCriticalSection KnownPakFilesAccessLock;
			};

			// At runtime, a descriptor of a named library
			struct FNamedShaderLibrary
			{
				/** A name that is passed to Open/CloseLibrary, like "Global", "ShooterGame", "MyPlugin" */
				FString	LogicalName;

				/** Shader platform */
				EShaderPlatform ShaderPlatform;

				/** Base directory for chunk 0 */
				FString BaseDirectory;

				/** Parts of the library corresponding to particular chunk Ids that we have found for this library.
				 *  This is used so we don't try to open a library for the chunk more than once
				 */
				TSet<int32> PresentChunks;

				/** Guards access to components*/
				FRWLock ComponentsMutex;

				/** Even putting aside chunking, each named library can be potentially comprised of multiple files */
				TArray<TUniquePtr<FShaderLibraryInstance>> Components;

				FNamedShaderLibrary(const FString& InLogicalName, const EShaderPlatform InShaderPlatform, const FString& InBaseDirectory)
					: LogicalName(InLogicalName)
					, ShaderPlatform(InShaderPlatform)
					, BaseDirectory(InBaseDirectory)
				{
				}

				int32 GetNumComponents() const
				{
					return Components.Num();
				}

				void OnPakFileMounted(const FMountedPakFileInfo& MountInfo)
				{
					if (!PresentChunks.Contains(MountInfo.ChunkId))
					{
						FString ChunkLibraryName = GetShaderLibraryNameForChunk(LogicalName, MountInfo.ChunkId);

						// Ignore chunk mount point as it's useless in locating the actual library directory. For instance, chunks can
						// have mount points like ../../../ProjectName, while the actual library file is still stored in Content subdirectory.
						// Just use the base directory always and expect the library to be placed in the same location for all chunks
						// (which is the current behavior).
						if (OpenShaderCode(BaseDirectory, ChunkLibraryName))
						{
							PresentChunks.Add(MountInfo.ChunkId);
						}
					}
				}

				bool OpenShaderCode(const FString& ShaderCodeDir, FString const& Library);
				FShaderLibraryInstance* FindShaderLibraryForShaderMap(const FSHAHash& Hash, int32& OutShaderMapIndex);
				FShaderLibraryInstance* FindShaderLibraryForShader(const FSHAHash& Hash, int32& OutShaderIndex);
				uint32 GetShaderCount();
#if UE_SHADERLIB_WITH_INTROSPECTION
				void DumpLibraryContents(const FString& Prefix);
#endif
			};
		}
	}
}

TSet<UE::ShaderLibrary::Private::FMountedPakFileInfo> UE::ShaderLibrary::Private::FMountedPakFileInfo::KnownPakFiles;
FCriticalSection UE::ShaderLibrary::Private::FMountedPakFileInfo::KnownPakFilesAccessLock;

FSharedShaderMapResourceExplicitRelease OnSharedShaderMapResourceExplicitRelease;

class FShaderMapResource_SharedCode final : public FShaderMapResource
{
public:
	FShaderMapResource_SharedCode(class FShaderLibraryInstance* InLibraryInstance, int32 InShaderMapIndex);
	virtual ~FShaderMapResource_SharedCode();

	// FRenderResource interface.
	virtual void ReleaseResource() override
	{
		FShaderMapResource::ReleaseResource();
		ensureMsgf(!bShaderMapPreloaded && !LibraryInstance, TEXT("FShaderMapResource_SharedCode::ReleaseRHI() was not called on a shadermap resource owned by %s"), *GetOwnerName().ToString());
	}
	virtual void ReleaseRHI() override;

	// FShaderMapResource interface
	virtual FSHAHash GetShaderHash(int32 ShaderIndex) override;
	virtual FRHIShader* CreateRHIShaderOrCrash(int32 ShaderIndex) override;
	virtual void ReleasePreloadedShaderCode(int32 ShaderIndex) override;
	virtual bool TryRelease() override;
	virtual uint32 GetSizeBytes() const override { return sizeof(*this) + GetAllocatedSize(); }

	class FShaderLibraryInstance* LibraryInstance;
	int32 ShaderMapIndex;
	bool bShaderMapPreloaded;
};

static FString GetCodeArchiveFilename(const FString& BaseDir, const FString& LibraryName, FName Platform)
{
	return BaseDir / FString::Printf(TEXT("ShaderArchive-%s-"), *LibraryName) + Platform.ToString() + ShaderExtension;
}

static FString GetStableInfoArchiveFilename(const FString& BaseDir, const FString& LibraryName, FName Platform)
{
	return BaseDir / FString::Printf(TEXT("ShaderStableInfo-%s-"), *LibraryName) + Platform.ToString() + StableExtension;
}

static FString GetPipelinesArchiveFilename(const FString& BaseDir, const FString& LibraryName, FName Platform)
{
	return BaseDir / FString::Printf(TEXT("ShaderArchive-%s-"), *LibraryName) + Platform.ToString() + PipelineExtension;
}

static FString GetShaderCodeFilename(const FString& BaseDir, const FString& LibraryName, FName Platform)
{
	return BaseDir / FString::Printf(TEXT("ShaderCode-%s-"), *LibraryName) + Platform.ToString() + ShaderExtension;
}

static FString GetShaderAssetInfoFilename(const FString& BaseDir, const FString& LibraryName, FName Platform)
{
	return BaseDir / FString::Printf(TEXT("ShaderAssetInfo-%s-"), *LibraryName) + Platform.ToString() + ShaderAssetInfoExtension;
}

static FString GetShaderDebugFolder(const FString& BaseDir, const FString& LibraryName, FName Platform)
{
	return BaseDir / FString::Printf(TEXT("ShaderDebug-%s-"), *LibraryName) + Platform.ToString();
}

FORCEINLINE FName ParseFNameCached(const FStringView& Src, TMap<uint32,FName>& NameCache)
{
	uint32 SrcHash = CityHash32(reinterpret_cast<const char*>(Src.GetData()), Src.Len() * sizeof(TCHAR));
	if (FName* Name = NameCache.Find(SrcHash))
	{
		return *Name;
	}
	else
	{
		return NameCache.Emplace(SrcHash, FName(Src.Len(), Src.GetData()));
	}
}

static void AppendFNameAsUTF8(FAnsiStringBuilderBase& Out, const FName& InName)
{
	if (!InName.TryAppendAnsiString(Out))
	{
		TStringBuilder<128> WideName;
		InName.AppendString(WideName);
		Out << TCHAR_TO_UTF8(WideName.ToString());
	}
}

static void AppendSanitizedFNameAsUTF8(FAnsiStringBuilderBase& Out, const FName& InName, ANSICHAR Delim)
{
	const int32 Offset = Out.Len();
	AppendFNameAsUTF8(Out, InName);
	Algo::Replace(MakeArrayView(Out).Slice(Offset, Out.Len() - Offset), Delim, ' ');
}

static void AppendSanitizedFName(FStringBuilderBase& Out, const FName& InName, TCHAR Delim)
{
	const int32 Offset = Out.Len();
	InName.AppendString(Out);
	Algo::Replace(MakeArrayView(Out).Slice(Offset, Out.Len() - Offset), Delim, TEXT(' '));
}

FString FCompactFullName::ToString() const
{
	TStringBuilder<256> RetString;
	AppendString(RetString);
	return FString(FStringView(RetString));
}

FString FCompactFullName::ToStringPathOnly() const
{
	const int32 ObjectClassAndPathCount = ObjectClassAndPath.Num();
	if (!ObjectClassAndPathCount)
	{
		return FString();
	}

	TStringBuilder<256> RetString;
	// skip the first element which is class name, and the last, which is the object name itself
	for (int32 NameIdx = 1; NameIdx < ObjectClassAndPathCount - 1; NameIdx++)
	{
		RetString << ObjectClassAndPath[NameIdx];
		if (NameIdx < ObjectClassAndPathCount - 2)
		{
			RetString << TEXT('/');
		}
	}
	return FString(FStringView(RetString));
}

void FCompactFullName::AppendString(FStringBuilderBase& Out) const
{
	const int32 ObjectClassAndPathCount = ObjectClassAndPath.Num();
	if (!ObjectClassAndPathCount)
	{
		Out << TEXT("empty");
	}
	else
	{
		for (int32 NameIdx = 0; NameIdx < ObjectClassAndPathCount; NameIdx++)
		{
			Out << ObjectClassAndPath[NameIdx];
			if (NameIdx == 0)
			{
				Out << TEXT(' ');
			}
			else if (NameIdx < ObjectClassAndPathCount - 1)
			{
				if (NameIdx == ObjectClassAndPathCount - 2)
				{
					Out << TEXT('.');
				}
				else
				{
					Out << TEXT('/');
				}
			}
		}
	}
}

void FCompactFullName::AppendString(FAnsiStringBuilderBase& Out) const
{
	const int32 ObjectClassAndPathCount = ObjectClassAndPath.Num();
	if (!ObjectClassAndPathCount)
	{
		Out << "empty";
	}
	else
	{
		for (int32 NameIdx = 0; NameIdx < ObjectClassAndPathCount; NameIdx++)
		{
			AppendFNameAsUTF8(Out, ObjectClassAndPath[NameIdx]);
			if (NameIdx == 0)
			{
				Out << ' ';
			}
			else if (NameIdx < ObjectClassAndPathCount - 1)
			{
				if (NameIdx == ObjectClassAndPathCount - 2)
				{
					Out << '.';
				}
				else
				{
					Out << '/';
				}
			}
		}
	}
}

void FCompactFullName::ParseFromString(const FStringView& InSrc)
{
	TArray<FStringView, TInlineAllocator<64>> Fields;
	// do not split by '/' as this splits the original FName into per-path components
	UE::String::ParseTokensMultiple(InSrc.TrimStartAndEnd(), {TEXT(' '), TEXT('.'), /*TEXT('/'),*/ TEXT('\t')},
		[&Fields](FStringView Field) { if (!Field.IsEmpty()) { Fields.Add(Field); } });
	if (Fields.Num() == 1 && Fields[0] == TEXTVIEW("empty"))
	{
		ObjectClassAndPath.Empty();
	}
	// fix up old format that removed the leading '/'
	else if (Fields.Num() == 3 && Fields[1][0] != TEXT('/'))
	{
		ObjectClassAndPath.Empty(3);
		ObjectClassAndPath.Emplace(Fields[0]);
		FString Fixup(TEXT("/"));
		Fixup += Fields[1];
		ObjectClassAndPath.Emplace(Fixup);
		ObjectClassAndPath.Emplace(Fields[2]);
	}
	else
	{
		ObjectClassAndPath.Empty(Fields.Num());
		for (const FStringView& Item : Fields)
		{
			ObjectClassAndPath.Emplace(Item);
		}
	}
}

#if WITH_EDITOR
void FCompactFullName::SetCompactFullNameFromObject(UObject* InDepObject)
{
	ObjectClassAndPath.Empty();

	UObject* DepObject = InDepObject;
	if (DepObject)
	{
		ObjectClassAndPath.Add(DepObject->GetClass()->GetFName());
		while (DepObject)
		{
			ObjectClassAndPath.Insert(DepObject->GetFName(), 1);
			DepObject = DepObject->GetOuter();
		}
	}
	else
	{
		ObjectClassAndPath.Add(FName("null"));
	}
}
#endif

uint32 GetTypeHash(const FCompactFullName& A)
{
	uint32 Hash = 0;
	for (const FName& Name : A.ObjectClassAndPath)
	{
		Hash = HashCombine(Hash, GetTypeHash(Name));
	}
	return Hash;
}

void FixupUnsanitizedNames(const FString& Src, TArray<FString>& OutFields) 
{
	FString NewSrc(Src);

	int32 ParenOpen = -1;
	int32 ParenClose = -1;

	if (NewSrc.FindChar(TCHAR('('), ParenOpen) && NewSrc.FindChar(TCHAR(')'), ParenClose) && ParenOpen < ParenClose && ParenOpen >= 0 && ParenClose >= 0)
	{
		for (int32 Index = ParenOpen + 1; Index < ParenClose; Index++)
		{
			if (NewSrc[Index] == TCHAR(','))
			{
				NewSrc[Index] = ' ';
			}
		}
		OutFields.Empty();
		NewSrc.TrimStartAndEnd().ParseIntoArray(OutFields, TEXT(","), false);
		// allow formats both with and without pipeline hash
		check(OutFields.Num() == 11 || OutFields.Num() == 12);
	}
}

void FStableShaderKeyAndValue::ComputeKeyHash()
{
	KeyHash = GetTypeHash(ClassNameAndObjectPath);

	KeyHash = HashCombine(KeyHash, GetTypeHash(ShaderType));
	KeyHash = HashCombine(KeyHash, GetTypeHash(ShaderClass));
	KeyHash = HashCombine(KeyHash, GetTypeHash(MaterialDomain));
	KeyHash = HashCombine(KeyHash, GetTypeHash(FeatureLevel));

	KeyHash = HashCombine(KeyHash, GetTypeHash(QualityLevel));
	KeyHash = HashCombine(KeyHash, GetTypeHash(TargetFrequency));
	KeyHash = HashCombine(KeyHash, GetTypeHash(TargetPlatform));

	KeyHash = HashCombine(KeyHash, GetTypeHash(VFType));
	KeyHash = HashCombine(KeyHash, GetTypeHash(PermutationId));
	KeyHash = HashCombine(KeyHash, GetTypeHash(PipelineHash));
}

void FStableShaderKeyAndValue::ParseFromString(const FStringView& Src)
{
	TArray<FStringView, TInlineAllocator<12>> Fields;
	UE::String::ParseTokens(Src.TrimStartAndEnd(), TEXT(','), [&Fields](FStringView Field) { Fields.Add(Field); });

	/* disabled, should not be happening since 1H 2018
	if (Fields.Num() > 12)
	{
		// hack fix for unsanitized names, should not occur anymore.
		FixupUnsanitizedNames(Src, Fields);
	}
	*/

	// for a while, accept old .scl.csv without pipelinehash
	check(Fields.Num() == 11 || Fields.Num() == 12);

	int32 Index = 0;
	ClassNameAndObjectPath.ParseFromString(Fields[Index++]);

	ShaderType = FName(Fields[Index++]);
	ShaderClass = FName(Fields[Index++]);
	MaterialDomain = FName(Fields[Index++]);
	FeatureLevel = FName(Fields[Index++]);

	QualityLevel = FName(Fields[Index++]);
	TargetFrequency = FName(Fields[Index++]);
	TargetPlatform = FName(Fields[Index++]);

	VFType = FName(Fields[Index++]);
	PermutationId = FName(Fields[Index++]);

	OutputHash.FromString(Fields[Index++]);

	check(Index == 11);

	if (Fields.Num() == 12)
	{
		PipelineHash.FromString(Fields[Index++]);
	}
	else
	{
		PipelineHash = FSHAHash();
	}

	ComputeKeyHash();
}


void FStableShaderKeyAndValue::ParseFromStringCached(const FStringView& Src, TMap<uint32, FName>& NameCache)
{
	TArray<FStringView, TInlineAllocator<12>> Fields;
	UE::String::ParseTokens(Src.TrimStartAndEnd(), TEXT(','), [&Fields](FStringView Field) { Fields.Add(Field); });

	/* disabled, should not be happening since 1H 2018
	if (Fields.Num() > 11)
	{
		// hack fix for unsanitized names, should not occur anymore.
		FixupUnsanitizedNames(Src, Fields);
	}
	*/

	// for a while, accept old .scl.csv without pipelinehash
	check(Fields.Num() == 11 || Fields.Num() == 12);

	int32 Index = 0;
	ClassNameAndObjectPath.ParseFromString(Fields[Index++]);

	// There is a high level of uniformity on the following FNames, use
	// the local name cache to accelerate lookup
	ShaderType = ParseFNameCached(Fields[Index++], NameCache);
	ShaderClass = ParseFNameCached(Fields[Index++], NameCache);
	MaterialDomain = ParseFNameCached(Fields[Index++], NameCache);
	FeatureLevel = ParseFNameCached(Fields[Index++], NameCache);

	QualityLevel = ParseFNameCached(Fields[Index++], NameCache);
	TargetFrequency = ParseFNameCached(Fields[Index++], NameCache);
	TargetPlatform = ParseFNameCached(Fields[Index++], NameCache);

	VFType = ParseFNameCached(Fields[Index++], NameCache);
	PermutationId = ParseFNameCached(Fields[Index++], NameCache);

	OutputHash.FromString(Fields[Index++]);

	check(Index == 11);

	if (Fields.Num() == 12)
	{
		PipelineHash.FromString(Fields[Index++]);
	}
	else
	{
		PipelineHash = FSHAHash();
	}

	ComputeKeyHash();
}

FString FStableShaderKeyAndValue::ToString() const
{
	FString Result;
	ToString(Result);
	return Result;
}

void FStableShaderKeyAndValue::ToString(FString& OutResult) const
{
	TStringBuilder<384> Out;
	const TCHAR Delim = TEXT(',');

	const int32 ClassNameAndObjectPathOffset = Out.Len();
	ClassNameAndObjectPath.AppendString(Out);
	Algo::Replace(MakeArrayView(Out).Slice(ClassNameAndObjectPathOffset, Out.Len() - ClassNameAndObjectPathOffset), Delim, TEXT(' '));
	Out << Delim;

	AppendSanitizedFName(Out, ShaderType, Delim);
	Out << Delim;
	AppendSanitizedFName(Out, ShaderClass, Delim);
	Out << Delim;

	Out << MaterialDomain << Delim;
	Out << FeatureLevel << Delim;
	Out << QualityLevel << Delim;
	Out << TargetFrequency << Delim;
	Out << TargetPlatform << Delim;
	Out << VFType << Delim;
	Out << PermutationId << Delim;

	Out << OutputHash << Delim;
	Out << PipelineHash;

	OutResult = FStringView(Out);
}

void FStableShaderKeyAndValue::AppendString(FAnsiStringBuilderBase& Out) const
{
	const ANSICHAR Delim = ',';

	const int32 ClassNameAndObjectPathOffset = Out.Len();
	ClassNameAndObjectPath.AppendString(Out);
	Algo::Replace(MakeArrayView(Out).Slice(ClassNameAndObjectPathOffset, Out.Len() - ClassNameAndObjectPathOffset), Delim, ' ');
	Out << Delim;

	AppendSanitizedFNameAsUTF8(Out, ShaderType, Delim);
	Out << Delim;
	AppendSanitizedFNameAsUTF8(Out, ShaderClass, Delim);
	Out << Delim;

	AppendFNameAsUTF8(Out, MaterialDomain);
	Out << Delim;
	AppendFNameAsUTF8(Out, FeatureLevel);
	Out << Delim;
	AppendFNameAsUTF8(Out, QualityLevel);
	Out << Delim;
	AppendFNameAsUTF8(Out, TargetFrequency);
	Out << Delim;
	AppendFNameAsUTF8(Out, TargetPlatform);
	Out << Delim;
	AppendFNameAsUTF8(Out, VFType);
	Out << Delim;
	AppendFNameAsUTF8(Out, PermutationId);
	Out << Delim;

	Out << OutputHash;
	Out << Delim;
	Out << PipelineHash;
}

FString FStableShaderKeyAndValue::HeaderLine()
{
	FString Result;

	const FString Delim(TEXT(","));

	Result += TEXT("ClassNameAndObjectPath");
	Result += Delim;

	Result += TEXT("ShaderType");
	Result += Delim;
	Result += TEXT("ShaderClass");
	Result += Delim;
	Result += TEXT("MaterialDomain");
	Result += Delim;
	Result += TEXT("FeatureLevel");
	Result += Delim;

	Result += TEXT("QualityLevel");
	Result += Delim;
	Result += TEXT("TargetFrequency");
	Result += Delim;
	Result += TEXT("TargetPlatform");
	Result += Delim;

	Result += TEXT("VFType");
	Result += Delim;
	Result += TEXT("Permutation");
	Result += Delim;

	Result += TEXT("OutputHash");
	Result += Delim;

	Result += TEXT("PipelineHash");

	return Result;
}

void FStableShaderKeyAndValue::SetPipelineHash(const FShaderPipeline* Pipeline)
{
	if (LIKELY(Pipeline))
	{
		// cache this?
		FShaderCodeLibraryPipeline LibraryPipeline;
		LibraryPipeline.Initialize(Pipeline);
		LibraryPipeline.GetPipelineHash(PipelineHash); 
	}
	else
	{
		PipelineHash = FSHAHash();
	}
}

#if WITH_EDITOR
void WriteToCompactBinary(FCbWriter& Writer, const FStableShaderKeyAndValue& Key,
	const TMap<FSHAHash, int32>& HashToIndex)
{
	Writer.BeginArray();
	{
		TStringBuilder<128> ClassNameAndObjectPathStr;
		Key.ClassNameAndObjectPath.AppendString(ClassNameAndObjectPathStr);
		Writer << ClassNameAndObjectPathStr;
	}
	Writer << Key.ShaderType;
	Writer << Key.ShaderClass;
	Writer << Key.MaterialDomain;
	Writer << Key.FeatureLevel;
	Writer << Key.QualityLevel;
	Writer << Key.TargetFrequency;
	Writer << Key.TargetPlatform;
	Writer << Key.VFType;
	Writer << Key.PermutationId;
	Writer << HashToIndex[Key.OutputHash];
	Writer << HashToIndex[Key.PipelineHash];
	Writer.EndArray();
}

bool LoadFromCompactBinary(FCbFieldView Field, FStableShaderKeyAndValue& Key,
	const TArray<FSHAHash>& IndexToHash)
{
	int32 NumFields = Field.AsArrayView().Num();
	if (NumFields != 12)
	{
		Key = FStableShaderKeyAndValue();
		return false;
	}
	FCbFieldViewIterator It = Field.CreateViewIterator();
	bool bOk = true;

	{
		TStringBuilder<128> ClassNameAndObjectPathStr;
		bOk = LoadFromCompactBinary(*It++, ClassNameAndObjectPathStr) & bOk;
		Key.ClassNameAndObjectPath.ParseFromString(ClassNameAndObjectPathStr.ToView());
	}

	auto LoadIndexedHashFromCompactBinary = [&IndexToHash](FCbFieldView HashField, FSHAHash& OutHash)
	{
		int32 Index;
		if (!LoadFromCompactBinary(HashField, Index))
		{
			OutHash = FSHAHash();
			return false;
		}
		if (!IndexToHash.IsValidIndex(Index))
		{
			OutHash = FSHAHash();
			return false;
		}
		OutHash = IndexToHash.GetData()[Index];
		return true;
	};

	bOk = LoadFromCompactBinary(*It++, Key.ShaderType) & bOk;
	bOk = LoadFromCompactBinary(*It++, Key.ShaderClass) & bOk;
	bOk = LoadFromCompactBinary(*It++, Key.MaterialDomain) & bOk;
	bOk = LoadFromCompactBinary(*It++, Key.FeatureLevel) & bOk;
	bOk = LoadFromCompactBinary(*It++, Key.QualityLevel) & bOk;
	bOk = LoadFromCompactBinary(*It++, Key.TargetFrequency) & bOk;
	bOk = LoadFromCompactBinary(*It++, Key.TargetPlatform) & bOk;
	bOk = LoadFromCompactBinary(*It++, Key.VFType) & bOk;
	bOk = LoadFromCompactBinary(*It++, Key.PermutationId) & bOk;
	bOk = LoadIndexedHashFromCompactBinary(*It++, Key.OutputHash) & bOk;
	bOk = LoadIndexedHashFromCompactBinary(*It++, Key.PipelineHash) & bOk;

	Key.ComputeKeyHash();
	return bOk;
}
#endif


void FShaderCodeLibraryPipeline::Initialize(const FShaderPipeline* Pipeline)
{
	check(Pipeline != nullptr);
	for (uint32 Frequency = 0u; Frequency < SF_NumGraphicsFrequencies; ++Frequency)
	{
		if (Pipeline->Shaders[Frequency].IsValid())
		{
			Shaders[Frequency] = Pipeline->Shaders[Frequency]->GetOutputHash();
		}
	}
}

void FShaderCodeLibraryPipeline::GetPipelineHash(FSHAHash& Output)
{
	FSHA1 Hasher;

	for (int32 Frequency = 0; Frequency < SF_NumGraphicsFrequencies; Frequency++)
	{
		Hasher.Update(&Shaders[Frequency].Hash[0], sizeof(FSHAHash));
	}

	Hasher.Final();
	Hasher.GetHash(&Output.Hash[0]);
}

class FShaderLibraryInstance
{
public:
	static FShaderLibraryInstance* Create(EShaderPlatform InShaderPlatform, const FString& ShaderCodeDir, FString const& InLibraryName)
	{
		FRHIShaderLibraryRef Library;
		FString ShaderCodeDirectory;
		if (RHISupportsNativeShaderLibraries(InShaderPlatform))
		{
			Library = RHICreateShaderLibrary(InShaderPlatform, ShaderCodeDir, InLibraryName);
			ShaderCodeDirectory = ShaderCodeDir;
		}

		if (!Library && UE::ShaderLibrary::Private::IsRunningWithIoStore())
		{
			Library = FIoStoreShaderCodeArchive::Create(InShaderPlatform, InLibraryName, FIoDispatcher::Get());
			ShaderCodeDirectory.Empty();	// paths don't matter for IoStore-based libraries
		}

		// Shader library as a ushaderbytecode file is no longer an option for distribution. Some cases (a build using loose files) still require its support though.
		if (!Library && UE::ShaderLibrary::Private::ShouldLookForLooseCookedChunks())
		{
			const FName PlatformName = FDataDrivenShaderPlatformInfo::GetName(InShaderPlatform);
			const FName ShaderFormatName = LegacyShaderPlatformToShaderFormat(InShaderPlatform);
			FString ShaderFormatAndPlatform = ShaderFormatName.ToString() + TEXT("-") + PlatformName.ToString();

			const FString DestFilePath = GetCodeArchiveFilename(ShaderCodeDir, InLibraryName, FName(ShaderFormatAndPlatform));
			TUniquePtr<FArchive> Ar(IFileManager::Get().CreateFileReader(*DestFilePath));
			if (Ar)
			{
				uint32 Version = 0;
				*Ar << Version;
				if (Version == GShaderCodeArchiveVersion)
				{
					Library = FShaderCodeArchive::Create(InShaderPlatform, *Ar, DestFilePath, ShaderCodeDir, InLibraryName);
					if (Library)
					{
						ShaderCodeDirectory = ShaderCodeDir;

						bool ShaderCodeLibrarySeparateLoadingCacheCommandLineOverride = FParse::Param(FCommandLine::Get(), TEXT("ShaderCodeLibrarySeparateLoadingCache"));;
						if (GShaderCodeLibrarySeparateLoadingCache || ShaderCodeLibrarySeparateLoadingCacheCommandLineOverride)
						{
							TArray<TArray<FString>> FilesToMakeUnique;
							FilesToMakeUnique.AddDefaulted(1);
							FilesToMakeUnique[0].Add(DestFilePath);
							FPlatformFileManager::Get().GetPlatformFile().MakeUniquePakFilesForTheseFiles(FilesToMakeUnique);
						}
					}
				}
			}
		}

		if (!Library)
		{
			return nullptr;
		}

		FShaderLibraryInstance* Instance = new FShaderLibraryInstance();
		Instance->Library = Library;
		Instance->ShaderCodeDirectory = ShaderCodeDirectory;

		const int32 NumResources = Library->GetNumShaderMaps();
		Instance->Resources.AddZeroed(NumResources);

		INC_DWORD_STAT_BY(STAT_Shaders_ShaderResourceMemory, Instance->GetSizeBytes());

		return Instance;
	}

	~FShaderLibraryInstance()
	{
		// release RHI resources on all of the resources (note: this has nothing to do with their own lifetime and refcount)
		for (FShaderMapResource_SharedCode* Resource : Resources)
		{
			if (Resource)
			{
				BeginReleaseResource(Resource);
			}
		}

		// if rendering thread is active, the actual teardown may happen later, so flush the rendering commands here
		checkf(IsInGameThread(), TEXT("Shader library closure is expected to happen only on the game thread, at the \'top\' of the pipeline"));
		FlushRenderingCommands();	// this will also flush pending deletes
		
		Library->Teardown();
		DEC_DWORD_STAT_BY(STAT_Shaders_ShaderResourceMemory, GetSizeBytes());
	}

	EShaderPlatform GetPlatform() const { return Library->GetPlatform(); }
	const int32 GetNumResources() const { return Resources.Num(); }
	const int32 GetNumShaders() const { return Library->GetNumShaders(); }

	uint32 GetSizeBytes()
	{
		uint32 ShaderBucketsSize = 0;
		for (int32 IdxBucket = 0, NumBuckets = UE_ARRAY_COUNT(RHIShaders); IdxBucket < NumBuckets; ++IdxBucket)
		{
			FRWScopeLock Locker(ShaderLocks[IdxBucket], SLT_ReadOnly);
			ShaderBucketsSize += RHIShaders[IdxBucket].GetAllocatedSize();
		}
		return sizeof(*this) + ShaderBucketsSize + Resources.GetAllocatedSize();
	}

	const int32 GetNumShadersForShaderMap(int32 ShaderMapIndex) const
	{
		return Library->GetNumShadersForShaderMap(ShaderMapIndex);
	}

	void PreloadShader(int32 ShaderIndex, FArchive* Ar)
	{
		SCOPED_LOADTIMER(FShaderLibraryInstance_PreloadShader);
		FGraphEventArray PreloadCompletionEvents;
		Library->PreloadShader(ShaderIndex, PreloadCompletionEvents);
		if (Ar && PreloadCompletionEvents.Num() > 0)
		{
			FExternalReadCallback ExternalReadCallback = [this, PreloadCompletionEvents = MoveTemp(PreloadCompletionEvents)](double ReaminingTime)
			{
				return this->OnExternalReadCallback(PreloadCompletionEvents, ReaminingTime);
			};
			Ar->AttachExternalReadDependency(ExternalReadCallback);
		}
	}

	void ReleasePreloadedShader(int32 ShaderIndex)
	{
		SCOPED_LOADTIMER(FShaderLibraryInstance_PreloadShader);
		Library->ReleasePreloadedShader(ShaderIndex);
	}

	TRefCountPtr<FShaderMapResource_SharedCode> GetResource(int32 ShaderMapIndex)
	{
		FRWScopeLock Locker(ResourceLock, SLT_ReadOnly);
		return Resources[ShaderMapIndex];
	}

	TRefCountPtr<FShaderMapResource_SharedCode> AddOrDeleteResource(FShaderMapResource_SharedCode* Resource, FArchive* Ar)
	{
		const int32 ShaderMapIndex = Resource->ShaderMapIndex;
		TRefCountPtr<FShaderMapResource_SharedCode> OutResource(Resource);
		bool bPreload = false;
		{
			FRWScopeLock Locker(ResourceLock, SLT_Write);
			FShaderMapResource_SharedCode* PrevResource = Resources[ShaderMapIndex];
			if (!PrevResource)
			{
				Resources[ShaderMapIndex] = Resource;
				bPreload = !GRHILazyShaderCodeLoading;
			}
			else
			{
				OutResource = PrevResource;
			}
		}

		if (bPreload)
		{
			SCOPED_LOADTIMER(FShaderLibraryInstance_PreloadShaderMap);
			FGraphEventArray PreloadCompletionEvents;
			Resource->bShaderMapPreloaded = Library->PreloadShaderMap(ShaderMapIndex, PreloadCompletionEvents);
			if (Ar && PreloadCompletionEvents.Num() > 0)
			{
				FExternalReadCallback ExternalReadCallback = [this, PreloadCompletionEvents = MoveTemp(PreloadCompletionEvents)](double ReaminingTime)
				{
					return this->OnExternalReadCallback(PreloadCompletionEvents, ReaminingTime);
				};
				Ar->AttachExternalReadDependency(ExternalReadCallback);
			}
		}

		return OutResource;
	}

	bool TryRemoveResource(FShaderMapResource_SharedCode* Resource)
	{
		FRWScopeLock Locker(ResourceLock, SLT_Write);

		if (Resource->GetNumRefs() == 0)
		{
			const int32 ShaderMapIndex = Resource->ShaderMapIndex;
			check(Resources[ShaderMapIndex] == Resource);
			Resources[ShaderMapIndex] = nullptr;
			return true;
		}

		// Another thread found the resource after ref-count was decremented to zero
		return false;
	}

	TRefCountPtr<FRHIShader> GetOrCreateShader(int32 ShaderIndex)
	{
		const int32 BucketIndex = ShaderIndex % NumShaderLocks;
		TRefCountPtr<FRHIShader> Shader;
		{
			FRWScopeLock Locker(ShaderLocks[BucketIndex], SLT_ReadOnly);
			TRefCountPtr<FRHIShader>* ShaderPtr = RHIShaders[BucketIndex].Find(ShaderIndex);
			if (ShaderPtr)
			{
				Shader = *ShaderPtr;
			}
		}
		if (!Shader)
		{
			Shader = Library->CreateShader(ShaderIndex);

			FRWScopeLock Locker(ShaderLocks[BucketIndex], SLT_Write);
			TRefCountPtr<FRHIShader>* ShaderPtr = RHIShaders[BucketIndex].Find(ShaderIndex);
			if (LIKELY(ShaderPtr == nullptr))
			{
				RHIShaders[BucketIndex].Add(ShaderIndex, Shader);
			}
			else
			{
				Shader = *ShaderPtr;
			}
		}
		return Shader;
	}

	void ReleaseShader(int32 ShaderIndex)
	{
		const int32 BucketIndex = ShaderIndex % NumShaderLocks;
		FRWScopeLock Locker(ShaderLocks[BucketIndex], SLT_Write);
		TRefCountPtr<FRHIShader>* ShaderPtr = RHIShaders[BucketIndex].Find(ShaderIndex);
		FRHIShader* Shader = ShaderPtr ? ShaderPtr->GetReference() : nullptr;
		if (Shader)
		{
			// The library instance is holding one ref
			// External caller of this method must be holding a ref as well, so there must be at least 2 refs
			// If those are the only 2 refs, we release the ref held by the library instance, to allow the shader to be destroyed once caller releases its ref
			const uint32 NumRefs = Shader->GetRefCount();
			check(NumRefs > 1u);
			if(NumRefs == 2u)
			{
				RHIShaders[BucketIndex].Remove(ShaderIndex);
			}
		}
	}

	void PreloadPackageShaderMap(int32 ShaderMapIndex, FCoreDelegates::FAttachShaderReadRequestFunc AttachShaderReadRequestFunc)
	{
		FRWScopeLock Locker(ResourceLock, SLT_Write);
		FShaderMapResource_SharedCode*& Resource = Resources[ShaderMapIndex];
		if (!Resource)
		{
			Resource = new FShaderMapResource_SharedCode(this, ShaderMapIndex);
			Resource->bShaderMapPreloaded = Library->PreloadShaderMap(ShaderMapIndex, AttachShaderReadRequestFunc);
			BeginInitResource(Resource);
		}
		Resource->AddRef();
	}

	void ReleasePreloadedPackageShaderMap(int32 ShaderMapIndex)
	{
		FShaderMapResource_SharedCode* Resource = nullptr;
		{
			FRWScopeLock Locker(ResourceLock, SLT_Write);
			Resource = Resources[ShaderMapIndex];
		}

		if (Resource)
		{
			Resource->Release();
		}
	}

	bool HasContentFrom(const FString& ShaderCodeDir, FString const& InLibraryName)
	{
		if (Library->GetName() == InLibraryName)
		{
			// IoStore-based libraries don't care about the directory, so name collision alone is enough to say yes.
			return ShaderCodeDirectory.IsEmpty() ? true : (ShaderCodeDirectory == ShaderCodeDir);
		}

		return false;
	}

public:
	FRHIShaderLibraryRef Library;

private:
	static const int32 NumShaderLocks = 32;

	FShaderLibraryInstance() {}

	bool OnExternalReadCallback(const FGraphEventArray& Events, double RemainingTime)
	{
		if (Events.Num())
		{
			if (RemainingTime < 0.0)
			{
				for (const FGraphEventRef& Event : Events)
				{
					if (!Event->IsComplete()) return false;
				}
				return true;
			}
			FTaskGraphInterface::Get().WaitUntilTasksComplete(Events);
		}
		return true;
	}

	/** Number of shaders can be pretty large (several hundred thousands). Do not allocate memory for them upfront, but instead store them in a map. 
	    There's number of maps to reduce the lock contention. */
	TMap<int32, TRefCountPtr<FRHIShader>> RHIShaders[NumShaderLocks];

	TArray<FShaderMapResource_SharedCode*> Resources;

	/** Locks that guard access to particular shader buckets. */
	FRWLock ShaderLocks[NumShaderLocks];
	FRWLock ResourceLock;

	/** Folder the library was created from (doesn't matter for IoStore-based libraries) */
	FString ShaderCodeDirectory;
};

FShaderMapResource_SharedCode::FShaderMapResource_SharedCode(FShaderLibraryInstance* InLibraryInstance, int32 InShaderMapIndex)
	: FShaderMapResource(InLibraryInstance->GetPlatform(), InLibraryInstance->GetNumShadersForShaderMap(InShaderMapIndex))
	, LibraryInstance(InLibraryInstance)
	, ShaderMapIndex(InShaderMapIndex)
	, bShaderMapPreloaded(false)
{
}

FShaderMapResource_SharedCode::~FShaderMapResource_SharedCode()
{
	
}

FSHAHash FShaderMapResource_SharedCode::GetShaderHash(int32 ShaderIndex)
{
	return LibraryInstance->Library->GetShaderHash(ShaderMapIndex, ShaderIndex);
}

FRHIShader* FShaderMapResource_SharedCode::CreateRHIShaderOrCrash(int32 ShaderIndex)
{
	SCOPED_LOADTIMER(FShaderMapResource_SharedCode_InitRHI);
#if STATS
	double TimeFunctionEntered = FPlatformTime::Seconds();
	ON_SCOPE_EXIT
	{
		double ShaderCreationTime = FPlatformTime::Seconds() - TimeFunctionEntered;
		INC_FLOAT_STAT_BY(STAT_Shaders_TotalRTShaderInitForRenderingTime, ShaderCreationTime);
	};
#endif

	const int32 LibraryShaderIndex = LibraryInstance->Library->GetShaderIndex(ShaderMapIndex, ShaderIndex);
	TRefCountPtr<FRHIShader> CreatedShader = LibraryInstance->GetOrCreateShader(LibraryShaderIndex);
	if (UNLIKELY(CreatedShader == nullptr))
	{
		UE_LOG(LogShaders, Fatal, TEXT("FShaderMapResource_SharedCode::InitRHI is unable to create a shader"));
		// unreachable
		return nullptr;
	}

	CreatedShader->AddRef();
	return CreatedShader;
}

void FShaderMapResource_SharedCode::ReleasePreloadedShaderCode(int32 ShaderIndex)
{
	SCOPED_LOADTIMER(FShaderMapResource_SharedCode_InitRHI);	// part of shader initialization in a way

	if (bShaderMapPreloaded)
	{
		const int32 LibraryShaderIndex = LibraryInstance->Library->GetShaderIndex(ShaderMapIndex, ShaderIndex);
		LibraryInstance->Library->ReleasePreloadedShader(LibraryShaderIndex);
	}
}

void FShaderMapResource_SharedCode::ReleaseRHI()
{
	if (LibraryInstance && ensureMsgf(LibraryInstance->Library, TEXT("LibraryInstance->Library pointer is expected to be valid as long as library's FShaderMapResource are alive.")))
	{
		const int32 NumShaders = GetNumShaders();
		for (int32 i = 0; i < NumShaders; ++i)
		{
			const int32 LibraryShaderIndex = LibraryInstance->Library->GetShaderIndex(ShaderMapIndex, i);
			if (HasShader(i))
			{
				LibraryInstance->ReleaseShader(LibraryShaderIndex);
			}
			else if (bShaderMapPreloaded)
			{
				// Release the preloaded memory if it was preloaded, but not created yet
				LibraryInstance->Library->ReleasePreloadedShader(LibraryShaderIndex);
			}
		}
	}

	bShaderMapPreloaded = false;

	FShaderMapResource::ReleaseRHI();

	// on assumption that we aren't going to get resurrected
	LibraryInstance = nullptr;

	if (GetNumRefs() > 0)
	{
		ensureMsgf(false, TEXT("FShaderMapResource_SharedCode::ReleaseRHI is still referenced (Num of references %d, owner %s). Invoking OnSharedShaderMapResourceExplicitRelease delegate."), GetNumRefs(), *GetOwnerName().ToString());
		UE_LOG(LogShaderLibrary, Warning, TEXT("FShaderMapResource_SharedCode::ReleaseRHI is still referenced (Num of references %d, owner %s). Invoking OnSharedShaderMapResourceExplicitRelease delegate."), 
			GetNumRefs(), *GetOwnerName().ToString());
		
		// Invoke delegate to notify shader map resource needs to be forced released
		OnSharedShaderMapResourceExplicitRelease.ExecuteIfBound(this);
	}
}

bool FShaderMapResource_SharedCode::TryRelease()
{
	if (LibraryInstance && LibraryInstance->TryRemoveResource(this))
	{
		return true;
	}

	return false;
}

#if WITH_EDITOR
struct FShaderCodeStats
{
	int64 ShadersSize;
	int64 ShadersUniqueSize;
	int32 NumShaders;
	int32 NumUniqueShaders;
	int32 NumShaderMaps;
};

struct FEditorShaderCodeArchive
{
	FEditorShaderCodeArchive(FName InFormat, bool bInNeedsDeterministicOrder)
		: FormatName(InFormat)
		, Format(nullptr)
		, bNeedsDeterministicOrder(bInNeedsDeterministicOrder)
	{
		TArray<FString> Components;
		FString Name = FormatName.ToString();
		Name.ParseIntoArray(Components, TEXT("-"));

		FName ShaderFormatName(Components[0]);
		Format = GetTargetPlatformManagerRef().FindShaderFormat(ShaderFormatName);

		check(Format);

		SerializedShaders.ShaderHashTable.Clear(0x10000);
		SerializedShaders.ShaderMapHashTable.Clear(0x10000);
	}

	~FEditorShaderCodeArchive() {}

	const IShaderFormat* GetFormat() const
	{
		return Format;
	}

	void OpenLibrary(FString const& Name)
	{
		check(LibraryName.Len() == 0);
		check(Name.Len() > 0);
		LibraryName = Name;
		SerializedShaders.Empty();
		ShaderCode.Empty();
	}

	void CloseLibrary(FString const& Name)
	{
		check(LibraryName == Name);
		LibraryName = TEXT("");
	}

	bool HasShaderMap(const FSHAHash& Hash) const
	{
		return SerializedShaders.FindShaderMap(Hash) != INDEX_NONE;
	}

	bool IsEmpty() const
	{
		return SerializedShaders.GetNumShaders() == 0;
	}

	int32 AddShaderCode(FShaderCodeStats& CodeStats, const FShaderMapResourceCode* Code, const FShaderMapAssetPaths& AssociatedAssets)
	{
		int32 ShaderMapIndex = INDEX_NONE;

		if (AssociatedAssets.Num() == 0 && LibraryName != TEXT("Global"))
		{
			UE_LOG(LogShaderLibrary, Warning, TEXT("Shadermap %s does not have assets associated with it, library layout may be inconsistent between builds"), *Code->ResourceHash.ToString());
		}

		if (SerializedShaders.FindOrAddShaderMap(Code->ResourceHash, ShaderMapIndex, &AssociatedAssets))
		{
			const int32 NumShaders = Code->ShaderEntries.Num();
			FShaderMapEntry& ShaderMapEntry = SerializedShaders.ShaderMapEntries[ShaderMapIndex];
			ShaderMapEntry.NumShaders = NumShaders;
			ShaderMapEntry.ShaderIndicesOffset = SerializedShaders.ShaderIndices.AddZeroed(NumShaders);

			for(int32 i = 0; i < NumShaders; ++i)
			{
				int32 ShaderIndex = INDEX_NONE;
				if (SerializedShaders.FindOrAddShader(Code->ShaderHashes[i], ShaderIndex))
				{
					const FShaderMapResourceCode::FShaderEntry& SourceShaderEntry = Code->ShaderEntries[i];
					FShaderCodeEntry& SerializedShaderEntry = SerializedShaders.ShaderEntries[ShaderIndex];
					SerializedShaderEntry.Frequency = SourceShaderEntry.Frequency;
					SerializedShaderEntry.Size = SourceShaderEntry.Code.Num();
					SerializedShaderEntry.UncompressedSize = SourceShaderEntry.UncompressedSize;
					check(!SourceShaderEntry.Code.IsEmpty());
					ShaderCode.Add(SourceShaderEntry.Code);
					check(ShaderCode.Num() == SerializedShaders.ShaderEntries.Num());

					CodeStats.NumUniqueShaders++;
					CodeStats.ShadersUniqueSize += SourceShaderEntry.Code.Num();
				}
				CodeStats.ShadersSize += Code->ShaderEntries[i].Code.Num();
				SerializedShaders.ShaderIndices[ShaderMapEntry.ShaderIndicesOffset + i] = ShaderIndex;
			}

			// for total shaders, only count shaders when we're adding a new shadermap. AddShaderCode() for the same shadermap can be called several times during
			// the cook because of serialization path being reused for other purposes than actual saving, so counting them every time artificially inflates number of shaders.
			CodeStats.NumShaders += Code->ShaderEntries.Num();
			CodeStats.NumShaderMaps++;
		}
		// always mark the shadermap dirty, because it might have gotten new asset associations
		MarkShaderMapDirty(ShaderMapIndex);
		return ShaderMapIndex;
	}

	void MarkShaderMapDirty(int32 ShaderMapIndex)
	{
		if (bHasCopiedAndCleared)
		{
			ShaderMapsToCopy.Add(ShaderMapIndex);
		}
	}

	void MarkAllShaderMapsDirty()
	{
		if (bHasCopiedAndCleared)
		{
			for (int32 ShaderMapIndex = 0; ShaderMapIndex < SerializedShaders.ShaderMapHashes.Num(); ++ShaderMapIndex)
			{
				ShaderMapsToCopy.Add(ShaderMapIndex);
			}
		}
	}

	bool HasDataToCopy() const
	{
		// After the first copy dirty elements are added to ShaderMapsToCopy; during the first copy every ShaderMap in SerializedShaders is copied
		return bHasCopiedAndCleared ? !ShaderMapsToCopy.IsEmpty() : !SerializedShaders.ShaderMapHashes.IsEmpty();
	}

	void CopyToCompactBinary(FCbWriter& Writer, FSerializedShaderArchive& TransferArchive, TArray<uint8>& TransferCode)
	{
		Writer.BeginObject();
		Writer << "SerializedShaders" << TransferArchive;
		Writer << "ShaderCode";
		Writer.AddBinary(FMemoryView(TransferCode.GetData(), TransferCode.Num()));
		Writer.EndObject();
	}

	bool AppendFromCompactBinary(FCbFieldView Field, FShaderCodeStats& CodeStats)
	{
		FSerializedShaderArchive TransferArchive;
		if (!LoadFromCompactBinary(Field["SerializedShaders"], TransferArchive))
		{
			return false;
		}

		FMemoryView FlatShaderCodeView = Field["ShaderCode"].AsBinaryView();
		TConstArrayView64<uint8> TransferCode(reinterpret_cast<const uint8*>(FlatShaderCodeView.GetData()), FlatShaderCodeView.GetSize());
		return AppendFromArchive(MoveTemp(TransferArchive), TransferCode, CodeStats);
	}

	void CopyToArchiveAndClear(FSerializedShaderArchive& TargetArchive, TArray<uint8>& TargetFlatShaderCode,
		bool& bOutRanOutOfRoom, int64 MaxShaderSize, int64 MaxShaderCount)
	{
		bOutRanOutOfRoom = false;
		if (!bHasCopiedAndCleared)
		{
			// First CopyAndClear adds all shadermaps to ShaderMapsToCopy, because as an optimization we do not write to
			// ShaderMapsToCopy until the first CopyAndClear call.
			for (int32 ShaderMapIndex = 0; ShaderMapIndex < SerializedShaders.ShaderMapHashes.Num(); ++ShaderMapIndex)
			{
				ShaderMapsToCopy.Add(ShaderMapIndex);
			}
		}

		TArray<int32> LocalShaderMapsToCopy = ShaderMapsToCopy.Array();
		LocalShaderMapsToCopy.Sort(); // Maintain the same order that the shadermaps were added in
		int32 NumShadersSentWithCode = 0;

		const FSerializedShaderArchive& SourceArchive = this->SerializedShaders;
		TArray<TArray<uint8>>& SourceShaderCodes = this->ShaderCode;
		for (int32 SourceShaderMapIndex : LocalShaderMapsToCopy)
		{
			const FSHAHash& SourceShaderMapHash = SourceArchive.ShaderMapHashes[SourceShaderMapIndex];
			const FShaderMapEntry& SourceEntry = SourceArchive.ShaderMapEntries[SourceShaderMapIndex];
			const FShaderMapAssetPaths* const SourceAssetPaths = SourceArchive.ShaderCodeToAssets.Find(SourceShaderMapHash);

			int32 TargetShaderMapIndex;
			const bool bIsNewShaderMap = TargetArchive.FindOrAddShaderMap(SourceShaderMapHash, TargetShaderMapIndex, SourceAssetPaths);
			if (!bIsNewShaderMap)
			{
				continue;
			}
			FShaderMapEntry& TargetEntry = TargetArchive.ShaderMapEntries[TargetShaderMapIndex];
			const int32 NumShaders = SourceEntry.NumShaders;

			TargetEntry.NumShaders = NumShaders;
			TargetEntry.ShaderIndicesOffset = TargetArchive.ShaderIndices.Num();
			TargetArchive.ShaderIndices.AddUninitialized(NumShaders);

			for (int32 ShaderIndexIndex = 0; ShaderIndexIndex < NumShaders; ++ShaderIndexIndex)
			{
				const uint32 SourceShaderIndex = SourceArchive.ShaderIndices[SourceEntry.ShaderIndicesOffset + ShaderIndexIndex];
				const FSHAHash& SourceShaderHash = SourceArchive.ShaderHashes[SourceShaderIndex];
				int32 TargetShaderIndex;
				const bool bIsNewShader = TargetArchive.FindOrAddShader(SourceShaderHash, TargetShaderIndex);
				TargetArchive.ShaderIndices[TargetEntry.ShaderIndicesOffset + ShaderIndexIndex] = TargetShaderIndex;
				if (bIsNewShader)
				{
					// We rely on the index of the newly added shader being at the end of the list of ShaderEntries,
					// so we can pop off the added index if we overflow below
					check(TargetShaderIndex == TargetArchive.ShaderEntries.Num() - 1);

					const FShaderCodeEntry& SourceShaderEntry = SourceArchive.ShaderEntries[SourceShaderIndex];
					TArray<uint8>& SourceShaderCode = SourceShaderCodes[SourceShaderIndex];
					FShaderCodeEntry& TargetShaderEntry = TargetArchive.ShaderEntries[TargetShaderIndex];

					TargetShaderEntry = SourceShaderEntry;
					if (!SourceShaderCode.IsEmpty())
					{
						TargetShaderEntry.Offset = TargetFlatShaderCode.Num();
						check(SourceShaderEntry.Size == SourceShaderCode.Num());
						if ((MaxShaderSize > 0 && TargetFlatShaderCode.Num() + SourceShaderCode.Num() > MaxShaderSize) ||
							(MaxShaderCount > 0 && NumShadersSentWithCode > MaxShaderCount))
						{
							// We have to stop here to avoid overflowing the shader limit. Send the shaders we have accumulated
							// but do not send&clear any other data.
							// Remove all ShaderMap data
							TargetArchive.EmptyShaderMaps();
							// Remove the ShaderEntry we just added; we are not adding its code so we have to remove it from the
							// list of shaders contained by the targetarchive
							check(TargetShaderIndex == TargetArchive.ShaderHashes.Num() - 1);
							TargetArchive.RemoveLastAddedShader();
							bOutRanOutOfRoom = true;
							// Keep any shaders we added before the one we just added
							return;
						}
						++NumShadersSentWithCode;

						TargetFlatShaderCode.Append(SourceShaderCode);

						// Empty the ShaderCode to save memory in the local process. The consumer of the TargetArchive and
						// TargetFlatShaderCode will be the only one that needs to read it.
						SourceShaderCode.Empty();
					}
					else
					{
						// The shadercode was already copied in an earlier CopyAndClear operation; we just need to note that
						// ShaderMaps in this call to CopyAndClear reference it.
						TargetShaderEntry.Offset = INDEX_NONE;
					}
				}
			}
		}

		ShaderMapsToCopy.Empty();
		bHasCopiedAndCleared = true;
	}

	bool AppendFromArchive(FSerializedShaderArchive&& SourceArchive, TConstArrayView64<uint8> SourceFlatShaderCode,
		FShaderCodeStats& CodeStats)
	{
		bool bOk = true;
		FSerializedShaderArchive& TargetArchive = this->SerializedShaders;
		TArray<TArray<uint8>>& TargetShaderCodes = this->ShaderCode;

		// Add all the shaders; we can sometimes get messages that send the shaders in advance without sending the shadermaps that use them
		for (int32 SourceShaderIndex = 0; SourceShaderIndex < SourceArchive.ShaderHashes.Num(); ++SourceShaderIndex)
		{
			const FSHAHash& SourceShaderHash = SourceArchive.ShaderHashes[SourceShaderIndex];
			int32 TargetShaderIndex = INDEX_NONE;
			const bool bShaderIsNew = TargetArchive.FindOrAddShader(SourceShaderHash, TargetShaderIndex);
			if (!bShaderIsNew)
			{
				continue;
			}
			check(TargetShaderIndex == TargetArchive.ShaderEntries.Num() - 1 &&
				TargetShaderCodes.Num() == TargetArchive.ShaderEntries.Num() - 1);
			TArray<uint8>& TargetShaderCode = TargetShaderCodes.Emplace_GetRef();

			const FShaderCodeEntry& SourceShaderEntry = SourceArchive.ShaderEntries[SourceShaderIndex];
			FShaderCodeEntry& TargetShaderEntry = TargetArchive.ShaderEntries[TargetShaderIndex];
			TargetShaderEntry = SourceShaderEntry;
			TargetShaderEntry.Offset = 0;

			CodeStats.NumUniqueShaders++;
			CodeStats.ShadersUniqueSize += SourceShaderEntry.Size;

			if (SourceShaderEntry.Offset == INDEX_NONE)
			{
				UE_LOG(LogShaderLibrary, Error, TEXT("ShaderMapLibrary transfer received from a remote machine has incomplete record for Shader %s. ")
					TEXT("The remote machine thought this machine already had the shader and did not send the ShaderCode for the Shader, but the shader is not found. ")
					TEXT("The ShaderMaps using the shader will be corrupt."),
					*SourceShaderHash.ToString());
				bOk = false;
				TargetShaderEntry.Size = 0;
			}
			else if (SourceShaderEntry.Offset + SourceShaderEntry.Size > static_cast<uint64>(SourceFlatShaderCode.Num()))
			{
				UE_LOG(LogShaderLibrary, Error, TEXT("ShaderMapLibrary transfer received from a remote machine has corrupt record for Shader %s. ")
					TEXT("The (Offset, Size) specified by the ShaderEntry does not fit in the TransferCode (an array of bytes that should contain the shader code for all transferred shaders. ")
					TEXT("The ShaderMaps using the shader will be corrupt."),
					*SourceShaderHash.ToString());
				bOk = false;
				TargetShaderEntry.Size = 0;
			}
			else
			{
				const TConstArrayView<uint8> SourceShaderCode(SourceFlatShaderCode.GetData() + SourceShaderEntry.Offset, SourceShaderEntry.Size);
				TargetShaderCode = SourceShaderCode; // Copy from source's flat list to the target's separate TArray<uint8> for each shader
			}
		}

		// Add all the shadermaps
		for (int32 SourceShaderMapIndex = 0; SourceShaderMapIndex < SourceArchive.ShaderMapHashes.Num(); ++SourceShaderMapIndex)
		{
			const FSHAHash& SourceShaderMapHash = SourceArchive.ShaderMapHashes[SourceShaderMapIndex];
			const FShaderMapAssetPaths* const SourceAssetPaths = SourceArchive.ShaderCodeToAssets.Find(SourceShaderMapHash);
			int32 TargetShaderMapIndex;
			const bool bIsNewShaderMap = TargetArchive.FindOrAddShaderMap(SourceShaderMapHash, TargetShaderMapIndex, SourceAssetPaths);
			// always mark the shadermap dirty, because it might have gotten new asset associations
			MarkShaderMapDirty(TargetShaderMapIndex);
			if (!bIsNewShaderMap)
			{
				// ShaderMap has already been loaded in this process, e.g. from another CookWorker loading the same Material
				continue;
			}

			const FShaderMapEntry& SourceEntry = SourceArchive.ShaderMapEntries[SourceShaderMapIndex];
			FShaderMapEntry& TargetEntry = TargetArchive.ShaderMapEntries[TargetShaderMapIndex];
			const int32 NumShaders = SourceEntry.NumShaders;

			TargetEntry.NumShaders = NumShaders;
			TargetEntry.ShaderIndicesOffset = TargetArchive.ShaderIndices.Num();
			TargetArchive.ShaderIndices.AddUninitialized(NumShaders);

			for (int32 ShaderIndexIndex = 0; ShaderIndexIndex < NumShaders; ++ShaderIndexIndex)
			{
				const uint32 SourceShaderIndex = SourceArchive.ShaderIndices[SourceEntry.ShaderIndicesOffset + ShaderIndexIndex];
				const FSHAHash& SourceShaderHash = SourceArchive.ShaderHashes[SourceShaderIndex];
				int32 TargetShaderIndex;
				const bool bShaderIsNew = TargetArchive.FindOrAddShader(SourceShaderHash, TargetShaderIndex);
				TargetArchive.ShaderIndices[TargetEntry.ShaderIndicesOffset + ShaderIndexIndex] = TargetShaderIndex;
				// Every shader in the SourceArchive should have already been added by the loop above over SourceArchive.ShaderHashes
				check(!bShaderIsNew);
			}

			CodeStats.NumShaders += TargetEntry.NumShaders; // Sum of shader counts used by each ShaderMap, without removing duplicates
			CodeStats.NumShaderMaps++;
		}
		return bOk;
	}

	/** Produces another archive that contains the code only for these assets */
	FEditorShaderCodeArchive* CreateChunk(int ChunkId, const TSet<FName>& PackagesInChunk)
	{
		checkf(!bHasCopiedAndCleared, TEXT("It is not valid to call CreateChunk on an FEditorShaderCodeArchive that has sent its shaders to another process by calling CopyAndClear."));

		FEditorShaderCodeArchive* NewChunk = new FEditorShaderCodeArchive(FormatName, bNeedsDeterministicOrder);
		NewChunk->OpenLibrary(UE::ShaderLibrary::Private::GetShaderLibraryNameForChunk(LibraryName, ChunkId));

		TArray<int32> ShaderCodeEntriesNeeded;	// this array is filled with the indices from the existing ShaderCode that will need to be taken
		NewChunk->SerializedShaders.CreateAsChunkFrom(SerializedShaders, PackagesInChunk, ShaderCodeEntriesNeeded);
		// extra integrity check
		checkf(ShaderCodeEntriesNeeded.Num() == NewChunk->SerializedShaders.ShaderHashes.Num(), TEXT("FSerializedShaderArchive for the new chunk did not create a valid shader code mapping"));
		checkf(ShaderCodeEntriesNeeded.Num() == NewChunk->SerializedShaders.ShaderEntries.Num(), TEXT("FSerializedShaderArchive for the new chunk did not create a valid shader code mapping"));

		// copy the shader code
		NewChunk->ShaderCode.Empty();
		for (int32 NewArchiveIdx = 0, NumIndices = ShaderCodeEntriesNeeded.Num(); NewArchiveIdx < NumIndices; ++NewArchiveIdx)
		{
			TArray<uint8>& SourceShaderCodeEntry = ShaderCode[ShaderCodeEntriesNeeded[NewArchiveIdx]];
			check(!SourceShaderCodeEntry.IsEmpty());
			NewChunk->ShaderCode.Add(SourceShaderCodeEntry);
		}

		return NewChunk;
	}

	int32 AddShaderCode(int32 OtherShaderMapIndex, const FEditorShaderCodeArchive& OtherArchive)
	{
		checkf(!OtherArchive.bHasCopiedAndCleared,
			TEXT("It is not valid to call AddShaderCode from an FEditorShaderCodeArchive that has sent its shaders to another process by calling CopyAndClear."));

		int32 ShaderMapIndex = 0;
		if (SerializedShaders.FindOrAddShaderMap(OtherArchive.SerializedShaders.ShaderMapHashes[OtherShaderMapIndex], ShaderMapIndex, 
				OtherArchive.SerializedShaders.ShaderCodeToAssets.Find(OtherArchive.SerializedShaders.ShaderMapHashes[OtherShaderMapIndex])))
		{
			const FShaderMapEntry& PrevShaderMapEntry = OtherArchive.SerializedShaders.ShaderMapEntries[OtherShaderMapIndex];
			FShaderMapEntry& ShaderMapEntry = SerializedShaders.ShaderMapEntries[ShaderMapIndex];
			ShaderMapEntry.NumShaders = PrevShaderMapEntry.NumShaders;
			ShaderMapEntry.ShaderIndicesOffset = SerializedShaders.ShaderIndices.AddZeroed(ShaderMapEntry.NumShaders);

			for (uint32 i = 0; i < ShaderMapEntry.NumShaders; ++i)
			{
				const int32 OtherShaderIndex = OtherArchive.SerializedShaders.ShaderIndices[PrevShaderMapEntry.ShaderIndicesOffset + i];
				int32 ShaderIndex = 0;
				if (SerializedShaders.FindOrAddShader(OtherArchive.SerializedShaders.ShaderHashes[OtherShaderIndex], ShaderIndex))
				{
					const FShaderCodeEntry& OtherShaderEntry = OtherArchive.SerializedShaders.ShaderEntries[OtherShaderIndex];
					SerializedShaders.ShaderEntries[ShaderIndex] = OtherShaderEntry;

					const TArray<uint8>& OtherShaderCodeEntry = OtherArchive.ShaderCode[OtherShaderIndex];
					check(!OtherShaderCodeEntry.IsEmpty());
					ShaderCode.Add(OtherShaderCodeEntry);
					check(ShaderCode.Num() == SerializedShaders.ShaderEntries.Num());
				}
				SerializedShaders.ShaderIndices[ShaderMapEntry.ShaderIndicesOffset + i] = ShaderIndex;
			}
		}
		// always mark the shadermap dirty, because it might have gotten new asset associations
		MarkShaderMapDirty(ShaderMapIndex);
		return ShaderMapIndex;
	}

	int32 AddShaderCode(int32 OtherShaderMapIndex,
		const FSerializedShaderArchive& OtherShaders,
		int64 OtherShaderCodeOffset,
		FArchive& Ar)
	{
		int32 ShaderMapIndex = 0;
		const FSHAHash& OtherShaderMapHash = OtherShaders.ShaderMapHashes[OtherShaderMapIndex];
		const FShaderMapAssetPaths* OtherAssets = OtherShaders.ShaderCodeToAssets.Find(OtherShaderMapHash);
		bool bIsNew = SerializedShaders.FindOrAddShaderMap(OtherShaderMapHash, ShaderMapIndex, OtherAssets);
		if (bIsNew)
		{
			const FShaderMapEntry& OtherEntry = OtherShaders.ShaderMapEntries[OtherShaderMapIndex];
			FShaderMapEntry& ShaderMapEntry = SerializedShaders.ShaderMapEntries[ShaderMapIndex];
			ShaderMapEntry.NumShaders = OtherEntry.NumShaders;
			ShaderMapEntry.ShaderIndicesOffset = SerializedShaders.ShaderIndices.AddZeroed(ShaderMapEntry.NumShaders);

			for (uint32 i = 0; i < ShaderMapEntry.NumShaders; ++i)
			{
				const int32 OtherShaderIndex = OtherShaders.ShaderIndices[OtherEntry.ShaderIndicesOffset + i];
				int32 ShaderIndex = 0;
				if (SerializedShaders.FindOrAddShader(OtherShaders.ShaderHashes[OtherShaderIndex], ShaderIndex))
				{
					const FShaderCodeEntry& OtherShaderEntry = OtherShaders.ShaderEntries[OtherShaderIndex];
					SerializedShaders.ShaderEntries[ShaderIndex] = OtherShaderEntry;

					TArray<uint8>& Code = ShaderCode.AddDefaulted_GetRef();
					check(ShaderCode.Num() == SerializedShaders.GetNumShaders());

					// Read shader code from archive and add shader to set
					const int64 ReadSize = OtherShaderEntry.Size;
					check(ReadSize > 0);
					const int64 ReadOffset = OtherShaderCodeOffset + OtherShaderEntry.Offset;
					Code.SetNumUninitialized(ReadSize);
					Ar.Seek(ReadOffset);
					Ar.Serialize(Code.GetData(), ReadSize);
				}
				SerializedShaders.ShaderIndices[ShaderMapEntry.ShaderIndicesOffset + i] = ShaderIndex;
			}
		}
		// always mark the shadermap dirty, because it might have gotten new asset associations
		MarkShaderMapDirty(ShaderMapIndex);
		return ShaderMapIndex;
	}

	bool LoadExistingShaderCodeLibrary(FString const& MetaDataDir)
	{
		FString IntermediateFormatPath = GetCodeArchiveFilename(MetaDataDir / TEXT("ShaderLibrarySource"), LibraryName, FormatName);
		FArchive* PrevCookedAr = IFileManager::Get().CreateFileReader(*IntermediateFormatPath);
		bool bOK = true;
		if (PrevCookedAr)
		{
			uint32 ArchiveVersion = 0;
			*PrevCookedAr << ArchiveVersion;
			if (ArchiveVersion == GShaderCodeArchiveVersion)
			{
				// Read shader library
				*PrevCookedAr << SerializedShaders;
				MarkAllShaderMapsDirty();

				ShaderCode.AddDefaulted(SerializedShaders.ShaderEntries.Num());
				for (int32 Index = 0; Index < ShaderCode.Num(); ++Index)
				{
					const FShaderCodeEntry& Entry = SerializedShaders.ShaderEntries[Index];
					TArray<uint8>& Code = ShaderCode[Index];
					check(Entry.Size > 0);
					Code.SetNumUninitialized(Entry.Size);
					PrevCookedAr->Serialize(Code.GetData(), Entry.Size);
					bOK = !PrevCookedAr->GetError();
					if (!bOK)
					{
						UE_LOG(LogShaderLibrary, Error, TEXT("Failed to deserialize shader code for %s from %s"), *SerializedShaders.ShaderHashes[Index].ToString(), *IntermediateFormatPath);
						break;
					}
				}
			}
			else
			{
				bOK = false;
				UE_LOG(LogShaderLibrary, Warning, TEXT("Failed to deserialize shader code from %s because the archive format %u is incompatible with the current version %u"), *IntermediateFormatPath, ArchiveVersion, GShaderCodeArchiveVersion);
			}
			
			PrevCookedAr->Close();
			delete PrevCookedAr;
		}
		else
		{
			bOK = false;
			UE_LOG(LogShaderLibrary, Error, TEXT("Failed to open shader code library from %s"), *IntermediateFormatPath);
		}
		
		return bOK;
	}

	void AddShaderCodeLibraryByName(const FString& BaseDir, const FString& InLibraryName)
	{
		if (FArchive* PrevCookedAr = IFileManager::Get().CreateFileReader(*GetCodeArchiveFilename(BaseDir, InLibraryName, FormatName)))
		{
			uint32 Version = 0;
			*PrevCookedAr << Version;

			if (Version == GShaderCodeArchiveVersion)
			{
				FSerializedShaderArchive PrevCookedShaders;

				*PrevCookedAr << PrevCookedShaders;

				// check if it also contains the asset info file
				if (PrevCookedShaders.LoadAssetInfo(GetShaderAssetInfoFilename(BaseDir, InLibraryName, FormatName)))
				{
					UE_LOG(LogShaderLibrary, Display, TEXT("Loaded asset info %s for the shader library %s: %d entries"),
						*GetShaderAssetInfoFilename(BaseDir, InLibraryName, FormatName),
						*GetCodeArchiveFilename(BaseDir, InLibraryName, FormatName),
						PrevCookedShaders.ShaderCodeToAssets.Num()
					);
				}
				else
				{
					UE_LOG(LogShaderLibrary, Warning, TEXT("Could not find or load asset info %s for the shader library %s"),
						*GetShaderAssetInfoFilename(BaseDir, InLibraryName, FormatName),
						*GetCodeArchiveFilename(BaseDir, InLibraryName, FormatName)
					);
				}

				int64 PrevCookedShadersCodeStart = PrevCookedAr->Tell();
				for (int32 PrevShaderMapIndex = 0; PrevShaderMapIndex < PrevCookedShaders.ShaderMapEntries.Num(); ++PrevShaderMapIndex)
				{
					AddShaderCode(PrevShaderMapIndex, PrevCookedShaders, PrevCookedShadersCodeStart, *PrevCookedAr);
				}
			}

			PrevCookedAr->Close();
			delete PrevCookedAr;
		}
	}

	void AddShaderCodeLibraryFromDirectory(const FString& BaseDir)
	{
		AddShaderCodeLibraryByName(BaseDir, LibraryName);
	}

	void AddExistingShaderCodeLibrary(FString const& OutputDir)
	{
		check(LibraryName.Len() > 0);
		static const FRegexPattern ShaderCodePattern(ShaderCodePatternStr);

		const FString FormatNameStr = FormatName.ToString();
		const FString ShaderIntermediateLocation = FPaths::ProjectSavedDir() / TEXT("Shaders") / FormatNameStr;

		TArray<FString> ShaderFiles;
		IFileManager::Get().FindFiles(ShaderFiles, *ShaderIntermediateLocation, *ShaderExtension);

		for (const FString& ShaderFileName : ShaderFiles)
		{
			FRegexMatcher FindShaderFormat(ShaderCodePattern, ShaderFileName);
			if (FindShaderFormat.FindNext())
			{
				const FString& FoundLibraryName = FindShaderFormat.GetCaptureGroup(1);
				const FString& FoundFormatName = FindShaderFormat.GetCaptureGroup(2);

				if (FoundLibraryName.StartsWith(LibraryName) && FoundFormatName.Equals(FormatNameStr, ESearchCase::IgnoreCase))
				{
					AddShaderCodeLibraryByName(OutputDir, FoundLibraryName);
				}
			}
		}
	}

	void FinishPopulate(const FString& OutputDir)
	{
		AddExistingShaderCodeLibrary(OutputDir);
	}

	bool SaveToDisk(const FString& OutputDir, const FString& MetaOutputDir, bool bSaveOnlyAssetInfo = false, TArray<FString>* OutputFilenames = nullptr)
	{
		check(LibraryName.Len() > 0);
		checkf(!bHasCopiedAndCleared, TEXT("It is not valid to call SaveToDisk on an FEditorShaderCodeArchive that has sent its shaders to another process by calling CopyAndClear."));

		bool bSuccess = IFileManager::Get().MakeDirectory(*OutputDir, true);

		auto CopyFile = [this](const FString& DestinationPath, const FString& SourcePath, TArray<FString>* OutputFilenames) -> bool
		{
			uint32 Result = IFileManager::Get().Copy(*DestinationPath, *SourcePath, true, true);
			if (Result != COPY_OK)
			{
				UE_LOG(LogShaderLibrary, Error, TEXT("FEditorShaderCodeArchive copying %s to %s failed. Failed to finalize Shared Shader Library %s with format %s"),
					*SourcePath, *DestinationPath, *LibraryName, *FormatName.ToString());
				return false;
			}

			if (OutputFilenames)
			{
				OutputFilenames->Add(DestinationPath);
			}
			return true;
		};

		// Shader library
		if (bSuccess && SerializedShaders.GetNumShaderMaps() > 0)
		{
			// Write to a intermediate file
			FString IntermediateFormatPath = GetShaderCodeFilename(FPaths::ProjectSavedDir() / TEXT("Shaders") / FormatName.ToString(), LibraryName, FormatName);
			FString AssetInfoIntermediatePath = GetShaderAssetInfoFilename(FPaths::ProjectSavedDir() / TEXT("Shaders") / FormatName.ToString(), LibraryName, FormatName);

			// save the actual shader code code
			if (!bSaveOnlyAssetInfo)
			{
				FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*IntermediateFormatPath, FILEWRITE_NoFail);
				if (FileWriter)
				{
					check(Format);

					SerializedShaders.Finalize();

					*FileWriter << GShaderCodeArchiveVersion;

					// Write shader library
					*FileWriter << SerializedShaders;
					for (auto& Code : ShaderCode)
					{
						check(!Code.IsEmpty());
						FileWriter->Serialize(Code.GetData(), Code.Num());
					}

					FileWriter->Close();
					delete FileWriter;

					// Copy to output location - support for iterative native library cooking
					if (!CopyFile(GetCodeArchiveFilename(OutputDir, LibraryName, FormatName), IntermediateFormatPath, OutputFilenames))
					{
						bSuccess = false;
					}
					else if (MetaOutputDir.Len())
					{
						if (!CopyFile(GetCodeArchiveFilename(MetaOutputDir / TEXT("../ShaderLibrarySource"), LibraryName, FormatName), IntermediateFormatPath, nullptr))
						{
							bSuccess = false;
						}
					}
				}
			}

			// save asset info
			{
				FArchive* AssetInfoWriter = IFileManager::Get().CreateFileWriter(*AssetInfoIntermediatePath, FILEWRITE_NoFail);
				if (AssetInfoWriter)
				{
					SerializedShaders.SaveAssetInfo(*AssetInfoWriter);
					AssetInfoWriter->Close();
					delete AssetInfoWriter;

					if (!CopyFile(GetShaderAssetInfoFilename(OutputDir, LibraryName, FormatName), AssetInfoIntermediatePath, nullptr))
					{
						bSuccess = false;
					}
					else if (MetaOutputDir.Len())
					{
						// copy asset info as well for debugging
						if (!CopyFile(GetShaderAssetInfoFilename(MetaOutputDir / TEXT("../ShaderLibrarySource"), LibraryName, FormatName), AssetInfoIntermediatePath, nullptr))
						{
							bSuccess = false;
						}
					}
				}
			}
		}

		return bSuccess;
	}

	bool PackageNativeShaderLibrary(const FString& ShaderCodeDir, TArray<FString>* OutputFilenames = nullptr)
	{
		if (SerializedShaders.GetNumShaders() == 0)
		{
			return true;
		}
		checkf(!bHasCopiedAndCleared, TEXT("It is not valid to call PackageNativeShaderLibrary on an FEditorShaderCodeArchive that has sent its shaders to another process by calling CopyAndClear."));

		FString IntermediateFormatPath = GetShaderDebugFolder(FPaths::ProjectSavedDir() / TEXT("Shaders") / FormatName.ToString(), LibraryName, FormatName);
		FString TempPath = IntermediateFormatPath / TEXT("NativeLibrary");

		IFileManager::Get().MakeDirectory(*TempPath, true);
		IFileManager::Get().MakeDirectory(*ShaderCodeDir, true);

		EShaderPlatform Platform = ShaderFormatToLegacyShaderPlatform(FormatName);
		const bool bOK = Format->CreateShaderArchive(LibraryName, FormatName, TempPath, ShaderCodeDir, IntermediateFormatPath, SerializedShaders, ShaderCode, OutputFilenames);

		if (bOK)
		{
			// Delete Shader code library / pipelines as we now have native versions
			// [RCL] 2020-12-02 FIXME: check if this doesn't ruin iterative cooking during Launch On
			{
				FString OutputFilePath = GetCodeArchiveFilename(ShaderCodeDir, LibraryName, FormatName);
				IFileManager::Get().Delete(*OutputFilePath);
			}
			{
				FString OutputFilePath = GetPipelinesArchiveFilename(ShaderCodeDir, LibraryName, FormatName);
				IFileManager::Get().Delete(*OutputFilePath);
			}
		}

		// Clean up the saved directory of temporary files
		IFileManager::Get().DeleteDirectory(*IntermediateFormatPath, false, true);
		IFileManager::Get().DeleteDirectory(*TempPath, false, true);

		return bOK;
	}

	void MakePatchLibrary(TArray<FEditorShaderCodeArchive*> const& OldLibraries, FEditorShaderCodeArchive const& NewLibrary)
	{
		for(int32 NewShaderMapIndex = 0; NewShaderMapIndex < NewLibrary.SerializedShaders.ShaderMapHashes.Num(); ++NewShaderMapIndex)
		{
			const FSHAHash& Hash = NewLibrary.SerializedShaders.ShaderMapHashes[NewShaderMapIndex];
			if (!HasShaderMap(Hash))
			{
				bool bInPreviousPatch = false;
				for (FEditorShaderCodeArchive const* OldLibrary : OldLibraries)
				{
					bInPreviousPatch |= OldLibrary->HasShaderMap(Hash);
					if (bInPreviousPatch)
					{
						break;
					}
				}
				if (!bInPreviousPatch)
				{
					AddShaderCode(NewShaderMapIndex, NewLibrary);
				}
			}
		}
	}
	
	static bool CreatePatchLibrary(FName FormatName, FString const& LibraryName, TArray<FString> const& OldMetaDataDirs, FString const& NewMetaDataDir, FString const& OutDir, bool bNativeFormat, bool bNeedsDeterministicOrder)
	{
		TArray<FEditorShaderCodeArchive*> OldLibraries;
		for (FString const& OldMetaDataDir : OldMetaDataDirs)
		{
			FEditorShaderCodeArchive* OldLibrary = new FEditorShaderCodeArchive(FormatName, bNeedsDeterministicOrder);
			OldLibrary->OpenLibrary(LibraryName);
			if (OldLibrary->LoadExistingShaderCodeLibrary(OldMetaDataDir))
			{
				OldLibraries.Add(OldLibrary);
			}
		}

		FEditorShaderCodeArchive NewLibrary(FormatName, bNeedsDeterministicOrder);
		NewLibrary.OpenLibrary(LibraryName);
		bool bOK = NewLibrary.LoadExistingShaderCodeLibrary(NewMetaDataDir);
		if (bOK)
		{
			FEditorShaderCodeArchive OutLibrary(FormatName, bNeedsDeterministicOrder);
			OutLibrary.OpenLibrary(LibraryName);
			OutLibrary.MakePatchLibrary(OldLibraries, NewLibrary);
			bOK = OutLibrary.SerializedShaders.GetNumShaderMaps() > 0;
			if (bOK)
			{
				FString Empty;
				OutLibrary.FinishPopulate(OutDir);
				bOK = OutLibrary.SaveToDisk(OutDir, Empty);
				UE_CLOG(!bOK, LogShaderLibrary, Error, TEXT("Failed to save %s shader patch library %s, %s, %s"), bNativeFormat ? TEXT("native") : TEXT(""), *FormatName.ToString(), *LibraryName, *OutDir);
				
				if (bOK && bNativeFormat && OutLibrary.GetFormat()->SupportsShaderArchives())
				{
					bOK = OutLibrary.PackageNativeShaderLibrary(OutDir);
					UE_CLOG(!bOK, LogShaderLibrary, Error, TEXT("Failed to package native shader patch library %s, %s, %s"), *FormatName.ToString(), *LibraryName, *OutDir);
				}
			}
			else
			{
				UE_LOG(LogShaderLibrary, Verbose, TEXT("No shaders to patch for library %s, %s, %s"), *FormatName.ToString(), *LibraryName, *OutDir);
			}
		}
		else
		{
			UE_LOG(LogShaderLibrary, Error, TEXT("Failed to open the shader library to patch against %s, %s, %s"), *FormatName.ToString(), *LibraryName, *NewMetaDataDir);
		}
		
		for (FEditorShaderCodeArchive* Lib : OldLibraries)
		{
			delete Lib;
		}
		return bOK;
	}

	void DumpStatsAndDebugInfo()
	{
		bool bUseExtendedDebugInfo = UE::ShaderLibrary::Private::GProduceExtendedStats != 0;

		UE_LOG(LogShaderLibrary, Display, TEXT(""));
		UE_LOG(LogShaderLibrary, Display, TEXT("Shader Library '%s' (%s) Stats:"), *LibraryName, *FormatName.ToString());
		UE_LOG(LogShaderLibrary, Display, TEXT("================="));

		FSerializedShaderArchive::FDebugStats Stats;
		FSerializedShaderArchive::FExtendedDebugStats ExtendedStats;
		SerializedShaders.CollectStatsAndDebugInfo(Stats, bUseExtendedDebugInfo ? &ExtendedStats : nullptr);

		UE_LOG(LogShaderLibrary, Display, TEXT("Assets: %d, Unique Shadermaps: %d (%.2f%%)"), 
			Stats.NumAssets, Stats.NumShaderMaps, (Stats.NumAssets > 0) ? 100.0 * static_cast<double>(Stats.NumShaderMaps) / static_cast<double>(Stats.NumAssets) : 0.0);
		UE_LOG(LogShaderLibrary, Display, TEXT("Total Shaders: %d, Unique Shaders: %d (%.2f%%)"), 
			Stats.NumShaders, Stats.NumUniqueShaders, (Stats.NumShaders > 0) ? 100.0 * static_cast<double>(Stats.NumUniqueShaders) / static_cast<double>(Stats.NumShaders) : 0.0);
		UE_LOG(LogShaderLibrary, Display, TEXT("Total Shader Size: %.2fmb, Unique Shaders Size: %.2fmb (%.2f%%)"), 
			FUnitConversion::Convert(static_cast<double>(Stats.ShadersSize), EUnit::Bytes, EUnit::Megabytes), 
			FUnitConversion::Convert(static_cast<double>(Stats.ShadersUniqueSize), EUnit::Bytes, EUnit::Megabytes),
			(Stats.ShadersSize > 0) ? 100.0 * static_cast<double>(Stats.ShadersUniqueSize) / static_cast<double>(Stats.ShadersSize) : 0.0
			);

		if (bUseExtendedDebugInfo)
		{
			UE_LOG(LogShaderLibrary, Display, TEXT("=== Extended info:"));
			UE_LOG(LogShaderLibrary, Display, TEXT("Minimum number of shaders in shadermap: %d"), ExtendedStats.MinNumberOfShadersPerSM);
			UE_LOG(LogShaderLibrary, Display, TEXT("Median number of shaders in shadermap: %d"), ExtendedStats.MedianNumberOfShadersPerSM);
			UE_LOG(LogShaderLibrary, Display, TEXT("Maximum number of shaders in shadermap: %d"), ExtendedStats.MaxNumberofShadersPerSM);
			if (ExtendedStats.TopShaderUsages.Num() > 0)
			{
				FString UsageString;
				UE_LOG(LogShaderLibrary, Display, TEXT("Number of shadermaps referencing top %d most shared shaders:"), ExtendedStats.TopShaderUsages.Num());
				for (int IdxUsage = 0; IdxUsage < ExtendedStats.TopShaderUsages.Num() - 1; ++IdxUsage)
				{
					UsageString += FString::Printf(TEXT("%d, "), ExtendedStats.TopShaderUsages[IdxUsage]);
				}
				UE_LOG(LogShaderLibrary, Display, TEXT("    %s%d"), *UsageString, ExtendedStats.TopShaderUsages[ExtendedStats.TopShaderUsages.Num() - 1]);

				// print per-frequency stats
				UE_LOG(LogShaderLibrary, Display, TEXT("Unique shaders itemization (sorted by compressed size):"));
				// sort by compressed size
				TArray<int32> SortedIndices;
				for (int32 IdxFreq = 0; IdxFreq < SF_NumFrequencies; ++IdxFreq)
				{
					SortedIndices.Add(IdxFreq);
				}
				SortedIndices.StableSort([&ExtendedStats](int32 IndexA, int32 IndexB) { return ExtendedStats.CompressedSizePerFrequency[IndexA] >= ExtendedStats.CompressedSizePerFrequency[IndexB]; });
				for (int32 Freq : SortedIndices)
				{
					if (ExtendedStats.NumShadersPerFrequency[Freq] > 0)
					{
						UE_LOG(LogShaderLibrary, Display, TEXT("%s: %d shaders (%.2f%%), compressed size: %.2f MB (%.2f KB avg per shader), uncompressed size: %.2f MB (%.2f KB avg per shader)"),
							GetShaderFrequencyString(static_cast<EShaderFrequency>(Freq)),
							ExtendedStats.NumShadersPerFrequency[Freq],
							100.0 * ExtendedStats.NumShadersPerFrequency[Freq] / double(Stats.NumUniqueShaders),
							double(ExtendedStats.CompressedSizePerFrequency[Freq]) / (1024.0 * 1024.0), double(ExtendedStats.CompressedSizePerFrequency[Freq]) / (double(ExtendedStats.NumShadersPerFrequency[Freq]) * 1024.0),
							double(ExtendedStats.UncompressedSizePerFrequency[Freq]) / (1024.0 * 1024.0), double(ExtendedStats.UncompressedSizePerFrequency[Freq]) / (double(ExtendedStats.NumShadersPerFrequency[Freq]) * 1024.0)
						);
					}
				}

			}
			else
			{
				UE_LOG(LogShaderLibrary, Display, TEXT("No shader usage info is provided"));
			}

			FString DebugLibFolder = GetShaderDebugFolder(FPaths::ProjectSavedDir() / TEXT("Shaders") / FormatName.ToString(), LibraryName, FormatName);
			IFileManager::Get().MakeDirectory(*DebugLibFolder, true);

			{
				FString DumpFile = DebugLibFolder / TEXT("Dump.txt");
				TUniquePtr<FArchive> DumpAr(IFileManager::Get().CreateFileWriter(*DumpFile));
				FTCHARToUTF8 Converter(*ExtendedStats.TextualRepresentation);
				DumpAr->Serialize(const_cast<UTF8CHAR*>(reinterpret_cast<const UTF8CHAR*>(Converter.Get())), Converter.Length());
				UE_LOG(LogShaderLibrary, Display, TEXT("Textual dump saved to '%s'"), *DumpFile);
			}
#if 0 // creating a graphviz graph - maybe one day we'll return to this
			FString DebugGraphFolder = GetShaderDebugFolder(FPaths::ProjectSavedDir() / TEXT("Shaders") / FormatName.ToString(), LibraryName, FormatName);
			FString DebugGraphFile = DebugGraphFolder / TEXT("RelationshipGraph.gv");

			IFileManager::Get().MakeDirectory(*DebugGraphFolder, true);
			TUniquePtr<FArchive> GraphVizAr(IFileManager::Get().CreateFileWriter(*DebugGraphFile));

			TAnsiStringBuilder<512> LineBuffer;
			LineBuffer << "digraph ShaderLibrary {\n";
			GraphVizAr->Serialize(const_cast<ANSICHAR*>(LineBuffer.ToString()), LineBuffer.Len() * sizeof(ANSICHAR));
			for (TTuple<FString, FString>& Edge : RelationshipGraph)
			{
				LineBuffer.Reset();
				LineBuffer << "\t \"";
				LineBuffer << TCHAR_TO_UTF8(*Edge.Key);
				LineBuffer << "\" -> \"";
				LineBuffer << TCHAR_TO_UTF8(*Edge.Value);
				LineBuffer << "\";\n";
				GraphVizAr->Serialize(const_cast<ANSICHAR*>(LineBuffer.ToString()), LineBuffer.Len() * sizeof(ANSICHAR));
			}
			LineBuffer.Reset();
			LineBuffer << "}\n";
			GraphVizAr->Serialize(const_cast<ANSICHAR*>(LineBuffer.ToString()), LineBuffer.Len() * sizeof(ANSICHAR));
#endif//
		}

		UE_LOG(LogShaderLibrary, Display, TEXT("================="));
	}

private:
	FName FormatName;
	FString LibraryName;
	FSerializedShaderArchive SerializedShaders;
	/**
	 * The element at index N holds the ShaderCode for the element of SerializedShaders.ShaderEntries at index N.
	 * In MultiprocessCooking elements can be empty if they have been transferred to the Director (bHasCopiedAndCleared will be true in this case).
	 */
	TArray<TArray<uint8>> ShaderCode;
	/** A list of ShaderMaps that have not yet been copied to the CookDirector. Used only by MultiprocessCookWorkers. */
	TSet<int32> ShaderMapsToCopy;
	/** True if CopyToArchiveAndClear has been called, otherwise false. If false we avoid doing some tracking since it might never be used. */
	bool bHasCopiedAndCleared = false;

	const IShaderFormat* Format;
	bool bNeedsDeterministicOrder;
};

struct FEditorShaderStableInfo
{
	FEditorShaderStableInfo(FName InFormat)
		: FormatName(InFormat)
	{
	}

	void OpenLibrary(FString const& Name)
	{
		check(LibraryName.Len() == 0);
		check(Name.Len() > 0);
		LibraryName = Name;
		StableMap.Empty();
		StableMapsToCopy.Empty();
		bHasCopied = false;
	}

	void CloseLibrary(FString const& Name)
	{
		check(LibraryName == Name);
		LibraryName = TEXT("");
	}

	enum class EMergeRule
	{
		/** If the key already exists, do not modify it, keep the existing value. */
		KeepExisting,
		/**
		 * If the key already exists, compare whether the output hash is different. If different,
		 * log a warning and keep the existing value. If the same, overwrite the existing value
		 * with the new value. 
		 */
		OverwriteUnmodifiedWarnModified,
	};
	void AddShader(FStableShaderKeyAndValue& StableKeyValue, EMergeRule MergeRule)
	{
		FStableShaderKeyAndValue* Existing = StableMap.Find(StableKeyValue);
		if (Existing)
		{
			switch (MergeRule)
			{
			case EMergeRule::KeepExisting:
				return;
			case EMergeRule::OverwriteUnmodifiedWarnModified:
				if (Existing->OutputHash != StableKeyValue.OutputHash)
				{
					UE_LOG(LogShaderLibrary, Warning, TEXT("Duplicate key in stable shader library, but different output, skipping new item:"));
					UE_LOG(LogShaderLibrary, Warning, TEXT("    Existing: %s"), *Existing->ToString());
					UE_LOG(LogShaderLibrary, Warning, TEXT("    New     : %s"), *StableKeyValue.ToString());
					return;
				}
				break; // Otherwise fall through to overwrite
			default:
				checkNoEntry();
				break;
			}
		}
		StableMap.Add(StableKeyValue);
		MarkKeyValueDirty(StableKeyValue);
	}

	void AddShaderCodeLibraryFromDirectory(const FString& BaseDir, EMergeRule MergeRule)
	{
		TArray<FStableShaderKeyAndValue> StableKeys;
		if (UE::PipelineCacheUtilities::LoadStableKeysFile(GetStableInfoArchiveFilename(BaseDir, LibraryName, FormatName), StableKeys))
		{
			for (FStableShaderKeyAndValue& Item : StableKeys)
			{
				AddShader(Item, MergeRule);
			}
		}
	}

	void AddExistingShaderCodeLibrary(FString const& OutputDir)
	{
		check(LibraryName.Len() > 0);

		bool bLibraryExistsInSavedShadersDir = false;
		{
			const FString ShaderIntermediateLocation = FPaths::ProjectSavedDir() / TEXT("Shaders") / FormatName.ToString();
			TArray<FString> ShaderFiles;
			IFileManager::Get().FindFiles(ShaderFiles, *ShaderIntermediateLocation, *StableExtension);
			FString ExpectedFileNameText = LibraryName + TEXT("-") + FormatName.ToString() + TEXT(".");

			for (const FString& ShaderFileName : ShaderFiles)
			{
				if (ShaderFileName.Contains(ExpectedFileNameText))
				{
					bLibraryExistsInSavedShadersDir = true;
					break;
				}
			}
		}
		if (bLibraryExistsInSavedShadersDir)
		{
			AddShaderCodeLibraryFromDirectory(OutputDir, EMergeRule::KeepExisting);
		}
	}

	void FinishPopulate(FString const& OutputDir)
	{
		AddExistingShaderCodeLibrary(OutputDir);
	}

	bool SaveToDisk(FString const& OutputDir, FString& OutSCLCSVPath)
	{
		check(LibraryName.Len() > 0);
		OutSCLCSVPath = FString();

		if (StableMap.IsEmpty())
		{
			// Do not touch the disk if empty, but also don't consider this a failure.
			// It is entirely possible that during the cook no assets with shaders were cooked.
			return true;
		}

		bool bSuccess = IFileManager::Get().MakeDirectory(*OutputDir, true);

		EShaderPlatform Platform = ShaderFormatToLegacyShaderPlatform(FormatName);

		// Shader library
		if (bSuccess)
		{
			// Write to a intermediate file
			FString IntermediateFormatPath = GetStableInfoArchiveFilename(FPaths::ProjectSavedDir() / TEXT("Shaders") / FormatName.ToString(), LibraryName, FormatName);

			// Write directly to the file
			{
				if (!UE::PipelineCacheUtilities::SaveStableKeysFile(IntermediateFormatPath, StableMap))
				{
					UE_LOG(LogShaderLibrary, Error, TEXT("Could not save stable map to file '%s'"), *IntermediateFormatPath);
				}

				// check that it works in a Debug build pm;u
				if (UE_BUILD_DEBUG)
				{
					TArray<FStableShaderKeyAndValue> LoadedBack;
					if (!UE::PipelineCacheUtilities::LoadStableKeysFile(IntermediateFormatPath, LoadedBack))
					{
						UE_LOG(LogShaderLibrary, Error, TEXT("Saved stable map could not be loaded back (from file '%s')"), *IntermediateFormatPath);
					}
					else
					{
						if (LoadedBack.Num() != StableMap.Num())
						{
							UE_LOG(LogShaderLibrary, Error, TEXT("Loaded stable map has a different number of entries (%d) than a saved one (%d)"), LoadedBack.Num(), StableMap.Num());
						}
						else
						{
							for (FStableShaderKeyAndValue& Value : LoadedBack)
							{
								Value.ComputeKeyHash();
								if (!StableMap.Contains(Value))
								{
									UE_LOG(LogShaderLibrary, Error, TEXT("Loaded stable map has an entry that is not present in the saved one"));
									UE_LOG(LogShaderLibrary, Error, TEXT("  %s"), *Value.ToString());
								}
							}
						}
					}
				}
			}

			// Only the primary cooker needs to write to the output directory, child cookers only write to the Saved directory
			FString OutputFilePath = GetStableInfoArchiveFilename(OutputDir, LibraryName, FormatName);

			// Copy to output location - support for iterative native library cooking
			uint32 Result = IFileManager::Get().Copy(*OutputFilePath, *IntermediateFormatPath, true, true);
			if (Result == COPY_OK)
			{
				OutSCLCSVPath = OutputFilePath;
			}
			else
			{
				UE_LOG(LogShaderLibrary, Error, TEXT("FEditorShaderStableInfo copy failed to %s. Failed to finalize Shared Shader Library %s with format %s"), *OutputFilePath, *LibraryName, *FormatName.ToString());
				bSuccess = false;
			}
		}

		return bSuccess;
	}

	void MarkKeyValueDirty(const FStableShaderKeyAndValue& StableKeyValue)
	{
		if (bHasCopied)
		{
			StableMapsToCopy.Add(StableKeyValue);
		}
	}

	bool HasDataToCopy() const
	{
		// After the first copy dirty elements are added to StableMapsToCopy; during the first copy every element from StableMap is copied
		return bHasCopied ? !StableMapsToCopy.IsEmpty() : !StableMap.IsEmpty();
	}

	void CopyToCompactBinary(FCbWriter& Writer)
	{
		if (!bHasCopied)
		{
			// First Copy adds all StableMaps to StableMapsToCopy, because as an optimization we do not write to
			// StableMapsToCopy until the first Copy call.
			StableMapsToCopy = StableMap;
		}

		// Deduplicate the hashes; there will be a significant number (up to 50%) of duplicate hashes.
		TArray<FSHAHash> Hashes;
		TMap<FSHAHash, int32> HashToIndex;
		auto IndexHash = [&Hashes, &HashToIndex](const FSHAHash& Hash)
		{
			if (HashToIndex.Find(Hash) == nullptr)
			{
				Hashes.Add(Hash);
				HashToIndex.Add(Hash, Hashes.Num() - 1);
			}
		};
		for (const FStableShaderKeyAndValue& StableInfo : StableMapsToCopy)
		{
			IndexHash(StableInfo.PipelineHash);
			IndexHash(StableInfo.OutputHash);
		}

		Writer.BeginObject();
		{
			Writer << "Hashes" << Hashes;
			Writer.BeginArray("StableShaderKeyAndValues");
			for (const FStableShaderKeyAndValue& StableInfo : StableMapsToCopy)
			{
				WriteToCompactBinary(Writer, StableInfo, HashToIndex);
			}
			Writer.EndArray();
		}
		Writer.EndObject();

		StableMapsToCopy.Empty();
		bHasCopied = true;
	}

	bool AppendFromCompactBinary(FCbFieldView Field)
	{
		TArray<FSHAHash> Hashes;
		if (!LoadFromCompactBinary(Field["Hashes"], Hashes))
		{
			// Can't load the StableShaderKeyAndValues if the hashes for them are missing
			return false;
		}
		FCbFieldView StableShaderKeyAndValuesField = Field["StableShaderKeyAndValues"];
		int32 NumInfos = StableShaderKeyAndValuesField.AsArrayView().Num();
		bool bOk = !StableShaderKeyAndValuesField.HasError();
		for (FCbFieldView InfoView : StableShaderKeyAndValuesField)
		{
			FStableShaderKeyAndValue StableInfo;
			if (LoadFromCompactBinary(InfoView, StableInfo, Hashes))
			{
				AddShader(StableInfo, EMergeRule::OverwriteUnmodifiedWarnModified);
			}
			else
			{
				bOk = false;
			}
		}
		return bOk;
	}

private:
	FName FormatName;
	FString LibraryName;
	TSet<FStableShaderKeyAndValue> StableMap;
	/** Copies of elements in StableMap that have not yet been copied to the CookDirector. Used only by MultiprocessCookWorkers. */
	TSet<FStableShaderKeyAndValue> StableMapsToCopy;
	/** True if CopyToCompactBinary has been called, otherwise false. If false we avoid doing some tracking since it might never be used. */
	bool bHasCopied = false;
};
#endif //WITH_EDITOR

class FShaderLibrariesCollection
{
	/** At runtime, this is set to the valid shader platform in use. At cook time, this value is SP_NumPlatforms. */
	EShaderPlatform ShaderPlatform;

	/** At runtime, shader code collection for current shader platform */
	TMap<FString, TUniquePtr<UE::ShaderLibrary::Private::FNamedShaderLibrary>> NamedLibrariesStack;

	/** Mutex that guards the access to the above stack. */
	FRWLock NamedLibrariesMutex;

#if UE_SHADERLIB_WITH_INTROSPECTION
	IConsoleObject* DumpLibraryContentsCmd;
#endif

#if WITH_EDITOR
	FCriticalSection ShaderCodeCS;
	// At cook time, shader code collection for each shader platform
	FEditorShaderCodeArchive* EditorShaderCodeArchive[EShaderPlatform::SP_NumPlatforms];
	// At cook time, whether we saved the shader code archive via SaveShaderLibraryChunk, so we can avoid saving it again in the end.
	// [RCL] FIXME 2020-11-25: this tracking is not perfect as the code in the asset registry performs chunking by ITargetPlatform, whereas if two platforms
	// share the same shader format (e.g. Vulkan on Linux and Windows), we cannot make such a distinction. However, as of now it is a very hypothetical case 
	// as the project settings don't allow disabling chunking for a particular platform.
	TSet<int32> ChunksSaved[EShaderPlatform::SP_NumPlatforms];
	// At cook time, shader code collection for each shader platform
	FEditorShaderStableInfo* EditorShaderStableInfo[EShaderPlatform::SP_NumPlatforms];
	// Cached bit field for shader formats that require stable keys
	TBitArray<> ShaderFormatsThatNeedStableKeys;
	// At cook time, shader stats for each shader platform
	FShaderCodeStats EditorShaderCodeStats[EShaderPlatform::SP_NumPlatforms];
	// At cook time, whether the shader archive supports pipelines (only OpenGL should)
	bool EditorArchivePipelines[EShaderPlatform::SP_NumPlatforms];
	bool bIsEditorShaderCodeArchiveEmpty = true;
	bool bIsEditorShaderStableInfoEmpty = true;
#endif //WITH_EDITOR
	bool bSupportsPipelines;
	bool bNativeFormat;

	/** This function only exists because I'm not able yet to untangle editor and non-editor usage (or rather cooking and not cooking). */
	inline bool IsLibraryInitializedForRuntime() const
	{
#if WITH_EDITOR
		return ShaderPlatform != SP_NumPlatforms;
#else
		// to make it a faster check, for games assume this function is no-op
		checkf(ShaderPlatform != SP_NumPlatforms, TEXT("Shader library has not been properly initialized for a cooked game"));
		return true;
#endif
	}

public:
	static FShaderLibrariesCollection* Impl;

	FShaderLibrariesCollection(EShaderPlatform InShaderPlatform, bool bInNativeFormat)
		: ShaderPlatform(InShaderPlatform)
#if UE_SHADERLIB_WITH_INTROSPECTION
		, DumpLibraryContentsCmd(nullptr)
#endif
		, bSupportsPipelines(false)
		, bNativeFormat(bInNativeFormat)
	{
#if WITH_EDITOR
		ShaderFormatsThatNeedStableKeys.Init(false, EShaderPlatform::SP_NumPlatforms);
		FMemory::Memzero(EditorShaderCodeArchive);
		FMemory::Memzero(EditorShaderStableInfo);
		FMemory::Memzero(EditorShaderCodeStats);
		FMemory::Memzero(EditorArchivePipelines);
		for (int32 Idx = 0; Idx < UE_ARRAY_COUNT(ChunksSaved); ++Idx)
		{
			ChunksSaved[Idx].Empty();
		}
#endif

#if UE_SHADERLIB_WITH_INTROSPECTION
		DumpLibraryContentsCmd = IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("r.ShaderLibrary.Dump"),
			TEXT("Dumps shader library map."),
			FConsoleCommandDelegate::CreateStatic(DumpLibraryContentsStatic),
			ECVF_Default
			);
#endif

		FCoreDelegates::PreloadPackageShaderMaps.BindRaw(this, &FShaderLibrariesCollection::PreloadPackageShaderMapsDelegate);
		FCoreDelegates::ReleasePreloadedPackageShaderMaps.BindRaw(this, &FShaderLibrariesCollection::ReleasePreloadedPackageShaderMapsDelegate);
	}

	~FShaderLibrariesCollection()
	{
		FCoreDelegates::PreloadPackageShaderMaps.Unbind();
		FCoreDelegates::ReleasePreloadedPackageShaderMaps.Unbind();

#if WITH_EDITOR
		for (uint32 i = 0; i < EShaderPlatform::SP_NumPlatforms; i++)
		{
			if (EditorShaderCodeArchive[i])
			{
				delete EditorShaderCodeArchive[i];
			}
			if (EditorShaderStableInfo[i])
			{
				delete EditorShaderStableInfo[i];
			}
		}
		FMemory::Memzero(EditorShaderCodeArchive);
		FMemory::Memzero(EditorShaderStableInfo);
#endif

#if UE_SHADERLIB_WITH_INTROSPECTION
		if (DumpLibraryContentsCmd)
		{
			IConsoleManager::Get().UnregisterConsoleObject(DumpLibraryContentsCmd);
		}
#endif
	}

	bool OpenLibrary(FString const& Name, FString const& Directory, const bool bMonolithicOnly = false)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(OpenShaderLibrary);

		using namespace UE::ShaderLibrary::Private;

		bool bResult = false;

		TSet<int32> NewComponentIDs;
		if (IsLibraryInitializedForRuntime())
		{
			LLM_SCOPE(ELLMTag::Shaders);
			bool bAddNewNamedLibrary = false;
			FNamedShaderLibrary* Library = nullptr;
			{
				// scope of this lock should be as limited as possible - particularly, OpenShaderCode will start async work which is not a good idea to do while holding a lock
				FRWScopeLock WriteLock(NamedLibrariesMutex, SLT_Write);

				// create a named library if one didn't exist
				TUniquePtr<FNamedShaderLibrary>* LibraryPtr = NamedLibrariesStack.Find(Name);
				if (LIKELY(LibraryPtr))
				{
					Library = LibraryPtr->Get();
				}
				else
				{
					bAddNewNamedLibrary = true;
					Library = new FNamedShaderLibrary(Name, ShaderPlatform, Directory);
				}
			}
			check(Library);
			// note, that since we're out of NamedLibrariesMutex locks now, other threads may arrive at the same point and acquire the same Library pointer
			// (or create yet another new named library). In the latter case, the duplicate library will be deleted later, since we will re-check (under a lock)
			// the presence of the same name in NamedLibrariesStack.  In the former case (two threads sharing the same Library pointer), we rely on FNamedShaderLibrary::OpenShaderCode
			// implementation being thread-safe (which it is).

			// more info for better logging
			bool bOpenedAsMonolithic = false;

			// if we're able to open the library by name, it's not chunked
			if (Library->OpenShaderCode(Directory, Name))
			{
				bResult = true;
				bOpenedAsMonolithic = true;
			}
			else if (!bMonolithicOnly) // attempt to open a chunked library
			{
				TSet<int32> PrevComponentSet = Library->PresentChunks;
				{
					FScopeLock KnownPakFilesLocker(&FMountedPakFileInfo::KnownPakFilesAccessLock);
					for (TSet<FMountedPakFileInfo>::TConstIterator Iter(FMountedPakFileInfo::KnownPakFiles); Iter; ++Iter)
					{
						Library->OnPakFileMounted(*Iter);
					}
				}
				NewComponentIDs = Library->PresentChunks.Difference(PrevComponentSet);
				bResult = !NewComponentIDs.IsEmpty();

#if UE_SHADERLIB_SUPPORT_CHUNK_DISCOVERY
				if (!bResult)
				{
					// Some deployment flows (e.g. Launch on) avoid pak files despite project packaging settings. 
					// In case we run under such circumstances, we need to discover the components ourselves
					if (!IsRunningWithPakFile() && ShouldLookForLooseCookedChunks())
					{
						UE_LOG(LogShaderLibrary, Display, TEXT("Running without a pakfile/IoStore and did not find a monolithic library '%s' - attempting disk search for its chunks"), *Name);

						TArray<FString> UshaderbytecodeFiles;
						FString SearchMask = Directory / FString::Printf(TEXT("ShaderArchive-*%s*.ushaderbytecode"), *Name);
						IFileManager::Get().FindFiles(UshaderbytecodeFiles, *SearchMask, true, false);

						if (UshaderbytecodeFiles.Num() > 0)
						{
							UE_LOG(LogShaderLibrary, Display, TEXT("   ....  found %d files"), UshaderbytecodeFiles.Num());
							for (const FString& Filename : UshaderbytecodeFiles)
							{
								const TCHAR* ChunkSubstring = TEXT("_Chunk");
								const int kChunkSubstringSize = 6; // stlren(ChunkSubstring)
								int32 ChunkSuffix = Filename.Find(ChunkSubstring, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
								if (ChunkSuffix != INDEX_NONE && ChunkSuffix + kChunkSubstringSize < Filename.Len())
								{
									const TCHAR* ChunkIDString = &Filename[ChunkSuffix + kChunkSubstringSize];
									int32 ChunkID = FCString::Atoi(ChunkIDString);
									if (ChunkID >= 0)
									{
										// create a fake FPakFileMountedInfo
										FMountedPakFileInfo PakFileInfo(Directory, ChunkID);
										Library->OnPakFileMounted(PakFileInfo);
									}
								}
							}

							NewComponentIDs = Library->PresentChunks.Difference(PrevComponentSet);
							bResult = !NewComponentIDs.IsEmpty();
						}
						else
						{
							UE_LOG(LogShaderLibrary, Display, TEXT("   ....  not found"));
						}
					}
				}
#endif // UE_SHADERLIB_SUPPORT_CHUNK_DISCOVERY
			}

			if (bResult)
			{
				if (bAddNewNamedLibrary)
				{
					// re-check that the library indeed was added right now - we can have multiple threads race to create it as described above
					FRWScopeLock WriteLock(NamedLibrariesMutex, SLT_Write);
					TUniquePtr<FNamedShaderLibrary>* LibraryPtr = NamedLibrariesStack.Find(Name);
					if (LibraryPtr == nullptr)
					{
						if (bOpenedAsMonolithic)
						{
							UE_LOG(LogShaderLibrary, Display, TEXT("Logical shader library '%s' has been created as a monolithic library"), *Name, Library->GetNumComponents());
						}
						else
						{
							UE_LOG(LogShaderLibrary, Display, TEXT("Logical shader library '%s' has been created, components %d"), *Name, Library->GetNumComponents());
						}
						NamedLibrariesStack.Emplace(Name, Library);
					}
					else 
					{
						// this is where concurrent work from thread(s) that lost the race to create the same library gets wasted.
						delete Library;
						Library = nullptr;
					}
				}
				else
				{
					UE_LOG(LogShaderLibrary, Display, TEXT("Discovered new %d components for logical shader library '%s' (total number of components is now %d)"), NewComponentIDs.Num(), *Name, Library->GetNumComponents());
				}

				// Inform the pipeline cache that the state of loaded libraries has changed (unless we had to delete the duplicate)
				if (Library != nullptr)
				{
					FShaderPipelineCache::ShaderLibraryStateChanged(FShaderPipelineCache::Opened, ShaderPlatform, Name, INDEX_NONE);
					for (int32 ComponentID : NewComponentIDs)
					{
						FShaderPipelineCache::ShaderLibraryStateChanged(FShaderPipelineCache::OpenedComponent, ShaderPlatform, Name, ComponentID);
					}
				}
			}
			else
			{
				if (bAddNewNamedLibrary)
				{
					UE_LOG(LogShaderLibrary, Display, TEXT("Tried to open shader library '%s', but could not find it%s"), *Name, 
						bMonolithicOnly ? TEXT(" (only tried to open it as a monolithic library).") : TEXT(" neither as a monolithic library nor as a chunked one."));

					check(Library->GetNumComponents() == 0);
					delete Library;
					Library = nullptr;
				}
				else
				{
					UE_LOG(LogShaderLibrary, Display, TEXT("Tried to open again shader library '%s', but could not find new components for it (existing components: %d)."), *Name, Library->GetNumComponents());
				}
			}
		}

#if WITH_EDITOR
		if (!bIsEditorShaderCodeArchiveEmpty)
		{
			for (uint32 i = 0; i < EShaderPlatform::SP_NumPlatforms; i++)
			{
				FEditorShaderCodeArchive* CodeArchive = EditorShaderCodeArchive[i];
				if (CodeArchive)
				{
					CodeArchive->OpenLibrary(Name);
				}
			}
		}
		if (!bIsEditorShaderStableInfoEmpty)
		{
			for (uint32 i = 0; i < EShaderPlatform::SP_NumPlatforms; i++)
			{
				FEditorShaderStableInfo* StableArchive = EditorShaderStableInfo[i];
				if (StableArchive)
				{
					StableArchive->OpenLibrary(Name);
				}
			}
		}
#endif
		
		return bResult;
	}

	void CloseLibrary(FString const& Name)
	{
		if (IsLibraryInitializedForRuntime())
		{
			TUniquePtr<UE::ShaderLibrary::Private::FNamedShaderLibrary> RemovedLibrary = nullptr;

			{
				FRWScopeLock WriteLock(NamedLibrariesMutex, SLT_Write);
				NamedLibrariesStack.RemoveAndCopyValue(Name, RemovedLibrary);
			}

			if (RemovedLibrary)
			{
				UE_LOG(LogShaderLibrary, Display, TEXT("Closing logical shader library '%s' with %d components"), *Name, RemovedLibrary->GetNumComponents());
				RemovedLibrary = nullptr;
			}
		}

		// Inform the pipeline cache that the state of loaded libraries has changed
		FShaderPipelineCache::ShaderLibraryStateChanged(FShaderPipelineCache::Closed, ShaderPlatform, Name, -1);

#if WITH_EDITOR
		for (uint32 i = 0; i < EShaderPlatform::SP_NumPlatforms; i++)
		{
			if (EditorShaderCodeArchive[i])
			{
				EditorShaderCodeArchive[i]->CloseLibrary(Name);
			}
			if (EditorShaderStableInfo[i])
			{
				EditorShaderStableInfo[i]->CloseLibrary(Name);
			}
			ChunksSaved[i].Empty();
		}
#endif
	}

	void OnPakFileMounted(const UE::ShaderLibrary::Private::FMountedPakFileInfo& MountInfo)
	{		
		if (IsLibraryInitializedForRuntime())
		{
			TArray<TUniqueFunction<void()>> ShaderLibraryStateChanges;
			{
				FRWScopeLock WriteLock(NamedLibrariesMutex, SLT_Write);
				for (TTuple<FString, TUniquePtr<UE::ShaderLibrary::Private::FNamedShaderLibrary>>& NamedLibraryPair : NamedLibrariesStack)
				{
					TSet<int32> PrevComponentSet = NamedLibraryPair.Value->PresentChunks;
					NamedLibraryPair.Value->OnPakFileMounted(MountInfo);
					for (int32 ComponentID : NamedLibraryPair.Value->PresentChunks.Difference(PrevComponentSet))
					{
						// Defer these for outside of the lock, ShaderPipelineCache may want to inspect shader library.
						ShaderLibraryStateChanges.Add([ShaderPlatform = NamedLibraryPair.Value->ShaderPlatform, LogicalName = NamedLibraryPair.Value->LogicalName, ComponentID]() {
							FShaderPipelineCache::ShaderLibraryStateChanged(FShaderPipelineCache::OpenedComponent, ShaderPlatform, LogicalName, ComponentID);
						});
					}
				}
			}
			for (TUniqueFunction<void()>& OnShaderLibraryStateChange : ShaderLibraryStateChanges)
			{
				OnShaderLibraryStateChange();
			}
		}
	}

	uint32 GetShaderCount(void)
	{
		FRWScopeLock ReadLock(NamedLibrariesMutex, SLT_ReadOnly);

		int32 ShaderCount = 0;
		for (TTuple<FString, TUniquePtr<UE::ShaderLibrary::Private::FNamedShaderLibrary>>& NamedLibraryPair : NamedLibrariesStack)
		{
			ShaderCount += NamedLibraryPair.Value->GetShaderCount();
		}
		return ShaderCount;
	}

#if UE_SHADERLIB_WITH_INTROSPECTION
	static void DumpLibraryContentsStatic()
	{
		if (FShaderLibrariesCollection::Impl)
		{
			FShaderLibrariesCollection::Impl->DumpLibraryContents();
		}
	}

	void DumpLibraryContents()
	{
		FRWScopeLock ReadLock(NamedLibrariesMutex, SLT_ReadOnly);

		UE_LOG(LogShaderLibrary, Display, TEXT("==== Dumping shader library contents ===="));
		UE_LOG(LogShaderLibrary, Display, TEXT("Shader platform (EShaderPlatform) is %d"), static_cast<int32>(ShaderPlatform));
		UE_LOG(LogShaderLibrary, Display, TEXT("%d named libraries open with %d shaders total"), NamedLibrariesStack.Num(), GetShaderCount());
		int32 LibraryIdx = 0;
		for (TTuple<FString, TUniquePtr<UE::ShaderLibrary::Private::FNamedShaderLibrary>>& NamedLibraryPair : NamedLibrariesStack)
		{
			UE_LOG(LogShaderLibrary, Display, TEXT("%d: Name='%s' Shaders %d Components %d"), 
				LibraryIdx, *NamedLibraryPair.Key, NamedLibraryPair.Value->GetShaderCount(), NamedLibraryPair.Value->GetNumComponents());

			NamedLibraryPair.Value->DumpLibraryContents(TEXT("  "));

			++LibraryIdx;
		}
		UE_LOG(LogShaderLibrary, Display, TEXT("==== End of shader library dump ===="));
	}
#endif

	EShaderPlatform GetRuntimeShaderPlatform(void)
	{
		return ShaderPlatform;
	}

	FShaderLibraryInstance* FindShaderLibraryForShaderMap(const FSHAHash& Hash, int32& OutShaderMapIndex)
	{
		FRWScopeLock ReadLock(NamedLibrariesMutex, SLT_ReadOnly);

		for (TTuple<FString, TUniquePtr<UE::ShaderLibrary::Private::FNamedShaderLibrary>>& NamedLibraryPair : NamedLibrariesStack)
		{
			FShaderLibraryInstance* Instance = NamedLibraryPair.Value->FindShaderLibraryForShaderMap(Hash, OutShaderMapIndex);
			if (Instance)
			{
				return Instance;
			}
		}
		return nullptr;
	}

	FShaderLibraryInstance* FindShaderLibraryForShader(const FSHAHash& Hash, int32& OutShaderIndex)
	{
		FRWScopeLock ReadLock(NamedLibrariesMutex, SLT_ReadOnly);

		for (TTuple<FString, TUniquePtr<UE::ShaderLibrary::Private::FNamedShaderLibrary>>& NamedLibraryPair : NamedLibrariesStack)
		{
			FShaderLibraryInstance* Instance = NamedLibraryPair.Value->FindShaderLibraryForShader(Hash, OutShaderIndex);
			if (Instance)
			{
				return Instance;
			}
		}
		return nullptr;
	}

	TRefCountPtr<FShaderMapResource_SharedCode> LoadResource(const FSHAHash& Hash, FArchive* Ar)
	{
		int32 ShaderMapIndex = INDEX_NONE;
		FShaderLibraryInstance* LibraryInstance = FindShaderLibraryForShaderMap(Hash, ShaderMapIndex);
		if (LibraryInstance)
		{
			SCOPED_LOADTIMER(LoadShaderResource_Internal);

			TRefCountPtr<FShaderMapResource_SharedCode> Resource = LibraryInstance->GetResource(ShaderMapIndex);
			if (!Resource)
			{
				SCOPED_LOADTIMER(LoadShaderResource_AddOrDeleteResource);
				Resource = LibraryInstance->AddOrDeleteResource(new FShaderMapResource_SharedCode(LibraryInstance, ShaderMapIndex), Ar);
			}

			return Resource;
		}

		return TRefCountPtr<FShaderMapResource_SharedCode>();
	}

	TRefCountPtr<FRHIShader> CreateShader(EShaderFrequency Frequency, const FSHAHash& Hash)
	{
		int32 ShaderIndex = INDEX_NONE;
		FShaderLibraryInstance* LibraryInstance = FindShaderLibraryForShader(Hash, ShaderIndex);
		if (LibraryInstance)
		{
			TRefCountPtr<FRHIShader> Shader = LibraryInstance->GetOrCreateShader(ShaderIndex);
			check(Shader->GetFrequency() == Frequency);
			return Shader;
		}
		return TRefCountPtr<FRHIShader>();
	}

	bool PreloadShader(const FSHAHash& Hash, FArchive* Ar)
	{
		int32 ShaderIndex = INDEX_NONE;
		FShaderLibraryInstance* LibraryInstance = FindShaderLibraryForShader(Hash, ShaderIndex);
		if (LibraryInstance)
		{
			LibraryInstance->PreloadShader(ShaderIndex, Ar);
			return true;
		}
		return false;
	}

	bool ReleasePreloadedShader(const FSHAHash& Hash)
	{
		int32 ShaderIndex = INDEX_NONE;
		FShaderLibraryInstance* LibraryInstance = FindShaderLibraryForShader(Hash, ShaderIndex);
		if (LibraryInstance)
		{
			LibraryInstance->ReleasePreloadedShader(ShaderIndex);
			return true;
		}
		return false;
	}

	bool ContainsShaderCode(const FSHAHash& Hash)
	{
		int32 ShaderIndex = INDEX_NONE;
		FShaderLibraryInstance* LibraryInstance = FindShaderLibraryForShader(Hash, ShaderIndex);
		return LibraryInstance != nullptr;
	}
	
	bool ContainsShaderCode(const FSHAHash& Hash, const FString& LogicalLibraryName)
	{
		int32 ShaderIndex = INDEX_NONE;
		FShaderLibraryInstance* LibraryInstance = FindShaderLibraryForShader(Hash, ShaderIndex);
		return LibraryInstance != nullptr && LibraryInstance->Library->GetName() == LogicalLibraryName;
	}

#if WITH_EDITOR

	FString GetFormatAndPlatformName(const FName& Format)
	{
		EShaderPlatform Platform = ShaderFormatToLegacyShaderPlatform(Format);
		FName PossiblyAdjustedFormat = LegacyShaderPlatformToShaderFormat(Platform);	// Vulkan and GL switch between name variants depending on CVars 

		return PossiblyAdjustedFormat.ToString() + TEXT("-") + FDataDrivenShaderPlatformInfo::GetName(Platform).ToString();
	}

	void CleanDirectories(TArray<FName> const& ShaderFormats)
	{
		for (FName const& Format : ShaderFormats)
		{
			FString ShaderIntermediateLocation = FPaths::ProjectSavedDir() / TEXT("Shaders") / Format.ToString();
			FString ShaderPlatformIntermediateLocation = FPaths::ProjectSavedDir() / TEXT("Shaders") / GetFormatAndPlatformName(Format);
			IFileManager::Get().DeleteDirectory(*ShaderIntermediateLocation, false, true);
			IFileManager::Get().DeleteDirectory(*ShaderPlatformIntermediateLocation, false, true);
		}
	}

	void CookShaderFormats(TArray<FShaderLibraryCooker::FShaderFormatDescriptor> const& ShaderFormats)
	{
		bool bAtLeastOneFormatNeedsDeterminism = false;

		for (const FShaderLibraryCooker::FShaderFormatDescriptor& Descriptor : ShaderFormats)
		{
			FName const& Format = Descriptor.ShaderFormat;

			EShaderPlatform Platform = ShaderFormatToLegacyShaderPlatform(Format);
			FString FormatAndPlatformName = GetFormatAndPlatformName(Format);
			FEditorShaderCodeArchive* CodeArchive = EditorShaderCodeArchive[Platform];
			if (!CodeArchive)
			{
				bIsEditorShaderCodeArchiveEmpty = false;
				CodeArchive = new FEditorShaderCodeArchive(FName(FormatAndPlatformName), Descriptor.bNeedsDeterministicOrder);
				EditorShaderCodeArchive[Platform] = CodeArchive;
				EditorArchivePipelines[Platform] = !bNativeFormat;
			}
			check(CodeArchive);

			if (Descriptor.bNeedsDeterministicOrder)
			{
				bAtLeastOneFormatNeedsDeterminism = true;
			}
		}
		for (const FShaderLibraryCooker::FShaderFormatDescriptor& Descriptor : ShaderFormats)
		{
			FName const& Format = Descriptor.ShaderFormat;
			bool bUseStableKeys = Descriptor.bNeedsStableKeys;

			EShaderPlatform Platform = ShaderFormatToLegacyShaderPlatform(Format);
			FName PossiblyAdjustedFormat = LegacyShaderPlatformToShaderFormat(Platform);	// Vulkan and GL switch between name variants depending on CVars 
			FEditorShaderStableInfo* StableArchive = EditorShaderStableInfo[Platform];
			if (!StableArchive && bUseStableKeys)
			{
				bIsEditorShaderStableInfoEmpty = false;
				StableArchive = new FEditorShaderStableInfo(PossiblyAdjustedFormat);
				EditorShaderStableInfo[Platform] = StableArchive;
				ShaderFormatsThatNeedStableKeys[(int)Platform] = true;
			}
		}
	}

	bool NeedsShaderStableKeys(EShaderPlatform Platform) 
	{
		if (Platform == EShaderPlatform::SP_NumPlatforms)
		{
			return ShaderFormatsThatNeedStableKeys.Find(true) != INDEX_NONE;
		}
		return (ShaderFormatsThatNeedStableKeys[(int)Platform]) != 0;
	}

	void AddShaderCode(EShaderPlatform Platform, const FShaderMapResourceCode* Code, const FShaderMapAssetPaths& AssociatedAssets)
	{
		FScopeLock ScopeLock(&ShaderCodeCS);
		checkf(Platform < UE_ARRAY_COUNT(EditorShaderCodeStats), TEXT("FShaderCodeLibrary::AddShaderCode can only be called with a valid shader platform (expected no more than %d, passed: %d)"), 
			static_cast<int32>(UE_ARRAY_COUNT(EditorShaderCodeStats)), static_cast<int32>(Platform));
		static_assert(UE_ARRAY_COUNT(EditorShaderCodeStats) == UE_ARRAY_COUNT(EditorShaderCodeArchive), "Size of EditorShaderCodeStats must match size of EditorShaderCodeArchive");

		FShaderCodeStats& CodeStats = EditorShaderCodeStats[Platform];
		FEditorShaderCodeArchive* CodeArchive = EditorShaderCodeArchive[Platform];
		checkf(CodeArchive, TEXT("EditorShaderCodeArchive for (EShaderPlatform)%d is null!"), (int32)Platform);

		CodeArchive->AddShaderCode(CodeStats, Code, AssociatedAssets);
	}

#if WITH_EDITOR
	void CopyToCompactBinaryAndClear(FCbWriter& Writer, bool& bOutHasData, bool& bOutRanOutOfRoom, int64 MaxShaderSize)
	{
		bOutRanOutOfRoom = false;

		TArray<EShaderPlatform, TInlineAllocator<10>> PlatformsToCopy;
		FScopeLock ScopeLock(&ShaderCodeCS);
		for (EShaderPlatform Platform = (EShaderPlatform)0; Platform < (EShaderPlatform)UE_ARRAY_COUNT(EditorShaderCodeStats);
			Platform = (EShaderPlatform)((int)Platform + 1))
		{
			FEditorShaderCodeArchive* CodeArchive = EditorShaderCodeArchive[Platform];
			FEditorShaderStableInfo* StableInfo = EditorShaderStableInfo[Platform];
			if ((CodeArchive && CodeArchive->HasDataToCopy()) || (StableInfo && StableInfo->HasDataToCopy()))
			{
				PlatformsToCopy.Add(Platform);
			}
		}

		if (PlatformsToCopy.IsEmpty())
		{
			bOutHasData = false;
			return;
		}
		bOutHasData = true;
		int64 RemainingSize = MaxShaderSize;
		Writer.BeginArray();
		for (EShaderPlatform Platform : PlatformsToCopy)
		{
			if (bOutRanOutOfRoom)
			{
				break;
			}
			Writer.BeginObject();
			{
				Writer << "Platform" << (uint32)Platform;
				FEditorShaderCodeArchive* CodeArchive = EditorShaderCodeArchive[Platform];
				FEditorShaderStableInfo* StableInfo = EditorShaderStableInfo[Platform];

				if ((CodeArchive && CodeArchive->HasDataToCopy()))
				{
					FSerializedShaderArchive TransferArchive;
					TArray<uint8> TransferCode;
					int64 MaxShaderSizeThisCall = RemainingSize;
					int64 MaxShaderCount = -1;
					bool bRanOutOfRoom;
					CodeArchive->CopyToArchiveAndClear(TransferArchive, TransferCode, bRanOutOfRoom, MaxShaderSizeThisCall, MaxShaderCount);
					bOutRanOutOfRoom |= bRanOutOfRoom;
					if (bRanOutOfRoom && TransferCode.IsEmpty() && RemainingSize == MaxShaderSize)
					{
						UE_LOG(LogShaderLibrary, Error,
							TEXT("MaxShaderSize %" INT64_FMT " is too small to read even a single shader. We will ignore it and allow uncapped size, which will possibly cause an overflow in the caller."),
							MaxShaderSize);
						MaxShaderSizeThisCall = -1;
						MaxShaderCount = 1;
						TransferArchive.Empty();
						CodeArchive->CopyToArchiveAndClear(TransferArchive, TransferCode, bRanOutOfRoom, MaxShaderSizeThisCall, MaxShaderCount);
					}
					if (!TransferArchive.IsEmpty())
					{
						RemainingSize -= TransferCode.Num();
						Writer.SetName("EditorShaderCodeArchive");
						CodeArchive->CopyToCompactBinary(Writer, TransferArchive, TransferCode);
					}

				}
				if (!bOutRanOutOfRoom && StableInfo && StableInfo->HasDataToCopy())
				{
					Writer.SetName("EditorShaderStableInfo");
					StableInfo->CopyToCompactBinary(Writer);
				}

				// ChunksSaved is not copied; it is constructed at end of cook after copying from remote workers is complete
				// EditorShaderCodeStats is not copied; it is gathered from the shadercode we send when the director receives it
				// bShaderFormatsThatNeedStableKeys is not copied; it is constructed during BeginCook and is the same on all machines
				// EditorArchivePipelines is not copied; it is constructed during BeginCook and is the same on all machines

			}
			Writer.EndObject();
		}
		Writer.EndArray();
	}

	bool AppendFromCompactBinary(FCbFieldView Field)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		bool bOk = true;
		for (FCbFieldView PlatformField : Field)
		{
			uint32 PlatformInt;
			if (!LoadFromCompactBinary(PlatformField["Platform"], PlatformInt) ||
				PlatformInt >= UE_ARRAY_COUNT(EditorShaderCodeStats))
			{
				bOk = false;
				continue;
			}
			EShaderPlatform Platform = (EShaderPlatform)PlatformInt;

			FCbFieldView CodeArchiveField = PlatformField["EditorShaderCodeArchive"];
			if (CodeArchiveField.HasValue())
			{
				FEditorShaderCodeArchive* CodeArchive = EditorShaderCodeArchive[Platform];
				if (CodeArchive)
				{
					FShaderCodeStats& CodeStats = EditorShaderCodeStats[Platform];
					bOk = CodeArchive->AppendFromCompactBinary(CodeArchiveField, CodeStats) & bOk;
				}
				else
				{
					UE_LOG(LogShaderLibrary, Error, TEXT("ShaderMapLibrary transfer received from a remote machine includes data for Platform %d, but the ShaderMapLibrary has not been initialized for that platform in the local process. ")
						TEXT("The information will be ignored."), (int32)Platform);
					bOk = false;
				}
			}

			FCbFieldView StableArchiveField = PlatformField["EditorShaderStableInfo"];
			if (StableArchiveField.HasValue())
			{
				FEditorShaderStableInfo* StableArchive = EditorShaderStableInfo[Platform];
				if (StableArchive)
				{
					bOk = StableArchive->AppendFromCompactBinary(StableArchiveField) & bOk;
				}
			}
		}
		return bOk;
	}
#endif

	void AddShaderStableKeyValue(EShaderPlatform InShaderPlatform, FStableShaderKeyAndValue& StableKeyValue)
	{
		FEditorShaderStableInfo* StableArchive = EditorShaderStableInfo[InShaderPlatform];
		if (!StableArchive)
		{
			return;
		}

		FScopeLock ScopeLock(&ShaderCodeCS);

		StableKeyValue.ComputeKeyHash();
		StableArchive->AddShader(StableKeyValue, FEditorShaderStableInfo::EMergeRule::OverwriteUnmodifiedWarnModified);
	}
 
	void FinishPopulateShaderCode(const FString& ShaderCodeDir, const FString& MetaOutputDir, const TArray<FName>& ShaderFormats)
	{
		for (FName ShaderFormatName : ShaderFormats)
		{
			EShaderPlatform SPlatform = ShaderFormatToLegacyShaderPlatform(ShaderFormatName);

			FEditorShaderCodeArchive* CodeArchive = EditorShaderCodeArchive[SPlatform];
			if (CodeArchive)
			{
				CodeArchive->FinishPopulate(ShaderCodeDir);
			}

			FEditorShaderStableInfo* StableArchive = EditorShaderStableInfo[SPlatform];
			if (StableArchive)
			{
				StableArchive->FinishPopulate(MetaOutputDir);
			}
		}
	}

	bool SaveShaderCode(const FString& ShaderCodeDir, const FString& MetaOutputDir, const TArray<FName>& ShaderFormats, TArray<FString>& OutSCLCSVPath)
	{
		bool bOk = ShaderFormats.Num() > 0;

		FScopeLock ScopeLock(&ShaderCodeCS);

		for (const FName ShaderFormatName : ShaderFormats)
		{
			EShaderPlatform SPlatform = ShaderFormatToLegacyShaderPlatform(ShaderFormatName);

			FEditorShaderCodeArchive* CodeArchive = EditorShaderCodeArchive[SPlatform];
			if (CodeArchive)
			{
				// If we saved the shader code while generating the chunk, do not save a single consolidated library as it should not be used and
				// will only bloat the build.
				// Still save the full asset info for debugging
				if (ChunksSaved[SPlatform].Num() == 0)
				{
					// always save shaders in our format even if the platform will use native one. This is needed for iterative cooks (Launch On et al)
					// to reload previously cooked shaders
					bOk = CodeArchive->SaveToDisk(ShaderCodeDir, MetaOutputDir) && bOk;

					bool bShouldWriteInNativeFormat = bOk && bNativeFormat && CodeArchive->GetFormat()->SupportsShaderArchives();
					if (bShouldWriteInNativeFormat)
					{
						bOk = CodeArchive->PackageNativeShaderLibrary(ShaderCodeDir) && bOk;
					}

					if (bOk)
					{
						CodeArchive->DumpStatsAndDebugInfo();
					}
				}
				else
				{
					// save asset info only, for debugging
					bOk = CodeArchive->SaveToDisk(ShaderCodeDir, MetaOutputDir, true) && bOk;
				}
			}
			FEditorShaderStableInfo* StableArchive = EditorShaderStableInfo[SPlatform];
			if (StableArchive)
			{
				// Stable shader info is not saved per-chunk (it is not needed at runtime), so save it always
				FString SCLCSVPath;
				bOk &= StableArchive->SaveToDisk(MetaOutputDir, SCLCSVPath);
				
				// Only add output files if they were actually written to disk (if there were no shaders in the library it is not a failure).
				if (!SCLCSVPath.IsEmpty())
				{
					OutSCLCSVPath.Add(SCLCSVPath);
				}
			}
		}

		return bOk;
	}

	bool SaveShaderCodeChunk(int32 ChunkId, const TSet<FName>& InPackagesInChunk, const TArray<FName>& ShaderFormats,
		const FString& SandboxDestinationPath, const FString& SandboxMetadataPath, TArray<FString>& OutChunkFilenames)
	{
		bool bOk = ShaderFormats.Num() > 0;

		FScopeLock ScopeLock(&ShaderCodeCS);

		for (const FName ShaderFormatName : ShaderFormats)
		{
			EShaderPlatform SPlatform = ShaderFormatToLegacyShaderPlatform(ShaderFormatName);

			// we may get duplicate calls for the same Chunk Id because the cooker sometimes calls asset registry SaveManifests twice.
			if (ChunksSaved[SPlatform].Contains(ChunkId))
			{
				continue;
			}

			FEditorShaderCodeArchive* OrginalCodeArchive = EditorShaderCodeArchive[SPlatform];
			if (!OrginalCodeArchive)
			{
				bOk = false;
				break;
			}

			FEditorShaderCodeArchive* PerChunkArchive = OrginalCodeArchive->CreateChunk(ChunkId, InPackagesInChunk);
			if (!PerChunkArchive)
			{
				bOk = false;
				break;
			}

			// skip saving if no shaders are actually stored
			if (!PerChunkArchive->IsEmpty())
			{

				// always save shaders in our format even if the platform will use native one. This is needed for iterative cooks (Launch On et al)
				// to reload previously cooked shaders
				bOk = PerChunkArchive->SaveToDisk(SandboxDestinationPath, SandboxMetadataPath, false, &OutChunkFilenames) && bOk;

				bool bShouldWriteInNativeFormat = bOk && bNativeFormat && PerChunkArchive->GetFormat()->SupportsShaderArchives();
				if (bShouldWriteInNativeFormat)
				{
					bOk = PerChunkArchive->PackageNativeShaderLibrary(SandboxDestinationPath, &OutChunkFilenames) && bOk;
				}

				if (bOk)
				{
					PerChunkArchive->DumpStatsAndDebugInfo();
					ChunksSaved[SPlatform].Add(ChunkId);
				}
			}
		}

		return bOk;
	}

	bool PackageNativeShaderLibrary(const FString& ShaderCodeDir, const TArray<FName>& ShaderFormats)
	{
		bool bOK = true;
		for (const FName ShaderFormatName : ShaderFormats)
		{
			EShaderPlatform SPlatform = ShaderFormatToLegacyShaderPlatform(ShaderFormatName);
			FEditorShaderCodeArchive* CodeArchive = EditorShaderCodeArchive[SPlatform];

			if (CodeArchive && CodeArchive->GetFormat()->SupportsShaderArchives())
			{
				bOK &= CodeArchive->PackageNativeShaderLibrary(ShaderCodeDir);
			}
		}
		return bOK;
	}

	void DumpShaderCodeStats()
	{
		int32 PlatformId = 0;
		for (const FShaderCodeStats& CodeStats : EditorShaderCodeStats)
		{
			if (CodeStats.NumShaders > 0)
			{
				float UniqueSize = CodeStats.ShadersUniqueSize;
				float UniqueSizeMB = FUnitConversion::Convert(UniqueSize, EUnit::Bytes, EUnit::Megabytes);
				float TotalSize = CodeStats.ShadersSize;
				float TotalSizeMB = FUnitConversion::Convert(TotalSize, EUnit::Bytes, EUnit::Megabytes);

				UE_LOG(LogShaderLibrary, Display, TEXT(""));
				UE_LOG(LogShaderLibrary, Display, TEXT("Shader Code Stats: %s"), *LegacyShaderPlatformToShaderFormat((EShaderPlatform)PlatformId).ToString());
				UE_LOG(LogShaderLibrary, Display, TEXT("================="));
				UE_LOG(LogShaderLibrary, Display, TEXT("Unique Shaders: %d, Total Shaders: %d, Unique Shadermaps: %d"), CodeStats.NumUniqueShaders, CodeStats.NumShaders, CodeStats.NumShaderMaps);
				UE_LOG(LogShaderLibrary, Display, TEXT("Unique Shaders Size: %.2fmb, Total Shader Size: %.2fmb"), UniqueSizeMB, TotalSizeMB);
				UE_LOG(LogShaderLibrary, Display, TEXT("================="));
			}

			PlatformId++;
		}
	}
#endif// WITH_EDITOR

	void PreloadPackageShaderMapsDelegate(TArrayView<const FSHAHash> ShaderMapHashes, FCoreDelegates::FAttachShaderReadRequestFunc AttachShaderReadRequestFunc)
	{
		LLM_SCOPE(ELLMTag::Shaders);
		for (const FSHAHash& ShaderMapHash : ShaderMapHashes)
		{
			int32 ShaderMapIndex;
			FShaderLibraryInstance* LibraryInstance = FindShaderLibraryForShaderMap(ShaderMapHash, ShaderMapIndex);
			if (LibraryInstance)
			{
				LibraryInstance->PreloadPackageShaderMap(ShaderMapIndex, AttachShaderReadRequestFunc);
			}
		}
	}

	void ReleasePreloadedPackageShaderMapsDelegate(TArrayView<const FSHAHash> ShaderMapHashes)
	{
		for (const FSHAHash& ShaderMapHash : ShaderMapHashes)
		{
			int32 ShaderMapIndex;
			FShaderLibraryInstance* LibraryInstance = FindShaderLibraryForShaderMap(ShaderMapHash, ShaderMapIndex);
			if (LibraryInstance)
			{
				LibraryInstance->ReleasePreloadedPackageShaderMap(ShaderMapIndex);
			}
		}
	}

};

static FSharedShaderCodeRequest OnSharedShaderCodeRequest;

FShaderLibrariesCollection* FShaderLibrariesCollection::Impl = nullptr;

static void FShaderCodeLibraryPluginMountedCallback(IPlugin& Plugin)
{
	if (FApp::CanEverRender() && UE::ShaderLibrary::Private::PluginsToIgnoreOnMount.Remove(Plugin.GetName()) == 0)
	{
		FShaderCodeLibrary::OpenPluginShaderLibrary(Plugin);
	}
}

static void FShaderCodeLibraryPluginUnmountedCallback(IPlugin& Plugin)
{
	class FShaderCodeLibraryCleanup : public FDeferredCleanupInterface
	{
	public:
		FShaderCodeLibraryCleanup(const FString& Name)
			: Name(Name)
		{
		}

		virtual ~FShaderCodeLibraryCleanup()
		{
			FShaderCodeLibrary::CloseLibrary(Name);
		}

	private:
		FString Name;
	};

	if (Plugin.CanContainContent() && Plugin.IsEnabled())
	{
		// unload any shader libraries that may exist in this plugin
		BeginCleanup(new FShaderCodeLibraryCleanup(Plugin.GetName()));
	}
}

static void FShaderLibraryPakFileMountedCallback(const IPakFile& PakFile)
{
	using namespace UE::ShaderLibrary::Private;

	int32 NewChunk = PakFile.PakGetPakchunkIndex();
	UE_LOG(LogShaderLibrary, Verbose, TEXT("ShaderCodeLibraryPakFileMountedCallback: PakFile '%s' (chunk index %d, root '%s') mounted"), *PakFile.PakGetPakFilename(), PakFile.PakGetPakchunkIndex(), *PakFile.PakGetMountPoint());

	FMountedPakFileInfo PakFileInfo(PakFile);
	{
		FScopeLock PakFilesLocker(&FMountedPakFileInfo::KnownPakFilesAccessLock);
		FMountedPakFileInfo::KnownPakFiles.Add(PakFileInfo);
	}

	// if shaderlibrary has not yet been initialized, add the chunk as pending
	if (FShaderLibrariesCollection::Impl)
	{
		FShaderLibrariesCollection::Impl->OnPakFileMounted(PakFileInfo);
	}
	else
	{
		UE_LOG(LogShaderLibrary, Verbose, TEXT("ShaderCodeLibraryPakFileMountedCallback: pending pak file info (%s)"), *PakFileInfo.ToString());
	}
}

void FShaderCodeLibrary::PreInit()
{
	// add a callback for opening later chunks
	UE::ShaderLibrary::Private::OnPakFileMountedDelegateHandle = FCoreDelegates::GetOnPakFileMounted2().AddStatic(&FShaderLibraryPakFileMountedCallback);
}

void FShaderCodeLibrary::InitForRuntime(EShaderPlatform ShaderPlatform)
{
	if (FShaderLibrariesCollection::Impl != nullptr)
	{
		//cooked, can't change shader platform on the fly
		check(FShaderLibrariesCollection::Impl->GetRuntimeShaderPlatform() == ShaderPlatform);
		return;
	}

	// Cannot be enabled by the server, pointless if we can't ever render and not compatible with cook-on-the-fly
	bool bArchive = false;
	GConfig->GetBool(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("bShareMaterialShaderCode"), bArchive, GGameIni);

	// We cannot enable native shader libraries when running with NullRHI, so for consistency all libraries (both native and non-native) are disabled if FApp::CanEverRender() == false
	bool bEnable = !FPlatformProperties::IsServerOnly() && FApp::CanEverRender() && bArchive;
#if !UE_BUILD_SHIPPING
	const bool bCookOnTheFly = IsRunningCookOnTheFly(); 
	bEnable &= !bCookOnTheFly;
#endif

	if (bEnable)
	{
		FShaderLibrariesCollection::Impl = new FShaderLibrariesCollection(ShaderPlatform, false);
		if (FShaderLibrariesCollection::Impl->OpenLibrary(TEXT("Global"), FPaths::ProjectContentDir()))
		{
			UE::ShaderLibrary::Private::OnPluginMountedDelegateHandle = IPluginManager::Get().OnNewPluginMounted().AddStatic(&FShaderCodeLibraryPluginMountedCallback);
			UE::ShaderLibrary::Private::OnPluginUnmountedDelegateHandle = IPluginManager::Get().OnPluginUnmounted().AddStatic(&FShaderCodeLibraryPluginUnmountedCallback);
		
#if (!UE_BUILD_SHIPPING && !UE_BUILD_TEST)	// test builds are supposed to be closer to Shipping than to Development, and as such not have development features
			// support shared cooked builds by also opening the shared cooked build shader code file
			FShaderLibrariesCollection::Impl->OpenLibrary(TEXT("Global_SC"), FPaths::ProjectContentDir());
#endif

			// mount shader library from the plugins as they may also have global shaders
			auto Plugins = IPluginManager::Get().GetEnabledPluginsWithContent();
			ParallelFor(Plugins.Num(), [&](int32 Index)
			{
				FShaderCodeLibrary::OpenPluginShaderLibrary(*Plugins[Index]);
			});
		}
		else
		{
			Shutdown();
#if !WITH_EDITOR
			if (FPlatformProperties::SupportsWindowedMode())
			{
				FPlatformSplash::Hide();

				UE_LOG(LogShaderLibrary, Error, TEXT("Failed to initialize ShaderCodeLibrary required by the project because part of the Global shader library is missing from %s."), *FPaths::ProjectContentDir());

				FText LocalizedMsg = FText::Format(NSLOCTEXT("MessageDialog", "MissingGlobalShaderLibraryFiles_Body", "Game files required to initialize the global shader library are missing from:\n\n{0}\n\nPlease make sure the game is installed correctly."), FText::FromString(FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir())));
				FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *LocalizedMsg.ToString(), *NSLOCTEXT("MessageDialog", "MissingGlobalShaderLibraryFiles_Title", "Missing game files").ToString());
			}
			else
			{
                FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("MessageDialog", "MissingGlobalShaderLibraryFilesClient_Body", "Game files required to initialize the global shader and cooked content are most likely missing. Refer to Engine log for details."));
				UE_LOG(LogShaderLibrary, Fatal, TEXT("Failed to initialize ShaderCodeLibrary required by the project because part of the Global shader library is missing from %s."), *FPaths::ProjectContentDir());
			}
			FPlatformMisc::RequestExit(true, TEXT("FShaderCodeLibrary::InitForRuntime"));
#endif // !WITH_EDITOR	
		}
	}
}

void FShaderCodeLibrary::Shutdown()
{
	if (UE::ShaderLibrary::Private::OnPakFileMountedDelegateHandle.IsValid())
	{
		FCoreDelegates::GetOnPakFileMounted2().Remove(UE::ShaderLibrary::Private::OnPakFileMountedDelegateHandle);
		UE::ShaderLibrary::Private::OnPakFileMountedDelegateHandle.Reset();
	}
	if (UE::ShaderLibrary::Private::OnPluginMountedDelegateHandle.IsValid())
	{
		IPluginManager::Get().OnNewPluginMounted().Remove(UE::ShaderLibrary::Private::OnPluginMountedDelegateHandle);
		UE::ShaderLibrary::Private::OnPluginMountedDelegateHandle.Reset();
	}
	if (UE::ShaderLibrary::Private::OnPluginUnmountedDelegateHandle.IsValid())
	{
		IPluginManager::Get().OnPluginUnmounted().Remove(UE::ShaderLibrary::Private::OnPluginUnmountedDelegateHandle);
		UE::ShaderLibrary::Private::OnPluginUnmountedDelegateHandle.Reset();
	}

	UE::ShaderLibrary::Private::PluginsToIgnoreOnMount.Empty();

	if (FShaderLibrariesCollection::Impl)
	{
		delete FShaderLibrariesCollection::Impl;
		FShaderLibrariesCollection::Impl = nullptr;
	}

	FScopeLock PakFilesLocker(&UE::ShaderLibrary::Private::FMountedPakFileInfo::KnownPakFilesAccessLock);
	UE::ShaderLibrary::Private::FMountedPakFileInfo::KnownPakFiles.Empty();
}

bool FShaderCodeLibrary::IsEnabled()
{
	return FShaderLibrariesCollection::Impl != nullptr;
}

bool FShaderCodeLibrary::ContainsShaderCode(const FSHAHash& Hash)
{
	if (FShaderLibrariesCollection::Impl)
	{
		return FShaderLibrariesCollection::Impl->ContainsShaderCode(Hash);
	}
	return false;
}

bool FShaderCodeLibrary::ContainsShaderCode(const FSHAHash& Hash, const FString& LogicalLibraryName)
{
	if (FShaderLibrariesCollection::Impl)
	{
		return FShaderLibrariesCollection::Impl->ContainsShaderCode(Hash, LogicalLibraryName);
	}
	return false;
}

TRefCountPtr<FShaderMapResource> FShaderCodeLibrary::LoadResource(const FSHAHash& Hash, FArchive* Ar)
{
	if (FShaderLibrariesCollection::Impl)
	{
		SCOPED_LOADTIMER(FShaderCodeLibrary_LoadResource);
		OnSharedShaderCodeRequest.Broadcast(Hash, Ar);
		return TRefCountPtr<FShaderMapResource>(FShaderLibrariesCollection::Impl->LoadResource(Hash, Ar));
	}
	return TRefCountPtr<FShaderMapResource>();
}

bool FShaderCodeLibrary::PreloadShader(const FSHAHash& Hash, FArchive* Ar)
{
	if (FShaderLibrariesCollection::Impl)
	{
		OnSharedShaderCodeRequest.Broadcast(Hash, Ar);
		return FShaderLibrariesCollection::Impl->PreloadShader(Hash, Ar);
	}
	return false;
}

bool FShaderCodeLibrary::ReleasePreloadedShader(const FSHAHash& Hash)
{
	if (FShaderLibrariesCollection::Impl)
	{
		return FShaderLibrariesCollection::Impl->ReleasePreloadedShader(Hash);
	}
	return false;
}

FVertexShaderRHIRef FShaderCodeLibrary::CreateVertexShader(EShaderPlatform Platform, const FSHAHash& Hash)
{
	if (FShaderLibrariesCollection::Impl)
	{
		return FVertexShaderRHIRef(FShaderLibrariesCollection::Impl->CreateShader(SF_Vertex, Hash));
	}
	return nullptr;
}

FPixelShaderRHIRef FShaderCodeLibrary::CreatePixelShader(EShaderPlatform Platform, const FSHAHash& Hash)
{
	if (FShaderLibrariesCollection::Impl)
	{
		return FPixelShaderRHIRef(FShaderLibrariesCollection::Impl->CreateShader(SF_Pixel, Hash));
	}
	return nullptr;
}

FGeometryShaderRHIRef FShaderCodeLibrary::CreateGeometryShader(EShaderPlatform Platform, const FSHAHash& Hash)
{
	if (FShaderLibrariesCollection::Impl)
	{
		return FGeometryShaderRHIRef(FShaderLibrariesCollection::Impl->CreateShader(SF_Geometry, Hash));
	}
	return nullptr;
}

FComputeShaderRHIRef FShaderCodeLibrary::CreateComputeShader(EShaderPlatform Platform, const FSHAHash& Hash)
{
	if (FShaderLibrariesCollection::Impl)
	{
		return FComputeShaderRHIRef(FShaderLibrariesCollection::Impl->CreateShader(SF_Compute, Hash));
	}
	return nullptr;
}

FMeshShaderRHIRef FShaderCodeLibrary::CreateMeshShader(EShaderPlatform Platform, const FSHAHash& Hash)
{
	if (FShaderLibrariesCollection::Impl)
	{
		return FMeshShaderRHIRef(FShaderLibrariesCollection::Impl->CreateShader(SF_Mesh, Hash));
	}
	return nullptr;
}

FAmplificationShaderRHIRef FShaderCodeLibrary::CreateAmplificationShader(EShaderPlatform Platform, const FSHAHash& Hash)
{
	if (FShaderLibrariesCollection::Impl)
	{
		return FAmplificationShaderRHIRef(FShaderLibrariesCollection::Impl->CreateShader(SF_Amplification, Hash));
	}
	return nullptr;
}

FRayTracingShaderRHIRef FShaderCodeLibrary::CreateRayTracingShader(EShaderPlatform Platform, const FSHAHash& Hash, EShaderFrequency Frequency)
{
	if (FShaderLibrariesCollection::Impl)
	{
		check(Frequency >= SF_RayGen && Frequency <= SF_RayCallable);
		return FRayTracingShaderRHIRef(FShaderLibrariesCollection::Impl->CreateShader(Frequency, Hash));
	}
	return nullptr;
}

uint32 FShaderCodeLibrary::GetShaderCount(void)
{
	uint32 Num = 0;
	if (FShaderLibrariesCollection::Impl)
	{
		Num = FShaderLibrariesCollection::Impl->GetShaderCount();
	}
	return Num;
}

EShaderPlatform FShaderCodeLibrary::GetRuntimeShaderPlatform(void)
{
	EShaderPlatform Platform = SP_NumPlatforms;
	if (FShaderLibrariesCollection::Impl)
	{
		Platform = FShaderLibrariesCollection::Impl->GetRuntimeShaderPlatform();
	}
	return Platform;
}

void FShaderCodeLibrary::AddKnownChunkIDs(const int32* IDs, const int32 NumChunkIDs)
{
	using namespace UE::ShaderLibrary::Private;

	checkf(IDs, TEXT("Invalid pointer to chunk IDs passed"));
	UE_LOG(LogShaderLibrary, Display, TEXT("AddKnownChunkIDs: adding %d chunk IDs"), NumChunkIDs);

	for (int32 IdxChunkId = 0; IdxChunkId < NumChunkIDs; ++IdxChunkId)
	{
		FMountedPakFileInfo PakFileInfo(IDs[IdxChunkId]);
		{
			FScopeLock PakFilesLocker(&FMountedPakFileInfo::KnownPakFilesAccessLock);
			FMountedPakFileInfo::KnownPakFiles.Add(PakFileInfo);
		}

		// if shaderlibrary has not yet been initialized, add the chunk as pending
		if (FShaderLibrariesCollection::Impl)
		{
			FShaderLibrariesCollection::Impl->OnPakFileMounted(PakFileInfo);
		}
		else
		{
			UE_LOG(LogShaderLibrary, Display, TEXT("AddKnownChunkIDs: pending pak file info (%s)"), *PakFileInfo.ToString());
		}
	}
}

bool FShaderCodeLibrary::OpenLibrary(FString const& Name, FString const& Directory, bool bMonolithicOnly)
{
	bool bResult = false;
	if (FShaderLibrariesCollection::Impl)
	{
		bResult = FShaderLibrariesCollection::Impl->OpenLibrary(Name, Directory, bMonolithicOnly);
	}
	return bResult;
}

void FShaderCodeLibrary::CloseLibrary(FString const& Name)
{
	if (FShaderLibrariesCollection::Impl)
	{
		FShaderLibrariesCollection::Impl->CloseLibrary(Name);
	}
}

#if WITH_EDITOR
// for now a lot of FShaderLibraryCooker code is aliased with the runtime code, but this will be refactored (UE-103486)
void FShaderLibraryCooker::InitForCooking(bool bNativeFormat)
{
	FShaderLibrariesCollection::Impl = new FShaderLibrariesCollection(SP_NumPlatforms, bNativeFormat);
}

void FShaderLibraryCooker::Shutdown()
{
	if (FShaderLibrariesCollection::Impl)
	{
		//DumpShaderCodeStats();

		delete FShaderLibrariesCollection::Impl;
		FShaderLibrariesCollection::Impl = nullptr;
	}
}

void FShaderLibraryCooker::CleanDirectories(TArray<FName> const& ShaderFormats)
{
	if (FShaderLibrariesCollection::Impl)
	{
		FShaderLibrariesCollection::Impl->CleanDirectories(ShaderFormats);
	}
}

bool FShaderLibraryCooker::BeginCookingLibrary(FString const& Name)
{
	// for now this is aliased with the runtime code, but this will be refactored (UE-103486)
	bool bResult = false;
	if (FShaderLibrariesCollection::Impl)
	{
		bResult = FShaderLibrariesCollection::Impl->OpenLibrary(Name, TEXT(""));
	}
	return bResult;
}

void FShaderLibraryCooker::EndCookingLibrary(FString const& Name)
{
	// for now this is aliased with the runtime code, but this will be refactored (UE-103486)
	if (FShaderLibrariesCollection::Impl)
	{
		FShaderLibrariesCollection::Impl->CloseLibrary(Name);
	}
}

bool FShaderLibraryCooker::IsShaderLibraryEnabled()
{
	return FShaderLibrariesCollection::Impl != nullptr;
}

void FShaderLibraryCooker::CookShaderFormats(TArray<FShaderFormatDescriptor> const& ShaderFormats)
{
	if (FShaderLibrariesCollection::Impl)
	{
		FShaderLibrariesCollection::Impl->CookShaderFormats(ShaderFormats);
	}
}

bool FShaderLibraryCooker::AddShaderCode(EShaderPlatform ShaderPlatform, const FShaderMapResourceCode* Code, const FShaderMapAssetPaths& AssociatedAssets)
{
	if (FShaderLibrariesCollection::Impl)
	{
		FShaderLibrariesCollection::Impl->AddShaderCode(ShaderPlatform, Code, AssociatedAssets);
		return true;
	}
	return false;
}

#if WITH_EDITOR
void FShaderLibraryCooker::CopyToCompactBinaryAndClear(FCbWriter& Writer, bool& bOutHasData,
	bool& bOutRanOutOfRoom, int64 MaxShaderSize)
{
	if (FShaderLibrariesCollection::Impl)
	{
		FShaderLibrariesCollection::Impl->CopyToCompactBinaryAndClear(Writer, bOutHasData,
			bOutRanOutOfRoom, MaxShaderSize);
	}
}

bool FShaderLibraryCooker::AppendFromCompactBinary(FCbFieldView Field)
{
	if (FShaderLibrariesCollection::Impl)
	{
		return FShaderLibrariesCollection::Impl->AppendFromCompactBinary(Field);
	}
	return false;
}
#endif

bool FShaderLibraryCooker::NeedsShaderStableKeys(EShaderPlatform ShaderPlatform)
{
	if (FShaderLibrariesCollection::Impl)
	{
		return FShaderLibrariesCollection::Impl->NeedsShaderStableKeys(ShaderPlatform);
	}
	return false;
}

void FShaderLibraryCooker::AddShaderStableKeyValue(EShaderPlatform ShaderPlatform, FStableShaderKeyAndValue& StableKeyValue)
{
	if (FShaderLibrariesCollection::Impl)
	{
		FShaderLibrariesCollection::Impl->AddShaderStableKeyValue(ShaderPlatform, StableKeyValue);
	}
}

void FShaderLibraryCooker::DumpShaderCodeStats()
{
	if (FShaderLibrariesCollection::Impl)
	{
		FShaderLibrariesCollection::Impl->DumpShaderCodeStats();
	}
}

bool FShaderLibraryCooker::CreatePatchLibrary(TArray<FString> const& OldMetaDataDirs, FString const& NewMetaDataDir, FString const& OutDir, bool bNativeFormat, bool bNeedsDeterministicOrder)
{
	TMap<FName, TSet<FString>> FormatLibraryMap;
	TArray<FString> LibraryFiles;
	IFileManager::Get().FindFiles(LibraryFiles, *(NewMetaDataDir / TEXT("ShaderLibrarySource")), *ShaderExtension);
	
	for (FString const& Path : LibraryFiles)
	{
		FString Name = FPaths::GetBaseFilename(Path);
		if (Name.RemoveFromStart(TEXT("ShaderArchive-")))
		{
			TArray<FString> Components;
			if (Name.ParseIntoArray(Components, TEXT("-")) == 3)
			{
				FName Format(*Components[1]);
				FName Platform(*Components[2]);
				FString FormatAndPlatform = Format.ToString() + TEXT("-") + Platform.ToString();

				TSet<FString>& Libraries = FormatLibraryMap.FindOrAdd(FName(FormatAndPlatform));
				Libraries.Add(Components[0]);
			}
		}
	}
	
	bool bOK = true;
	for (auto const& Entry : FormatLibraryMap)
	{
		for (auto const& Library : Entry.Value)
		{
			bOK |= FEditorShaderCodeArchive::CreatePatchLibrary(Entry.Key, Library, OldMetaDataDirs, NewMetaDataDir, OutDir, bNativeFormat, bNeedsDeterministicOrder);
		}
	}
	return bOK;
}

void FShaderLibraryCooker::FinishPopulateShaderLibrary(const ITargetPlatform* TargetPlatform, FString const& Name,
	FString const& SandboxDestinationPath, FString const& SandboxMetadataPath)
{
	const FString& ShaderCodeDir = SandboxDestinationPath;
	const FString& MetaDataPath = SandboxMetadataPath;

	checkf(FShaderLibrariesCollection::Impl != nullptr, TEXT("FShaderLibraryCooker was not initialized properly"));
	checkf(TargetPlatform, TEXT("A valid TargetPlatform is expected"));

	// note that shader formats can be shared across the target platforms
	TArray<FName> ShaderFormats;
	TargetPlatform->GetAllTargetedShaderFormats(ShaderFormats);
	if (ShaderFormats.Num() > 0)
	{
		FShaderLibrariesCollection::Impl->FinishPopulateShaderCode(ShaderCodeDir, MetaDataPath, ShaderFormats);
	}
}

bool FShaderLibraryCooker::MergeShaderCodeArchive(const TArray<FString>& CookedMetadataDirs, const FString& OutputDir, TArray<FString>& OutWrittenFiles)
{
	OutWrittenFiles.Empty();

	bool bNeedsDeterministicOrder = false;
	GConfig->GetBool(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("bDeterministicShaderCodeOrder"), bNeedsDeterministicOrder, GGameIni);

	static const FRegexPattern ShaderArchivePattern(ShaderArchivePatternStr);
	static const FRegexPattern StableInfoPattern(StableInfoPatternStr);

	// Map of file name to the archive we are unioning
	TMap<FString, FEditorShaderCodeArchive> ShaderCodeArchives;
	TMap<FString, FEditorShaderStableInfo> ShaderStableInfos;

	for (const FString& MetadataDir : CookedMetadataDirs)
	{
		const FString ShaderCodeDir = MetadataDir / TEXT("ShaderLibrarySource");
		const FString ShaderStableInfoDir = MetadataDir / TEXT("PipelineCaches");

		TArray<FString> ShaderBytecodeFiles;
		IFileManager::Get().FindFiles(ShaderBytecodeFiles, *ShaderCodeDir, *ShaderExtension);
		for (const FString& ByteCodeFile : ShaderBytecodeFiles)
		{
			if (ShaderCodeArchives.Contains(ByteCodeFile))
			{
				ShaderCodeArchives[ByteCodeFile].AddShaderCodeLibraryFromDirectory(ShaderCodeDir);
			}
			else
			{
				FRegexMatcher FindShaderFormat(ShaderArchivePattern, ByteCodeFile);
				if (ensureMsgf(FindShaderFormat.FindNext(), TEXT("Unable to parse out shader format from %s"), *ByteCodeFile))
				{
					const FName ShaderFormat(*FindShaderFormat.GetCaptureGroup(2));
					FEditorShaderCodeArchive NewCodeArchive(ShaderFormat, bNeedsDeterministicOrder);
					NewCodeArchive.OpenLibrary(FindShaderFormat.GetCaptureGroup(1));
					NewCodeArchive.AddShaderCodeLibraryFromDirectory(ShaderCodeDir);

					ShaderCodeArchives.FindOrAdd(ByteCodeFile, MoveTemp(NewCodeArchive));
				}
			}
		}

		TArray<FString> StableInfoFiles;
		IFileManager::Get().FindFiles(StableInfoFiles, *ShaderStableInfoDir, *StableExtension);
		for (const FString& StableInfoFile : StableInfoFiles)
		{
			if (ShaderStableInfos.Contains(StableInfoFile))
			{
				ShaderStableInfos[StableInfoFile].AddShaderCodeLibraryFromDirectory(ShaderStableInfoDir,
					FEditorShaderStableInfo::EMergeRule::OverwriteUnmodifiedWarnModified);
			}
			else
			{
				FRegexMatcher FindShaderInfo(StableInfoPattern, StableInfoFile);
				if (ensureMsgf(FindShaderInfo.FindNext(), TEXT("Unable to parse out shader format from %s"), *StableInfoFile))
				{
					const FName ShaderFormat(*FindShaderInfo.GetCaptureGroup(2));
					FEditorShaderStableInfo NewStableInfo(ShaderFormat);
					NewStableInfo.OpenLibrary(FindShaderInfo.GetCaptureGroup(1));
					NewStableInfo.AddShaderCodeLibraryFromDirectory(ShaderStableInfoDir,
						FEditorShaderStableInfo::EMergeRule::OverwriteUnmodifiedWarnModified);

					ShaderStableInfos.FindOrAdd(StableInfoFile, MoveTemp(NewStableInfo));
				}
			}
		}
	}

	const FString OutShaderCodeDir = OutputDir / TEXT("ShaderLibrarySource");
	const FString OutContentDir = OutputDir / TEXT("../Content");
	const FString OutShaderStableInfoDir = OutputDir / TEXT("PipelineCaches");

	bool bSuccess = true;
	for (TPair<FString, FEditorShaderCodeArchive>& ShaderCodeArchivePair : ShaderCodeArchives)
	{
		TArray<FString> CreatedFiles;
		if (ShaderCodeArchivePair.Value.SaveToDisk(OutShaderCodeDir, TEXT(""), false, &CreatedFiles))
		{
			OutWrittenFiles.Append(MoveTemp(CreatedFiles));
		}
		else
		{
			UE_LOG(LogShaderLibrary, Error, TEXT("Failed to save %s"), *ShaderCodeArchivePair.Key);
			bSuccess = false;
		}
		CreatedFiles.Reset();
		if (ShaderCodeArchivePair.Value.SaveToDisk(OutContentDir, TEXT(""), false, &CreatedFiles))
		{
			OutWrittenFiles.Append(MoveTemp(CreatedFiles));
		}
		else
		{
			UE_LOG(LogShaderLibrary, Error, TEXT("Failed to save to Content Dir %s"), *ShaderCodeArchivePair.Key);
			bSuccess = false;
		}
	}

	for (TPair<FString, FEditorShaderStableInfo>& ShaderStableInfoPair : ShaderStableInfos)
	{
		FString WrittenFile;
		if (ShaderStableInfoPair.Value.SaveToDisk(OutShaderStableInfoDir, WrittenFile))
		{
			OutWrittenFiles.Add(MoveTemp(WrittenFile));
		}
		else
		{
			UE_LOG(LogShaderLibrary, Error, TEXT("Failed to save %s"), *ShaderStableInfoPair.Key);
			bSuccess = false;
		}
	}

	return bSuccess;
}

bool FShaderLibraryCooker::SaveShaderLibraryWithoutChunking(const ITargetPlatform* TargetPlatform, FString const& Name,
	FString const& SandboxDestinationPath, FString const& SandboxMetadataPath, TArray<FString>& PlatformSCLCSVPaths, FString& OutErrorMessage,
	bool& bOutHasData)
{
	// note that shader formats can be shared across the target platforms
	TArray<FName> ShaderFormats;
	TargetPlatform->GetAllTargetedShaderFormats(ShaderFormats);
	if (ShaderFormats.IsEmpty())
	{
		bOutHasData = false;
		return true;
	}
	bOutHasData = true;

	const FString& ShaderCodeDir = SandboxDestinationPath;
	const FString& MetaDataPath = SandboxMetadataPath;

	checkf(FShaderLibrariesCollection::Impl != nullptr, TEXT("FShaderLibraryCooker was not initialized properly"));
	checkf(TargetPlatform, TEXT("A valid TargetPlatform is expected"));

	const bool bSaved = FShaderLibrariesCollection::Impl->SaveShaderCode(ShaderCodeDir, MetaDataPath, ShaderFormats, PlatformSCLCSVPaths);
	if (UNLIKELY(!bSaved))
	{
		OutErrorMessage = FString::Printf(TEXT("Saving shared material shader code library failed for %s."),
			*TargetPlatform->PlatformName());
		return false;
	}

	return true;
}

bool FShaderLibraryCooker::SaveShaderLibraryChunk(int32 ChunkId, const TSet<FName>& InPackagesInChunk, const ITargetPlatform* TargetPlatform,
	const FString& SandboxDestinationPath, const FString& SandboxMetadataPath, TArray<FString>& OutChunkFilenames, bool& bOutHasData)
{
	TArray<FName> ShaderFormats;
	TargetPlatform->GetAllTargetedShaderFormats(ShaderFormats);
	if (ShaderFormats.IsEmpty())
	{
		bOutHasData = false;
		return true;
	}
	bOutHasData = true;

	checkf(FShaderLibrariesCollection::Impl != nullptr, TEXT("FShaderLibraryCooker was not initialized properly"));
	checkf(TargetPlatform, TEXT("A valid TargetPlatform is expected"));
	return FShaderLibrariesCollection::Impl->SaveShaderCodeChunk(ChunkId, InPackagesInChunk, ShaderFormats,
		SandboxDestinationPath, SandboxMetadataPath, OutChunkFilenames);
}

#endif// WITH_EDITOR

void FShaderCodeLibrary::SafeAssignHash(FRHIShader* InShader, const FSHAHash& Hash)
{
	if (InShader)
	{
		InShader->SetHash(Hash);
	}
}

FDelegateHandle FShaderCodeLibrary::RegisterSharedShaderCodeRequestDelegate_Handle(const FSharedShaderCodeRequest::FDelegate& Delegate)
{
	return OnSharedShaderCodeRequest.Add(Delegate);
}

void FShaderCodeLibrary::UnregisterSharedShaderCodeRequestDelegate_Handle(FDelegateHandle Handle)
{
	OnSharedShaderCodeRequest.Remove(Handle);
}

void FShaderCodeLibrary::DontOpenPluginShaderLibraryOnMount(const FString& PluginName)
{
	check(IsInGameThread());
	if (FApp::CanEverRender())
	{
		UE::ShaderLibrary::Private::PluginsToIgnoreOnMount.Add(PluginName);
	}
}

void FShaderCodeLibrary::OpenPluginShaderLibrary(IPlugin& Plugin, bool bMonolithicOnly)
{
	if (Plugin.CanContainContent() && Plugin.IsEnabled() && FApp::CanEverRender())
	{
		// load any shader libraries that may exist in this plugin
		if (!bMonolithicOnly)
		{
			// Chunked libraries in plugins that are not built in (i.e. ones that were cooked separately as DLC) are not supported atm. This is because the main game can be cooked without chunks (-fastcook), but still needs
			// to load the same plugins, so it would not know which ChunkIDs to try.
			// Plugins that are built-in can be chunked, but their shaders don't go into a separate library (cooker doesn't separate that atm), everything goes into main project's library.
			UE_LOG(LogShaderLibrary, Display, TEXT("Opening a chunked shader library for plugin '%s' is ignored. Chunked libraries for plugins are not supported."), *Plugin.GetName());
		}
		bMonolithicOnly = true;
		FShaderCodeLibrary::OpenLibrary(Plugin.GetName(), Plugin.GetContentDir(), bMonolithicOnly);
	}
}

// FNamedShaderLibrary methods

// At runtime, open shader code collection for specified shader platform. Returns true if new code was opened
bool UE::ShaderLibrary::Private::FNamedShaderLibrary::OpenShaderCode(const FString& ShaderCodeDir, FString const& Library)
{
	// check if any of the components has this content
	{
		FRWScopeLock ReadLock(ComponentsMutex, SLT_Write);
		for (TUniquePtr<FShaderLibraryInstance>& Component : Components)
		{
			if (Component->HasContentFrom(ShaderCodeDir, Library))
			{
				// in this context, "false" means "no new library was opened"
				return false;
			}
		}
	}

	FShaderLibraryInstance* LibraryInstance = FShaderLibraryInstance::Create(ShaderPlatform, ShaderCodeDir, Library);
	if (LibraryInstance == nullptr)
	{
		UE_LOG(LogShaderLibrary, Verbose, TEXT("Cooked Context: No Shared Shader Library for: %s and native library not supported."), *Library);
		// PVS reports "The function was exited without releasing the 'LibraryInstance' pointer. A memory leak is possible", which is totally bogus here
		return false; //-V773
	}

	FRWScopeLock WriteLock(ComponentsMutex, SLT_Write);

	// re-check that no one has added the same library while we were creating it. If so, delete ours
	for (TUniquePtr<FShaderLibraryInstance>& Component : Components)
	{
		if (Component->HasContentFrom(ShaderCodeDir, Library))
		{
			// in this context, "false" means "no new library was opened"
			delete LibraryInstance;
			return false;
		}
	}

	if (LibraryInstance->Library->IsNativeLibrary())
	{
		UE_LOG(LogShaderLibrary, Display, TEXT("Cooked Context: Loaded Native Shared Shader Library %s"), *Library);
	}
	else
	{
		UE_LOG(LogShaderLibrary, Display, TEXT("Cooked Context: Using Shared Shader Library %s"), *Library);
	}

	Components.Emplace(LibraryInstance);
	return true;
}

FShaderLibraryInstance* UE::ShaderLibrary::Private::FNamedShaderLibrary::FindShaderLibraryForShaderMap(const FSHAHash& Hash, int32& OutShaderMapIndex)
{
	FRWScopeLock ReadLock(ComponentsMutex, SLT_ReadOnly);

	// Search in library opened order
	for (const TUniquePtr<FShaderLibraryInstance>& Instance : Components)
	{
		const int32 ShaderMapIndex = Instance->Library->FindShaderMapIndex(Hash);
		if (ShaderMapIndex != INDEX_NONE)
		{
			OutShaderMapIndex = ShaderMapIndex;
			return Instance.Get();
		}
	}
	return nullptr;
}

FShaderLibraryInstance* UE::ShaderLibrary::Private::FNamedShaderLibrary::FindShaderLibraryForShader(const FSHAHash& Hash, int32& OutShaderIndex)
{
	FRWScopeLock ReadLock(ComponentsMutex, SLT_ReadOnly);

	// Search in library opened order
	for (const TUniquePtr<FShaderLibraryInstance>& Instance : Components)
	{
		const int32 ShaderIndex = Instance->Library->FindShaderIndex(Hash);
		if (ShaderIndex != INDEX_NONE)
		{
			OutShaderIndex = ShaderIndex;
			return Instance.Get();
		}
	}
	return nullptr;
}

uint32 UE::ShaderLibrary::Private::FNamedShaderLibrary::GetShaderCount(void)
{
	FRWScopeLock ReadLock(ComponentsMutex, SLT_ReadOnly);

	int ShaderCount = 0;
	for (const TUniquePtr<FShaderLibraryInstance>& Instance : Components)
	{
		ShaderCount += Instance->Library->GetNumShaders();
	}
	return ShaderCount;
}

#if UE_SHADERLIB_WITH_INTROSPECTION
void UE::ShaderLibrary::Private::FNamedShaderLibrary::DumpLibraryContents(const FString& Prefix)
{
	FRWScopeLock ReadLock(ComponentsMutex, SLT_ReadOnly);

	int32 ComponentIdx = 0;
	for (const TUniquePtr<FShaderLibraryInstance>& Instance : Components)
	{
		UE_LOG(LogShaderLibrary, Display, TEXT("%sComponent %d: Native=%s Shaders: %d Name: %s"),
			*Prefix, ComponentIdx, Instance->Library->IsNativeLibrary() ? TEXT("yes") : TEXT("no"), Instance->GetNumShaders(), *Instance->Library->GetName() );
		++ComponentIdx;
	}
}
#endif
