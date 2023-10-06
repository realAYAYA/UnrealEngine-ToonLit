// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Common.h"

#include <vector>
#include <memory>

namespace BlackmagicDesign
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
			bool GetDeviceTextId(int32_t InDeviceIndex, BlackmagicDesign::BlackmagicDeviceScanner::FormatedTextType& OutTextId) const;
			bool GetDeviceInfo(int32_t InDeviceIndex, BlackmagicDesign::BlackmagicDeviceScanner::DeviceInfo& OutDeviceInfo) const;

		private:
			IDeckLink* GetDevice(int32_t InDeviceIndex) const;

		private:
			std::vector<IDeckLink*> Devices;
		};
	}
}
