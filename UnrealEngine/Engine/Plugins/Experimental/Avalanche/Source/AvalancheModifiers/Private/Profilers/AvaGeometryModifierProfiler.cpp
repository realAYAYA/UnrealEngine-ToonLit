// Copyright Epic Games, Inc. All Rights Reserved.

#include "Profilers/AvaGeometryModifierProfiler.h"

#include "Components/DynamicMeshComponent.h"
#include "Modifiers/AvaGeometryBaseModifier.h"

void FAvaGeometryModifierProfiler::SetupProfilingStats()
{
	FActorModifierCoreProfiler::SetupProfilingStats();

	ProfilerStats.AddProperty(VertexInName, EPropertyBagPropertyType::Int32);
	ProfilerStats.AddProperty(VertexOutName, EPropertyBagPropertyType::Int32);
}

void FAvaGeometryModifierProfiler::BeginProfiling()
{
	FActorModifierCoreProfiler::BeginProfiling();

	const UAvaGeometryBaseModifier* GeometryModifier = GetModifier<UAvaGeometryBaseModifier>();
	if (GeometryModifier && GeometryModifier->IsMeshValid())
	{
		const int32 TriangleCount = GeometryModifier->GetMeshComponent()->GetDynamicMesh()->GetTriangleCount();
		ProfilerStats.SetValueInt32(VertexInName, TriangleCount);
	}
}

void FAvaGeometryModifierProfiler::EndProfiling()
{
	FActorModifierCoreProfiler::EndProfiling();

	const UAvaGeometryBaseModifier* GeometryModifier = GetModifier<UAvaGeometryBaseModifier>();
	if (GeometryModifier && GeometryModifier->IsMeshValid())
	{
		const int32 TriangleCount = GeometryModifier->GetMeshComponent()->GetDynamicMesh()->GetTriangleCount();
		ProfilerStats.SetValueInt32(VertexOutName, TriangleCount);
	}
}

TSet<FName> FAvaGeometryModifierProfiler::GetMainProfilingStats() const
{
	return TSet<FName>
	{
		ExecutionTimeName,
		VertexInName,
		VertexOutName
	};
}

int32 FAvaGeometryModifierProfiler::GetVertexIn() const
{
	TValueOrError<int32, EPropertyBagResult> ValueResult = ProfilerStats.GetValueInt32(VertexInName);
	return ValueResult.HasValue() ? ValueResult.GetValue() : 0;
}

int32 FAvaGeometryModifierProfiler::GetVertexOut() const
{
	TValueOrError<int32, EPropertyBagResult> ValueResult = ProfilerStats.GetValueInt32(VertexOutName);
	return ValueResult.HasValue() ? ValueResult.GetValue() : 0;
}
