#!/usr/bin/env python3
# -*- mode: python; coding: utf-8-unix; -*-
# coding: utf-8

import argparse
import os
import json
import pathlib

import jinja2
import xlrd

from collections import OrderedDict

XLS_VAR_NAME_ROW_IDX = 0  # 变量名行
XLS_VAR_TYPE_ROW_IDX = 1  # 变量类型行
XLS_SHOW_NAME_ROW_IDX = 2  # 显示名称行
XLS_DATA_START_ROW_IDX = 3  # 数据起始行 

ENTRY_TEMPL = """import * as UE from "ue";
// =============================================================================
{{custom_code1_begin}}
{{custom_code1_content}}
{{custom_code1_end}}
// =============================================================================

class Ts{{class_name}} extends UE.Object {
{%- for def in defines %}

    /** {{def['show_name']}} */
{%- if def['var_is_array'] %}
    {{def['var_name']}}: UE.TArray<{{def['var_type']}} {%- if def['var_type'] != def['var_cpp_type']  -%}/*@cpp:{{def['var_cpp_type']}}*/{%- endif -%} >;
{%- else %}
    {%- if def['var_cpp_type'] != ''  %}
    //@cpp:{{def['var_cpp_type']}}
    {%- endif %}    
    {{def['var_name']}}: {{def['var_type']}};
{%- endif %}
{%- endfor %}

    // =========================================================
    {{custom_code2_begin}}
{{custom_code2_content}}
    {{custom_code2_end}}
    // =========================================================
    
    InitData(): boolean {
        // =========================================================
        {{custom_code3_begin}}
{{custom_code3_content}}
        {{custom_code3_end}}
        // =========================================================    
        return true
    }    
}

export default Ts{{class_name}}
"""

LIB_TEMPL = """import * as UE from "ue";
import * as fs from '../tools/fs'
// =============================================================================
{{custom_code1_begin}}
{{custom_code1_content}}
{{custom_code1_end}}
// =============================================================================

export type EntryType = UE.Game.Blueprints.TypeScript.game_tables.Ts{{class_name}}.Ts{{class_name}}_C
const entryUeClassAssetPath: string = '/Game/Blueprints/TypeScript/game_tables/Ts{{class_name}}.Ts{{class_name}}_C'
const dataFileName: string = '{{base_name}}.jsondata'

let allDataMap = new Map<number, EntryType>()
let initDataDone = false

function InitNewEntryData(ueEntry: EntryType, rawObj: any): boolean {
{%- for def in defines %}
    {  // {{def['var_name']}}
        {%- if def['var_is_array'] %}
        ueEntry.{{def['var_name']}}.Empty()
        {%- endif %}
        if (rawObj.{{def['var_name']}} != undefined) {
            {%- if def['var_is_soft_class'] %}
            let softClassPath = UE.KismetSystemLibrary.MakeSoftClassPath(rawObj.{{def['var_name']}})
            // @ts-ignore
            ueEntry.{{def['var_name']}} = UE.KismetSystemLibrary.Conv_SoftClassPathToSoftClassRef(softClassPath)
            {%- elif def['var_is_soft_object'] %}
            let softObjectPath = UE.KismetSystemLibrary.MakeSoftObjectPath(rawObj.{{def['var_name']}})
            // @ts-ignore
            ueEntry.{{def['var_name']}} = UE.KismetSystemLibrary.Conv_SoftObjPathToSoftObjRef(softObjectPath)            
            {%- else %}
            {%- if def['var_is_array'] %}
            for (let idx in rawObj.{{def['var_name']}}) {
                ueEntry.{{def['var_name']}}.Add(rawObj.{{def['var_name']}}[idx])
            }
            {%- else %}
            ueEntry.{{def['var_name']}} = rawObj.{{def['var_name']}}
            {%- endif %}
            {%- endif %}
        } else {
            {%- if def['var_is_array'] %}
            {%- else %}
            {%- if def['var_type'] == 'string'  %}
            ueEntry.{{def['var_name']}} = ''
            {%- elif def['var_type'] == 'number'  %}
            ueEntry.{{def['var_name']}} = 0
            {%- endif %}
            {%- endif %}
        }
    }
{%- endfor %}
    ueEntry.InitData()
    return true
}

function TryLoadData(): void {
    if (initDataDone) {
        return
    }
    initDataDone = true

    allDataMap.clear()
    
    let entryUeClass = UE.Class.Load(entryUeClassAssetPath)
    
    let gddDir = fs.combinePaths(fs.getProjectContentFullDir(), 'GDD')
    let filePath = fs.combinePaths(gddDir, dataFileName)
    let fileContent = fs.readFile(filePath, 'utf8')
    
    let dataArray = JSON.parse(fileContent)
    for (let idx in dataArray) {
        let rawObj = dataArray[idx]
        let ueEntry = UE.ZExtensionLibrary.ZJsNewPermanentObject(entryUeClass) as EntryType
        if (InitNewEntryData(ueEntry, rawObj)) {
            allDataMap.set(ueEntry.{{key_field_name}}, ueEntry)
        }
    }
    
    // =========================================================
    {{custom_code2_begin}}
{{custom_code2_content}}
    {{custom_code2_end}}
    // =========================================================    
    console.log("Init{{base_name}}", dataArray.length)            
}


export function Find{{table_name}}(in{{key_field_name}}: {{key_field_type}} {%- if key_field_type != key_field_cpp_type  -%}/*@cpp:{{key_field_cpp_type}}*/{%- endif -%}): EntryType {
    TryLoadData()
    return allDataMap.get(in{{key_field_name}})    
} 

export function Foreach{{table_name}}(inCallback: (entry: EntryType) => boolean) {
    TryLoadData()
    for (let [key, value] of allDataMap) {
        if (!inCallback(value)) {
            break
        }        
    }
}

// =========================================================
{{custom_code3_begin}}
{{custom_code3_content}}
{{custom_code3_end}}
// =========================================================

class Ts{{lib_name}} extends UE.BlueprintFunctionLibrary {

    public static Find{{table_name}}(In: {{key_field_type}} {%- if key_field_type != key_field_cpp_type  -%}/*@cpp:{{key_field_cpp_type}}*/{%- endif -%}): EntryType {
        return Find{{table_name}}(In)
    }

    public static Get{{table_name}}Keys(): UE.TArray<{{key_field_type}} {%- if key_field_type != key_field_cpp_type  -%}/*@cpp:{{key_field_cpp_type}}*/{%- endif -%}> {
        let Out = UE.NewArray(UE.BuiltinInt)
        Foreach{{table_name}}((entry: EntryType): boolean => {
            Out.Add(entry.{{key_field_name}})
            return true
        })
        return Out
    }

    // =========================================================
    {{custom_code4_begin}}
{{custom_code4_content}}
    {{custom_code4_end}}
    // =========================================================
};

export default Ts{{lib_name}}
"""

