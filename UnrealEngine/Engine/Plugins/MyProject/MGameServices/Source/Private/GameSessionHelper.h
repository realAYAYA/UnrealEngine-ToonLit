#pragma once
#include "PbDispatcher.h"


class FMGameSession;

#ifndef M_MODULAR_NAME
#define M_MODULAR_NAME Default
#endif

#define M_GAME_MESSAGE_HANDLE_REGISTRANT_TYPENAME PREPROCESSOR_JOIN(_PB_MESSAGE_,PREPROCESSOR_JOIN(M_MODULAR_NAME,PREPROCESSOR_JOIN(_,__LINE__)))
#define M_GAME_MESSAGE_HANDLE_REGISTRANT_VAR_NAME PREPROCESSOR_JOIN(_PBVAR_,PREPROCESSOR_JOIN(M_MODULAR_NAME,PREPROCESSOR_JOIN(_,__LINE__)))

// ---------------------------------------------------------------------------------------

#define M_PB_MESSAGE_HANDLE(PbType, GameSessionVar, MessageVar) \
	PB_MESSAGE_HANDLE(FMGameSession, PbType, GameSessionVar, MessageVar)

// ---------------------------------------------------------------------------------------

struct FRpcServerHandleInitializers
{
	static FRpcServerHandleInitializers& Get();
	static void TearDown();

	void Register(const char* InRpcInterfaceName, const TFunction<void(FMGameSession*, void* InRpcInterface)>& Func);
	void Bind(FMGameSession* InGameSession, const FString& InRpcInterfaceName, void* InRpcInterfacePtr);

	template<typename T>
	void Bind(FMGameSession* InGameSession, T* Ptr)
	{
		Bind(InGameSession, Ptr->GetName(), (void*)Ptr);
	}
	
private:

	TMap<FString, TArray<TFunction<void(FMGameSession*, void*)>>> Handles;
};

#define M_PB_RPC_HANDLE(RpcInterfaceName, RpcMethodName, GameSessionVar, ReqVar, RspVar) \
	PB_RPC_HANDLE(FMGameSession, RpcInterfaceName, RpcMethodName, GameSessionVar, ReqVar, RspVar)