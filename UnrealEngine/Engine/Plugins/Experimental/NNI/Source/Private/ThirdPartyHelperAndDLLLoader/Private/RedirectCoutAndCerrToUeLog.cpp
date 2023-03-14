// Copyright Epic Games, Inc. All Rights Reserved.

#include "RedirectCoutAndCerrToUeLog.h"
#include "ThirdPartyHelperAndDLLLoaderUtils.h"



/* FRedirectCoutAndCerrToUeLog public classes
 *****************************************************************************/

FRedirectCoutAndCerrToUeLog::FRedirectCoutAndCerrToUeLog()
	: StreamCout(true)
	, StreamCerr(false)
{
	// Save pointer to std::cout buffer
	BackupStreamReadBufCout = std::cout.rdbuf();
	BackupStreamReadBufCerr = std::cerr.rdbuf();
	// Substitute internal std::cout/std::cerr buffer with
	std::cout.rdbuf(&StreamCout); // now std::cout work with 'StreamCout' buffer
	std::cerr.rdbuf(&StreamCerr); // now std::cerr work with 'StreamCerr' buffer
}

FRedirectCoutAndCerrToUeLog::~FRedirectCoutAndCerrToUeLog()
{
	// Go back to old buffers
	std::cout.rdbuf(BackupStreamReadBufCout);
	std::cerr.rdbuf(BackupStreamReadBufCerr);
}

FRedirectCoutAndCerrToUeLog::LStream::LStream(const bool bInTrueIfCoutFalseIfCerr)
	: bTrueIfCoutFalseIfCerr(bInTrueIfCoutFalseIfCerr)
{
}

int FRedirectCoutAndCerrToUeLog::LStream::sync()
{
	FString StdMessage = FString(str().c_str());
	// Print message
	if (StdMessage.Len() > 0)
	{
		// Remove break lines and/or spaces at the end of the message
		if (StdMessage.Len() > 1)
		{
			if (StdMessage.Right(1) == TEXT("\n") || StdMessage.Right(1) == TEXT(" "))
			{
				// Find how many break lines and/or spaces at the end
				int64 BreakLineCounter = 1;
				while (StdMessage.Right(BreakLineCounter) == TEXT("\n") || StdMessage.Right(BreakLineCounter) == TEXT(" "))
				{
					++BreakLineCounter;
					// E.g., If message = \n\n\n --> We want to keep only 1 \n
					if (BreakLineCounter >= StdMessage.Len())
					{
						BreakLineCounter = StdMessage.Len() - 1;
						break;
					}
				}
				// Remove those final break lines and/or spaces
				if (StdMessage.Len() > BreakLineCounter)
				{
					StdMessage.LeftChopInline(BreakLineCounter - 1);
				}
			}
		}
		// Print cleaned message
		if (bTrueIfCoutFalseIfCerr)
		{
			UE_LOG(LogNeuralNetworkInferenceThirdPartyHelperAndDLLLoader, Display, TEXT("std::cout: %s"), *StdMessage);
		}
		else
		{
			UE_LOG(LogNeuralNetworkInferenceThirdPartyHelperAndDLLLoader, Warning, TEXT("std::cerr: %s"), *StdMessage);
		}
	}
	// Reset buffer (otherwise it'd keep printing the old messages together with the new ones)
	str("");
	return std::stringbuf::sync();
}
