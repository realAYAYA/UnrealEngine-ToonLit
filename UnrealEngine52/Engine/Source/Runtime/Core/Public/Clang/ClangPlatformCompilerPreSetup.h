// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// HEADER_UNIT_UNSUPPORTED - Clang not supporting header units

#ifndef DISABLE_DEPRECATION
	#pragma clang diagnostic warning "-Wdeprecated-declarations"

	#define PRAGMA_DISABLE_DEPRECATION_WARNINGS \
		_Pragma("clang diagnostic push") \
		_Pragma("clang diagnostic ignored \"-Wdeprecated-declarations\"")

	#define PRAGMA_ENABLE_DEPRECATION_WARNINGS \
		_Pragma("clang diagnostic pop")
#endif // DISABLE_DEPRECATION

#ifndef PRAGMA_DISABLE_OVERLOADED_VIRTUAL_WARNINGS
	#define PRAGMA_DISABLE_OVERLOADED_VIRTUAL_WARNINGS \
		_Pragma("clang diagnostic push") \
		_Pragma("clang diagnostic ignored \"-Woverloaded-virtual\"")
#endif // PRAGMA_DISABLE_OVERLOADED_VIRTUAL_WARNINGS

#ifndef PRAGMA_ENABLE_OVERLOADED_VIRTUAL_WARNINGS
	#define PRAGMA_ENABLE_OVERLOADED_VIRTUAL_WARNINGS \
		_Pragma("clang diagnostic pop")
#endif // PRAGMA_ENABLE_OVERLOADED_VIRTUAL_WARNINGS

#ifndef PRAGMA_DISABLE_MISSING_BRACES_WARNINGS
	#define PRAGMA_DISABLE_MISSING_BRACES_WARNINGS \
		_Pragma("clang diagnostic push") \
		_Pragma("clang diagnostic ignored \"-Wmissing-braces\"")
#endif // PRAGMA_DISABLE_MISSING_BRACES_WARNINGS

#ifndef PRAGMA_ENABLE_MISSING_BRACES_WARNINGS
	#define PRAGMA_ENABLE_MISSING_BRACES_WARNINGS \
		_Pragma("clang diagnostic pop")
#endif // PRAGMA_ENABLE_MISSING_BRACES_WARNINGS

#ifndef PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS
	#define PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS \
		_Pragma("clang diagnostic push") \
		_Pragma("clang diagnostic ignored \"-Wshadow\"")
#endif // PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS

#ifndef PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS
	#define PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS \
		_Pragma("clang diagnostic pop")
#endif // PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS

#if __has_warning("-Wimplicit-float-conversion")
#define DISABLE_IMPLICIT_FLOAT_CONVERSION_FRAGMENT _Pragma("clang diagnostic ignored \"-Wimplicit-float-conversion\"")
#else
#define DISABLE_IMPLICIT_FLOAT_CONVERSION_FRAGMENT
#endif

#if __has_warning("-Wimplicit-float-conversion")
#define FORCE_IMPLICIT_FLOAT_CONVERSION_FRAGMENT _Pragma("clang diagnostic warning \"-Wimplicit-float-conversion\"")
#else
#define FORCE_IMPLICIT_FLOAT_CONVERSION_FRAGMENT
#endif

#if __has_warning("-Wimplicit-int-conversion")
#define DISABLE_IMPLICIT_INT_CONVERSION_FRAGMENT _Pragma("clang diagnostic ignored \"-Wimplicit-int-conversion\"")
#else
#define DISABLE_IMPLICIT_INT_CONVERSION_FRAGMENT
#endif

#if __has_warning("-Wimplicit-int-conversion")
#define FORCE_IMPLICIT_INT_CONVERSION_FRAGMENT _Pragma("clang diagnostic warning \"-Wimplicit-int-conversion\"")
#else
#define FORCE_IMPLICIT_INT_CONVERSION_FRAGMENT
#endif

#ifndef PRAGMA_DISABLE_UNSAFE_TYPECAST_WARNINGS
	#define PRAGMA_DISABLE_UNSAFE_TYPECAST_WARNINGS \
		_Pragma("clang diagnostic push") \
		_Pragma("clang diagnostic ignored \"-Wfloat-conversion\"") \
		DISABLE_IMPLICIT_FLOAT_CONVERSION_FRAGMENT \
		DISABLE_IMPLICIT_INT_CONVERSION_FRAGMENT \
		_Pragma("clang diagnostic ignored \"-Wc++11-narrowing\"")
#endif // PRAGMA_DISABLE_UNSAFE_TYPECAST_WARNINGS

