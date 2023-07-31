project(libunwind)

cmake_minimum_required(VERSION 3.0)

#set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} -Werror -Wno-#pragma-messages -Wno-unused-parameter -Wno-maybe-uninitialized)	
#set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -Wno-old-style-cast)

add_definitions(-DHAVE_CONFIG_H -D_GNU_SOURCE)
enable_language(ASM)
 # set(TARGET_AARCH64 1)
 # set(arch aarch64)
 # add_definitions(-D__aarch64__)
 # add_definitions(-D__linux__)

#include(CheckCSourceCompiles)
#include(CheckIncludeFiles)
#configure_file(../libunwind/include/config.h.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/include/config.h)
#configure_file(../libunwind/include/libunwind-common.h.in ${CMAKE_CURRENT_BINARY_DIR}/include/libunwind-common.h)
#configure_file(../libunwind/include/libunwind.h.in ${CMAKE_CURRENT_BINARY_DIR}/include/libunwind.h)
#configure_file(../libunwind/include/tdep/libunwind_i.h.in ${CMAKE_CURRENT_BINARY_DIR}/include/tdep/libunwind_i.h)

if(CMAKE_BUILD_TYPE EQUAL "RELEASE")
	add_definitions(-DNDEBUG)
endif()

add_compile_definitions(_FORTIFY_SOURCE=2)

set(SOURCE_ROOT
    ${CMAKE_SOURCE_DIR}/../libunwind
)

set(libunwind_includes
    ${SOURCE_ROOT}/src
    ${SOURCE_ROOT}/include
)

if(ANDROID_ABI STREQUAL "arm64-v8a")
    set(libunwind_includes ${libunwind_includes} ${SOURCE_ROOT}/include/tdep-aarch64)
elseif(ANDROID_ABI STREQUAL "x86_64")
    set(libunwind_includes ${libunwind_includes} ${SOURCE_ROOT}/include/tdep-x86_64)
else()
    message(FATAL_ERROR "Unsupported ANDROID_ABI: ${ANDROID_ABI}")
endif()

include_directories(${libunwind_includes}) 

