// Copyright Epic Games, Inc. All Rights Reserved.

#include "FX/SlateFXSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Engine/GameInstance.h"
#include "FX/SlateRHIPostBufferProcessor.h"
#include "SlateRHIRendererSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SlateFXSubsystem)

USlateRHIPostBufferProcessor* USlateFXSubsystem::GetPostProcessor(ESlatePostRT InSlatePostBufferBit)
{
	USlateRHIPostBufferProcessor* Result = nullptr;

	if (GEngine)
	{
		if (USlateFXSubsystem* SlateFXSubsystem = GEngine->GetEngineSubsystem<USlateFXSubsystem>())
		{
			Result = SlateFXSubsystem->GetSlatePostProcessor(InSlatePostBufferBit);
		}
	}

	return Result;
}

TSharedPtr<FSlateRHIPostBufferProcessorProxy> USlateFXSubsystem::GetPostProcessorProxy(ESlatePostRT InSlatePostBufferBit)
{
	TSharedPtr<FSlateRHIPostBufferProcessorProxy> Result = nullptr;

	if (GEngine)
	{
		if (USlateFXSubsystem* SlateFXSubsystem = GEngine->GetEngineSubsystem<USlateFXSubsystem>())
		{
			Result = SlateFXSubsystem->GetSlatePostProcessorProxy(InSlatePostBufferBit);
		}
	}

	return Result;
}

void USlateFXSubsystem::BeginDestroy()
{
	// Flush rendering commands since this subsystem can be used in render thread
	FlushRenderingCommands();

	Super::BeginDestroy();
}

bool USlateFXSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	TArray<UClass*> ChildClasses;
	GetDerivedClasses(GetClass(), ChildClasses, false);

	// Only create an instance if there is no override implementation defined elsewhere
	return ChildClasses.Num() == 0;
}

void USlateFXSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FWorldDelegates::OnPreWorldInitialization.AddUObject(this, &USlateFXSubsystem::OnPreWorldInitialization);
	FWorldDelegates::OnPostWorldCleanup.AddUObject(this, &USlateFXSubsystem::OnPostWorldCleanup);
}

void USlateFXSubsystem::Deinitialize()
{
	SlatePostBufferProcessors.Empty();

	FWorldDelegates::OnPreWorldInitialization.RemoveAll(this);
	FWorldDelegates::OnPostWorldCleanup.RemoveAll(this);

	Super::Deinitialize();
}

TSharedPtr<FSlateRHIPostBufferProcessorProxy> USlateFXSubsystem::GetSlatePostProcessorProxy(ESlatePostRT InPostBufferBit)
{
	if (TSharedPtr<FSlateRHIPostBufferProcessorProxy>* ProcessorProxy = SlatePostBufferProcessorProxies.Find(InPostBufferBit))
	{
		return *ProcessorProxy;
	}

	return nullptr;
}

USlateRHIPostBufferProcessor* USlateFXSubsystem::GetSlatePostProcessor(ESlatePostRT InPostBufferBit)
{
	if (TObjectPtr<USlateRHIPostBufferProcessor>* Processor = SlatePostBufferProcessors.Find(InPostBufferBit))
	{
		return *Processor;
	}

	return nullptr;
}

void USlateFXSubsystem::OnPreWorldInitialization(UWorld* World, const UWorld::InitializationValues IVS)
{
	if (World && World->WorldType == EWorldType::EditorPreview)
	{
		return;
	}

	SlatePostBufferProcessors.Empty();
	SlatePostBufferProcessorProxies.Empty();

	if (const USlateRHIRendererSettings* SlateRendererSettings = USlateRHIRendererSettings::Get())
	{
		for (ESlatePostRT SlatePostBufferBit : TEnumRange<ESlatePostRT>())
		{
			const FSlatePostSettings& PostSetting = SlateRendererSettings->GetSlatePostSetting(SlatePostBufferBit);
			if (PostSetting.bEnabled && PostSetting.PostProcessorClass)
			{
				if (TObjectPtr<USlateRHIPostBufferProcessor> BufferProcessor = NewObject<USlateRHIPostBufferProcessor>(this, PostSetting.PostProcessorClass))
				{
					SlatePostBufferProcessors.Add(SlatePostBufferBit, BufferProcessor);
					SlatePostBufferProcessorProxies.Add(SlatePostBufferBit, BufferProcessor->GetRenderThreadProxy());
				}
			}
		}
	}
}

void USlateFXSubsystem::OnPostWorldCleanup(UWorld* World, bool SessionEnded, bool bCleanupResources)
{
	if (World && World->WorldType == EWorldType::EditorPreview)
	{
		return;
	}

	SlatePostBufferProcessors.Empty();
	SlatePostBufferProcessorProxies.Empty();
}
