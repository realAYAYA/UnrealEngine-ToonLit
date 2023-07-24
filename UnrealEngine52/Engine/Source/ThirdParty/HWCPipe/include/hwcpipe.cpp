/*
 * Copyright (c) 2019 ARM Limited.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "hwcpipe.h"
#include "hwcpipe_log.h"

#ifdef __linux__
#	include "vendor/arm/pmu/pmu_profiler.h"
#	include "vendor/arm/mali/mali_profiler.h"
#endif

#ifndef HWCPIPE_NO_JSON
#include <json.hpp>
using json = nlohmann::json;
#endif

#include <memory>

namespace hwcpipe
{
const char* error_msg = nullptr;

// Mapping from CPU counter names to enum values. Used for JSON initialization.
const std::unordered_map<std::string, CpuCounter> cpu_counter_names{
    {"Cycles", CpuCounter::Cycles},
    {"Instructions", CpuCounter::Instructions},
    {"CacheReferences", CpuCounter::CacheReferences},
    {"CacheMisses", CpuCounter::CacheMisses},
    {"BranchInstructions", CpuCounter::BranchInstructions},
    {"BranchMisses", CpuCounter::BranchMisses},

    {"L1Accesses", CpuCounter::L1Accesses},
    {"InstrRetired", CpuCounter::InstrRetired},
    {"L2Accesses", CpuCounter::L2Accesses},
    {"L3Accesses", CpuCounter::L3Accesses},
    {"BusReads", CpuCounter::BusReads},
    {"BusWrites", CpuCounter::BusWrites},
    {"MemReads", CpuCounter::MemReads},
    {"MemWrites", CpuCounter::MemWrites},
    {"ASESpec", CpuCounter::ASESpec},
    {"VFPSpec", CpuCounter::VFPSpec},
    {"CryptoSpec", CpuCounter::CryptoSpec},
};

// Mapping from GPU counter names to enum values. Used for JSON initialization.
const std::unordered_map<std::string, GpuCounter> gpu_counter_names{
    {"GpuCycles", GpuCounter::GpuCycles},
    {"VertexComputeCycles", GpuCounter::VertexComputeCycles},
    {"FragmentCycles", GpuCounter::FragmentCycles},
    {"TilerCycles", GpuCounter::TilerCycles},

    {"VertexComputeJobs", GpuCounter::VertexComputeJobs},
    {"Tiles", GpuCounter::Tiles},
    {"TransactionEliminations", GpuCounter::TransactionEliminations},
    {"FragmentJobs", GpuCounter::FragmentJobs},
    {"Pixels", GpuCounter::Pixels},

    {"EarlyZTests", GpuCounter::EarlyZTests},
    {"EarlyZKilled", GpuCounter::EarlyZKilled},
    {"LateZTests", GpuCounter::LateZTests},
    {"LateZKilled", GpuCounter::LateZKilled},

    {"Instructions", GpuCounter::Instructions},
    {"DivergedInstructions", GpuCounter::DivergedInstructions},

    {"ShaderCycles", GpuCounter::ShaderCycles},
    {"ShaderArithmeticCycles", GpuCounter::ShaderArithmeticCycles},
    {"ShaderLoadStoreCycles", GpuCounter::ShaderLoadStoreCycles},
    {"ShaderTextureCycles", GpuCounter::ShaderTextureCycles},

    {"CacheReadLookups", GpuCounter::CacheReadLookups},
    {"CacheWriteLookups", GpuCounter::CacheWriteLookups},
    {"ExternalMemoryReadAccesses", GpuCounter::ExternalMemoryReadAccesses},
    {"ExternalMemoryWriteAccesses", GpuCounter::ExternalMemoryWriteAccesses},
    {"ExternalMemoryReadStalls", GpuCounter::ExternalMemoryReadStalls},
    {"ExternalMemoryWriteStalls", GpuCounter::ExternalMemoryWriteStalls},
    {"ExternalMemoryReadBytes", GpuCounter::ExternalMemoryReadBytes},
    {"ExternalMemoryWriteBytes", GpuCounter::ExternalMemoryWriteBytes},
};

#ifndef HWCPIPE_NO_JSON
HWCPipe::HWCPipe(const char *json_string)
{
	auto json = json::parse(json_string);

	CpuCounterSet enabled_cpu_counters{};
	auto          cpu = json.find("cpu");
	if (cpu != json.end())
	{
		for (auto &counter_name : cpu->items())
		{
			auto counter = cpu_counter_names.find(counter_name.value().get<std::string>());
			if (counter != cpu_counter_names.end())
			{
				enabled_cpu_counters.insert(counter->second);
			}
			else
			{
				HWCPIPE_LOG("CPU counter \"%s\" not found.", counter_name.value().get<std::string>().c_str());
			}
		}
	}

	GpuCounterSet enabled_gpu_counters{};
	auto          gpu = json.find("gpu");
	if (gpu != json.end())
	{
		for (auto &counter_name : gpu->items())
		{
			auto counter = gpu_counter_names.find(counter_name.value().get<std::string>());
			if (counter != gpu_counter_names.end())
			{
				enabled_gpu_counters.insert(counter->second);
			}
			else
			{
				HWCPIPE_LOG("GPU counter \"%s\" not found.", counter_name.value().get<std::string>().c_str());
			}
		}
	}

	create_profilers(std::move(enabled_cpu_counters), std::move(enabled_gpu_counters));
}
#endif

HWCPipe::HWCPipe(const CpuCounterSet &enabled_cpu_counters, const GpuCounterSet& enabled_gpu_counters)
{
	create_profilers(enabled_cpu_counters, enabled_gpu_counters);
}

HWCPipe::HWCPipe()
{
	CpuCounterSet enabled_cpu_counters{CpuCounter::Cycles,
	                                   CpuCounter::Instructions,
	                                   CpuCounter::CacheReferences,
	                                   CpuCounter::CacheMisses,
	                                   CpuCounter::BranchInstructions,
	                                   CpuCounter::BranchMisses};

	GpuCounterSet enabled_gpu_counters{GpuCounter::GpuCycles,
	                                   GpuCounter::VertexComputeCycles,
	                                   GpuCounter::FragmentCycles,
	                                   GpuCounter::TilerCycles,
	                                   GpuCounter::CacheReadLookups,
	                                   GpuCounter::CacheWriteLookups,
	                                   GpuCounter::ExternalMemoryReadAccesses,
	                                   GpuCounter::ExternalMemoryWriteAccesses,
	                                   GpuCounter::ExternalMemoryReadStalls,
	                                   GpuCounter::ExternalMemoryWriteStalls,
	                                   GpuCounter::ExternalMemoryReadBytes,
	                                   GpuCounter::ExternalMemoryWriteBytes};

	create_profilers(enabled_cpu_counters, enabled_gpu_counters);
}

void HWCPipe::set_enabled_cpu_counters(const CpuCounterSet& counters)
{
	if (cpu_profiler_)
	{
		cpu_profiler_->set_enabled_counters(counters);
	}
}

void HWCPipe::set_enabled_gpu_counters(const GpuCounterSet& counters)
{
	if (gpu_profiler_)
	{
		gpu_profiler_->set_enabled_counters(counters);
	}
}

void HWCPipe::run()
{
	if (cpu_profiler_)
	{
		cpu_profiler_->run();
	}
	if (gpu_profiler_)
	{
		gpu_profiler_->run();
	}
}

Measurements HWCPipe::sample()
{
	Measurements m;
	if (cpu_profiler_)
	{
		m.cpu = &cpu_profiler_->sample();
	}
	if (gpu_profiler_)
	{
		m.gpu = &gpu_profiler_->sample();
	}
	return m;
}

void HWCPipe::create_profilers(const CpuCounterSet &enabled_cpu_counters, const GpuCounterSet& enabled_gpu_counters)
{
	// Automated platform detection
#ifdef __linux__
	if (enabled_cpu_counters.size())
	{
		cpu_profiler_ = std::unique_ptr<PmuProfiler>(new PmuProfiler(enabled_cpu_counters));
		if (hwcpipe::error_msg)
		{
			HWCPIPE_LOG("PMU profiler initialization failed: %s", hwcpipe::error_msg);
			return;
		}
	}

	if (enabled_gpu_counters.size())
	{
		gpu_profiler_ = std::unique_ptr<MaliProfiler>(new MaliProfiler(enabled_gpu_counters));
		if (hwcpipe::error_msg)	
		{
			HWCPIPE_LOG("Mali profiler initialization failed: %s", hwcpipe::error_msg);
		}
	}
#else
	hwcpipe::error_msg = "No counters available for this platform.";
	HWCPIPE_LOG(hwcpipe::error_msg);
#endif
}

}        // namespace hwcpipe