def parse_custom_code(file_path, begin_str, end_str):
    content = None
    with open(file_path, encoding='utf-8') as f:
        for line in f.readlines():
            data = line.strip('\n').strip().strip('\n').strip()
            if content is None:
                if data == begin_str:
                    content = ''
                    continue
            else:
                if data == end_str:
                    if len(content) > 0 and content[-1] == '\n':
                        content = content[0:len(content) - 1]
                    return content
            if content is not None:
                content += line
    return ''


def generate_entry_code(args, class_name, manager_name, meta_data):
    ts_file = f'{args.ts_dst_dir}/{args.entry_code_name}.ts'

    custom_code1_begin = f'// *** BEGIN WRITING YOUR CODE - ***'
    custom_code1_end = f'// *** END WRITING YOUR CODE - ***'

    custom_code2_begin = f'// *** BEGIN WRITING YOUR CODE _ ***'
    custom_code2_end = f'// *** END WRITING YOUR CODE _  ***'

    custom_code3_begin = f'// *** BEGIN WRITING YOUR CODE . ***'
    custom_code3_end = f'// *** END WRITING YOUR CODE .  ***'

    custom_code1_content = ''
    custom_code2_content = ''
    custom_code3_content = ''

    try:
        if os.path.exists(ts_file):
            custom_code1_content = parse_custom_code(ts_file, custom_code1_begin, custom_code1_end)
            custom_code2_content = parse_custom_code(ts_file, custom_code2_begin, custom_code2_end)
            custom_code3_content = parse_custom_code(ts_file, custom_code3_begin, custom_code3_end)
    except Exception as parse_err:
        raise Exception(f'ParseHppError {parse_err}')

    meta_data['custom_code1_begin'] = custom_code1_begin
    meta_data['custom_code1_end'] = custom_code1_end
    meta_data['custom_code1_content'] = custom_code1_content

    meta_data['custom_code2_begin'] = custom_code2_begin
    meta_data['custom_code2_end'] = custom_code2_end
    meta_data['custom_code2_content'] = custom_code2_content

    meta_data['custom_code3_begin'] = custom_code3_begin
    meta_data['custom_code3_end'] = custom_code3_end
    meta_data['custom_code3_content'] = custom_code3_content

    try:
        hpp_content = jinja2.Template(ENTRY_TEMPL).render(meta_data)
    except Exception as hpp_err:
        raise Exception('ENTRY_TEMPL 文件渲染失败 {}'.format(hpp_err))

    # 写入磁盘
    try:
        with open(ts_file, 'wb') as f:
            f.write(hpp_content.encode('utf-8'))
    except Exception as write_err:
        raise Exception('文件写入失败 {}'.format(write_err))


