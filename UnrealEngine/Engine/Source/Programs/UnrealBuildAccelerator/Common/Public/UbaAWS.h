// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaFileAccessor.h"
#include "UbaNetworkBackend.h"
#include "UbaStringBuffer.h"

#define UBA_USE_AWS !PLATFORM_MAC

#if UBA_USE_AWS

namespace uba
{
	class AWS
	{
	public:

		static constexpr char g_imdsHost[]						= "169.254.169.254";
		static constexpr char g_imdsInstanceId[]				= "latest/meta-data/instance-id";
		static constexpr char g_imdsInstanceLifeCycle[]			= "latest/meta-data/instance-life-cycle";
		static constexpr char g_imdsAutoScalingLifeCycleState[] = "latest/meta-data/autoscaling/target-lifecycle-state";
		static constexpr char g_imdsInstanceAvailabilityZone[]	= "latest/meta-data/placement/availability-zone";
		static constexpr char g_imdsSpotInstanceAction[]		= "latest/meta-data/spot/instance-action";
		static constexpr char g_imdsToken[]						= "latest/api/token";


		bool QueryInformation(Logger& logger, StringBufferBase& outExtraInfo, const tchar* rootDir)
		{
			if (IsNotAWS(logger, rootDir))
				return false;

			HttpConnection http;

			u32 statusCode = 0;

			StringBuffer<128> token;
			token.Append(TC("X-aws-ec2-metadata-token: "));
			if (!http.Query(logger, "PUT", token, statusCode, g_imdsHost, g_imdsToken, "X-aws-ec2-metadata-token-ttl-seconds: 21600\r\n"))
				return false;
			token.Append("\r\n");

			#if PLATFORM_WINDOWS
			char tokenString[512];
			size_t tokenStringLen;
			wcstombs_s(&tokenStringLen, tokenString, sizeof_array(tokenString), token.data, _TRUNCATE);
			m_tokenString = tokenString;
			#else
			m_tokenString = token.data;
			#endif

			StringBuffer<128> instanceId;
			if (!http.Query(logger, "GET", instanceId, statusCode, g_imdsHost, g_imdsInstanceId, m_tokenString.c_str()))
				return WriteIsNotAws(logger, rootDir);

			outExtraInfo.Append(TC(", AWS: ")).Append(instanceId);

			StringBuffer<32> instanceLifeCycle;
			if (http.Query(logger, "GET", instanceLifeCycle, statusCode, g_imdsHost, g_imdsInstanceLifeCycle, m_tokenString.c_str()))
			{
				outExtraInfo.Append(' ').Append(instanceLifeCycle);
				m_isSpot = instanceLifeCycle.Contains(TC("spot"));
			}

			StringBuffer<32> autoscaling;
			if (http.Query(logger, "GET", autoscaling, statusCode, g_imdsHost, g_imdsAutoScalingLifeCycleState, m_tokenString.c_str()) && statusCode == 200)
			{
				outExtraInfo.Append(m_isSpot ? '/' : ' ').Append(TC("autoscale"));
				m_isAutoscaling = true;
			}

			if (!QueryAvailabilityZone(logger, nullptr))
				return false;

			return true;
		}

		bool QueryAvailabilityZone(Logger& logger, const tchar* rootDir)
		{
			if (rootDir && IsNotAWS(logger, rootDir))
				return false;

			HttpConnection http;

			StringBuffer<128> availabilityZone;
			u32 statusCode = 0;
			if (!http.Query(logger, "GET", availabilityZone, statusCode, g_imdsHost, g_imdsInstanceAvailabilityZone, m_tokenString.c_str()))
			{
				if (rootDir)
					WriteIsNotAws(logger, rootDir);
				return false;
			}
			m_availabilityZone = availabilityZone.data;
			return true;
		}

		// Returns true if we _know_ we are not in AWS
		bool IsNotAWS(Logger& logger, const tchar* rootDir)
		{
			StringBuffer<512> file;
			file.Append(rootDir).EnsureEndsWithSlash().Append(".isNotAWS");
			if (FileExists(logger, file.data))
				return true;
			return false;
		}

		bool WriteIsNotAws(Logger& logger, const tchar* rootDir)
		{
			StringBuffer<512> file;
			file.Append(rootDir).EnsureEndsWithSlash().Append(".isNotAWS");
			FileAccessor f(logger, file.data);
			if (!f.CreateWrite())
				return false;
			if (!f.Close())
				return false;
			return false;
		}

		bool IsTerminating(Logger& logger, StringBufferBase& outReason, u64& outTerminationTimeMs)
		{
			HttpConnection http;

			outTerminationTimeMs = 0;
			if (m_isSpot)
			{
				StringBuffer<1024> content;
				u32 statusCode = 0;
				if (http.Query(logger, "GET", content, statusCode, g_imdsHost, g_imdsSpotInstanceAction, m_tokenString.c_str()) && statusCode == 200)
				{
					outReason.Append(TC("AWS spot instance interruption"));
					return true;
				}
			}

			if (m_isAutoscaling)
			{
				StringBuffer<1024> content;
				u32 statusCode = 0;
				if (http.Query(logger, "GET", content, statusCode, g_imdsHost, g_imdsAutoScalingLifeCycleState, m_tokenString.c_str()) && statusCode == 200)
				{
					//if (!content.Equals(L"InService"))
					//{
					//	wprintf(L"AWSACTION: AUTOSCALE: %ls\n", content.data);
					//}

					if (!content.Contains(TC("InService"))) // AWS can return "InServiceI" as well?
					{
						//wprintf(L"AWSACTION: AUTOSCALE REBALANCING!!!! (%ls)\n", content.data);
						outReason.Append(TC("AWS autoscale rebalancing"));
						return true;
					}
				}
			}
			return false;
		}

		const tchar* GetAvailabilityZone()
		{
			return m_availabilityZone.c_str();
		}

		TString m_availabilityZone;
		std::string m_tokenString;
		bool m_isSpot = false;
		bool m_isAutoscaling = false;
	};
}

#endif
