// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimData/AnimDataModel.h"
#include "Features/IModularFeatures.h"

namespace UE::Anim::DataModel
{
	UClass* IAnimationDataModels::FindClassForAnimationAsset(UAnimSequenceBase* AnimSequenceBase)	
	{
		TArray<IAnimationDataModels*> DataModels = IModularFeatures::Get().GetModularFeatureImplementations<IAnimationDataModels>(IAnimationDataModels::GetModularFeatureName());
		UClass* TargetClass = UAnimDataModel::StaticClass();

		for (const IAnimationDataModels* Model : DataModels)
		{
			if (Model)
			{
				if (UClass* SubClass = Model->GetModelClass(AnimSequenceBase))
				{
					TargetClass = SubClass;
				}
			}
		}

		return TargetClass;
	}	
}
