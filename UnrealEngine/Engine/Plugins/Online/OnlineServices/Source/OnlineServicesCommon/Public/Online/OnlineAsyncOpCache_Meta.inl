// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/OnlineMeta.h"

namespace UE::Online::Meta {

BEGIN_ONLINE_STRUCT_META(FOperationConfig)
	ONLINE_STRUCT_FIELD(FOperationConfig, CacheExpiration),
	ONLINE_STRUCT_FIELD(FOperationConfig, CacheExpirySeconds),
	ONLINE_STRUCT_FIELD(FOperationConfig, bCacheError)
END_ONLINE_STRUCT_META()

/* UE::Online::Meta*/ }
