# Copyright Epic Games, Inc. All Rights Reserved.

import os
import re
import unreal
import tempfile
import flow.cmd
import unrealcmd
import prettyprinter

#-------------------------------------------------------------------------------
def _fuzzy_find_code(prompt):
    import fzf
    import subprocess as sp

    rg_args = (
        "rg",
        "--files",
        "--path-separator=/",
        "--no-ignore",
        "-g*.cpp", "-g*.Build.cs",
        "--ignore-file=" + os.path.abspath(__file__ + "/../../dirs.rgignore"),
    )

    rg = sp.Popen(rg_args, stdout=sp.PIPE, stderr=sp.DEVNULL, stdin=sp.DEVNULL)

    ret = None
    for reply in fzf.run(None, prompt=prompt, ext_stdin=rg.stdout):
        ret = reply
        break

    rg.stdout.close()
    rg.terminate()
    return ret


#-------------------------------------------------------------------------------
class _Builder(object):
    def __init__(self, ue_context):
        self._args = []
        self._ue_context = ue_context
        self._targets = ()
        self._projected = False
        self.set_platform(unreal.Platform.get_host())
        self.set_variant("development")

    def set_targets(self, *targets):    self._targets = targets
    def set_platform(self, platform):   self._platform = platform
    def set_variant(self, variant):     self._variant = variant
    def set_projected(self, value=True):self._projected = value

    def add_args(self, *args):
        self._args += list(args)

    def read_actions(self):
        platform = self._platform
        variant = self._variant
        targets = (x.get_name() for x in self._targets)

        args = self._args
        projected = any(x.is_project_target() for x in self._targets)
        if self._projected or projected:
            if project := self._ue_context.get_project():
                args = ("-Project=" + str(project.get_path()), *args)

        ubt = self._ue_context.get_engine().get_ubt()
        yield from ubt.read_actions(*targets, platform, variant, *args)



#-------------------------------------------------------------------------------
class _PrettyPrinter(prettyprinter.Printer):
    def __init__(self, logger):
        self._source_re = re.compile(r"^(\s+\[[0-9/]+\]\s+|)([^ ]+\.[a-z]+)")
        self._error_re = re.compile("([Ee]rror:|ERROR|Exception: |error LNK|error C)")
        self._warning_re = re.compile("(warning C|[Ww]arning: |note:)")
        self._progress_re = re.compile(r"^\s*@progress\s+('([^']+)'|\w+)(\s+(\d+))?")
        self._for_target_re = re.compile(r"^\*\* For ([^\s]+) \*\*")
        self._percent = 0
        self._for_target = ""
        self._section = None
        self._logger = logger
        self._error_file = tempfile.TemporaryFile(mode="w+t")

    def print_error_summary(self):
        import textwrap
        import shutil
        msg_width = shutil.get_terminal_size(fallback=(9999,0))[0]

        error_file = self._error_file
        error_file.seek(0);

        files = {}
        for line in error_file:
            m = re.search(r"\s*(.+\.[a-z]+)[(:](\d+)[,:)0-9]+\s*(.+)$", line[1:])
            if not m:
                continue

            line_no = int(m.group(2), 0)

            lines = files.setdefault(m.group(1), {})
            msgs = lines.setdefault(line_no, set())
            msgs.add((line[0] != "W", m.group(3)))

        if not files:
            return

        print()
        print(flow.cmd.text.light_yellow("Errors and warnings:"))
        print()
        msg_width -= 6
        for file_path, lines in files.items():
            print(flow.cmd.text.white(file_path))
            for line in sorted(lines.keys()):
                msgs = lines[line]
                print(
                    "  Line",
                    flow.cmd.text.white(line),
                    flow.cmd.text.grey("/ " + os.path.basename(file_path))
                )
                for is_error, msg in msgs:
                    decorator = flow.cmd.text.light_red if is_error else flow.cmd.text.light_yellow
                    with decorator:
                        for msg_line in textwrap.wrap(msg, width=msg_width):
                            print("   ", msg_line)

    def _print(self, line):
        if "  Creating library " in line:
            return

        progress = self._progress_re.search(line)
        if progress:
            next_section = progress.group(2)
            if next_section:
                self._percent = min(int(progress.group(3)), 100)
                if next_section != self._section:
                    self._section = next_section
                    self._logger.print_info(self._section)
            return

        if self._error_re.search(line):
            line = line.lstrip()
            print("E", line, sep="", file=self._error_file)
            print(flow.cmd.text.light_red(line))
            return

        if self._warning_re.search(line):
            line = line.lstrip()
            if "note:" not in line:
                print("W", line, sep="", file=self._error_file)
            print(flow.cmd.text.light_yellow(line))
            return

        for_target = self._for_target_re.search(line)
        if for_target:
            for_target = for_target.group(1)
            self._for_target = for_target[:5]
            return

        out = ""
        if self._section != None:
            out = flow.cmd.text.grey("%3d%%" % self._percent) + " "

        source = self._source_re.search(line)
        if source:
            spos, epos = source.span(2)
            if self._for_target:
                out += flow.cmd.text.grey(self._for_target)
                out += " "
            out += line[:spos]
            out += flow.cmd.text.white(line[spos:epos])
            out += line[epos:]
        else:
            out += line
        print(out)



