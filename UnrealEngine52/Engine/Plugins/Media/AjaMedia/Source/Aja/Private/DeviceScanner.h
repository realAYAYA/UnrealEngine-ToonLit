// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Helpers.h"

#include <vector>


namespace AJA
{
	namespace Private
	{
		/* DeviceScanner definition
		*****************************************************************************/
		class DeviceScanner
		{
		public:
			DeviceScanner();
			~DeviceScanner();

			DeviceScanner(const DeviceScanner&) = delete;
			DeviceScanner& operator=(const DeviceScanner&) = delete;

			int32_t GetNumDevices() const;
			bool GetDeviceTextId(int32_t InDeviceIndex, AJADeviceScanner::FormatedTextType& OutTextId) const;
			bool GetDeviceInfo(int32_t InDeviceIndex, AJADeviceScanner::DeviceInfo& OutDeviceInfo) const;

		private:
			CNTV2DeviceScanner* Scanner;
		};
	}
}
