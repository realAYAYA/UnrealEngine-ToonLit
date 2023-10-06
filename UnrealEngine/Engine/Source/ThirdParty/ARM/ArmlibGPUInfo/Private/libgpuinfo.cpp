/*
 * Copyright (c) 2021-2023 ARM Limited.
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

#include <array>
#include <cerrno>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "libgpuinfo.hpp"

namespace libgpuinfo {

struct product_entry {
    uint32_t id;
    uint32_t mask;
    uint32_t min_cores;
    const char* name;
    const char* architecture;
    uint32_t fp32_fmas_per_engine;
    std::function<uint32_t(int, uint32_t, uint32_t)> get_num_texels;
    std::function<uint32_t(int, uint32_t, uint32_t)> get_num_pixels;
    std::function<uint32_t(int, uint32_t, uint32_t)> get_num_exec_engines;
};

static const uint32_t MASK_OLD { 0xFFFF };
static const uint32_t MASK_NEW { 0xF00F };

static uint32_t get_num_1(
    int core_count,
    uint32_t core_features,
    uint32_t thread_features
) {
    return 1;
}

static uint32_t get_num_2(
    int core_count,
    uint32_t core_features,
    uint32_t thread_features
) {
    return 2;
}

static uint32_t get_num_3(
    int core_count,
    uint32_t core_features,
    uint32_t thread_features
) {
    return 3;
}

static uint32_t get_num_4(
    int core_count,
    uint32_t core_features,
    uint32_t thread_features
) {
    return 4;
}

static uint32_t get_num_8(
    int core_count,
    uint32_t core_features,
    uint32_t thread_features
) {
    return 8;
}

static uint32_t get_num_ee_g31(
    int core_count,
    uint32_t core_features,
    uint32_t thread_features
) {
    if ((core_count == 1) && ((thread_features & 0xFFFF) == 0x2000))
    {
        return 1;
    }
    return 2;
}

static uint32_t get_num_ee_g51(
    int core_count,
    uint32_t core_features,
    uint32_t thread_features
) {
    if ((core_count == 1) && ((thread_features & 0xFFFF) == 0x2000))
    {
        return 1;
    }
    return 3;
}

static uint32_t get_num_ee_g52(
    int core_count,
    uint32_t core_features,
    uint32_t thread_features
) {
    return core_features & 0xF;
}

static uint32_t get_num_ee_g310_g510(
    int core_count,
    uint32_t core_features,
    uint32_t thread_features
) {
    if ((core_features & 0xF) <= 1)
    {
        return 1;
    }

    return 2;
}

const std::array<product_entry, 30> PRODUCT_VERSIONS {{
    //                  ID,  ID Mask, Min cores,              Name,      Args, FMA,    Texels,    Pixels,   Engines
    product_entry { 0x6956, MASK_OLD,         1,       "Mali-T600", "Midgard",  4, get_num_1, get_num_1, get_num_2 },
    product_entry { 0x0620, MASK_OLD,         1,       "Mali-T620", "Midgard",  4, get_num_1, get_num_1, get_num_2 },
    product_entry { 0x0720, MASK_OLD,         1,       "Mali-T720", "Midgard",  4, get_num_1, get_num_1, get_num_1 },
    product_entry { 0x0750, MASK_OLD,         1,       "Mali-T760", "Midgard",  4, get_num_1, get_num_1, get_num_2 },
    product_entry { 0x0820, MASK_OLD,         1,       "Mali-T820", "Midgard",  4, get_num_1, get_num_1, get_num_1 },
    product_entry { 0x0830, MASK_OLD,         1,       "Mali-T830", "Midgard",  4, get_num_1, get_num_1, get_num_2 },
    product_entry { 0x0860, MASK_OLD,         1,       "Mali-T860", "Midgard",  4, get_num_1, get_num_1, get_num_2 },
    product_entry { 0x0880, MASK_OLD,         1,       "Mali-T880", "Midgard",  4, get_num_1, get_num_1, get_num_3 },
    product_entry { 0x6000, MASK_NEW,         1,        "Mali-G71", "Bifrost",  4, get_num_1, get_num_1, get_num_3 },
    product_entry { 0x6001, MASK_NEW,         1,        "Mali-G72", "Bifrost",  4, get_num_1, get_num_1, get_num_3 },
    product_entry { 0x7000, MASK_NEW,         1,        "Mali-G51", "Bifrost",  4, get_num_2, get_num_2, get_num_ee_g51 },
    product_entry { 0x7001, MASK_NEW,         1,        "Mali-G76", "Bifrost",  8, get_num_2, get_num_2, get_num_3 },
    product_entry { 0x7002, MASK_NEW,         1,        "Mali-G52", "Bifrost",  8, get_num_2, get_num_2, get_num_ee_g52 },
    product_entry { 0x7003, MASK_NEW,         1,        "Mali-G31", "Bifrost",  4, get_num_2, get_num_2, get_num_ee_g31 },
    product_entry { 0x9000, MASK_NEW,         1,        "Mali-G77", "Valhall", 16, get_num_4, get_num_2, get_num_2 },
    product_entry { 0x9001, MASK_NEW,         1,        "Mali-G57", "Valhall", 16, get_num_4, get_num_2, get_num_2 },
    product_entry { 0x9003, MASK_NEW,         1,        "Mali-G57", "Valhall", 16, get_num_4, get_num_2, get_num_2 },
    product_entry { 0x9004, MASK_NEW,         1,        "Mali-G68", "Valhall", 16, get_num_4, get_num_2, get_num_2 },
    product_entry { 0x9002, MASK_NEW,         1,        "Mali-G78", "Valhall", 16, get_num_4, get_num_2, get_num_2 },
    product_entry { 0x9005, MASK_NEW,         1,      "Mali-G78AE", "Valhall", 16, get_num_4, get_num_2, get_num_2 },
    product_entry { 0xa002, MASK_NEW,         1,       "Mali-G710", "Valhall", 32, get_num_8, get_num_4, get_num_2 },
    product_entry { 0xa007, MASK_NEW,         1,       "Mali-G610", "Valhall", 32, get_num_8, get_num_4, get_num_2 },
    // TODO: Extract FMA, pixel, and texel settings
    product_entry { 0xa003, MASK_NEW,         1,       "Mali-G510", "Valhall", 32, get_num_8, get_num_4, get_num_ee_g310_g510 },
    // TODO: Extract FMA, pixel, and texel settings
    product_entry { 0xa004, MASK_NEW,         1,       "Mali-G310", "Valhall", 32, get_num_8, get_num_4, get_num_ee_g310_g510 },
    product_entry { 0xb002, MASK_NEW,        10, "Immortalis-G715", "Valhall", 64, get_num_8, get_num_4, get_num_2 },
    product_entry { 0xb003, MASK_NEW,        10, "Immortalis-G715", "Valhall", 64, get_num_8, get_num_4, get_num_2 },
    product_entry { 0xb002, MASK_NEW,         7,       "Mali-G715", "Valhall", 64, get_num_8, get_num_4, get_num_2 },
    product_entry { 0xb003, MASK_NEW,         7,       "Mali-G715", "Valhall", 64, get_num_8, get_num_4, get_num_2 },
    product_entry { 0xb002, MASK_NEW,         1,       "Mali-G615", "Valhall", 64, get_num_8, get_num_4, get_num_2 },
    product_entry { 0xb003, MASK_NEW,         1,       "Mali-G615", "Valhall", 64, get_num_8, get_num_4, get_num_2 },
}};

uint32_t get_gpu_id(
    uint32_t gpu_id
) {
    for (const auto& entry : PRODUCT_VERSIONS)
    {
        if (((gpu_id & entry.mask) == entry.id))
        {
            return gpu_id & entry.mask;
        }
    }

    return gpu_id;
}

const char* get_gpu_name(
    uint32_t gpu_id,
    int core_count
) {
    for (const auto& entry : PRODUCT_VERSIONS)
    {
        if(((gpu_id & entry.mask) == entry.id) &&
           (core_count >= entry.min_cores))
        {
            return entry.name;
        }
    }

    return "Unknown gpu_id";
}

const char* get_architecture_name(
    uint32_t gpu_id
) {
    for (const auto& entry : PRODUCT_VERSIONS)
    {
        if((gpu_id & entry.mask) == entry.id)
        {
            return entry.architecture;
        }
    }

    return "Unknown gpu_id";
}

int get_num_exec_engines(
    uint32_t gpu_id,
    int core_count,
    uint32_t core_features,
    uint32_t thread_features
) {
    for (const auto& entry : PRODUCT_VERSIONS)
    {
        if(((gpu_id & entry.mask) == entry.id) &&
           (core_count >= entry.min_cores))
        {
            return entry.get_num_exec_engines(core_count, core_features, thread_features);
        }
    }

    return 0;
}

const uint32_t get_num_fp32_fmas(
    uint32_t gpu_id,
    int core_count,
    uint32_t core_features,
    uint32_t thread_features
) {
    for (const auto& entry : PRODUCT_VERSIONS)
    {
        if(((gpu_id & entry.mask) == entry.id) &&
           (core_count >= entry.min_cores))
        {
            return entry.fp32_fmas_per_engine * entry.get_num_exec_engines(core_count, core_features, thread_features);
        }
    }

    return 0;
}

const uint32_t get_num_texels(
    uint32_t gpu_id,
    int core_count,
    uint32_t core_features,
    uint32_t thread_features
) {
    for (const auto& entry : PRODUCT_VERSIONS)
    {
        if(((gpu_id & entry.mask) == entry.id) &&
           (core_count >= entry.min_cores))
        {
            return entry.get_num_texels(core_count, core_features, thread_features);
        }
    }

    return 0;
}

const uint32_t get_num_pixels(
    uint32_t gpu_id,
    int core_count,
    uint32_t core_features,
    uint32_t thread_features
) {
    for (const auto& entry : PRODUCT_VERSIONS)
    {
        if(((gpu_id & entry.mask) == entry.id) &&
           (core_count >= entry.min_cores))
        {
            return entry.get_num_pixels(core_count, core_features, thread_features);
        }
    }

    return 0;
}

/** Kbase Pre R21 ioctl interface. */
namespace kbase_pre_r21 {

/** Related to mali0 ioctl interface */
enum class header_id : uint32_t {
    /** Version check. */
    version_check = 0,
    /** Base Context Create Kernel Flags. */
    create_kernel_flags = 2,
    /** Kbase Func Get Props. */
    get_props = 526,
    /** Kbase Func Set Flags. */
    set_flags = 530,
};

/** Message header. */
union uk_header {
    /** Number identifying the called UK function. */
    header_id id;
    /** The return code of the called UK function. */
    uint32_t ret;
    /** Dummy to ensure type has 64-bit alignment */
    uint64_t sizer;
};

/** Check version compatibility between kernel and userspace. */
struct version_check_t {
    /** UK header */
    uk_header header;
    /** Major version number */
    uint16_t major;
    /** Minor version number */
    uint16_t minor;

