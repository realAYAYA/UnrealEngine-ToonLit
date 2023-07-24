// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"




namespace Electra
{

	namespace Playlist
	{

		enum class EListType
		{
			Master,
			Variant
		};

		static const TCHAR* const GetPlaylistTypeString(EListType InListType)
		{
			switch (InListType)
			{
				case EListType::Master:
					return TEXT("Master");
				case EListType::Variant:
					return TEXT("Variant");
			}
			return TEXT("n/a");
		}


		enum class ELoadType
		{
			Initial,
			Update
		};

		static const TCHAR* const GetPlaylistLoadTypeString(ELoadType InLoadType)
		{
			switch (InLoadType)
			{
				case ELoadType::Initial:
					return TEXT("Initial");
				case ELoadType::Update:
					return TEXT("Update");
			}
			return TEXT("n/a");
		}

	}

} // namespace Electra


