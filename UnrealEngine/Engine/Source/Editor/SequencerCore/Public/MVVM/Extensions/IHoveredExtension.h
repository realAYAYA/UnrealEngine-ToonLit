// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "MVVM/ViewModelTypeID.h"

namespace UE
{
namespace Sequencer
{

class SEQUENCERCORE_API IHoveredExtension
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(IHoveredExtension)

	virtual ~IHoveredExtension() {}

	virtual bool IsHovered() const = 0;

	virtual void OnHovered() = 0;
	virtual void OnUnhovered() = 0;
};

class SEQUENCERCORE_API FHoveredExtensionShim : public IHoveredExtension
{
public:

	virtual bool IsHovered() const { return bIsHovered; }

	virtual void OnHovered() { bIsHovered = true; }
	virtual void OnUnhovered() { bIsHovered = false; }

protected:

	bool bIsHovered = false;
};

} // namespace Sequencer
} // namespace UE