def generate_lib_code(args, class_name, manager_name, lib_name, meta_data):
    ts_file = f'{args.ts_dst_dir}/{args.lib_code_name}.ts'

    custom_code1_begin = f'// *** BEGIN WRITING YOUR CODE - ***'
    custom_code1_end = f'// *** END WRITING YOUR CODE - ***'

    custom_code2_begin = f'// *** BEGIN WRITING YOUR CODE _ ***'
    custom_code2_end = f'// *** END WRITING YOUR CODE _  ***'

    custom_code3_begin = f'// *** BEGIN WRITING YOUR CODE . ***'
    custom_code3_end = f'// *** END WRITING YOUR CODE .  ***'

    custom_code4_begin = f'// *** BEGIN WRITING YOUR CODE ^ ***'
    custom_code4_end = f'// *** END WRITING YOUR CODE ^  ***'

    custom_code1_content = ''
    custom_code2_content = ''
    custom_code3_content = ''
    custom_code4_content = ''

    try:
        if os.path.exists(ts_file):
            custom_code1_content = parse_custom_code(ts_file, custom_code1_begin, custom_code1_end)
            custom_code2_content = parse_custom_code(ts_file, custom_code2_begin, custom_code2_end)
            custom_code3_content = parse_custom_code(ts_file, custom_code3_begin, custom_code3_end)
            custom_code4_content = parse_custom_code(ts_file, custom_code4_begin, custom_code4_end)
    except Exception as parse_err:
        raise Exception(f'ParseHppError {parse_err}')

    meta_data['custom_code1_begin'] = custom_code1_begin
    meta_data['custom_code1_end'] = custom_code1_end
    meta_data['custom_code1_content'] = custom_code1_content

    meta_data['custom_code2_begin'] = custom_code2_begin
    meta_data['custom_code2_end'] = custom_code2_end
    meta_data['custom_code2_content'] = custom_code2_content

    meta_data['custom_code3_begin'] = custom_code3_begin
    meta_data['custom_code3_end'] = custom_code3_end
    meta_data['custom_code3_content'] = custom_code3_content

    meta_data['custom_code4_begin'] = custom_code4_begin
    meta_data['custom_code4_end'] = custom_code4_end
    meta_data['custom_code4_content'] = custom_code4_content

    try:
        hpp_content = jinja2.Template(LIB_TEMPL).render(meta_data)
    except Exception as hpp_err:
        raise Exception('ENTRY_TEMPL 文件渲染失败 {}'.format(hpp_err))

    # 写入磁盘
    try:
        with open(ts_file, 'wb') as f:
            f.write(hpp_content.encode('utf-8'))
    except Exception as write_err:
        raise Exception('文件写入失败 {}'.format(write_err))


# 生成代码文件
def generate_code(args, defines: list):
    table_name = args.table_name
    base_name = f'{args.code_prefix}{args.pure_name}'
    struct_name = f'{base_name}Row'
    class_name = f'{table_name}Entry'
    manager_name = f'{table_name}'
    lib_name = f'{table_name}FunctionLibrary'
    key_field_name = defines[0].get('var_name')
    key_field_type = defines[0].get('var_type')
    key_field_cpp_type = defines[0].get('var_cpp_type')
    meta_data = {
        'code_prefix': args.code_prefix,
        'dllexport_decl': args.dllexport_decl,
        'pure_name': args.pure_name,
        'code_file_base_name': args.code_file_base_name,
        'table_name': args.table_name,
        'base_name': base_name,
        'struct_name': struct_name,
        'class_name': class_name,
        'manager_name': manager_name,
        'lib_name': lib_name,
        'defines': defines,
        'key_field_name': key_field_name,
        'key_field_type': key_field_type,
        'key_field_cpp_type': key_field_cpp_type,
    }
    
    generate_entry_code(args, class_name, manager_name, meta_data)
    
    if args.enable_lib_code:
        generate_lib_code(args, class_name, manager_name, lib_name, meta_data)