#-------------------------------------------------------------------------------
class _BuildCmd(unrealcmd.Cmd):
    """ The 'fileormod' argument allows the build to be constrained to a specific
    module or source file to. It accepts one of the following forms as input;

      CoreTest                 - builds the target's "CoreTest" module
      CoreTest/SourceFile.cpp  - compiles CoreTest/Private/**/SourceFile.cpp
      path/to/a/SourceFile.cpp - absolute or relative single file to compile
      -                        - fuzzy find the file/modules to build (single dash)
    """
    _variant  = unrealcmd.Arg("development", "The build variant to have the build build")
    fileormod = unrealcmd.Arg("", "Constrain build to a single file or module")
    ubtargs   = unrealcmd.Arg([str], "Arguments to pass to UnrealBuildTool")
    clean     = unrealcmd.Opt(False, "Cleans the current build")
    nouht     = unrealcmd.Opt(False, "Skips building UnrealHeaderTool")
    noxge     = unrealcmd.Opt(False, "Disables IncrediBuild")
    analyze   = unrealcmd.Opt(("", "visualcpp"), "Run static analysis (add =pvsstudio for PVS)")
    projected = unrealcmd.Opt(False, "Add the -Project= argument when running UnrealBuildTool")

    complete_analyze = ("visualcpp", "pvsstudio")
    #complete_ubtargs = ...see end of file

    def complete_fileormod(self, prefix):
        search_roots = (
            "Source/Runtime/*",
            "Source/Developer/*",
            "Source/Editor/*",
        )

        ue_context = self.get_unreal_context()

        if prefix:
            from pathlib import Path
            for root in search_roots:
                for suffix in ("./Private/", "./Internal/"):
                    haystack = root[:-1] + prefix + suffix
                    yield from (x.name for x in ue_context.glob(haystack + "**/*.cpp"))
        else:
            seen = set()
            for root in search_roots:
                for item in (x for x in ue_context.glob(root) if x.is_dir()):
                    if item.name not in seen:
                        seen.add(item.name)
                        yield item.name
                        yield item.name + "/"

    def _add_file_or_module(self, builder):
        file_or_module = self.args.fileormod
        self._constrained_build = bool(file_or_module)

        if not file_or_module:
            return

        if file_or_module == "-":
            print("Select .cpp/.Build.cs...", end="")
            file_or_module = _fuzzy_find_code(".build [...] ")
            if not file_or_module:
                raise SystemExit

        if file_or_module.endswith(".Build.cs"):
            file_or_module = os.path.basename(file_or_module)[:-9]

        ue_context = self.get_unreal_context()

        all_modules = True
        if project := ue_context.get_project():
            project_path_str = str(project.get_dir())
            all_modules = any(x in project_path_str for x in ("EngineTest", "Samples"))

        if all_modules:
            builder.add_args("-AllModules")

        # If there's no ./\ in the argument then it is deemed to be a module
        if not any((x in ".\\/") for x in file_or_module):
            builder.add_args("-Module=" + file_or_module)
            return

        # We're expecting a file
        if os.path.isfile(file_or_module):
            file_or_module = os.path.abspath(file_or_module)
            # (...N.B. an elif follows)

        # Perhaps this is in module/file shorthand?
        elif shorthand := file_or_module.split("/"):
            rglob_pattern = "/".join(shorthand[1:])
            search_roots = (
                "Source/Runtime/",
                "Source/Developer/",
                "Source/Editor/",
            )
            for root in search_roots:
                for item in ue_context.glob(root + shorthand[0]):
                    if file := next(item.rglob(rglob_pattern), None):
                        file_or_module = str(file.resolve())
                        break

        builder.add_args("-SingleFile=" + file_or_module)
        builder.add_args("-SkipDeploy")

    def is_constrained_build(self):
        return getattr(self, "_constrained_build", False)

    def get_builder(self):
        ue_context = self.get_unreal_context()
        builder = _Builder(ue_context)

        if self.args.clean: builder.add_args("-Clean")
        if self.args.nouht: builder.add_args("-NoBuildUHT")
        if self.args.noxge: builder.add_args("-NoXGE")
        if self.args.analyze: builder.add_args("-StaticAnalyzer=" + self.args.analyze)
        if self.args.ubtargs: builder.add_args(*self.args.ubtargs)
        if self.args.projected: builder.set_projected() # forces -Project=
        builder.add_args("-Progress")

        # Handle shortcut to compile a specific module or individual file.
        self._add_file_or_module(builder)

        return builder

    def run_build(self, builder):
        try: exec_context = self._exec_context
        except AttributeError: self._exec_context = self.get_exec_context()
        exec_context = self._exec_context

        # Inform the user of the current build configuration
        self.print_info("Build configuration")
        ue_context = self.get_unreal_context()
        ubt = ue_context.get_engine().get_ubt()
        for config in (x for x in ubt.read_configurations() if x.exists()):
            level = config.get_level()
            try:
                for category, name, value in config.read_values():
                    name = flow.cmd.text.white(name)
                    print(category, ".", name, " = ", value, " (", level.name.title(), ")", sep="")
            except IOError as e:
                self.print_warning(f"Failed loading '{config.get_path()}'")
                self.print_warning(str(e))
                continue

        self.print_info("Running UnrealBuildTool")
        printer = _PrettyPrinter(self)
        try:
            for cmd, args in builder.read_actions():
                cmd = exec_context.create_runnable(cmd, *args)
                printer.run(cmd)

                if ret := cmd.get_return_code():
                    break
            else:
                return
        except KeyboardInterrupt:
            raise KeyboardInterrupt("Waiting for UBT...")
        finally:
            printer.print_error_summary()

        return ret




