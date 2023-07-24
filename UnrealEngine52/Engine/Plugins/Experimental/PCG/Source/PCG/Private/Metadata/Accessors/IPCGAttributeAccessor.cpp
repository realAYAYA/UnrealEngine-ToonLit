// Copyright Epic Games, Inc. All Rights Reserved.

#include "Metadata/Accessors/IPCGAttributeAccessor.h"

// Specialization of IPCGAttributeAccessor::Get and IPCGAttributeAccessor::Set, for all supported types.
#define IACCESSOR_DECL(T) \
template <> bool IPCGAttributeAccessor::GetRange<T>(TArrayView<T> OutValues, int32 Index, const IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags Flags) const { return GetRange##T(OutValues, Index, Keys, Flags); } \
template <> bool IPCGAttributeAccessor::SetRange<T>(TArrayView<const T> InValues, int32 Index, IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags Flags) { return SetRange##T(InValues, Index, Keys, Flags); }
PCG_FOREACH_SUPPORTEDTYPES(IACCESSOR_DECL);
#undef IACCESSOR_DECL