    bool is_set() const
    {
        return major || minor;
    }
};

/** IOCTL parameters to set flags */
struct set_flags_t {
    /** UK header */
    uk_header header;
    /** Create flags */
    uint32_t create_flags;
    /** Padding */
    uint32_t padding;
};

/** Base GPU Num Texture Features Registers. */
static constexpr const uint32_t base_gpu_num_texture_features_registers = 3;

/** Base Max Coherent Groups. */
static constexpr const uint32_t base_max_coherent_groups = 16;

/** GPU Max Job Slots. */
static constexpr const uint32_t gpu_max_job_slots = 16;

/** Kbase UK GPU props. */
struct uk_gpuprops_t {
    /**
     * IOCTL parameters to probe GPU properties
     *
     * NOTE: the raw_props member in this data structure contains the register
     * values from which the value of the other members are derived. The derived
     * members exist to allow for efficient access and/or shielding the details
     * of the layout of the registers.
     *
     */
    struct gpu_props {
        /** Core. */
        struct core {
            /** Product specific value. */
            uint32_t product_id;
            /**
             * Status of the GPU release.
             * No defined values, but starts at 0 and increases by one for each
             * release status (alpha, beta, EAC, etc.).
             * 4 bit values (0-15).
             */
            uint16_t version_status;
            /**
             * Minor release number of the GPU. "P" part of an "RnPn" release number.
             * 8 bit values (0-255).
             */
            uint16_t minor_revision;
            /**
             * Major release number of the GPU. "R" part of an "RnPn" release number.
             * 4 bit values (0-15).
             */
            uint16_t major_revision;
            /** Padding. */
            uint16_t padding;
            /**
             * This property is deprecated since it has not contained the real current
             * value of GPU clock speed. It is kept here only for backwards compatibility.
             * For the new ioctl interface, it is ignored and is treated as a padding
             * to keep the structure of the same size and retain the placement of its
             * members.
             */
            uint32_t gpu_speed_mhz;
            /**
             * @usecase GPU clock max speed is required for computing best case
             * in tasks as job scheduling ant irq_throttling. (It is not specified in the
             * Midgard Architecture).
             * Also, GPU clock max speed is used for OpenCL's clGetDeviceInfo() function.
             */
            uint32_t gpu_freq_khz_max;
            /**
             * @usecase GPU clock min speed is required for computing worst case
             * in tasks as job scheduling ant irq_throttling. (It is not specified in the
             * Midgard Architecture).
             */
            uint32_t gpu_freq_khz_min;
            /** Size of the shader program counter, in bits. */
            uint32_t log2_program_counter_size;
            /**
             * TEXTURE_FEATURES_x registers, as exposed by the GPU. This is a
             * bitpattern where a set bit indicates that the format is supported.
             *
             * Before using a texture format, it is recommended that the corresponding
             * bit be checked.
             */
            uint32_t texture_features[base_gpu_num_texture_features_registers];
            /**
             * Theoretical maximum memory available to the GPU. It is unlikely that a
             * client will be able to allocate all of this memory for their own
             * purposes, but this at least provides an upper bound on the memory
             * available to the GPU.
             *
             * This is required for OpenCL's clGetDeviceInfo() call when
             * CL_DEVICE_GLOBAL_MEM_SIZE is requested, for OpenCL GPU devices. The
             * client will not be expecting to allocate anywhere near this value.
             */
            uint64_t gpu_available_memory_size;
        };

