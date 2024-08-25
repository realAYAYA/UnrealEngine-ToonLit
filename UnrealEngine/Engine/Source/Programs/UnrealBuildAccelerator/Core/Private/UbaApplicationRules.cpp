// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaApplicationRules.h"
#include "UbaPlatform.h"

namespace uba
{
	class ApplicationRulesVC : public ApplicationRules
	{
		using Super = ApplicationRules;

		virtual bool AllowDetach() override
		{
			return true;
		}

		virtual u64 FileTypeMaxSize(const StringBufferBase& file, bool isSystemOrTempFile) override
		{
			if (file.EndsWith(TC(".pdb")))
				return 14ull * 1024 * 1024 * 1024; // This is ridiculous
			if (file.EndsWith(TC(".json")) || file.EndsWith(TC(".exp")))
				return 32 * 1024 * 1024;
			if (file.EndsWith(TC(".obj")) || (isSystemOrTempFile && file.Contains(TC("_cl_")))) // There are _huge_ obj files when building with -stresstestunity
				return 1024 * 1024 * 1024;
			return Super::FileTypeMaxSize(file, isSystemOrTempFile);
		}
		virtual bool CanDetour(const tchar* file) override
		{
			if (file[0] == '\\' && file[1] == '\\' && !Contains(file, TC("vctip_"))) // This might be too aggressive but will cover pipes etc.. might need revisit
				return false;
			return true;
		}
		virtual bool IsThrowAway(const tchar* fileName, u32 fileNameLen) override
		{
			return Contains(fileName, TC("vctip_")) || Super::IsThrowAway(fileName, fileNameLen);
		}
		virtual bool KeepInMemory(const tchar* fileName, u32 fileNameLen, const tchar* systemTemp) override
		{
			if (Contains(fileName, TC("\\vctip_")) != 0)
				return true;
			if (Contains(fileName, systemTemp))
				return true;
			return Super::KeepInMemory(fileName, fileNameLen, systemTemp);
		}

		virtual bool IsExitCodeSuccess(u32 exitCode) override
		{
			return exitCode == 0;
		}
	};

	class ApplicationRulesClExe : public ApplicationRulesVC
	{
	public:
		virtual bool IsOutputFile(const tchar* file, u64 fileLen) override
		{
			return EndsWith(file, fileLen, TC(".h.obj"))
				|| EndsWith(file, fileLen, TC(".c.obj"))
				|| EndsWith(file, fileLen, TC(".cc.obj"))
				|| EndsWith(file, fileLen, TC(".cpp.obj"))
				|| EndsWith(file, fileLen, TC(".dep.json"))
				|| EndsWith(file, fileLen, TC(".rc2.res")) // Not really an obj file.. 
				;// || EndsWith(file, fileLen, TC(".h.pch") // Not tested enough
		}

		virtual bool IsRarelyRead(const StringBufferBase& file) override
		{
			return file.EndsWith(TC(".cpp"))
				|| file.EndsWith(TC(".obj.rsp"));
		}

		virtual bool IsRarelyReadAfterWritten(const tchar* fileName, u64 fileNameLen) const override
		{
			return EndsWith(fileName, fileNameLen, TC(".dep.json"))
				|| EndsWith(fileName, fileNameLen, TC(".exe"))
				|| EndsWith(fileName, fileNameLen, TC(".dll"));
		}

		virtual bool NeedsSharedMemory(const tchar* file) override
		{
			return Contains(file, TC("\\_cl_")); // This file is needed when cl.exe spawns link.exe
		}

	};

	class ApplicationRulesLinkExe : public ApplicationRulesVC
	{
		using Super = ApplicationRulesVC;
	public:
		virtual bool IsOutputFile(const tchar* file, u64 fileLen) override
		{
			return EndsWith(file, fileLen, TC(".lib"))
				|| EndsWith(file, fileLen, TC(".exp"))
				|| EndsWith(file, fileLen, TC(".pdb"))
				|| EndsWith(file, fileLen, TC(".dll"))
				|| EndsWith(file, fileLen, TC(".exe"))
				|| EndsWith(file, fileLen, TC(".rc2.res")); // Not really an obj file.. 
		}

		virtual bool IsThrowAway(const tchar* fileName, u32 fileNameLen) override
		{
			return Contains(fileName, TC(".sup.")); // .sup.lib/exp are throw-away files that we don't want created
		}

