// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Attribute.h"
#include "Widgets/SWidget.h"
#include "Components/Widget.h"
#include "Rendering/SlateRendererTypes.h"

#include "PostBufferUpdate.generated.h"

class SPostBufferUpdate;

/**
 * Widget that when drawn, will trigger the slate post buffer to update. Does not draw anything itself.
 * This allows for you to perform layered UI / sampling effects with the GetSlatePost material functions,
 * by placing this widget after UI you would like to be processed / sampled is drawn.
 *
 * * No Children
 */
UCLASS(MinimalAPI)
class UPostBufferUpdate : public UWidget
{
	GENERATED_BODY()

	UPostBufferUpdate();

private:

	/** 
	 * True if we should do the default post buffer update of the scene before any UI.
	 * If any PostBufferUpdate widget has this set as false, be default scene copy / processing will not occur.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetPerformDefaultPostBufferUpdate", Category = "Behavior", meta = (AllowPrivateAccess = "true"))
	bool bPerformDefaultPostBufferUpdate;

	/** Buffers that we should update, all of these buffers will be affected by 'bPerformDefaultPostBufferUpdate' if disabled */
	UPROPERTY(EditAnywhere, Category = "Behavior", meta = (AllowPrivateAccess = "true"))
	TArray<ESlatePostRT> BuffersToUpdate;

public:
	/** Set the orientation of the stack box. The existing elements will be rearranged. */
	UMG_API void SetPerformDefaultPostBufferUpdate(bool bInPerformDefaultPostBufferUpdate);

protected:
	//~ Begin UWidget Interface
	UMG_API virtual TSharedRef<SWidget> RebuildWidget() override;
	UMG_API virtual void SynchronizeProperties() override;
	UMG_API virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UWidget Interface

protected:
	TSharedPtr<SPostBufferUpdate> MyPostBufferUpdate;
};
