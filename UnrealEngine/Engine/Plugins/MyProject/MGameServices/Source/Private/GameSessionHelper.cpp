#include "GameSessionHelper.h"
#include "Misc/LazySingleton.h"

// ---------------------------------------------------------------------------------------

FRpcServerHandleInitializers& FRpcServerHandleInitializers::Get()
{
	return TLazySingleton<FRpcServerHandleInitializers>::Get();
}

void FRpcServerHandleInitializers::TearDown()
{
}

void FRpcServerHandleInitializers::Register(const char* InRpcInterfaceName, const TFunction<void(FMGameSession*, void* InRpcInterface)>& Func)
{
	auto ContPtr = Handles.Find(InRpcInterfaceName);
	if (!ContPtr)
	{
		ContPtr = &Handles.Emplace(InRpcInterfaceName);
	}

	ContPtr->Emplace(Func);
}

void FRpcServerHandleInitializers::Bind(FMGameSession* InGameSession, const FString& InRpcInterfaceName, void* InRpcInterfacePtr)
{
	const auto ContPtr = Handles.Find(InRpcInterfaceName);
	if (!ContPtr)
		return;
	
	for (auto& Elem : *ContPtr)
	{
		Elem(InGameSession, InRpcInterfacePtr);
	}
}