# 生成数据文件
def generate_data(args, all_data: list):
    data_content = json.dumps(all_data, ensure_ascii=False, sort_keys=False, indent=4)
    data_file_path = f'{args.data_dst_dir}/{args.pure_name}.jsondata'
    try:
        with open(data_file_path, 'wb') as f:
            f.write(data_content.encode('utf-8'))
    except Exception as e:
        raise Exception('文件写入失败 {}'.format(e))


# 处理类型
def process_type(args, data_cell: xlrd.sheet.Cell):
    value = str(data_cell.value).strip()
    ts_type_name = value
    delimiter_char = None
    delimiter_pos = ts_type_name.find('[')
    if delimiter_pos != -1:
        delimiter_char = ts_type_name[delimiter_pos:].strip('[').strip(']').strip()
        ts_type_name = ts_type_name[:delimiter_pos]

    row_struct_name = ''
    custom_default_value = ''

    data = ts_type_name.split('|')
    raw_name = data[0].strip()
    ts_type_name = raw_name  # TS 类型
    cpp_type_name = ''  # 蓝图类型

    is_soft_class = False
    is_soft_object = False

    if raw_name == 'string':
        ts_type_name = 'string'
    elif raw_name == 'name':
        ts_type_name = 'string'
        cpp_type_name = 'name'
    elif raw_name == 'text':
        ts_type_name = 'string'
        cpp_type_name = 'text'
    elif raw_name == 'int' or raw_name == 'integer' or raw_name == 'int64':
        ts_type_name = 'number'
        cpp_type_name = 'int'
    elif raw_name == 'float' or raw_name == 'number':
        ts_type_name = 'number'
        cpp_type_name = 'float'
    elif raw_name == 'bool' or raw_name == 'boolean':
        ts_type_name = 'boolean'
        cpp_type_name = 'bool'
    elif raw_name.startswith('TSoft'):  # 软引用
        # 例：
        #   TSoftClassPtr<AActor>  --> UE.TSoftClassPtr<UE.Actor>
        #   TSoftObjectPtr<UTexture2D> --> UE.TSoftObjectPtr<UE.Texture2D>
        is_soft_class = raw_name.startswith('TSoftClass')
        is_soft_object = raw_name.startswith('TSoftObject')        
        old_name = raw_name[raw_name.find('<')+1: raw_name.find('>')]
        new_name = 'UE.' + raw_name[raw_name.find('<') + 2: raw_name.find('>')]        
        ts_type_name = raw_name.replace(old_name, new_name).replace('TSoft', 'UE.TSoft')
    else:
        if raw_name.startswith('E'):    
            if raw_name.startswith('EZ'):
                name = 'E' + raw_name.strip('EZ') 
            else:
                name = raw_name
            full_name = args.typing_lines.get(name)
            # print(raw_name, name, full_name)
            if full_name is not None:
                ts_type_name = full_name
            else:
                ts_type_name = 'UE.' + raw_name            
        
        # is_cpp_enum = raw_name.startswith('E')  # C++枚举
        # if is_cpp_enum:
        #     ts_type_name = 'UE.' + raw_name 
        # else:
        #     ts_type_name = 'UE.Game.Blueprints.TypeScript.protocol.' + raw_name + '.' + raw_name 

    # print(f'{ts_type_name} {cpp_type_name}')

    need_init = False
    if ts_type_name.startswith('int') or ts_type_name == 'float' or ts_type_name == 'bool' or ts_type_name.startswith('E'):
        need_init = True

    if len(custom_default_value) > 0:
        need_init = False

    return {
        'raw_name': raw_name,
        'type_name': ts_type_name,
        'cpp_type_name': cpp_type_name,
        'is_array': (delimiter_char is not None),
        'is_soft_class': is_soft_class,
        'is_soft_object': is_soft_object,
        'delimiter': (delimiter_char or ''),
        'need_init': need_init,
        'row_struct_name': row_struct_name,
        'custom_default_value': custom_default_value,
    }


def cast_to_int(value):
    try:
        return int(value or 0)
    except:
        return int(float(value or 0))

