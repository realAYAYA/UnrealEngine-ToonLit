#pragma once
#include "MyNetFwd.h"
#include "PbCommon.h"

class FMGameSession;

class FMPlayer
{
	
public:

	FMPlayer();
	virtual ~FMPlayer();

	bool Init(const uint64 InPlayerId, const FString& InAccount);
	void Cleanup();

	void Online(FMGameSession* InSession);
	void Offline(const FMGameSession* InSession);

	FMGameSession* GetSession() const;
	uint64 GetConnId() const;

	uint64 GetPlayerID() const { return PlayerId; }

	void SendToMe(const TSharedPtr<FPbMessage>& InMessage) const;

	// ========================================================
	
	/** 是否在线 */
	bool IsOnline() const;
	
	/** 读档 */
	bool Load();
	
	/** 存档 */
	void Save();

	void MarkNeedSave(bool bImmediately = false);

	bool IsRecycle() const;
	
private:
	
	FMGameSession* Session = nullptr;
	
	uint64 PlayerId = 0;
	FString Account;
	
	FPbPlayerData PlayerData;
};
