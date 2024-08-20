#include "GameSessionHelper.h"

#include "Misc/LazySingleton.h"

FRpcServerHandleInitializers& FRpcServerHandleInitializers::Get()
{
	return TLazySingleton<FRpcServerHandleInitializers>::Get();
}

void FRpcServerHandleInitializers::TearDown()
{
	
}

void FRpcServerHandleInitializers::Register(
	const char* InRpcInterfaceName,
	const TFunction<void(UMGameSession*, void* InRpcInterface)>& Func)
{
	TArray<TFunction<void(UMGameSession*, void*)>>* ContPtr = Handles.Find(InRpcInterfaceName);
	if (!ContPtr)
	{
		ContPtr = &Handles.Emplace(InRpcInterfaceName);
	}
    
	ContPtr->Emplace(Func);
}

void FRpcServerHandleInitializers::Bind(
	UMGameSession* InGameSession,
	const FString& InRpcInterfaceName,
	void* InRpcInterfacePtr)
{
	TArray<TFunction<void(UMGameSession*, void*)>>* ContPtr = Handles.Find(InRpcInterfaceName);
	if (!ContPtr)
		return;
	
	for (auto& Elem : *ContPtr)
	{
		Elem(InGameSession, InRpcInterfacePtr);
	}
}
