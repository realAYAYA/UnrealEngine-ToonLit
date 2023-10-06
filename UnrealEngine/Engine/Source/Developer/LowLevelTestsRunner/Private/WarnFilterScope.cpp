// Copyright Epic Games, Inc. All Rights Reserved.

#include "LowLevelTestsRunner/WarnFilterScope.h"
#include "Misc/FeedbackContext.h"

namespace UE::Testing
{
	struct FFilterFeedback : public FFeedbackContext
	{

		FFilterFeedback(FFeedbackContext* OldFeedback, TFunction<bool(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category)> Handler)
			: OldFeedback(OldFeedback)
			, Handler(Handler)
		{

		}

		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			if (!Handler(V, Verbosity, Category))
			{
				OldFeedback->Serialize(V, Verbosity, Category);
			}
		}
		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category, double Time) override
		{
			if (!Handler(V, Verbosity, Category))
			{
				OldFeedback->Serialize(V, Verbosity, Category, Time);
			}
		}

		FFeedbackContext* OldFeedback;
		TFunction<bool(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category)> Handler;
	};

	FWarnFilterScope::FWarnFilterScope(TFunction<bool(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category)> LogHandler)
		: OldWarn(GWarn)
		, Feedback(new FFilterFeedback(OldWarn, LogHandler))
	{
		GWarn = Feedback;
	}

	FWarnFilterScope::~FWarnFilterScope()
	{
		GWarn = OldWarn;
		delete Feedback;
	}
}