// Copyright Epic Games, Inc. All Rights Reserved.

#include "TargetDomain/TargetDomainUtils.h"

#include "Algo/BinarySearch.h"
#include "Algo/IsSorted.h"
#include "Algo/Sort.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "Cooker/PackageBuildDependencyTracker.h"
#include "DerivedDataBuildDefinition.h"
#include "DerivedDataBuildKey.h"
#include "DerivedDataSharedString.h"
#include "EditorDomain/EditorDomain.h"
#include "EditorDomain/EditorDomainUtils.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "IO/IoDispatcher.h"
#include "IO/IoHash.h"
#include "Misc/App.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/StringBuilder.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinaryWriter.h"
#include "Serialization/PackageWriter.h"
#include "ZenStoreHttpClient.h"

namespace UE::TargetDomain
{

/**
 * Reads / writes an oplog for EditorDomain BuildDefinitionLists.
 * TODO: Reduce duplication between this class and FZenStoreWriter
 */
class FEditorDomainOplog
{
public:
	FEditorDomainOplog();

	bool IsValid() const;
	void CommitPackage(FName PackageName, TArrayView<IPackageWriter::FCommitAttachmentInfo> Attachments);
	FCbObject GetOplogAttachment(FName PackageName, FUtf8StringView AttachmentKey);

private:
	struct FOplogEntry
	{
		struct FAttachment
		{
			const UTF8CHAR* Key;
			FIoHash Hash;
		};

		TArray<FAttachment> Attachments;
	};

	void InitializeRead();
	
	FCbAttachment CreateAttachment(FSharedBuffer AttachmentData);
	FCbAttachment CreateAttachment(FCbObject AttachmentData)
	{
		return CreateAttachment(AttachmentData.GetBuffer().ToShared());
	}

	static void StaticInit();
	static bool IsReservedOplogKey(FUtf8StringView Key);

	UE::FZenStoreHttpClient HttpClient;
	FCriticalSection Lock;
	TMap<FName, FOplogEntry> Entries;
	bool bConnectSuccessful = false;
	bool bInitializedRead = false;