#ifndef PRAGMA_ENABLE_UNSAFE_TYPECAST_WARNINGS
	#define PRAGMA_ENABLE_UNSAFE_TYPECAST_WARNINGS \
		UE_DEPRECATED_MACRO(5.0, "The PRAGMA_ENABLE_UNSAFE_TYPECAST_WARNINGS macro has been deprecated in favor of PRAGMA_RESTORE_UNSAFE_TYPECAST_WARNINGS. To force enable warnings use PRAGMA_FORCE_UNSAFE_TYPECAST_WARNINGS.")
#endif // PRAGMA_ENABLE_UNSAFE_TYPECAST_WARNINGS

#ifndef PRAGMA_FORCE_UNSAFE_TYPECAST_WARNINGS
	#define PRAGMA_FORCE_UNSAFE_TYPECAST_WARNINGS \
		_Pragma("clang diagnostic push") \
		_Pragma("clang diagnostic warning \"-Wfloat-conversion\"") \
		FORCE_IMPLICIT_FLOAT_CONVERSION_FRAGMENT \
		FORCE_IMPLICIT_INT_CONVERSION_FRAGMENT \
		_Pragma("clang diagnostic warning \"-Wc++11-narrowing\"")
#endif // PRAGMA_DISABLE_UNSAFE_TYPECAST_WARNINGS

#ifndef PRAGMA_RESTORE_UNSAFE_TYPECAST_WARNINGS
	#define PRAGMA_RESTORE_UNSAFE_TYPECAST_WARNINGS \
	_Pragma("clang diagnostic pop")
#endif // PRAGMA_RESTORE_UNSAFE_TYPECAST_WARNINGS

#if __has_warning("-Wordered-compare-function-pointers")
#define DISABLE_ORDERED_COMPARE_FUNCTION_POINTERS _Pragma("clang diagnostic ignored \"-Wordered-compare-function-pointers\"")
#else
#define DISABLE_ORDERED_COMPARE_FUNCTION_POINTERS
#endif

#ifndef PRAGMA_DISABLE_ORDERED_COMPARE_FUNCTION_POINTERS
	#define PRAGMA_DISABLE_ORDERED_COMPARE_FUNCTION_POINTERS \
		_Pragma("clang diagnostic push") \
		DISABLE_ORDERED_COMPARE_FUNCTION_POINTERS
#endif // PRAGMA_DISABLE_ORDERED_COMPARE_FUNCTION_POINTERS

#ifndef PRAGMA_ENABLE_ORDERED_COMPARE_FUNCTION_POINTERS
	#define PRAGMA_ENABLE_ORDERED_COMPARE_FUNCTION_POINTERS \
	_Pragma("clang diagnostic pop")
#endif // PRAGMA_ENABLE_ORDERED_COMPARE_FUNCTION_POINTERS

#ifndef PRAGMA_DISABLE_UNDEFINED_IDENTIFIER_WARNINGS
	#define PRAGMA_DISABLE_UNDEFINED_IDENTIFIER_WARNINGS \
		_Pragma("clang diagnostic push") \
		_Pragma("clang diagnostic ignored \"-Wundef\"")
#endif // PRAGMA_DISABLE_UNDEFINED_IDENTIFIER_WARNINGS

#ifndef PRAGMA_ENABLE_UNDEFINED_IDENTIFIER_WARNINGS
	#define PRAGMA_ENABLE_UNDEFINED_IDENTIFIER_WARNINGS \
		_Pragma("clang diagnostic pop")
#endif // PRAGMA_ENABLE_UNDEFINED_IDENTIFIER_WARNINGS

#ifndef PRAGMA_DISABLE_MISSING_VIRTUAL_DESTRUCTOR_WARNINGS
	#define PRAGMA_DISABLE_MISSING_VIRTUAL_DESTRUCTOR_WARNINGS \
		_Pragma("clang diagnostic push") \
		_Pragma("clang diagnostic ignored \"-Wdelete-non-virtual-dtor\"")
#endif // PRAGMA_DISABLE_MISSING_VIRTUAL_DESTRUCTOR_WARNINGS

#ifndef PRAGMA_ENABLE_MISSING_VIRTUAL_DESTRUCTOR_WARNINGS
	#define PRAGMA_ENABLE_MISSING_VIRTUAL_DESTRUCTOR_WARNINGS \
		_Pragma("clang diagnostic pop")
#endif // PRAGMA_ENABLE_MISSING_VIRTUAL_DESTRUCTOR_WARNINGS

#ifndef PRAGMA_DISABLE_REORDER_WARNINGS
	#define PRAGMA_DISABLE_REORDER_WARNINGS \
		_Pragma("clang diagnostic push") \
		_Pragma("clang diagnostic ignored \"-Wreorder\"")
