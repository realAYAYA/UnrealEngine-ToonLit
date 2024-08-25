// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiUserReplicationStyle.h"

#include "Brushes/SlateBoxBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

#include "Styling/SlateTypes.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/StarshipCoreStyle.h"
#include "Styling/SlateStyle.h"

namespace UE::MultiUserClient
{
	FString FMultiUserReplicationStyle::InContent(const FString& RelativePath, const ANSICHAR* Extension)
	{
		static FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("ConcertSharedSlate"))->GetContentDir();
		return (ContentDir / RelativePath) + Extension;
	}

	TSharedPtr< class FSlateStyleSet > FMultiUserReplicationStyle::StyleSet;

	FName FMultiUserReplicationStyle::GetStyleSetName()
	{
		return FName(TEXT("MultiUserReplicationStyle"));
	}

	void FMultiUserReplicationStyle::Initialize()
	{
		// Only register once
		if (StyleSet.IsValid())
		{
			return;
		}

		StyleSet = MakeShared<FSlateStyleSet>(GetStyleSetName());
		StyleSet->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
		StyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

		// Column widths
		StyleSet->Set("AllClients.Object.OwnerColumnWidth", 200.f);
		StyleSet->Set("AllClients.Object.ReplicationToggle", 45.f);
		StyleSet->Set("AllClients.Property.OwnerColumnWidth", 200.f);
		StyleSet->Set("SingleClient.Object.OwnerColumnWidth", 200.f);
		StyleSet->Set("SingleClient.Property.OwnerColumnWidth", 200.f);

		// Timing
		StyleSet->Set("AllClients.Reassignment.DisplayThrobberAfterSeconds", 0.2f);
		
		FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
	};

	void FMultiUserReplicationStyle::Shutdown()
	{
		if (StyleSet.IsValid())
		{
			FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
			ensure(StyleSet.IsUnique());
			StyleSet.Reset();
		}
	}

	TSharedPtr<class ISlateStyle> FMultiUserReplicationStyle::Get()
	{
		return StyleSet;
	}
}