#-------------------------------------------------------------------------------
class Build(_BuildCmd, unrealcmd.MultiPlatformCmd):
    """ Direct access to running UBT """
    target   = unrealcmd.Arg(str, "Name of the target to build")
    platform = unrealcmd.Arg("", "Platform to build (default = host)")
    variant  = _BuildCmd._variant

    def complete_target(self, prefix):
        ue_context = self.get_unreal_context()
        ubt_manifests = list(ue_context.glob("Intermediate/Build/BuildRules/*RulesManifest.json"))
        if not ubt_manifests:
            yield from (x.name[:-10] for x in ue_context.glob("Source/*.Target.cs"))
            yield from (x.name[:-10] for x in ue_context.glob("Source/Programs/*/*.Target.cs"))

        for item in ubt_manifests:
            with item.open("rt") as inp:
                for line in inp:
                    if ".Target.cs" not in line:
                        continue

                    slash = line.rfind("\\")
                    if -1 == (slash := slash if slash != -1 else line.rfind("/")):
                        continue

                    line = line[slash + 1:]
                    if -1 == (dot := line.find(".")):
                        continue

                    yield line[:dot]

    @unrealcmd.Cmd.summarise
    def main(self):
        self.use_all_platforms()

        ue_context = self.get_unreal_context()
        targets = self.args.target.split()
        targets = map(ue_context.get_target_by_name, targets)

        builder = self.get_builder()
        builder.set_targets(*targets)
        builder.set_variant(self.args.variant)
        if self.args.platform:
            try: platform = self.get_platform(self.args.platform).get_name()
            except ValueError: platform = self.args.platform
            builder.set_platform(platform)
        return self.run_build(builder)


#-------------------------------------------------------------------------------
def _add_clean_cmd(cmd_class):
    # Create a class for a CleanX command
    class _Clean(object):
        def main(self):
            self.args.clean = True
            return super().main()

    clean_name = "Clean" + cmd_class.__name__
    clean_class = type(clean_name, (_Clean, cmd_class), {})
    globals()[clean_name] = clean_class

    # Rather crudely change the first work from Build to Clean
    doc = cmd_class.__doc__.lstrip()
    if doc.startswith("Builds"):
        new_cmd_class = type(cmd_class.__name__, (cmd_class,), {})
        new_cmd_class.__doc__ = doc
        cmd_class.__doc__ = None
        cmd_class = new_cmd_class

        doc = "Cleans" + doc[6:]
        clean_class.__doc__ = doc

    return cmd_class

