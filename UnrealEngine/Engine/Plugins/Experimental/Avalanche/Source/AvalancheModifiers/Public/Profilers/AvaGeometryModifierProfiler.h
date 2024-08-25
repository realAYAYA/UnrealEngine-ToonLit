// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modifiers/Profiler/ActorModifierCoreProfiler.h"

/** Modifier profiler used by geometry modifiers */
class FAvaGeometryModifierProfiler : public FActorModifierCoreProfiler
{
public:
	static inline const FName VertexInName = TEXT("VertexIn");
	static inline const FName VertexOutName = TEXT("VertexOut");

	//~ Begin FActorModifierCoreProfiler
	virtual void SetupProfilingStats() override;
	virtual void BeginProfiling() override;
	virtual void EndProfiling() override;
	virtual TSet<FName> GetMainProfilingStats() const override;
	//~ End FActorModifierCoreProfiler

	AVALANCHEMODIFIERS_API int32 GetVertexIn() const;
	AVALANCHEMODIFIERS_API int32 GetVertexOut() const;
};
