# Copyright Epic Games, Inc. All Rights Reserved.

import sys
import os
import subprocess
from pathlib import Path
import argparse
import concurrent.futures


parser = argparse.ArgumentParser(fromfile_prefix_chars='@')
parser.add_argument('--sdks-root', type=Path, required=True)
parser.add_argument('--sketchup-version', required=True)
parser.add_argument('--sketchup-sdk-version', required=True)
parser.add_argument('--target_archs', nargs='+', required=True)
parser.add_argument('--datasmithsdk-lib', type=Path, required=True)
parser.add_argument('--output-path', type=Path, required=True)
parser.add_argument('--intermediate-path', type=Path, required=True)

parser.add_argument('--multithread', action='store_true')
parser.add_argument('--dry-run', action='store_true')
parser.add_argument('--no-compile', action='store_true')
parser.add_argument('-v', '--verbose', action='store_true')

args = parser.parse_args()

## parameters
UESDKRoot = args.sdks_root 

plugin_src_root = Path(__file__).resolve().parents[1]
UE_Engine = plugin_src_root.parents[4] # get full path to the engine
assert UE_Engine.parts[-1] == 'Engine', UE_Engine

SU_SDK_VERSION = args.sketchup_sdk_version
SU_VERSION = args.sketchup_version

TARGET_ARCHS = args.target_archs;

arch_target_dict = {
    'x86_64': 'x86_64-apple-macos11',
    'arm64': 'arm64-apple-macos11'
}

output_path = args.output_path/'Plugin/UnrealDatasmithSketchUp'
obj_dir = args.intermediate_path
###########

def log_debug(msg):
    if args.verbose:
        print(msg)

def log_info(msg):
    if args.verbose:
        print(msg)

def log_warn(msg):
    if args.verbose:
        print(msg)

def parse_paths(paths_string):
    path = None
    for c in paths_string:
        if c == '"':
            if path is None: # starting '"'
                path = ''
            else: # ending '"'
                yield path
                path = None
        else: 
            if path is not None:
                path += c
            else: # parsing separating spaces
                assert c == ' '

def add_include_paths(cmd, paths):
    for p in paths:
        cmd.append('--include-directory')
        cmd.append(f'{p}')
        if not os.path.isdir(p):
            log_warn(f"WARNING: Include path not present: '{p}'")

def add_framework_search_paths(cmd, paths):
    for p in paths:
        cmd.append(f'-F{p}')    
        if not os.path.isdir(p):
            log_debug(f"Framework search path: '{p}'")

class Compiler:

    def __init__(self, cmd_template):
        self.cmd_template = cmd_template
        self.objs = []

        if args.multithread:
            log_debug(f"Compiling in {os.cpu_count()} threads")
            self.executor = concurrent.futures.ThreadPoolExecutor(os.cpu_count())

    def done(self):
        if args.multithread:
            self.executor.shutdown()

    def compile_source(self, src_cpp, dst_obj):
        cmd = self.cmd_template[:]
        cmd += ['-c', str(src_cpp)]
        cmd += ['-o', str(dst_obj)]
        if args.verbose:
            log_info(cmd)
        if not args.dry_run and not args.no_compile:
            if args.multithread:
                self.executor.submit(subprocess.check_call, cmd)
            else:
                subprocess.check_call(cmd)
        self.objs.append(str(dst_obj))

########

SU_SDK_PATH = f'{UESDKRoot}/HostMac/Mac/SketchUp/{SU_SDK_VERSION}'

DatasmithSDKPath = f'{UE_Engine}/Binaries/Mac/DatasmithSDK'

UE_Runtime = f'{UE_Engine}/Source/Runtime'
UE_Core = f'{UE_Runtime}/Core/Public'
UE_Project = f'{UE_Runtime}/Projects/Public'

UE_ThirdParty = f'{UE_Engine}/Source/ThirdParty'
UE_Imath = f'{UE_ThirdParty}/Imath/Deploy/Imath-3.1.9'

