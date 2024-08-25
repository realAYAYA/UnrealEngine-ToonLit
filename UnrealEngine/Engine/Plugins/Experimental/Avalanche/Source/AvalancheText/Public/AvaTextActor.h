// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaText3DComponent.h"
#include "GameFramework/Actor.h"
#include "AvaTextActor.generated.h"

class UAvaTextCharacterTransform;
struct FAvaColorChangeData;

UCLASS(MinimalAPI, ClassGroup = (Text3D), DisplayName = "Motion Design Text", meta = (ComponentWrapperClass))
class AAvaTextActor : public AActor
{
	GENERATED_BODY()

public:
	AAvaTextActor();

	UText3DComponent* GetText3DComponent() const
	{
		return Text3DComponent;
	}

	AVALANCHETEXT_API FAvaColorChangeData GetColorData() const;

	AVALANCHETEXT_API void SetColorData(const FAvaColorChangeData& InColorData);

private:
	UPROPERTY(Category = "Text", VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UText3DComponent> Text3DComponent;

	UPROPERTY(Category = "Layout", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAvaTextCharacterTransform> Text3DCharacterTransform;
};
