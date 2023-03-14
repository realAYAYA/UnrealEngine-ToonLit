// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActorDeferredScriptManager.h"
#include "CoreGlobals.h"
#include "GameFramework/Actor.h"
#include "StaticMeshCompiler.h"

#if WITH_EDITOR

#define LOCTEXT_NAMESPACE "ActorDeferredScriptManager"


FActorDeferredScriptManager& FActorDeferredScriptManager::Get()
{
	static FActorDeferredScriptManager Singleton;
	return Singleton;
}

void FActorDeferredScriptManager::Shutdown()
{
}

FActorDeferredScriptManager::FActorDeferredScriptManager()
	: Notification(GetAssetNameFormat())
{
}

FName FActorDeferredScriptManager::GetStaticAssetTypeName()
{
	return TEXT("UE-ActorConstructionScripts");
}

void FActorDeferredScriptManager::AddActor(AActor* InActor)
{
	PendingConstructionScriptActors.PushFirst(InActor);
}

FName FActorDeferredScriptManager::GetAssetTypeName() const
{
	return GetStaticAssetTypeName();
}

FTextFormat FActorDeferredScriptManager::GetAssetNameFormat() const
{
	return LOCTEXT("ScriptNameFormat", "{0}|plural(one=Construction Script,other=Construction Scripts)");
}

TArrayView<FName> FActorDeferredScriptManager::GetDependentTypeNames() const
{
	static FName DependentTypeNames[] =
	{
		// Construction scripts needs to wait until static meshes are done prior to running
		FStaticMeshCompilingManager::GetStaticAssetTypeName()
	};
	return TArrayView<FName>(DependentTypeNames);
}

int32 FActorDeferredScriptManager::GetNumRemainingAssets() const
{
	return PendingConstructionScriptActors.Num();
}

void FActorDeferredScriptManager::ProcessAsyncTasks(bool bLimitExecutionTime)
{
	if (!PendingConstructionScriptActors.IsEmpty() && FStaticMeshCompilingManager::Get().GetNumRemainingMeshes() == 0)
	{
		double TickStartTime = FPlatformTime::Seconds();
		const double MaxSecondsPerFrame = 0.016;
		bool bHasTimeLeft = true;

		// since this deferred run of construction script was supposed to be done during level load
		// temporarily set the global flag to prevent dirtying the level package.
		// @note: it would quite preferable if we would have a scoped context than touching a global variable... 
		GIsEditorLoadingPackage = true;
		while (!PendingConstructionScriptActors.IsEmpty() && bHasTimeLeft)
		{
			TWeakObjectPtr<AActor> WeakActor = PendingConstructionScriptActors.Last();
			PendingConstructionScriptActors.PopLast();
			if (AActor* Actor = WeakActor.Get(); Actor && Actor->GetWorld())
			{
				// Temporarily do not consider actor as initialized if they were when running deferred construction scripts
				FGuardValue_Bitfield(Actor->bActorInitialized, false);
				Actor->RerunConstructionScripts();
			}
			bHasTimeLeft = bLimitExecutionTime ? ((FPlatformTime::Seconds() - TickStartTime) < MaxSecondsPerFrame) : true;
		}
		GIsEditorLoadingPackage = false;
	}

	UpdateCompilationNotification();
}

void FActorDeferredScriptManager::UpdateCompilationNotification()
{
	Notification.Update(GetNumRemainingAssets());
}


#undef LOCTEXT_NAMESPACE

#endif // #if WITH_EDITOR
