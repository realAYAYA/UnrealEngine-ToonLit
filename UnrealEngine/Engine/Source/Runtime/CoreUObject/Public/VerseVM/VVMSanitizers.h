// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#if defined(__clang__) || defined(__GNUC__)
#define V_NO_SANITIZE_ADDRESS __attribute__((no_sanitize_address))
#else
#define V_NO_SANITIZE_ADDRESS
#endif

#endif // WITH_VERSE_VM
