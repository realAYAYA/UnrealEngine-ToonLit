// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISourceControlOperation.h"

/** Adds some common functionality to source control operations. */
class FSourceControlOperationBase : public ISourceControlOperation
{
public:
	/** Retrieve any info or error messages that may have accumulated during the operation. */
	virtual const FSourceControlResultInfo& GetResultInfo() const override
	{
		return ResultInfo;
	}

	/** Add info/warning message. */
	virtual void AddInfoMessge(const FText& InInfo)
	{
		ResultInfo.InfoMessages.Add(InInfo);
	}

	/** Add error message. */
	virtual void AddErrorMessge(const FText& InError)
	{
		ResultInfo.ErrorMessages.Add(InError);
	}

	/** Add tag. */
	virtual void AddTag(const FString& InTag)
	{
		ResultInfo.Tags.Add(InTag);
	}

	/**
	 * Append any info or error messages that may have accumulated during the operation prior
	 * to returning a result, ensuring to keep any already accumulated info.
	 */
	virtual void AppendResultInfo(const FSourceControlResultInfo& InResultInfo) override
	{
		ResultInfo.Append(InResultInfo);
	}

	FSourceControlResultInfo ResultInfo;
};
