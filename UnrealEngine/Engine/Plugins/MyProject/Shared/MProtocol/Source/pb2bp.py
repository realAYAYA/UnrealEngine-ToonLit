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

# xxxx.h
HPP_TEMPL = """#pragma once
#include "ZFmt.h"
{%- for entry in import_file_list %}
#include "{{entry}}.h"
{%- endfor %}
#include "{{to_file_name}}.generated.h"

{% for def_entry in def_list %}
{%- if def_entry['type'] != "enum" %}
namespace {{package_name}} {
class {{def_entry['old_name']}};
}  // namespace {{package_name}}
{%- endif %}

/**
{%- for comment in def_entry['comment'] %}
 * {{comment}}
{%- endfor %}
*/
{%- if def_entry['type'] == "enum" %}
{%- if def_entry['no_blueprint'] %}
UENUM()
enum class {{def_entry['name']}} : uint16
{%- else %}
UENUM(BlueprintType)
enum class {{def_entry['name']}} : uint8
{%- endif %}
{
{%- for member in def_entry['members'] %}
    {{member['name']}} = {{member['index']}} UMETA(DisplayName="{{member['comment']}}"),
{%- endfor %}
};
{%- set first_enum = def_entry['members'] | first %}
{%- set last_enum = def_entry['members'] | last %}
constexpr {{def_entry['name']}} {{def_entry['name']}}_Min = {{def_entry['name']}}::{{first_enum['name']}};
constexpr {{def_entry['name']}} {{def_entry['name']}}_Max = {{def_entry['name']}}::{{last_enum['name']}};
constexpr int32 {{def_entry['name']}}_ArraySize = static_cast<int32>({{def_entry['name']}}_Max) + 1;
{{dllexport_decl}} bool Check{{def_entry['name']}}Valid(int32 Val);
{{dllexport_decl}} const TCHAR* Get{{def_entry['name']}}Description({{def_entry['name']}} Val);

template <typename Char>
struct fmt::formatter<{{def_entry['name']}}, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const {{def_entry['name']}}& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};

{%- else %}
USTRUCT(BlueprintType)
struct {{dllexport_decl}} {{def_entry['name']}}
{
    GENERATED_BODY();
{%- for member in def_entry['members'] %}

    /** {{member['comment']}} */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    {{member['type']}} {{member['name']}};
{%- endfor %}


    {{def_entry['name']}}();
    {{def_entry['name']}}(const {{package_name}}::{{def_entry['old_name']}}& Right);
    void FromPb(const {{package_name}}::{{def_entry['old_name']}}& Right);
    void ToPb({{package_name}}::{{def_entry['old_name']}}* Out) const;
    void Reset();
    void operator=(const {{package_name}}::{{def_entry['old_name']}}& Right);
    bool operator==(const {{def_entry['name']}}& Right) const;
    bool operator!=(const {{def_entry['name']}}& Right) const;
    
    {%- if def_entry['generated_member_ptr'] == 1 %}
    void* GetMemberPtrByIndex(int32 Index);
    const void* GetMemberPtrByIndex(int32 Index) const;
    const char* GetMemberTypeNameByIndex(int32 Index) const;
    {%- endif %}
    
    {%- if def_entry['generated_simple_plus'] == 1 %}
    void SimplePlus(const {{def_entry['name']}}& Right);
    {%- endif %}
     
};
{%- endif %}
{% endfor %}
"""

