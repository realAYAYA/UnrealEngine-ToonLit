// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComputeFramework/ComputeKernelAssetActions.h"

#include "ComputeFramework/ComputeKernel.h"

FAssetTypeActions_ComputeKernel::FAssetTypeActions_ComputeKernel(EAssetTypeCategories::Type InAssetCategoryBit)
	: AssetCategoryBit(InAssetCategoryBit)
{
}

FText FAssetTypeActions_ComputeKernel::GetName() const
{
	return NSLOCTEXT("ComputeFramework", "ComputeKernelInstanceName", "Compute Kernel Instance");
}

FColor FAssetTypeActions_ComputeKernel::GetTypeColor() const
{
	return FColor::Turquoise;
}

UClass* FAssetTypeActions_ComputeKernel::GetSupportedClass() const
{
	return UComputeKernel::StaticClass();
}

uint32 FAssetTypeActions_ComputeKernel::GetCategories()
{
	return AssetCategoryBit;
}

const TArray<FText>& FAssetTypeActions_ComputeKernel::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		NSLOCTEXT("ComputeFramework", "AnimDeformersSubMenu", "Deformers")
	};
	return SubMenus;
}
