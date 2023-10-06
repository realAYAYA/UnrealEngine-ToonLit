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

/**
 * @brief The core libGPUInfo library interface.
 *
 * This library provides developers with an easy way to query the Arm
 * Immortalis or Arm Mali GPU configuration in their system.  This information
 * can be used to adjust rendering workload to match the capabilities of the
 * device.
 *
 * The library is simple to use:
 *
 *     // Create a connection with the kernel driver ...
 *     std::unique_ptr<instance> conn = libgpuinfo::instance::create();
 *     if (!conn)
 *     {
 *         std::cout << "ERROR: Failed to create Mali instance\n";
 *         return;
 *     }
 *
 *     // Fetch the information result and do something with it ...
 *     const gpuinfo& info = conn->get_info();
 *     std::cout << "GPU: " << info.gpu_name << " MP" << info.num_shader_cores << "\n";
 *
 * Note that the returned information object is returned by reference, and has
 * the same lifetime as the instance object.
 */

#pragma once

#include <array>
#include <cerrno>
#include <cstdint>
#include <vector>
#include <string>
#include <memory>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace libgpuinfo {

/** Mali GPU information. */
struct gpuinfo
{
    /** GPU name */
    const char* gpu_name;

    /** GPU architecture name */
    const char* architecture_name;

    /** GPU ID */
    uint32_t gpu_id;

    /** Number of shader cores */
    uint32_t num_shader_cores;

    /** Number of L2 cache slices */
    uint32_t num_l2_slices;

    /** L2 cache size, summed for all slices, in bytes */
    uint32_t num_l2_bytes;

    /** GPU external bus width per cache slice, in bits */
    uint32_t num_bus_bits;

    /** Number of execution engines per core */
    uint32_t num_exec_engines;

    /** Maximum number of 32-bit floating-point FMAs per clock per core */
    uint32_t num_fp32_fmas_per_cy;

    /** Maximum number of 16-bit floating-point FMAs per clock per core */
    uint32_t num_fp16_fmas_per_cy;

    /** Maximum number of bilinear filtered texels per clock per core */
    uint32_t num_texels_per_cy;

    /** Maximum number of output pixels per clock per core */
    uint32_t num_pixels_per_cy;
};


/** Kbase ioctl interface type. */
enum class iface_type {
    /** Pre R21 kernel */
    pre_r21,
    /** Post R21 kernel (inclusive) */
    post_r21
};

/**
 * Mali device driver instance.
 */
class instance
{
public:
    /**
     * Factory function to create a device instance.
     *
     * @param id   The driver instance, e.g. 0 for /dev/mali0.
     *
     * @return The created instance, or @c nullptr on failure.
     */
    static std::unique_ptr<instance> create(const uint32_t id=0);

    /**
     * Get the GPU device property information.
     *
     * The returned reference has the same lifetime as the instance.
     *
     * @return The device property information.
     */
    const gpuinfo& get_info() const;

    /**
     * Destroy an instance.
     *
     * Any returned information references become invalid.
     */
    ~instance();

private:
    /**
     * Create a new instance.
     *
     * @param fd   The opened driver file descriptor.
     *
     */
    instance(int fd);

    /** Check the Mali kernel driver interface version. */
    bool check_version();

    /** Configure Mali kernel driver connection flags. */
    bool set_flags();

    /** Query properties and store them locally. */
    bool init_props();

    /** Get device constants from the old format ioctl. */
    bool init_props_pre_r21();

    /** Get device constants from the new format ioctl. */
    bool init_props_post_r21();

    /** The queries device properties. */
    gpuinfo info_ {};

    /** The driver interface type. */
    iface_type iface_ {};

    /** The validity state of the object if initialization fails. */
    bool valid_ { true };

    /** The kernel driver file descriptor. */
    int fd_ {};
};

}