# xxxx.cpp
CPP_TEMPL = """#include "{{to_file_name}}.h"
#include "{{service_name}}.pb.h"

{% for def_entry in def_list %}
{%- if def_entry['type'] == "enum" %}

bool Check{{def_entry['name']}}Valid(int32 Val)
{
    return {{package_name}}::{{def_entry['old_name']}}_IsValid(Val);
}

const TCHAR* Get{{def_entry['name']}}Description({{def_entry['name']}} Val)
{
    switch (Val)
    {
{%- for member in def_entry['members'] %}
        case {{def_entry['name']}}::{{member['name']}}: return TEXT("{{member['comment']}}");
{%- endfor %}
    }
    return TEXT("UNKNOWN");
}

{%- else %}

{{def_entry['name']}}::{{def_entry['name']}}()
{
    Reset();        
}

{{def_entry['name']}}::{{def_entry['name']}}(const {{package_name}}::{{def_entry['old_name']}}& Right)
{
    this->FromPb(Right);
}

void {{def_entry['name']}}::FromPb(const {{package_name}}::{{def_entry['old_name']}}& Right)
{
{%- for member in def_entry['members'] %}
    {%- if member["is_enum_value"] %}
    {{member['name']}} = static_cast<{{member['type']}}>(Right.{{member['name']}}());
    {%- elif member["is_array_value"] %}
    {{member['name']}}.Empty();
    for (const auto& Elem : Right.{{member['name']}}())
    {
        {%- if member["is_string_value"] %}
        {{member['name']}}.Emplace(UTF8_TO_TCHAR(Elem.c_str()));
        {%- else %}
        {{member['name']}}.Emplace(Elem);
        {%- endif %}
    }    
    {%- elif member["is_map_value"] %}
    {{member['name']}}.Empty();    
    for (const auto& Elem : Right.{{member['name']}}())
    {
        {%- if member["is_string_value"] %}
        {{member['name']}}.Emplace(Elem.first, UTF8_TO_TCHAR(Elem.second.c_str()));
        {%- else %}
        {{member['name']}}.Emplace(Elem.first, Elem.second);
        {%- endif %}
    }
    {%- elif member["is_string_value"] %}
    {{member['name']}} = UTF8_TO_TCHAR(Right.{{member['name']}}().c_str());
    {%- elif member["is_bytes_value"] %}
    {{member['name']}}.Empty({{member['name']}}.Num());
    {{member['name']}}.Append(reinterpret_cast<const uint8*>(Right.{{member['name']}}().c_str()), Right.{{member['name']}}().size());
    {%- else %}
    {{member['name']}} = Right.{{member['name']}}();
    {%- endif %}  
{%- endfor %}
}

void {{def_entry['name']}}::ToPb({{package_name}}::{{def_entry['old_name']}}* Out) const
{
{%- for member in def_entry['members'] %}
    {%- if member["is_enum_value"] %}
    Out->set_{{member['name']}}(static_cast<{{package_name}}::{{member['old_type']}}>({{member['name']}}));
    {%- elif member["is_map_value"] %}
    for (const auto& Elem : {{member['name']}})
    {
        {%- if member["is_struct_value"] %}
        Elem.ToPb(&Out->mutable_{{member['name']}}()->operator[](Elem.Key));
        {%- elif member["is_string_value"] %}
        Out->mutable_{{member['name']}}()->operator[](Elem.Key) = TCHAR_TO_UTF8(GetData(Elem.Value));   
        {%- else %}
        Out->mutable_{{member['name']}}()->operator[](Elem.Key) = Elem.Value;
        {%- endif %}                
    }
    {%- elif member["is_array_value"] %}
    for (const auto& Elem : {{member['name']}})
    {
        {%- if member["is_struct_value"] %}
        Elem.ToPb(Out->add_{{member['name']}}());
        {%- elif member["is_native_value"] %}
        Out->add_{{member['name']}}(Elem);
        {%- elif member["is_string_value"] %}
        Out->add_{{member['name']}}(TCHAR_TO_UTF8(GetData(Elem)));
        {%- else %}
        *(Out->add_{{member['name']}}()) = Elem;
        {%- endif %}    
    }
    {%- elif member["is_struct_value"] %}
    {{member['name']}}.ToPb(Out->mutable_{{member['name']}}());
    {%- elif member["is_string_value"] %}
    Out->set_{{member['name']}}(TCHAR_TO_UTF8(*{{member['name']}}));
    {%- elif member["is_bytes_value"] %}
    Out->set_{{member['name']}}({{member['name']}}.GetData(), {{member['name']}}.Num());
    {%- else %}
    Out->set_{{member['name']}}({{member['name']}});
    {%- endif %}  
{%- endfor %}    
}

void {{def_entry['name']}}::Reset()
{
{%- for member in def_entry['members'] %}
    {{member['name']}} = {{member['type']}}();  
{%- endfor %}    
}

void {{def_entry['name']}}::operator=(const {{package_name}}::{{def_entry['old_name']}}& Right)
{
    this->FromPb(Right);
}

bool {{def_entry['name']}}::operator==(const {{def_entry['name']}}& Right) const
{
{%- for member in def_entry['members'] %}
    if (this->{{member['name']}} != Right.{{member['name']}})
        return false;
{%- endfor %}
    return true;
}

bool {{def_entry['name']}}::operator!=(const {{def_entry['name']}}& Right) const
{
    return !operator==(Right);
}

{%- if def_entry['generated_member_ptr'] == 1 %}

void* {{def_entry['name']}}::GetMemberPtrByIndex(int32 Index)
{
    switch (Index)
    {
{%- for member in def_entry['members'] %}
    case {{member['index']}}:
        return &{{member['name']}};  
{%- endfor %}
    default:
        return nullptr;
    }
}

const void* {{def_entry['name']}}::GetMemberPtrByIndex(int32 Index) const
{
    switch (Index)
    {
{%- for member in def_entry['members'] %}
    case {{member['index']}}:
        return &{{member['name']}};  
{%- endfor %}
    default:
        return nullptr;
    }
}

const char* {{def_entry['name']}}::GetMemberTypeNameByIndex(int32 Index) const
{
    switch (Index)
    {
{%- for member in def_entry['members'] %}
    case {{member['index']}}:
        return "{{member['old_type']}}";  // {{member['name']}}  
{%- endfor %}
    default:
        return nullptr;
    }
}
{% endif %}

{%- if def_entry['generated_simple_plus'] == 1 %}
void {{def_entry['name']}}::SimplePlus(const {{def_entry['name']}}& Right)
{
{%- for member in def_entry['members'] %}
    this->{{member['name']}} += Right.{{member['name']}};
{%- endfor %}
}
{% endif %}

{%- endif %}
{%- endfor %}
"""