UE_INCLUDES_PATH = f'''"{UE_Core}" "{UE_Core}/Internationalization" "{UE_Core}/Async" "{UE_Core}/Containers" "{UE_Core}/Delegates" "{UE_Core}/GenericPlatform" "{UE_Core}/HAL" "{UE_Core}/Logging" "{UE_Core}/Math" "{UE_Core}/Misc" "{UE_Core}/Modules" "{UE_Core}/Modules/Boilerplate" "{UE_Core}/ProfilingDebugging" "{UE_Core}/Serialization" "{UE_Core}/Serialization/Csv" "{UE_Core}/Stats" "{UE_Core}/Templates" "{UE_Core}/UObject" "{UE_Runtime}/CoreUObject/Public" "{UE_Project}/Interfaces" "{UE_Runtime}/TraceLog/Public"  "{UE_Runtime}/Datasmith/DatasmithCore/Public" "{UE_Runtime}/Datasmith/DirectLink/Public" "{UE_Runtime}/Messaging/Public" "{UE_Runtime}/Launch/Resources" "{UE_Engine}/Source/Developer/Datasmith/DatasmithExporter/Public" "{UE_Engine}/Source/Developer/Datasmith/DatasmithExporterUI/Public"'''

# Framework search path needed to lookup SketchUpApi and Ruby
framework_search_paths = []
framework_search_paths.append(f'{SU_SDK_PATH}')
framework_search_paths.append(f'{SU_SDK_PATH}/samples/common/ThirdParty/ruby/lib/mac')

def create_compiler_for_arch(arch):
    CXX = ['xcrun', 'clang', 
    '-x', 'objective-c++', 
    '-arch', arch,
    '-std=c++17',
    '-stdlib=libc++',
    #    '-isysroot', '/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX11.1.sdk',
    '-target', arch_target_dict[arch],
    ]


    CXX_FLAGS = []

    CXX_FLAGS += [
        '-O2', # using -Os or -O0 creates symbol relocations in .o files that fails linking SketchUp api, i.e. ld: illegal text-relocation to '_SUSceneGetName'... for architecture x86_64
    ]

    CXX_FLAGS += [
        # just adding
        '-fno-common',
        '-fmodules',
        '-fpascal-strings',
        '-fvisibility=hidden',
        '-fstrict-aliasing',
        '-fasm-blocks',
        ]
    # todo: Debug/release

    include_paths = []
    include_paths += parse_paths(UE_INCLUDES_PATH)
    include_paths.append(f'{UE_Imath}/include')
    include_paths.append(f'{SU_SDK_PATH}/samples/common/ThirdParty/ruby/include/mac')
    include_paths.append(f'{SU_SDK_PATH}/samples/common/ThirdParty/ruby/include/mac/{arch}-darwin') # todo: might not be needed - sample uses x86_64-darwin17 which doesn't exist


    definitions = [f'SKP_SDK_{SU_VERSION}', 'macintosh=1', '__register_unsupported__']
    UE_PREPROCESSOR_DEFINITIONS = 'UE_BUILD_DEVELOPMENT=1 UE_BUILD_MINIMAL=1 WITH_EDITOR=0 WITH_EDITORONLY_DATA=0 WITH_SERVER_CODE=1 WITH_ENGINE=0 WITH_UNREAL_DEVELOPER_TOOLS=0 WITH_PLUGIN_SUPPORT=0 IS_MONOLITHIC=1 IS_PROGRAM=1 PLATFORM_MAC=1 PLATFORM_APPLE=1 UE_BUILD_DEVELOPMENT_WITH_DEBUGGAME=0 UBT_COMPILED_PLATFORM=Mac CORE_API=DLLIMPORT COREUOBJECT_API=DLLIMPORT DATASMITHEXPORTER_API=DLLIMPORT DATASMITHCORE_API=DLLIMPORT DIRECTLINK_API=DLLIMPORT DATASMITHEXPORTERUI_API=DLLIMPORT'
    definitions += UE_PREPROCESSOR_DEFINITIONS.split()

    compile_cmd_template = CXX + CXX_FLAGS
    add_include_paths(compile_cmd_template, include_paths)            
    add_framework_search_paths(compile_cmd_template, framework_search_paths)
    for d in definitions:
        compile_cmd_template.append(f'-D{d}')

    return Compiler(compile_cmd_template)

