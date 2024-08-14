// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZProtocolModule.h"
#include "ZDynamicPbTypeBuilder.h"

struct FZProtocolModule::ImplType
{
	ImplType()
	{
		DynamicPbTypeBuilder.Reset(new FZDynamicPbTypeBuilder);
	}

	~ImplType()
	{
		DynamicPbTypeBuilder.Reset();		
	}
		
	TUniquePtr<FZDynamicPbTypeBuilder> DynamicPbTypeBuilder;

};

void FZProtocolModule::StartupModule()
{
	ImplPtr = new ImplType;
	RebuildPbTypes();	
}

void FZProtocolModule::ShutdownModule()
{
	if (ImplPtr)
	{
		delete ImplPtr;
		ImplPtr = nullptr;
	}
}

FString FZProtocolModule::GetJsDir()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("JavaScript") / TEXT("protocol"));	
}

void FZProtocolModule::RebuildPbTypes()
{
	if (ImplPtr)
	{
		ImplPtr->DynamicPbTypeBuilder->Init(GetJsDir());
	}
}

void FZProtocolModule::ForeachTypes(const TFunction<bool(const FString& Name, UObject* Object)>& Callback)
{
	if (ImplPtr)
		ImplPtr->DynamicPbTypeBuilder->ForeachTypes(Callback);
}

