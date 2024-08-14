#!/usr/bin/env python3
# -*- mode: python; coding: utf-8-unix; -*-
# coding: utf-8

import argparse
import os
import sys
import copy
import pathlib
import collections
import jinja2


# xxxx.h
HPP_TEMPL = """#pragma once

namespace idlezt { class GameStatsData; }

class {{dllexport_decl}} FZPbGameStats
{
public:

    void LoadData(const idlezt::GameStatsData& InData);
    void SaveData(idlezt::GameStatsData* OutData) const;
    
    void Reset();  // 重置属性集
    void SimplePlus(const FZPbGameStats& InRight);  // 和指定属性集简单累加
    void CopyFrom(const FZPbGameStats& InRight);  // 复制指定属性集
    void operator=(const FZPbGameStats& InRight);

    void SetValueByType(int32 InType, float InValue);
    float GetValueByType(int32 InType, float DefaultValue = 0) const;

    void ForeachStat(const TFunction<bool(int32, float)>& Callback) const;

    static int32 GetIdByName(FName InName);
    static FName GetNameById(int32 InId);

{% for game_stats_entry in game_stats_entries %}
{%- if game_stats_entry["id"] != 0 %}
    // ================================================================

    /** {{game_stats_entry['comment']}}  */
    float {{game_stats_entry['pure_name']}}() const;

    /** 设置 {{game_stats_entry['comment']}} 数值 */
    void set_{{game_stats_entry['pure_name']}}(float InValue);

    /** 累加 {{game_stats_entry['comment']}} 数值 */
    void accumulate_{{game_stats_entry['pure_name']}}(float InValue);

{%- endif%}
{% endfor %}

private:
    TMap<int32, float> AllStats;
};

"""

# xxxx.cpp
CPP_TEMPL = """#include "{{to_file_name}}.h"
#include "{{service_name}}.pb.h"
#include "common.pb.h"

void FZPbGameStats::LoadData(const idlezt::GameStatsData& InData)
{
    Reset();
    for (auto& Elem : InData.stats())
    {
        this->AllStats.Emplace(Elem.type(), Elem.value());
    }
}

void FZPbGameStats::SaveData(idlezt::GameStatsData* OutData) const
{
    OutData->clear_stats();
    for (auto& Elem : this->AllStats)
    {
        if (auto* Stats = OutData->add_stats())
        {
            Stats->set_type(Elem.Key);
            Stats->set_value(Elem.Value);
        }
    }
}

void FZPbGameStats::Reset()
{
    this->AllStats.Empty(idlezt::GameStatType_MAX);
}

void FZPbGameStats::SimplePlus(const FZPbGameStats& InRight)
{
    for (auto& Elem : InRight.AllStats)
    {
        float Val = this->GetValueByType(Elem.Key) + Elem.Value;
        this->SetValueByType(Elem.Key, Val);
    }
}

void FZPbGameStats::CopyFrom(const FZPbGameStats& InRight)
{
    this->AllStats = InRight.AllStats;
}

void FZPbGameStats::operator=(const FZPbGameStats& InRight)
{
    this->CopyFrom(InRight);
}

void FZPbGameStats::SetValueByType(int32 InType, float InValue)
{
    if (InType == 0)
    {
        return;
    }

    auto& Ret = this->AllStats.FindOrAdd(InType);
    Ret = InValue;
}

float FZPbGameStats::GetValueByType(int32 InType, float DefaultValue) const
{
    if (auto* Ret = this->AllStats.Find(InType))
    {
        return *Ret;
    }
    return DefaultValue;
}

void FZPbGameStats::ForeachStat(const TFunction<bool(int32, float)>& Callback) const
{
    for (const auto& Stat : this->AllStats)
    {
        if (!Callback(Stat.Key, Stat.Value))
            break;
    }
}

// static
int32 FZPbGameStats::GetIdByName(FName InName)
{
    static TMap<FName, int32> Names {
{%- for game_stats_entry in game_stats_entries %}
{%- if game_stats_entry["id"] != 0 %}
        { TEXT("{{game_stats_entry['pure_name']}}"), idlezt::{{game_stats_entry['name']}} },
{%- endif%}
{%- endfor %}
    };

    auto* Ret = Names.Find(InName);
    if (!Ret)
    {
        return 0;
    }
    return *Ret;    
}

// static
FName FZPbGameStats::GetNameById(int32 InId)
{
    static TMap<int32, FName> Names {
{%- for game_stats_entry in game_stats_entries %}
{%- if game_stats_entry["id"] != 0 %}
        { idlezt::{{game_stats_entry['name']}}, TEXT("{{game_stats_entry['pure_name']}}") },
{%- endif%}
{%- endfor %}
    };

    auto* Ret = Names.Find(InId);
    if (!Ret)
    {
        return NAME_None;
    }
    return *Ret;    
}

{% for game_stats_entry in game_stats_entries %}
{%- if game_stats_entry["id"] != 0 %}
float FZPbGameStats::{{game_stats_entry['pure_name']}}() const
{
    return GetValueByType(idlezt::{{game_stats_entry['name']}});
}

void FZPbGameStats::set_{{game_stats_entry['pure_name']}}(float InValue)
{
    SetValueByType(idlezt::{{game_stats_entry['name']}}, InValue);
}

void FZPbGameStats::accumulate_{{game_stats_entry['pure_name']}}(float InValue)
{
    auto Num = {{game_stats_entry['pure_name']}}() + InValue;
    set_{{game_stats_entry['pure_name']}}(Num);
}
    
{%- endif%}
{% endfor %}
"""


