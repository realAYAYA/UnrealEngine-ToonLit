// Copyright Epic Games, Inc. All Rights Reserved.

#include "Processor.h"
#include "Trace/Analysis.h"

namespace UE {
namespace Trace {

////////////////////////////////////////////////////////////////////////////////
void FAnalysisContext::AddAnalyzer(IAnalyzer& Analyzer)
{
	Analyzers.Add(&Analyzer);
}

////////////////////////////////////////////////////////////////////////////////
FAnalysisProcessor FAnalysisContext::Process(IInDataStream& DataStream)
{
	FAnalysisProcessor Processor;
	if (Analyzers.Num() > 0)
	{
		Processor.Impl = new FAnalysisProcessor::FImpl(DataStream, MoveTemp(Analyzers));
	}
	return MoveTemp(Processor);
}

} // namespace Trace
} // namespace UE
