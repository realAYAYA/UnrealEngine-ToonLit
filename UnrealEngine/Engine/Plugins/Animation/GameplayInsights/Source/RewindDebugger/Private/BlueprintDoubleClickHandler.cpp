// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlueprintDoubleClickHandler.h"

#include "IGameplayProvider.h"
#include "ObjectTrace.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"

#if OBJECT_TRACE_ENABLED

/** Open blueprint editor given an object id. If no editor found, it will use the properties editor. */
static bool OpenBlueprintAndAttachDebugger(const TraceServices::IAnalysisSession* Session, uint64 ObjectId)
{
	TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session);

	if (const IGameplayProvider* GameplayProvider = Session->ReadProvider<IGameplayProvider>("GameplayProvider"))
	{
		if (const FObjectInfo* ObjectInfo = GameplayProvider->FindObjectInfo(ObjectId))
		{
			if (const FClassInfo* ClassInfo = GameplayProvider->FindClassInfo(ObjectInfo->ClassId))
			{
				if (const UBlueprintGeneratedClass* InstanceClass = TSoftObjectPtr<UBlueprintGeneratedClass>(FSoftObjectPath(ClassInfo->PathName)).LoadSynchronous())
				{
					if (UBlueprint* Blueprint = Cast<UBlueprint>(InstanceClass->ClassGeneratedBy))
					{
						GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Blueprint);
						
						if (UObject* SelectedInstance = FObjectTrace::GetObjectFromId(ObjectId))
						{
							Blueprint->SetObjectBeingDebugged(SelectedInstance);
						}

						return true;
					}
				}
			}
		}
	}
	return false;
}

#endif

bool FBlueprintDoubleClickHandler::HandleDoubleClick(IRewindDebugger* RewindDebugger)
{
#if OBJECT_TRACE_ENABLED
	const TSharedPtr<FDebugObjectInfo> SelectedObject = RewindDebugger->GetSelectedComponent();
	if (SelectedObject.IsValid())
	{
		return OpenBlueprintAndAttachDebugger(RewindDebugger->GetAnalysisSession(), SelectedObject->ObjectId);
	}
#endif
	
	return false;
}

FName FBlueprintDoubleClickHandler::GetTargetTypeName() const
{
	static const FName ObjectTypeName = "Object";
	return ObjectTypeName;
}