def parse_other_pb(src_file, out_enum_list):
    src_file_path = pathlib.Path(src_file)
    to_file_name = ''
    with open(src_file_path, 'r', encoding='utf-8') as f:
        line_num = 0
        for line in f.readlines():
            line_num += 1

            if line_num == 1 and line.startswith('//') and line.find('PB2BP') != -1:
                for field in line.split():
                    if field.startswith('BP_FILE_NAME='):
                        to_file_name = field.split('=')[1]
                continue

            line = line.strip().strip('\n').strip('\r')
            line = ' '.join(line.split())
            data = line.split(' ')
            op = data[0].strip()
            if op == 'enum':
                name = data[1].rstrip("{")
                out_enum_list.append(name)
    
    if len(to_file_name) == 0:
        to_file_name = src_file_path.stem        
    return to_file_name


def to_ue_type(old_type, enum_name_list):
    if old_type in {'bool', 'float', 'int32', 'int64'}:
        return old_type
    elif old_type == 'double':
        return 'float'  # 蓝图不支持double
    elif old_type in {'uint32', 'uint64', 'sint32', 'sint64'}:
        return old_type[1:]  # 蓝图不支持无符号整数
    elif old_type == 'string':
        return 'FString'
    elif old_type == 'bytes':
        return 'TArray<uint8>'
    elif old_type in enum_name_list:
        return 'EPb' + old_type
    else:
        return 'FPb' + old_type


