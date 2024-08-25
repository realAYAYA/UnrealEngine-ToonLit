// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorSysConfigFeature.h"

enum class EEditorSysConfigIssueSeverity : uint8
{
	High,
	Medium,
	Low,
};

struct FEditorSysConfigIssue
{
	IEditorSysConfigFeature* Feature;
	EEditorSysConfigIssueSeverity Severity;
};