def cast_data_by_type(value, type):
    if type.startswith('int'):
        return cast_to_int(value)
    elif type.startswith('float') or type.startswith('double'):
        return float(value or 0.0)
    elif type.startswith('E'):  # Unreal 中的枚举类型肯定用 E 开头
        return cast_to_int(value)
    return str(value or '')


# 处理值
def process_value(data_cell: xlrd.sheet.Cell, define):
    value = data_cell.value

    def_var_type = define.get('var_cpp_type')
    if def_var_type == '':
        define.get('var_type')
    
    if define.get('var_is_array'):
        value = str(value or '')
        if len(value) == 0:
            return []
        cont = value.split(define.get('var_delimiter'))
        for x in range(len(cont)):
            cont[x] = cast_data_by_type(cont[x], def_var_type)
        return cont

    return cast_data_by_type(value, def_var_type)


def parse_header(args, data_sheet: xlrd.sheet.Sheet) -> list:
    table_define = list()
    for col_idx in range(data_sheet.ncols):
        col = data_sheet.col(col_idx)
        if len(col) < XLS_DATA_START_ROW_IDX:
            continue
        var_name = col[XLS_VAR_NAME_ROW_IDX].value
        try:
            type_data = process_type(args, col[XLS_VAR_TYPE_ROW_IDX])
            var_type = type_data['type_name']
            var_cpp_type = type_data['cpp_type_name']
            var_is_array = type_data['is_array']
            var_is_soft_object = type_data['is_soft_object']
            var_is_soft_class = type_data['is_soft_class']
            var_delimiter = type_data['delimiter']
            var_need_init = type_data['need_init']
            var_row_struct_name = type_data['row_struct_name']
            var_custom_default_value = type_data['custom_default_value']
        except Exception as e:
            raise Exception(f'处理类型错误 col={col_idx + 1} {e}')
        show_name = col[XLS_SHOW_NAME_ROW_IDX].value
        if var_name.startswith('#') or len(var_name) == 0 or len(var_type) == 0:
            continue  # 变量名或类型为空，或者变量名以#号开头，跳过
        table_define.append({
            'col_idx': col_idx,
            'var_name': var_name,
            'var_type': var_type,
            'var_cpp_type': var_cpp_type,
            'var_is_array': var_is_array,
            'var_is_soft_object': var_is_soft_object,
            'var_is_soft_class': var_is_soft_class,
            'var_delimiter': var_delimiter,
            'var_need_init': var_need_init,
            'var_row_struct_name': var_row_struct_name,
            'var_need_row_struct_name': (len(var_row_struct_name) > 0),
            'var_custom_default_value': var_custom_default_value,
            'var_need_custom_default_value': (len(var_custom_default_value) > 0),
            'show_name': show_name,
        })
    return table_define


def parse_data(data_sheet: xlrd.sheet.Sheet, defines: list) -> list:
    all_data = list()
    for row_idx in range(XLS_DATA_START_ROW_IDX, data_sheet.nrows):
        row = data_sheet.row(row_idx)
        data = OrderedDict()
        for define in defines:
            def_col_idx = define.get('col_idx')
            def_var_name = define.get('var_name')
            def_var_type = define.get('var_type')
            try:
                cell = row[def_col_idx]
                data[def_var_name] = process_value(cell, define)
            except Exception as e:
                raise Exception(f'处理数据时发生错误 row={row_idx + 1} col={def_col_idx + 1} {e}')
        all_data.append(data)
    return all_data


def print_table_define(define):
    for data in define:
        print(f'name={data.get("var_name")}({data.get("show_name")}) type={data.get("var_type")}')


def print_table_data(data):
    for entry in data:
        print(entry)


def process_xls(args):
    book = xlrd.open_workbook(args.src_file)
    if book.nsheets == 0:
        raise Exception('xls文件为空')
    sheet = book.sheet_by_index(0)  # 选取第一张表为数据表
    # print(f'sheet name={sheet.name} nrows={sheet.nrows} ncols={sheet.ncols}')

    defines = parse_header(args, sheet)
    # print_table_define(defines)
    if len(defines) == 0:
        raise Exception('未找到表格定义信息(注意：数据页必须是第一页')

    all_data = parse_data(sheet, defines)
    # print_table_data(all_data)

    if args.enable_code:
        generate_code(args, defines)

    if args.enable_data:
        generate_data(args, all_data)