        /**
         * More information is possible - but associativity and bus width are not
         * required by upper-level apis.
         */
        struct l2_cache {
            /** Log2 Line Size. */
            uint8_t log2_line_size;
            /** Log2 Cache Size. */
            uint8_t log2_cache_size;
            /** Num L2 Slices. */
            uint8_t num_l2_slices;
            /** Padding bytes. */
            uint8_t padding[5];
        };

        /** Tiler. */
        struct tiler {
            /** Max is 4*2^15 */
            uint32_t bin_size_bytes;
            /** Max is 2^15 */
            uint32_t max_active_levels;
        };

        /** GPU threading system details. */
        struct thread {
            /** Max. number of threads per core */
            uint32_t max_threads;
            /** Max. number of threads per workgroup */
            uint32_t max_workgroup_size;
            /** Max. number of threads that can synchronize on a simple barrier */
            uint32_t max_barrier_size;
            /** Total size [1..65535] of the register file available per core. */
            uint16_t max_registers;
            /** Max. tasks [1..255] which may be sent to a core before it becomes blocked. */
            uint8_t max_task_queue;
            /** Max. allowed value [1..15] of the Thread Group Split field. */
            uint8_t max_thread_group_split;
            /** 0 = Not specified, 1 = Silicon, 2 = FPGA, 3 = SW Model/Emulation */
            uint8_t impl_tech;
            /** Padding bytes. */
            uint8_t padding[7];
        };

