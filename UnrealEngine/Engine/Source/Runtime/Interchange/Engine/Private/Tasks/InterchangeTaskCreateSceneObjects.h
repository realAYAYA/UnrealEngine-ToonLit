// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/TaskGraphInterfaces.h"
#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Nodes/InterchangeBaseNode.h"

class UInterchangeBaseNode;
class UInterchangeFactoryBaseNode;

namespace UE
{
	namespace Interchange
	{
		class FImportAsyncHelper;

		class FTaskCreateSceneObjects
		{
		private:
			FString PackageBasePath;
			int32 SourceIndex;
			TWeakPtr<FImportAsyncHelper> WeakAsyncHelper;
			TArray<UInterchangeFactoryBaseNode*> FactoryNodes;
			const UClass* FactoryClass;

		public:
			explicit FTaskCreateSceneObjects(const FString& InPackageBasePath, const int32 InSourceIndex, TWeakPtr<FImportAsyncHelper> InAsyncHelper, TArrayView<UInterchangeFactoryBaseNode*> InNodes, const UClass* InFactoryClass);

			ENamedThreads::Type GetDesiredThread()
			{
				// We are creating the factories in this task so it must execute on the GameThread.
				// Also, there are no "CreatePackage Task" equivalent for scene objects right now, so the factories must create those on the game thread.
				return ENamedThreads::GameThread;
			}

			static ESubsequentsMode::Type GetSubsequentsMode()
			{
				return ESubsequentsMode::TrackSubsequents;
			}

			TStatId GetStatId() const
			{
				RETURN_QUICK_DECLARE_CYCLE_STAT(FTaskCreateSceneObjects, STATGROUP_TaskGraphTasks);
			}

			void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);
		};


	} //ns Interchange
}//ns UE