set(libunwind_source
    ${SOURCE_ROOT}/src/mi/backtrace.c
    ${SOURCE_ROOT}/src/mi/dyn-cancel.c
    ${SOURCE_ROOT}/src/mi/dyn-info-list.c
    ${SOURCE_ROOT}/src/mi/dyn-register.c
    ${SOURCE_ROOT}/src/mi/flush_cache.c
    ${SOURCE_ROOT}/src/mi/Gdestroy_addr_space.c
    ${SOURCE_ROOT}/src/mi/Gdyn-extract.c
    ${SOURCE_ROOT}/src/mi/Gdyn-remote.c
    ${SOURCE_ROOT}/src/mi/Gfind_dynamic_proc_info.c
    ${SOURCE_ROOT}/src/mi/Gget_accessors.c
    ${SOURCE_ROOT}/src/mi/Gget_fpreg.c
    ${SOURCE_ROOT}/src/mi/Gget_proc_info_by_ip.c
    ${SOURCE_ROOT}/src/mi/Gget_proc_name.c
    ${SOURCE_ROOT}/src/mi/Gget_reg.c
    ${SOURCE_ROOT}/src/mi/Gput_dynamic_unwind_info.c
    ${SOURCE_ROOT}/src/mi/Gset_cache_size.c
    ${SOURCE_ROOT}/src/mi/Gset_caching_policy.c
    ${SOURCE_ROOT}/src/mi/Gset_fpreg.c
    ${SOURCE_ROOT}/src/mi/Gset_reg.c
    ${SOURCE_ROOT}/src/mi/init.c
    ${SOURCE_ROOT}/src/mi/Ldestroy_addr_space.c
    ${SOURCE_ROOT}/src/mi/Ldyn-extract.c
    #${SOURCE_ROOT}/src/mi/Ldyn-remote.c
    ${SOURCE_ROOT}/src/mi/Lfind_dynamic_proc_info.c
    ${SOURCE_ROOT}/src/mi/Lget_accessors.c
    ${SOURCE_ROOT}/src/mi/Lget_fpreg.c
    ${SOURCE_ROOT}/src/mi/Lget_proc_info_by_ip.c
    ${SOURCE_ROOT}/src/mi/Lget_proc_name.c
    ${SOURCE_ROOT}/src/mi/Lget_reg.c
    ${SOURCE_ROOT}/src/mi/Lput_dynamic_unwind_info.c
    ${SOURCE_ROOT}/src/mi/Lset_cache_size.c
    ${SOURCE_ROOT}/src/mi/Lset_caching_policy.c
    ${SOURCE_ROOT}/src/mi/Lset_fpreg.c
    ${SOURCE_ROOT}/src/mi/Lset_reg.c
    ${SOURCE_ROOT}/src/mi/mempool.c
    ${SOURCE_ROOT}/src/mi/strerror.c
    ${SOURCE_ROOT}/src/mi/_ReadSLEB.c
    ${SOURCE_ROOT}/src/mi/_ReadULEB.c

    ${SOURCE_ROOT}/src/dwarf/Gexpr.c
    ${SOURCE_ROOT}/src/dwarf/Gfde.c
    ${SOURCE_ROOT}/src/dwarf/Gfind_proc_info-lsb.c
    ${SOURCE_ROOT}/src/dwarf/Gfind_unwind_table.c
    ${SOURCE_ROOT}/src/dwarf/global.c
    ${SOURCE_ROOT}/src/dwarf/Gparser.c
    ${SOURCE_ROOT}/src/dwarf/Gpe.c
    ${SOURCE_ROOT}/src/dwarf/Lexpr.c
    ${SOURCE_ROOT}/src/dwarf/Lfde.c
    ${SOURCE_ROOT}/src/dwarf/Lfind_proc_info-lsb.c
    ${SOURCE_ROOT}/src/dwarf/Lfind_unwind_table.c
    ${SOURCE_ROOT}/src/dwarf/Lparser.c
    ${SOURCE_ROOT}/src/dwarf/Lpe.c

     ${SOURCE_ROOT}/src/dl-iterate-phdr.c
     #${SOURCE_ROOT}/src/elf32.c
     #${SOURCE_ROOT}/src/elf64.c
     #${SOURCE_ROOT}/src/elfxx.c
     #${SOURCE_ROOT}/src/os-freebsd.c
     #${SOURCE_ROOT}/src/os-hpux.c
     ${SOURCE_ROOT}/src/os-linux.c
#     ${SOURCE_ROOT}/src/os-qnx.c
     #${SOURCE_ROOT}/src/os-solaris.c

    ${SOURCE_ROOT}/src/ptrace/_UPT_accessors.c
    ${SOURCE_ROOT}/src/ptrace/_UPT_access_fpreg.c
    ${SOURCE_ROOT}/src/ptrace/_UPT_access_mem.c
    ${SOURCE_ROOT}/src/ptrace/_UPT_access_reg.c
    ${SOURCE_ROOT}/src/ptrace/_UPT_create.c
    ${SOURCE_ROOT}/src/ptrace/_UPT_destroy.c
    ${SOURCE_ROOT}/src/ptrace/_UPT_elf.c
    ${SOURCE_ROOT}/src/ptrace/_UPT_find_proc_info.c
    ${SOURCE_ROOT}/src/ptrace/_UPT_get_dyn_info_list_addr.c
    ${SOURCE_ROOT}/src/ptrace/_UPT_get_proc_name.c
    ${SOURCE_ROOT}/src/ptrace/_UPT_put_unwind_info.c
    ${SOURCE_ROOT}/src/ptrace/_UPT_reg_offset.c
    ${SOURCE_ROOT}/src/ptrace/_UPT_resume.c
)