        /**
         * A complete description of the GPU's Hardware Configuration Discovery
         * registers.
         *
         * The information is presented inefficiently for access. For frequent access,
         * the values should be better expressed in an unpacked form in the
         * base_gpu_props structure.
         *
         * @usecase The raw properties in @ref gpu_raw_gpu_props are necessary to
         * allow a user of the Mali Tools (e.g. PAT) to determine "Why is this device
         * behaving differently?". In this case, all information about the
         * by the driver</b>. Instead, the raw registers can be processed by the Mali
         * Tools software on the host PC.
         */
        struct raw {
            /** Shader Present. */
            uint64_t shader_present;
            /** Tiler Present. */
            uint64_t tiler_present;
            /** L2 Present. */
            uint64_t l2_present;
            /** Unused 1. */
            uint64_t unused_1;
            /** L2 Features. */
            uint32_t l2_features;
            /** Suspend Size. */
            uint32_t suspend_size;
            /** Mem Features. */
            uint32_t mem_features;
            /** Mmu Features. */
            uint32_t mmu_features;
            /** As Present. */
            uint32_t as_present;
            /** Js Present. */
            uint32_t js_present;
            /** Js Features. */
            uint32_t js_features[gpu_max_job_slots];
            /** Tiler Features. */
            uint32_t tiler_features;
            /** Texture Features. */
            uint32_t texture_features[3];
            /** GPU ID. */
            uint32_t gpu_id;
            /** Thread Max Threads. */
            uint32_t thread_max_threads;
            /** Thread Max Workgroup Size. */
            uint32_t thread_max_workgroup_size;
            /** Thread Max Barrier Size. */
            uint32_t thread_max_barrier_size;
            /** Thread Features. */
            uint32_t thread_features;
            /**
             * Coherency Mode.
             * Note: This is the _selected_ coherency mode rather than the
             * available modes as exposed in the coherency_features register.
             */
            uint32_t coherency_mode;
        };

