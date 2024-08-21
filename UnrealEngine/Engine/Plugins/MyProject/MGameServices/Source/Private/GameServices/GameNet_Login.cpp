#include "MGameSession.h"
#include "GameSessionHelper.h"
#include "MGameServerPrivate.h"
#include "MPlayer.h"
#include "MPlayerManager.h"
#include "MTools.h"
#include "RedisOp.h"

#ifdef M_MODULAR_NAME
#undef M_MODULAR_NAME 
#endif
#define M_MODULAR_NAME Login

M_PB_MESSAGE_HANDLE(idlepb::Ping, InSession, InMessage)
{
}

M_PB_MESSAGE_HANDLE(idlepb::Pong, InSession, InMessage)
{
	
}

M_PB_RPC_HANDLE(GameRpc, LoginGame, InSession, Req, Ack)
{
	Ack->set_ret(idlepb::LoginGameRetCode_Unknown);

	FString NewAccount;
	FMyTools::ToFString(Req->account(), &NewAccount);
	FString ClientVersion;
	FMyTools::ToFString(Req->client_version(), &ClientVersion);

	const uint64 ConnId = InSession->ConnId;
	FString& ConnAccount = InSession->Account;
	uint64& PlayerId = InSession->UserId;

	FMPlayer*& Player = InSession->Player;
	if (Player)
	{
		// Todo 重复登录处理
		Ack->set_ret(idlepb::LoginGameRetCode_DuplicateLogin);
		return;
	}

	// 检查账号
	{
		const int32 AccountLen = NewAccount.Len();  
		if (AccountLen < 3 || AccountLen > 20)
		{
			UE_LOG(LogMGameServices, Warning, TEXT("OnLoginGameReq 失败,账号长度非法 ConnId=%llu Account=%s"), ConnId, *NewAccount);
			Ack->set_ret(idlepb::LoginGameRetCode_AccountInvalid);
			return;
		}
		if (!FMyTools::IsPureAlphabetString(NewAccount))
		{
			UE_LOG(LogMGameServices, Warning, TEXT("OnLoginGameReq 失败,账号有非法字符 ConnId=%llu Account=%s"), ConnId, *NewAccount);
			Ack->set_ret(idlepb::LoginGameRetCode_AccountInvalid);
			return;
		}

		// Todo 密码错误
	}

	// Todo 版本检查
	{
		
	}

	ConnAccount = NewAccount;  // 更新 Session 上记录的帐号

	uint64 TempPlayerId = 0;
	if (!FRedisOp::GetAccountInfo(ConnAccount, &TempPlayerId))
	{
		UE_LOG(LogMGameServices, Warning, TEXT("OnLoginGameReq 失败,获取ID失败 ConnId=%llu Account=%s"), ConnId, *ConnAccount);
		return;
	}
	if (TempPlayerId == 0)
	{
		TempPlayerId = FFnv::MemFnv64(Req->account().c_str(), Req->account().size());
	}

	PlayerId = TempPlayerId;

	Player = FMPlayerManager::Get()->GetByPlayerId(PlayerId);
	if (Player)
	{
		// 服务器角色还未销毁
		if (Player->IsRecycle())
		{
			UE_LOG(LogMGameServices, Warning, TEXT("OnLoginGameReq 失败,错误的Role实例 ConnId=%llu Account=%s PlayerId=%llu"), ConnId, *ConnAccount, PlayerId);
			Ack->set_ret(idlepb::LoginGameRetCode_Unknown);
			Player = nullptr;
			return;
		}
		
		if (Player->GetSession())
		{
			UE_LOG(LogMGameServices, Warning, TEXT("OnLoginGameReq 失败,玩家已在线 ConnId=%llu Account=%s PlayerId=%llu"), ConnId, *ConnAccount, PlayerId);
			Ack->set_ret(idlepb::LoginGameRetCode_DuplicateLogin);
			Player = nullptr;
			return;
		}

		Player->Online(InSession);
		Ack->set_is_relogin(true);
	}
	else
	{
		Player = FMPlayerManager::Get()->CreatePlayer(PlayerId, ConnAccount);
		if (Player)
		{
			if (!Player->Load())
			{
				UE_LOG(LogMGameServices, Warning, TEXT("OnLoginGameReq 失败,读档错误 ConnId=%llu Account=%s PlayerId=%llu"), ConnId, *ConnAccount, PlayerId);
			}

			Player->Online(InSession);
		}
	}

	if (!Player)
	{
		UE_LOG(LogMGameServices, Warning, TEXT("OnLoginGameReq 失败, Player创建失败 ConnId=%llu Account=%s PlayerId=%llu"), ConnId, *ConnAccount, PlayerId);
		Ack->set_ret(idlepb::LoginGameRetCode_Unknown);
	}
	else
	{
		//Role->FillRoleData(Rsp->mutable_role_data());  // 玩家主数据
		Ack->set_ret(idlepb::LoginGameRetCode_Ok);
	}
}

M_PB_RPC_HANDLE(GameRpc, EnterLevel, InSession, Req, Ack)
{
	
}

/*// 登出账号
M_GAME_RPC_HANDLE(GameRpc, LogoutGame, InSession, Req, Ack)
{
	UMPlayer* Player = InSession->Player;
	if (!Player)
		return;

	Player->Offline(InSession);
	Ack.Ok = true;
}

// 创建角色
M_GAME_RPC_HANDLE(GameRpc, CreateRole, InSession, Req, Ack)
{
	UMPlayer* Player = InSession->Player;
	if (!Player)
		return;

	Ack.Ok = Player->CreateRole(Req.Params);
	if (Ack.Ok)
	{
		if (const FRoleData* RoleData = Player->GetRoleDataRef(Req.Params.RoleName))
		{
			RoleDataToPreview(*RoleData, Ack.PreviewRoleData);
		}
	}
}

// 请求进入世界
M_GAME_RPC_HANDLE(GameRpc, EnterWorld, InSession, Req, Ack)
{
	UMPlayer* Player = InSession->Player;
	if (!Player)
		return;
	
	// 角色不存在
	Player->SetCurrentRole(Req.RoleID);
	if (!Player->CurrentRole)
	{
		Ack.NetAddress = TEXT("Role");
		return;
	}

	if (UMWorldManager::Get()->MainWorld)
	{
		Ack.Success = true;
		Ack.NetAddress = TEXT("127.0.0.1:7777");
	}
	else
	{
		Ack.NetAddress = TEXT("World");
	}
}*/