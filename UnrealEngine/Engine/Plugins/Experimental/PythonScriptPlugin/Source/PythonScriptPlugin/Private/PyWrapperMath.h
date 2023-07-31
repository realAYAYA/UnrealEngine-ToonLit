// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PyWrapperStruct.h"
#include "PyUtil.h"

#if WITH_PYTHON

/** Initialize the PyWrapperMath types and add them to the given Python module */
void InitializePyWrapperMath(PyGenUtil::FNativePythonModule& ModuleInfo);

#endif	// WITH_PYTHON
