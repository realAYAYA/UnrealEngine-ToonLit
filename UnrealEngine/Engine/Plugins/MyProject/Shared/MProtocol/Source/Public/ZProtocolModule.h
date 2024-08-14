#pragma once

#include "CoreMinimal.h"

class MPROTOCOL_API FZProtocolModule : public IModuleInterface
{
public:

	static inline FZProtocolModule &Get()
	{
		return FModuleManager::LoadModuleChecked<FZProtocolModule>("ZProtocol");
	}
	
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	FString GetJsDir();
	
	void RebuildPbTypes();

	void ForeachTypes(const TFunction<bool(const FString& Name, UObject* Object)>& Callback);

private:
	struct ImplType;
	ImplType* ImplPtr = nullptr;
};
