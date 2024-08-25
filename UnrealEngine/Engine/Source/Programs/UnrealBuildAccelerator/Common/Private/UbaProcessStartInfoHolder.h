// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#define Local_GetLongPathNameW uba::GetLongPathNameW

#include "UbaBinaryReaderWriter.h"
#include "UbaFile.h"
#include "UbaPathUtils.h"
#include "UbaProcessStartInfo.h"

namespace uba
{
	inline void FixFileName(StringBufferBase& out, const tchar* fileName, const tchar* workingDir)
	{
		tchar buffer[1024];
		u32 charLen;
		u64 workingDirLen = 0;
		if (workingDir)
			workingDirLen = TStrlen(workingDir);
		FixPath2(fileName, workingDir, workingDirLen, buffer, sizeof_array(buffer), &charLen);
		out.Append(buffer);
	}

	struct ProcessStartInfoHolder
	{
		ProcessStartInfoHolder() {}

		ProcessStartInfoHolder(const ProcessStartInfo& si)
		{
			startInfo = si;

			StringBuffer<512> temp;
			FixFileName(temp, si.workingDir, nullptr);
			temp.EnsureEndsWithSlash();
			workingDir = temp.data;
			startInfo.workingDir = workingDir.c_str();

			temp.EnsureEndsWithSlash();
			StringBuffer<512> temp2;
			FixFileName(temp2, si.application, temp.data);
			application = temp2.data;
			startInfo.application = application.c_str();

			arguments = si.arguments;
			startInfo.arguments = arguments.c_str();

			description = si.description;
			startInfo.description = description.c_str();

			logFile = si.logFile;
			startInfo.logFile = logFile.c_str();
		}

		void Write(BinaryWriter& writer)
		{
			writer.WriteString(description);
			writer.WriteString(application);
			writer.WriteString(arguments);
			writer.WriteString(workingDir);
			writer.WriteString(logFile);
			writer.WriteU32(*(u32*)&weight);
			writer.WriteBool(startInfo.writeOutputFilesOnFail);
			writer.WriteU64(startInfo.outputStatsThresholdMs);
		}

		void Read(BinaryReader& reader)
		{
			description = reader.ReadString();
			application = reader.ReadString();
			arguments = reader.ReadString();
			workingDir = reader.ReadString();
			logFile = reader.ReadString();

			Replace(application.data(), '/', PathSeparator); // TODO: Is this needed?

			u32 weight32 = reader.ReadU32();
			weight = *(float*)&weight32;
			
			startInfo.outputStatsThresholdMs = reader.ReadU64();
			startInfo.writeOutputFilesOnFail = reader.ReadBool();

			startInfo.description = description.c_str();
			startInfo.application = application.c_str();
			startInfo.arguments = arguments.c_str();
			startInfo.workingDir = workingDir.c_str();
			startInfo.logFile = logFile.c_str();
		}

		ProcessStartInfoHolder(const ProcessStartInfoHolder& o)
		{
			*this = o;
		}

		void operator=(const ProcessStartInfoHolder& o)
		{
			startInfo = o.startInfo;

			workingDir = o.workingDir;
			startInfo.workingDir = workingDir.c_str();
			application = o.application;
			startInfo.application = application.c_str();
			arguments = o.arguments;
			startInfo.arguments = arguments.c_str();
			description = o.description;
			startInfo.description = description.c_str();
			logFile = o.logFile;
			startInfo.logFile = logFile.c_str();

			weight = o.weight;
		}

		ProcessStartInfo startInfo;

		TString description;
		TString application;
		TString arguments;
		TString workingDir;
		TString logFile;
		float weight = 1.0f;
	};

}