		virtual bool CanExist(const tchar* file) override
		{
			return Contains(file, TC("vctip.exe")) == false; // This is hacky but we don't want to start vctip.exe
		}
		virtual bool NeedsSharedMemory(const tchar* file) override
		{
			return Contains(file, TC("lnk{")) // This file is shared from link.exe to mt.exe and rc.exe so we need to put it shared memory
			 	|| Contains(file, TC("\\_cl_")); // When link.exe is spawned by cl.exe we might use this which is in shared memory
		}

		virtual bool IsRarelyRead(const StringBufferBase& file) override
		{
			return file.EndsWith(TC(".exp"))
				|| file.EndsWith(TC(".dll.rsp"))
				|| file.EndsWith(TC(".lib.rsp"))
				|| file.EndsWith(TC(".ilk"))
				|| file.EndsWith(TC(".pdb"));
		}

		virtual bool AllowStorageProxy(const StringBufferBase& file) override
		{
			if (file.EndsWith(TC(".obj")))
				return false;
			return Super::AllowStorageProxy(file);
		}

		virtual bool IsRarelyReadAfterWritten(const tchar* fileName, u64 fileNameLen) const override
		{
			return EndsWith(fileName, fileNameLen, TC(".pdb"))
				|| EndsWith(fileName, fileNameLen, TC(".exe"))
				|| EndsWith(fileName, fileNameLen, TC(".dll"));
		}
	};

	// ==== Clang tool chain ====

	class ApplicationRulesClang : public ApplicationRules
	{
	public:
		virtual bool EnableVectoredExceptionHandler() override
		{
			return true;
		}

		virtual bool IsExitCodeSuccess(u32 exitCode) override
		{
			return exitCode == 0;
		}
	};

	class ApplicationRulesClangPlusPlusExe : public ApplicationRulesClang
	{
	public:
		virtual bool AllowDetach() override
		{
			return true;
		}

		virtual bool IsOutputFile(const tchar* file, u64 fileLen) override
		{
			return EndsWith(file, fileLen, TC(".c.d"))
				|| EndsWith(file, fileLen, TC(".h.d"))
				|| EndsWith(file, fileLen, TC(".cc.d"))
				|| EndsWith(file, fileLen, TC(".cpp.d"))
				|| EndsWith(file, fileLen, TC(".o.tmp")) // Clang writes to tmp file and then move
				|| EndsWith(file, fileLen, TC(".obj.tmp")) // Clang (verse) writes to tmp file and then move
				;// || EndsWith(file, fileLen, TC(".gch.tmp")); // Need to fix "has been modified since the precompiled header"
		}

		virtual bool IsRarelyRead(const StringBufferBase& file) override
		{
			return file.EndsWith(TC(".cpp"))
				|| file.EndsWith(TC(".o.rsp"));
		}

		virtual bool IsRarelyReadAfterWritten(const tchar* fileName, u64 fileNameLen) const override
		{
			return EndsWith(fileName, fileNameLen, TC(".d"));
		}

		virtual bool AllowMiMalloc() override
		{
			return true;
		}
	};

	class ApplicationRulesLdLLdExe : public ApplicationRulesClang
	{
		using Super = ApplicationRulesClang;

		virtual bool IsOutputFile(const tchar* file, u64 fileLen) override
		{
			return Contains(file, TC(".tmp")); // both .so.tmp and .tmp123456
		}

		virtual bool IsRarelyRead(const StringBufferBase& file) override
		{
			return file.EndsWith(TC(".so.rsp"));
		}

		virtual u64 FileTypeMaxSize(const StringBufferBase& file, bool isSystemOrTempFile) override
		{
			return 14ull * 1024 * 1024 * 1024; // This is ridiculous (needed for asan targets)
		}
	};

	class ApplicationRulesLlvmObjCopyExe : public ApplicationRulesClang
	{
		using Super = ApplicationRulesClang;

		virtual bool IsOutputFile(const tchar* file, u64 fileLen) override
		{
			return Contains(file, TC(".temp-stream-"));
		}

		virtual u64 FileTypeMaxSize(const StringBufferBase& file, bool isSystemOrTempFile) override
		{
			if (IsOutputFile(file.data, file.count))
				return 14ull * 1024 * 1024 * 1024; // This is ridiculous (needed for asan targets)
			return Super::FileTypeMaxSize(file, isSystemOrTempFile);
		}
	};

