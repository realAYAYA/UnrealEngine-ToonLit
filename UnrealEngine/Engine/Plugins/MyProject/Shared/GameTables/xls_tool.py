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

# ======================================================================================================================

# 包含头文件，使得生成的胶水代码类型可以识别自定义类型
custom_includes = """
//#include "***.h"
"""

# 生成胶水代码的头文件目录
hpp_dir = 'Excels'

# 前缀命名
custom_prefix = 'Excel'

# Category
custom_category = "GameTables | Xlsx"

# ======================================================================================================================

XLS_VAR_NAME_ROW_IDX = 0  # 变量名行
XLS_VAR_TYPE_ROW_IDX = 1  # 变量类型行
XLS_SHOW_NAME_ROW_IDX = 2  # 显示名称行
XLS_DATA_START_ROW_IDX = 3  # 数据起始行 

# xxx.h
HPP_TEMPL = """#pragma once
#include "Engine/DataTable.h"
{{custom_includes}}
// =========================================================
{{custom_include_begin}}
{{custom_include_content}}
{{custom_include_end}}
// =========================================================
#include "{{code_file_base_name}}.generated.h"


// =============================================================================
{{custom_code_begin}}
{{custom_code_hpp}}
{{custom_code_end}}
// =============================================================================


USTRUCT()
struct {{dllexport_decl}} {{struct_name}} : public FTableRowBase
{
    GENERATED_BODY()
{%- for def in defines %}

    /** {{def['show_name']}} */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "{{custom_category}}")
{%- if def['var_is_array'] %}
    TArray<{{def['var_type']}}> {{def['var_name']}};
{%- else %}
{%- if def['var_need_init'] %}
    {{def['var_type']}} {{def['var_name']}} = {{def['var_type']}}();
{%- else %}
    {{def['var_type']}} {{def['var_name']}};
{%- endif %}
{%- endif %}
{%- endfor %}

};

UCLASS(BlueprintType)
class {{dllexport_decl}} {{class_name}} : public UObject
{
    GENERATED_BODY()
public: 

    bool Init();
   
{%- for def in defines %}

    /** {{def['show_name']}} */
    UPROPERTY(BlueprintReadOnly, Category = "{{custom_category}}")
{%- if def['var_is_array'] %}
    TArray<{{def['var_type']}}> {{def['var_name']}};
{%- else %}
{%- if def['var_need_init'] %}
    {{def['var_type']}} {{def['var_name']}} = {{def['var_type']}}();
{%- else %}
    {{def['var_type']}} {{def['var_name']}};
{%- endif %}
{%- endif %}   
{%- endfor %}

    // =========================================================
    {{custom_class_member_begin}}
{{custom_class_member_hpp}}
    {{custom_class_member_end}}
    // =========================================================
};

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnForeach{{base_name}}Config, const {{class_name}}*, Entry);


UCLASS(BlueprintType)
class {{dllexport_decl}} {{manager_name}} : public UObject
{
    GENERATED_BODY()

public:

    typedef {{class_name}} EntryType;

    bool Init(bool bLoadImmediately = false);
    void Foreach(const TFunction<bool(const {{class_name}}*)>& Func) const;
    void MutableForeach(const TFunction<bool({{class_name}}*)>& Func) const;

    /** 查找 */
    UFUNCTION(BlueprintPure, Category = "{{custom_category}}") 
    const {{class_name}}* Get(const {{key_field_type}}& InKey) const; 

    /** 遍历 */
    UFUNCTION(BlueprintCallable, Category = "{{custom_category}}", DisplayName = "Foreach")
    void K2_Foreach(const FOnForeach{{base_name}}Config& InCallback); 

    /** 配置文件名称 */
    UFUNCTION(BlueprintPure, Category = "{{custom_category}}")
    static FString GetConfigFileName();

    // =========================================================
    {{custom_manager_member_begin}}
{{custom_manager_hpp_content}}
    {{custom_manager_member_end}}
    // =========================================================
    
private:

    bool TryLoadData(bool bForce = false) const;

    UPROPERTY()
    mutable TMap<{{key_field_type}}, {{class_name}}*> Data; 
    
    mutable bool bInitDataDone = false;
};

"""

