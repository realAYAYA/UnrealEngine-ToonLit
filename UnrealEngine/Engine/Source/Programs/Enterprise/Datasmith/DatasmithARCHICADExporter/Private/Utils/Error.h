// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AddonTools.h"

#include <stdexcept>
#include "GSException.hpp"

BEGIN_NAMESPACE_UE_AC

class UE_AC_Error : public std::exception
{
  public:
	typedef enum
	{
		kNotIn3DView,
		kUserCancelled
	} EErrorCode;

	UE_AC_Error(const utf8_t* InWhat, EErrorCode InErrorCode);

	~UE_AC_Error();

	const utf8_t* what() const throw() { return What; }

	EErrorCode GetErrorCode() const throw() { return ErrorCode; }

  private:
	const utf8_t* What;
	EErrorCode	  ErrorCode;
};

extern void ShowAlert(const UE_AC_Error& InException, const utf8_t* InFct);
extern void ShowAlert(const GS::GSException& inGSException, const utf8_t* InFct);
extern void ShowAlert(const utf8_t* InWhat, const utf8_t* InFct);

template < typename Functor > GSErrCode TryFunctionCatchAndAlert(const utf8_t* InFctName, Functor InFunctor)
{
	GSErrCode GSErr = APIERR_GENERAL;

	try
	{
		GSErr = InFunctor();
	}
	catch (UE_AC_Error& e)
	{
		ShowAlert(e, InFctName);
		if (e.GetErrorCode() == UE_AC_Error::kUserCancelled)
		{
			GSErr = APIERR_CANCEL;
		}
	}
	catch (std::exception& e)
	{
		ShowAlert(e.what(), InFctName);
	}
	catch (GS::GSException& gs)
	{
		ShowAlert(gs, InFctName);
	}
	catch (...)
	{
		ShowAlert("Unknown", InFctName);
	}

	return GSErr;
}

template < typename Functor > GSErrCode TryFunctionCatchAndLog(const utf8_t* InFctName, Functor InFunctor)
{
	GSErrCode GSErr = APIERR_GENERAL;

	try
	{
		GSErr = InFunctor();
	}
	catch (UE_AC_Error& e)
	{
		if (e.GetErrorCode() == UE_AC_Error::kUserCancelled)
		{
			UE_AC_DebugF("%s - User cancelled", InFctName);
			GSErr = APIERR_CANCEL;
		}
		else
		{
			UE_AC_DebugF("%s - AC Exception %s\n", InFctName, e.what());
		}
	}
	catch (std::exception& e)
	{
		UE_AC_DebugF("%s - Std Exception %s\n", InFctName, e.what());
	}
	catch (GS::GSException& gs)
	{
		if (gs.GetFileName() != nullptr)
		{
			UE_AC_DebugF("%s - GS Exception %s - %d : %s, %s, %d\n", InFctName, gs.GetName(), gs.GetID(),
						 gs.GetMessage().ToUtf8(), gs.GetFileName(), gs.GetLineNumber());
		}
		else
		{
			UE_AC_DebugF("%s - GS Exception %s - %d : %s\n", InFctName, gs.GetName(), gs.GetID(),
						 gs.GetMessage().ToUtf8());
		}
	}
	catch (...)
	{
		UE_AC_DebugF("%s - Unknown Exception\n", InFctName);
	}

	return GSErr;
}

END_NAMESPACE_UE_AC
