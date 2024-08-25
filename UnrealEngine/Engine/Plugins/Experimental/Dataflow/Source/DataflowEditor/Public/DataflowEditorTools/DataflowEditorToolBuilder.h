// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "UObject/Interface.h"

#include "DataflowEditorToolBuilder.generated.h"

namespace Dataflow
{
	enum class EDataflowPatternVertexType : uint8;
}


UINTERFACE(MinimalAPI)
class UDataflowEditorToolBuilder : public UInterface
{
	GENERATED_BODY()
};


class IDataflowEditorToolBuilder
{
	GENERATED_BODY()

public:

	/** Returns all Construction View modes that this tool can operate in. The first element should be the preferred mode to switch to if necessary. */
	virtual void GetSupportedViewModes(TArray<Dataflow::EDataflowPatternVertexType>& Modes) const = 0;

	/** Returns whether or not view can be set to wireframe when this tool is active.. */
	virtual bool CanSetConstructionViewWireframeActive() const { return true; }
};

