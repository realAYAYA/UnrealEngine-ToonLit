import os
import sys
import subprocess
import argparse
import jinja2


FullFiles = [
    'net',
    'defines',
    'common',
    'login',
    'game',
    'gdd_global',
]

TsOnlyFiles = [    
]


TEMPL = """
# 签出所有可能影响到的文件 Todo 暂时不部署p4
#{{P4CMD}} edit {{CWD}}/Private/*.*
#{{P4CMD}} edit {{CWD}}/Public/*.*
#{{P4CMD}} edit {{TSDIR}}/*.*

# -----------------------------------------------------------------------------
# 需生成 .cpp/.h/.ts 完整一系列文件
{%- for file_name in FullFiles %}

# .proto 文件生成
{{PBCMD}} --cpp_out=dllexport_decl=MPROTOCOL_API:{{CWD}} --proto_path={{CWD}} {{file_name}}.proto

# Mac中脚本调用mv似不支持通配符
{{MVCMD}} {{CWD}}/{{file_name}}.pb.cc {{CWD}}/Private/
{{MVCMD}} {{CWD}}/{{file_name}}.pb.h {{CWD}}/Public/

# 生成Blueprint数据结构
{{PB2BP}} -dllexport_decl=MPROTOCOL_API -src_file={{CWD}}/{{file_name}}.proto -hpp_dst_dir={{CWD}}/Public/ -cpp_dst_dir={{CWD}}/Private/

# .ts 文件生成 Todo 暂不部署Ts
#{{NPXCMD}} protoc --ts_opt=use_proto_field_name --ts_out={{TSDIR}} --proto_path={{CWD}} {{CWD}}/{{file_name}}.proto

{%- endfor %}
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# 只需生成 .ts 文件
{%- for file_name in TsOnlyFiles %}

# .ts 文件生成 Todo 暂不部署Ts
#{{NPXCMD}} protoc --ts_opt=use_proto_field_name --ts_out={{TSDIR}} --proto_path={{CWD}} {{CWD}}/{{file_name}}.proto

{%- endfor %}
#
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# 针对特定文件的处理 Todo 待清理的功能模块
#{{GAMESTATS}} -dllexport_decl=PROTOCOL_API -src_file={{CWD}}/game_stats.proto -hpp_dst_dir={{CWD}}/Public -cpp_dst_dir={{CWD}}/Private -ts_dst_dir={{TSDIR}}
#
# -----------------------------------------------------------------------------

# 还原未修改的文件 Todo 暂时不部署p4
#{{P4CMD}} revert -a {{CWD}}/Private/...
#{{P4CMD}} revert -a {{CWD}}/Public/...
#{{P4CMD}} revert -a {{TSDIR}}/...
"""


def get_ue_platform_name():
    platform = sys.platform
    if platform == 'darwin':
        return 'Mac'
    elif platform == 'win32':
        return 'Win64'
    else:
        return 'Linux'


def main():
    platform = get_ue_platform_name()
    root_dir = f'../../../../../..'# Engine\Plugins\MyProject\Shared\MProtocol\Source
    
    project_dir = f'{root_dir}/Projects/IdleZT'# 项目位置有待重设
    ts_dir = f'{project_dir}/TypeScript/protocol'

    engine_dir = f'{root_dir}/Engine'
    pb_cmd = f'{engine_dir}/Plugins/MyProject/ZThirdParty/ZProtobuf/Source/bin/{platform}/protoc'# 插件名有待重设
    node_dir = f'{engine_dir}/Binaries/ThirdParty/Nodejs/{platform}'
    if platform == 'Win64':
        py_cmd = f'{engine_dir}/Binaries/ThirdParty/Python3/{platform}/python.exe'
        p4_cmd = f'{engine_dir}/Binaries/ThirdParty/Perforce/{platform}/p4.exe'
        mv_cmd = f'{engine_dir}/Binaries/ThirdParty/Windows/msys/mv.exe -f'
    else:
        py_cmd = f'{engine_dir}/Binaries/ThirdParty/Python3/{platform}/bin/python'
        p4_cmd = f'{engine_dir}/Binaries/ThirdParty/Perforce/{platform}/p4'
        mv_cmd = f'mv -f'
    cwd = os.getcwd()
    node_dir = os.path.abspath(cwd + '/' + node_dir)
    ts_dir = os.path.abspath(cwd + '/' + ts_dir)
    pb_cmd = os.path.abspath(cwd + '/' + pb_cmd)
    p4_cmd = os.path.abspath(cwd + '/' + p4_cmd)
    py_cmd = os.path.abspath(cwd + '/' + py_cmd)
    pb2bp_cmd = os.path.abspath(cwd + '/' + 'pb2bp.py')
    game_stats_cmd = os.path.abspath(cwd + '/' + 'game_stats_tool.py')
    npx_cmd = 'npx'

    if platform == 'Win64':
        npx_cmd = f'C:\\Windows\\System32\\cmd.exe /C {npx_cmd}'

    meta_data = {
        "CWD": cwd,
        'TSDIR': ts_dir,
        'PBCMD': pb_cmd,
        'MVCMD': mv_cmd,
        "P4CMD": p4_cmd,
        "PYCMD": py_cmd,
        'NPXCMD': npx_cmd,
        'PB2BP': f'{py_cmd} {pb2bp_cmd}',
        'GAMESTATS': f'{py_cmd} {game_stats_cmd}', 
        'FullFiles': FullFiles,
        'TsOnlyFiles': TsOnlyFiles,
    }

    try:
        content = jinja2.Template(TEMPL).render(meta_data)
    except Exception as render_err:
        raise Exception(f'TEMPL 文件渲染失败\n{render_err}')

    my_env = os.environ.copy()
    my_env['P4CONFIG'] = '.p4config'
    for line in content.splitlines():
        line = line.strip().strip('\n').strip().strip('\n')
        if not line.startswith('#') and len(line) > 0:
            try:
                print(line)
                ret = subprocess.run(line.split(' '), env=my_env, cwd=node_dir)
                if ret.returncode != 0:
                    raise Exception(f'code={ret.returncode}')
            except Exception as run_err:
                raise Exception(f'\n指令执行失败\ncmd\n{line}\n{run_err} ')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-log_file', nargs='?', const='none', help='日志文件')
    args = parser.parse_args()

    try:
        main()
    except Exception as e:
        text = f'ERROR {e}'
        if args.log_file is not None:
            with open(args.log_file, 'wb') as f:
                f.write(text.encode('utf-8'))
        print(text)
        exit(1)
    print(f'DONE.')
    exit(0)
