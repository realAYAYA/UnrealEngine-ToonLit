#pragma once

class MPROTOCOL_API FZDynamicPbTypeBuilder
{
public:
	void Init(const FString& BasePath);
	void ForeachTypes(const TFunction<bool(const FString& Name, UObject* Object)>& Callback);
};