#-------------------------------------------------------------------------------
@_add_clean_cmd
class Editor(_BuildCmd, unrealcmd.MultiPlatformCmd):
    """ Builds the editor. By default the current host platform is targeted. This
    can be overridden using the '--platform' option to cross-compile the editor."""
    variant     = _BuildCmd._variant
    noscw       = unrealcmd.Opt(False, "Skip building ShaderCompileWorker")
    nopak       = unrealcmd.Opt(False, "Do not build UnrealPak")
    nointworker = unrealcmd.Opt(False, "Exclude the implicit InterchangeWorker dependency")
    platform    = unrealcmd.Opt("", "Platform to build the editor for")

    @unrealcmd.Cmd.summarise
    def main(self):
        self.use_all_platforms()

        builder = self.get_builder()
        builder.set_variant(self.args.variant)

        if self.args.platform:
            platform = self.get_platform(self.args.platform).get_name()
            builder.set_platform(platform)

        ue_context = self.get_unreal_context()
        editor_target = ue_context.get_target_by_type(unreal.TargetType.EDITOR)
        targets = (editor_target,)

        editor_only = self.is_constrained_build() # i.e. single file or module
        editor_only |= bool(self.args.analyze)
        editor_only |= self.args.variant != "development"
        if not editor_only:

            targets_to_add = []
            if not self.args.noscw:
                targets_to_add.append("ShaderCompileWorker")
            if not self.args.nopak:
                targets_to_add.append("UnrealPak")
            if not self.args.nointworker:
                targets_to_add.append("InterchangeWorker")

            for name in targets_to_add:
                prog_target = ue_context.get_target_by_name(name)
                if prog_target.get_type() == unreal.TargetType.PROGRAM:
                    targets = (*targets, prog_target)

        builder.set_targets(*targets)
        return self.run_build(builder)




#-------------------------------------------------------------------------------
@_add_clean_cmd
class Program(_BuildCmd, unrealcmd.MultiPlatformCmd):
    """ Builds a program for the current host platform (unless the --platform
    argument is provided) """
    program   = unrealcmd.Arg(str, "Target name of the program to build")
    variant   = _BuildCmd._variant
    platform  = unrealcmd.Opt("", "Platform to build the program for")

    def complete_program(self, prefix):
        ue_context = self.get_unreal_context()

        for depth in ("*", "*/*"):
            for target_cs in ue_context.glob(f"Source/Programs/{depth}/*.Target.cs"):
                yield target_cs.name[:-10]

    @unrealcmd.Cmd.summarise
    def main(self):
        ue_context = self.get_unreal_context()
        target = ue_context.get_target_by_name(self.args.program)

        builder = self.get_builder()
        builder.set_targets(target)
        builder.set_variant(self.args.variant)

        self.use_all_platforms()

        if self.args.platform:
            platform = self.get_platform(self.args.platform).get_name()
            builder.set_platform(platform)

        return self.run_build(builder)



#-------------------------------------------------------------------------------
@_add_clean_cmd
class Server(_BuildCmd, unrealcmd.MultiPlatformCmd):
    """ Builds the server target for the host platform """
    platform = unrealcmd.Arg("", "Platform to build the server for")
    variant  = _BuildCmd._variant

    complete_platform = ("win64", "linux")

    @unrealcmd.Cmd.summarise
    def main(self):
        # Set up the platform, defaulting to the host if none was given.
        platform = self.args.platform
        if not platform:
            platform = unreal.Platform.get_host()
        self.use_platform(self.args.platform)

        ue_context = self.get_unreal_context()
        target = ue_context.get_target_by_type(unreal.TargetType.SERVER)

        builder = self.get_builder()
        builder.set_targets(target)
        builder.set_variant(self.args.variant)
        if self.args.platform:
            platform = self.get_platform(self.args.platform).get_name()
            builder.set_platform(platform)

        return self.run_build(builder)



#-------------------------------------------------------------------------------
class _Runtime(_BuildCmd, unrealcmd.MultiPlatformCmd):
    platform = unrealcmd.Arg("", "Build the runtime for this platform")
    variant  = _BuildCmd._variant

    @unrealcmd.Cmd.summarise
    def _main_impl(self, target):
        platform = self.args.platform
        if not platform:
            platform = unreal.Platform.get_host().lower()
        self.use_platform(platform)

        platform = self.get_platform(platform).get_name()

        builder = self.get_builder()
        builder.set_platform(platform)
        builder.set_targets(target)
        builder.set_variant(self.args.variant)

        return self.run_build(builder)