def check_ts_typing_exist(args):
    with open(f'{args.ts_typing_dir}/ue/ue_bp.d.ts', encoding='utf-8') as f:
        for line in f.readlines():
            if line.find(f'{args.entry_code_name}') != -1:
                return True
    return False


def main(args):
    if args.src_file is None:
        raise Exception(f'未指定源文件')

    if not os.path.exists(args.src_file):
        raise Exception(f'源文件非法 {args.src_file}')

    args.enable_code = False
    if args.ts_dst_dir:
        args.enable_code = True
        if not os.path.exists(args.ts_dst_dir or ''):
            raise Exception(f'TS文件输出目录非法 {args.ts_dst_dir}')
        
    args.enable_data = False
    if args.data_dst_dir:
        args.enable_data = True
        if not os.path.exists(args.data_dst_dir):
            raise Exception(f'ERROR: 数据文件输出目录非法 {args.data_dst_dir}')

    args.pure_name = pathlib.Path(args.src_file).stem  # '/home/hudawei/Item.xlsx' -> 'Item'

    if args.code_prefix is None:
        args.code_prefix = ''

    prefix = args.code_prefix
    suffix = 'Table'
    if args.pure_name.startswith(prefix):
        prefix = ''
    if args.pure_name.endswith(suffix):
        suffix = ''
    args.table_name = f'{args.pure_name}{suffix}'  # ItemTable
    args.code_file_base_name = f'{prefix}{args.pure_name}{suffix}'  # ZItemTable

    args.entry_code_name = f'Ts{args.table_name}Entry'
    args.lib_code_name = f'Ts{args.table_name}FunctionLibrary'

    args.enable_lib_code = False
    if args.enable_code:
        if args.ts_typing_dir:
            args.enable_lib_code = check_ts_typing_exist(args)
            args.typing_lines = dict()
            #with open(f'{args.ts_typing_dir}/ue/zprotocol.d.ts', encoding='utf-8') as f:
            #    for line in f.readlines():
            #        line = line.strip()
            #        if line.startswith('class ') or line.startswith('enum '):
            #            name = line.split(' ')[1]
            #            args.typing_lines[name] = 'UE.' + name
            #            # print(name, args.typing_lines[name])
            with open(f'{args.ts_typing_dir}/ue/ue.d.ts', encoding='utf-8') as f:
                for line in f.readlines():
                    line = line.strip()
                    if line.startswith('class ') or line.startswith('enum '):
                        name = line.split(' ')[1]
                        args.typing_lines[name] = 'UE.' + name
                        # print(name, args.typing_lines[name])
            with open(f'{args.ts_typing_dir}/ue/ue_s.d.ts', encoding='utf-8') as f:
                for line in f.readlines():
                    line = line.strip()
                    if line.startswith('class ') or line.startswith('enum '):
                        name = line.split(' ')[1]
                        args.typing_lines[name] = 'UE.' + name
                        # print(name, args.typing_lines[name])
            with open(f'{args.ts_typing_dir}/ue/ue_bp.d.ts', encoding='utf-8') as f:
                for line in f.readlines():
                    line = line.strip()
                    if line.startswith('namespace '):
                        full_name = line.split(' ')[1]                        
                        name = full_name.split('.')[-1]
                        args.typing_lines[name] = 'UE.' + full_name + '.' + name
                        # print(name, args.typing_lines[name])
            if not args.enable_lib_code:
                print(f'未找到类型定义，本次不生成相应蓝图库。请点击编辑器主工具栏的 puerts 按钮，重新生成 TS 定义文件后再试')
    process_xls(args)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-src_file', nargs='?', const='none', help='源文件')
    parser.add_argument('-ts_dst_dir', nargs='?', const='none', help='TS文件目标目录')
    parser.add_argument('-ts_typing_dir', nargs='?', const='none', help='TS定义目录')
    parser.add_argument('-dllexport_decl', nargs='?', const='none', default='', help='DLL导出宏')
    parser.add_argument('-data_dst_dir', nargs='?', const='none', help='数据文件目标目录')
    parser.add_argument('-code_prefix', nargs='?', const='none', help='代码名称前缀')
    cmd_args = parser.parse_args()

    try:
        main(cmd_args)
    except Exception as e:
        print(f'{cmd_args.src_file} ERROR! {e}')
        exit(1)
    print(f'{cmd_args.src_file} DONE.')
    exit(0)
