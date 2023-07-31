// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/*
Many third party headers require some care when importing. NNI third party includes should be wrapped like this:
	#include "ThirdPartyWarningDisabler.h"
	NNI_THIRD_PARTY_INCLUDES_START
	#undef check
	#undef TEXT
	// your ONNXRUNTIME include directives go here...
	NNI_THIRD_PARTY_INCLUDES_END
*/

#ifdef PLATFORM_NNI_MICROSOFT
#define NNI_THIRD_PARTY_INCLUDES_START THIRD_PARTY_INCLUDES_START \
	__pragma(warning(disable: 4100)) /* C4100: 'inline_element_size': unreferenced formal parameter*/ \
	__pragma(warning(disable: 4127)) /* C4127: conditional expression is constant*/ \
	__pragma(warning(disable: 4191)) /* C4191: 'reinterpret_cast': unsafe conversion from unsafe conversion from 'X' to 'Y' */ \
	__pragma(warning(disable: 4497)) /* C4497: nonstandard extension 'sealed' used: replace with 'final' */ \
	__pragma(warning(disable: 6001)) /* C6001: Using uninitialized memory '*pNode'. */ \
	__pragma(warning(disable: 6294)) /* C6294: Ill-defined for-loop:  initial condition does not satisfy test.  Loop body not executed. */ \
	__pragma(warning(disable: 6313)) /* C6313: Incorrect operator:  zero-valued flag cannot be tested with bitwise-and.  Use an equality test to check for zero-valued flags. */ \
	__pragma(warning(disable: 6387)) /* C6387: 'X' could be '0':  this does not adhere to the specification for the function 'memset'. */ \
	__pragma(warning(disable: 6388)) /* C6388: '*X' might not be '0':  this does not adhere to the specification for the function 'Y'. */ \
	__pragma(warning(disable: 28020)) /* C28020: The expression '0<=_Param_(1)&&_Param_(1)<=400-1' is not true at this call. */ \
	__pragma(warning(disable: 28196)) /* C28196: This warning indicates that the function prototype for the function being analyzed has a __notnull, __null or __drv_valueIs on an _Out_ parameter or the return value, but the value returned is inconsistent with that annotation. */ \
	UE_PUSH_MACRO("check") \
	UE_PUSH_MACRO("TEXT")
#else
// If support added for other platforms, this definition may require updating
#define NNI_THIRD_PARTY_INCLUDES_START THIRD_PARTY_INCLUDES_START UE_PUSH_MACRO("check") UE_PUSH_MACRO("TEXT")
#endif //PLATFORM_NNI_MICROSOFT

#define NNI_THIRD_PARTY_INCLUDES_END THIRD_PARTY_INCLUDES_END UE_POP_MACRO("check") UE_POP_MACRO("TEXT")