# xxx.cpp
CPP_TEMPL = """#include "{{hpp_dir}}/{{code_file_base_name}}.h"
#include "ConfigLoadHelper.h"


// =============================================================================
{{custom_code_begin}}
{{custom_code_cpp}}
{{custom_code_end}}
// =============================================================================


bool {{class_name}}::Init()
{
    // =========================================================
    {{custom_class_init_begin}}
{{custom_class_init_cpp}}
    {{custom_class_init_end}}
    // =========================================================
    return true;
} 

bool {{manager_name}}::Init(bool bLoadImmediately)
{
    if (bLoadImmediately)
    {
        return TryLoadData(true);
    }

    FString Path = GetGameDesignDataFullPath() / GetConfigFileName();
    auto Table = LoadTableFromJsonFile<{{struct_name}}>(Path, TEXT("{{key_field_name}}"));
    if (!Table)
        return false;
    
    return true;
}
    
bool {{manager_name}}::TryLoadData(bool bForce) const
{
    if (!bForce)
    {
        if (bInitDataDone)
        {
            return true;
        }
    }
    bInitDataDone = true;

    FString Path = GetGameDesignDataFullPath() / GetConfigFileName();
    auto Table = LoadTableFromJsonFile<{{struct_name}}>(Path, TEXT("{{key_field_name}}"));
    if (!Table)
        return false;

    Table->ForeachRow<{{struct_name}}>(
        TEXT("{{manager_name}}::Init"), 
        [this](const FName& Key, const {{struct_name}}& Row)
        {
            bool bIsNew = true;
            {{class_name}}* Config = nullptr;
            {
                auto Ret = Data.Find(Row.{{key_field_name}});
                if (Ret && *Ret)
                {
                    bIsNew = false;
                    Config = *Ret;
                    for (TFieldIterator<FProperty> It({{class_name}}::StaticClass()); It; ++It)
                    {
                        FProperty* Prop = *It;
                        if (!Prop)
                            continue;
                        void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Config, 0);
                        if (!ValuePtr)
                            continue;
                        Prop->InitializeValue(ValuePtr);
                    }
                }
                else
                {
                    Config = NewObject<{{class_name}}>();
                }
            }
{%- for def in defines %}
            Config->{{def['var_name']}} = Row.{{def['var_name']}};
{%- endfor %}
            if (Config->Init())
            {
                if (bIsNew)
                {
                    Data.Emplace(Config->{{key_field_name}}, Config);
                }
            }
        });

    // =========================================================
    {{custom_manager_init_begin}}
{{custom_manager_init_cpp}}
    {{custom_manager_init_end}}
    // =========================================================
    return true;
}

void {{manager_name}}::Foreach(const TFunction<bool(const {{class_name}}*)>& Func) const
{
    TryLoadData();
    for (const auto& Elem : Data)
    {
        if (!Func(Elem.Value)) 
            break;
    }
}

void {{manager_name}}::MutableForeach(const TFunction<bool({{class_name}}*)>& Func) const
{
    TryLoadData();
    for (const auto& Elem : Data)
    {
        if (!Func(Elem.Value)) 
            break;
    }
}
 
const {{class_name}}* {{manager_name}}::Get(const {{key_field_type}}& InKey) const 
{
    TryLoadData();
    auto Ret = Data.Find(InKey);
    if (!Ret)
        return nullptr;
    return *Ret;
}

void {{manager_name}}::K2_Foreach(const FOnForeach{{base_name}}Config& InCallback)
{
    TryLoadData();
    Foreach([InCallback](const {{class_name}}* Entry) -> bool {
        InCallback.Execute(Entry);
        return true;
    });
}

FString {{manager_name}}::GetConfigFileName()
{
    return TEXT("{{pure_name}}.jsondata");
}

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
                        content = content[0:len(content)-1]
                    return content    
            if content is not None:
                content += line
    return ''
                

# 生成代码文件
def generate_code(args, defines: list):
    base_name = f'{custom_prefix}{args.pure_name}'
    struct_name = f'F{base_name}Row'
    class_name = f'U{base_name}Config'
    manager_name = f'U{base_name}Table'
    key_field_name = defines[0].get('var_name')
    key_field_type = defines[0].get('var_type')
    meta_data = {
        'dllexport_decl': args.dllexport_decl,
        'pure_name': args.pure_name,
        'code_file_base_name': args.code_file_base_name,
        'base_name': base_name,
        'struct_name': struct_name,
        'class_name': class_name, 
        'manager_name': manager_name,  
        'defines': defines, 
        'key_field_name': key_field_name,
        'key_field_type': key_field_type,
    }
    
    hpp_file = f'{args.hpp_dst_dir}/{args.code_file_base_name}.h' 
    cpp_file = f'{args.cpp_dst_dir}/{args.code_file_base_name}.cpp'

    custom_include_begin = f'// *** BEGIN WRITING YOUR CODE - INCLUDE ***'
    custom_include_end = f'// *** END WRITING YOUR CODE - INCLUDE ***'

    custom_class_member_begin = f'// *** BEGIN WRITING YOUR CODE - {class_name} ***' 
    custom_class_member_end = f'// *** END WRITING YOUR CODE - {class_name} ***'
    custom_manager_member_begin = f'// *** BEGIN WRITING YOUR CODE - {manager_name} ***'
    custom_manager_member_end = f'// *** END WRITING YOUR CODE - {manager_name} ***'

    custom_class_init_begin = f'// *** BEGIN WRITING YOUR CODE - {class_name}::Init ***' 
    custom_class_init_end = f'// *** END WRITING YOUR CODE - {class_name}::Init ***'
    custom_manager_init_begin = f'// *** BEGIN WRITING YOUR CODE - {manager_name}::Init ***'
    custom_manager_init_end = f'// *** END WRITING YOUR CODE - {manager_name}::Init ***'

    custom_code_begin = f'// *** BEGIN WRITING YOUR CODE - CUSTOMIZE ***'
    custom_code_end = f'// *** END WRITING YOUR CODE - CUSTOMIZE ***'

    custom_include_content = ''

    custom_class_member_hpp = ''
    custom_manager_hpp_content = ''

    custom_class_init_cpp = ''
    custom_manager_init_cpp = ''

    custom_code_hpp = ''
    custom_code_cpp = ''

    try:
        if os.path.exists(hpp_file):
            custom_include_content = parse_custom_code(hpp_file, custom_include_begin, custom_include_end)
            custom_class_member_hpp = parse_custom_code(hpp_file, custom_class_member_begin, custom_class_member_end)
            custom_manager_hpp_content = parse_custom_code(hpp_file, custom_manager_member_begin, custom_manager_member_end)
            custom_code_hpp = parse_custom_code(hpp_file, custom_code_begin, custom_code_end) 
    except Exception as parse_err:
        raise Exception(f'ParseHppError {parse_err}')

    try:
        if os.path.exists(cpp_file):
            custom_class_init_cpp = parse_custom_code(cpp_file, custom_class_init_begin, custom_class_init_end)
            custom_manager_init_cpp = parse_custom_code(cpp_file, custom_manager_init_begin, custom_manager_init_end)
            custom_code_cpp = parse_custom_code(cpp_file, custom_code_begin, custom_code_end) 
    except Exception as parse_err:
        raise Exception(f'ParseCppError {parse_err}')

    meta_data['custom_category'] = custom_category
    meta_data['custom_includes'] = custom_includes
    meta_data['hpp_dir'] = hpp_dir

    meta_data['custom_include_begin'] = custom_include_begin
    meta_data['custom_include_end'] = custom_include_end
    meta_data['custom_include_content'] = custom_include_content

    meta_data['custom_class_member_begin'] = custom_class_member_begin
    meta_data['custom_class_member_end'] = custom_class_member_end
    meta_data['custom_manager_member_begin'] = custom_manager_member_begin
    meta_data['custom_manager_member_end'] = custom_manager_member_end

    meta_data['custom_class_member_hpp'] = custom_class_member_hpp
    meta_data['custom_manager_hpp_content'] = custom_manager_hpp_content

    meta_data['custom_class_init_begin'] = custom_class_init_begin
    meta_data['custom_class_init_end'] = custom_class_init_end
    meta_data['custom_manager_init_begin'] = custom_manager_init_begin
    meta_data['custom_manager_init_end'] = custom_manager_init_end

    meta_data['custom_class_init_cpp'] = custom_class_init_cpp
    meta_data['custom_manager_init_cpp'] = custom_manager_init_cpp

    meta_data['custom_code_begin'] = custom_code_begin
    meta_data['custom_code_end'] = custom_code_end
    meta_data['custom_code_hpp'] = custom_code_hpp
    meta_data['custom_code_cpp'] = custom_code_cpp

    try:
        hpp_content = jinja2.Template(HPP_TEMPL).render(meta_data)
    except Exception as hpp_err:
        raise Exception('HPP_TEMPL 文件渲染失败 {}'.format(hpp_err))
    try:
        cpp_content = jinja2.Template(CPP_TEMPL).render(meta_data)
    except Exception as cpp_err:
        raise Exception('CPP_TEMPL 文件渲染失败 {}'.format(cpp_err))

    # 写入磁盘
    try:
        with open(hpp_file, 'wb') as f:
            f.write(hpp_content.encode('utf-8'))
        with open(cpp_file, 'wb') as f:
            f.write(cpp_content.encode('utf-8'))
    except Exception as write_err:
        raise Exception('文件写入失败 {}'.format(write_err))    


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
def process_type(data_cell: xlrd.sheet.Cell):
    value = str(data_cell.value).strip()

    type_name = value
    delimiter_char = None
    delimiter_pos = type_name.find('[')
    if delimiter_pos != -1:
        delimiter_char = type_name[delimiter_pos:].strip('[').strip(']').strip()
        type_name = type_name[:delimiter_pos]
    
    raw_name = type_name 
    if type_name == 'string':
        type_name = 'FString'
    elif type_name == 'int':
        type_name = 'int32'
    elif type_name == 'double':  # UE 的 BP 只支持 float
        type_name = 'float'

    need_init = False
    if type_name.startswith('int') or type_name == 'float' or type_name == 'bool' or type_name.startswith('E'):
        need_init = True

    return {
        'raw_name': raw_name,
        'type_name': type_name,
        'is_array': (delimiter_char is not None),
        'delimiter': (delimiter_char or ''), 
        'need_init': need_init,
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

    def_var_type = define.get('var_type')
    if define.get('var_is_array'):
        value = str(value or '') 
        if len(value) == 0:
            return []
        cont = value.split(define.get('var_delimiter'))
        for x in range(len(cont)):
            cont[x] = cast_data_by_type(cont[x], def_var_type)
        return cont
    
    return cast_data_by_type(value, def_var_type)


def parse_header(data_sheet: xlrd.sheet.Sheet) -> list:
    table_define = list()
    for col_idx in range(data_sheet.ncols):
        col = data_sheet.col(col_idx)
        if len(col) < XLS_DATA_START_ROW_IDX:
            continue
        var_name = col[XLS_VAR_NAME_ROW_IDX].value
        try:
            type_data = process_type(col[XLS_VAR_TYPE_ROW_IDX])
            var_type = type_data['type_name']
            var_is_array = type_data['is_array']
            var_delimiter = type_data['delimiter']
            var_need_init = type_data['need_init']
        except Exception as e:
            raise Exception(f'处理类型错误 col={col_idx+1} {e}')
        show_name = col[XLS_SHOW_NAME_ROW_IDX].value
        if var_name.startswith('#') or len(var_name) == 0 or len(var_type) == 0:
            continue  # 变量名或类型为空，或者变量名以#号开头，跳过
        table_define.append({
            'col_idx': col_idx,
            'var_name': var_name,
            'var_type': var_type,
            'var_is_array': var_is_array,
            'var_delimiter': var_delimiter,
            'var_need_init': var_need_init,
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
                raise Exception(f'处理数据时发生错误 row={row_idx+1} col={def_col_idx+1} {e}')   
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

    defines = parse_header(sheet)
    # print_table_define(defines)
    if len(defines) == 0:
        raise Exception('未找到表格定义信息(注意：数据页必须是第一页')

    all_data = parse_data(sheet, defines)
    # print_table_data(all_data)

    if args.enable_code:
        generate_code(args, defines)

    if args.enable_data:
        generate_data(args, all_data)


def main(args):
    if args.src_file is None:
        raise Exception(f'未指定源文件')

    if not os.path.exists(args.src_file):
        raise Exception(f'源文件非法 {args.src_file}')

    args.enable_code = False
    if args.hpp_dst_dir or args.cpp_dst_dir:
        args.enable_code = True
        if not os.path.exists(args.hpp_dst_dir or ''):
            raise Exception(f'HPP文件输出目录非法 {args.hpp_dst_dir}')
        if not os.path.exists(args.cpp_dst_dir or ''):
            raise Exception(f'CPP文件输出目录非法 {args.cpp_dst_dir}')

    args.enable_data = False
    if args.data_dst_dir:
        args.enable_data = True
        if not os.path.exists(args.data_dst_dir):
            raise Exception(f'ERROR: 数据文件输出目录非法 {args.data_dst_dir}')

    args.pure_name = pathlib.Path(args.src_file).stem  # '/home/hudawei/Item.xlsx' -> 'Item'

    prefix = custom_prefix
    suffix = 'Table'
    if args.pure_name.startswith(prefix):
        prefix = ''
    if args.pure_name.endswith(suffix):
        suffix = ''
    args.code_file_base_name = f'{prefix}{args.pure_name}{suffix}'  # ItemTable

    process_xls(args)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-src_file', nargs='?', const='none', help='源文件')
    parser.add_argument('-hpp_dst_dir', nargs='?', const='none', help='头文件目标目录')
    parser.add_argument('-cpp_dst_dir', nargs='?', const='none', help='源文件文件目标目录')
    parser.add_argument('-dllexport_decl', nargs='?', const='none', default='', help='DLL导出宏')
    parser.add_argument('-data_dst_dir', nargs='?', const='none', help='数据文件目标目录')
    cmd_args = parser.parse_args()

    try:
        main(cmd_args)
    except Exception as e:
        print(f'{cmd_args.src_file} ERROR! {e}')
        exit(1)
    print(f'{cmd_args.src_file} DONE.')
    exit(0)