#-------------------------------------------------------------------------------
@_add_clean_cmd
class Client(_Runtime):
    """ Builds the client runtime """
    def main(self):
        ue_context = self.get_unreal_context()
        target = ue_context.get_target_by_type(unreal.TargetType.CLIENT)
        return super()._main_impl(target)

#-------------------------------------------------------------------------------
@_add_clean_cmd
class Game(_Runtime):
    """ Builds the game runtime target binary executable """
    def main(self):
        ue_context = self.get_unreal_context()
        target = ue_context.get_target_by_type(unreal.TargetType.GAME)
        return super()._main_impl(target)



#-------------------------------------------------------------------------------
_BuildCmd.complete_ubtargs = (
    "-2015", "-2017", "-2019", "-AllModules", "-alwaysgeneratedsym",
    "-Architectures", "-build-as-framework", "-BuildPlugin", "-BuildVersion",
    "-BundleVersion", "-Clean", "-CLion", "-CMakefile", "-CodeliteFiles",
    "-CompileAsDll", "-CompileChaos", "-CompilerArguments", "-CompilerVersion",
    "-CppStd", "-CreateStub", "-Define:", "-DependencyList", "-Deploy",
    "-DisableAdaptiveUnity", "-DisablePlugin", "-DisableUnity", "-distribution",
    "-dSYM", "-EdditProjectFiles", "-EnableASan", "-EnableMSan",
    "-EnablePlugin", "-EnableTSan", "-EnableUBSan",
    "-FailIfGeneratedCodeChanges", "-FastMonoCalls", "-FastPDB", "-FlushMac",
    "-ForceDebugInfo", "-ForceHeaderGeneration", "-ForceHotReload",
    "-ForceUnity", "-Formal", "-FromMsBuild", "-generatedsymbundle",
    "-generatedsymfile", "-GPUArchitectures", "-IgnoreJunk",
    "-ImportCertificate", "-ImportCertificatePassword", "-ImportProvision",
    "-IncrementalLinking", "-Input", "-IWYU", "-KDevelopfile",
    "-LinkerArguments", "-LiveCoding", "-LiveCodingLimit",
    "-LiveCodingManifest", "-LiveCodingModules", "-Log", "-LTCG", "-Makefile",
    "-Manifest", "-MapFile", "-MaxParallelActions", "-Modular", "-Module",
    "-Monolithic", "-NoBuildUHT", "-NoCompileChaos", "-NoDebugInfo", "-NoDSYM",
    "-NoEngineChanges", "-NoFASTBuild", "-NoFastMonoCalls", "-NoHotReload",
    "-NoHotReloadFromIDE", "-NoIncrementalLinking", "-NoLink",
    "-NoManifestChanges", "-NoMutex", "-NoPCH", "-NoPDB", "-NoSharedPCH",
    "-NoUBTMakefiles", "-NoUseChaos", "-NoXGE", "-ObjSrcMap", "-Output",
    "-OverrideBuildEnvironment", "-PGOOptimize", "-PGOProfile", "-Plugin",
    "-Precompile", "-Preprocess", "-PrintDebugInfo", "-Progress",
    "-ProjectDefine:", "-ProjectFileFormat", "-ProjectFiles",
    "-PublicSymbolsByDefault", "-QMakefile", "-Quiet", "-RemoteIni", "-Rider",
    "-rtti", "-ShadowVariableErrors", "-SharedBuildEnvironment",
    "-ShowIncludes", "-SingleFile", "-SkipBuild", "-skipcrashlytics",
    "-SkipDeploy", "-SkipPreBuildTargets", "-SkipRulesCompile",
    "-StaticAnalyzer", "-StressTestUnity", "-Strict", "-stripsymbols",
    "-ThinLTO", "-Timestamps", "-Timing", "-ToolChain", "-Tracing",
    "-UniqueBuildEnvironment", "-UseChaos", "-UsePrecompiled", "-Verbose",
    "-VeryVerbose", "-VSCode", "-VSMac", "-WaitMutex", "-WarningsAsErrors",
    "-WriteOutdatedActions", "-XCodeProjectFiles", "-XGEExport",
)