arch_compiler_dict = {}

for arch in TARGET_ARCHS:

    target = arch_target_dict.get(arch, None)

    if target is None:
        log_warn('Cannot compile unknown architecture: '+ arch)
        continue

    compiler = create_compiler_for_arch(arch)
    arch_compiler_dict[arch] = compiler    

    # Compile source files
    plugin_private_dir = plugin_src_root/'Private'
    exporter_source_files = [p.parts[-1] for p in plugin_private_dir.iterdir() if p.suffix == '.cpp']

    log_debug(exporter_source_files)
    sources = {'Private': (plugin_private_dir, exporter_source_files)}
    for name, (root, files) in sources.items():
        group_obj_dir = obj_dir/arch/name	
        if not args.dry_run:
            group_obj_dir.mkdir(parents=True, exist_ok=True)
        for f in files:
            compiler.compile_source(Path(root)/f, (group_obj_dir/f).with_suffix('.o'))
    compiler.done()
########

# Link the bundle
DatasmithSDKlib = args.datasmithsdk_lib
assert os.path.isfile(DatasmithSDKlib), DatasmithSDKlib

def get_output_path(arch):
    return str(output_path/('DatasmithSketchUp.bundle')) if arch is None else str(output_path/('DatasmithSketchUp_' + arch + '.bundle'))

def link(arch, target):
    link_cmd = [
        'xcrun',
        'clang++', 
        '-target', target,
        '-Xlinker', '-export_dynamic',
        '-Xlinker', '-no_deduplicate',
        '-stdlib=libc++',
        '-demangle',
        '-dynamic',
        ]

    link_cmd.append('-bundle') # link ruby extension as a MacOs 'bundle' not dynamic lib

    #link_cmd += ['-framework', 'Ruby'] # link to RUby framework (found by framework_search_paths)

    # Handle not linking against SketchUpAPI and Ruby frameworks. Undefined will be solved at runtime
    link_cmd += [
        '-undefined', 'dynamic_lookup', # We are referencing SketchUp API symbols that are defined in the SketchUp app
        # looks like the following(referencing SketchUp app) is not needed when we have the above(-undefined dynamic_lookup)
        # Although it's useful to uncomment to catch symbols that are missing and won't be found in the SketchUp app
        #'-bundle_loader', f'/Applications/SketchUp {SU_VERSION}/SketchUp.app/Contents/MacOS/SketchUp', # this seems enough to resolve SU api symbols during linking(no need for -F)
        ]

    link_cmd += arch_compiler_dict[arch].objs
    link_cmd += [DatasmithSDKlib] # link DatasmithSDK by full path(no 'lib' prefix to use search with -l)

    link_cmd += ['-o', get_output_path(arch)]

    add_framework_search_paths(link_cmd, framework_search_paths)

    if args.verbose:
        log_info(link_cmd)
    if not args.dry_run:
        output_path.mkdir(parents=True, exist_ok=True)
        obj_dir.mkdir(parents=True, exist_ok=True)
        subprocess.check_call(link_cmd)

for arch in TARGET_ARCHS:
    target = arch_target_dict.get(arch, None)

    if target is not None:
        link(arch, target)
    else:
        log_warn('Cannot link unknown architecture: '+ arch)

if len(TARGET_ARCHS) > 1:
    bundles = []
    for arch in TARGET_ARCHS:
        bundles.append(get_output_path(arch))
    # using the lipo utility tool to merge multiple binaries into a single universal binay.
    lipo_cmd = ['lipo', '-create', '-output', get_output_path(None)] + bundles
    subprocess.check_call(lipo_cmd)

    # remove the source bundles to clear up the directory.
    for bundle in bundles:
       os.remove(bundle)
elif len(TARGET_ARCHS) == 1:
    # remove the architecture from the bundle name, only a single bundle was generated, no need to make a distinction.
    os.rename(get_output_path(TARGET_ARCHS[0]),get_output_path(None))

if args.verbose:
    log_debug('SUCCESS!')

# todo: @rpath for DatasmithSDK 
