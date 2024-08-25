// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMCVars.h"

namespace Verse
{
TAutoConsoleVariable<bool> CVarTraceExecution(TEXT("verse.TraceExecution"), false, TEXT("When true, print a trace of Verse instructions executed to the log.\n"), ECVF_Default);
TAutoConsoleVariable<bool> CVarSingleStepTraceExecution(TEXT("verse.SingleStepTraceExecution"), false, TEXT("When true, require input from stdin before continuing to the next bytecode.\n"), ECVF_Default);
TAutoConsoleVariable<bool> CVarDumpBytecode(TEXT("verse.DumpBytecode"), false, TEXT("When true, dump bytecode of all functions.\n"), ECVF_Default);
TAutoConsoleVariable<float> CVarUObjectProbablity(TEXT("verse.UObjectProbablity"), 0.0f, TEXT("Probability (0.0..1.0) that we substitute VObjects with UObjects upon creation (for testing only).\n"), ECVF_Default);
FRandomStream RandomUObjectProbablity{42}; // Constant seed so sequence is deterministic
} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