set(libunwind_arm64_source
    ${SOURCE_ROOT}/src/aarch64/Gapply_reg_state.c
    ${SOURCE_ROOT}/src/aarch64/Gcreate_addr_space.c
    ${SOURCE_ROOT}/src/aarch64/gen-offsets.c
    ${SOURCE_ROOT}/src/aarch64/getcontext.S
    ${SOURCE_ROOT}/src/aarch64/Gget_proc_info.c
    ${SOURCE_ROOT}/src/aarch64/Gget_save_loc.c
    ${SOURCE_ROOT}/src/aarch64/Gglobal.c
    ${SOURCE_ROOT}/src/aarch64/Ginit.c
    ${SOURCE_ROOT}/src/aarch64/Ginit_local.c
    ${SOURCE_ROOT}/src/aarch64/Ginit_remote.c
    ${SOURCE_ROOT}/src/aarch64/Gis_signal_frame.c
    ${SOURCE_ROOT}/src/aarch64/Gregs.c
    ${SOURCE_ROOT}/src/aarch64/Greg_states_iterate.c
    ${SOURCE_ROOT}/src/aarch64/Gresume.c
    ${SOURCE_ROOT}/src/aarch64/Gstash_frame.c
    ${SOURCE_ROOT}/src/aarch64/Gstep.c
    ${SOURCE_ROOT}/src/aarch64/Gtrace.c
    ${SOURCE_ROOT}/src/aarch64/init.h
    ${SOURCE_ROOT}/src/aarch64/is_fpreg.c
    ${SOURCE_ROOT}/src/aarch64/Lapply_reg_state.c
    ${SOURCE_ROOT}/src/aarch64/Lcreate_addr_space.c
    ${SOURCE_ROOT}/src/aarch64/Lget_proc_info.c
    ${SOURCE_ROOT}/src/aarch64/Lget_save_loc.c
    ${SOURCE_ROOT}/src/aarch64/Lglobal.c
    ${SOURCE_ROOT}/src/aarch64/Linit.c
    ${SOURCE_ROOT}/src/aarch64/Linit_local.c
    ${SOURCE_ROOT}/src/aarch64/Linit_remote.c
    ${SOURCE_ROOT}/src/aarch64/Lis_signal_frame.c
    ${SOURCE_ROOT}/src/aarch64/Lregs.c
    ${SOURCE_ROOT}/src/aarch64/Lreg_states_iterate.c
    ${SOURCE_ROOT}/src/aarch64/Lresume.c
    ${SOURCE_ROOT}/src/aarch64/Lstash_frame.c
    ${SOURCE_ROOT}/src/aarch64/Lstep.c
    ${SOURCE_ROOT}/src/aarch64/Ltrace.c
    ${SOURCE_ROOT}/src/aarch64/offsets.h
    ${SOURCE_ROOT}/src/aarch64/regname.c
    ${SOURCE_ROOT}/src/aarch64/siglongjmp.S
    ${SOURCE_ROOT}/src/aarch64/unwind_i.h
    ${SOURCE_ROOT}/src/elf64.c
)

