#!/usr/bin/env python3
# -*- mode: python; coding: utf-8-unix; -*-
# coding: utf-8

import subprocess
import argparse
import os
import sys
import copy
import pathlib
import collections
import jinja2
import fnv1a

from collections import OrderedDict

STUB_HPP_TEMPL = """// Code generated - DO NOT EDIT.
import * as UE from 'ue'
import {argv, blueprint} from 'puerts'
import * as tools from 'tools'
{%- for entry in import_pb_dict %}
import * as {{entry}} from 'protocol/{{entry}}'
{%- endfor %}

{% for rpc_entry in rpc_list %}
/**
{%- for comment in rpc_entry['comment'] %}
 * {{comment}}
{%- endfor %}
*/
export function {{rpc_entry['name']}}(req: {{type_dict[rpc_entry['p1']].pb_file_name}}.{{rpc_entry['p1']}}, ackHandle: (ack: {{type_dict[rpc_entry['p2']].pb_file_name}}.{{rpc_entry['p2']}}) => void) {
    let rpcId = 0x{{rpc_entry['id']}}n
    let messageId = tools.fnv64({{type_dict[rpc_entry['p1']].pb_file_name}}.{{rpc_entry['p1']}}.typeName)
    let messageBinary = {{type_dict[rpc_entry['p1']].pb_file_name}}.{{rpc_entry['p1']}}.toBinary(req)
    let stub = UE.NewObject(UE.ZTsRpcStub.StaticClass(), argv.getByName("GameInstance")) as UE.ZTsRpcStub
    stub.AckCallback.Bind((ok: boolean, buffer: Uint8Array): void => {
        let message = {{type_dict[rpc_entry['p2']].pb_file_name}}.{{rpc_entry['p2']}}.fromBinary(new Uint8Array(buffer))
        if (message) {
            if (ackHandle) {
                ackHandle(message)
            }
        } else {
            throw new Error("{{type_dict[rpc_entry['p2']].pb_file_name}}.{{rpc_entry['p2']}}.fromBinary FAILED")
        }
    })
    let jsSubsystem = tools.GetJavaScriptSubsystem()
    jsSubsystem.SendRpc(rpcId, messageId, messageBinary, stub)
    return stub
}
{% endfor %}

{% for notify_entry in notify_list %}
/**
{%- for comment in notify_entry['comment'] %}
 * {{comment}}
{%- endfor %}
*/
export function Bind{{notify_entry['name']}}(ackHandle: (ack: {{type_dict[notify_entry['p1']].pb_file_name}}.{{notify_entry['p1']}}) => void) {
    let messageId = tools.fnv64({{type_dict[notify_entry['p1']].pb_file_name}}.{{notify_entry['p1']}}.typeName)
    let stub = UE.NewObject(UE.ZTsRpcStub.StaticClass(), argv.getByName("GameInstance")) as UE.ZTsRpcStub
    stub.AckCallback.Bind((ok: boolean, buffer: Uint8Array): void => {
        let message = {{type_dict[notify_entry['p1']].pb_file_name}}.{{notify_entry['p1']}}.fromBinary(new Uint8Array(buffer))
        if (message) {
            if (ackHandle) {
                ackHandle(message)
            }
        } else {
            throw new Error("{{type_dict[notify_entry['p1']].pb_file_name}}.{{notify_entry['p1']}}.fromBinary FAILED")            
        }
    })
    let jsSubsystem = tools.GetJavaScriptSubsystem()
    jsSubsystem.RegisterNotify(messageId, stub)
    return stub
}
{% endfor %}


"""


def record_type(all_pb_types, types, s):
    s = s.strip('::')
    if s == 'EMPTY':
        return
    
    space_name = ''
    type_name = ''
    data = s.split('::')    
    if len(data) >= 2:
        space_name = data[0]
        type_name = data[1]
    elif len(data) == 1:
        type_name = data[0]
    
    pb_file_name = all_pb_types.get(type_name) 
    if pb_file_name is not None:
        types[type_name] = {'space_name': space_name, 'type_name': type_name, 'pb_file_name': pb_file_name}