        /**
         * Coherency group information
         *
         * Note that the sizes of the members could be reduced. However, the \c group
         * member might be 8-byte aligned to ensure the u64 core_mask is 8-byte
         * aligned, thus leading to wastage if the other members sizes were reduced.
         *
         * The groups are sorted by core mask. The core masks are non-repeating and do
         * not intersect.
         */
        struct coherent_group_info {
            /**
             * descriptor for a coherent group
             *
             * \c core_mask exposes all cores in that coherent group, and \c num_cores
             * provides a cached population-count for that mask.
             *
             * @note Whilst all cores are exposed in the mask, not all may be available to
             * the application, depending on the Kernel Power policy.
             *
             * @note if u64s must be 8-byte aligned, then this structure has 32-bits of
             * wastage.
             */
            struct coherent_group {
                /** Core restriction mask required for the group */
                uint64_t core_mask;
                /** Number of cores in the group */
                uint16_t num_cores;
                /** Padding bytes. */
                uint16_t padding[3];
            };

            /** Num Groups. */
            uint32_t num_groups;
            /**
             * Number of core groups (coherent or not) in the GPU. Equivalent to the number of
             * L2 Caches.
             * The GPU Counter dumping writes 2048 bytes per core group, regardless of whether
             * the core groups are coherent or not. Hence this member is needed to calculate
             * how much memory is required for dumping.
             * @note Do not use it to work out how many valid elements are in the group[]
             * member. Use num_groups instead.
             */
            uint32_t num_core_groups;
            /** Coherency features of the memory, accessed by @ref gpu_mem_features methods. */
            uint32_t coherency;
            /** Padding. */
            uint32_t padding;
            /** Descriptors of coherent groups */
            coherent_group group[base_max_coherent_groups];
        };

        /** Core Props. */
        core core_props;
        /** L2 Props. */
        l2_cache l2_props;
        /** Unused to keep for backwards compatibility. */
        uint64_t unused;
        /** Tiler Props. */
        tiler tiler_props;
        /** Thread Props. */
        thread thread_props;
        /** This member is large, likely to be 128 bytes. */
        raw raw_props;
        /** This must be last member of the structure. */
        coherent_group_info coherency_info;
    };

    /** Header. */
    uk_header header;
    /** Props. */
    gpu_props props;
};

constexpr auto iface_number = 0x80;

/** Commands describing kbase_pre_r21 ioctl interface. */
enum command_type {
    /** Check version compatibility between JM kernel and userspace. */
    version_check = _IOWR(iface_number, 0x0, version_check_t),
    /** Set kernel context creation flags. */
    set_flags = _IOWR(iface_number, 0x212, set_flags_t),
    /** Get GPU properties. */
    get_gpuprops = _IOWR(iface_number, 0x20e, uk_gpuprops_t),
};

}

/** Kbase Post R21 ioctl interface. */
namespace kbase_post_r21 {

template <typename value_t>
class pointer64 {
  public:
    /** @return Pointer to the object. */
    value_t* get() const {
        return reinterpret_cast<value_t*>(static_cast<uintptr_t>(value));
    }

    /**
     * Set pointer value.
     *
     * @param ptr   The new pointer value.
     */
    void reset(value_t* ptr) {
        value = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ptr));
    }

  private:
    /** Pointer value as uint64_t. */
    uint64_t value { 0 };
};

/** Check version compatibility between kernel and userspace. */
struct version_check_t {
    /** Major version number. */
    uint16_t major;
    /** Minor version number */
    uint16_t minor;

    bool is_set() const
    {
        return major || minor;
    }
};

/** Set kernel context creation flags. */
struct set_flags_t {
    /** kernel context creation flags. */
    uint32_t create_flags;
};

/**
 * The ioctl will return the number of bytes stored into buffer or an error
 * on failure (e.g. size is too small). If size is specified as 0 then no
 * data will be written but the return value will be the number of bytes needed
 * for all the properties.
 *
 * flags may be used in the future to request a different format for the
 * buffer. With flags == 0 the following format is used.
 *
 * The buffer will be filled with pairs of values, a __u32 key identifying the
 * property followed by the value. The size of the value is identified using
 * the bottom bits of the key. The value then immediately followed the key and
 * is tightly packed (there is no padding). All keys and values are
 * little-endian.
 *
 * 00 = __u8
 * 01 = __u16
 * 10 = __u32
 * 11 = __u64
 */