# xxxx.ts
TS_TEMPL = """// Code generated - DO NOT EDIT.
import * as UE from "ue"
import * as common_pb from 'protocol/common'
import * as game_stats_pb from 'protocol/game_stats'

export type GameStatTypeEnum = game_stats_pb.GameStatType
export const GameStatType = game_stats_pb.GameStatType

export class TsGameStats {

    public LoadData(inData: common_pb.GameStatsData): void {
        this.allStats.clear()
        if (inData.stats) {
            for (let stats of inData.stats) {
                this.allStats.set(stats.type, stats.value)
            }
        }
    }

    public SaveData(outData: common_pb.GameStatsData): void {
        outData.stats = []
        for (let [type, value] of this.allStats) {
            let data = common_pb.GameStatData.create()
            data.type = type
            data.value = value
            outData.stats.push(data)
        }
    }

    // 重置属性集
    public Reset(): void {
        this.allStats.clear()
    }

    public CopyFrom(inGameStats: TsGameStats): void {
        this.Reset()
        for (let [type, value] of inGameStats.allStats) {
            this.allStats.set(type, value)
        }
    }

    // 简单累加属性集
    public SimplePlus(inGameStats: TsGameStats): void {
        for (let [type, value] of inGameStats.allStats) {
            let val = this.GetValueByType(type) + value
            this.SetValueByType(type, val)
        }
    }

    public SetValueByType(type: GameStatTypeEnum, value: number): void {
        this.allStats.set(type, value)
    }

    public GetValueByType(type: GameStatTypeEnum, defaultValue: number = 0): number {
        let ret = this.allStats.get(type)
        if (!ret) {
            return defaultValue
        }
        return ret
    }

{% for game_stats_entry in game_stats_entries %}
{%- if game_stats_entry["id"] != 0 %}
    // ================================================================

    /** {{game_stats_entry['comment']}}  */
    public {{game_stats_entry['pure_name']}}(): number {
        return this.GetValueByType(GameStatType.GST_{{game_stats_entry['pure_name']}});
    }

    /** 设置 {{game_stats_entry['comment']}} 数值 */
    public set_{{game_stats_entry['pure_name']}}(inValue: number): void {
        this.SetValueByType(GameStatType.GST_{{game_stats_entry['pure_name']}}, inValue);
    }

    /** 累加 {{game_stats_entry['comment']}} 数值 */
    public accumulate_{{game_stats_entry['pure_name']}}(inValue: number): void {
        let newValue = this.GetValueByType(GameStatType.GST_{{game_stats_entry['pure_name']}}, 0) + inValue;
        this.SetValueByType(GameStatType.GST_{{game_stats_entry['pure_name']}}, newValue);
    }
    
{%- endif%}
{% endfor %}

    private allStats = new Map<GameStatTypeEnum, number>()
}

export function GetStatNameById(statId: number): string {
    switch (statId) {
{%- for game_stats_entry in game_stats_entries %}
        case {{game_stats_entry["id"]}}: return "{{game_stats_entry['pure_name']}}"  // {{game_stats_entry['comment']}}
{%- endfor %}
    }
    return "none"
}

export function GetStatIdByName(statName: string): number {
    switch (statName) {
{%- for game_stats_entry in game_stats_entries %}
        case "{{game_stats_entry["pure_name"]}}": return {{game_stats_entry["id"]}}  // {{game_stats_entry['comment']}}
{%- endfor %}
    }
    return 0
}
"""