#endif // PRAGMA_DISABLE_REORDER_WARNINGS

#ifndef PRAGMA_ENABLE_REORDER_WARNINGS
	#define PRAGMA_ENABLE_REORDER_WARNINGS \
		_Pragma("clang diagnostic pop")
#endif // PRAGMA_ENABLE_REORDER_WARNINGS

#ifndef PRAGMA_DISABLE_REGISTER_WARNINGS
	#define PRAGMA_DISABLE_REGISTER_WARNINGS \
		_Pragma("clang diagnostic push") \
		_Pragma("clang diagnostic ignored \"-Wregister\"")
#endif // PRAGMA_DISABLE_REGISTER_WARNINGS

#ifndef PRAGMA_ENABLE_REGISTER_WARNINGS
	#define PRAGMA_ENABLE_REGISTER_WARNINGS \
		_Pragma("clang diagnostic pop")
#endif // PRAGMA_ENABLE_REGISTER_WARNINGS

#ifndef PRAGMA_DISABLE_MACRO_REDEFINED_WARNINGS
	#define PRAGMA_DISABLE_MACRO_REDEFINED_WARNINGS \
		_Pragma("clang diagnostic push") \
		_Pragma("clang diagnostic ignored \"-Wmacro-redefined\"")
#endif // PRAGMA_DISABLE_MACRO_REDEFINED_WARNINGS

#ifndef PRAGMA_DISABLE_UNUSED_PRIVATE_FIELDS_WARNINGS
#define PRAGMA_DISABLE_UNUSED_PRIVATE_FIELDS_WARNINGS \
		_Pragma("clang diagnostic push") \
		_Pragma("clang diagnostic ignored \"-Wunused-private-field\"")
#endif // PRAGMA_DISABLE_UNUSED_PRIVATE_FIELDS_WARNINGS

#ifndef PRAGMA_ENABLE_UNUSED_PRIVATE_FIELDS_WARNINGS
#define PRAGMA_ENABLE_UNUSED_PRIVATE_FIELDS_WARNINGS \
		_Pragma("clang diagnostic pop")
#endif // PRAGMA_ENABLE_UNUSED_PRIVATE_FIELDS_WARNINGS

#ifndef PRAGMA_ENABLE_MACRO_REDEFINED_WARNINGS
	#define PRAGMA_ENABLE_MACRO_REDEFINED_WARNINGS \
		_Pragma("clang diagnostic pop")
#endif // PRAGMA_ENABLE_MACRO_REDEFINED_WARNINGS

#if __has_warning("-Wuninitialized-const-reference")
#define DISABLE_UNINITIALIZED_CONST_REFERENCE _Pragma("clang diagnostic ignored \"-Wuninitialized-const-reference\"")
#else
#define DISABLE_UNINITIALIZED_CONST_REFERENCE
#endif

#ifndef PRAGMA_DISABLE_UNINITIALIZED_CONST_REFERENCE_WARNINGS
	#define PRAGMA_DISABLE_UNINITIALIZED_CONST_REFERENCE_WARNINGS \
		_Pragma("clang diagnostic push") \
        DISABLE_UNINITIALIZED_CONST_REFERENCE
#endif // PRAGMA_DISABLE_UNINITIALIZED_CONST_REFERENCE_WARNINGS

#ifndef PRAGMA_ENABLE_UNINITIALIZED_CONST_REFERENCE_WARNINGS
	#define PRAGMA_ENABLE_UNINITIALIZED_CONST_REFERENCE_WARNINGS \
		_Pragma("clang diagnostic pop")
#endif // PRAGMA_ENABLE_UNINITIALIZED_CONST_REFERENCE_WARNINGS

#ifndef PRAGMA_POP
	#define PRAGMA_POP \
		_Pragma("clang diagnostic pop")
#endif // PRAGMA_POP

#ifndef PRAGMA_PUSH_PLATFORM_DEFAULT_PACKING
	#define PRAGMA_PUSH_PLATFORM_DEFAULT_PACKING
#endif // PRAGMA_PUSH_PLATFORM_DEFAULT_PACKING
	
#ifndef PRAGMA_POP_PLATFORM_DEFAULT_PACKING
	#define PRAGMA_POP_PLATFORM_DEFAULT_PACKING
#endif // PRAGMA_POP_PLATFORM_DEFAULT_PACKING

#ifndef EMIT_CUSTOM_WARNING_AT_LINE
	#define EMIT_CUSTOM_WARNING_AT_LINE(Line, Warning) \
		_Pragma(PREPROCESSOR_TO_STRING(message(Warning)))
#endif // EMIT_CUSTOM_WARNING_AT_LINE