struct get_gpuprops_t {
    /** GPU property size. */
    enum class gpuprop_size : uint8_t {
        /** Property type is uint8_t. */
        uint8 = 0x0,
        /** Property type is uint16_t. */
        uint16 = 0x1,
        /** Property type is uint32_t. */
        uint32 = 0x2,
        /** Property type is uint64_t. */
        uint64 = 0x3
    };

    /** GPU properties codes. */
    enum class gpuprop_code : uint8_t {
        /** Product id. */
        product_id = 1,
        /** L2 log2 line size. */
        l2_log2_line_size = 13,
        /** L2 log2 cache size. */
        l2_log2_cache_size = 14,
        /** L2 num l2 slices. */
        l2_num_l2_slices = 15,
        /** Max threads. */
        max_threads = 18,
        /** Max registers. */
        max_registers = 21,
        /** Raw l2 features. */
        raw_l2_features = 29,
        /** Raw core features. */
        raw_core_features = 30,
        /** Raw thread max threads. */
        raw_thread_max_threads = 56,
        /** Raw thread max workgroup size. */
        raw_thread_max_workgroup_size = 57,
        /** Raw thread max barrier size. */
        raw_thread_max_barrier_size = 58,
        /** Raw thread features. */
        raw_thread_features = 59,
        /** Raw coherency mode. */
        raw_coherency_mode = 60,
        /** Coherency num groups. */
        coherency_num_groups = 61,
        /** Coherency num core groups. */
        coherency_num_core_groups = 62,
        /** Coherency coherency. */
        coherency_coherency = 63,
        /** Coherency group 0. */
        coherency_group_0 = 64,
        /** Coherency group 1. */
        coherency_group_1 = 65,
        /** Coherency group 2. */
        coherency_group_2 = 66,
        /** Coherency group 3. */
        coherency_group_3 = 67,
        /** Num exec engines. */
        num_exec_engines = 82
    };

    /** Pointer to the buffer to store properties into. */
    pointer64<uint8_t> buffer;

    /** Size of the buffer. */
    uint32_t size;

    /** Flags - must be zero for now. */
    uint32_t flags;
};

constexpr auto iface_number = 0x80;

/** Commands describing kbase ioctl interface. */
enum command_type {
    /** Check version compatibility between JM kernel and userspace. */
    version_check_jm = _IOWR(iface_number, 0x0, version_check_t),
    /** Check version compatibility between CSF kernel and userspace. */
    version_check_csf = _IOWR(iface_number, 0x34, version_check_t),
    /** Set kernel context creation flags. */
    set_flags = _IOW(iface_number, 0x1, set_flags_t),
    /** Get GPU properties. */
    get_gpuprops = _IOW(iface_number, 0x3, get_gpuprops_t),
};

}

class prop_decoder {
  public:
    prop_decoder(std::vector<unsigned char> buffer)
        : buffer_{ std::move(buffer) }
        , data_{ buffer_.data() }
        , size_{ buffer_.size() } {}

    bool decode(gpuinfo& info) {
        bool success = true;

        uint64_t raw_core_features {};
        uint64_t raw_thread_features {};

        while (size_ > 0) {
            auto p = next(success);
            if (!success) {
                return false;
            }

            prop_id_t id = p.first;
            uint64_t value = p.second;

            switch (id) {
            case prop_id_t::product_id:
                info.gpu_id = value;
                break;
            case prop_id_t::l2_log2_cache_size:
                info.num_l2_bytes = 1UL << value;
                break;
            case prop_id_t::l2_num_l2_slices:
                info.num_l2_slices = value;
                break;
            case prop_id_t::raw_l2_features:
                /* log2(bus width) stored in top 8 bits of register. */
                info.num_bus_bits = 1UL << ((value >> 24) & 0xFF);
                break;
            case prop_id_t::raw_core_features:
                raw_core_features = value;
                break;
            case prop_id_t::raw_thread_features:
                raw_thread_features = value;
                break;
            case prop_id_t::coherency_group_0:
            case prop_id_t::coherency_group_1:
            case prop_id_t::coherency_group_2:
            case prop_id_t::coherency_group_3:
                info.num_shader_cores += __builtin_popcount(value);
                break;
            default:
                break;
            }
        }

        info.num_exec_engines = get_num_exec_engines(
            info.gpu_id,
            info.num_shader_cores,
            raw_core_features,
            raw_thread_features);

        if (!info.num_exec_engines) {
            return false;
        }

        info.num_fp32_fmas_per_cy = get_num_fp32_fmas(
            info.gpu_id,
            info.num_shader_cores,
            raw_core_features,
            raw_thread_features);

        info.num_fp16_fmas_per_cy = info.num_fp32_fmas_per_cy * 2;

        info.num_texels_per_cy = get_num_texels(
            info.gpu_id,
            info.num_shader_cores,
            raw_core_features,
            raw_thread_features);

        info.num_pixels_per_cy = get_num_pixels(
            info.gpu_id,
            info.num_shader_cores,
            raw_core_features,
            raw_thread_features);

        return true;
    }