	class ApplicationRulesDumpSymsExe : public ApplicationRulesClang
	{
		virtual bool IsOutputFile(const tchar* file, u64 fileLen) override
		{
			return false; // EndsWith(file, fileLen, TC(".psym")); With psym as output file the BreakpadSymbolEncoder fails to output a .sym file
		}
	};

	class ApplicationRulesOrbisClangPlusPlusExe : public ApplicationRulesClangPlusPlusExe
	{
		using Super = ApplicationRulesClangPlusPlusExe;

		virtual bool IsThrowAway(const tchar* fileName, u32 fileNameLen) override
		{
			return EndsWith(fileName, fileNameLen, TC("-telemetry.json")) || Super::IsThrowAway(fileName, fileNameLen);
		}

		//virtual bool NeedsSharedMemory(const tchar* file)
		//{
		//	return Contains(file, TC("lto-llvm")); // Used by a clang based platform's link time optimization pass. Shared from lto process back to linker process
		//}
	};

	class ApplicationRulesOrbisLdExe : public ApplicationRules
	{
		using Super = ApplicationRules;

		//virtual bool IsOutputFile(const tchar* file, u64 fileLen) override
		//{
		//	return EndsWith(file, fileLen, TC(".self")) || Equals(file, TC("Symbols.map"));
		//}
		virtual bool KeepInMemory(const tchar* fileName, u32 fileNameLen, const tchar* systemTemp) override
		{
			return Super::KeepInMemory(fileName, fileNameLen, systemTemp)
				|| Contains(fileName, TC("thinlto-"));// Used by a clang based platform's link time optimization pass. Shared from lto process back to linker process
		}
		virtual bool NeedsSharedMemory(const tchar* file) override
		{
			return Contains(file, TC("thinlto-")); // Used by a clang based platform's link time optimization pass. Shared from lto process back to linker process
		}
	};

	class ApplicationRulesProsperoClangPlusPlusExe : public ApplicationRulesClangPlusPlusExe
	{
		using Super = ApplicationRulesClangPlusPlusExe;

		virtual bool IsOutputFile(const tchar* file, u64 fileLen) override
		{
			return Contains(file, TC(".self")) || Super::IsOutputFile(file, fileLen);
		}

		virtual bool IsThrowAway(const tchar* fileName, u32 fileNameLen) override
		{
			return EndsWith(fileName, fileNameLen, TC("-telemetry.json")) || Super::IsThrowAway(fileName, fileNameLen);
		}

		//virtual bool NeedsSharedMemory(const tchar* file)
		//{
		//	return Contains(file, TC("lto-llvm")); // Used by a clang based platform's link time optimization pass. Shared from lto process back to linker process
		//}
	};

	class ApplicationRulesProsperoLldExe : public ApplicationRules
	{
		using Super = ApplicationRules;

		virtual bool IsOutputFile(const tchar* file, u64 fileLen) override
		{
			return Contains(file, TC(".self"));
		}

		virtual bool IsThrowAway(const tchar* fileName, u32 fileNameLen) override
		{
			return EndsWith(fileName, fileNameLen, TC("-telemetry.json")) || Super::IsThrowAway(fileName, fileNameLen);
		}

		//virtual bool NeedsSharedMemory(const tchar* file)
		//{
		//	return Contains(file, TC("lto-llvm")); // Used by a clang based platform's link time optimization pass. Shared from lto process back to linker process
		//}
	};

	// ====

	class ApplicationRulesISPCExe : public ApplicationRules
	{
		virtual bool AllowDetach() override
		{
			return true;
		}

		virtual bool IsOutputFile(const tchar* file, u64 fileLen) override
		{
			return EndsWith(file, fileLen, TC(".dummy.h"))
				|| EndsWith(file, fileLen, TC(".ispc.bc"))
				|| EndsWith(file, fileLen, TC(".ispc.txt"));
		}
	};

	class ApplicationRulesUBTDll : public ApplicationRules
	{
		virtual bool IsOutputFile(const tchar* file, u64 fileLen) override
		{
			return false;
			// TODO: These does not work when UnrealBuildTool creates these files multiple times in a row (building multiple targets)
			// ... on output they get stored as file mappings.. and next execution of ubt opens them for write (writing file mappings not implemented right now)
			//return EndsWith(file, fileLen, TC(".modules"))
			//	|| EndsWith(file, fileLen, TC(".target"))
			//	|| EndsWith(file, fileLen, TC(".version"));
		}
	};

