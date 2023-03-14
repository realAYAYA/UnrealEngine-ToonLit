// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IPC/Containers/TextureShareCoreInterprocessEnums.h"
#include "IPC/Containers/TextureShareCoreInterprocessObject.h"

#include "Containers/TextureShareCoreContainers.h"

/**
 * Enums to text wrappers for logging
 */
namespace TextureShareCoreHelpers
{
	// Debug log purpose
	static constexpr auto GetTEXT(const ETextureShareDeviceType In)
	{
		switch (In)
		{
		case ETextureShareDeviceType::D3D11:  return TEXT("D3D11");
		case ETextureShareDeviceType::D3D12:  return TEXT("D3D12");
		case ETextureShareDeviceType::Vulkan: return TEXT("Vulkan");
		default:
			break;
		}

		return TEXT("Undefined");
	}

	static constexpr auto GetTEXT(const ETextureShareFrameSyncTemplate In)
	{
		switch (In)
		{
		case ETextureShareFrameSyncTemplate::Default: return TEXT("Default");
		case ETextureShareFrameSyncTemplate::SDK:     return TEXT("SDK");
		case ETextureShareFrameSyncTemplate::DisplayCluster: return TEXT("DisplayCluster");
		default:
			break;
		}

		return TEXT("?");
	}

	static constexpr auto GetTEXT(const ETextureShareThreadMutex In)
	{
		switch (In)
		{
		case ETextureShareThreadMutex::GameThread:      return TEXT("GameThread");
		case ETextureShareThreadMutex::RenderingThread: return TEXT("RenderingThread");
		default:
			break;
		}

		return TEXT("?");
	}

	static constexpr auto GetTEXT(const ETextureShareSyncPass In)
	{
		switch (In)
		{
		case ETextureShareSyncPass::Enter:    return TEXT("Enter");
		case ETextureShareSyncPass::Exit:     return TEXT("Exit");
		case ETextureShareSyncPass::Complete: return TEXT("Complete");
		default:
			break;
		}

		return TEXT("?");
	}

	static constexpr auto GetTEXT(const ETextureShareInterprocessObjectSyncBarrierState In)
	{
		switch (In)
		{
			case ETextureShareInterprocessObjectSyncBarrierState::Undefined: return TEXT("Undefined");
			case ETextureShareInterprocessObjectSyncBarrierState::UnusedSyncStep: return TEXT("UnusedSyncStep");

			case ETextureShareInterprocessObjectSyncBarrierState::WaitConnection: return TEXT("WaitConnection");
			case ETextureShareInterprocessObjectSyncBarrierState::AcceptConnection: return TEXT("AcceptConnection");

			case ETextureShareInterprocessObjectSyncBarrierState::Wait: return TEXT("Wait");
			case ETextureShareInterprocessObjectSyncBarrierState::Accept: return TEXT("Accept");

			case ETextureShareInterprocessObjectSyncBarrierState::ObjectLost: return TEXT("ObjectLost");
			case ETextureShareInterprocessObjectSyncBarrierState::FrameSyncLost: return TEXT("FrameSyncLost");

			case ETextureShareInterprocessObjectSyncBarrierState::InvalidLogic: return TEXT("InvalidLogic");
		}

		return TEXT("Unknown");
	};

	static constexpr auto GetTEXT(const ETextureShareCoreInterprocessObjectFrameSyncState In)
	{
		switch (In)
		{
		case  ETextureShareCoreInterprocessObjectFrameSyncState::Undefined: return TEXT("Undefined");
		case  ETextureShareCoreInterprocessObjectFrameSyncState::FrameSyncLost: return TEXT("FrameSyncLost");

		case  ETextureShareCoreInterprocessObjectFrameSyncState::NewFrame: return TEXT("NewFrame");
		case  ETextureShareCoreInterprocessObjectFrameSyncState::FrameConnected: return TEXT("FrameConnected");
		case  ETextureShareCoreInterprocessObjectFrameSyncState::FrameBegin: return TEXT("FrameBegin");
		case  ETextureShareCoreInterprocessObjectFrameSyncState::FrameEnd: return TEXT("FrameEnd");

		case  ETextureShareCoreInterprocessObjectFrameSyncState::FrameProxyBegin:  return TEXT("FrameProxyBegin");
		case  ETextureShareCoreInterprocessObjectFrameSyncState::FrameProxyEnd:  return TEXT("FrameProxyEnd");
		default:
			break;
		}

		return TEXT("Unknown");
	};