  private:
    /** Property id type. */
    using prop_id_t = kbase_post_r21::get_gpuprops_t::gpuprop_code;
    /** Property size type. */
    using prop_size_t = kbase_post_r21::get_gpuprops_t::gpuprop_size;

    static std::pair<prop_id_t, prop_size_t> to_prop_metadata(uint32_t v)  {
        /* Property id/size encoding is:
         * +--------+----------+
         * | 31   2 | 1      0 |
         * +--------+----------+
         * | PropId | PropSize |
         * +--------+----------+
         */
        static unsigned int id_shift { 2 };
        static unsigned int size_mask { 0b11 };

        return { static_cast<prop_id_t>(v >> id_shift), static_cast<prop_size_t>(v & size_mask) };
    }

    std::pair<prop_id_t, uint64_t> next(bool& success)  {
        success = true;
        auto p = to_prop_metadata(read_bytes<uint32_t>(success));
        if (success)
        {
            prop_id_t id = p.first;
            prop_size_t size = p.second;

            switch (size) {
            case prop_size_t::uint8:
                return { id, read_bytes<uint8_t>(success) };
            case prop_size_t::uint16:
                return { id, read_bytes<uint16_t>(success) };
            case prop_size_t::uint32:
                return { id, read_bytes<uint32_t>(success) };
            case prop_size_t::uint64:
                return { id, read_bytes<uint64_t>(success) };
            }
        }

        return {};
    }

    template <typename T>
    T read_bytes(bool& success)  {
        // Check we have enough bytes in the buffer
        if (size_ < sizeof(T)) {
            success = false;
            return 0;
        }

        T ret {};
        for (size_t b = 0; b < sizeof(T); b++)
        {
            ret |= static_cast<T>(static_cast<uint64_t>(data_[b]) << (8 * b));
        }
        data_ += sizeof(T);
        size_ -= sizeof(T);
        return ret;
    }

    std::vector<unsigned char> const buffer_;
    unsigned char const *data_;
    std::size_t size_;
};

/* See header for documentation */
std::unique_ptr<instance> instance::create(
    const uint32_t id
) {
    std::string device_path("/dev/mali" + std::to_string(id));

    // Open the kernel driver device node
    const int fd = ::open(device_path.c_str(), O_RDONLY);
    if (fd < 0) {
        return nullptr;
    }

    // Check that it is a character device
    struct stat s {};
    const int fs_result = fstat(fd, &s);
    if ((fs_result < 0) || (S_ISCHR(s.st_mode) == 0)) {
        ::close(fd);
        return nullptr;
    }

    // Create the instance
    auto result = std::unique_ptr<instance>(new instance(fd));
    if (!result || !result->valid_) {
        return nullptr;
    }

    return result;
}

/* See header for documentation */
const gpuinfo& instance::get_info() const
{
    return info_;
};

/* See header for documentation */
instance::~instance()
{
    ::close(fd_);
}

/* See header for documentation */
instance::instance(int fd):
    fd_(fd)
{
    if (!check_version()) {
        valid_ = false;
        return;
    }

    if (!set_flags()) {
        valid_ = false;
        return;
    }

    if (!init_props()) {
        valid_ = false;
        return;
    }
}

static bool is_supported(unsigned int major, unsigned int minor)
{
    return (major > 10) || ((major == 10) && (minor >= 2));
}

