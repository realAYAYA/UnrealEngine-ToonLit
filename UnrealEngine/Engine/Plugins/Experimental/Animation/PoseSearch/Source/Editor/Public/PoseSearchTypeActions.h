// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

namespace UE::PoseSearch
{
	class FDatabaseTypeActions : public FAssetTypeActions_Base
	{
	public:

		FDatabaseTypeActions() {}
		virtual FText GetName() const override;
		virtual FColor GetTypeColor() const override;
		virtual UClass* GetSupportedClass() const override;
		virtual void OpenAssetEditor(
			const TArray<UObject*>& InObjects,
			TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
		virtual uint32 GetCategories() override;
		virtual const TArray<FText>& GetSubMenus() const override;
	};

	class FSchemaTypeActions : public FAssetTypeActions_Base
	{
	public:

		FSchemaTypeActions() {}
		virtual FText GetName() const override;
		virtual FColor GetTypeColor() const override;
		virtual UClass* GetSupportedClass() const override;
		virtual uint32 GetCategories() override;
		virtual const TArray<FText>& GetSubMenus() const override;
	};
}
