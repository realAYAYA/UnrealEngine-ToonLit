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

M_PB_RPC_HANDLE(GameRpc, LoginGame, InSession, Req, Ack)
{
	/*{
		// Todo 检查版本
		//Req.ClientVersion;
	}
	
	UMPlayer* Player = InSession->Player;

	Ack.Ret = ELoginGameRetCode::UnKnow;
	
	// 重复请求登录，如果是新Session，下文还会进行顶号判定
	if (Player)
	{
		return;
	}

	const int32 AccountLen = Req.Account.Len();  
	if (AccountLen < 1 || AccountLen > 20)
	{
		UE_LOG(LogMGameServer, Warning, TEXT("LoginGame失败, 用户名 %s 长度非法"), *Req.Account);
		return;
	}

	uint64 PlayerID = 0;
	if (!IsPureAlphabetString(Req.Account))
	{
		UE_LOG(LogMGameServer, Warning, TEXT("LoginGame失败, 用户名 %s 含有非法字符"), *Req.Account);
		return;
	}

	if (!FRedisOp::GetAccountInfo(Req.Account, &PlayerID))
	{
		UE_LOG(LogMGameServer, Warning, TEXT("LoginGame失败, 获取ID失败 Account = %s"), *Req.Account)
		return;
	}

	// 无此玩家，进入建新号流程
	if (PlayerID == 0)
	{
		PlayerID = GenerateUID(Req.Account);
		if (!FRedisOp::SetAccountInfo(Req.Account, PlayerID))
		{
			UE_LOG(LogMGameServer, Display, TEXT("OnCreateCharacter 错误,报错ID失败 Account=%s UserId=%llu"), *Req.Account, PlayerID);
			return;
		}
	}

	Player = UMPlayerManager::Get()->GetByPlayerID(PlayerID);
	if (Player)
	{
		if (Player->GetSession())
		{
			UE_LOG(LogMGameServer, Warning, TEXT("LoginGame失败, ConnID = %s Account = %s PlayerID = %llu 角色已经在线"), *InSession->ID.ToString(), *Req.Account, PlayerID);

			Ack.Ret = ELoginGameRetCode::DuplicateLogin;// Todo 当前不允许顶号
			Player = nullptr;
			return;
		}
	}
	else
	{
		Player = UMPlayerManager::Get()->CreatePlayer(PlayerID, Req.Account);
		if (Player)
		{
			if (!Player->Load())
			{
				UMPlayerManager::Get()->DeletePlayer(Player);
				Player = nullptr;
				UE_LOG(LogMGameServer, Warning, TEXT("LoginGame失败, ConnID = %s Account = %s PlayerID = %llu 读档错误"), *InSession->ID.ToString(), *Req.Account, PlayerID);
			}
		}
	}

	if (Player)
	{
		Player->Online(InSession);
		
		Ack.Ret = ELoginGameRetCode::Ok;
		Ack.PlayerID = Player->GetPlayerID();
		Player->Fill(Ack.RolePreviewData);// 准备预览数据
	}*/
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