/* See header for documentation */
bool instance::check_version() {
    // Probe pre-r21 JM kernel
    // Must be first in the list because CSF reuses an old IOCTL ID
    iface_ = iface_type::pre_r21;
    kbase_pre_r21::version_check_t pre_r21 {};
    pre_r21.header.id = kbase_pre_r21::header_id::version_check;
    ::ioctl(fd_, kbase_pre_r21::version_check, &pre_r21);
    // If this is non-zero this must be pre-r21 driver, so check version
    if (pre_r21.is_set()) {
        return is_supported(pre_r21.major, pre_r21.minor);
    }

    // Probe r21+ JM kernel
    iface_ = iface_type::post_r21;
    kbase_post_r21::version_check_t post_r21 {};
    ::ioctl(fd_, kbase_post_r21::version_check_jm, &post_r21);
    // If this is non-zero this must be post-r21 JM driver, so check version
    if (post_r21.is_set()) {
        return is_supported(post_r21.major, post_r21.minor);
    }

    // Probe r21+ CSF kernel
    ::ioctl(fd_, kbase_post_r21::version_check_csf, &post_r21);
    // If this is any non-zero value this is a valid CSF GPU
    return post_r21.is_set();
}

/** Call set flags ioctl. */
bool instance::set_flags() {
    static constexpr auto system_monitor_flag_submit_disabled_bit = 1;
    static constexpr auto system_monitor_flag = 1U << system_monitor_flag_submit_disabled_bit;

    // Clear errno
    errno = 0;

    if (iface_ == iface_type::pre_r21) {
        kbase_pre_r21::set_flags_t flags {};
        flags.header.id = kbase_pre_r21::header_id::set_flags;
        flags.create_flags = system_monitor_flag;
        ::ioctl(fd_, kbase_pre_r21::set_flags, &flags);
    } else {
        kbase_post_r21::set_flags_t flags { system_monitor_flag };
        ::ioctl(fd_, kbase_post_r21::set_flags, &flags);
    }

    // Mali driver will fail if reinitialized, but it's benign
    // TODO: Does this ever happen with this usage pattern
    return errno == 0 || errno == EINVAL || errno == EPERM;
}

/* See header for documentation */
bool instance::init_props() {
    bool success;
    if (iface_ == iface_type::pre_r21) {
        success = init_props_pre_r21();
    } else {
        success = init_props_post_r21();
    }

    // Perform some common cleanup on the data
    if (!success)
    {
        return false;
    }

    info_.num_l2_bytes *= info_.num_l2_slices;
    info_.gpu_name = get_gpu_name(info_.gpu_id, info_.num_shader_cores);
    info_.architecture_name = get_architecture_name(info_.gpu_id);
    info_.gpu_id = get_gpu_id(info_.gpu_id);
    return true;
}

/* See header for documentation */
bool instance::init_props_pre_r21() {
    int error = 0;

    kbase_pre_r21::uk_gpuprops_t props {};
    props.header.id = kbase_pre_r21::header_id::get_props;
    errno = 0;
    ::ioctl(fd_, kbase_pre_r21::get_gpuprops, &props);
    if (errno) {
        return false;
    }

    info_.gpu_id = props.props.core_props.product_id;
    info_.num_l2_bytes = 1UL << props.props.l2_props.log2_cache_size;
    info_.num_l2_slices = props.props.l2_props.num_l2_slices;
    info_.num_bus_bits = 1UL << (props.props.raw_props.l2_features >> 24);

    info_.num_shader_cores = 0;
    for (uint32_t i = 0; i < props.props.coherency_info.num_core_groups; i++)
    {
        info_.num_shader_cores += __builtin_popcount(props.props.coherency_info.group[i].core_mask);
    }

    info_.num_exec_engines = get_num_exec_engines(
        info_.gpu_id,
        info_.num_shader_cores,
        0, 0);

    info_.num_fp32_fmas_per_cy = get_num_fp32_fmas(
        info_.gpu_id,
        info_.num_shader_cores,
        0, 0);

    info_.num_fp16_fmas_per_cy = info_.num_fp32_fmas_per_cy * 2;

    info_.num_texels_per_cy = get_num_texels(
        info_.gpu_id,
        info_.num_shader_cores,
        0, 0);

    info_.num_pixels_per_cy = get_num_pixels(
        info_.gpu_id,
        info_.num_shader_cores,
        0, 0);

    return true;
}

/* See header for documentation */
bool instance::init_props_post_r21() {
    errno = 0;

    kbase_post_r21::get_gpuprops_t get_props = {};
    int size = ::ioctl(fd_, kbase_post_r21::get_gpuprops, &get_props);
    if (errno) {
        return false;
    }

    std::vector<unsigned char> buffer(static_cast<std::size_t>(size));
    get_props.size = static_cast<uint32_t>(size);
    get_props.buffer.reset(buffer.data());
    ::ioctl(fd_, kbase_post_r21::get_gpuprops, &get_props);
    if (errno) {
        return false;
    }

    prop_decoder decoder { buffer };
    return decoder.decode(info_);
}

}
