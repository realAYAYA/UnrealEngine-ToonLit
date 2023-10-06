// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"

class UNNEModelData;

namespace UE::NNEEditorTools::Private
{
	class FModelDataEditorToolkit : public FAssetEditorToolkit
	{
	public:
		void InitEditor(UNNEModelData* ModelData);

		virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
		virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
		virtual FName GetToolkitFName() const override { return FName("NNEModelDataEditor"); }
		virtual FText GetBaseToolkitName() const override { return INVTEXT("NNE Model Data Editor"); }
		virtual FString GetWorldCentricTabPrefix() const override { return TEXT("NNE Model Data Editor "); }
		virtual FLinearColor GetWorldCentricTabColorScale() const override { return {}; }

	private:
		TArrayView<const FString> GetTargetRuntimes() const;
		void SetTargetRuntimes(TArrayView<const FString> TargetRuntimes);

		TArray<FString> RegisteredRuntimes;
		UNNEModelData* ModelData;
	};

} // UE::NNEEditorTools::Private