	class ApplicationRulesPVSStudio : public ApplicationRules
	{
		virtual bool IsOutputFile(const tchar* file, u64 fileLen) override
		{
			return EndsWith(file, fileLen, TC(".PVS-Studio.log"))
				|| EndsWith(file, fileLen, TC(".pvslog"))
				|| EndsWith(file, fileLen, TC(".stacktrace.txt"));
		}
		
		virtual bool IsRarelyRead(const StringBufferBase& file) override
		{
			return file.EndsWith(TC(".i"))
				|| file.EndsWith(TC(".PVS-Studio.log"))
				|| file.EndsWith(TC(".pvslog"))
				|| file.EndsWith(TC(".stacktrace.txt"));
		}

#if PLATFORM_WINDOWS
		virtual void RepairMalformedLibPath(const tchar* path) override
		{
			// There is a bug where the path passed into wsplitpath_s is malformed and not null terminated correctly
			const tchar* pext = TStrstr(path, TC(".dll"));
			if (pext == nullptr) pext = TStrstr(path, TC(".DLL"));
			if (pext == nullptr) pext = TStrstr(path, TC(".exe"));
			if (pext == nullptr) pext = TStrstr(path, TC(".EXE"));
			if (pext != nullptr && *(pext + 4) != 0) *(const_cast<tchar*>(pext + 4)) = 0;
		}
#endif // #if PLATFORM_WINDOWS
	};

	class ApplicationRulesShaderCompileWorker : public ApplicationRules
	{
		virtual bool IsRarelyRead(const StringBufferBase& file) override
		{
			return file.Contains(TC(".uba."));
		}
	};

	const RulesRec* GetApplicationRules()
	{
		// TODO: Add support for data driven rules.
		// Note, they need to be possible to serialize from server to client and then from client to each detoured process

		static RulesRec rules[]
		{
			{ TC(""),							new ApplicationRules() },		// Must be index 0
			{ TC("cl.exe"),						new ApplicationRulesClExe() },	// Must be index 1
			{ TC("link.exe"),					new ApplicationRulesLinkExe() }, // Must be index 2
			{ TC("lib.exe"),					new ApplicationRulesLinkExe() },
			{ TC("cvtres.exe"),					new ApplicationRulesLinkExe() },
			{ TC("mt.exe"),						new ApplicationRulesLinkExe() },
			{ TC("rc.exe"),						new ApplicationRulesLinkExe() },
			{ TC("clang++.exe"),				new ApplicationRulesClangPlusPlusExe() },
			{ TC("clang-cl.exe"),				new ApplicationRulesClangPlusPlusExe() },
			{ TC("verse-clang-cl.exe"),			new ApplicationRulesClangPlusPlusExe() },
			{ TC("ispc.exe"),					new ApplicationRulesISPCExe() },
			{ TC("orbis-clang.exe"),			new ApplicationRulesOrbisClangPlusPlusExe() },
			{ TC("orbis-ld.exe"),				new ApplicationRulesOrbisLdExe() },
			{ TC("orbis-ltop.exe"),				new ApplicationRulesOrbisLdExe() },
			{ TC("prospero-clang.exe"),			new ApplicationRulesProsperoClangPlusPlusExe() },
			{ TC("prospero-lld.exe"),			new ApplicationRulesProsperoLldExe() },
			{ TC("dump_syms.exe"),				new ApplicationRulesDumpSymsExe() },
			{ TC("ld.lld.exe"),					new ApplicationRulesLdLLdExe() },
			{ TC("llvm-objcopy.exe"),			new ApplicationRulesLlvmObjCopyExe() },
			{ TC("UnrealBuildTool.dll"),		new ApplicationRulesUBTDll() },
			{ TC("PVS-Studio.exe"),				new ApplicationRulesPVSStudio() },
			{ TC("ShaderCompileWorker.exe"),	new ApplicationRulesShaderCompileWorker() },
			//{ L"MSBuild.dll"),				new ApplicationRules() },
			//{ L"BreakpadSymbolEncoder.exe"),	new ApplicationRulesClang() },
			//{ L"cmd.exe"),		new ApplicationRules() },
			{ nullptr, nullptr }
		};

		return rules;
	}
}