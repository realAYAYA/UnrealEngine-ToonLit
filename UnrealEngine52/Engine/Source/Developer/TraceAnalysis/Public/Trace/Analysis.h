// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"

class FMessageLog;

namespace UE {
namespace Trace {

class IAnalyzer;
class IInDataStream;

/**
 * Represents the processing (e.g. analysis) of a trace stream. Instances are
 * created by constructing an FAnalysisContext object to marry an event trace
 * with how it should be analyzed. Note that the processing (and thus analysis)
 * happens on another thread.
 */
class TRACEANALYSIS_API FAnalysisProcessor
{
public:
	/** Checks if this object instance is valid and currently processing */
	bool IsActive() const;

	/** End processing a trace stream. */
	void Stop();

	/** Wait for the entire stream to have been processed and analysed. */
	void Wait();

	/** Pause or resume the processing.
	 * @param bState Pause if true, resume if false. */
	void Pause(bool bState);


	/**
	 * Gets the message log. Must be called after analysis has finished.
	 */
	FMessageLog* GetLog();

						~FAnalysisProcessor();
						FAnalysisProcessor() = default;
						FAnalysisProcessor(FAnalysisProcessor&& Rhs);
	FAnalysisProcessor&	operator = (FAnalysisProcessor&&);

private:
	friend				class FAnalysisContext;
						FAnalysisProcessor(FAnalysisProcessor&) = delete;
	FAnalysisProcessor&	operator = (const FAnalysisProcessor&) = delete;
	class				FImpl;
	FImpl*				Impl = nullptr;
};



/**
 * Used to describe how a log of trace events should be analyzed and being the
 * analysis on a particular trace stream.  
 */
class TRACEANALYSIS_API FAnalysisContext
{
public:
	/** Adds an analyzer instance that will subscribe to and receive event data
	 * from the trace stream. */
	void AddAnalyzer(IAnalyzer& Analyzer);

	/** Creates and starts analysis returning an FAnalysisProcessor instance which
	 * represents the analysis and affords some control over it.
	 * @param DataStream Input stream of trace log data to be analysed. */
	FAnalysisProcessor Process(IInDataStream& DataStream);

private:
	TArray<IAnalyzer*>	Analyzers;
};

} // namespace Trace
} // namespace UE
