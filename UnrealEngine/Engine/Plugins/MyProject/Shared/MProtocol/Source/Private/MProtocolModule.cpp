// Copyright Epic Games, Inc. All Rights Reserved.

#include "MProtocolModule.h"
#include "ZDynamicPbTypeBuilder.h"

struct FMProtocolModule::ImplType
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

void FMProtocolModule::StartupModule()
{
	ImplPtr = new ImplType;
	RebuildPbTypes();	
}

void FMProtocolModule::ShutdownModule()
{
	if (ImplPtr)
	{
		delete ImplPtr;
		ImplPtr = nullptr;
	}
}

FString FMProtocolModule::GetJsDir()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("JavaScript") / TEXT("protocol"));	
}

void FMProtocolModule::RebuildPbTypes()
{
	if (ImplPtr)
	{
		ImplPtr->DynamicPbTypeBuilder->Init(GetJsDir());
	}
}

void FMProtocolModule::ForeachTypes(const TFunction<bool(const FString& Name, UObject* Object)>& Callback)
{
	if (ImplPtr)
		ImplPtr->DynamicPbTypeBuilder->ForeachTypes(Callback);
}

#define LOCTEXT_NAMESPACE "FMProtocolModule"

IMPLEMENT_MODULE(FMProtocolModule, MProtocol);

#undef LOCTEXT_NAMESPACE