set(libunwind_x86_64_source
    ${SOURCE_ROOT}/src/x86_64/is_fpreg.c
    ${SOURCE_ROOT}/src/x86_64/regname.c
    ${SOURCE_ROOT}/src/x86_64/Gcreate_addr_space.c
    ${SOURCE_ROOT}/src/x86_64/Gget_proc_info.c
    ${SOURCE_ROOT}/src/x86_64/Gget_save_loc.c
    ${SOURCE_ROOT}/src/x86_64/Gglobal.c
    ${SOURCE_ROOT}/src/x86_64/Ginit.c
    ${SOURCE_ROOT}/src/x86_64/Ginit_local.c
    ${SOURCE_ROOT}/src/x86_64/Ginit_remote.c
    ${SOURCE_ROOT}/src/x86_64/Gregs.c
    ${SOURCE_ROOT}/src/x86_64/Gresume.c
    ${SOURCE_ROOT}/src/x86_64/Gstep.c
    ${SOURCE_ROOT}/src/x86_64/Lcreate_addr_space.c
    ${SOURCE_ROOT}/src/x86_64/Lget_proc_info.c
    ${SOURCE_ROOT}/src/x86_64/Lget_save_loc.c
    ${SOURCE_ROOT}/src/x86_64/Lglobal.c
    ${SOURCE_ROOT}/src/x86_64/Linit.c
    ${SOURCE_ROOT}/src/x86_64/Linit_local.c
    ${SOURCE_ROOT}/src/x86_64/Linit_remote.c
    ${SOURCE_ROOT}/src/x86_64/Lregs.c
    ${SOURCE_ROOT}/src/x86_64/Lresume.c
    ${SOURCE_ROOT}/src/x86_64/Lstep.c
    ${SOURCE_ROOT}/src/x86_64/getcontext.S
    ${SOURCE_ROOT}/src/x86_64/Gstash_frame.c
    ${SOURCE_ROOT}/src/x86_64/Gtrace.c
    ${SOURCE_ROOT}/src/x86_64/Gos-linux.c
    ${SOURCE_ROOT}/src/x86_64/Lstash_frame.c
    ${SOURCE_ROOT}/src/x86_64/Ltrace.c
    ${SOURCE_ROOT}/src/x86_64/Los-linux.c
    ${SOURCE_ROOT}/src/x86_64/setcontext.S
    ${SOURCE_ROOT}/src/elf64.c
)

if(ANDROID_ABI STREQUAL "arm64-v8a")
    set(libunwind_source ${libunwind_source} ${libunwind_arm64_source})
elseif(ANDROID_ABI STREQUAL "x86_64")
    set(libunwind_source ${libunwind_source} ${libunwind_x86_64_source})
else()
    message(FATAL_ERROR "Unsupported ANDROID_ABI: ${ANDROID_ABI}")
endif()

#add_library(unwind SHARED ${libunwind_source})
add_library(unwind STATIC ${libunwind_source})

set(libunwindbacktrace_source
    ${SOURCE_ROOT}/src/unwind/Backtrace.c
    ${SOURCE_ROOT}/src/unwind/DeleteException.c
    ${SOURCE_ROOT}/src/unwind/FindEnclosingFunction.c
    ${SOURCE_ROOT}/src/unwind/ForcedUnwind.c
    ${SOURCE_ROOT}/src/unwind/GetBSP.c
    ${SOURCE_ROOT}/src/unwind/GetCFA.c
    ${SOURCE_ROOT}/src/unwind/GetDataRelBase.c
    ${SOURCE_ROOT}/src/unwind/GetGR.c
    ${SOURCE_ROOT}/src/unwind/GetIP.c
    ${SOURCE_ROOT}/src/unwind/GetIPInfo.c
    ${SOURCE_ROOT}/src/unwind/GetLanguageSpecificData.c
    ${SOURCE_ROOT}/src/unwind/GetRegionStart.c
    ${SOURCE_ROOT}/src/unwind/GetTextRelBase.c
    ${SOURCE_ROOT}/src/unwind/libunwind.pc.in
    ${SOURCE_ROOT}/src/unwind/RaiseException.c
    ${SOURCE_ROOT}/src/unwind/Resume.c
    ${SOURCE_ROOT}/src/unwind/Resume_or_Rethrow.c
    ${SOURCE_ROOT}/src/unwind/SetGR.c
    ${SOURCE_ROOT}/src/unwind/SetIP.c
)

#set(libunwindbacktrace_cflags
#    -Wno-old-style-declaration
#    -fvisibility=hidden
#)

add_library(unwindbacktrace STATIC ${libunwindbacktrace_source})
#target_compile_definitions(unwindbacktrace ${libunwindbacktrace_cflags})