// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaSequenceDirectorShared.h"
#include "Containers/Array.h"
#include "Containers/ContainersFwd.h"
#include "Engine/Blueprint.h"
#include "AvaSequenceDirectorBlueprint.generated.h"

UCLASS(MinimalAPI)
class UAvaSequenceDirectorBlueprint : public UBlueprint
{
	GENERATED_BODY()

public:
	AVALANCHESEQUENCE_API TConstArrayView<FAvaSequenceInfo> GetSequenceInfos();

#if WITH_EDITOR
	bool OnOuterWorldRenamed(const TCHAR* InName, UObject* InNewOuter, ERenameFlags InRenameFlags);

protected:
	//~ Begin UBlueprint
	virtual UClass* GetBlueprintClass() const override;
	virtual void GetReparentingRules(TSet<const UClass*>& OutAllowedChildrenOfClasses, TSet<const UClass*>& OutDisallowedChildrenOfClasses) const override;
#endif
	//~ End UBlueprint

private:
	void UpdateSequenceInfos();

	UPROPERTY()
	TArray<FAvaSequenceInfo> SequenceInfos;
};