	static constexpr auto GetTEXT(const ETextureShareSyncState In)
	{
		switch (In)
		{
		case ETextureShareSyncState::Enter:          return TEXT("1");
		case ETextureShareSyncState::EnterCompleted: return TEXT("1+");

		case ETextureShareSyncState::Exit:          return TEXT("2");
		case ETextureShareSyncState::ExitCompleted: return TEXT("2+");

		case ETextureShareSyncState::Completed: return TEXT("##");

		case ETextureShareSyncState::Undefined:
			break;
		}

		return TEXT("?");
	}

	static constexpr auto GetTEXT(const ETextureShareSyncStep In)
	{
		switch (In)
		{
		case ETextureShareSyncStep::InterprocessConnection: return TEXT("IC");

		/**
		 * Frame sync steps(GameThread)
		 */

		case ETextureShareSyncStep::FrameBegin: return TEXT("f");

		case ETextureShareSyncStep::FramePreSetupBegin: return TEXT("fpS");
		case ETextureShareSyncStep::FramePreSetupEnd: return TEXT("fpS+");

		case ETextureShareSyncStep::FrameSetupBegin: return TEXT("fS");
		case ETextureShareSyncStep::FrameSetupEnd: return TEXT("fS+");

		case ETextureShareSyncStep::FramePostSetupBegin: return TEXT("fPS");
		case ETextureShareSyncStep::FramePostSetupEnd: return TEXT("fPS+");

		case ETextureShareSyncStep::FrameEnd: return TEXT("f+");

		/**
		 * FrameProxy sync steps (RenderThread)
		 */

		case ETextureShareSyncStep::FrameProxyBegin: return TEXT("F");

		case ETextureShareSyncStep::FrameSceneFinalColorBegin: return TEXT("F_fc");
		case ETextureShareSyncStep::FrameSceneFinalColorEnd: return TEXT("F_fc+");

		case ETextureShareSyncStep::FrameProxyPreRenderBegin: return TEXT("FpR");
		case ETextureShareSyncStep::FrameProxyPreRenderEnd: return TEXT("FpR+");

		case ETextureShareSyncStep::FrameProxyRenderBegin: return TEXT("FR");
		case ETextureShareSyncStep::FrameProxyRenderEnd: return TEXT("FR+");

		case ETextureShareSyncStep::FrameProxyPostWarpBegin: return TEXT("FPW");
		case ETextureShareSyncStep::FrameProxyPostWarpEnd: return TEXT("FPW+");

		case ETextureShareSyncStep::FrameProxyPostRenderBegin: return TEXT("FPR");
		case ETextureShareSyncStep::FrameProxyPostRenderEnd: return TEXT("FPR+");
		
		case ETextureShareSyncStep::FrameProxyBackBufferReadyToPresentBegin: return TEXT("FBbPr");
		case ETextureShareSyncStep::FrameProxyBackBufferReadyToPresentEnd: return TEXT("FBbPr+");

		case ETextureShareSyncStep::FrameProxyFlush: return TEXT("FLUSH");

		case ETextureShareSyncStep::FrameProxyEnd: return TEXT("F+");

		default:
			break;
		}

		return TEXT("?");
	}

	static FString ToString(const FTextureShareCoreInterprocessObject& InObject)
	{
		return FString::Printf(TEXT("%s(%s)"), *InObject.Desc.ProcessName.ToString(), *InObject.Sync.GetSyncState().ToString());
	}

	static FString ToString(const TArray<FTextureShareCoreInterprocessObject*>& InObjects)
	{
		FString Result;
		for (const FTextureShareCoreInterprocessObject* It : InObjects)
		{
			if (!Result.IsEmpty())
			{
				Result += TEXT(", ");
			}

			Result += ToString(*It);
		}

		return Result;
	}

	static FString ToString(const FTextureShareCoreObjectDesc& InObject)
	{
		const FTextureShareCoreObjectSyncState& InSyncState = InObject.Sync.SyncState;
		const FString SyncState = FString::Printf(TEXT("%s[%s]{%s < %s}"), GetTEXT(InSyncState.Step), GetTEXT(InSyncState.State), GetTEXT(InSyncState.NextStep), GetTEXT(InSyncState.PrevStep));

		return FString::Printf(TEXT("%s(%s)"), *InObject.ProcessDesc.ProcessId, *SyncState);
	}

	static FString ToString(const TArraySerializable<FTextureShareCoreObjectDesc>& InObjects)
	{
		FString Result;
		for (const FTextureShareCoreObjectDesc& ObjectDescIt : InObjects)
		{
			if (!Result.IsEmpty())
			{
				Result += TEXT(", ");
			}

			Result += ToString(ObjectDescIt);
		}

		return Result;
	}
};
