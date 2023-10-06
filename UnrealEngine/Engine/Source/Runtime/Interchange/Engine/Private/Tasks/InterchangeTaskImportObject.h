// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/TaskGraphInterfaces.h"
#include "CoreMinimal.h"
#include "InterchangeFactoryBase.h"
#include "InterchangeManager.h"
#include "Stats/Stats.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Nodes/InterchangeFactoryBaseNode.h"

namespace UE::Interchange
{
	/**
		* This task create UPackage and UObject, Cook::PackageTracker::NotifyUObjectCreated is not thread safe, so we need to create the packages on the main thread
		*/
	class FTaskImportObject_GameThread
	{
	private:
		FString PackageBasePath;
		int32 SourceIndex;
		TWeakPtr<FImportAsyncHelper, ESPMode::ThreadSafe> WeakAsyncHelper;
		UInterchangeFactoryBaseNode* FactoryNode;
		const UClass* FactoryClass;

	public:
		FTaskImportObject_GameThread(const FString& InPackageBasePath, const int32 InSourceIndex, TWeakPtr<FImportAsyncHelper, ESPMode::ThreadSafe> InAsyncHelper, UInterchangeFactoryBaseNode* InFactoryNode, const UClass* InFactoryClass)
			: PackageBasePath(InPackageBasePath)
			, SourceIndex(InSourceIndex)
			, WeakAsyncHelper(InAsyncHelper)
			, FactoryNode(InFactoryNode)
			, FactoryClass(InFactoryClass)
		{
			check(FactoryNode);
			check(FactoryClass);
		}

		ENamedThreads::Type GetDesiredThread() const
		{
			return ENamedThreads::GameThread;
		}

		static ESubsequentsMode::Type GetSubsequentsMode()
		{
			return ESubsequentsMode::TrackSubsequents;
		}

		TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FTaskImportObject_GameThread, STATGROUP_TaskGraphTasks);
		}

		void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);
	};

	class FTaskImportObject_Async
	{
	private:
		FString PackageBasePath;
		int32 SourceIndex;
		TWeakPtr<FImportAsyncHelper, ESPMode::ThreadSafe> WeakAsyncHelper;
		UInterchangeFactoryBaseNode* FactoryNode;

	public:
		FTaskImportObject_Async(const FString& InPackageBasePath, const int32 InSourceIndex, TWeakPtr<FImportAsyncHelper, ESPMode::ThreadSafe> InAsyncHelper, UInterchangeFactoryBaseNode* InFactoryNode)
			: PackageBasePath(InPackageBasePath)
			, SourceIndex(InSourceIndex)
			, WeakAsyncHelper(InAsyncHelper)
			, FactoryNode(InFactoryNode)
		{
			check(FactoryNode);
		}

		ENamedThreads::Type GetDesiredThread() const
		{
			return ENamedThreads::AnyBackgroundThreadNormalTask;
		}

		static ESubsequentsMode::Type GetSubsequentsMode()
		{
			return ESubsequentsMode::TrackSubsequents;
		}

		TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FTaskImportObject_Async, STATGROUP_TaskGraphTasks);
		}

		void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);
	};

	/**
		* This task create UPackage and UObject, Cook::PackageTracker::NotifyUObjectCreated is not thread safe, so we need to create the packages on the main thread
		*/
	class FTaskImportObjectFinalize_GameThread
	{
	private:
		FString PackageBasePath;
		int32 SourceIndex;
		TWeakPtr<FImportAsyncHelper, ESPMode::ThreadSafe> WeakAsyncHelper;
		UInterchangeFactoryBaseNode* FactoryNode;
		const UClass* FactoryClass;

	public:
		FTaskImportObjectFinalize_GameThread(const FString& InPackageBasePath, const int32 InSourceIndex, TWeakPtr<FImportAsyncHelper, ESPMode::ThreadSafe> InAsyncHelper, UInterchangeFactoryBaseNode* InFactoryNode)
			: PackageBasePath(InPackageBasePath)
			, SourceIndex(InSourceIndex)
			, WeakAsyncHelper(InAsyncHelper)
			, FactoryNode(InFactoryNode)
		{
			check(FactoryNode);
		}

		ENamedThreads::Type GetDesiredThread() const
		{
			return ENamedThreads::GameThread;
		}

		static ESubsequentsMode::Type GetSubsequentsMode()
		{
			return ESubsequentsMode::TrackSubsequents;
		}

		TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FTaskImportObjectFinalize_GameThread, STATGROUP_TaskGraphTasks);
		}

		void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);
	};

}//ns UE::Interchange
