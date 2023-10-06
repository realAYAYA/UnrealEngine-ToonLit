// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

struct FNiagaraCompilationGraphBridge;
struct FNiagaraCompilationDigestBridge;

template<typename GraphBridge>
struct TNiagaraParameterMapHistory;

template<typename GraphBridge>
class TNiagaraParameterMapHistoryBuilder;

template<typename GraphBridge>
class TNiagaraParameterMapHistoryWithMetaDataBuilder;

using FNiagaraParameterMapHistory = TNiagaraParameterMapHistory<FNiagaraCompilationGraphBridge>;
using FNiagaraParameterMapHistoryBuilder = TNiagaraParameterMapHistoryBuilder<FNiagaraCompilationGraphBridge>;
using FNiagaraParameterMapHistoryWithMetaDataBuilder = TNiagaraParameterMapHistoryWithMetaDataBuilder<FNiagaraCompilationGraphBridge>;
