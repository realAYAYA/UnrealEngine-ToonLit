#pragma once

#include "CoreMinimal.h"

class MPROTOCOL_API FMProtocolModule : public IModuleInterface
{
public:

	static inline FMProtocolModule &Get()
	{
		return FModuleManager::LoadModuleChecked<FMProtocolModule>("MProtocol");
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
