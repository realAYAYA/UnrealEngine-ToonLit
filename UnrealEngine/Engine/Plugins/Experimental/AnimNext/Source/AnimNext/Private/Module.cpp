// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "AnimNextConfig.h"
#include "Animation/BlendProfile.h"
#include "Curves/CurveFloat.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Misc/CoreDelegates.h"
#include "DataRegistry.h"
#include "DecoratorBase/DecoratorRegistry.h"
#include "DecoratorBase/NodeTemplateRegistry.h"
#include "Graph/AnimNextGraph.h"
#include "RigVMCore/RigVMRegistry.h"
#include "RigVMRuntimeDataRegistry.h"
#include "Animation/AnimSequence.h"
#include "Scheduler/Scheduler.h"
#include "Param/ExternalParameterRegistry.h"
#include "Param/ObjectProxyFactory.h"

// Enable console commands only in development builds when logging is enabled
#define WITH_ANIMNEXT_CONSOLE_COMMANDS (!UE_BUILD_SHIPPING && !NO_LOGGING)

#if WITH_ANIMNEXT_CONSOLE_COMMANDS
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"

#include "DecoratorBase/DecoratorTemplate.h"
#include "DecoratorBase/NodeDescription.h"
#include "DecoratorBase/NodeTemplate.h"
#endif

namespace UE::AnimNext
{

class FModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		GetMutableDefault<UAnimNextConfig>()->LoadConfig();

		static TPair<UClass*, FRigVMRegistry::ERegisterObjectOperation> const AllowedObjectTypes[] =
		{
			{ UAnimSequence::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
			{ UScriptStruct::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
			{ UBlendProfile::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
			{ UCurveFloat::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
			{ UAnimNextGraph::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
		};

		FRigVMRegistry::Get().RegisterObjectTypes(AllowedObjectTypes);

		FObjectProxyFactory::Init();
		FExternalParameterRegistry::Init();
		FDataRegistry::Init();
		FDecoratorRegistry::Init();
		FNodeTemplateRegistry::Init();
		FScheduler::Init();
		FRigVMRuntimeDataRegistry::Init();

#if WITH_ANIMNEXT_CONSOLE_COMMANDS
		if (!IsRunningCommandlet())
		{
			ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
				TEXT("AnimNext.ListNodeTemplates"),
				TEXT("Dumps statistics about node templates to the log."),
				FConsoleCommandWithArgsDelegate::CreateRaw(this, &FModule::ListNodeTemplates),
				ECVF_Default
			));
			ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
				TEXT("AnimNext.ListAnimGraphs"),
				TEXT("Dumps statistics about animation graphs to the log."),
				FConsoleCommandWithArgsDelegate::CreateRaw(this, &FModule::ListAnimGraphs),
				ECVF_Default
			));
		}
#endif
	}

	virtual void ShutdownModule() override
	{
		FRigVMRuntimeDataRegistry::Destroy();
		FScheduler::Destroy();
		FNodeTemplateRegistry::Destroy();
		FDecoratorRegistry::Destroy();
		FDataRegistry::Destroy();
		FObjectProxyFactory::Destroy();
		FExternalParameterRegistry::Destroy();

#if WITH_ANIMNEXT_CONSOLE_COMMANDS
		for (IConsoleObject* Cmd : ConsoleCommands)
		{
			IConsoleManager::Get().UnregisterConsoleObject(Cmd);
		}
		ConsoleCommands.Empty();
#endif
	}

#if WITH_ANIMNEXT_CONSOLE_COMMANDS
	TArray<IConsoleObject*> ConsoleCommands;

