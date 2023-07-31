// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraEditorCommon.h"
#include "NiagaraParameters.h"
#include "NiagaraScript.h"

class Error;
class UEdGraphPin;
class UNiagaraNode;
class UNiagaraGraph;
class UEdGraphPin;
class FCompilerResultsLog;
class UNiagaraDataInterface;
struct FNiagaraTranslateResults;
struct FNiagaraTranslatorOutput;
struct FNiagaraVMExecutableData;
class FNiagaraCompileRequestData;
class FNiagaraCompileOptions;
//struct FNiagaraCompileEvent;

/** Defines information about the results of a Niagara script compile. */
struct FNiagaraCompileResults
{
	/** Whether or not the script compiled successfully for VectorVM */
	bool bVMSucceeded = false;
	/** Whether or not the script compiled successfully for GPU compute */
	bool bComputeSucceeded = false;
	
	/** The actual final compiled data.*/
	TSharedPtr<FNiagaraVMExecutableData> Data;
	float CompileTime = 0.0f;
	/** Tracking any compilation warnings or errors that occur.*/
	TArray<FNiagaraCompileEvent> CompileEvents;
	uint32 NumErrors = 0;
	uint32 NumWarnings = 0;
	FString DumpDebugInfoPath;
 
 
 
 
 
	static ENiagaraScriptCompileStatus CompileResultsToSummary(const FNiagaraCompileResults* CompileResults);
	void AppendCompileEvents(TArrayView<const FNiagaraCompileEvent> InCompileEvents)
	{
		CompileEvents.Reserve(CompileEvents.Num() + InCompileEvents.Num());
		for (const auto& CompileEvent : InCompileEvents)
		{
			CompileEvents.Add(CompileEvent);
			NumErrors += (CompileEvent.Severity == FNiagaraCompileEventSeverity::Error) ? 1 : 0;
			NumWarnings += (CompileEvent.Severity == FNiagaraCompileEventSeverity::Warning) ? 1 : 0;
		}
	}
};

//Interface for Niagara compilers.
// NOTE: the graph->hlsl translation step is now in FNiagaraHlslTranslator
//
class INiagaraCompiler
{
public:
	/** Starts the async compilation of a script and returns the job handle to retrieve the results */
	virtual int32 CompileScript(const FNiagaraCompileRequestData* InCompileRequest, const FNiagaraCompileOptions& InOptions, const FNiagaraTranslateResults& InTranslateResults, FNiagaraTranslatorOutput *TranslatorOutput, FString& TranslatedHLSL) = 0;

	/** Returns the compile result for a given job id once the job has finished compiling. */
	virtual TOptional<FNiagaraCompileResults> GetCompileResult(int32 JobID, bool bWait = false) = 0;

	/** Adds an error to be reported to the user. Any error will lead to compilation failure. */
	virtual void Error(FText ErrorText) = 0 ;

	/** Adds a warning to be reported to the user. Warnings will not cause a compilation failure. */
	virtual void Warning(FText WarningText) = 0;
};