def main(src_file, hpp_dst_dir, cpp_dst_dir, ts_dst_dir, dllexport_decl):
    if hpp_dst_dir[-1] != '/':
        hpp_dst_dir += '/'

    if cpp_dst_dir[-1] != '/':
        cpp_dst_dir += '/'

    if ts_dst_dir[-1] != '/':
        ts_dst_dir += '/'

    src_file_path = pathlib.Path(src_file)
    service_name = src_file_path.stem  # 即不含扩展名的文件名    

    to_file_name = 'ZPbGameStats'

    game_stats_entries = None
    game_stats_entries_done = False

    with open(src_file_path, 'r', encoding='utf-8') as f:
        for line in f.readlines():
            line = line.strip()
            if len(line) == 0:
                continue
            if line.startswith('//') or line.startswith('/*'):
                continue
            if line.startswith('enum GameStatType'):
                game_stats_entries = list()
            elif line.startswith('}'):
                if game_stats_entries is not None:
                    game_stats_entries_done = True
            elif not game_stats_entries_done and game_stats_entries is not None:
                data = line.split(';')
                if len(data) >= 2:                
                    comment = data[1].strip().strip('//').strip('/*').strip('*/').strip()
                else:
                    comment = ''
                data = data[0].split('=')
                entry_name = data[0].strip()
                entry_id = int(data[1])
                pure_name = entry_name.lstrip('GST_')
                game_stats_entries.append({
                    'name': entry_name,
                    'id': entry_id,
                    'pure_name': pure_name,
                    'comment': comment,
                })
                # print(pure_name, entry_name, entry_id, comment)

    if not game_stats_entries_done:
        raise Exception('处理 GameStats 未结束')
                
    meta_data = {
        'service_name': service_name,
        'game_stats_entries': game_stats_entries,
        'to_file_name': to_file_name,
        'dllexport_decl': dllexport_decl,
    }

    try:
        hpp_content = jinja2.Template(HPP_TEMPL).render(meta_data)
    except Exception as e:
        raise Exception('HPP_TEMPL 文件渲染失败 {}'.format(e))

    try:
        cpp_content = jinja2.Template(CPP_TEMPL).render(meta_data)
    except Exception as e:
        raise Exception('CPP_TEMPL 文件渲染失败 {}'.format(e))

    try:
        ts_content = jinja2.Template(TS_TEMPL).render(meta_data)
    except Exception as e:
        raise Exception('TS_TEMPL 文件渲染失败 {}'.format(e))

    hpp_file_path = hpp_dst_dir + to_file_name + '.h'
    cpp_file_path = cpp_dst_dir + to_file_name + '.cpp'
    ts_file_path = ts_dst_dir + service_name + '_helper.ts'

    # 写入磁盘
    try:
        with open(hpp_file_path, 'wb') as f:
            f.write(hpp_content.encode('utf-8'))
        with open(cpp_file_path, 'wb') as f:
            f.write(cpp_content.encode('utf-8'))
        with open(ts_file_path, 'wb') as f:
            f.write(ts_content.encode('utf-8'))
    except Exception as e:
        raise Exception('文件写入失败 {}'.format(e))    


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-src_file', nargs='?', const='none', help='源文件')
    parser.add_argument('-hpp_dst_dir', nargs='?', const='none', help='头文件目标目录')
    parser.add_argument('-cpp_dst_dir', nargs='?', const='none', help='源文件目标目录')
    parser.add_argument('-ts_dst_dir', nargs='?', const='none', help='TS文件目标目录')
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

    if not os.path.exists(args.ts_dst_dir):
        print('ERROR: TS文件输出目录非法! {}'.format(args.ts_dst_dir))
        exit(1)

    dllexport_decl = args.dllexport_decl or ''

    try:
        main(args.src_file, args.hpp_dst_dir, args.cpp_dst_dir, args.ts_dst_dir, dllexport_decl)
    except Exception as e:
        print('处理失败({}): {}'.format(args.src_file, e))
        exit(1)
    exit(0)