	void ListNodeTemplates(const TArray<FString>& Args)
	{
		// Turn off log times to make diff-ing easier
		TGuardValue<ELogTimes::Type> DisableLogTimes(GPrintLogTimes, ELogTimes::None);

		// Make sure to log everything
		const ELogVerbosity::Type OldVerbosity = LogAnimation.GetVerbosity();
		LogAnimation.SetVerbosity(ELogVerbosity::All);

		const FNodeTemplateRegistry& NodeTemplateRegistry = FNodeTemplateRegistry::Get();
		const FDecoratorRegistry& DecoratorRegistry = FDecoratorRegistry::Get();

		UE_LOG(LogAnimation, Log, TEXT("===== AnimNext Node Templates ====="));
		UE_LOG(LogAnimation, Log, TEXT("Template Buffer Size: %u bytes"), NodeTemplateRegistry.TemplateBuffer.GetAllocatedSize());

		for (auto It = NodeTemplateRegistry.TemplateUIDToHandleMap.CreateConstIterator(); It; ++It)
		{
			const FNodeTemplateRegistryHandle Handle = It.Value();
			const FNodeTemplate* NodeTemplate = NodeTemplateRegistry.Find(Handle);

			const uint32 NumDecorators = NodeTemplate->GetNumDecorators();

			UE_LOG(LogAnimation, Log, TEXT("[%x] has %u decorators ..."), NodeTemplate->GetUID(), NumDecorators);
			UE_LOG(LogAnimation, Log, TEXT("    Template Size: %u bytes"), NodeTemplate->GetNodeTemplateSize());
			UE_LOG(LogAnimation, Log, TEXT("    Shared Data Size: %u bytes"), NodeTemplate->GetNodeSharedDataSize());
			UE_LOG(LogAnimation, Log, TEXT("    Instance Data Size: %u bytes"), NodeTemplate->GetNodeInstanceDataSize());
			UE_LOG(LogAnimation, Log, TEXT("    Decorators ..."));

			const FDecoratorTemplate* DecoratorTemplates = NodeTemplate->GetDecorators();
			for (uint32 DecoratorIndex = 0; DecoratorIndex < NumDecorators; ++DecoratorIndex)
			{
				const FDecoratorTemplate* DecoratorTemplate = DecoratorTemplates + DecoratorIndex;
				const FDecorator* Decorator = DecoratorRegistry.Find(DecoratorTemplate->GetRegistryHandle());
				const FString DecoratorName = Decorator != nullptr ? Decorator->GetDecoratorName() : TEXT("<Unknown>");

				const uint32 NextDecoratorIndex = DecoratorIndex + 1;
				const uint32 EndOfNextDecoratorSharedData = NextDecoratorIndex < NumDecorators ? DecoratorTemplates[NextDecoratorIndex].GetNodeSharedOffset() : NodeTemplate->GetNodeSharedDataSize();
				const uint32 DecoratorSharedDataSize = EndOfNextDecoratorSharedData - DecoratorTemplate->GetNodeSharedOffset();

				const uint32 EndOfNextDecoratorInstanceData = NextDecoratorIndex < NumDecorators ? DecoratorTemplates[NextDecoratorIndex].GetNodeInstanceOffset() : NodeTemplate->GetNodeInstanceDataSize();
				const uint32 DecoratorInstanceDataSize = EndOfNextDecoratorInstanceData - DecoratorTemplate->GetNodeInstanceOffset();

				UE_LOG(LogAnimation, Log, TEXT("            %u: [%x] %s (%s)"), DecoratorIndex, DecoratorTemplate->GetUID().GetUID(), *DecoratorName, DecoratorTemplate->GetMode() == EDecoratorMode::Base ? TEXT("Base") : TEXT("Additive"));
				UE_LOG(LogAnimation, Log, TEXT("                Shared Data: [Offset: %u bytes, Size: %u bytes]"), DecoratorTemplate->GetNodeSharedOffset(), DecoratorSharedDataSize);
				if (DecoratorTemplate->HasLatentProperties() && Decorator != nullptr)
				{
					UE_LOG(LogAnimation, Log, TEXT("                Shared Data Latent Property Handles: [Offset: %u bytes, Count: %u]"), DecoratorTemplate->GetNodeSharedLatentPropertyHandlesOffset(), Decorator->GetNumLatentDecoratorProperties());
				}
				UE_LOG(LogAnimation, Log, TEXT("                Instance Data: [Offset: %u bytes, Size: %u bytes]"), DecoratorTemplate->GetNodeInstanceOffset(), DecoratorInstanceDataSize);
			}
		}

		LogAnimation.SetVerbosity(OldVerbosity);
	}