def main(src_file, hpp_dst_dir, cpp_dst_dir, dllexport_decl):
    if hpp_dst_dir[-1] != '/':
        hpp_dst_dir += '/'

    if cpp_dst_dir[-1] != '/':
        cpp_dst_dir += '/'

    src_file_path = pathlib.Path(src_file)

    service_name = src_file_path.stem  # 即不含扩展名的文件名    

    package_name = ''
    to_file_name = ''

    def_list = list()

    enum_name_list = list()
    import_file_list = list()

    last_def_comment = list()
    with open(src_file_path, 'r', encoding='utf-8') as f:
        line_num = 0
                
        cur_def = None
        for line in f.readlines():
            line_num += 1
            line = line.strip().strip('\n').strip('\r')
            
            # 第一行为 PB2BP 的配置
            # // PB2BP: BP_FILE_NAME=PbCommon.h
            if line_num == 1 and line.startswith('//') and line.find('PB2BP') != -1:
                for field in line.split():
                    if field.startswith('BP_FILE_NAME='):
                        to_file_name = field.split('=')[1]
                continue
            
            line = ' '.join(line.split())
            if line.startswith('//'):  # 注释
                last_def_comment.append(line.lstrip('//').lstrip('/**').rstrip('*/').strip())
                continue
            line = line.strip()
            data = line.split(' ')
            op = data[0].strip()
            
            if len(op) == 0 or op == 'syntax':
                last_def_comment = list()
                pass
            elif op == 'package':
                package_name = data[1].rstrip(';').strip()
            elif op == 'import':
                file_name = data[1].strip(';').strip('"')
                need_import_file = parse_other_pb(src_file_path.parent / file_name, enum_name_list)
                if len(need_import_file) > 0:
                    import_file_list.append(need_import_file)
            elif op.startswith('}'):
                if cur_def is not None:
                    no_blueprint = False
                    for c in cur_def['comment']:
                        if c.find('@no-blueprint') != -1:
                            no_blueprint = True
                    cur_def['no_blueprint'] = no_blueprint
                    def_list.append(cur_def)
                    cur_def = None
            elif op == 'enum' or op == 'message':
                if cur_def is not None:
                    raise Exception(f'格式错误，上个定义还未结束！ 行号:{line_num} 上个定义名称:{cur_def["name"]}') 
                if len(data) > 2:
                    cur_def = dict()
                    cur_def['type'] = op
                    cur_def['name'] = data[1].rstrip("{")
                    cur_def['members'] = list()
                    cur_def['comment'] = last_def_comment
                    last_def_comment = list()

                    cur_def['old_name'] = cur_def['name']
                    if op == 'enum':
                        cur_def['name'] = 'EPb' + cur_def['name']
                        enum_name_list.append(cur_def['old_name'])
                    else:
                        cur_def['name'] = 'FPb' + cur_def['name']
                    
                    cur_def['generated_simple_plus'] = 0
                    cur_def['generated_member_ptr'] = 0
                    if cur_def['old_name'] == 'RoleAttribute':
                        cur_def['generated_simple_plus'] = 1
                        cur_def['generated_member_ptr'] = 1                    
                else:
                    raise Exception(f'格式错误，定义头缺少必要字段！ 行号:{line_num}')
            else:
                if cur_def is None:
                    raise Exception(f'格式错误，上个定义还未开始！ 行号:{line_num}')                            
                if len(data) < 4:
                    raise Exception(f'格式错误，定义体缺少必要字段！ 行号:{line_num}')
                member = dict()
                if cur_def['type'] == 'enum':  # 枚举
                    member['type'] = 'int32'
                    member['name'] = data[0]
                    member['index'] = data[2].rstrip(';').strip()

                    member['comment'] = ''
                    if len(data) > 3:
                        s = ' '.join(data[3:])                        
                        member['comment'] = s.lstrip('//').lstrip('/**').rstrip('*/').strip()
                else:  # 类
                    is_array = False
                    is_map = False                        
                    if data[0].startswith('map'):
                        is_map = True
                        pos1 = line.find('<')
                        pos2 = line.find('>')
                        pos3 = line.find('=')
                        pos4 = line.find(';')
                        if pos1 == -1 or pos2 == -1 or pos3 == -1 or pos4 == -1:
                            raise Exception(f'格式错误，map定义不正确！ 行号:{line_num}')
                        type = line[pos1+1:pos2].split(',')                        
                        member['key_type'] = type[0].strip()
                        member['value_type'] = type[1].strip()
                        member['type'] = member['value_type'] 
                        member['name'] = line[pos2+1:pos3].strip()
                        member['index'] = line[pos3+1:pos4].strip()
                        member['comment'] = ''
                        if pos4 + 1 < len(line):
                            member['comment'] = line[pos4+1:].lstrip('//').lstrip('/**').rstrip('*/').strip()                        
                    else:
                        field_offset = 0
                        if data[0] == 'repeated':
                            is_array = True
                            field_offset = 1
                        member['type'] = data[field_offset]
                        
                        s = ' '.join(data[field_offset+1:])
                        data = s.split(';')

                        member['comment'] = ''
                        if len(data) >= 2:
                            s = ''.join(data[1:]).strip()
                            member['comment'] = s.lstrip('//').lstrip('/**').rstrip('*/').strip()
                            
                        data = data[0].split('=')
                        member['name'] = data[0].strip()
                        member['index'] = data[1].strip()

                        # member['name'] = data[field_offset + 1]
                        # member['index'] = data[field_offset + 3].rstrip(';').strip()
                        # member['comment'] = ''
                        # if len(data) > (4 + field_offset):
                        #     s = ' '.join(data[(4 + field_offset):])
                        #     member['comment'] = s.lstrip('//').lstrip('/**').rstrip('*/').strip()
                    
                    member['is_enum_value'] = False
                    member['is_struct_value'] = False
                    member['is_string_value'] = False
                    member['is_bytes_value'] = False
                    member['is_array_value'] = False
                    member['is_map_value'] = False
                    member['is_native_value'] = False
                    
                    member['old_type'] = member['type']                    
                    if is_array:
                        new_type = to_ue_type(member['type'], enum_name_list)
                        member['type'] = f"TArray<{new_type}>"
                        member['is_array_value'] = True
                        member['is_string_value'] = new_type.startswith('FString')
                        member['is_struct_value'] = new_type.startswith('FPb')
                        member['is_native_value'] = new_type.startswith('int') or new_type == 'float'
                    elif is_map:
                        key_type = to_ue_type(member['key_type'], enum_name_list)
                        value_type = to_ue_type(member['value_type'], enum_name_list)
                        member['type'] = f'TMap<{key_type}, {value_type}>'
                        member['is_map_value'] = True
                        member['is_string_value'] = value_type.startswith('FString')
                        member['is_struct_value'] = value_type.startswith('FPb')
                        member['is_native_value'] = value_type.startswith('int') or value_type == 'float'
                    else:
                        member['type'] = to_ue_type(member['old_type'], enum_name_list)
                        member['is_enum_value'] = member['type'].startswith('EPb')
                        member['is_struct_value'] = member['type'].startswith('FPb')
                        member['is_string_value'] = member['type'].startswith('FString')
                        member['is_bytes_value'] = member['type'].startswith('TArray<uint8>')
                        member['is_native_value'] = member['type'].startswith('int') or member['type'] == 'float'
                    
                cur_def['members'].append(member)

    if len(to_file_name) == 0:
        to_file_name = service_name

    meta_data = {
        'service_name': service_name,
        'package_name': package_name,
        'def_list': def_list,
        'to_file_name': to_file_name,
        'import_file_list': import_file_list,
        'dllexport_decl': dllexport_decl,
    }

    try:
        hpp_content = jinja2.Template(HPP_TEMPL).render(meta_data)
    except Exception as e:
        raise Exception('FWD_TEMPL 文件渲染失败 {}'.format(e))

    try:
        cpp_content = jinja2.Template(CPP_TEMPL).render(meta_data)
    except Exception as e:
        raise Exception('CPP_TEMPL 文件渲染失败 {}'.format(e))

    hpp_file_path = hpp_dst_dir + to_file_name + '.h'
    cpp_file_path = cpp_dst_dir + to_file_name + '.cpp'

    # 写入磁盘
    try:
        with open(hpp_file_path, 'wb') as f:
            f.write(hpp_content.encode('utf-8'))
        with open(cpp_file_path, 'wb') as f:
            f.write(cpp_content.encode('utf-8'))
    except Exception as e:
        raise Exception('文件写入失败 {}'.format(e))    


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-src_file', nargs='?', const='none', help='源文件')
    parser.add_argument('-hpp_dst_dir', nargs='?', const='none', help='头文件目标目录')
    parser.add_argument('-cpp_dst_dir', nargs='?', const='none', help='源文件文件目标目录')
    parser.add_argument('-dllexport_decl', nargs='?', const='none', help='DLL导出宏')
    args = parser.parse_args()

    if args.src_file is None:
        print('ERROR: 未指定源文件')
        exit(1)

    if not os.path.exists(args.src_file):
        print('ERROR: 源文件非法! {}'.format(args.src_file))
        exit(1)

    if not os.path.exists(args.hpp_dst_dir):
        print('ERROR: 头文件输出目录非法! {}'.format(args.hpp_dst_dir))
        exit(1)
        
    if not os.path.exists(args.cpp_dst_dir):
        print('ERROR: 源文件输出目录非法! {}'.format(args.cpp_dst_dir))
        exit(1)

    dllexport_decl = args.dllexport_decl or ''

    try:
        main(args.src_file, args.hpp_dst_dir, args.cpp_dst_dir, dllexport_decl)
    except Exception as e:
        print('处理失败({}): {}'.format(args.src_file, e))
        exit(1)
    exit(0)
