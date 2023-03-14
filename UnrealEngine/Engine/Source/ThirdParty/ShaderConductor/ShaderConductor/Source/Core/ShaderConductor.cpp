/*
 * ShaderConductor
 *
 * Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the MIT License.
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons
 * to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <ShaderConductor/ShaderConductor.hpp>

#include <dxc/Support/Global.h>
#include <dxc/Support/Unicode.h>
#include <dxc/Support/WinAdapter.h>
#include <dxc/Support/WinIncludes.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <fstream>
#include <memory>
// UE Change Begin: Allow remapping of variables in glsl
#include <sstream>
// UE Change End: Allow remapping of variables in glsl

#include <dxc/DxilContainer/DxilContainer.h>
#include <dxc/dxcapi.h>
// UE Change Begin: Add functionality to rewrite HLSL to remove unused code and globals.
#include <dxc/dxctools.h>
// UE Change End: Add functionality to rewrite HLSL to remove unused code and globals.
#include <llvm/Support/ErrorHandling.h>

// UE Change Begin: Allow optimization after source-to-spirv conversion and before spirv-to-source cross-compilation
#include <spirv-tools/optimizer.hpp>
// UE Change End: Allow optimization after source-to-spirv conversion and before spirv-to-source cross-compilation

#include <spirv-tools/libspirv.h>
#include <spirv.hpp>
#include <spirv_cross.hpp>
#include <spirv_glsl.hpp>
#include <spirv_hlsl.hpp>
#include <spirv_msl.hpp>
#include <spirv_cross_util.hpp>

#ifdef LLVM_ON_WIN32
#include <d3d12shader.h>
#endif

#define SC_UNUSED(x) (void)(x);

using namespace ShaderConductor;

// UE Change Begin: Clean up parameter parsing
static bool ParseSpirvCrossOption(const ShaderConductor::MacroDefine& define, const char* name, uint32_t& outValue)
{
    if (::strcmp(define.name, name) == 0)
    {
        outValue = static_cast<uint32_t>(std::stoi(define.value));
        return true;
    }
    return false;
}

static bool ParseSpirvCrossOption(const ShaderConductor::MacroDefine& define, const char* name, bool& outValue)
{
    if (::strcmp(define.name, name) == 0)
    {
        outValue = (std::stoi(define.value) != 0);
        return true;
    }
    return false;
}

#define PARSE_SPIRVCROSS_OPTION(DEFINE, NAME, VALUE)    \
    if (ParseSpirvCrossOption(DEFINE, NAME, VALUE))     \
    {                                                   \
        return true;                                    \
    }

// These options are shared between GLSL, HLSL, and Metal compilers
static bool ParseSpirvCrossOptionCommon(spirv_cross::CompilerGLSL::Options& opt, const ShaderConductor::MacroDefine& define)
{
    PARSE_SPIRVCROSS_OPTION(define, "reconstruct_global_uniforms", opt.reconstruct_global_uniforms);
	PARSE_SPIRVCROSS_OPTION(define, "force_zero_initialized_variables", opt.force_zero_initialized_variables);
    return false;
}

static bool ParseSpirvCrossOptionGlsl(spirv_cross::CompilerGLSL::Options& opt, const ShaderConductor::MacroDefine& define)
{
    PARSE_SPIRVCROSS_OPTION(define, "emit_push_constant_as_uniform_buffer", opt.emit_push_constant_as_uniform_buffer);
    PARSE_SPIRVCROSS_OPTION(define, "emit_uniform_buffer_as_plain_uniforms", opt.emit_uniform_buffer_as_plain_uniforms);
    PARSE_SPIRVCROSS_OPTION(define, "flatten_multidimensional_arrays", opt.flatten_multidimensional_arrays);
    PARSE_SPIRVCROSS_OPTION(define, "force_flattened_io_blocks", opt.force_flattened_io_blocks);
    PARSE_SPIRVCROSS_OPTION(define, "emit_ssbo_alias_type_name", opt.emit_ssbo_alias_type_name);
    PARSE_SPIRVCROSS_OPTION(define, "separate_texture_types", opt.separate_texture_types);
    PARSE_SPIRVCROSS_OPTION(define, "disable_ssbo_block_layout", opt.disable_ssbo_block_layout);
    PARSE_SPIRVCROSS_OPTION(define, "force_ubo_std140_layout", opt.force_ubo_std140_layout);
    PARSE_SPIRVCROSS_OPTION(define, "disable_explicit_binding", opt.disable_explicit_binding);
    PARSE_SPIRVCROSS_OPTION(define, "enable_texture_buffer", opt.enable_texture_buffer);
	PARSE_SPIRVCROSS_OPTION(define, "ovr_multiview_view_count", opt.ovr_multiview_view_count);
	PARSE_SPIRVCROSS_OPTION(define, "pad_ubo_blocks", opt.pad_ubo_blocks);
	PARSE_SPIRVCROSS_OPTION(define, "force_temporary", opt.force_temporary);
	PARSE_SPIRVCROSS_OPTION(define, "force_glsl_clipspace", opt.force_glsl_clipspace);
    return false;
}

static bool ParseSpirvCrossOptionHlsl(spirv_cross::CompilerHLSL::Options& opt, const ShaderConductor::MacroDefine& define)
{
    PARSE_SPIRVCROSS_OPTION(define, "reconstruct_semantics", opt.reconstruct_semantics);
    PARSE_SPIRVCROSS_OPTION(define, "reconstruct_cbuffer_names", opt.reconstruct_cbuffer_names);
    PARSE_SPIRVCROSS_OPTION(define, "implicit_resource_binding", opt.implicit_resource_binding);
    return false;
}

static bool ParseSpirvCrossOptionMetal(spirv_cross::CompilerMSL::Options& opt, const ShaderConductor::MacroDefine& define)
{
    PARSE_SPIRVCROSS_OPTION(define, "ios_support_base_vertex_instance", opt.ios_support_base_vertex_instance);
    PARSE_SPIRVCROSS_OPTION(define, "swizzle_texture_samples", opt.swizzle_texture_samples);
    PARSE_SPIRVCROSS_OPTION(define, "texel_buffer_texture_width", opt.texel_buffer_texture_width);
    // Use Metal's native texture-buffer type for HLSL buffers.
    PARSE_SPIRVCROSS_OPTION(define, "texture_buffer_native", opt.texture_buffer_native);
    // Use Metal's native frame-buffer fetch API for subpass inputs.
    PARSE_SPIRVCROSS_OPTION(define, "use_framebuffer_fetch_subpasses", opt.use_framebuffer_fetch_subpasses);
    // Storage buffer robustness - clamps access to SSBOs to the size of the buffer.
    PARSE_SPIRVCROSS_OPTION(define, "enforce_storge_buffer_bounds", opt.enforce_storge_buffer_bounds);
    PARSE_SPIRVCROSS_OPTION(define, "buffer_size_buffer_index", opt.buffer_size_buffer_index);
    // Capture shader output to a buffer - used for vertex streaming to emulate GS & Tess.
    PARSE_SPIRVCROSS_OPTION(define, "capture_output_to_buffer", opt.capture_output_to_buffer);
    PARSE_SPIRVCROSS_OPTION(define, "shader_output_buffer_index", opt.shader_output_buffer_index);
    // Allow the caller to specify the various auxiliary Metal buffer indices.
    PARSE_SPIRVCROSS_OPTION(define, "indirect_params_buffer_index", opt.indirect_params_buffer_index);
    PARSE_SPIRVCROSS_OPTION(define, "shader_patch_output_buffer_index", opt.shader_patch_output_buffer_index);
    PARSE_SPIRVCROSS_OPTION(define, "shader_tess_factor_buffer_index", opt.shader_tess_factor_buffer_index);
    PARSE_SPIRVCROSS_OPTION(define, "shader_input_wg_index", opt.shader_input_wg_index);
    // Allow the caller to specify the Metal translation should use argument buffers.
    PARSE_SPIRVCROSS_OPTION(define, "argument_buffers", opt.argument_buffers);
    //PARSE_SPIRVCROSS_OPTION(define, "argument_buffer_offset", opt.argument_buffer_offset);
    PARSE_SPIRVCROSS_OPTION(define, "invariant_float_math", opt.invariant_float_math);
    // Emulate texturecube_array with texture2d_array for iOS where this type is not available.
    PARSE_SPIRVCROSS_OPTION(define, "emulate_cube_array", opt.emulate_cube_array);
    // Allow user to enable decoration binding.
    PARSE_SPIRVCROSS_OPTION(define, "enable_decoration_binding", opt.enable_decoration_binding);

    // Specify dimension of subpass input attachments.
    static const char* subpassInputDimIdent = "subpass_input_dimension";
    static const size_t subpassInputDimIdentLen = std::strlen(subpassInputDimIdent);
    if (!strncmp(define.name, subpassInputDimIdent, subpassInputDimIdentLen))
    {
        int binding = std::stoi(define.name + subpassInputDimIdentLen);
        opt.subpass_input_dimensions[static_cast<uint32_t>(binding)] = std::stoi(define.value);
    }

    return false;
}
// UE Change End: Clean up parameter parsing


// UE Change Begin: Improved support for PLS and FBF
struct Remap
{
	std::string src_name;
	std::string dst_name;
	unsigned components;
};

static bool remap_generic(spirv_cross::Compiler& compiler, const spirv_cross::SmallVector<spirv_cross::Resource>& resources,
	const Remap& remap)
{
	auto itr = std::find_if(std::begin(resources), std::end(resources),
		[&remap](const spirv_cross::Resource& res) { return res.name == remap.src_name; });

	if (itr != std::end(resources))
	{
		compiler.set_remapped_variable_state(itr->id, true);
		compiler.set_name(itr->id, remap.dst_name);
		compiler.set_subpass_input_remapped_components(itr->id, remap.components);
		return true;
	}
	else
		return false;
}

static void remap(spirv_cross::Compiler& compiler, const spirv_cross::ShaderResources& res, const std::vector<Remap>& remaps)
{
	for (auto& remap : remaps)
	{
		if (remap_generic(compiler, res.stage_inputs, remap))
			return;
		if (remap_generic(compiler, res.stage_outputs, remap))
			return;
		if (remap_generic(compiler, res.subpass_inputs, remap))
			return;
	}
}
// UE Change End: Allow remapping of variables in glsl

struct PLSInOutArg
{
	spirv_cross::PlsFormat format;
	std::string input_name;
	std::string output_name;
};

struct PLSArg
{
	spirv_cross::PlsFormat format;
	std::string name;
};

static spirv_cross::PlsFormat pls_format(const char* str)
{
	if (!strcmp(str, "r11f_g11f_b10f"))
		return spirv_cross::PlsR11FG11FB10F;
	else if (!strcmp(str, "r32f"))
		return spirv_cross::PlsR32F;
	else if (!strcmp(str, "rg16f"))
		return spirv_cross::PlsRG16F;
	else if (!strcmp(str, "rg16"))
		return spirv_cross::PlsRG16;
	else if (!strcmp(str, "rgb10_a2"))
		return spirv_cross::PlsRGB10A2;
	else if (!strcmp(str, "rgba8"))
		return spirv_cross::PlsRGBA8;
	else if (!strcmp(str, "rgba8i"))
		return spirv_cross::PlsRGBA8I;
	else if (!strcmp(str, "rgba8ui"))
		return spirv_cross::PlsRGBA8UI;
	else if (!strcmp(str, "rg16i"))
		return spirv_cross::PlsRG16I;
	else if (!strcmp(str, "rgb10_a2ui"))
		return spirv_cross::PlsRGB10A2UI;
	else if (!strcmp(str, "rg16ui"))
		return spirv_cross::PlsRG16UI;
	else if (!strcmp(str, "r32ui"))
		return spirv_cross::PlsR32UI;
	else
		return spirv_cross::PlsNone;
}

bool FindVariableID(const std::string& name, spirv_cross::ID& id, spirv_cross::SmallVector<spirv_cross::Resource>& resources, const spirv_cross::SmallVector<spirv_cross::Resource>* secondary_resources)
{
	bool found = false;
	for (auto& res : resources)
	{
		if (res.name == name)
		{
			id = res.id;
			found = true;
			break;
		}
	}

	if (!found && secondary_resources)
	{
		for (auto& res : *secondary_resources)
		{
			if (res.name == name)
			{
				id = res.id;
				found = true;
				break;
			}
		}
	}

	if (!found)
	{
		id = UINT32_MAX;
	}

	return found;
}

static std::vector<spirv_cross::PlsRemap> remap_pls(const std::vector<PLSArg>& pls_variables, spirv_cross::SmallVector<spirv_cross::Resource>& resources,
									const spirv_cross::SmallVector<spirv_cross::Resource>* secondary_resources)
{
	std::vector<spirv_cross::PlsRemap> ret;

	for (auto& pls : pls_variables)
	{
		spirv_cross::ID id;
		FindVariableID(pls.name, id, resources, secondary_resources);
		ret.push_back({ id, pls.name, pls.format });
	}

	return ret;
}

static std::vector<spirv_cross::PlsInOutRemap> remap_pls_inout(spirv_cross::Compiler& compiler, const std::vector<PLSInOutArg>& pls_variables, spirv_cross::SmallVector<spirv_cross::Resource>& resources, spirv_cross::SmallVector<spirv_cross::Resource>& secondary_resources)
{
	std::vector<spirv_cross::PlsInOutRemap> ret;

	for (auto& pls : pls_variables)
	{
		std::vector<Remap> Remaps;
		Remap remap = { pls.output_name, pls.input_name, spirv_cross::CompilerGLSL::pls_format_to_components(pls.format) };
		remap_generic(compiler, resources, remap);
		remap_generic(compiler, secondary_resources, remap);

		//find input id
		spirv_cross::ID input_id;
		FindVariableID(pls.input_name, input_id, resources, &secondary_resources);

		//find output id
		spirv_cross::ID output_id;
		FindVariableID(pls.output_name, output_id, resources, &secondary_resources);

		ret.push_back({ input_id, pls.input_name, output_id, pls.output_name, pls.format });
	}

	return ret;
}

static bool GatherPLSRemaps(std::vector<PLSArg>& PLSInputs, std::vector<PLSArg>& PLSOutputs, std::vector<PLSInOutArg>& PLSInOuts, const ShaderConductor::MacroDefine& define)
{
	static const char* PLSDelim = " ";

	static const char* PLSInIdent = "pls_in";
	static const size_t PLSInIdentLen = std::strlen(PLSInIdent);

	if (!strncmp(define.name, PLSInIdent, PLSInIdentLen))
	{
		std::string Value = define.value;

		size_t Offset = Value.find(PLSDelim, 0);

		if (Offset != std::string::npos)
		{
			PLSInputs.push_back({ pls_format(Value.substr(0, Offset).c_str()), Value.substr(Offset + 1) });
		}

		return true;
	}

	static const char* PLSOutIdent = "pls_out";
	static const size_t PLSOuIdentLen = std::strlen(PLSOutIdent);

	if (!strncmp(define.name, PLSOutIdent, PLSOuIdentLen))
	{
		std::string Value = define.value;

		size_t Offset = Value.find(PLSDelim, 0);

		if (Offset != std::string::npos)
		{
			PLSOutputs.push_back({ pls_format(Value.substr(0, Offset).c_str()), Value.substr(Offset + 1) });
		}

		return true;
	}

	static const char* PLSInOutIdent = "pls_io";
	static const size_t PLSInOutIdentLen = std::strlen(PLSInOutIdent);

	if (!strncmp(define.name, PLSInOutIdent, PLSInOutIdentLen))
	{
		std::string Value = define.value;

		size_t OffsetFirst = Value.find_first_of(PLSDelim, 0);
		size_t OffsetLast = Value.find_last_of(PLSDelim);

		if (OffsetFirst != std::string::npos && OffsetLast != std::string::npos && OffsetLast != OffsetFirst)
		{
			PLSInOuts.push_back({ pls_format(Value.substr(0, OffsetFirst).c_str()), Value.substr(OffsetFirst + 1, OffsetLast - (OffsetFirst + 1)), Value.substr(OffsetLast + 1) });
		}

		return true;
	}

	return false;
}

struct FBFArg
{
	int32_t input_index;
	int32_t color_attachment;
};

static bool GatherFBFRemaps(std::vector<FBFArg>& FBFArgs, const ShaderConductor::MacroDefine& define)
{
	static const char* FBFDelim = " ";

	static const char* FBFIdent = "remap_ext_framebuffer_fetch";
	static const size_t FBFIdentLen = std::strlen(FBFIdent);

	if (!strncmp(define.name, FBFIdent, FBFIdentLen))
	{
		std::string Value = define.value;

		size_t Offset = Value.find(FBFDelim, 0);

		if (Offset != std::string::npos)
		{
			FBFArgs.push_back({ std::atoi(Value.substr(0, Offset).c_str()), std::atoi(Value.substr(Offset + 1).c_str()) });
		}

		return true;
	}

	return false;
}
// UE Change End: Improved support for PLS and FBF

namespace
{
    bool dllDetaching = false;

    class Dxcompiler
    {
    public:
        ~Dxcompiler()
        {
            this->Destroy();
        }

        static Dxcompiler& Instance()
        {
            static Dxcompiler instance;
            return instance;
        }

        IDxcLibrary* Library() const
        {
            return m_library;
        }

        IDxcCompiler* Compiler() const
        {
            return m_compiler;
        }

        // UE Change Begin: Add functionality to rewrite HLSL to remove unused code and globals.
        IDxcRewriter* Rewriter() const
        {
            return m_rewriter;
        }
        // UE Change End: Add functionality to rewrite HLSL to remove unused code and globals.

        IDxcContainerReflection* ContainerReflection() const
        {
            return m_containerReflection;
        }

        CComPtr<IDxcLinker> CreateLinker() const
        {
            CComPtr<IDxcLinker> linker;
            IFT(m_createInstanceFunc(CLSID_DxcLinker, __uuidof(IDxcLinker), reinterpret_cast<void**>(&linker)));
            return linker;
        }

        bool LinkerSupport() const
        {
            return m_linkerSupport;
        }

        void Destroy()
        {
            if (m_dxcompilerDll)
            {
                m_compiler = nullptr;
                m_library = nullptr;
                m_containerReflection = nullptr;

                m_createInstanceFunc = nullptr;

#ifdef _WIN32
                ::FreeLibrary(m_dxcompilerDll);
#else
                ::dlclose(m_dxcompilerDll);
#endif

                m_dxcompilerDll = nullptr;
            }
        }

        void Terminate()
        {
            if (m_dxcompilerDll)
            {
                m_compiler.Detach();
                m_library.Detach();
                m_containerReflection.Detach();
                // UE Change Begin: Add functionality to rewrite HLSL to remove unused code and globals.
                m_rewriter.Detach();
                // UE Change End: Add functionality to rewrite HLSL to remove unused code and globals.

                m_createInstanceFunc = nullptr;

                m_dxcompilerDll = nullptr;
            }
        }

		// UE Change Begin: Allow to manually shutdown compiler to avoid dangling mutex on Linux.
#if defined(SC_EXPLICIT_DLLSHUTDOWN)
		void Shutdown()
		{
			if (m_dllShutdownFunc)
			{
				m_dllShutdownFunc();
				m_dllShutdownFunc = nullptr;
			}
		}
#endif
		// UE Change End: Allow to manually shutdown compiler to avoid dangling mutex on Linux.

    private:
        Dxcompiler()
        {
            if (dllDetaching)
            {
                return;
            }

#ifdef _WIN32
            const char* dllName = "dxcompiler.dll";
#elif __APPLE__
            const char* dllName = "libdxcompiler.dylib";
#else
            const char* dllName = "libdxcompiler.so";
#endif
            const char* functionName = "DxcCreateInstance";

#ifdef _WIN32
            m_dxcompilerDll = ::LoadLibraryA(dllName);
#else
            m_dxcompilerDll = ::dlopen(dllName, RTLD_LAZY);
// UE Change Begin: Unreal Engine uses rpaths on Mac for loading dylibs, so "@rpath/" needs to be added before the name of the dylib, so that macOS can find the file
#if __APPLE__
            if (m_dxcompilerDll == nullptr)
            {
                m_dxcompilerDll = ::dlopen((std::string("@rpath/") + dllName).c_str(), RTLD_LAZY);
            }
#endif
// UE Change End: Unreal Engine uses rpaths on Mac for loading dylibs, so "@rpath/" needs to be added before the name of the dylib, so that macOS can find the file
#endif

            if (m_dxcompilerDll != nullptr)
            {
#ifdef _WIN32
                m_createInstanceFunc = (DxcCreateInstanceProc)::GetProcAddress(m_dxcompilerDll, functionName);
#else
                m_createInstanceFunc = (DxcCreateInstanceProc)::dlsym(m_dxcompilerDll, functionName);
#if defined(SC_EXPLICIT_DLLSHUTDOWN)
				m_dllShutdownFunc = (DxcDllShutdownProc)::dlsym(m_dxcompilerDll, "DllShutdown");
#endif
#endif

                if (m_createInstanceFunc != nullptr)
                {
                    IFT(m_createInstanceFunc(CLSID_DxcLibrary, __uuidof(IDxcLibrary), reinterpret_cast<void**>(&m_library)));
                    IFT(m_createInstanceFunc(CLSID_DxcCompiler, __uuidof(IDxcCompiler), reinterpret_cast<void**>(&m_compiler)));
#ifdef _WIN32
                    IFT(m_createInstanceFunc(CLSID_DxcContainerReflection, __uuidof(IDxcContainerReflection),
                                             reinterpret_cast<void**>(&m_containerReflection)));
#endif
                    // UE Change Begin: Add functionality to rewrite HLSL to remove unused code and globals.
                    IFT(m_createInstanceFunc(CLSID_DxcRewriter, __uuidof(IDxcRewriter), reinterpret_cast<void**>(&m_rewriter)));
                    // UE Change End: Add functionality to rewrite HLSL to remove unused code and globals.
                }
                else
                {
                    this->Destroy();

                    throw std::runtime_error(std::string("COULDN'T get ") + functionName + " from dxcompiler.");
                }
            }
            else
            {
                throw std::runtime_error("COULDN'T load dxcompiler.");
            }

#ifdef _WIN32
            m_linkerSupport = (CreateLinker() != nullptr);
#else
			m_linkerSupport = false;
#endif
        }

    private:
        HMODULE m_dxcompilerDll = nullptr;
        DxcCreateInstanceProc m_createInstanceFunc = nullptr;

        CComPtr<IDxcLibrary> m_library;
        CComPtr<IDxcCompiler> m_compiler;
        CComPtr<IDxcContainerReflection> m_containerReflection;
        // UE Change Begin: Add functionality to rewrite HLSL to remove unused code and globals.
        CComPtr<IDxcRewriter> m_rewriter;
        // UE Change End: Add functionality to rewrite HLSL to remove unused code and globals.

		// UE Change Begin: Allow to manually shutdown compiler to avoid dangling mutex on Linux.
#if defined(SC_EXPLICIT_DLLSHUTDOWN)
		typedef void(*DxcDllShutdownProc)();
		DxcDllShutdownProc m_dllShutdownFunc = nullptr;
#endif
		// UE Change End: Allow to manually shutdown compiler to avoid dangling mutex on Linux.

        bool m_linkerSupport;
    };

    class ScIncludeHandler : public IDxcIncludeHandler
    {
    public:
        explicit ScIncludeHandler(std::function<Blob(const char* includeName)> loadCallback) : m_loadCallback(std::move(loadCallback))
        {
        }

        HRESULT STDMETHODCALLTYPE LoadSource(LPCWSTR fileName, IDxcBlob** includeSource) override
        {
            if ((fileName[0] == L'.') && (fileName[1] == L'/'))
            {
                fileName += 2;
            }

            std::string utf8FileName;
            if (!Unicode::WideToUTF8String(fileName, &utf8FileName))
            {
                return E_FAIL;
            }

            Blob source;
            try
            {
                source = m_loadCallback(utf8FileName.c_str());
            }
            catch (...)
            {
                return E_FAIL;
            }

            *includeSource = nullptr;
            return Dxcompiler::Instance().Library()->CreateBlobWithEncodingOnHeapCopy(source.Data(), source.Size(), CP_UTF8,
                                                                                      reinterpret_cast<IDxcBlobEncoding**>(includeSource));
        }

        ULONG STDMETHODCALLTYPE AddRef() override
        {
            ++m_ref;
            return m_ref;
        }

        ULONG STDMETHODCALLTYPE Release() override
        {
            --m_ref;
            ULONG result = m_ref;
            if (result == 0)
            {
                delete this;
            }
            return result;
        }

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override
        {
            if (IsEqualIID(iid, __uuidof(IDxcIncludeHandler)))
            {
                *object = dynamic_cast<IDxcIncludeHandler*>(this);
                this->AddRef();
                return S_OK;
            }
            else if (IsEqualIID(iid, __uuidof(IUnknown)))
            {
                *object = dynamic_cast<IUnknown*>(this);
                this->AddRef();
                return S_OK;
            }
            else
            {
                return E_NOINTERFACE;
            }
        }

    private:
        std::function<Blob(const char* includeName)> m_loadCallback;

        std::atomic<ULONG> m_ref = 0;
    };

    Blob DefaultLoadCallback(const char* includeName)
    {
        std::vector<char> ret;
        std::ifstream includeFile(includeName, std::ios_base::in);
        if (includeFile)
        {
            includeFile.seekg(0, std::ios::end);
            ret.resize(static_cast<size_t>(includeFile.tellg()));
            includeFile.seekg(0, std::ios::beg);
            includeFile.read(ret.data(), ret.size());
            ret.resize(static_cast<size_t>(includeFile.gcount()));
        }
        else
        {
            throw std::runtime_error(std::string("COULDN'T load included file ") + includeName + ".");
        }
        return Blob(ret.data(), static_cast<uint32_t>(ret.size()));
    }

    void AppendError(Compiler::ResultDesc& result, const std::string& msg)
    {
        std::string errorMSg;
        if (result.errorWarningMsg.Size() != 0)
        {
            errorMSg.assign(reinterpret_cast<const char*>(result.errorWarningMsg.Data()), result.errorWarningMsg.Size());
        }
        if (!errorMSg.empty())
        {
            errorMSg += "\n";
        }
        errorMSg += msg;
        result.errorWarningMsg.Reset(errorMSg.data(), static_cast<uint32_t>(errorMSg.size()));
        result.hasError = true;
    }

    // UE Change Begin: Add functionality to rewrite HLSL to remove unused code and globals.
    Compiler::ResultDesc RewriteHlsl(const Compiler::SourceDesc& source, const Compiler::Options& options)
    {
        CComPtr<IDxcBlobEncoding> sourceBlob;
        IFT(Dxcompiler::Instance().Library()->CreateBlobWithEncodingOnHeapCopy(source.source, static_cast<UINT32>(strlen(source.source)),
                                                                               CP_UTF8, &sourceBlob));
        IFTARG(sourceBlob->GetBufferSize() >= 4);

        std::wstring shaderNameUtf16;
        Unicode::UTF8ToWideString(source.fileName, &shaderNameUtf16);

        std::wstring entryPointUtf16;
        Unicode::UTF8ToWideString(source.entryPoint, &entryPointUtf16);

        std::vector<DxcDefine> dxcDefines;
        std::vector<std::wstring> dxcDefineStrings;
        // Need to reserve capacity so that small-string optimization does not
        // invalidate the pointers to internal string data while resizing.
        dxcDefineStrings.reserve(source.numDefines * 2);
        for (size_t i = 0; i < source.numDefines; ++i)
        {
            const auto& define = source.defines[i];

            std::wstring nameUtf16Str;
            Unicode::UTF8ToWideString(define.name, &nameUtf16Str);
            dxcDefineStrings.emplace_back(std::move(nameUtf16Str));
            const wchar_t* nameUtf16 = dxcDefineStrings.back().c_str();

            const wchar_t* valueUtf16;
            if (define.value != nullptr)
            {
                std::wstring valueUtf16Str;
                Unicode::UTF8ToWideString(define.value, &valueUtf16Str);
                dxcDefineStrings.emplace_back(std::move(valueUtf16Str));
                valueUtf16 = dxcDefineStrings.back().c_str();
            }
            else
            {
                valueUtf16 = nullptr;
            }

            dxcDefines.push_back({nameUtf16, valueUtf16});
        }

        CComPtr<IDxcOperationResult> rewriteResult;
        CComPtr<IDxcIncludeHandler> includeHandler = new ScIncludeHandler(std::move(source.loadIncludeCallback));
        IFT(Dxcompiler::Instance().Rewriter()->RewriteUnchangedWithInclude(sourceBlob, shaderNameUtf16.c_str(), dxcDefines.data(),
                                                                           static_cast<UINT32>(dxcDefines.size()), options.DXCArgs,
                                                                           options.numDXCArgs, includeHandler, 0, &rewriteResult));

        HRESULT statusRewrite;
        IFT(rewriteResult->GetStatus(&statusRewrite));

        Compiler::ResultDesc ret = {};
        ret.isText = true;
        ret.hasError = true;

        if (SUCCEEDED(statusRewrite))
        {
            CComPtr<IDxcBlobEncoding> rewritten;

            CComPtr<IDxcBlobEncoding> temp;
            IFT(rewriteResult->GetResult((IDxcBlob**)&temp));

            if (options.removeUnusedGlobals)
            {
                CComPtr<IDxcOperationResult> removeUnusedGlobalsResult;
                IFT(Dxcompiler::Instance().Rewriter()->RemoveUnusedGlobals(temp, entryPointUtf16.c_str(), dxcDefines.data(),
                                                                           static_cast<UINT32>(dxcDefines.size()), options.DXCArgs,
                                                                           options.numDXCArgs, &removeUnusedGlobalsResult));
                IFT(removeUnusedGlobalsResult->GetStatus(&statusRewrite));

                if (SUCCEEDED(statusRewrite))
                {
                    IFT(removeUnusedGlobalsResult->GetResult((IDxcBlob**)&rewritten));
                    ret.hasError = false;
                    ret.target.Reset(rewritten->GetBufferPointer(), static_cast<uint32_t>(rewritten->GetBufferSize()));
                }
                else
                {
                    CComPtr<IDxcBlobEncoding> errorMsg;
                    IFT(removeUnusedGlobalsResult->GetErrorBuffer((IDxcBlobEncoding**)&errorMsg));
                    ret.errorWarningMsg.Reset(errorMsg->GetBufferPointer(), static_cast<uint32_t>(errorMsg->GetBufferSize()));
                }
            }
            else
            {
                IFT(rewriteResult->GetResult((IDxcBlob**)&rewritten));
                ret.hasError = false;
                ret.target.Reset(rewritten->GetBufferPointer(), static_cast<uint32_t>(rewritten->GetBufferSize()));
            }
        }
        else
        {
            CComPtr<IDxcBlobEncoding> errorMsg;
            IFT(rewriteResult->GetErrorBuffer((IDxcBlobEncoding**)&errorMsg));
            ret.errorWarningMsg.Reset(errorMsg->GetBufferPointer(), static_cast<uint32_t>(errorMsg->GetBufferSize()));
        }

        return ret;
    }
    // UE Change End: Add functionality to rewrite HLSL to remove unused code and globals.

#ifdef LLVM_ON_WIN32
    template <typename T>
    HRESULT CreateDxcReflectionFromBlob(IDxcBlob* dxilBlob, CComPtr<T>& outReflection)
    {
        IDxcContainerReflection* containReflection = Dxcompiler::Instance().ContainerReflection();
        IFT(containReflection->Load(dxilBlob));

        uint32_t dxilPartIndex = ~0u;
        IFT(containReflection->FindFirstPartKind(hlsl::DFCC_DXIL, &dxilPartIndex));
        HRESULT result = containReflection->GetPartReflection(dxilPartIndex, __uuidof(T), reinterpret_cast<void**>(&outReflection));

        return result;
    }

    void ShaderReflection(Compiler::ReflectionResultDesc& result, IDxcBlob* dxilBlob)
    {
        CComPtr<ID3D12ShaderReflection> shaderReflection;
        IFT(CreateDxcReflectionFromBlob(dxilBlob, shaderReflection));

        D3D12_SHADER_DESC shaderDesc;
        shaderReflection->GetDesc(&shaderDesc);

        std::vector<Compiler::ReflectionDesc> vecReflectionDescs;
        for (uint32_t resourceIndex = 0; resourceIndex < shaderDesc.BoundResources; ++resourceIndex)
        {
            D3D12_SHADER_INPUT_BIND_DESC bindDesc;
            shaderReflection->GetResourceBindingDesc(resourceIndex, &bindDesc);

            Compiler::ReflectionDesc reflectionDesc{};

            if (bindDesc.Type == D3D_SIT_CBUFFER || bindDesc.Type == D3D_SIT_TBUFFER)
            {
                ID3D12ShaderReflectionConstantBuffer* constantBuffer = shaderReflection->GetConstantBufferByName(bindDesc.Name);

                D3D12_SHADER_BUFFER_DESC bufferDesc;
                constantBuffer->GetDesc(&bufferDesc);

                if (strcmp(bufferDesc.Name, "$Globals") == 0)
                {
                    for (uint32_t variableIndex = 0; variableIndex < bufferDesc.Variables; ++variableIndex)
                    {
                        ID3D12ShaderReflectionVariable* variable = constantBuffer->GetVariableByIndex(variableIndex);
                        D3D12_SHADER_VARIABLE_DESC variableDesc;
                        variable->GetDesc(&variableDesc);

                        std::strncpy(reflectionDesc.name, variableDesc.Name,
                                     std::min(std::strlen(variableDesc.Name) + 1, sizeof(reflectionDesc.name)));

                        reflectionDesc.type = ShaderResourceType::Parameter;
                        reflectionDesc.bufferBindPoint = bindDesc.BindPoint;
                        reflectionDesc.bindPoint = variableDesc.StartOffset;
                        reflectionDesc.bindCount = variableDesc.Size;
                    }
                }
                else
                {
                    std::strncpy(reflectionDesc.name, bufferDesc.Name,
                                 std::min(std::strlen(bufferDesc.Name) + 1, sizeof(reflectionDesc.name)));

                    reflectionDesc.type = ShaderResourceType::ConstantBuffer;
                    reflectionDesc.bufferBindPoint = bindDesc.BindPoint;
                    reflectionDesc.bindPoint = 0;
                    reflectionDesc.bindCount = 0;
                }
            }
            else
            {
                switch (bindDesc.Type)
                {
                case D3D_SIT_TEXTURE:
                    reflectionDesc.type = ShaderResourceType::Texture;
                    break;

                case D3D_SIT_SAMPLER:
                    reflectionDesc.type = ShaderResourceType::Sampler;
                    break;

                case D3D_SIT_STRUCTURED:
                case D3D_SIT_BYTEADDRESS:
                    reflectionDesc.type = ShaderResourceType::ShaderResourceView;
                    break;

                case D3D_SIT_UAV_RWTYPED:
                case D3D_SIT_UAV_RWSTRUCTURED:
                case D3D_SIT_UAV_RWBYTEADDRESS:
                case D3D_SIT_UAV_APPEND_STRUCTURED:
                case D3D_SIT_UAV_CONSUME_STRUCTURED:
                case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
                    reflectionDesc.type = ShaderResourceType::UnorderedAccessView;
                    break;

                default:
                    llvm_unreachable("Unknown bind type.");
                    break;
                }

                std::strncpy(reflectionDesc.name, bindDesc.Name, std::min(std::strlen(bindDesc.Name) + 1, sizeof(reflectionDesc.name)));

                reflectionDesc.bufferBindPoint = 0;
                reflectionDesc.bindPoint = bindDesc.BindPoint;
                reflectionDesc.bindCount = bindDesc.BindCount;
            }

            vecReflectionDescs.push_back(reflectionDesc);
        }

        result.descCount = static_cast<uint32_t>(vecReflectionDescs.size());
        result.descs.Reset(vecReflectionDescs.data(), sizeof(Compiler::ReflectionDesc) * result.descCount);
        result.instructionCount = shaderDesc.InstructionCount;
    }
#endif

    std::wstring ShaderProfileName(ShaderStage stage, Compiler::ShaderModel shaderModel)
    {
        std::wstring shaderProfile;
        switch (stage)
        {
        case ShaderStage::VertexShader:
            shaderProfile = L"vs";
            break;

        case ShaderStage::PixelShader:
            shaderProfile = L"ps";
            break;

        case ShaderStage::GeometryShader:
            shaderProfile = L"gs";
            break;

        case ShaderStage::HullShader:
            shaderProfile = L"hs";
            break;

        case ShaderStage::DomainShader:
            shaderProfile = L"ds";
            break;

        case ShaderStage::ComputeShader:
            shaderProfile = L"cs";
            break;

        // UE Change Begin: Ray tracing shaders use a library profile.
        case ShaderStage::RayGen:
        case ShaderStage::RayMiss:
        case ShaderStage::RayHitGroup:
        case ShaderStage::RayCallable:
            return L"lib_6_3";
        // UE Change End: Ray tracing shaders use a library profile.

        default:
            llvm_unreachable("Invalid shader stage.");
        }

        shaderProfile.push_back(L'_');
        shaderProfile.push_back(L'0' + shaderModel.major_ver);
        shaderProfile.push_back(L'_');
        shaderProfile.push_back(L'0' + shaderModel.minor_ver);

        return shaderProfile;
    }

    void ConvertDxcResult(Compiler::ResultDesc& result, IDxcOperationResult* dxcResult, ShadingLanguage targetLanguage, bool asModule)
    {
        HRESULT status;
        IFT(dxcResult->GetStatus(&status));

        result.target.Reset();
        result.errorWarningMsg.Reset();

        CComPtr<IDxcBlobEncoding> errors;
        IFT(dxcResult->GetErrorBuffer(&errors));
        if (errors != nullptr)
        {
            result.errorWarningMsg.Reset(errors->GetBufferPointer(), static_cast<uint32_t>(errors->GetBufferSize()));
            errors = nullptr;
        }

        result.hasError = true;
        if (SUCCEEDED(status))
        {
            CComPtr<IDxcBlob> program;
            IFT(dxcResult->GetResult(&program));
            dxcResult = nullptr;
            if (program != nullptr)
            {
                result.target.Reset(program->GetBufferPointer(), static_cast<uint32_t>(program->GetBufferSize()));
                result.hasError = false;
            }

#ifdef LLVM_ON_WIN32
            if ((targetLanguage == ShadingLanguage::Dxil) && !asModule)
            {
                // Gather reflection information only for ShadingLanguage::Dxil
                ShaderReflection(result.reflection, program);
            }
#else
            SC_UNUSED(targetLanguage);
            SC_UNUSED(asModule);
#endif
        }
    }

    Compiler::ResultDesc CompileToBinary(const Compiler::SourceDesc& source, const Compiler::Options& options,
                                         ShadingLanguage targetLanguage, bool asModule)
    {
        assert((targetLanguage == ShadingLanguage::Dxil) || (targetLanguage == ShadingLanguage::SpirV));

        std::wstring shaderProfile;
        if (asModule)
        {
            if (targetLanguage == ShadingLanguage::Dxil)
            {
                shaderProfile = L"lib_6_x";
            }
            else
            {
                llvm_unreachable("Spir-V module is not supported.");
            }
        }
        else
        {
            shaderProfile = ShaderProfileName(source.stage, options.shaderModel);
        }

        std::vector<DxcDefine> dxcDefines;
        std::vector<std::wstring> dxcDefineStrings;
        // Need to reserve capacity so that small-string optimization does not
        // invalidate the pointers to internal string data while resizing.
        dxcDefineStrings.reserve(source.numDefines * 2);
        for (size_t i = 0; i < source.numDefines; ++i)
        {
            const auto& define = source.defines[i];

            std::wstring nameUtf16Str;
            Unicode::UTF8ToWideString(define.name, &nameUtf16Str);
            dxcDefineStrings.emplace_back(std::move(nameUtf16Str));
            const wchar_t* nameUtf16 = dxcDefineStrings.back().c_str();

            const wchar_t* valueUtf16;
            if (define.value != nullptr)
            {
                std::wstring valueUtf16Str;
                Unicode::UTF8ToWideString(define.value, &valueUtf16Str);
                dxcDefineStrings.emplace_back(std::move(valueUtf16Str));
                valueUtf16 = dxcDefineStrings.back().c_str();
            }
            else
            {
                valueUtf16 = nullptr;
            }

            dxcDefines.push_back({nameUtf16, valueUtf16});
        }

        CComPtr<IDxcBlobEncoding> sourceBlob;
        IFT(Dxcompiler::Instance().Library()->CreateBlobWithEncodingOnHeapCopy(
            source.source, static_cast<UINT32>(std::strlen(source.source)), CP_UTF8, &sourceBlob));
        IFTARG(sourceBlob->GetBufferSize() >= 4);

        std::wstring shaderNameUtf16;
        Unicode::UTF8ToWideString(source.fileName, &shaderNameUtf16);

        std::wstring entryPointUtf16;
        Unicode::UTF8ToWideString(source.entryPoint, &entryPointUtf16);

        std::vector<std::wstring> dxcArgStrings;

        // HLSL matrices are translated into SPIR-V OpTypeMatrixs in a transposed manner,
        // See also https://antiagainst.github.io/post/hlsl-for-vulkan-matrices/
        if (options.packMatricesInRowMajor)
        {
            dxcArgStrings.push_back(L"-Zpr");
        }
        else
        {
            dxcArgStrings.push_back(L"-Zpc");
        }

        if (options.enable16bitTypes)
        {
            if (options.shaderModel >= Compiler::ShaderModel{6, 2})
            {
                dxcArgStrings.push_back(L"-enable-16bit-types");
            }
            else
            {
                throw std::runtime_error("16-bit types requires shader model 6.2 or up.");
            }
        }

        if (options.enableDebugInfo)
        {
            dxcArgStrings.push_back(L"-Zi");
        }

        if (options.disableOptimizations)
        {
            dxcArgStrings.push_back(L"-Od");
        }
        else
        {
            if (options.optimizationLevel < 4)
            {
                dxcArgStrings.push_back(std::wstring(L"-O") + static_cast<wchar_t>(L'0' + options.optimizationLevel));
            }
            else
            {
                llvm_unreachable("Invalid optimization level.");
            }
        }

        if (options.shiftAllCBuffersBindings > 0)
        {
            dxcArgStrings.push_back(L"-fvk-b-shift");
            dxcArgStrings.push_back(std::to_wstring(options.shiftAllCBuffersBindings));
            dxcArgStrings.push_back(L"all");
        }

        if (options.shiftAllUABuffersBindings > 0)
        {
            dxcArgStrings.push_back(L"-fvk-u-shift");
            dxcArgStrings.push_back(std::to_wstring(options.shiftAllUABuffersBindings));
            dxcArgStrings.push_back(L"all");
        }

        if (options.shiftAllSamplersBindings > 0)
        {
            dxcArgStrings.push_back(L"-fvk-s-shift");
            dxcArgStrings.push_back(std::to_wstring(options.shiftAllSamplersBindings));
            dxcArgStrings.push_back(L"all");
        }

        if (options.shiftAllTexturesBindings > 0)
        {
            dxcArgStrings.push_back(L"-fvk-t-shift");
            dxcArgStrings.push_back(std::to_wstring(options.shiftAllTexturesBindings));
            dxcArgStrings.push_back(L"all");
        }

        // UE Change Begin: Ensure 1.2 for ray tracing shaders
        const bool bIsRayTracingShader = (source.stage >= ShaderStage::RayGen) && (source.stage <= ShaderStage::RayCallable);
        if (bIsRayTracingShader)
        {
            dxcArgStrings.push_back(L"-fspv-target-env=vulkan1.2");
        }
        // UE Change End: Ensure 1.2 for ray tracing shaders

        switch (targetLanguage)
        {
        case ShadingLanguage::Dxil:
            break;

        case ShadingLanguage::SpirV:
        case ShadingLanguage::Hlsl:
        case ShadingLanguage::Glsl:
        case ShadingLanguage::Essl:
        case ShadingLanguage::Msl_macOS:
        case ShadingLanguage::Msl_iOS:
            dxcArgStrings.push_back(L"-spirv");
            // UE Change Begin: Use UE5 specific layout rules
            dxcArgStrings.push_back(L"-fvk-ue5-layout");
            // UE Change End: 
            // UE Change Begin: Proper fix for SV_Position.w being inverted in SPIRV & Metal vs. D3D.
            if (targetLanguage != ShadingLanguage::Hlsl)
                dxcArgStrings.push_back(L"-fvk-use-dx-position-w");
            // UE Change End: Proper fix for SV_Position.w being inverted in SPIRV & Metal vs. D3D.
            // UE Change Begin: Specify SPIRV reflection so that we retain semantic strings.
            dxcArgStrings.push_back(L"-fspv-reflect");
            // UE Change End: Specify SPIRV reflection so that we retain semantic strings.
            // UE Change Begin: Specify the Fused-Multiply-Add pass for Metal - we'll define it away later when we can.
            if (targetLanguage == ShadingLanguage::Msl_macOS || targetLanguage == ShadingLanguage::Msl_iOS || options.enableFMAPass)
                dxcArgStrings.push_back(L"-fspv-fusemuladd");
            // UE Change End: Specify the Fused-Multiply-Add pass for Metal - we'll define it away later when we can.
            // UE Change Begin: Emit SPIRV debug info when asked to.
            if (options.enableDebugInfo)
                dxcArgStrings.push_back(L"-fspv-debug=line");
            // UE Change End: Emit SPIRV debug info when asked to.
            // UE Change Begin: Support for specifying direct arguments to DXC
            for (uint32_t arg = 0; arg < options.numDXCArgs; ++arg)
            {
                std::wstring argUTF16;
                Unicode::UTF8ToWideString(options.DXCArgs[arg], &argUTF16);
                if (argUTF16.compare(0, 8, L"-Oconfig") == 0)
                {
                    // Replace previous '-O' argument with the custom configuration
                    auto dxcOptArgIter = std::find_if(dxcArgStrings.begin(), dxcArgStrings.end(),
                                                      [](const std::wstring& entry) { return entry.compare(0, 2, L"-O") == 0; });
                    if (dxcOptArgIter != dxcArgStrings.end())
                        *dxcOptArgIter = argUTF16;
                    else
                        dxcArgStrings.push_back(argUTF16);
                }
                else
                    dxcArgStrings.push_back(argUTF16);
            }
            // UE Change End: Support for specifying direct arguments to DXC
            break;

        default:
            llvm_unreachable("Invalid shading language.");
        }

        std::vector<const wchar_t*> dxcArgs;
        dxcArgs.reserve(dxcArgStrings.size());
        for (const auto& arg : dxcArgStrings)
        {
            dxcArgs.push_back(arg.c_str());
        }

        CComPtr<IDxcIncludeHandler> includeHandler = new ScIncludeHandler(std::move(source.loadIncludeCallback));
        CComPtr<IDxcOperationResult> compileResult;
        IFT(Dxcompiler::Instance().Compiler()->Compile(sourceBlob, shaderNameUtf16.c_str(), entryPointUtf16.c_str(), shaderProfile.c_str(),
                                                       dxcArgs.data(), static_cast<UINT32>(dxcArgs.size()), dxcDefines.data(),
                                                       static_cast<UINT32>(dxcDefines.size()), includeHandler, &compileResult));

        Compiler::ResultDesc ret{};
        ConvertDxcResult(ret, compileResult, targetLanguage, asModule);

        return ret;
    }

    Compiler::ResultDesc CrossCompile(const Compiler::ResultDesc& binaryResult, const Compiler::SourceDesc& source,
                                      const Compiler::Options& options, const Compiler::TargetDesc& target)
    {
        assert((target.language != ShadingLanguage::Dxil) && (target.language != ShadingLanguage::SpirV));
        assert((binaryResult.target.Size() & (sizeof(uint32_t) - 1)) == 0);

        Compiler::ResultDesc ret;

        ret.errorWarningMsg = binaryResult.errorWarningMsg;
        ret.isText = true;

        uint32_t intVersion = 0;
        if (target.version != nullptr)
        {
            intVersion = std::stoi(target.version);
        }

        const uint32_t* spirvIr = reinterpret_cast<const uint32_t*>(binaryResult.target.Data());
        const size_t spirvSize = binaryResult.target.Size() / sizeof(uint32_t);

        std::unique_ptr<spirv_cross::CompilerGLSL> compiler;
        bool combinedImageSamplers = false;
        bool buildDummySampler = false;

        // UE Change Begin: Allow remapping of variables in glsl
        std::vector<Remap> remaps;
        // UE Change End: Allow remapping of variables in glsl

        switch (target.language)
        {
        case ShadingLanguage::Hlsl:
            if ((source.stage == ShaderStage::GeometryShader) || (source.stage == ShaderStage::HullShader) ||
                (source.stage == ShaderStage::DomainShader))
            {
                // Check https://github.com/KhronosGroup/SPIRV-Cross/issues/121 for details
                AppendError(ret, "GS, HS, and DS has not been supported yet.");
                return ret;
            }
            if ((source.stage == ShaderStage::GeometryShader) && (intVersion < 40))
            {
                AppendError(ret, "HLSL shader model earlier than 4.0 doesn't have GS or CS.");
                return ret;
            }
            if ((source.stage == ShaderStage::ComputeShader) && (intVersion < 50))
            {
                AppendError(ret, "CS in HLSL shader model earlier than 5.0 is not supported.");
                return ret;
            }
            if (((source.stage == ShaderStage::HullShader) || (source.stage == ShaderStage::DomainShader)) && (intVersion < 50))
            {
                AppendError(ret, "HLSL shader model earlier than 5.0 doesn't have HS or DS.");
                return ret;
            }
            compiler = std::make_unique<spirv_cross::CompilerHLSL>(spirvIr, spirvSize);
            break;

        case ShadingLanguage::Glsl:
        case ShadingLanguage::Essl:
            compiler = std::make_unique<spirv_cross::CompilerGLSL>(spirvIr, spirvSize);
            // UE Change Begin: Allow separate samplers in GLSL via extensions.
            combinedImageSamplers = !options.enableSeparateSamplers;
            // UE Change End: Allow separate samplers in GLSL via extensions.
            buildDummySampler = true;

            // Legacy GLSL fixups
            if (intVersion <= 300)
            {
                auto vars = compiler->get_active_interface_variables();
                for (auto& var : vars)
                {
                    auto varClass = compiler->get_storage_class(var);

                    // Make VS out and PS in variable names match
                    if ((source.stage == ShaderStage::VertexShader) && (varClass == spv::StorageClass::StorageClassOutput))
                    {
                        auto name = compiler->get_name(var);
                        if ((name.find("out_var_") == 0) || (name.find("out.var.") == 0))
                        {
                            name.replace(0, 8, "varying_");
                            compiler->set_name(var, name);
                        }
                    }
                    else if ((source.stage == ShaderStage::PixelShader) && (varClass == spv::StorageClass::StorageClassInput))
                    {
                        auto name = compiler->get_name(var);
                        if ((name.find("in_var_") == 0) || (name.find("in.var.") == 0))
                        {
                            name.replace(0, 7, "varying_");
                            compiler->set_name(var, name);
                        }
                    }
                }
            }
            break;

        case ShadingLanguage::Msl_macOS:
        case ShadingLanguage::Msl_iOS:
            if (source.stage == ShaderStage::GeometryShader)
            {
                AppendError(ret, "MSL doesn't have GS.");
                return ret;
            }
            compiler = std::make_unique<spirv_cross::CompilerMSL>(spirvIr, spirvSize);
            break;

        default:
            llvm_unreachable("Invalid target language.");
        }

        spv::ExecutionModel model;
        switch (source.stage)
        {
        case ShaderStage::VertexShader:
            model = spv::ExecutionModelVertex;
            break;

        case ShaderStage::HullShader:
            model = spv::ExecutionModelTessellationControl;
            break;

        case ShaderStage::DomainShader:
            model = spv::ExecutionModelTessellationEvaluation;
            break;

        case ShaderStage::GeometryShader:
            model = spv::ExecutionModelGeometry;
            break;

        case ShaderStage::PixelShader:
            model = spv::ExecutionModelFragment;
            break;

        case ShaderStage::ComputeShader:
            model = spv::ExecutionModelGLCompute;
            break;

        default:
            llvm_unreachable("Invalid shader stage.");
        }
        compiler->set_entry_point(source.entryPoint, model);

        spirv_cross::CompilerGLSL::Options opts = compiler->get_common_options();
        if (target.version != nullptr)
        {
            opts.version = intVersion;
        }
        opts.es = (target.language == ShadingLanguage::Essl);
        opts.separate_shader_objects = !opts.es;
        opts.flatten_multidimensional_arrays = false;
        opts.enable_420pack_extension =
            (target.language == ShadingLanguage::Glsl) && ((target.version == nullptr) || (opts.version >= 420));
        // UE Change Begin: Fixup layout locations to include padding for arrays.
        opts.fixup_layout_locations = options.remapAttributeLocations;
        // UE Change End: Fixup layout locations to include padding for arrays.
        // UE Change Begin: Always enable Vulkan semantics if we don't target GLSL or ESSL
        opts.vulkan_semantics = !(target.language == ShadingLanguage::Glsl || target.language == ShadingLanguage::Essl);
        // UE Change End: Always enable Vulkan semantics if we don't target GLSL or ESSL
        opts.vertex.fixup_clipspace = opts.es;
        opts.vertex.flip_vert_y = opts.es;
        opts.vertex.support_nonzero_base_instance = true;
        compiler->set_common_options(opts);

        // UE Change Begin: Allow variable typenames to be renamed to support samplerExternalOES in ESSL.
        if (target.language == ShadingLanguage::Glsl || target.language == ShadingLanguage::Essl)
        {
            auto* glslCompiler = static_cast<spirv_cross::CompilerGLSL*>(compiler.get());
            auto glslOpts = glslCompiler->get_common_options();

			// UE Change Begin: Improved support for PLS and FBF
			std::vector<PLSArg> PLSInputs;
			std::vector<PLSArg> PLSOutputs;
			std::vector<PLSInOutArg> PLSInOuts;

			std::vector<FBFArg> FBFArgs;
			// UE Change End: Improved support for PLS and FBF

            for (unsigned i = 0; i < target.numOptions; i++)
            {
                auto& Define = target.options[i];
                if (!ParseSpirvCrossOptionGlsl(glslOpts, Define))
                {
					// UE Change Begin: Improved support for PLS and FBF
					if (!GatherPLSRemaps(PLSInputs, PLSOutputs, PLSInOuts, Define) &&
						!GatherFBFRemaps(FBFArgs, Define))
					{
						if (!strcmp(Define.name, "remap_glsl"))
						{
							std::vector<std::string> Args;
							std::stringstream ss(Define.value);
							std::string Arg;

							while (std::getline(ss, Arg, ' '))
							{
								Args.push_back(Arg);
							}

							if (Args.size() < 3)
								continue;

							remaps.push_back({ Args[0], Args[1], (uint32_t)std::atoi(Args[2].c_str()) });
						}
					}
					// UE Change End: Improved support for PLS and FBF
                }
            }

            // UE Change Begin: Allow remapping of variables in glsl
            remap(*glslCompiler, glslCompiler->get_shader_resources(), remaps);
            // UE Change End: Allow remapping of variables in glsl

			// UE Change Begin: Improved support for PLS and FBF
			spirv_cross::ShaderResources res = compiler->get_shader_resources();
			auto pls_inputs = remap_pls(PLSInputs, res.stage_inputs, &res.subpass_inputs);
			auto pls_outputs = remap_pls(PLSOutputs, res.stage_outputs, nullptr);
			auto pls_inouts = remap_pls_inout(*glslCompiler, PLSInOuts, res.stage_outputs, res.subpass_inputs);

			compiler->remap_pixel_local_storage(move(pls_inputs), move(pls_outputs), move(pls_inouts));
			for (FBFArg & fetch : FBFArgs)
			{
				compiler->remap_ext_framebuffer_fetch(fetch.input_index, fetch.color_attachment, true);
			}
			// UE Change End: Improved support for PLS and FBF

			// UE Change Begin: Force Glsl Clipspace when using ES
			if (glslOpts.force_glsl_clipspace)
			{
				glslOpts.vertex.fixup_clipspace = false;
				glslOpts.vertex.flip_vert_y = false;
			}
			// UE Change End: Force Glsl Clipspace when using ES

            glslCompiler->set_common_options(glslOpts);

            if (target.variableTypeRenameCallback)
            {
                compiler->set_variable_type_remap_callback(
                    [&target](const spirv_cross::SPIRType&, const std::string& var_name, std::string& name_of_type)
                    {
                        Blob Result = target.variableTypeRenameCallback(var_name.c_str(), name_of_type.c_str());
                        if (Result.Size() > 0)
                        {
                            name_of_type = (char const*)Result.Data();
                        }
                    }
                );
            }
        }
        else
        // UE Change End: Allow variable typenames to be renamed to support samplerExternalOES in ESSL.
        if (target.language == ShadingLanguage::Hlsl)
        {
            auto* hlslCompiler = static_cast<spirv_cross::CompilerHLSL*>(compiler.get());
            auto hlslOpts = hlslCompiler->get_hlsl_options();
            if (target.version != nullptr)
            {
                if (opts.version < 30)
                {
                    AppendError(ret, "HLSL shader model earlier than 3.0 is not supported.");
                    return ret;
                }
                hlslOpts.shader_model = opts.version;
            }

            if (hlslOpts.shader_model <= 30)
            {
                combinedImageSamplers = true;
                buildDummySampler = true;
            }

            // UE Change Begin: Support overriding HLSL options.
            auto commonOpts = hlslCompiler->get_common_options();
            for (unsigned i = 0; i < target.numOptions; i++)
            {
                if (!ParseSpirvCrossOptionCommon(commonOpts, target.options[i]))
                {
                    ParseSpirvCrossOptionHlsl(hlslOpts, target.options[i]);
                }
            }
            hlslCompiler->set_common_options(commonOpts);
            // UE Change End: Support overriding HLSL options.

            hlslCompiler->set_hlsl_options(hlslOpts);
        }
        else if ((target.language == ShadingLanguage::Msl_macOS) || (target.language == ShadingLanguage::Msl_iOS))
        {
            auto* mslCompiler = static_cast<spirv_cross::CompilerMSL*>(compiler.get());
            auto mslOpts = mslCompiler->get_msl_options();
            if (target.version != nullptr)
            {
                mslOpts.msl_version = opts.version;
            }
            mslOpts.swizzle_texture_samples = false;

            // UE Change Begin: Ensure base vertex and instance indices start with zero if source language is HLSL.
            mslOpts.enable_base_index_zero = true;
            // UE Change End: Ensure base vertex and instance indices start with zero if source language is HLSL.

            // UE Change Begin: Support reflection & overriding Metal options & resource bindings to generate correct code.
            for (unsigned i = 0; i < target.numOptions; i++)
            {
                ParseSpirvCrossOptionMetal(mslOpts, target.options[i]);
            }
            // UE Change End: Support reflection & overriding Metal options & resource bindings to generate correct code.

            mslOpts.platform = (target.language == ShadingLanguage::Msl_iOS) ? spirv_cross::CompilerMSL::Options::iOS
                                                                             : spirv_cross::CompilerMSL::Options::macOS;

            mslCompiler->set_msl_options(mslOpts);

			// UE Change Begin: Don't re-assign binding slots. This is done with SPIRV-Reflect.
#if 0
            const auto& resources = mslCompiler->get_shader_resources();

            uint32_t textureBinding = 0;
            for (const auto& image : resources.separate_images)
            {
                mslCompiler->set_decoration(image.id, spv::DecorationBinding, textureBinding);
                ++textureBinding;
            }

            uint32_t samplerBinding = 0;
            for (const auto& sampler : resources.separate_samplers)
            {
                mslCompiler->set_decoration(sampler.id, spv::DecorationBinding, samplerBinding);
                ++samplerBinding;
            }
#endif
			// UE Change End: Don't re-assign binding slots. This is done with SPIRV-Reflect.
        }

        if (buildDummySampler)
        {
            const uint32_t sampler = compiler->build_dummy_sampler_for_combined_images();
            if (sampler != 0)
            {
                compiler->set_decoration(sampler, spv::DecorationDescriptorSet, 0);
                compiler->set_decoration(sampler, spv::DecorationBinding, 0);
            }
        }

        if (combinedImageSamplers)
        {
            // UE Change Begin: For OpenGL based platforms we merge all samplers to a single sampler per texture
            bool singleSamplerPerTexture = (target.language == ShadingLanguage::Glsl || target.language == ShadingLanguage::Essl);
            compiler->build_combined_image_samplers(singleSamplerPerTexture);
            // UE Change End: For OpenGL based platforms we merge all samplers to a single sampler per texture

            if (options.inheritCombinedSamplerBindings)
            {
                spirv_cross_util::inherit_combined_sampler_bindings(*compiler);
            }

            for (auto& remap : compiler->get_combined_image_samplers())
            {
                compiler->set_name(remap.combined_id,
                                   "SPIRV_Cross_Combined" + compiler->get_name(remap.image_id) + compiler->get_name(remap.sampler_id));
            }
        }

        if (target.language == ShadingLanguage::Hlsl)
        {
            auto* hlslCompiler = static_cast<spirv_cross::CompilerHLSL*>(compiler.get());
            const uint32_t newBuiltin = hlslCompiler->remap_num_workgroups_builtin();
            if (newBuiltin)
            {
                compiler->set_decoration(newBuiltin, spv::DecorationDescriptorSet, 0);
                compiler->set_decoration(newBuiltin, spv::DecorationBinding, 0);
            }
        }

        try
        {
            const std::string targetStr = compiler->compile();
            ret.target.Reset(targetStr.data(), static_cast<uint32_t>(targetStr.size()));
            ret.hasError = false;
            ret.reflection.descs.Reset(binaryResult.reflection.descs.Data(),
                                       sizeof(Compiler::ReflectionDesc) * binaryResult.reflection.descCount);
            ret.reflection.descCount = binaryResult.reflection.descCount;
            ret.reflection.instructionCount = binaryResult.reflection.instructionCount;
        }
        catch (spirv_cross::CompilerError& error)
        {
            const char* errorMsg = error.what();
            ret.errorWarningMsg.Reset(errorMsg, static_cast<uint32_t>(std::strlen(errorMsg)));
            ret.hasError = true;
        }

        return ret;
    }

// UE Change Begin: Two stage compilation is preferable for UE4 as it avoids polluting SC with SPIRV->MSL complexities.
} // namespace
namespace ShaderConductor
{
// UE Change End: Two stage compilation is preferable for UE4 as it avoids polluting SC with SPIRV->MSL complexities.

    Compiler::ResultDesc Compiler::ConvertBinary(const Compiler::ResultDesc& binaryResult, const Compiler::SourceDesc& source,
                                       const Compiler::Options& options, const Compiler::TargetDesc& target)
    {
        if (!binaryResult.hasError)
        {
            if (target.asModule)
            {
                return binaryResult;
            }
            else
            {
                switch (target.language)
                {
                case ShadingLanguage::Dxil:
                case ShadingLanguage::SpirV:
                    return binaryResult;

                case ShadingLanguage::Hlsl:
                case ShadingLanguage::Glsl:
                case ShadingLanguage::Essl:
                case ShadingLanguage::Msl_macOS:
                case ShadingLanguage::Msl_iOS:
                    return CrossCompile(binaryResult, source, options, target);

                default:
                    llvm_unreachable("Invalid shading language.");
                    break;
                }
            }
        }
        else
        {
            return binaryResult;
        }
    }
} // namespace

namespace ShaderConductor
{
    class Blob::BlobImpl
    {
    public:
        BlobImpl(const void* data, uint32_t size) noexcept
            : m_data(reinterpret_cast<const uint8_t*>(data), reinterpret_cast<const uint8_t*>(data) + size)
        {
        }

        const void* Data() const noexcept
        {
            return m_data.data();
        }

        uint32_t Size() const noexcept
        {
            return static_cast<uint32_t>(m_data.size());
        }

    private:
        std::vector<uint8_t> m_data;
    };

    Blob::Blob() noexcept = default;

    Blob::Blob(const void* data, uint32_t size)
    {
        this->Reset(data, size);
    }

    Blob::Blob(const Blob& other)
    {
        this->Reset(other.Data(), other.Size());
    }

    Blob::Blob(Blob&& other) noexcept : m_impl(std::move(other.m_impl))
    {
        other.m_impl = nullptr;
    }

    Blob::~Blob() noexcept
    {
        delete m_impl;
    }

    Blob& Blob::operator=(const Blob& other)
    {
        if (this != &other)
        {
            this->Reset(other.Data(), other.Size());
        }
        return *this;
    }

    Blob& Blob::operator=(Blob&& other) noexcept
    {
        if (this != &other)
        {
            m_impl = std::move(other.m_impl);
            other.m_impl = nullptr;
        }
        return *this;
    }

    void Blob::Reset()
    {
        delete m_impl;
        m_impl = nullptr;
    }

    void Blob::Reset(const void* data, uint32_t size)
    {
        this->Reset();
        if ((data != nullptr) && (size > 0))
        {
            m_impl = new BlobImpl(data, size);
        }
    }

    const void* Blob::Data() const noexcept
    {
        return m_impl ? m_impl->Data() : nullptr;
    }

    uint32_t Blob::Size() const noexcept
    {
        return m_impl ? m_impl->Size() : 0;
    }

    Compiler::ResultDesc Compiler::Compile(const SourceDesc& source, const Options& options, const TargetDesc& target)
    {
        ResultDesc result;
        Compiler::Compile(source, options, &target, 1, &result);
        return result;
    }

    void Compiler::Compile(const SourceDesc& source, const Options& options, const TargetDesc* targets, uint32_t numTargets,
                           ResultDesc* results)
    {
        SourceDesc sourceOverride = source;
        if (!sourceOverride.entryPoint || (std::strlen(sourceOverride.entryPoint) == 0))
        {
            sourceOverride.entryPoint = "main";
        }
        if (!sourceOverride.loadIncludeCallback)
        {
            sourceOverride.loadIncludeCallback = DefaultLoadCallback;
        }

        bool hasDxil = false;
        bool hasDxilModule = false;
        bool hasSpirV = false;
        for (uint32_t i = 0; i < numTargets; ++i)
        {
            if (targets[i].language == ShadingLanguage::Dxil)
            {
                hasDxil = true;
                if (targets[i].asModule)
                {
                    hasDxilModule = true;
                }
            }
            else
            {
                hasSpirV = true;
            }
        }

        ResultDesc dxilBinaryResult{};
        if (hasDxil)
        {
            dxilBinaryResult = CompileToBinary(sourceOverride, options, ShadingLanguage::Dxil, false);
        }

        ResultDesc dxilModuleBinaryResult{};
        if (hasDxilModule)
        {
            dxilModuleBinaryResult = CompileToBinary(sourceOverride, options, ShadingLanguage::Dxil, true);
        }

        ResultDesc spirvBinaryResult{};
        if (hasSpirV)
        {
            spirvBinaryResult = CompileToBinary(sourceOverride, options, ShadingLanguage::SpirV, false);
        }

        for (uint32_t i = 0; i < numTargets; ++i)
        {
            ResultDesc binaryResult;
            if (targets[i].language == ShadingLanguage::Dxil)
            {
                if (targets[i].asModule)
                {
                    binaryResult = dxilModuleBinaryResult;
                }
                else
                {
                    binaryResult = dxilBinaryResult;
                }
            }
            else
            {
                binaryResult = spirvBinaryResult;
            }

            results[i] = ConvertBinary(binaryResult, sourceOverride, options, targets[i]);
        }
    }

    Compiler::ResultDesc Compiler::Disassemble(const DisassembleDesc& source)
    {
        assert((source.language == ShadingLanguage::SpirV) || (source.language == ShadingLanguage::Dxil));

        Compiler::ResultDesc ret;

        ret.isText = true;

        if (source.language == ShadingLanguage::SpirV)
        {
            const uint32_t* spirvIr = reinterpret_cast<const uint32_t*>(source.binary);
            const size_t spirvSize = source.binarySize / sizeof(uint32_t);

            spv_context context = spvContextCreate(SPV_ENV_UNIVERSAL_1_3);
			// UE Change Begin: Enable comments to improve readability for SPIR-V disassembly
            uint32_t options =
                SPV_BINARY_TO_TEXT_OPTION_COMMENT | SPV_BINARY_TO_TEXT_OPTION_INDENT | SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES;
            // UE Change End: Enable comments to improve readability for SPIR-V disassembly
            spv_text text = nullptr;
            spv_diagnostic diagnostic = nullptr;

            spv_result_t error = spvBinaryToText(context, spirvIr, spirvSize, options, &text, &diagnostic);
            spvContextDestroy(context);

            if (error)
            {
                ret.errorWarningMsg.Reset(diagnostic->error, static_cast<uint32_t>(std::strlen(diagnostic->error)));
                ret.hasError = true;
                spvDiagnosticDestroy(diagnostic);
            }
            else
            {
                const std::string disassemble = text->str;
                ret.target.Reset(disassemble.data(), static_cast<uint32_t>(disassemble.size()));
                ret.hasError = false;
            }

            spvTextDestroy(text);
        }
        else
        {
            CComPtr<IDxcBlobEncoding> blob;
            CComPtr<IDxcBlobEncoding> disassembly;
            IFT(Dxcompiler::Instance().Library()->CreateBlobWithEncodingOnHeapCopy(source.binary, source.binarySize, CP_UTF8, &blob));
            IFT(Dxcompiler::Instance().Compiler()->Disassemble(blob, &disassembly));

            if (disassembly != nullptr)
            {
                // Remove the tailing \0
                ret.target.Reset(disassembly->GetBufferPointer(), static_cast<uint32_t>(disassembly->GetBufferSize() - 1));
                ret.hasError = false;
            }
            else
            {
                ret.hasError = true;
            }
        }

        return ret;
    }

	// UE Change Begin: Add functionality to rewrite HLSL to remove unused code and globals.
    Compiler::ResultDesc Compiler::Rewrite(SourceDesc source, const Compiler::Options& options)
    {
        if (source.entryPoint == nullptr)
        {
            source.entryPoint = "main";
        }
        if (!source.loadIncludeCallback)
        {
            source.loadIncludeCallback = DefaultLoadCallback;
        }
        return RewriteHlsl(source, options);
    }
    // UE Change End: Add functionality to rewrite HLSL to remove unused code and globals.

    // UE Change Begin: Allow optimization after source-to-spirv conversion and before spirv-to-source cross-compilation
    Compiler::ResultDesc Compiler::Optimize(const ResultDesc& binaryResult, const char* const* optConfigs, uint32_t numOptConfigs)
    {
        Compiler::ResultDesc result;
        result.isText = false;
        result.hasError = false;

        spvtools::Optimizer optimizer(SPV_ENV_UNIVERSAL_1_3);

        std::string messages;
        optimizer.SetMessageConsumer([&messages](spv_message_level_t /*level*/, const char* /*filename*/,
                                                 const spv_position_t& /*position*/, const char* msg) { messages += msg; });

        // Register optimization passes specified by configuration arguments
        spvtools::OptimizerOptions options;
        options.set_run_validator(false);

        for (uint32_t optConfigIndex = 0; optConfigIndex < numOptConfigs; ++optConfigIndex)
        {
            if (!optimizer.RegisterPassFromFlag(optConfigs[optConfigIndex]))
            {
                result.hasError = true;
                result.errorWarningMsg = Blob(messages.data(), static_cast<uint32_t>(messages.size() * sizeof(char)));
                return result;
            }
        }

        // Convert SPIR-V module to STL vector for the SPIRV-Tools interface and run optimization passes
        const uint32_t* SpirvModuleData = reinterpret_cast<const uint32_t*>(binaryResult.target.Data());
        std::vector<uint32_t> SpirvModule(SpirvModuleData, SpirvModuleData + binaryResult.target.Size() / 4);

        if (optimizer.Run(SpirvModule.data(), SpirvModule.size(), &SpirvModule, options))
        {
            result.target = Blob(SpirvModule.data(), static_cast<uint32_t>(SpirvModule.size() * sizeof(uint32_t)));
        }
        else
        {
            result.hasError = true;
            result.errorWarningMsg = Blob(messages.data(), static_cast<uint32_t>(messages.size() * sizeof(char)));
        }

        return result;
    }
    // UE Change End: Allow optimization after source-to-spirv conversion and before spirv-to-source cross-compilation

    bool Compiler::LinkSupport()
    {
        return Dxcompiler::Instance().LinkerSupport();
    }

    Compiler::ResultDesc Compiler::Link(const LinkDesc& modules, const Compiler::Options& options, const TargetDesc& target)
    {
        auto linker = Dxcompiler::Instance().CreateLinker();
        IFTPTR(linker);

        auto* library = Dxcompiler::Instance().Library();

        std::vector<std::wstring> moduleNames(modules.numModules);
        std::vector<const wchar_t*> moduleNamesUtf16(modules.numModules);
        std::vector<CComPtr<IDxcBlobEncoding>> moduleBlobs(modules.numModules);
        for (uint32_t i = 0; i < modules.numModules; ++i)
        {
            IFTARG(modules.modules[i] != nullptr);

            IFT(library->CreateBlobWithEncodingOnHeapCopy(modules.modules[i]->target.Data(), modules.modules[i]->target.Size(), CP_UTF8,
                                                          &moduleBlobs[i]));
            IFTARG(moduleBlobs[i]->GetBufferSize() >= 4);

            Unicode::UTF8ToWideString(modules.modules[i]->name, &moduleNames[i]);
            moduleNamesUtf16[i] = moduleNames[i].c_str();
            IFT(linker->RegisterLibrary(moduleNamesUtf16[i], moduleBlobs[i]));
        }

        std::wstring entryPointUtf16;
        Unicode::UTF8ToWideString(modules.entryPoint, &entryPointUtf16);

        const std::wstring shaderProfile = ShaderProfileName(modules.stage, options.shaderModel);
        CComPtr<IDxcOperationResult> linkResult;
        IFT(linker->Link(entryPointUtf16.c_str(), shaderProfile.c_str(), moduleNamesUtf16.data(),
                         static_cast<UINT32>(moduleNamesUtf16.size()), nullptr, 0, &linkResult));

        Compiler::ResultDesc binaryResult{};
        ConvertDxcResult(binaryResult, linkResult, ShadingLanguage::Dxil, false);

        Compiler::SourceDesc source{};
        source.entryPoint = modules.entryPoint;
        source.stage = modules.stage;
        return ConvertBinary(binaryResult, source, options, target);
    }

	// UE Change Begin: Allow to manually shutdown compiler to avoid dangling mutex on Linux.
	void Compiler::Shutdown()
	{
#if defined(SC_EXPLICIT_DLLSHUTDOWN)
		Dxcompiler::Instance().Shutdown();
#endif
	}
	// UE Change End: Allow to manually shutdown compiler to avoid dangling mutex on Linux.
} // namespace ShaderConductor

#ifdef _WIN32
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    SC_UNUSED(instance);

    BOOL result = TRUE;
    if (reason == DLL_PROCESS_DETACH)
    {
        dllDetaching = true;

        if (reserved == 0)
        {
            // FreeLibrary has been called or the DLL load failed
            Dxcompiler::Instance().Destroy();
        }
        else
        {
            // Process termination. We should not call FreeLibrary()
            Dxcompiler::Instance().Terminate();
        }
    }

    return result;
}
#endif
