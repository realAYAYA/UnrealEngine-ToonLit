#include "RedisOp.h"
#include "MGameServerPrivate.h"

#include "Misc/Fnv.h"

#define CHECK_REDIS_CLIENT \
if (!GGameServicesModule->RedisClient) \
{ \
	UE_LOG(LogMGameServices, Error, TEXT("%s 错误,还未初始化RedisClient"), ANSI_TO_TCHAR(__FUNCTION__)); \
	return false; \
}

FString MakeKey(const FString& InKey)
{
#if WITH_EDITOR
	static FString MachineId;
	if (MachineId.IsEmpty())
	{
		const FString Name = FPlatformProcess::ComputerName();
		const uint64 HashId = FFnv::MemFnv64(*Name, Name.Len() * sizeof(TCHAR));
		MachineId = BytesToHex(reinterpret_cast<const uint8*>(&HashId), sizeof(HashId));
	}
	return FString::Printf(TEXT("PROJECT_M:%s_%s"), *MachineId, *InKey);  // 编辑器模式将当前机器名编码到KEY里面
#else
	return FString::Printf(TEXT("PROJECT_M_%s"), *InKey);
#endif	
}

FString GenerateAccountKey(const FString& InAccount)
{
	return MakeKey(FString::Printf(TEXT("ACC1_%s"), *InAccount));
}

FString GeneratePlayerKey(const uint64 PlayerId)
{
	return MakeKey(FString::Printf(TEXT("Player_%llu"), PlayerId));
}

static constexpr TCHAR GKeyPlayerDataField[] = TEXT("PlayerData");
//static const FString OfflineDataKey = MakeKey(TEXT("OFFLINE_DATA"));
//const FString Field = FString::Printf(TEXT("OFFLINE_ROLE_DATA_%llu"), OfflineData.role_id);

bool FRedisOp::LoadPlayerData(const uint64 PlayerId, idlepb::PlayerSaveData* OutData)
{
	CHECK_REDIS_CLIENT
	
	TArray<char> OutValue;
	
	const FString Key = GeneratePlayerKey(PlayerId);
	if (!GGameServicesModule->RedisClient->HGetBin(Key, GKeyPlayerDataField, OutValue))
	{
		UE_LOG(LogMGameServices, Error, TEXT("%s 错误, Redis操作失败 Key = %s"), *FString(__FUNCTION__), *Key);
		return false;
	}
	
	if (!OutData->ParseFromArray(OutValue.GetData(), OutValue.Num()))
	{
		UE_LOG(LogMGameServices, Error, TEXT("[RedisOp] %s 错误,反序列化失败 Key=%s Size=%d"), *FString(__FUNCTION__), *Key, OutValue.Num());	
		return false;
	}
	
	return true;
}

bool FRedisOp::SavePlayerData(const uint64 PlayerId, const idlepb::PlayerSaveData& InData)
{
	CHECK_REDIS_CLIENT;

	const int32 Size = InData.ByteSizeLong();
	TArray<char> Buffer;
	Buffer.SetNumUninitialized(Size);
	
	if (!InData.SerializeToArray(Buffer.GetData(), Size))
	{
		UE_LOG(LogMGameServices, Error, TEXT("[RedisOp] %s 错误,序列化失败 RoldId=%llu"), *FString(__FUNCTION__), PlayerId);
		return false;
	}
	
	const FString Key = GeneratePlayerKey(PlayerId);
	if (!GGameServicesModule->RedisClient->HSetBin(Key, GKeyPlayerDataField, Buffer))
	{
		UE_LOG(LogMGameServices, Error, TEXT("%s 错误, Redis操作失败 Key = %s Size = %d"), *FString(__FUNCTION__), *Key, Buffer.Num());
		return false;
	}
	
	return true;
}

bool FRedisOp::GetAccountInfo(const FString& InAccount, uint64* OutPlayerId)
{
	CHECK_REDIS_CLIENT
	
	TSet<FString> Fields;
	Fields.Emplace(TEXT("PlayerId"));

	TMap<FString, FString> ResultMap;
	
	const FString Key = GenerateAccountKey(InAccount);
	if (!GGameServicesModule->RedisClient->HMGet(Key, Fields, ResultMap))
	{
		UE_LOG(LogMGameServices, Error, TEXT("%s 错误,Redis操作失败 Key=%s"), *FString(__FUNCTION__), *Key);
		return false;
	}

	const FString* PlayerIdStr = ResultMap.Find(TEXT("PlayerId"));
	if (PlayerIdStr && !PlayerIdStr->IsEmpty())
		LexFromString(*OutPlayerId, GetData(*PlayerIdStr));
	
	UE_LOG(LogMGameServices, Display, TEXT("%s Account = %s PlayerId = %s"), *FString(__FUNCTION__), *InAccount, *(*PlayerIdStr));
	
	return true;
}

bool FRedisOp::SetAccountInfo(const FString& InAccount, const uint64 InPlayerId)
{
	CHECK_REDIS_CLIENT;
	
	TMap<FString, FString> MemberMap;
	MemberMap.Emplace(TEXT("PlayerId"), LexToString(InPlayerId));

	const FString Key = GenerateAccountKey(InAccount);
	if (!GGameServicesModule->RedisClient->HMSet(Key, MemberMap))
	{
		UE_LOG(LogMGameServices, Error, TEXT("%s 错误,Redis操作失败 Key = %s"), *FString(__FUNCTION__), *Key);
		return false;
	}

	UE_LOG(LogMGameServices, Display, TEXT("%s Account = %s PlayerId = %llu"), *FString(__FUNCTION__), *InAccount, InPlayerId);
	return true;
}

bool FRedisOp::OccupyName(const FString& Name, const int64 InId)
{
	CHECK_REDIS_CLIENT;

	std::string AsciiName;
	FMyTools::ToString(Name, &AsciiName);
	const uint64 NameHash = FFnv::MemFnv64(AsciiName.c_str(), AsciiName.size());
	
	const FString Key = MakeKey(FString::Printf(TEXT("UQNAME_%llu"), NameHash));
	if (!GGameServicesModule->RedisClient->SetNxInt(Key, InId))
	{
		return false;
	}
	
	return true;	
}

bool FRedisOp::GetOccupyNameID(const FString& Name, int64& OutId)
{
	CHECK_REDIS_CLIENT;

	std::string AsciiName;
	FMyTools::ToString(Name, &AsciiName);
	const uint64 NameHash = FFnv::MemFnv64(AsciiName.c_str(), AsciiName.size());

	const FString Key = MakeKey(FString::Printf(TEXT("UQNAME_%llu"), NameHash));
	if (!GGameServicesModule->RedisClient->GetInt(Key, OutId))
	{
		return false;
	}

	return true;
}