def main(src_file, ts_dst_dir, pb_src_dir):
    if ts_dst_dir[-1] != '/':
        ts_dst_dir += '/'

    if pb_src_dir[-1] != '/':
        pb_src_dir += '/'
    
    import_pb_dict = OrderedDict()
    
    all_pb_types = dict()
    pb_src_path = pathlib.Path(pb_src_dir)
    for pb_file_path in pb_src_path.glob('*.proto'):
        pb_file_name = pb_file_path.stem
        with open(pb_file_path, encoding='utf-8') as f:
            for line in f.readlines():
                data = line.strip().split()
                if len(data) > 1:
                    if data[0] == 'message':
                        all_pb_types[data[1]] = pb_file_name
                        import_pb_dict[pb_file_name] = 1 

    src_file_path = pathlib.Path(src_file)

    service_name = src_file_path.stem
    
    rpcs = list()
    notifys = list()
    includes = list()
    type_dict = dict()
    rpc_space_name = ''

    fnv = fnv1a.FNV1a()

    last_comment = list()
    with open(src_file_path, 'r', encoding='utf-8') as f:
        line_num = 0
        for line in f.readlines():
            line_num += 1
            line = line.strip().strip('\n').strip('\r')
            line = ' '.join(line.split())
            if line.startswith('#'):  # 以#号开头是注释
                last_comment.append(line.lstrip('#').strip())
                continue
            data = line.split(' ')
            op = data[0]
            if op == 'rpc':  # rpc函数
                if len(data) >= 4:
                    rpc = dict()
                    rpc['id'] = fnv.hash(data[1])
                    rpc['name'] = data[1]  # rpc函数名称
                    rpc['p1'] = 'EMPTY'  # 请求参数
                    rpc['p2'] = 'EMPTY'  # 返回参数
                    rpc['p1'] = data[2]
                    rpc['p2'] = data[3]
                    record_type(all_pb_types, type_dict, data[2])
                    record_type(all_pb_types, type_dict, data[3])
                    rpc['comment'] = copy.copy(last_comment)  # 注释
                    rpcs.append(rpc)
                else:
                    raise Exception(f'rpc格式错误，缺少必要字段！ 行号:{line_num}')
            elif op == 'send' or op == 'result':
                if len(data) >= 2:
                    notify = dict()
                    notify['name'] = data[1]
                    notify['p1'] = data[1]
                    record_type(all_pb_types, type_dict, data[1])
                    notify['comment'] = copy.copy(last_comment)  # 注释
                    if op == 'send':
                        notify['type'] = 1
                    else:
                        notify['type'] = 2
                    notifys.append(notify)
                else:
                    raise Exception(f'rpc格式错误，缺少必要字段！ 行号:{line_num}')
            elif op == 'include':  # include文件列表
                for e in data[1:]:
                    includes.append(e)
            elif op == "namespace":  # 名字空间
                rpc_space_name = data[1]
            last_comment.clear()
                
    # type_list = list()
    # for k in type_dict:
    #     type_list.append(type_dict[k])

    meta_data = {
        'service_name': service_name,
        'rpc_space_name': rpc_space_name,
        'include_list': includes,
        'rpc_list': rpcs,
        'notify_list': notifys,
        'type_dict': type_dict,
        'import_pb_dict': import_pb_dict,
    }

    try:
        stub_hpp_content = jinja2.Template(STUB_HPP_TEMPL).render(meta_data)
    except Exception as e:
        raise Exception('STUB_HPP_TEMPL 文件渲染失败 {}'.format(e))
    
    stub_hpp_file_path = ts_dst_dir + service_name + '.ts'
    
    # 写入磁盘
    try:
        with open(stub_hpp_file_path, 'wb') as f:
            f.write(stub_hpp_content.encode('utf-8'))
    except Exception as e:
        raise Exception('文件写入失败 {}'.format(e))    
    

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-src_file', nargs='?', const='none', help='源文件')
    parser.add_argument('-ts_dst_dir', nargs='?', const='none', help='TS文件目标目录')
    parser.add_argument('-pb_src_dir', nargs='?', const='none', help='PB文件目录')
    args = parser.parse_args()

    if args.src_file is None:
        print('ERROR: 未指定源文件')
        exit(1)

    if not os.path.exists(args.src_file):
        print('ERROR: 源文件非法! {}'.format(args.src_file))
        exit(1)

    if not os.path.exists(args.ts_dst_dir):
        print('ERROR: TS文件输出目录非法! {}'.format(args.ts_dst_dir))
        exit(1)

    if not os.path.exists(args.pb_src_dir):
        print('ERROR: PB文件目录非法! {}'.format(args.pb_src_dir))
        exit(1)

    try:
        main(args.src_file, args.ts_dst_dir, args.pb_src_dir)
    except Exception as e:
        print('处理失败({}): {}'.format(args.src_file, e))
        exit(1)
    exit(0)
    
