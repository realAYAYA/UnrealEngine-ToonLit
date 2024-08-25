// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMAssetViewMenuContext.generated.h"

namespace UE::AnimNext::Editor
{
	class SRigVMAssetView;
}

UCLASS()
class URigVMAssetViewMenuContext : public UObject
{
	GENERATED_BODY()

	friend class UE::AnimNext::Editor::SRigVMAssetView;
	
	// The RigVM asset view that we are editing
	TWeakPtr<UE::AnimNext::Editor::SRigVMAssetView> RigVMAssetView;
};