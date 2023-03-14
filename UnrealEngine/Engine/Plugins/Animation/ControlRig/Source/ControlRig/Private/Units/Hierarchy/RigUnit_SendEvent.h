// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "RigUnit_SendEvent.generated.h"

/**
 * SendEvent is used to notify the engine / editor of a change that happend within the Control Rig.
 */
USTRUCT(meta=(DisplayName="Send Event", Category="Hierarchy", DocumentationPolicy="Strict", Keywords = "SendEvent", TemplateName="Event,Notify,Notification", NodeColor="1, 0, 0"))
struct CONTROLRIG_API FRigUnit_SendEvent : public FRigUnitMutable
{
	GENERATED_BODY()

	FRigUnit_SendEvent()
		: Event(ERigEvent::RequestAutoKey)
		, Item()
		, OffsetInSeconds(0.f)
		, bEnable(true)
		, bOnlyDuringInteraction(true)
	{}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/**
	 * The event to send to the engine
	 */
	UPROPERTY(meta = (Input))
	ERigEvent Event;

	/**
	 * The item to send the event for
	 */
	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Item;

	/**
	 * The time offset to use for the send event
	 */
	UPROPERTY(meta = (Input, UIMin = "-1000", UIMax = "1000"))
	float OffsetInSeconds;

	/**
	 * The event will be sent if this is checked
	 */
	UPROPERTY(meta = (Input))
	bool bEnable;

	/**
	 * The event will be sent if this only during an interaction
	 */
	UPROPERTY(meta = (Input))
	bool bOnlyDuringInteraction;
};