	static TArray<const UTF8CHAR*> ReservedOplogKeys;
};
TUniquePtr<FEditorDomainOplog> GEditorDomainOplog;

bool TryCreateKey(FName PackageName, TConstArrayView<FName> SortedBuildDependencies, FIoHash* OutHash, FString* OutErrorMessage)
{
	IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (!AssetRegistry)
	{
		if (OutErrorMessage) *OutErrorMessage = TEXT("AssetRegistry is unavailable.");
		return false;
	}
	FEditorDomain* EditorDomain = FEditorDomain::Get();
	if (!EditorDomain)
	{
		if (OutErrorMessage) *OutErrorMessage = TEXT("EditorDomain is unavailable.");
		return false;
	}
	FBlake3 KeyBuilder;
	UE::EditorDomain::FPackageDigest PackageDigest = EditorDomain->GetPackageDigest(PackageName);
	if (!PackageDigest.IsSuccessful())
	{
		if (OutErrorMessage) *OutErrorMessage = PackageDigest.GetStatusString();
		return false;
	}
	KeyBuilder.Update(&PackageDigest.Hash, sizeof(PackageDigest.Hash));

	for (FName DependencyName : SortedBuildDependencies)
	{
		PackageDigest = EditorDomain->GetPackageDigest(PackageName);
		if (!PackageDigest.IsSuccessful())
		{
			if (OutErrorMessage)
			{
				*OutErrorMessage = FString::Printf(TEXT("Could not create PackageDigest for %s: %s"),
					*DependencyName.ToString(), *PackageDigest.GetStatusString());
			}
			return false;
		}
		KeyBuilder.Update(&PackageDigest.Hash, sizeof(PackageDigest.Hash));
	}

	if (OutHash)
	{
		*OutHash = KeyBuilder.Finalize();
	}
	return true;
}

bool TryCollectKeyAndDependencies(UPackage* Package, const ITargetPlatform* TargetPlatform, FIoHash* OutHash, TArray<FName>* OutBuildDependencies,
	TArray<FName>* OutRuntimeOnlyDependencies, FString* OutErrorMessage)
{
	if (!Package)
	{
		if (OutErrorMessage) *OutErrorMessage = TEXT("Invalid null package.");
		return false;
	}
	IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (!AssetRegistry)
	{
		if (OutErrorMessage) *OutErrorMessage = TEXT("AssetRegistry is unavailable.");
		return false;
	}
	FEditorDomain* EditorDomain = FEditorDomain::Get();
	if (!EditorDomain)
	{
		if (OutErrorMessage) *OutErrorMessage = TEXT("EditorDomain is unavailable.");
		return false;
	}

	FName PackageName = Package->GetFName();
	TSet<FName> BuildDependencies;
	TSet<FName> RuntimeOnlyDependencies;

	TArray<FName> AssetDependencies;
	AssetRegistry->GetDependencies(PackageName, AssetDependencies, UE::AssetRegistry::EDependencyCategory::Package,
		UE::AssetRegistry::EDependencyQuery::Game);

	FPackageBuildDependencyTracker& Tracker = FPackageBuildDependencyTracker::Get();

	if (Tracker.IsEnabled())
	{
		TArray<FBuildDependencyAccessData> AccessDatas = Tracker.GetAccessDatas(PackageName);

		BuildDependencies.Reserve(AccessDatas.Num());
		for (FBuildDependencyAccessData& AccessData : AccessDatas)
		{
			if (AccessData.TargetPlatform == TargetPlatform || AccessData.TargetPlatform == nullptr)
			{
				BuildDependencies.Add(AccessData.ReferencedPackage);
			}
		}

		RuntimeOnlyDependencies.Reserve(AssetDependencies.Num());
		for (FName DependencyName : AssetDependencies)
		{
			if (!BuildDependencies.Contains(DependencyName))
			{
				RuntimeOnlyDependencies.Add(DependencyName);
			}
		}
	}
	else
	{
		// Defensively treat all asset dependencies as build dependencies and have zero runtime only dependencies
		BuildDependencies.Append(AssetDependencies);
	}

	TArray<FName> SortedBuild;
	SortedBuild = BuildDependencies.Array();
	TStringBuilder<256> StringBuffer;
	FName TransientPackageName = GetTransientPackage()->GetFName();
	auto IsTransientPackageName = [&StringBuffer, TransientPackageName](FName PackageName)
	{
		PackageName.ToString(StringBuffer);
		return PackageName == TransientPackageName ||
			FPackageName::IsMemoryPackage(StringBuffer) ||
			FPackageName::IsScriptPackage(StringBuffer);
	};
	SortedBuild.RemoveAllSwap(IsTransientPackageName, EAllowShrinking::No);
	SortedBuild.Sort(FNameLexicalLess());
	TArray<FName> SortedRuntimeOnly;
	SortedRuntimeOnly = RuntimeOnlyDependencies.Array();
	SortedRuntimeOnly.RemoveAllSwap(IsTransientPackageName, EAllowShrinking::No);
	SortedRuntimeOnly.Sort(FNameLexicalLess());

	if (!TryCreateKey(PackageName, SortedBuild, OutHash, OutErrorMessage))
	{
		return false;
	}

	if (OutBuildDependencies)
	{
		*OutBuildDependencies = MoveTemp(SortedBuild);
	}
	if (OutRuntimeOnlyDependencies)
	{
		*OutRuntimeOnlyDependencies = MoveTemp(SortedRuntimeOnly);
	}
	if (OutErrorMessage)
	{
		OutErrorMessage->Reset();
	}
	
	return true;
}

FCbObject CollectDependenciesObject(UPackage* Package, const ITargetPlatform* TargetPlatform, FString* ErrorMessage)
{
	FIoHash TargetDomainKey;
	TArray<FName> BuildDependencies;
	TArray<FName> RuntimeOnlyDependencies;
	if (!UE::TargetDomain::TryCollectKeyAndDependencies(Package, TargetPlatform, &TargetDomainKey, &BuildDependencies, &RuntimeOnlyDependencies, ErrorMessage))
	{
		return FCbObject();
	}

	FCbWriter Writer;
	Writer.BeginObject();
	Writer << "targetdomainkey" << TargetDomainKey;
	TStringBuilder<128> PackageNameBuffer;
	if (!BuildDependencies.IsEmpty())
	{
		Writer.BeginArray("builddependencies");
		for (FName DependencyName : BuildDependencies)
		{
			DependencyName.ToString(PackageNameBuffer);
			Writer << PackageNameBuffer;
		}
		Writer.EndArray();
	}
	if (!RuntimeOnlyDependencies.IsEmpty())
	{
		Writer.BeginArray("runtimeonlydependencies");
		for (FName DependencyName : RuntimeOnlyDependencies)
		{
			DependencyName.ToString(PackageNameBuffer);
			Writer << PackageNameBuffer;
		}
		Writer.EndArray();
	}
	Writer.EndObject();
	return Writer.Save().AsObject();
}

FCbObject BuildDefinitionListToObject(TConstArrayView<UE::DerivedData::FBuildDefinition> BuildDefinitionList)
{
	using namespace UE::DerivedData;

	if (BuildDefinitionList.IsEmpty())
	{
		return FCbObject();
	}

	TArray<const FBuildDefinition*> Sorted;
	Sorted.Reserve(BuildDefinitionList.Num());
	for (const FBuildDefinition& BuildDefinition : BuildDefinitionList)
	{
		Sorted.Add(&BuildDefinition);
	}
	Sorted.Sort([](const FBuildDefinition& A, const FBuildDefinition& B)
		{
			return A.GetKey().Hash < B.GetKey().Hash;
		});

	FCbWriter Writer;
	Writer.BeginObject();
	Writer.BeginArray("BuildDefinitions");
	for (const FBuildDefinition* BuildDefinition : Sorted)
	{
		BuildDefinition->Save(Writer);
	}
	Writer.EndArray();
	return Writer.Save().AsObject();
}

void FetchCookAttachments(TArrayView<FName> PackageNames, const ITargetPlatform* TargetPlatform, ICookedPackageWriter* PackageWriter,
	TUniqueFunction<void(FName PackageName, FCookAttachments&& Results)>&& Callback)
{
	for (FName PackageName : PackageNames)
	{
		FCookAttachments Result;
		const ANSICHAR* DependenciesKey = "Dependencies";
		FCbObject DependenciesObj;
		if (TargetPlatform)
		{
			check(PackageWriter);
			DependenciesObj = PackageWriter->GetOplogAttachment(PackageName, DependenciesKey);
		}
		else
		{
			if (!GEditorDomainOplog)
			{
				Callback(PackageName, MoveTemp(Result));
				continue;
			}
			DependenciesObj = GEditorDomainOplog->GetOplogAttachment(PackageName, DependenciesKey);
		}

		Result.StoredKey = DependenciesObj["targetdomainkey"].AsHash();
		if (Result.StoredKey.IsZero())
		{
			Callback(PackageName, MoveTemp(Result));
			continue;
		}

		for (FCbFieldView DepObj : DependenciesObj["builddependencies"])
		{
			if (FUtf8StringView DependencyName(DepObj.AsString()); !DependencyName.IsEmpty())
			{
				Result.BuildDependencies.Add(FName(DependencyName));
			}
		}

		for (FCbFieldView DepObj : DependenciesObj["runtimeonlydependencies"])
		{
			if (FUtf8StringView DependencyName(DepObj.AsString()); !DependencyName.IsEmpty())
			{
				Result.RuntimeOnlyDependencies.Add(FName(DependencyName));
			}
		}

		const ANSICHAR* BuildDefinitionListKey = "BuildDefinitionList";
		FCbObject BuildDefinitionListObj;
		if (TargetPlatform)
		{
			BuildDefinitionListObj = PackageWriter->GetOplogAttachment(PackageName, BuildDefinitionListKey);
		}
		else
		{
			BuildDefinitionListObj = GEditorDomainOplog->GetOplogAttachment(PackageName, BuildDefinitionListKey);
		}

		for (FCbField BuildDefinitionObj : BuildDefinitionListObj)
		{
			UE::DerivedData::FOptionalBuildDefinition BuildDefinition =
				UE::DerivedData::FBuildDefinition::Load(TEXTVIEW("TargetDomainBuildDefinitionList"),
					BuildDefinitionObj.AsObject());
			if (!BuildDefinition)
			{
				Result.BuildDefinitionList.Empty();
				break;
			}
			Result.BuildDefinitionList.Add(MoveTemp(BuildDefinition).Get());
		}
		Result.bValid = true;
		Callback(PackageName, MoveTemp(Result));
	}
}

bool IsCookAttachmentsValid(FName PackageName, const FCookAttachments& CookAttachments)
{
	if (!CookAttachments.bValid)
	{
		return false;
	}

	FIoHash CurrentKey;
	if (!TryCreateKey(PackageName, CookAttachments.BuildDependencies, &CurrentKey, nullptr /* OutErrorMessage */))
	{
		return false;
	}

	if (CookAttachments.StoredKey != CurrentKey)
	{
		return false;
	}

	return true;
}

bool IsIterativeEnabled(FName PackageName, bool bAllowAllClasses)
{
	IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (!AssetRegistry)
	{
		return false;
	}
	TOptional<FAssetPackageData> PackageDataOpt = AssetRegistry->GetAssetPackageDataCopy(PackageName);
	if (!PackageDataOpt)
	{
		return false;
	}
	FAssetPackageData& PackageData = *PackageDataOpt;

	if (!bAllowAllClasses)
	{
		auto LogInvalidDueTo = [](FName PackageName, FName ClassPath)
			{
				UE_LOG(LogEditorDomain, Verbose, TEXT("NonIterative Package %s due to %s"), *PackageName.ToString(), *ClassPath.ToString());
			};

		UE::EditorDomain::FClassDigestMap& ClassDigests = UE::EditorDomain::GetClassDigests();
		FReadScopeLock ClassDigestsScopeLock(ClassDigests.Lock);
		for (FName ClassName : PackageData.ImportedClasses)
		{
			FTopLevelAssetPath ClassPath(WriteToString<256>(ClassName).ToView());
			UE::EditorDomain::FClassDigestData* ExistingData = nullptr;
			if (ClassPath.IsValid())
			{
				ExistingData = ClassDigests.Map.Find(ClassPath);
			}
			if (!ExistingData)
			{
				// !ExistingData -> !allowed, because caller has already called CalculatePackageDigest, so all
				// existing classes in the package have been added to ClassDigests.
				LogInvalidDueTo(PackageName, ClassName);
				return false;
			}
			if (!ExistingData->bNative)
			{
				// TODO: We need to add a way to mark non-native classes (there can be many of them) as allowed or denied.
				// Currently we are allowing them all, so long as their closest native is allowed. But this is not completely
				// safe to do, because non-native classes can add constructionevents that e.g. use the Random function.
				ExistingData = ClassDigests.Map.Find(ExistingData->ClosestNative);
				if (!ExistingData)
				{
					LogInvalidDueTo(PackageName, ClassName);
					return false;
				}
			}
			if (!ExistingData->bTargetIterativeEnabled)
			{
				LogInvalidDueTo(PackageName, ClassName);
				return false;
			}
		}
	}
	return true;
}

TArray<const UTF8CHAR*> FEditorDomainOplog::ReservedOplogKeys;

FEditorDomainOplog::FEditorDomainOplog()
#if UE_WITH_ZEN
: HttpClient(TEXT("localhost"), UE::Zen::FZenServiceInstance::GetAutoLaunchedPort() > 0 ? UE::Zen::FZenServiceInstance::GetAutoLaunchedPort() : 8558)
#else
: HttpClient(TEXT("localhost"), 8558)
#endif
{
	StaticInit();

	FString ProjectId = FApp::GetZenStoreProjectId();
	FString OplogId = TEXT("EditorDomain");

	FString RootDir = FPaths::RootDir();
	FString EngineDir = FPaths::EngineDir();
	FPaths::NormalizeDirectoryName(EngineDir);
	FString ProjectDir = FPaths::ProjectDir();
	FPaths::NormalizeDirectoryName(ProjectDir);
	FString ProjectPath = FPaths::GetProjectFilePath();
	FPaths::NormalizeFilename(ProjectPath);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	FString AbsServerRoot = PlatformFile.ConvertToAbsolutePathForExternalAppForRead(*RootDir);
	FString AbsEngineDir = PlatformFile.ConvertToAbsolutePathForExternalAppForRead(*EngineDir);
	FString AbsProjectDir = PlatformFile.ConvertToAbsolutePathForExternalAppForRead(*ProjectDir);
	FString ProjectFilePath = PlatformFile.ConvertToAbsolutePathForExternalAppForRead(*ProjectPath);

#if UE_WITH_ZEN
	if (UE::Zen::IsDefaultServicePresent())
	{
		bool IsLocalConnection = HttpClient.GetZenServiceInstance().IsServiceRunningLocally();
		HttpClient.TryCreateProject(ProjectId, OplogId, AbsServerRoot, AbsEngineDir, AbsProjectDir, IsLocalConnection ? ProjectFilePath : FStringView());
		HttpClient.TryCreateOplog(ProjectId, OplogId, TEXT("") /*InOplogMarkerFile*/, false /* bFullBuild */);
	}
#endif
}

void FEditorDomainOplog::InitializeRead()
{
	if (bInitializedRead)
	{
		return;
	}
	UE_LOG(LogEditorDomain, Display, TEXT("Fetching EditorDomain oplog..."));

	TFuture<FIoStatus> FutureOplogStatus = HttpClient.GetOplog().Next([this](TIoStatusOr<FCbObject> OplogStatus)
		{
			if (!OplogStatus.IsOk())
			{
				return OplogStatus.Status();
			}

			FCbObject Oplog = OplogStatus.ConsumeValueOrDie();

			for (FCbField& EntryObject : Oplog["entries"])
			{
				FUtf8StringView PackageName = EntryObject["key"].AsString();
				if (PackageName.IsEmpty())
				{
					continue;
				}
				FName PackageFName(PackageName);
				FOplogEntry& Entry = Entries.FindOrAdd(PackageFName);
				Entry.Attachments.Empty();

				for (FCbFieldView Field : EntryObject)
				{
					FUtf8StringView FieldName = Field.GetName();
					if (IsReservedOplogKey(FieldName))
					{
						continue;
					}
					if (Field.IsHash())
					{
						const UTF8CHAR* AttachmentId = UE::FZenStoreHttpClient::FindOrAddAttachmentId(FieldName);
						Entry.Attachments.Add({ AttachmentId, Field.AsHash() });
					}
				}
				Entry.Attachments.Shrink();
				check(Algo::IsSorted(Entry.Attachments, [](const FOplogEntry::FAttachment& A, const FOplogEntry::FAttachment& B)
					{
						return FUtf8StringView(A.Key).Compare(FUtf8StringView(B.Key), ESearchCase::IgnoreCase) < 0;
					}));
			}

			return FIoStatus::Ok;
		});
	FutureOplogStatus.Get();
	bInitializedRead = true;
}

FCbAttachment FEditorDomainOplog::CreateAttachment(FSharedBuffer AttachmentData)
{
	FCompressedBuffer CompressedBuffer = FCompressedBuffer::Compress(AttachmentData);
	check(!CompressedBuffer.IsNull());
	return FCbAttachment(CompressedBuffer);
}

void FEditorDomainOplog::StaticInit()
{
	if (ReservedOplogKeys.Num() > 0)
	{
		return;
	}

	ReservedOplogKeys.Append({ UTF8TEXT("key") });
	Algo::Sort(ReservedOplogKeys, [](const UTF8CHAR* A, const UTF8CHAR* B)
		{
			return FUtf8StringView(A).Compare(FUtf8StringView(B), ESearchCase::IgnoreCase) < 0;
		});;
}

bool FEditorDomainOplog::IsReservedOplogKey(FUtf8StringView Key)
{
	int32 Index = Algo::LowerBound(ReservedOplogKeys, Key,
		[](const UTF8CHAR* Existing, FUtf8StringView Key)
		{
			return FUtf8StringView(Existing).Compare(Key, ESearchCase::IgnoreCase) < 0;
		});
	return Index != ReservedOplogKeys.Num() &&
		FUtf8StringView(ReservedOplogKeys[Index]).Equals(Key, ESearchCase::IgnoreCase);
}

bool FEditorDomainOplog::IsValid() const
{
	return HttpClient.IsConnected();
}

void FEditorDomainOplog::CommitPackage(FName PackageName, TArrayView<IPackageWriter::FCommitAttachmentInfo> Attachments)
{
	FScopeLock ScopeLock(&Lock);

	FCbPackage Pkg;

	TArray<FCbAttachment, TInlineAllocator<2>> CbAttachments;
	int32 NumAttachments = Attachments.Num();
	FOplogEntry& Entry = Entries.FindOrAdd(PackageName);
	Entry.Attachments.Empty(NumAttachments);
	if (NumAttachments)
	{
		TArray<const IPackageWriter::FCommitAttachmentInfo*, TInlineAllocator<2>> SortedAttachments;
		SortedAttachments.Reserve(NumAttachments);
		for (const IPackageWriter::FCommitAttachmentInfo& AttachmentInfo : Attachments)
		{
			SortedAttachments.Add(&AttachmentInfo);
		}
		SortedAttachments.Sort([](const IPackageWriter::FCommitAttachmentInfo& A, const IPackageWriter::FCommitAttachmentInfo& B)
			{
				return A.Key.Compare(B.Key, ESearchCase::IgnoreCase) < 0;
			});
		CbAttachments.Reserve(NumAttachments);
		for (const IPackageWriter::FCommitAttachmentInfo* AttachmentInfo : SortedAttachments)
		{
			const FCbAttachment& CbAttachment = CbAttachments.Add_GetRef(CreateAttachment(AttachmentInfo->Value));
			check(!IsReservedOplogKey(AttachmentInfo->Key));
			Pkg.AddAttachment(CbAttachment);
			Entry.Attachments.Add(FOplogEntry::FAttachment{
				UE::FZenStoreHttpClient::FindOrAddAttachmentId(AttachmentInfo->Key), CbAttachment.GetHash() });
		}
	}

	FCbWriter PackageObj;
	FString PackageNameKey = PackageName.ToString();
	PackageNameKey.ToLowerInline();
	PackageObj.BeginObject();
	PackageObj << "key" << PackageNameKey;
	for (int32 Index = 0; Index < NumAttachments; ++Index)
	{
		FCbAttachment& CbAttachment = CbAttachments[Index];
		FOplogEntry::FAttachment& EntryAttachment = Entry.Attachments[Index];
		PackageObj << EntryAttachment.Key << CbAttachment;
	}
	PackageObj.EndObject();

	FCbObject Obj = PackageObj.Save().AsObject();
	Pkg.SetObject(Obj);
	HttpClient.AppendOp(Pkg);
}

// Note that this is destructive - we yank out the buffer memory from the 
// IoBuffer into the FSharedBuffer
FSharedBuffer IoBufferToSharedBuffer(FIoBuffer& InBuffer)
{
	InBuffer.EnsureOwned();
	const uint64 DataSize = InBuffer.DataSize();
	uint8* DataPtr = InBuffer.Release().ValueOrDie();
	return FSharedBuffer{ FSharedBuffer::TakeOwnership(DataPtr, DataSize, FMemory::Free) };
};

FCbObject FEditorDomainOplog::GetOplogAttachment(FName PackageName, FUtf8StringView AttachmentKey)
{
	FScopeLock ScopeLock(&Lock);
	InitializeRead();

	FOplogEntry* Entry = Entries.Find(PackageName);
	if (!Entry)
	{
		return FCbObject();
	}

	const UTF8CHAR* AttachmentId = UE::FZenStoreHttpClient::FindAttachmentId(AttachmentKey);
	if (!AttachmentId)
	{
		return FCbObject();
	}
	FUtf8StringView AttachmentIdView(AttachmentId);

	int32 AttachmentIndex = Algo::LowerBound(Entry->Attachments, AttachmentIdView,
		[](const FOplogEntry::FAttachment& Existing, FUtf8StringView AttachmentIdView)
		{
			return FUtf8StringView(Existing.Key).Compare(AttachmentIdView, ESearchCase::IgnoreCase) < 0;
		});
	if (AttachmentIndex == Entry->Attachments.Num())
	{
		return FCbObject();
	}
	const FOplogEntry::FAttachment& Existing = Entry->Attachments[AttachmentIndex];
	if (!FUtf8StringView(Existing.Key).Equals(AttachmentIdView, ESearchCase::IgnoreCase))
	{
		return FCbObject();
	}
	TIoStatusOr<FIoBuffer> BufferResult = HttpClient.ReadOpLogAttachment(WriteToString<48>(Existing.Hash));
	if (!BufferResult.IsOk())
	{
		return FCbObject();
	}
	FIoBuffer Buffer = BufferResult.ValueOrDie();
	if (Buffer.DataSize() == 0)
	{
		return FCbObject();
	}

	FSharedBuffer SharedBuffer = IoBufferToSharedBuffer(Buffer);
	return FCbObject(SharedBuffer);
}

void CommitEditorDomainCookAttachments(FName PackageName, TArrayView<IPackageWriter::FCommitAttachmentInfo> Attachments)
{
	if (!GEditorDomainOplog)
	{
		return;
	}
	GEditorDomainOplog->CommitPackage(PackageName, Attachments);
}

void CookInitialize()
{
	bool bCookAttachmentsEnabled = true;
	GConfig->GetBool(TEXT("EditorDomain"), TEXT("CookAttachmentsEnabled"), bCookAttachmentsEnabled, GEditorIni);
	if (bCookAttachmentsEnabled)
	{
		GEditorDomainOplog = MakeUnique<FEditorDomainOplog>();
		if (!GEditorDomainOplog->IsValid())
		{
			UE_LOG(LogEditorDomain, Display, TEXT("Failed to connect to ZenServer; EditorDomain oplog is unavailable."));
			GEditorDomainOplog.Reset();
		}
	}
}


} // namespace UE::TargetDomain