	void ListAnimGraphs(const TArray<FString>& Args)
	{
		// Turn off log times to make diff-ing easier
		TGuardValue<ELogTimes::Type> DisableLogTimes(GPrintLogTimes, ELogTimes::None);

		// Make sure to log everything
		const ELogVerbosity::Type OldVerbosity = LogAnimation.GetVerbosity();
		LogAnimation.SetVerbosity(ELogVerbosity::All);

		TArray<const UAnimNextGraph*> AnimGraphs;

		for (TObjectIterator<UAnimNextGraph> It; It; ++It)
		{
			AnimGraphs.Add(*It);
		}

		struct FCompareObjectNames
		{
			FORCEINLINE bool operator()(const UAnimNextGraph& Lhs, const UAnimNextGraph& Rhs) const
			{
				return Lhs.GetPathName().Compare(Rhs.GetPathName()) < 0;
			}
		};
		AnimGraphs.Sort(FCompareObjectNames());

		const FNodeTemplateRegistry& NodeTemplateRegistry = FNodeTemplateRegistry::Get();
		const FDecoratorRegistry& DecoratorRegistry = FDecoratorRegistry::Get();
		const bool bDetailedOutput = true;

		UE_LOG(LogAnimation, Log, TEXT("===== AnimNext Animation Graphs ====="));
		UE_LOG(LogAnimation, Log, TEXT("Num Graphs: %u"), AnimGraphs.Num());

		for (const UAnimNextGraph* AnimGraph : AnimGraphs)
		{
			uint32 TotalInstanceSize = 0;
			uint32 NumNodes = 0;
			{
				// We always have a node at offset 0
				int32 NodeOffset = 0;

				while (NodeOffset < AnimGraph->SharedDataBuffer.Num())
				{
					const FNodeDescription* NodeDesc = reinterpret_cast<const FNodeDescription*>(&AnimGraph->SharedDataBuffer[NodeOffset]);

					TotalInstanceSize += NodeDesc->GetNodeInstanceDataSize();
					NumNodes++;

					const FNodeTemplate* NodeTemplate = NodeTemplateRegistry.Find(NodeDesc->GetTemplateHandle());
					NodeOffset += NodeTemplate->GetNodeSharedDataSize();
				}
			}

			UE_LOG(LogAnimation, Log, TEXT("    %s ..."), *AnimGraph->GetPathName());
			UE_LOG(LogAnimation, Log, TEXT("        Shared Data Size: %.2f KB"), double(AnimGraph->SharedDataBuffer.Num()) / 1024.0);
			UE_LOG(LogAnimation, Log, TEXT("        Max Instance Data Size: %.2f KB"), double(TotalInstanceSize) / 1024.0);
			UE_LOG(LogAnimation, Log, TEXT("        Num Nodes: %u"), NumNodes);

			if (bDetailedOutput)
			{
				// We always have a node at offset 0
				int32 NodeOffset = 0;

				while (NodeOffset < AnimGraph->SharedDataBuffer.Num())
				{
					const FNodeDescription* NodeDesc = reinterpret_cast<const FNodeDescription*>(&AnimGraph->SharedDataBuffer[NodeOffset]);
					const FNodeTemplate* NodeTemplate = NodeTemplateRegistry.Find(NodeDesc->GetTemplateHandle());

					const uint32 NumDecorators = NodeTemplate->GetNumDecorators();

					UE_LOG(LogAnimation, Log, TEXT("        Node %u: [Template %x with %u decorators]"), NodeDesc->GetUID().GetNodeIndex(), NodeTemplate->GetUID(), NumDecorators);
					UE_LOG(LogAnimation, Log, TEXT("            Shared Data: [Offset: %u bytes, Size: %u bytes]"), NodeOffset, NodeTemplate->GetNodeSharedDataSize());
					UE_LOG(LogAnimation, Log, TEXT("            Instance Data Size: %u bytes"), NodeDesc->GetNodeInstanceDataSize());
					UE_LOG(LogAnimation, Log, TEXT("            Decorators ..."));

					const FDecoratorTemplate* DecoratorTemplates = NodeTemplate->GetDecorators();
					for (uint32 DecoratorIndex = 0; DecoratorIndex < NumDecorators; ++DecoratorIndex)
					{
						const FDecoratorTemplate* DecoratorTemplate = DecoratorTemplates + DecoratorIndex;
						const FDecorator* Decorator = DecoratorRegistry.Find(DecoratorTemplate->GetRegistryHandle());
						const FString DecoratorName = Decorator != nullptr ? Decorator->GetDecoratorName() : TEXT("<Unknown>");

						const uint32 NextDecoratorIndex = DecoratorIndex + 1;
						const uint32 EndOfNextDecoratorSharedData = NextDecoratorIndex < NumDecorators ? DecoratorTemplates[NextDecoratorIndex].GetNodeSharedOffset() : NodeTemplate->GetNodeSharedDataSize();
						const uint32 DecoratorSharedDataSize = EndOfNextDecoratorSharedData - DecoratorTemplate->GetNodeSharedOffset();

						const uint32 EndOfNextDecoratorInstanceData = NextDecoratorIndex < NumDecorators ? DecoratorTemplates[NextDecoratorIndex].GetNodeInstanceOffset() : NodeTemplate->GetNodeInstanceDataSize();
						const uint32 DecoratorInstanceDataSize = EndOfNextDecoratorInstanceData - DecoratorTemplate->GetNodeInstanceOffset();

						UE_LOG(LogAnimation, Log, TEXT("                    %u: [%x] %s (%s)"), DecoratorIndex, DecoratorTemplate->GetUID().GetUID(), *DecoratorName, DecoratorTemplate->GetMode() == EDecoratorMode::Base ? TEXT("Base") : TEXT("Additive"));
						UE_LOG(LogAnimation, Log, TEXT("                        Shared Data: [Offset: %u bytes, Size: %u bytes]"), DecoratorTemplate->GetNodeSharedOffset(), DecoratorSharedDataSize);
						if (DecoratorTemplate->HasLatentProperties() && Decorator != nullptr)
						{
							UE_LOG(LogAnimation, Log, TEXT("                        Shared Data Latent Property Handles: [Offset: %u bytes, Count: %u]"), DecoratorTemplate->GetNodeSharedLatentPropertyHandlesOffset(), Decorator->GetNumLatentDecoratorProperties());
						}
						UE_LOG(LogAnimation, Log, TEXT("                        Instance Data: [Offset: %u bytes, Size: %u bytes]"), DecoratorTemplate->GetNodeInstanceOffset(), DecoratorInstanceDataSize);
					}

					NodeOffset += NodeTemplate->GetNodeSharedDataSize();
				}
			}
		}

		LogAnimation.SetVerbosity(OldVerbosity);
	}
#endif
};

}

IMPLEMENT_MODULE(UE::AnimNext::FModule, AnimNext)
