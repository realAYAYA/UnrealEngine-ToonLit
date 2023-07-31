// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_Blackboard.h"

#include "AIModule.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTreeEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Templates/Casts.h"
#include "Toolkits/IToolkit.h"
#include "UObject/Object.h"

class UClass;

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_Blackboard::GetSupportedClass() const
{
	return UBlackboardData::StaticClass(); 
}

void FAssetTypeActions_Blackboard::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for(auto Object : InObjects)
	{
		auto BlackboardData = Cast<UBlackboardData>(Object);
		if(BlackboardData != nullptr)
		{
			FBehaviorTreeEditorModule& BehaviorTreeEditorModule = FModuleManager::LoadModuleChecked<FBehaviorTreeEditorModule>( "BehaviorTreeEditor" );
			BehaviorTreeEditorModule.CreateBehaviorTreeEditor( EToolkitMode::Standalone, EditWithinLevelEditor, BlackboardData );
		}
	}	
}

uint32 FAssetTypeActions_Blackboard::GetCategories()
{ 
	IAIModule& AIModule = FModuleManager::GetModuleChecked<IAIModule>("AIModule").Get();
	return AIModule.GetAIAssetCategoryBit();
}

#undef LOCTEXT_NAMESPACE
