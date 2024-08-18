#include "PbCommon.h"
#include "common.pb.h"



FPbInt64Data::FPbInt64Data()
{
    Reset();        
}

FPbInt64Data::FPbInt64Data(const idlepb::Int64Data& Right)
{
    this->FromPb(Right);
}

void FPbInt64Data::FromPb(const idlepb::Int64Data& Right)
{
    value = Right.value();
}

void FPbInt64Data::ToPb(idlepb::Int64Data* Out) const
{
    Out->set_value(value);    
}

void FPbInt64Data::Reset()
{
    value = int64();    
}

void FPbInt64Data::operator=(const idlepb::Int64Data& Right)
{
    this->FromPb(Right);
}

bool FPbInt64Data::operator==(const FPbInt64Data& Right) const
{
    if (this->value != Right.value)
        return false;
    return true;
}

bool FPbInt64Data::operator!=(const FPbInt64Data& Right) const
{
    return !operator==(Right);
}

FPbVector2::FPbVector2()
{
    Reset();        
}

FPbVector2::FPbVector2(const idlepb::Vector2& Right)
{
    this->FromPb(Right);
}

void FPbVector2::FromPb(const idlepb::Vector2& Right)
{
    x = Right.x();
    y = Right.y();
}

void FPbVector2::ToPb(idlepb::Vector2* Out) const
{
    Out->set_x(x);
    Out->set_y(y);    
}

void FPbVector2::Reset()
{
    x = float();
    y = float();    
}

void FPbVector2::operator=(const idlepb::Vector2& Right)
{
    this->FromPb(Right);
}

bool FPbVector2::operator==(const FPbVector2& Right) const
{
    if (this->x != Right.x)
        return false;
    if (this->y != Right.y)
        return false;
    return true;
}

bool FPbVector2::operator!=(const FPbVector2& Right) const
{
    return !operator==(Right);
}

FPbVector3::FPbVector3()
{
    Reset();        
}

FPbVector3::FPbVector3(const idlepb::Vector3& Right)
{
    this->FromPb(Right);
}

void FPbVector3::FromPb(const idlepb::Vector3& Right)
{
    x = Right.x();
    y = Right.y();
    z = Right.z();
}

void FPbVector3::ToPb(idlepb::Vector3* Out) const
{
    Out->set_x(x);
    Out->set_y(y);
    Out->set_z(z);    
}

void FPbVector3::Reset()
{
    x = float();
    y = float();
    z = float();    
}

void FPbVector3::operator=(const idlepb::Vector3& Right)
{
    this->FromPb(Right);
}

bool FPbVector3::operator==(const FPbVector3& Right) const
{
    if (this->x != Right.x)
        return false;
    if (this->y != Right.y)
        return false;
    if (this->z != Right.z)
        return false;
    return true;
}

bool FPbVector3::operator!=(const FPbVector3& Right) const
{
    return !operator==(Right);
}

FPbColor::FPbColor()
{
    Reset();        
}

FPbColor::FPbColor(const idlepb::Color& Right)
{
    this->FromPb(Right);
}

void FPbColor::FromPb(const idlepb::Color& Right)
{
    r = Right.r();
    g = Right.g();
    b = Right.b();
    a = Right.a();
}

void FPbColor::ToPb(idlepb::Color* Out) const
{
    Out->set_r(r);
    Out->set_g(g);
    Out->set_b(b);
    Out->set_a(a);    
}

void FPbColor::Reset()
{
    r = float();
    g = float();
    b = float();
    a = float();    
}

void FPbColor::operator=(const idlepb::Color& Right)
{
    this->FromPb(Right);
}

bool FPbColor::operator==(const FPbColor& Right) const
{
    if (this->r != Right.r)
        return false;
    if (this->g != Right.g)
        return false;
    if (this->b != Right.b)
        return false;
    if (this->a != Right.a)
        return false;
    return true;
}

bool FPbColor::operator!=(const FPbColor& Right) const
{
    return !operator==(Right);
}

FPbInt64Pair::FPbInt64Pair()
{
    Reset();        
}

FPbInt64Pair::FPbInt64Pair(const idlepb::Int64Pair& Right)
{
    this->FromPb(Right);
}

void FPbInt64Pair::FromPb(const idlepb::Int64Pair& Right)
{
    key = Right.key();
    value = Right.value();
}

void FPbInt64Pair::ToPb(idlepb::Int64Pair* Out) const
{
    Out->set_key(key);
    Out->set_value(value);    
}

void FPbInt64Pair::Reset()
{
    key = int64();
    value = int64();    
}

void FPbInt64Pair::operator=(const idlepb::Int64Pair& Right)
{
    this->FromPb(Right);
}

bool FPbInt64Pair::operator==(const FPbInt64Pair& Right) const
{
    if (this->key != Right.key)
        return false;
    if (this->value != Right.value)
        return false;
    return true;
}

bool FPbInt64Pair::operator!=(const FPbInt64Pair& Right) const
{
    return !operator==(Right);
}

FPbStringKeyInt32ValueEntry::FPbStringKeyInt32ValueEntry()
{
    Reset();        
}

FPbStringKeyInt32ValueEntry::FPbStringKeyInt32ValueEntry(const idlepb::StringKeyInt32ValueEntry& Right)
{
    this->FromPb(Right);
}

void FPbStringKeyInt32ValueEntry::FromPb(const idlepb::StringKeyInt32ValueEntry& Right)
{
    key = UTF8_TO_TCHAR(Right.key().c_str());
    value = Right.value();
}

void FPbStringKeyInt32ValueEntry::ToPb(idlepb::StringKeyInt32ValueEntry* Out) const
{
    Out->set_key(TCHAR_TO_UTF8(*key));
    Out->set_value(value);    
}

void FPbStringKeyInt32ValueEntry::Reset()
{
    key = FString();
    value = int32();    
}

void FPbStringKeyInt32ValueEntry::operator=(const idlepb::StringKeyInt32ValueEntry& Right)
{
    this->FromPb(Right);
}

bool FPbStringKeyInt32ValueEntry::operator==(const FPbStringKeyInt32ValueEntry& Right) const
{
    if (this->key != Right.key)
        return false;
    if (this->value != Right.value)
        return false;
    return true;
}

bool FPbStringKeyInt32ValueEntry::operator!=(const FPbStringKeyInt32ValueEntry& Right) const
{
    return !operator==(Right);
}

FPbMapValueInt32::FPbMapValueInt32()
{
    Reset();        
}

FPbMapValueInt32::FPbMapValueInt32(const idlepb::MapValueInt32& Right)
{
    this->FromPb(Right);
}

void FPbMapValueInt32::FromPb(const idlepb::MapValueInt32& Right)
{
    key = Right.key();
    value = Right.value();
}

void FPbMapValueInt32::ToPb(idlepb::MapValueInt32* Out) const
{
    Out->set_key(key);
    Out->set_value(value);    
}

void FPbMapValueInt32::Reset()
{
    key = int32();
    value = int32();    
}

void FPbMapValueInt32::operator=(const idlepb::MapValueInt32& Right)
{
    this->FromPb(Right);
}

bool FPbMapValueInt32::operator==(const FPbMapValueInt32& Right) const
{
    if (this->key != Right.key)
        return false;
    if (this->value != Right.value)
        return false;
    return true;
}

bool FPbMapValueInt32::operator!=(const FPbMapValueInt32& Right) const
{
    return !operator==(Right);
}

FPbStringInt64Pair::FPbStringInt64Pair()
{
    Reset();        
}

FPbStringInt64Pair::FPbStringInt64Pair(const idlepb::StringInt64Pair& Right)
{
    this->FromPb(Right);
}

void FPbStringInt64Pair::FromPb(const idlepb::StringInt64Pair& Right)
{
    str = UTF8_TO_TCHAR(Right.str().c_str());
    value = Right.value();
}

void FPbStringInt64Pair::ToPb(idlepb::StringInt64Pair* Out) const
{
    Out->set_str(TCHAR_TO_UTF8(*str));
    Out->set_value(value);    
}

void FPbStringInt64Pair::Reset()
{
    str = FString();
    value = int64();    
}

void FPbStringInt64Pair::operator=(const idlepb::StringInt64Pair& Right)
{
    this->FromPb(Right);
}

bool FPbStringInt64Pair::operator==(const FPbStringInt64Pair& Right) const
{
    if (this->str != Right.str)
        return false;
    if (this->value != Right.value)
        return false;
    return true;
}

bool FPbStringInt64Pair::operator!=(const FPbStringInt64Pair& Right) const
{
    return !operator==(Right);
}

FPbAbilityEffectData::FPbAbilityEffectData()
{
    Reset();        
}

FPbAbilityEffectData::FPbAbilityEffectData(const idlepb::AbilityEffectData& Right)
{
    this->FromPb(Right);
}

void FPbAbilityEffectData::FromPb(const idlepb::AbilityEffectData& Right)
{
    type = Right.type();
    duration = Right.duration();
    period = Right.period();
    duration_policy = Right.duration_policy();
    starttime_world = Right.starttime_world();
    starttime_utc = Right.starttime_utc();
    x = Right.x();
    y = Right.y();
    z = Right.z();
    m = Right.m();
    n = Right.n();
}

void FPbAbilityEffectData::ToPb(idlepb::AbilityEffectData* Out) const
{
    Out->set_type(type);
    Out->set_duration(duration);
    Out->set_period(period);
    Out->set_duration_policy(duration_policy);
    Out->set_starttime_world(starttime_world);
    Out->set_starttime_utc(starttime_utc);
    Out->set_x(x);
    Out->set_y(y);
    Out->set_z(z);
    Out->set_m(m);
    Out->set_n(n);    
}

void FPbAbilityEffectData::Reset()
{
    type = int32();
    duration = float();
    period = float();
    duration_policy = int32();
    starttime_world = float();
    starttime_utc = int64();
    x = float();
    y = float();
    z = float();
    m = float();
    n = float();    
}

void FPbAbilityEffectData::operator=(const idlepb::AbilityEffectData& Right)
{
    this->FromPb(Right);
}

bool FPbAbilityEffectData::operator==(const FPbAbilityEffectData& Right) const
{
    if (this->type != Right.type)
        return false;
    if (this->duration != Right.duration)
        return false;
    if (this->period != Right.period)
        return false;
    if (this->duration_policy != Right.duration_policy)
        return false;
    if (this->starttime_world != Right.starttime_world)
        return false;
    if (this->starttime_utc != Right.starttime_utc)
        return false;
    if (this->x != Right.x)
        return false;
    if (this->y != Right.y)
        return false;
    if (this->z != Right.z)
        return false;
    if (this->m != Right.m)
        return false;
    if (this->n != Right.n)
        return false;
    return true;
}

bool FPbAbilityEffectData::operator!=(const FPbAbilityEffectData& Right) const
{
    return !operator==(Right);
}

bool CheckEPbReplicationTargetTypeValid(int32 Val)
{
    return idlepb::ReplicationTargetType_IsValid(Val);
}

const TCHAR* GetEPbReplicationTargetTypeDescription(EPbReplicationTargetType Val)
{
    switch (Val)
    {
        case EPbReplicationTargetType::RTT_Self: return TEXT("自己所在客户端");
        case EPbReplicationTargetType::RTT_World: return TEXT("当前场景");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbSystemNoticeStyleValid(int32 Val)
{
    return idlepb::SystemNoticeStyle_IsValid(Val);
}

const TCHAR* GetEPbSystemNoticeStyleDescription(EPbSystemNoticeStyle Val)
{
    switch (Val)
    {
        case EPbSystemNoticeStyle::SystemNoticeStyle_None: return TEXT("未知");
        case EPbSystemNoticeStyle::SystemNoticeStyle_Dialog: return TEXT("弹框提示");
        case EPbSystemNoticeStyle::SystemNoticeStyle_ScreenCenter: return TEXT("屏幕中央提示");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbSystemNoticeIdValid(int32 Val)
{
    return idlepb::SystemNoticeId_IsValid(Val);
}

const TCHAR* GetEPbSystemNoticeIdDescription(EPbSystemNoticeId Val)
{
    switch (Val)
    {
        case EPbSystemNoticeId::SystemNoticeId_None: return TEXT("未知");
        case EPbSystemNoticeId::SystemNoticeId_AddItem: return TEXT("添加道具");
    }
    return TEXT("UNKNOWN");
}

FPbGameStatData::FPbGameStatData()
{
    Reset();        
}

FPbGameStatData::FPbGameStatData(const idlepb::GameStatData& Right)
{
    this->FromPb(Right);
}

void FPbGameStatData::FromPb(const idlepb::GameStatData& Right)
{
    type = Right.type();
    value = Right.value();
}

void FPbGameStatData::ToPb(idlepb::GameStatData* Out) const
{
    Out->set_type(type);
    Out->set_value(value);    
}

void FPbGameStatData::Reset()
{
    type = int32();
    value = float();    
}

void FPbGameStatData::operator=(const idlepb::GameStatData& Right)
{
    this->FromPb(Right);
}

bool FPbGameStatData::operator==(const FPbGameStatData& Right) const
{
    if (this->type != Right.type)
        return false;
    if (this->value != Right.value)
        return false;
    return true;
}

bool FPbGameStatData::operator!=(const FPbGameStatData& Right) const
{
    return !operator==(Right);
}

FPbGameStatsData::FPbGameStatsData()
{
    Reset();        
}

FPbGameStatsData::FPbGameStatsData(const idlepb::GameStatsData& Right)
{
    this->FromPb(Right);
}

void FPbGameStatsData::FromPb(const idlepb::GameStatsData& Right)
{
    stats.Empty();
    for (const auto& Elem : Right.stats())
    {
        stats.Emplace(Elem);
    }
}

void FPbGameStatsData::ToPb(idlepb::GameStatsData* Out) const
{
    for (const auto& Elem : stats)
    {
        Elem.ToPb(Out->add_stats());    
    }    
}

void FPbGameStatsData::Reset()
{
    stats = TArray<FPbGameStatData>();    
}

void FPbGameStatsData::operator=(const idlepb::GameStatsData& Right)
{
    this->FromPb(Right);
}

bool FPbGameStatsData::operator==(const FPbGameStatsData& Right) const
{
    if (this->stats != Right.stats)
        return false;
    return true;
}

bool FPbGameStatsData::operator!=(const FPbGameStatsData& Right) const
{
    return !operator==(Right);
}

FPbGameStatsModuleData::FPbGameStatsModuleData()
{
    Reset();        
}

FPbGameStatsModuleData::FPbGameStatsModuleData(const idlepb::GameStatsModuleData& Right)
{
    this->FromPb(Right);
}

void FPbGameStatsModuleData::FromPb(const idlepb::GameStatsModuleData& Right)
{
    type = Right.type();
    stats = Right.stats();
}

void FPbGameStatsModuleData::ToPb(idlepb::GameStatsModuleData* Out) const
{
    Out->set_type(type);
    stats.ToPb(Out->mutable_stats());    
}

void FPbGameStatsModuleData::Reset()
{
    type = int32();
    stats = FPbGameStatsData();    
}

void FPbGameStatsModuleData::operator=(const idlepb::GameStatsModuleData& Right)
{
    this->FromPb(Right);
}

bool FPbGameStatsModuleData::operator==(const FPbGameStatsModuleData& Right) const
{
    if (this->type != Right.type)
        return false;
    if (this->stats != Right.stats)
        return false;
    return true;
}

bool FPbGameStatsModuleData::operator!=(const FPbGameStatsModuleData& Right) const
{
    return !operator==(Right);
}

FPbGameStatsAllModuleData::FPbGameStatsAllModuleData()
{
    Reset();        
}

FPbGameStatsAllModuleData::FPbGameStatsAllModuleData(const idlepb::GameStatsAllModuleData& Right)
{
    this->FromPb(Right);
}

void FPbGameStatsAllModuleData::FromPb(const idlepb::GameStatsAllModuleData& Right)
{
    all_module.Empty();
    for (const auto& Elem : Right.all_module())
    {
        all_module.Emplace(Elem);
    }
}

void FPbGameStatsAllModuleData::ToPb(idlepb::GameStatsAllModuleData* Out) const
{
    for (const auto& Elem : all_module)
    {
        Elem.ToPb(Out->add_all_module());    
    }    
}

void FPbGameStatsAllModuleData::Reset()
{
    all_module = TArray<FPbGameStatsModuleData>();    
}

void FPbGameStatsAllModuleData::operator=(const idlepb::GameStatsAllModuleData& Right)
{
    this->FromPb(Right);
}

bool FPbGameStatsAllModuleData::operator==(const FPbGameStatsAllModuleData& Right) const
{
    if (this->all_module != Right.all_module)
        return false;
    return true;
}

bool FPbGameStatsAllModuleData::operator!=(const FPbGameStatsAllModuleData& Right) const
{
    return !operator==(Right);
}

FPbRoleAttribute::FPbRoleAttribute()
{
    Reset();        
}

FPbRoleAttribute::FPbRoleAttribute(const idlepb::RoleAttribute& Right)
{
    this->FromPb(Right);
}

void FPbRoleAttribute::FromPb(const idlepb::RoleAttribute& Right)
{
    health = Right.health();
    max_health = Right.max_health();
    mana = Right.mana();
    max_mana = Right.max_mana();
    mind = Right.mind();
    strength = Right.strength();
    intellect = Right.intellect();
    agility = Right.agility();
    move_speed = Right.move_speed();
    phy_att = Right.phy_att();
    phy_def = Right.phy_def();
    mag_att = Right.mag_att();
    mag_def = Right.mag_def();
    phy_dodge = Right.phy_dodge();
    mag_dodge = Right.mag_dodge();
    phy_hit = Right.phy_hit();
    mag_hit = Right.mag_hit();
    crit = Right.crit();
    crit_def = Right.crit_def();
    hp_recover_percent = Right.hp_recover_percent();
    mp_recover_percent = Right.mp_recover_percent();
    crit_coef = Right.crit_coef();
    crit_block = Right.crit_block();
    crit_additional_damage = Right.crit_additional_damage();
    arena_money_add_percent = Right.arena_money_add_percent();
    mag_break = Right.mag_break();
    phy_break = Right.phy_break();
    mag_block = Right.mag_block();
    phy_block = Right.phy_block();
    fen_qi = Right.fen_qi();
    tuna_num = Right.tuna_num();
    tuna_add_percent = Right.tuna_add_percent();
    medicine_num = Right.medicine_num();
    medicine_add_percent = Right.medicine_add_percent();
    baseqi_add_percent = Right.baseqi_add_percent();
    shen_tong_damage_to_player_add_percent = Right.shen_tong_damage_to_player_add_percent();
    shen_tong_damage_to_player_reduce_percent = Right.shen_tong_damage_to_player_reduce_percent();
    fa_bao_damage_to_player_add_percent = Right.fa_bao_damage_to_player_add_percent();
    fa_bao_damage_to_player_reduce_percent = Right.fa_bao_damage_to_player_reduce_percent();
    phy_damage_add_percent = Right.phy_damage_add_percent();
    mag_damage_add_percent = Right.mag_damage_add_percent();
    phy_damage_reduce_percent = Right.phy_damage_reduce_percent();
    mag_damage_reduce_percent = Right.mag_damage_reduce_percent();
    attack_monster_damage_add_percent = Right.attack_monster_damage_add_percent();
    take_monster_damage_reduce_percent = Right.take_monster_damage_reduce_percent();
    base_hp_add_percent = Right.base_hp_add_percent();
    base_mp_add_percent = Right.base_mp_add_percent();
    base_phy_att_add_percent = Right.base_phy_att_add_percent();
    base_mag_att_add_percent = Right.base_mag_att_add_percent();
    base_phy_def_add_percent = Right.base_phy_def_add_percent();
    base_mag_def_add_percent = Right.base_mag_def_add_percent();
    base_phy_hit_add_percent = Right.base_phy_hit_add_percent();
    base_mag_hit_add_percent = Right.base_mag_hit_add_percent();
    base_phy_dodge_add_percent = Right.base_phy_dodge_add_percent();
    base_mag_dodge_add_percent = Right.base_mag_dodge_add_percent();
    base_crit_add_percent = Right.base_crit_add_percent();
    base_crit_def_add_percent = Right.base_crit_def_add_percent();
}

void FPbRoleAttribute::ToPb(idlepb::RoleAttribute* Out) const
{
    Out->set_health(health);
    Out->set_max_health(max_health);
    Out->set_mana(mana);
    Out->set_max_mana(max_mana);
    Out->set_mind(mind);
    Out->set_strength(strength);
    Out->set_intellect(intellect);
    Out->set_agility(agility);
    Out->set_move_speed(move_speed);
    Out->set_phy_att(phy_att);
    Out->set_phy_def(phy_def);
    Out->set_mag_att(mag_att);
    Out->set_mag_def(mag_def);
    Out->set_phy_dodge(phy_dodge);
    Out->set_mag_dodge(mag_dodge);
    Out->set_phy_hit(phy_hit);
    Out->set_mag_hit(mag_hit);
    Out->set_crit(crit);
    Out->set_crit_def(crit_def);
    Out->set_hp_recover_percent(hp_recover_percent);
    Out->set_mp_recover_percent(mp_recover_percent);
    Out->set_crit_coef(crit_coef);
    Out->set_crit_block(crit_block);
    Out->set_crit_additional_damage(crit_additional_damage);
    Out->set_arena_money_add_percent(arena_money_add_percent);
    Out->set_mag_break(mag_break);
    Out->set_phy_break(phy_break);
    Out->set_mag_block(mag_block);
    Out->set_phy_block(phy_block);
    Out->set_fen_qi(fen_qi);
    Out->set_tuna_num(tuna_num);
    Out->set_tuna_add_percent(tuna_add_percent);
    Out->set_medicine_num(medicine_num);
    Out->set_medicine_add_percent(medicine_add_percent);
    Out->set_baseqi_add_percent(baseqi_add_percent);
    Out->set_shen_tong_damage_to_player_add_percent(shen_tong_damage_to_player_add_percent);
    Out->set_shen_tong_damage_to_player_reduce_percent(shen_tong_damage_to_player_reduce_percent);
    Out->set_fa_bao_damage_to_player_add_percent(fa_bao_damage_to_player_add_percent);
    Out->set_fa_bao_damage_to_player_reduce_percent(fa_bao_damage_to_player_reduce_percent);
    Out->set_phy_damage_add_percent(phy_damage_add_percent);
    Out->set_mag_damage_add_percent(mag_damage_add_percent);
    Out->set_phy_damage_reduce_percent(phy_damage_reduce_percent);
    Out->set_mag_damage_reduce_percent(mag_damage_reduce_percent);
    Out->set_attack_monster_damage_add_percent(attack_monster_damage_add_percent);
    Out->set_take_monster_damage_reduce_percent(take_monster_damage_reduce_percent);
    Out->set_base_hp_add_percent(base_hp_add_percent);
    Out->set_base_mp_add_percent(base_mp_add_percent);
    Out->set_base_phy_att_add_percent(base_phy_att_add_percent);
    Out->set_base_mag_att_add_percent(base_mag_att_add_percent);
    Out->set_base_phy_def_add_percent(base_phy_def_add_percent);
    Out->set_base_mag_def_add_percent(base_mag_def_add_percent);
    Out->set_base_phy_hit_add_percent(base_phy_hit_add_percent);
    Out->set_base_mag_hit_add_percent(base_mag_hit_add_percent);
    Out->set_base_phy_dodge_add_percent(base_phy_dodge_add_percent);
    Out->set_base_mag_dodge_add_percent(base_mag_dodge_add_percent);
    Out->set_base_crit_add_percent(base_crit_add_percent);
    Out->set_base_crit_def_add_percent(base_crit_def_add_percent);    
}

void FPbRoleAttribute::Reset()
{
    health = float();
    max_health = float();
    mana = float();
    max_mana = float();
    mind = float();
    strength = float();
    intellect = float();
    agility = float();
    move_speed = float();
    phy_att = float();
    phy_def = float();
    mag_att = float();
    mag_def = float();
    phy_dodge = float();
    mag_dodge = float();
    phy_hit = float();
    mag_hit = float();
    crit = float();
    crit_def = float();
    hp_recover_percent = float();
    mp_recover_percent = float();
    crit_coef = float();
    crit_block = float();
    crit_additional_damage = float();
    arena_money_add_percent = float();
    mag_break = float();
    phy_break = float();
    mag_block = float();
    phy_block = float();
    fen_qi = float();
    tuna_num = float();
    tuna_add_percent = float();
    medicine_num = float();
    medicine_add_percent = float();
    baseqi_add_percent = float();
    shen_tong_damage_to_player_add_percent = float();
    shen_tong_damage_to_player_reduce_percent = float();
    fa_bao_damage_to_player_add_percent = float();
    fa_bao_damage_to_player_reduce_percent = float();
    phy_damage_add_percent = float();
    mag_damage_add_percent = float();
    phy_damage_reduce_percent = float();
    mag_damage_reduce_percent = float();
    attack_monster_damage_add_percent = float();
    take_monster_damage_reduce_percent = float();
    base_hp_add_percent = float();
    base_mp_add_percent = float();
    base_phy_att_add_percent = float();
    base_mag_att_add_percent = float();
    base_phy_def_add_percent = float();
    base_mag_def_add_percent = float();
    base_phy_hit_add_percent = float();
    base_mag_hit_add_percent = float();
    base_phy_dodge_add_percent = float();
    base_mag_dodge_add_percent = float();
    base_crit_add_percent = float();
    base_crit_def_add_percent = float();    
}

void FPbRoleAttribute::operator=(const idlepb::RoleAttribute& Right)
{
    this->FromPb(Right);
}

bool FPbRoleAttribute::operator==(const FPbRoleAttribute& Right) const
{
    if (this->health != Right.health)
        return false;
    if (this->max_health != Right.max_health)
        return false;
    if (this->mana != Right.mana)
        return false;
    if (this->max_mana != Right.max_mana)
        return false;
    if (this->mind != Right.mind)
        return false;
    if (this->strength != Right.strength)
        return false;
    if (this->intellect != Right.intellect)
        return false;
    if (this->agility != Right.agility)
        return false;
    if (this->move_speed != Right.move_speed)
        return false;
    if (this->phy_att != Right.phy_att)
        return false;
    if (this->phy_def != Right.phy_def)
        return false;
    if (this->mag_att != Right.mag_att)
        return false;
    if (this->mag_def != Right.mag_def)
        return false;
    if (this->phy_dodge != Right.phy_dodge)
        return false;
    if (this->mag_dodge != Right.mag_dodge)
        return false;
    if (this->phy_hit != Right.phy_hit)
        return false;
    if (this->mag_hit != Right.mag_hit)
        return false;
    if (this->crit != Right.crit)
        return false;
    if (this->crit_def != Right.crit_def)
        return false;
    if (this->hp_recover_percent != Right.hp_recover_percent)
        return false;
    if (this->mp_recover_percent != Right.mp_recover_percent)
        return false;
    if (this->crit_coef != Right.crit_coef)
        return false;
    if (this->crit_block != Right.crit_block)
        return false;
    if (this->crit_additional_damage != Right.crit_additional_damage)
        return false;
    if (this->arena_money_add_percent != Right.arena_money_add_percent)
        return false;
    if (this->mag_break != Right.mag_break)
        return false;
    if (this->phy_break != Right.phy_break)
        return false;
    if (this->mag_block != Right.mag_block)
        return false;
    if (this->phy_block != Right.phy_block)
        return false;
    if (this->fen_qi != Right.fen_qi)
        return false;
    if (this->tuna_num != Right.tuna_num)
        return false;
    if (this->tuna_add_percent != Right.tuna_add_percent)
        return false;
    if (this->medicine_num != Right.medicine_num)
        return false;
    if (this->medicine_add_percent != Right.medicine_add_percent)
        return false;
    if (this->baseqi_add_percent != Right.baseqi_add_percent)
        return false;
    if (this->shen_tong_damage_to_player_add_percent != Right.shen_tong_damage_to_player_add_percent)
        return false;
    if (this->shen_tong_damage_to_player_reduce_percent != Right.shen_tong_damage_to_player_reduce_percent)
        return false;
    if (this->fa_bao_damage_to_player_add_percent != Right.fa_bao_damage_to_player_add_percent)
        return false;
    if (this->fa_bao_damage_to_player_reduce_percent != Right.fa_bao_damage_to_player_reduce_percent)
        return false;
    if (this->phy_damage_add_percent != Right.phy_damage_add_percent)
        return false;
    if (this->mag_damage_add_percent != Right.mag_damage_add_percent)
        return false;
    if (this->phy_damage_reduce_percent != Right.phy_damage_reduce_percent)
        return false;
    if (this->mag_damage_reduce_percent != Right.mag_damage_reduce_percent)
        return false;
    if (this->attack_monster_damage_add_percent != Right.attack_monster_damage_add_percent)
        return false;
    if (this->take_monster_damage_reduce_percent != Right.take_monster_damage_reduce_percent)
        return false;
    if (this->base_hp_add_percent != Right.base_hp_add_percent)
        return false;
    if (this->base_mp_add_percent != Right.base_mp_add_percent)
        return false;
    if (this->base_phy_att_add_percent != Right.base_phy_att_add_percent)
        return false;
    if (this->base_mag_att_add_percent != Right.base_mag_att_add_percent)
        return false;
    if (this->base_phy_def_add_percent != Right.base_phy_def_add_percent)
        return false;
    if (this->base_mag_def_add_percent != Right.base_mag_def_add_percent)
        return false;
    if (this->base_phy_hit_add_percent != Right.base_phy_hit_add_percent)
        return false;
    if (this->base_mag_hit_add_percent != Right.base_mag_hit_add_percent)
        return false;
    if (this->base_phy_dodge_add_percent != Right.base_phy_dodge_add_percent)
        return false;
    if (this->base_mag_dodge_add_percent != Right.base_mag_dodge_add_percent)
        return false;
    if (this->base_crit_add_percent != Right.base_crit_add_percent)
        return false;
    if (this->base_crit_def_add_percent != Right.base_crit_def_add_percent)
        return false;
    return true;
}

bool FPbRoleAttribute::operator!=(const FPbRoleAttribute& Right) const
{
    return !operator==(Right);
}

void* FPbRoleAttribute::GetMemberPtrByIndex(int32 Index)
{
    switch (Index)
    {
    case 1:
        return &health;
    case 2:
        return &max_health;
    case 3:
        return &mana;
    case 4:
        return &max_mana;
    case 5:
        return &mind;
    case 6:
        return &strength;
    case 7:
        return &intellect;
    case 8:
        return &agility;
    case 9:
        return &move_speed;
    case 10:
        return &phy_att;
    case 11:
        return &phy_def;
    case 12:
        return &mag_att;
    case 13:
        return &mag_def;
    case 14:
        return &phy_dodge;
    case 15:
        return &mag_dodge;
    case 16:
        return &phy_hit;
    case 17:
        return &mag_hit;
    case 18:
        return &crit;
    case 19:
        return &crit_def;
    case 20:
        return &hp_recover_percent;
    case 21:
        return &mp_recover_percent;
    case 22:
        return &crit_coef;
    case 23:
        return &crit_block;
    case 24:
        return &crit_additional_damage;
    case 25:
        return &arena_money_add_percent;
    case 26:
        return &mag_break;
    case 27:
        return &phy_break;
    case 28:
        return &mag_block;
    case 29:
        return &phy_block;
    case 30:
        return &fen_qi;
    case 200:
        return &tuna_num;
    case 201:
        return &tuna_add_percent;
    case 202:
        return &medicine_num;
    case 203:
        return &medicine_add_percent;
    case 204:
        return &baseqi_add_percent;
    case 205:
        return &shen_tong_damage_to_player_add_percent;
    case 206:
        return &shen_tong_damage_to_player_reduce_percent;
    case 207:
        return &fa_bao_damage_to_player_add_percent;
    case 208:
        return &fa_bao_damage_to_player_reduce_percent;
    case 209:
        return &phy_damage_add_percent;
    case 210:
        return &mag_damage_add_percent;
    case 211:
        return &phy_damage_reduce_percent;
    case 212:
        return &mag_damage_reduce_percent;
    case 213:
        return &attack_monster_damage_add_percent;
    case 214:
        return &take_monster_damage_reduce_percent;
    case 215:
        return &base_hp_add_percent;
    case 216:
        return &base_mp_add_percent;
    case 217:
        return &base_phy_att_add_percent;
    case 218:
        return &base_mag_att_add_percent;
    case 219:
        return &base_phy_def_add_percent;
    case 220:
        return &base_mag_def_add_percent;
    case 221:
        return &base_phy_hit_add_percent;
    case 222:
        return &base_mag_hit_add_percent;
    case 223:
        return &base_phy_dodge_add_percent;
    case 224:
        return &base_mag_dodge_add_percent;
    case 225:
        return &base_crit_add_percent;
    case 226:
        return &base_crit_def_add_percent;
    default:
        return nullptr;
    }
}

const void* FPbRoleAttribute::GetMemberPtrByIndex(int32 Index) const
{
    switch (Index)
    {
    case 1:
        return &health;
    case 2:
        return &max_health;
    case 3:
        return &mana;
    case 4:
        return &max_mana;
    case 5:
        return &mind;
    case 6:
        return &strength;
    case 7:
        return &intellect;
    case 8:
        return &agility;
    case 9:
        return &move_speed;
    case 10:
        return &phy_att;
    case 11:
        return &phy_def;
    case 12:
        return &mag_att;
    case 13:
        return &mag_def;
    case 14:
        return &phy_dodge;
    case 15:
        return &mag_dodge;
    case 16:
        return &phy_hit;
    case 17:
        return &mag_hit;
    case 18:
        return &crit;
    case 19:
        return &crit_def;
    case 20:
        return &hp_recover_percent;
    case 21:
        return &mp_recover_percent;
    case 22:
        return &crit_coef;
    case 23:
        return &crit_block;
    case 24:
        return &crit_additional_damage;
    case 25:
        return &arena_money_add_percent;
    case 26:
        return &mag_break;
    case 27:
        return &phy_break;
    case 28:
        return &mag_block;
    case 29:
        return &phy_block;
    case 30:
        return &fen_qi;
    case 200:
        return &tuna_num;
    case 201:
        return &tuna_add_percent;
    case 202:
        return &medicine_num;
    case 203:
        return &medicine_add_percent;
    case 204:
        return &baseqi_add_percent;
    case 205:
        return &shen_tong_damage_to_player_add_percent;
    case 206:
        return &shen_tong_damage_to_player_reduce_percent;
    case 207:
        return &fa_bao_damage_to_player_add_percent;
    case 208:
        return &fa_bao_damage_to_player_reduce_percent;
    case 209:
        return &phy_damage_add_percent;
    case 210:
        return &mag_damage_add_percent;
    case 211:
        return &phy_damage_reduce_percent;
    case 212:
        return &mag_damage_reduce_percent;
    case 213:
        return &attack_monster_damage_add_percent;
    case 214:
        return &take_monster_damage_reduce_percent;
    case 215:
        return &base_hp_add_percent;
    case 216:
        return &base_mp_add_percent;
    case 217:
        return &base_phy_att_add_percent;
    case 218:
        return &base_mag_att_add_percent;
    case 219:
        return &base_phy_def_add_percent;
    case 220:
        return &base_mag_def_add_percent;
    case 221:
        return &base_phy_hit_add_percent;
    case 222:
        return &base_mag_hit_add_percent;
    case 223:
        return &base_phy_dodge_add_percent;
    case 224:
        return &base_mag_dodge_add_percent;
    case 225:
        return &base_crit_add_percent;
    case 226:
        return &base_crit_def_add_percent;
    default:
        return nullptr;
    }
}

const char* FPbRoleAttribute::GetMemberTypeNameByIndex(int32 Index) const
{
    switch (Index)
    {
    case 1:
        return "float";  // health
    case 2:
        return "float";  // max_health
    case 3:
        return "float";  // mana
    case 4:
        return "float";  // max_mana
    case 5:
        return "float";  // mind
    case 6:
        return "float";  // strength
    case 7:
        return "float";  // intellect
    case 8:
        return "float";  // agility
    case 9:
        return "float";  // move_speed
    case 10:
        return "float";  // phy_att
    case 11:
        return "float";  // phy_def
    case 12:
        return "float";  // mag_att
    case 13:
        return "float";  // mag_def
    case 14:
        return "float";  // phy_dodge
    case 15:
        return "float";  // mag_dodge
    case 16:
        return "float";  // phy_hit
    case 17:
        return "float";  // mag_hit
    case 18:
        return "float";  // crit
    case 19:
        return "float";  // crit_def
    case 20:
        return "float";  // hp_recover_percent
    case 21:
        return "float";  // mp_recover_percent
    case 22:
        return "float";  // crit_coef
    case 23:
        return "float";  // crit_block
    case 24:
        return "float";  // crit_additional_damage
    case 25:
        return "float";  // arena_money_add_percent
    case 26:
        return "float";  // mag_break
    case 27:
        return "float";  // phy_break
    case 28:
        return "float";  // mag_block
    case 29:
        return "float";  // phy_block
    case 30:
        return "float";  // fen_qi
    case 200:
        return "float";  // tuna_num
    case 201:
        return "float";  // tuna_add_percent
    case 202:
        return "float";  // medicine_num
    case 203:
        return "float";  // medicine_add_percent
    case 204:
        return "float";  // baseqi_add_percent
    case 205:
        return "float";  // shen_tong_damage_to_player_add_percent
    case 206:
        return "float";  // shen_tong_damage_to_player_reduce_percent
    case 207:
        return "float";  // fa_bao_damage_to_player_add_percent
    case 208:
        return "float";  // fa_bao_damage_to_player_reduce_percent
    case 209:
        return "float";  // phy_damage_add_percent
    case 210:
        return "float";  // mag_damage_add_percent
    case 211:
        return "float";  // phy_damage_reduce_percent
    case 212:
        return "float";  // mag_damage_reduce_percent
    case 213:
        return "float";  // attack_monster_damage_add_percent
    case 214:
        return "float";  // take_monster_damage_reduce_percent
    case 215:
        return "float";  // base_hp_add_percent
    case 216:
        return "float";  // base_mp_add_percent
    case 217:
        return "float";  // base_phy_att_add_percent
    case 218:
        return "float";  // base_mag_att_add_percent
    case 219:
        return "float";  // base_phy_def_add_percent
    case 220:
        return "float";  // base_mag_def_add_percent
    case 221:
        return "float";  // base_phy_hit_add_percent
    case 222:
        return "float";  // base_mag_hit_add_percent
    case 223:
        return "float";  // base_phy_dodge_add_percent
    case 224:
        return "float";  // base_mag_dodge_add_percent
    case 225:
        return "float";  // base_crit_add_percent
    case 226:
        return "float";  // base_crit_def_add_percent
    default:
        return nullptr;
    }
}

void FPbRoleAttribute::SimplePlus(const FPbRoleAttribute& Right)
{
    this->health += Right.health;
    this->max_health += Right.max_health;
    this->mana += Right.mana;
    this->max_mana += Right.max_mana;
    this->mind += Right.mind;
    this->strength += Right.strength;
    this->intellect += Right.intellect;
    this->agility += Right.agility;
    this->move_speed += Right.move_speed;
    this->phy_att += Right.phy_att;
    this->phy_def += Right.phy_def;
    this->mag_att += Right.mag_att;
    this->mag_def += Right.mag_def;
    this->phy_dodge += Right.phy_dodge;
    this->mag_dodge += Right.mag_dodge;
    this->phy_hit += Right.phy_hit;
    this->mag_hit += Right.mag_hit;
    this->crit += Right.crit;
    this->crit_def += Right.crit_def;
    this->hp_recover_percent += Right.hp_recover_percent;
    this->mp_recover_percent += Right.mp_recover_percent;
    this->crit_coef += Right.crit_coef;
    this->crit_block += Right.crit_block;
    this->crit_additional_damage += Right.crit_additional_damage;
    this->arena_money_add_percent += Right.arena_money_add_percent;
    this->mag_break += Right.mag_break;
    this->phy_break += Right.phy_break;
    this->mag_block += Right.mag_block;
    this->phy_block += Right.phy_block;
    this->fen_qi += Right.fen_qi;
    this->tuna_num += Right.tuna_num;
    this->tuna_add_percent += Right.tuna_add_percent;
    this->medicine_num += Right.medicine_num;
    this->medicine_add_percent += Right.medicine_add_percent;
    this->baseqi_add_percent += Right.baseqi_add_percent;
    this->shen_tong_damage_to_player_add_percent += Right.shen_tong_damage_to_player_add_percent;
    this->shen_tong_damage_to_player_reduce_percent += Right.shen_tong_damage_to_player_reduce_percent;
    this->fa_bao_damage_to_player_add_percent += Right.fa_bao_damage_to_player_add_percent;
    this->fa_bao_damage_to_player_reduce_percent += Right.fa_bao_damage_to_player_reduce_percent;
    this->phy_damage_add_percent += Right.phy_damage_add_percent;
    this->mag_damage_add_percent += Right.mag_damage_add_percent;
    this->phy_damage_reduce_percent += Right.phy_damage_reduce_percent;
    this->mag_damage_reduce_percent += Right.mag_damage_reduce_percent;
    this->attack_monster_damage_add_percent += Right.attack_monster_damage_add_percent;
    this->take_monster_damage_reduce_percent += Right.take_monster_damage_reduce_percent;
    this->base_hp_add_percent += Right.base_hp_add_percent;
    this->base_mp_add_percent += Right.base_mp_add_percent;
    this->base_phy_att_add_percent += Right.base_phy_att_add_percent;
    this->base_mag_att_add_percent += Right.base_mag_att_add_percent;
    this->base_phy_def_add_percent += Right.base_phy_def_add_percent;
    this->base_mag_def_add_percent += Right.base_mag_def_add_percent;
    this->base_phy_hit_add_percent += Right.base_phy_hit_add_percent;
    this->base_mag_hit_add_percent += Right.base_mag_hit_add_percent;
    this->base_phy_dodge_add_percent += Right.base_phy_dodge_add_percent;
    this->base_mag_dodge_add_percent += Right.base_mag_dodge_add_percent;
    this->base_crit_add_percent += Right.base_crit_add_percent;
    this->base_crit_def_add_percent += Right.base_crit_def_add_percent;
}


FPbRankData::FPbRankData()
{
    Reset();        
}

FPbRankData::FPbRankData(const idlepb::RankData& Right)
{
    this->FromPb(Right);
}

void FPbRankData::FromPb(const idlepb::RankData& Right)
{
    rank = Right.rank();
    exp = Right.exp();
    layer = Right.layer();
    stage = Right.stage();
    degree = Right.degree();
    breakthrough_type = static_cast<EPbBreakthroughType>(Right.breakthrough_type());
    lose_add_probability = Right.lose_add_probability();
    lose_recover_timestamp = Right.lose_recover_timestamp();
    stage_add_att = Right.stage_add_att();
}

void FPbRankData::ToPb(idlepb::RankData* Out) const
{
    Out->set_rank(rank);
    Out->set_exp(exp);
    Out->set_layer(layer);
    Out->set_stage(stage);
    Out->set_degree(degree);
    Out->set_breakthrough_type(static_cast<idlepb::BreakthroughType>(breakthrough_type));
    Out->set_lose_add_probability(lose_add_probability);
    Out->set_lose_recover_timestamp(lose_recover_timestamp);
    Out->set_stage_add_att(stage_add_att);    
}

void FPbRankData::Reset()
{
    rank = int32();
    exp = float();
    layer = int32();
    stage = int32();
    degree = int32();
    breakthrough_type = EPbBreakthroughType();
    lose_add_probability = int32();
    lose_recover_timestamp = int64();
    stage_add_att = int64();    
}

void FPbRankData::operator=(const idlepb::RankData& Right)
{
    this->FromPb(Right);
}

bool FPbRankData::operator==(const FPbRankData& Right) const
{
    if (this->rank != Right.rank)
        return false;
    if (this->exp != Right.exp)
        return false;
    if (this->layer != Right.layer)
        return false;
    if (this->stage != Right.stage)
        return false;
    if (this->degree != Right.degree)
        return false;
    if (this->breakthrough_type != Right.breakthrough_type)
        return false;
    if (this->lose_add_probability != Right.lose_add_probability)
        return false;
    if (this->lose_recover_timestamp != Right.lose_recover_timestamp)
        return false;
    if (this->stage_add_att != Right.stage_add_att)
        return false;
    return true;
}

bool FPbRankData::operator!=(const FPbRankData& Right) const
{
    return !operator==(Right);
}

FPbBreathingReward::FPbBreathingReward()
{
    Reset();        
}

FPbBreathingReward::FPbBreathingReward(const idlepb::BreathingReward& Right)
{
    this->FromPb(Right);
}

void FPbBreathingReward::FromPb(const idlepb::BreathingReward& Right)
{
    index = Right.index();
    item_id.Empty();
    for (const auto& Elem : Right.item_id())
    {
        item_id.Emplace(Elem);
    }
    num.Empty();
    for (const auto& Elem : Right.num())
    {
        num.Emplace(Elem);
    }
    dir = Right.dir();
    received = Right.received();
}

void FPbBreathingReward::ToPb(idlepb::BreathingReward* Out) const
{
    Out->set_index(index);
    for (const auto& Elem : item_id)
    {
        Out->add_item_id(Elem);    
    }
    for (const auto& Elem : num)
    {
        Out->add_num(Elem);    
    }
    Out->set_dir(dir);
    Out->set_received(received);    
}

void FPbBreathingReward::Reset()
{
    index = int32();
    item_id = TArray<int32>();
    num = TArray<int32>();
    dir = int32();
    received = bool();    
}

void FPbBreathingReward::operator=(const idlepb::BreathingReward& Right)
{
    this->FromPb(Right);
}

bool FPbBreathingReward::operator==(const FPbBreathingReward& Right) const
{
    if (this->index != Right.index)
        return false;
    if (this->item_id != Right.item_id)
        return false;
    if (this->num != Right.num)
        return false;
    if (this->dir != Right.dir)
        return false;
    if (this->received != Right.received)
        return false;
    return true;
}

bool FPbBreathingReward::operator!=(const FPbBreathingReward& Right) const
{
    return !operator==(Right);
}

FPbCommonCultivationData::FPbCommonCultivationData()
{
    Reset();        
}

FPbCommonCultivationData::FPbCommonCultivationData(const idlepb::CommonCultivationData& Right)
{
    this->FromPb(Right);
}

void FPbCommonCultivationData::FromPb(const idlepb::CommonCultivationData& Right)
{
    breathing_rewards.Empty();
    for (const auto& Elem : Right.breathing_rewards())
    {
        breathing_rewards.Emplace(Elem);
    }
    merge_breathing = Right.merge_breathing();
}

void FPbCommonCultivationData::ToPb(idlepb::CommonCultivationData* Out) const
{
    for (const auto& Elem : breathing_rewards)
    {
        Elem.ToPb(Out->add_breathing_rewards());    
    }
    Out->set_merge_breathing(merge_breathing);    
}

void FPbCommonCultivationData::Reset()
{
    breathing_rewards = TArray<FPbBreathingReward>();
    merge_breathing = bool();    
}

void FPbCommonCultivationData::operator=(const idlepb::CommonCultivationData& Right)
{
    this->FromPb(Right);
}

bool FPbCommonCultivationData::operator==(const FPbCommonCultivationData& Right) const
{
    if (this->breathing_rewards != Right.breathing_rewards)
        return false;
    if (this->merge_breathing != Right.merge_breathing)
        return false;
    return true;
}

bool FPbCommonCultivationData::operator!=(const FPbCommonCultivationData& Right) const
{
    return !operator==(Right);
}

FPbCultivationData::FPbCultivationData()
{
    Reset();        
}

FPbCultivationData::FPbCultivationData(const idlepb::CultivationData& Right)
{
    this->FromPb(Right);
}

void FPbCultivationData::FromPb(const idlepb::CultivationData& Right)
{
    rank_data = Right.rank_data();
}

void FPbCultivationData::ToPb(idlepb::CultivationData* Out) const
{
    rank_data.ToPb(Out->mutable_rank_data());    
}

void FPbCultivationData::Reset()
{
    rank_data = FPbRankData();    
}

void FPbCultivationData::operator=(const idlepb::CultivationData& Right)
{
    this->FromPb(Right);
}

bool FPbCultivationData::operator==(const FPbCultivationData& Right) const
{
    if (this->rank_data != Right.rank_data)
        return false;
    return true;
}

bool FPbCultivationData::operator!=(const FPbCultivationData& Right) const
{
    return !operator==(Right);
}

bool CheckEPbRoleDailyCounterTypeValid(int32 Val)
{
    return idlepb::RoleDailyCounterType_IsValid(Val);
}

const TCHAR* GetEPbRoleDailyCounterTypeDescription(EPbRoleDailyCounterType Val)
{
    switch (Val)
    {
        case EPbRoleDailyCounterType::RDCT_BreathingExerciseTimes: return TEXT("今日吐纳次数");
        case EPbRoleDailyCounterType::RDCT_TakeMedicineTimes: return TEXT("今日服药次数");
        case EPbRoleDailyCounterType::RDCT_LeaderboardClickLikeNum: return TEXT("排行榜今日已点赞次数");
        case EPbRoleDailyCounterType::RDCT_AlchemyTimes: return TEXT("今日炼丹次数");
        case EPbRoleDailyCounterType::RDCT_ForgeTimes: return TEXT("今日炼器次数");
        case EPbRoleDailyCounterType::RDCT_UseExtraMaterialsTimes: return TEXT("今日炼器使用辅材次数");
        case EPbRoleDailyCounterType::RDCT_TotalBreathingExerciseTimes: return TEXT("今日总共吐纳次数");
        case EPbRoleDailyCounterType::RDCT_ForgeProduceQuality_None: return TEXT("今日炼制产出品质统计-其他");
        case EPbRoleDailyCounterType::RDCT_ForgeProduceQuality_White: return TEXT("今日炼制产出品质统计-白");
        case EPbRoleDailyCounterType::RDCT_ForgeProduceQuality_Green: return TEXT("今日炼制产出品质统计-绿");
        case EPbRoleDailyCounterType::RDCT_ForgeProduceQuality_Blue: return TEXT("今日炼制产出品质统计-蓝");
        case EPbRoleDailyCounterType::RDCT_ForgeProduceQuality_Purple: return TEXT("今日炼制产出品质统计-紫");
        case EPbRoleDailyCounterType::RDCT_ForgeProduceQuality_Orange: return TEXT("今日炼制产出品质统计-橙");
        case EPbRoleDailyCounterType::RDCT_ForgeProduceQuality_Red: return TEXT("今日炼制产出品质统计-红");
        case EPbRoleDailyCounterType::RDCT_GiftPackage_Other: return TEXT("今日礼包打开数目统计-其它");
        case EPbRoleDailyCounterType::RDCT_GiftPackage_Phy: return TEXT("今日礼包打开数目统计-炼体福袋");
        case EPbRoleDailyCounterType::RDCT_GiftPackage_Magic: return TEXT("今日礼包打开数目统计-修法福袋");
        case EPbRoleDailyCounterType::RDCT_GiftPackage_Money: return TEXT("今日礼包打开数目统计-灵石宝箱");
        case EPbRoleDailyCounterType::RDCT_GiftPackage_Weapon: return TEXT("今日礼包打开数目统计-仙品武器");
        case EPbRoleDailyCounterType::RDCT_GiftPackage_Treasure: return TEXT("今日礼包打开数目统计-仙品法宝");
        case EPbRoleDailyCounterType::RDCT_GiftPackage_Materials: return TEXT("今日礼包打开数目统计-古宝注灵材料");
        case EPbRoleDailyCounterType::RDCT_GiftPackage_GrabBag: return TEXT("今日礼包打开数目统计-福袋");
        case EPbRoleDailyCounterType::RDCT_GiftPackage_MonsterInvasion: return TEXT("今日礼包打开数目统计-神兽入侵宝箱");
        case EPbRoleDailyCounterType::RDCT_GiftPackage_StorageBag: return TEXT("今日礼包打开数目统计-储物袋");
        case EPbRoleDailyCounterType::RDCT_GiftPackage_Select: return TEXT("今日礼包打开数目统计-自选礼包");
        case EPbRoleDailyCounterType::RDCT_MonsterTowerChallengeTimes: return TEXT("今日挑战镇妖塔次数");
        case EPbRoleDailyCounterType::RDCT_MonsterTowerClosedDoorTrainingTimes: return TEXT("今日镇妖塔闭关次数");
        case EPbRoleDailyCounterType::RDCT_FriendlySoloTimes: return TEXT("今日切磋次数");
        case EPbRoleDailyCounterType::RDCT_SwordPkTimes: return TEXT("今日论剑台次数");
        case EPbRoleDailyCounterType::RDCT_ExchangeHeroCard: return TEXT("今日兑换英雄帖次数");
        case EPbRoleDailyCounterType::RDCT_TodaySeptConstructTimes: return TEXT("今日宗门建设次数");
        case EPbRoleDailyCounterType::RDCT_TodaySearchSeptByNameTimes: return TEXT("今日使用名字搜索宗门次数");
        case EPbRoleDailyCounterType::RDCT_GatherSeptStoneSeconds: return TEXT("本日采集矿脉时长(秒)");
        case EPbRoleDailyCounterType::RDCT_MonsterTowerClickLikeNum: return TEXT("镇妖塔排行榜今日已点赞次数");
        case EPbRoleDailyCounterType::RDCT_FarmlandWatering: return TEXT("药园本日浇灌次数");
        case EPbRoleDailyCounterType::RDCT_FriendRequestNum: return TEXT("本日好友请求次数");
        case EPbRoleDailyCounterType::RDCT_FriendSearchNum: return TEXT("本日道友查找次数");
        case EPbRoleDailyCounterType::RDCT_FuZeReward: return TEXT("本日领取福泽");
    }
    return TEXT("UNKNOWN");
}

FPbRoleDailyCounterEntry::FPbRoleDailyCounterEntry()
{
    Reset();        
}

FPbRoleDailyCounterEntry::FPbRoleDailyCounterEntry(const idlepb::RoleDailyCounterEntry& Right)
{
    this->FromPb(Right);
}

void FPbRoleDailyCounterEntry::FromPb(const idlepb::RoleDailyCounterEntry& Right)
{
    type = static_cast<EPbRoleDailyCounterType>(Right.type());
    num = Right.num();
}

void FPbRoleDailyCounterEntry::ToPb(idlepb::RoleDailyCounterEntry* Out) const
{
    Out->set_type(static_cast<idlepb::RoleDailyCounterType>(type));
    Out->set_num(num);    
}

void FPbRoleDailyCounterEntry::Reset()
{
    type = EPbRoleDailyCounterType();
    num = int32();    
}

void FPbRoleDailyCounterEntry::operator=(const idlepb::RoleDailyCounterEntry& Right)
{
    this->FromPb(Right);
}

bool FPbRoleDailyCounterEntry::operator==(const FPbRoleDailyCounterEntry& Right) const
{
    if (this->type != Right.type)
        return false;
    if (this->num != Right.num)
        return false;
    return true;
}

bool FPbRoleDailyCounterEntry::operator!=(const FPbRoleDailyCounterEntry& Right) const
{
    return !operator==(Right);
}

FPbRoleDailyCounter::FPbRoleDailyCounter()
{
    Reset();        
}

FPbRoleDailyCounter::FPbRoleDailyCounter(const idlepb::RoleDailyCounter& Right)
{
    this->FromPb(Right);
}

void FPbRoleDailyCounter::FromPb(const idlepb::RoleDailyCounter& Right)
{
    entries.Empty();
    for (const auto& Elem : Right.entries())
    {
        entries.Emplace(Elem);
    }
    last_reset_time = Right.last_reset_time();
}

void FPbRoleDailyCounter::ToPb(idlepb::RoleDailyCounter* Out) const
{
    for (const auto& Elem : entries)
    {
        Elem.ToPb(Out->add_entries());    
    }
    Out->set_last_reset_time(last_reset_time);    
}

void FPbRoleDailyCounter::Reset()
{
    entries = TArray<FPbRoleDailyCounterEntry>();
    last_reset_time = int64();    
}

void FPbRoleDailyCounter::operator=(const idlepb::RoleDailyCounter& Right)
{
    this->FromPb(Right);
}

bool FPbRoleDailyCounter::operator==(const FPbRoleDailyCounter& Right) const
{
    if (this->entries != Right.entries)
        return false;
    if (this->last_reset_time != Right.last_reset_time)
        return false;
    return true;
}

bool FPbRoleDailyCounter::operator!=(const FPbRoleDailyCounter& Right) const
{
    return !operator==(Right);
}

bool CheckEPbRoleWeeklyCounterTypeValid(int32 Val)
{
    return idlepb::RoleWeeklyCounterType_IsValid(Val);
}

const TCHAR* GetEPbRoleWeeklyCounterTypeDescription(EPbRoleWeeklyCounterType Val)
{
    switch (Val)
    {
        case EPbRoleWeeklyCounterType::RWCT_GatherSeptStoneSeconds: return TEXT("本周采集矿脉时长(秒)");
    }
    return TEXT("UNKNOWN");
}

FPbRoleWeeklyCounterEntry::FPbRoleWeeklyCounterEntry()
{
    Reset();        
}

FPbRoleWeeklyCounterEntry::FPbRoleWeeklyCounterEntry(const idlepb::RoleWeeklyCounterEntry& Right)
{
    this->FromPb(Right);
}

void FPbRoleWeeklyCounterEntry::FromPb(const idlepb::RoleWeeklyCounterEntry& Right)
{
    type = static_cast<EPbRoleWeeklyCounterType>(Right.type());
    num = Right.num();
}

void FPbRoleWeeklyCounterEntry::ToPb(idlepb::RoleWeeklyCounterEntry* Out) const
{
    Out->set_type(static_cast<idlepb::RoleWeeklyCounterType>(type));
    Out->set_num(num);    
}

void FPbRoleWeeklyCounterEntry::Reset()
{
    type = EPbRoleWeeklyCounterType();
    num = int32();    
}

void FPbRoleWeeklyCounterEntry::operator=(const idlepb::RoleWeeklyCounterEntry& Right)
{
    this->FromPb(Right);
}

bool FPbRoleWeeklyCounterEntry::operator==(const FPbRoleWeeklyCounterEntry& Right) const
{
    if (this->type != Right.type)
        return false;
    if (this->num != Right.num)
        return false;
    return true;
}

bool FPbRoleWeeklyCounterEntry::operator!=(const FPbRoleWeeklyCounterEntry& Right) const
{
    return !operator==(Right);
}

FPbRoleWeeklyCounter::FPbRoleWeeklyCounter()
{
    Reset();        
}

FPbRoleWeeklyCounter::FPbRoleWeeklyCounter(const idlepb::RoleWeeklyCounter& Right)
{
    this->FromPb(Right);
}

void FPbRoleWeeklyCounter::FromPb(const idlepb::RoleWeeklyCounter& Right)
{
    entries.Empty();
    for (const auto& Elem : Right.entries())
    {
        entries.Emplace(Elem);
    }
    last_reset_time = Right.last_reset_time();
}

void FPbRoleWeeklyCounter::ToPb(idlepb::RoleWeeklyCounter* Out) const
{
    for (const auto& Elem : entries)
    {
        Elem.ToPb(Out->add_entries());    
    }
    Out->set_last_reset_time(last_reset_time);    
}

void FPbRoleWeeklyCounter::Reset()
{
    entries = TArray<FPbRoleWeeklyCounterEntry>();
    last_reset_time = int64();    
}

void FPbRoleWeeklyCounter::operator=(const idlepb::RoleWeeklyCounter& Right)
{
    this->FromPb(Right);
}

bool FPbRoleWeeklyCounter::operator==(const FPbRoleWeeklyCounter& Right) const
{
    if (this->entries != Right.entries)
        return false;
    if (this->last_reset_time != Right.last_reset_time)
        return false;
    return true;
}

bool FPbRoleWeeklyCounter::operator!=(const FPbRoleWeeklyCounter& Right) const
{
    return !operator==(Right);
}

FPbCurrencyEntry::FPbCurrencyEntry()
{
    Reset();        
}

FPbCurrencyEntry::FPbCurrencyEntry(const idlepb::CurrencyEntry& Right)
{
    this->FromPb(Right);
}

void FPbCurrencyEntry::FromPb(const idlepb::CurrencyEntry& Right)
{
    type = static_cast<EPbCurrencyType>(Right.type());
    num = Right.num();
}

void FPbCurrencyEntry::ToPb(idlepb::CurrencyEntry* Out) const
{
    Out->set_type(static_cast<idlepb::CurrencyType>(type));
    Out->set_num(num);    
}

void FPbCurrencyEntry::Reset()
{
    type = EPbCurrencyType();
    num = int64();    
}

void FPbCurrencyEntry::operator=(const idlepb::CurrencyEntry& Right)
{
    this->FromPb(Right);
}

bool FPbCurrencyEntry::operator==(const FPbCurrencyEntry& Right) const
{
    if (this->type != Right.type)
        return false;
    if (this->num != Right.num)
        return false;
    return true;
}

bool FPbCurrencyEntry::operator!=(const FPbCurrencyEntry& Right) const
{
    return !operator==(Right);
}

FPbCurrencyData::FPbCurrencyData()
{
    Reset();        
}

FPbCurrencyData::FPbCurrencyData(const idlepb::CurrencyData& Right)
{
    this->FromPb(Right);
}

void FPbCurrencyData::FromPb(const idlepb::CurrencyData& Right)
{
    currencies.Empty();
    for (const auto& Elem : Right.currencies())
    {
        currencies.Emplace(Elem);
    }
}

void FPbCurrencyData::ToPb(idlepb::CurrencyData* Out) const
{
    for (const auto& Elem : currencies)
    {
        Elem.ToPb(Out->add_currencies());    
    }    
}

void FPbCurrencyData::Reset()
{
    currencies = TArray<FPbCurrencyEntry>();    
}

void FPbCurrencyData::operator=(const idlepb::CurrencyData& Right)
{
    this->FromPb(Right);
}

bool FPbCurrencyData::operator==(const FPbCurrencyData& Right) const
{
    if (this->currencies != Right.currencies)
        return false;
    return true;
}

bool FPbCurrencyData::operator!=(const FPbCurrencyData& Right) const
{
    return !operator==(Right);
}

FPbEquipPerkEntry::FPbEquipPerkEntry()
{
    Reset();        
}

FPbEquipPerkEntry::FPbEquipPerkEntry(const idlepb::EquipPerkEntry& Right)
{
    this->FromPb(Right);
}

void FPbEquipPerkEntry::FromPb(const idlepb::EquipPerkEntry& Right)
{
    id = Right.id();
    quality = static_cast<EPbItemQuality>(Right.quality());
    num = Right.num();
}

void FPbEquipPerkEntry::ToPb(idlepb::EquipPerkEntry* Out) const
{
    Out->set_id(id);
    Out->set_quality(static_cast<idlepb::ItemQuality>(quality));
    Out->set_num(num);    
}

void FPbEquipPerkEntry::Reset()
{
    id = int32();
    quality = EPbItemQuality();
    num = int32();    
}

void FPbEquipPerkEntry::operator=(const idlepb::EquipPerkEntry& Right)
{
    this->FromPb(Right);
}

bool FPbEquipPerkEntry::operator==(const FPbEquipPerkEntry& Right) const
{
    if (this->id != Right.id)
        return false;
    if (this->quality != Right.quality)
        return false;
    if (this->num != Right.num)
        return false;
    return true;
}

bool FPbEquipPerkEntry::operator!=(const FPbEquipPerkEntry& Right) const
{
    return !operator==(Right);
}

FPbSkillEquipmentAttributes::FPbSkillEquipmentAttributes()
{
    Reset();        
}

FPbSkillEquipmentAttributes::FPbSkillEquipmentAttributes(const idlepb::SkillEquipmentAttributes& Right)
{
    this->FromPb(Right);
}

void FPbSkillEquipmentAttributes::FromPb(const idlepb::SkillEquipmentAttributes& Right)
{
    cool_down = Right.cool_down();
    target_num = Right.target_num();
    attack_count = Right.attack_count();
    phy_coefficient = Right.phy_coefficient();
    phy_damage = Right.phy_damage();
    mag_coefficient = Right.mag_coefficient();
    mag_damage = Right.mag_damage();
    max_damage = Right.max_damage();
    effects.Empty();
    for (const auto& Elem : Right.effects())
    {
        effects.Emplace(Elem);
    }
    shield_effects.Empty();
    for (const auto& Elem : Right.shield_effects())
    {
        shield_effects.Emplace(Elem);
    }
}

void FPbSkillEquipmentAttributes::ToPb(idlepb::SkillEquipmentAttributes* Out) const
{
    Out->set_cool_down(cool_down);
    Out->set_target_num(target_num);
    Out->set_attack_count(attack_count);
    Out->set_phy_coefficient(phy_coefficient);
    Out->set_phy_damage(phy_damage);
    Out->set_mag_coefficient(mag_coefficient);
    Out->set_mag_damage(mag_damage);
    Out->set_max_damage(max_damage);
    for (const auto& Elem : effects)
    {
        Elem.ToPb(Out->add_effects());    
    }
    for (const auto& Elem : shield_effects)
    {
        Elem.ToPb(Out->add_shield_effects());    
    }    
}

void FPbSkillEquipmentAttributes::Reset()
{
    cool_down = float();
    target_num = int32();
    attack_count = int32();
    phy_coefficient = float();
    phy_damage = float();
    mag_coefficient = float();
    mag_damage = float();
    max_damage = float();
    effects = TArray<FPbAbilityEffectData>();
    shield_effects = TArray<FPbAbilityEffectData>();    
}

void FPbSkillEquipmentAttributes::operator=(const idlepb::SkillEquipmentAttributes& Right)
{
    this->FromPb(Right);
}

bool FPbSkillEquipmentAttributes::operator==(const FPbSkillEquipmentAttributes& Right) const
{
    if (this->cool_down != Right.cool_down)
        return false;
    if (this->target_num != Right.target_num)
        return false;
    if (this->attack_count != Right.attack_count)
        return false;
    if (this->phy_coefficient != Right.phy_coefficient)
        return false;
    if (this->phy_damage != Right.phy_damage)
        return false;
    if (this->mag_coefficient != Right.mag_coefficient)
        return false;
    if (this->mag_damage != Right.mag_damage)
        return false;
    if (this->max_damage != Right.max_damage)
        return false;
    if (this->effects != Right.effects)
        return false;
    if (this->shield_effects != Right.shield_effects)
        return false;
    return true;
}

bool FPbSkillEquipmentAttributes::operator!=(const FPbSkillEquipmentAttributes& Right) const
{
    return !operator==(Right);
}

FPbSkillEquipmentData::FPbSkillEquipmentData()
{
    Reset();        
}

FPbSkillEquipmentData::FPbSkillEquipmentData(const idlepb::SkillEquipmentData& Right)
{
    this->FromPb(Right);
}

void FPbSkillEquipmentData::FromPb(const idlepb::SkillEquipmentData& Right)
{
    attributes = Right.attributes();
    reinforce_attributes = Right.reinforce_attributes();
    qiwen_attributes = Right.qiwen_attributes();
    jinglian_attributes = Right.jinglian_attributes();
}

void FPbSkillEquipmentData::ToPb(idlepb::SkillEquipmentData* Out) const
{
    attributes.ToPb(Out->mutable_attributes());
    reinforce_attributes.ToPb(Out->mutable_reinforce_attributes());
    qiwen_attributes.ToPb(Out->mutable_qiwen_attributes());
    jinglian_attributes.ToPb(Out->mutable_jinglian_attributes());    
}

void FPbSkillEquipmentData::Reset()
{
    attributes = FPbSkillEquipmentAttributes();
    reinforce_attributes = FPbSkillEquipmentAttributes();
    qiwen_attributes = FPbSkillEquipmentAttributes();
    jinglian_attributes = FPbSkillEquipmentAttributes();    
}

void FPbSkillEquipmentData::operator=(const idlepb::SkillEquipmentData& Right)
{
    this->FromPb(Right);
}

bool FPbSkillEquipmentData::operator==(const FPbSkillEquipmentData& Right) const
{
    if (this->attributes != Right.attributes)
        return false;
    if (this->reinforce_attributes != Right.reinforce_attributes)
        return false;
    if (this->qiwen_attributes != Right.qiwen_attributes)
        return false;
    if (this->jinglian_attributes != Right.jinglian_attributes)
        return false;
    return true;
}

bool FPbSkillEquipmentData::operator!=(const FPbSkillEquipmentData& Right) const
{
    return !operator==(Right);
}

FPbCollectionEntry::FPbCollectionEntry()
{
    Reset();        
}

FPbCollectionEntry::FPbCollectionEntry(const idlepb::CollectionEntry& Right)
{
    this->FromPb(Right);
}

void FPbCollectionEntry::FromPb(const idlepb::CollectionEntry& Right)
{
    id = Right.id();
    level = Right.level();
    star = Right.star();
    is_activated = Right.is_activated();
    piece_num = Right.piece_num();
    life_num = Right.life_num();
    combat_power = Right.combat_power();
}

void FPbCollectionEntry::ToPb(idlepb::CollectionEntry* Out) const
{
    Out->set_id(id);
    Out->set_level(level);
    Out->set_star(star);
    Out->set_is_activated(is_activated);
    Out->set_piece_num(piece_num);
    Out->set_life_num(life_num);
    Out->set_combat_power(combat_power);    
}

void FPbCollectionEntry::Reset()
{
    id = int32();
    level = int32();
    star = int32();
    is_activated = bool();
    piece_num = int32();
    life_num = int32();
    combat_power = float();    
}

void FPbCollectionEntry::operator=(const idlepb::CollectionEntry& Right)
{
    this->FromPb(Right);
}

bool FPbCollectionEntry::operator==(const FPbCollectionEntry& Right) const
{
    if (this->id != Right.id)
        return false;
    if (this->level != Right.level)
        return false;
    if (this->star != Right.star)
        return false;
    if (this->is_activated != Right.is_activated)
        return false;
    if (this->piece_num != Right.piece_num)
        return false;
    if (this->life_num != Right.life_num)
        return false;
    if (this->combat_power != Right.combat_power)
        return false;
    return true;
}

bool FPbCollectionEntry::operator!=(const FPbCollectionEntry& Right) const
{
    return !operator==(Right);
}

FPbEquipmentData::FPbEquipmentData()
{
    Reset();        
}

FPbEquipmentData::FPbEquipmentData(const idlepb::EquipmentData& Right)
{
    this->FromPb(Right);
}

void FPbEquipmentData::FromPb(const idlepb::EquipmentData& Right)
{
    slot_index = Right.slot_index();
    combat_power = Right.combat_power();
    maker_name = UTF8_TO_TCHAR(Right.maker_name().c_str());
    maker_roleid = Right.maker_roleid();
    perks.Empty();
    for (const auto& Elem : Right.perks())
    {
        perks.Emplace(Elem);
    }
    skill_equipment_data = Right.skill_equipment_data();
    collection_data = Right.collection_data();
    reinforce_level = Right.reinforce_level();
    refine_level = Right.refine_level();
    qiwen_a_level = Right.qiwen_a_level();
    qiwen_b_level = Right.qiwen_b_level();
    qiwen_c_level = Right.qiwen_c_level();
    qiwen_extra_effect_num = Right.qiwen_extra_effect_num();
    qiwen_moneycast = Right.qiwen_moneycast();
    qiwen_current_exp_a = Right.qiwen_current_exp_a();
    qiwen_current_exp_b = Right.qiwen_current_exp_b();
    qiwen_current_exp_c = Right.qiwen_current_exp_c();
    qiwen_total_exp_a = Right.qiwen_total_exp_a();
    qiwen_total_exp_b = Right.qiwen_total_exp_b();
    qiwen_total_exp_c = Right.qiwen_total_exp_c();
    base_stats = Right.base_stats();
    reinforce_stats = Right.reinforce_stats();
    qiwen_stats = Right.qiwen_stats();
    refine_stats = Right.refine_stats();
    perk_stats = Right.perk_stats();
    qiwen_resonance_stats = Right.qiwen_resonance_stats();
}

void FPbEquipmentData::ToPb(idlepb::EquipmentData* Out) const
{
    Out->set_slot_index(slot_index);
    Out->set_combat_power(combat_power);
    Out->set_maker_name(TCHAR_TO_UTF8(*maker_name));
    Out->set_maker_roleid(maker_roleid);
    for (const auto& Elem : perks)
    {
        Elem.ToPb(Out->add_perks());    
    }
    skill_equipment_data.ToPb(Out->mutable_skill_equipment_data());
    collection_data.ToPb(Out->mutable_collection_data());
    Out->set_reinforce_level(reinforce_level);
    Out->set_refine_level(refine_level);
    Out->set_qiwen_a_level(qiwen_a_level);
    Out->set_qiwen_b_level(qiwen_b_level);
    Out->set_qiwen_c_level(qiwen_c_level);
    Out->set_qiwen_extra_effect_num(qiwen_extra_effect_num);
    Out->set_qiwen_moneycast(qiwen_moneycast);
    Out->set_qiwen_current_exp_a(qiwen_current_exp_a);
    Out->set_qiwen_current_exp_b(qiwen_current_exp_b);
    Out->set_qiwen_current_exp_c(qiwen_current_exp_c);
    Out->set_qiwen_total_exp_a(qiwen_total_exp_a);
    Out->set_qiwen_total_exp_b(qiwen_total_exp_b);
    Out->set_qiwen_total_exp_c(qiwen_total_exp_c);
    base_stats.ToPb(Out->mutable_base_stats());
    reinforce_stats.ToPb(Out->mutable_reinforce_stats());
    qiwen_stats.ToPb(Out->mutable_qiwen_stats());
    refine_stats.ToPb(Out->mutable_refine_stats());
    perk_stats.ToPb(Out->mutable_perk_stats());
    qiwen_resonance_stats.ToPb(Out->mutable_qiwen_resonance_stats());    
}

void FPbEquipmentData::Reset()
{
    slot_index = int32();
    combat_power = int64();
    maker_name = FString();
    maker_roleid = int64();
    perks = TArray<FPbEquipPerkEntry>();
    skill_equipment_data = FPbSkillEquipmentData();
    collection_data = FPbCollectionEntry();
    reinforce_level = int32();
    refine_level = int32();
    qiwen_a_level = int32();
    qiwen_b_level = int32();
    qiwen_c_level = int32();
    qiwen_extra_effect_num = int32();
    qiwen_moneycast = int32();
    qiwen_current_exp_a = int32();
    qiwen_current_exp_b = int32();
    qiwen_current_exp_c = int32();
    qiwen_total_exp_a = int32();
    qiwen_total_exp_b = int32();
    qiwen_total_exp_c = int32();
    base_stats = FPbGameStatsData();
    reinforce_stats = FPbGameStatsData();
    qiwen_stats = FPbGameStatsData();
    refine_stats = FPbGameStatsData();
    perk_stats = FPbGameStatsData();
    qiwen_resonance_stats = FPbGameStatsData();    
}

void FPbEquipmentData::operator=(const idlepb::EquipmentData& Right)
{
    this->FromPb(Right);
}

bool FPbEquipmentData::operator==(const FPbEquipmentData& Right) const
{
    if (this->slot_index != Right.slot_index)
        return false;
    if (this->combat_power != Right.combat_power)
        return false;
    if (this->maker_name != Right.maker_name)
        return false;
    if (this->maker_roleid != Right.maker_roleid)
        return false;
    if (this->perks != Right.perks)
        return false;
    if (this->skill_equipment_data != Right.skill_equipment_data)
        return false;
    if (this->collection_data != Right.collection_data)
        return false;
    if (this->reinforce_level != Right.reinforce_level)
        return false;
    if (this->refine_level != Right.refine_level)
        return false;
    if (this->qiwen_a_level != Right.qiwen_a_level)
        return false;
    if (this->qiwen_b_level != Right.qiwen_b_level)
        return false;
    if (this->qiwen_c_level != Right.qiwen_c_level)
        return false;
    if (this->qiwen_extra_effect_num != Right.qiwen_extra_effect_num)
        return false;
    if (this->qiwen_moneycast != Right.qiwen_moneycast)
        return false;
    if (this->qiwen_current_exp_a != Right.qiwen_current_exp_a)
        return false;
    if (this->qiwen_current_exp_b != Right.qiwen_current_exp_b)
        return false;
    if (this->qiwen_current_exp_c != Right.qiwen_current_exp_c)
        return false;
    if (this->qiwen_total_exp_a != Right.qiwen_total_exp_a)
        return false;
    if (this->qiwen_total_exp_b != Right.qiwen_total_exp_b)
        return false;
    if (this->qiwen_total_exp_c != Right.qiwen_total_exp_c)
        return false;
    if (this->base_stats != Right.base_stats)
        return false;
    if (this->reinforce_stats != Right.reinforce_stats)
        return false;
    if (this->qiwen_stats != Right.qiwen_stats)
        return false;
    if (this->refine_stats != Right.refine_stats)
        return false;
    if (this->perk_stats != Right.perk_stats)
        return false;
    if (this->qiwen_resonance_stats != Right.qiwen_resonance_stats)
        return false;
    return true;
}

bool FPbEquipmentData::operator!=(const FPbEquipmentData& Right) const
{
    return !operator==(Right);
}

FPbItemData::FPbItemData()
{
    Reset();        
}

FPbItemData::FPbItemData(const idlepb::ItemData& Right)
{
    this->FromPb(Right);
}

void FPbItemData::FromPb(const idlepb::ItemData& Right)
{
    id = Right.id();
    cfg_id = Right.cfg_id();
    num = Right.num();
    locked = Right.locked();
    equipment_data = Right.equipment_data();
}

void FPbItemData::ToPb(idlepb::ItemData* Out) const
{
    Out->set_id(id);
    Out->set_cfg_id(cfg_id);
    Out->set_num(num);
    Out->set_locked(locked);
    equipment_data.ToPb(Out->mutable_equipment_data());    
}

void FPbItemData::Reset()
{
    id = int64();
    cfg_id = int32();
    num = int32();
    locked = bool();
    equipment_data = FPbEquipmentData();    
}

void FPbItemData::operator=(const idlepb::ItemData& Right)
{
    this->FromPb(Right);
}

bool FPbItemData::operator==(const FPbItemData& Right) const
{
    if (this->id != Right.id)
        return false;
    if (this->cfg_id != Right.cfg_id)
        return false;
    if (this->num != Right.num)
        return false;
    if (this->locked != Right.locked)
        return false;
    if (this->equipment_data != Right.equipment_data)
        return false;
    return true;
}

bool FPbItemData::operator!=(const FPbItemData& Right) const
{
    return !operator==(Right);
}

FPbSimpleItemData::FPbSimpleItemData()
{
    Reset();        
}

FPbSimpleItemData::FPbSimpleItemData(const idlepb::SimpleItemData& Right)
{
    this->FromPb(Right);
}

void FPbSimpleItemData::FromPb(const idlepb::SimpleItemData& Right)
{
    cfg_id = Right.cfg_id();
    num = Right.num();
}

void FPbSimpleItemData::ToPb(idlepb::SimpleItemData* Out) const
{
    Out->set_cfg_id(cfg_id);
    Out->set_num(num);    
}

void FPbSimpleItemData::Reset()
{
    cfg_id = int32();
    num = int32();    
}

void FPbSimpleItemData::operator=(const idlepb::SimpleItemData& Right)
{
    this->FromPb(Right);
}

bool FPbSimpleItemData::operator==(const FPbSimpleItemData& Right) const
{
    if (this->cfg_id != Right.cfg_id)
        return false;
    if (this->num != Right.num)
        return false;
    return true;
}

bool FPbSimpleItemData::operator!=(const FPbSimpleItemData& Right) const
{
    return !operator==(Right);
}

FPbTemporaryPackageItem::FPbTemporaryPackageItem()
{
    Reset();        
}

FPbTemporaryPackageItem::FPbTemporaryPackageItem(const idlepb::TemporaryPackageItem& Right)
{
    this->FromPb(Right);
}

void FPbTemporaryPackageItem::FromPb(const idlepb::TemporaryPackageItem& Right)
{
    id = Right.id();
    cfg_id = Right.cfg_id();
    num = Right.num();
}

void FPbTemporaryPackageItem::ToPb(idlepb::TemporaryPackageItem* Out) const
{
    Out->set_id(id);
    Out->set_cfg_id(cfg_id);
    Out->set_num(num);    
}

void FPbTemporaryPackageItem::Reset()
{
    id = int64();
    cfg_id = int32();
    num = int32();    
}

void FPbTemporaryPackageItem::operator=(const idlepb::TemporaryPackageItem& Right)
{
    this->FromPb(Right);
}

bool FPbTemporaryPackageItem::operator==(const FPbTemporaryPackageItem& Right) const
{
    if (this->id != Right.id)
        return false;
    if (this->cfg_id != Right.cfg_id)
        return false;
    if (this->num != Right.num)
        return false;
    return true;
}

bool FPbTemporaryPackageItem::operator!=(const FPbTemporaryPackageItem& Right) const
{
    return !operator==(Right);
}

FPbArenaExplorationStatisticalItem::FPbArenaExplorationStatisticalItem()
{
    Reset();        
}

FPbArenaExplorationStatisticalItem::FPbArenaExplorationStatisticalItem(const idlepb::ArenaExplorationStatisticalItem& Right)
{
    this->FromPb(Right);
}

void FPbArenaExplorationStatisticalItem::FromPb(const idlepb::ArenaExplorationStatisticalItem& Right)
{
    time = Right.time();
    mapname = UTF8_TO_TCHAR(Right.mapname().c_str());
    killnum = Right.killnum();
    deathnum = Right.deathnum();
    itemnum = Right.itemnum();
    moneynum = Right.moneynum();
}

void FPbArenaExplorationStatisticalItem::ToPb(idlepb::ArenaExplorationStatisticalItem* Out) const
{
    Out->set_time(time);
    Out->set_mapname(TCHAR_TO_UTF8(*mapname));
    Out->set_killnum(killnum);
    Out->set_deathnum(deathnum);
    Out->set_itemnum(itemnum);
    Out->set_moneynum(moneynum);    
}

void FPbArenaExplorationStatisticalItem::Reset()
{
    time = int64();
    mapname = FString();
    killnum = int32();
    deathnum = int32();
    itemnum = int64();
    moneynum = int64();    
}

void FPbArenaExplorationStatisticalItem::operator=(const idlepb::ArenaExplorationStatisticalItem& Right)
{
    this->FromPb(Right);
}

bool FPbArenaExplorationStatisticalItem::operator==(const FPbArenaExplorationStatisticalItem& Right) const
{
    if (this->time != Right.time)
        return false;
    if (this->mapname != Right.mapname)
        return false;
    if (this->killnum != Right.killnum)
        return false;
    if (this->deathnum != Right.deathnum)
        return false;
    if (this->itemnum != Right.itemnum)
        return false;
    if (this->moneynum != Right.moneynum)
        return false;
    return true;
}

bool FPbArenaExplorationStatisticalItem::operator!=(const FPbArenaExplorationStatisticalItem& Right) const
{
    return !operator==(Right);
}

FPbShopItemBase::FPbShopItemBase()
{
    Reset();        
}

FPbShopItemBase::FPbShopItemBase(const idlepb::ShopItemBase& Right)
{
    this->FromPb(Right);
}

void FPbShopItemBase::FromPb(const idlepb::ShopItemBase& Right)
{
    index = Right.index();
    item_id = Right.item_id();
    num = Right.num();
    price = Right.price();
    count = Right.count();
    bought_count = Right.bought_count();
    cfg_id = Right.cfg_id();
    must_buy = Right.must_buy();
    discount = Right.discount();
}

void FPbShopItemBase::ToPb(idlepb::ShopItemBase* Out) const
{
    Out->set_index(index);
    Out->set_item_id(item_id);
    Out->set_num(num);
    Out->set_price(price);
    Out->set_count(count);
    Out->set_bought_count(bought_count);
    Out->set_cfg_id(cfg_id);
    Out->set_must_buy(must_buy);
    Out->set_discount(discount);    
}

void FPbShopItemBase::Reset()
{
    index = int32();
    item_id = int32();
    num = int32();
    price = int32();
    count = int32();
    bought_count = int32();
    cfg_id = int32();
    must_buy = bool();
    discount = float();    
}

void FPbShopItemBase::operator=(const idlepb::ShopItemBase& Right)
{
    this->FromPb(Right);
}

bool FPbShopItemBase::operator==(const FPbShopItemBase& Right) const
{
    if (this->index != Right.index)
        return false;
    if (this->item_id != Right.item_id)
        return false;
    if (this->num != Right.num)
        return false;
    if (this->price != Right.price)
        return false;
    if (this->count != Right.count)
        return false;
    if (this->bought_count != Right.bought_count)
        return false;
    if (this->cfg_id != Right.cfg_id)
        return false;
    if (this->must_buy != Right.must_buy)
        return false;
    if (this->discount != Right.discount)
        return false;
    return true;
}

bool FPbShopItemBase::operator!=(const FPbShopItemBase& Right) const
{
    return !operator==(Right);
}

FPbShopItem::FPbShopItem()
{
    Reset();        
}

FPbShopItem::FPbShopItem(const idlepb::ShopItem& Right)
{
    this->FromPb(Right);
}

void FPbShopItem::FromPb(const idlepb::ShopItem& Right)
{
    index = Right.index();
    cfg_id = Right.cfg_id();
    num = Right.num();
    money = Right.money();
    is_sold_out = Right.is_sold_out();
    item_data = Right.item_data();
}

void FPbShopItem::ToPb(idlepb::ShopItem* Out) const
{
    Out->set_index(index);
    Out->set_cfg_id(cfg_id);
    Out->set_num(num);
    Out->set_money(money);
    Out->set_is_sold_out(is_sold_out);
    item_data.ToPb(Out->mutable_item_data());    
}

void FPbShopItem::Reset()
{
    index = int32();
    cfg_id = int32();
    num = int32();
    money = int32();
    is_sold_out = bool();
    item_data = FPbItemData();    
}

void FPbShopItem::operator=(const idlepb::ShopItem& Right)
{
    this->FromPb(Right);
}

bool FPbShopItem::operator==(const FPbShopItem& Right) const
{
    if (this->index != Right.index)
        return false;
    if (this->cfg_id != Right.cfg_id)
        return false;
    if (this->num != Right.num)
        return false;
    if (this->money != Right.money)
        return false;
    if (this->is_sold_out != Right.is_sold_out)
        return false;
    if (this->item_data != Right.item_data)
        return false;
    return true;
}

bool FPbShopItem::operator!=(const FPbShopItem& Right) const
{
    return !operator==(Right);
}

FPbDeluxeShopItem::FPbDeluxeShopItem()
{
    Reset();        
}

FPbDeluxeShopItem::FPbDeluxeShopItem(const idlepb::DeluxeShopItem& Right)
{
    this->FromPb(Right);
}

void FPbDeluxeShopItem::FromPb(const idlepb::DeluxeShopItem& Right)
{
    index = Right.index();
    cfg_id = Right.cfg_id();
    num = Right.num();
    sellcount = Right.sellcount();
    discount = Right.discount();
    money = Right.money();
    is_sold_out = Right.is_sold_out();
    item_data = Right.item_data();
    must_buy = Right.must_buy();
}

void FPbDeluxeShopItem::ToPb(idlepb::DeluxeShopItem* Out) const
{
    Out->set_index(index);
    Out->set_cfg_id(cfg_id);
    Out->set_num(num);
    Out->set_sellcount(sellcount);
    Out->set_discount(discount);
    Out->set_money(money);
    Out->set_is_sold_out(is_sold_out);
    item_data.ToPb(Out->mutable_item_data());
    Out->set_must_buy(must_buy);    
}

void FPbDeluxeShopItem::Reset()
{
    index = int32();
    cfg_id = int32();
    num = int32();
    sellcount = int32();
    discount = int32();
    money = int32();
    is_sold_out = bool();
    item_data = FPbItemData();
    must_buy = bool();    
}

void FPbDeluxeShopItem::operator=(const idlepb::DeluxeShopItem& Right)
{
    this->FromPb(Right);
}

bool FPbDeluxeShopItem::operator==(const FPbDeluxeShopItem& Right) const
{
    if (this->index != Right.index)
        return false;
    if (this->cfg_id != Right.cfg_id)
        return false;
    if (this->num != Right.num)
        return false;
    if (this->sellcount != Right.sellcount)
        return false;
    if (this->discount != Right.discount)
        return false;
    if (this->money != Right.money)
        return false;
    if (this->is_sold_out != Right.is_sold_out)
        return false;
    if (this->item_data != Right.item_data)
        return false;
    if (this->must_buy != Right.must_buy)
        return false;
    return true;
}

bool FPbDeluxeShopItem::operator!=(const FPbDeluxeShopItem& Right) const
{
    return !operator==(Right);
}

FPbRoleVipShopData::FPbRoleVipShopData()
{
    Reset();        
}

FPbRoleVipShopData::FPbRoleVipShopData(const idlepb::RoleVipShopData& Right)
{
    this->FromPb(Right);
}

void FPbRoleVipShopData::FromPb(const idlepb::RoleVipShopData& Right)
{
    shop_items.Empty();
    for (const auto& Elem : Right.shop_items())
    {
        shop_items.Emplace(Elem);
    }
    last_day_refresh_time = Right.last_day_refresh_time();
    last_week_refresh_time = Right.last_week_refresh_time();
}

void FPbRoleVipShopData::ToPb(idlepb::RoleVipShopData* Out) const
{
    for (const auto& Elem : shop_items)
    {
        Elem.ToPb(Out->add_shop_items());    
    }
    Out->set_last_day_refresh_time(last_day_refresh_time);
    Out->set_last_week_refresh_time(last_week_refresh_time);    
}

void FPbRoleVipShopData::Reset()
{
    shop_items = TArray<FPbShopItemBase>();
    last_day_refresh_time = int64();
    last_week_refresh_time = int64();    
}

void FPbRoleVipShopData::operator=(const idlepb::RoleVipShopData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleVipShopData::operator==(const FPbRoleVipShopData& Right) const
{
    if (this->shop_items != Right.shop_items)
        return false;
    if (this->last_day_refresh_time != Right.last_day_refresh_time)
        return false;
    if (this->last_week_refresh_time != Right.last_week_refresh_time)
        return false;
    return true;
}

bool FPbRoleVipShopData::operator!=(const FPbRoleVipShopData& Right) const
{
    return !operator==(Right);
}

FPbCharacterModelConfig::FPbCharacterModelConfig()
{
    Reset();        
}

FPbCharacterModelConfig::FPbCharacterModelConfig(const idlepb::CharacterModelConfig& Right)
{
    this->FromPb(Right);
}

void FPbCharacterModelConfig::FromPb(const idlepb::CharacterModelConfig& Right)
{
    skeleton_type = Right.skeleton_type();
    model_slots.Empty();
    for (const auto& Elem : Right.model_slots())
    {
        model_slots.Emplace(Elem);
    }
}

void FPbCharacterModelConfig::ToPb(idlepb::CharacterModelConfig* Out) const
{
    Out->set_skeleton_type(skeleton_type);
    for (const auto& Elem : model_slots)
    {
        Out->add_model_slots(Elem);    
    }    
}

void FPbCharacterModelConfig::Reset()
{
    skeleton_type = int32();
    model_slots = TArray<int32>();    
}

void FPbCharacterModelConfig::operator=(const idlepb::CharacterModelConfig& Right)
{
    this->FromPb(Right);
}

bool FPbCharacterModelConfig::operator==(const FPbCharacterModelConfig& Right) const
{
    if (this->skeleton_type != Right.skeleton_type)
        return false;
    if (this->model_slots != Right.model_slots)
        return false;
    return true;
}

bool FPbCharacterModelConfig::operator!=(const FPbCharacterModelConfig& Right) const
{
    return !operator==(Right);
}

FPbRoleAppearanceShopData::FPbRoleAppearanceShopData()
{
    Reset();        
}

FPbRoleAppearanceShopData::FPbRoleAppearanceShopData(const idlepb::RoleAppearanceShopData& Right)
{
    this->FromPb(Right);
}

void FPbRoleAppearanceShopData::FromPb(const idlepb::RoleAppearanceShopData& Right)
{
    goods1.Empty();
    for (const auto& Elem : Right.goods1())
    {
        goods1.Emplace(Elem);
    }
    last_auto_refresh_time = Right.last_auto_refresh_time();
}

void FPbRoleAppearanceShopData::ToPb(idlepb::RoleAppearanceShopData* Out) const
{
    for (const auto& Elem : goods1)
    {
        Elem.ToPb(Out->add_goods1());    
    }
    Out->set_last_auto_refresh_time(last_auto_refresh_time);    
}

void FPbRoleAppearanceShopData::Reset()
{
    goods1 = TArray<FPbShopItemBase>();
    last_auto_refresh_time = int64();    
}

void FPbRoleAppearanceShopData::operator=(const idlepb::RoleAppearanceShopData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleAppearanceShopData::operator==(const FPbRoleAppearanceShopData& Right) const
{
    if (this->goods1 != Right.goods1)
        return false;
    if (this->last_auto_refresh_time != Right.last_auto_refresh_time)
        return false;
    return true;
}

bool FPbRoleAppearanceShopData::operator!=(const FPbRoleAppearanceShopData& Right) const
{
    return !operator==(Right);
}

FPbAppearanceCollection::FPbAppearanceCollection()
{
    Reset();        
}

FPbAppearanceCollection::FPbAppearanceCollection(const idlepb::AppearanceCollection& Right)
{
    this->FromPb(Right);
}

void FPbAppearanceCollection::FromPb(const idlepb::AppearanceCollection& Right)
{
    group_id = Right.group_id();
    duration = Right.duration();
    begin_date = Right.begin_date();
}

void FPbAppearanceCollection::ToPb(idlepb::AppearanceCollection* Out) const
{
    Out->set_group_id(group_id);
    Out->set_duration(duration);
    Out->set_begin_date(begin_date);    
}

void FPbAppearanceCollection::Reset()
{
    group_id = int32();
    duration = int64();
    begin_date = int64();    
}

void FPbAppearanceCollection::operator=(const idlepb::AppearanceCollection& Right)
{
    this->FromPb(Right);
}

bool FPbAppearanceCollection::operator==(const FPbAppearanceCollection& Right) const
{
    if (this->group_id != Right.group_id)
        return false;
    if (this->duration != Right.duration)
        return false;
    if (this->begin_date != Right.begin_date)
        return false;
    return true;
}

bool FPbAppearanceCollection::operator!=(const FPbAppearanceCollection& Right) const
{
    return !operator==(Right);
}

FPbRoleAppearanceData::FPbRoleAppearanceData()
{
    Reset();        
}

FPbRoleAppearanceData::FPbRoleAppearanceData(const idlepb::RoleAppearanceData& Right)
{
    this->FromPb(Right);
}

void FPbRoleAppearanceData::FromPb(const idlepb::RoleAppearanceData& Right)
{
    last_change_skeleton_time = Right.last_change_skeleton_time();
    collection.Empty();
    for (const auto& Elem : Right.collection())
    {
        collection.Emplace(Elem);
    }
    current_model = Right.current_model();
    shop_data = Right.shop_data();
}

void FPbRoleAppearanceData::ToPb(idlepb::RoleAppearanceData* Out) const
{
    Out->set_last_change_skeleton_time(last_change_skeleton_time);
    for (const auto& Elem : collection)
    {
        Elem.ToPb(Out->add_collection());    
    }
    current_model.ToPb(Out->mutable_current_model());
    shop_data.ToPb(Out->mutable_shop_data());    
}

void FPbRoleAppearanceData::Reset()
{
    last_change_skeleton_time = int64();
    collection = TArray<FPbAppearanceCollection>();
    current_model = FPbCharacterModelConfig();
    shop_data = FPbRoleAppearanceShopData();    
}

void FPbRoleAppearanceData::operator=(const idlepb::RoleAppearanceData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleAppearanceData::operator==(const FPbRoleAppearanceData& Right) const
{
    if (this->last_change_skeleton_time != Right.last_change_skeleton_time)
        return false;
    if (this->collection != Right.collection)
        return false;
    if (this->current_model != Right.current_model)
        return false;
    if (this->shop_data != Right.shop_data)
        return false;
    return true;
}

bool FPbRoleAppearanceData::operator!=(const FPbRoleAppearanceData& Right) const
{
    return !operator==(Right);
}

FPbAlchemyPackageItem::FPbAlchemyPackageItem()
{
    Reset();        
}

FPbAlchemyPackageItem::FPbAlchemyPackageItem(const idlepb::AlchemyPackageItem& Right)
{
    this->FromPb(Right);
}

void FPbAlchemyPackageItem::FromPb(const idlepb::AlchemyPackageItem& Right)
{
    cfg_id = Right.cfg_id();
    num = Right.num();
}

void FPbAlchemyPackageItem::ToPb(idlepb::AlchemyPackageItem* Out) const
{
    Out->set_cfg_id(cfg_id);
    Out->set_num(num);    
}

void FPbAlchemyPackageItem::Reset()
{
    cfg_id = int32();
    num = int32();    
}

void FPbAlchemyPackageItem::operator=(const idlepb::AlchemyPackageItem& Right)
{
    this->FromPb(Right);
}

bool FPbAlchemyPackageItem::operator==(const FPbAlchemyPackageItem& Right) const
{
    if (this->cfg_id != Right.cfg_id)
        return false;
    if (this->num != Right.num)
        return false;
    return true;
}

bool FPbAlchemyPackageItem::operator!=(const FPbAlchemyPackageItem& Right) const
{
    return !operator==(Right);
}

FPbAlchemyMakeData::FPbAlchemyMakeData()
{
    Reset();        
}

FPbAlchemyMakeData::FPbAlchemyMakeData(const idlepb::AlchemyMakeData& Right)
{
    this->FromPb(Right);
}

void FPbAlchemyMakeData::FromPb(const idlepb::AlchemyMakeData& Right)
{
    recipe_id = Right.recipe_id();
    recipe_degree = Right.recipe_degree();
    material_id = Right.material_id();
    material_quality = static_cast<EPbItemQuality>(Right.material_quality());
    target_num = Right.target_num();
    cur_num = Right.cur_num();
    next_done_time = Right.next_done_time();
    items.Empty();
    for (const auto& Elem : Right.items())
    {
        items.Emplace(Elem);
    }
    last_produce_item_cfg_id = Right.last_produce_item_cfg_id();
    last_produce_item_num = Right.last_produce_item_num();
    total_start_time = Right.total_start_time();
    total_done_time = Right.total_done_time();
    add_exp = Right.add_exp();
    cur_successed_num = Right.cur_successed_num();
    cur_failed_num = Right.cur_failed_num();
}

void FPbAlchemyMakeData::ToPb(idlepb::AlchemyMakeData* Out) const
{
    Out->set_recipe_id(recipe_id);
    Out->set_recipe_degree(recipe_degree);
    Out->set_material_id(material_id);
    Out->set_material_quality(static_cast<idlepb::ItemQuality>(material_quality));
    Out->set_target_num(target_num);
    Out->set_cur_num(cur_num);
    Out->set_next_done_time(next_done_time);
    for (const auto& Elem : items)
    {
        Elem.ToPb(Out->add_items());    
    }
    Out->set_last_produce_item_cfg_id(last_produce_item_cfg_id);
    Out->set_last_produce_item_num(last_produce_item_num);
    Out->set_total_start_time(total_start_time);
    Out->set_total_done_time(total_done_time);
    Out->set_add_exp(add_exp);
    Out->set_cur_successed_num(cur_successed_num);
    Out->set_cur_failed_num(cur_failed_num);    
}

void FPbAlchemyMakeData::Reset()
{
    recipe_id = int32();
    recipe_degree = int32();
    material_id = int32();
    material_quality = EPbItemQuality();
    target_num = int32();
    cur_num = int32();
    next_done_time = int64();
    items = TArray<FPbAlchemyPackageItem>();
    last_produce_item_cfg_id = int32();
    last_produce_item_num = int32();
    total_start_time = int64();
    total_done_time = int64();
    add_exp = int32();
    cur_successed_num = int32();
    cur_failed_num = int32();    
}

void FPbAlchemyMakeData::operator=(const idlepb::AlchemyMakeData& Right)
{
    this->FromPb(Right);
}

bool FPbAlchemyMakeData::operator==(const FPbAlchemyMakeData& Right) const
{
    if (this->recipe_id != Right.recipe_id)
        return false;
    if (this->recipe_degree != Right.recipe_degree)
        return false;
    if (this->material_id != Right.material_id)
        return false;
    if (this->material_quality != Right.material_quality)
        return false;
    if (this->target_num != Right.target_num)
        return false;
    if (this->cur_num != Right.cur_num)
        return false;
    if (this->next_done_time != Right.next_done_time)
        return false;
    if (this->items != Right.items)
        return false;
    if (this->last_produce_item_cfg_id != Right.last_produce_item_cfg_id)
        return false;
    if (this->last_produce_item_num != Right.last_produce_item_num)
        return false;
    if (this->total_start_time != Right.total_start_time)
        return false;
    if (this->total_done_time != Right.total_done_time)
        return false;
    if (this->add_exp != Right.add_exp)
        return false;
    if (this->cur_successed_num != Right.cur_successed_num)
        return false;
    if (this->cur_failed_num != Right.cur_failed_num)
        return false;
    return true;
}

bool FPbAlchemyMakeData::operator!=(const FPbAlchemyMakeData& Right) const
{
    return !operator==(Right);
}

FPbAlchemyRecipeData::FPbAlchemyRecipeData()
{
    Reset();        
}

FPbAlchemyRecipeData::FPbAlchemyRecipeData(const idlepb::AlchemyRecipeData& Right)
{
    this->FromPb(Right);
}

void FPbAlchemyRecipeData::FromPb(const idlepb::AlchemyRecipeData& Right)
{
    recipe_id = Right.recipe_id();
    big_chance = Right.big_chance();
    small_chance = Right.small_chance();
}

void FPbAlchemyRecipeData::ToPb(idlepb::AlchemyRecipeData* Out) const
{
    Out->set_recipe_id(recipe_id);
    Out->set_big_chance(big_chance);
    Out->set_small_chance(small_chance);    
}

void FPbAlchemyRecipeData::Reset()
{
    recipe_id = int32();
    big_chance = int32();
    small_chance = int32();    
}

void FPbAlchemyRecipeData::operator=(const idlepb::AlchemyRecipeData& Right)
{
    this->FromPb(Right);
}

bool FPbAlchemyRecipeData::operator==(const FPbAlchemyRecipeData& Right) const
{
    if (this->recipe_id != Right.recipe_id)
        return false;
    if (this->big_chance != Right.big_chance)
        return false;
    if (this->small_chance != Right.small_chance)
        return false;
    return true;
}

bool FPbAlchemyRecipeData::operator!=(const FPbAlchemyRecipeData& Right) const
{
    return !operator==(Right);
}

FPbRoleAlchemyData::FPbRoleAlchemyData()
{
    Reset();        
}

FPbRoleAlchemyData::FPbRoleAlchemyData(const idlepb::RoleAlchemyData& Right)
{
    this->FromPb(Right);
}

void FPbRoleAlchemyData::FromPb(const idlepb::RoleAlchemyData& Right)
{
    rank = Right.rank();
    exp = Right.exp();
    cur_make_data = Right.cur_make_data();
    total_refine_num = Right.total_refine_num();
    produce_quality_stats.Empty();
    for (const auto& Elem : Right.produce_quality_stats())
    {
        produce_quality_stats.Emplace(Elem);
    }
    recipes.Empty();
    for (const auto& Elem : Right.recipes())
    {
        recipes.Emplace(Elem);
    }
}

void FPbRoleAlchemyData::ToPb(idlepb::RoleAlchemyData* Out) const
{
    Out->set_rank(rank);
    Out->set_exp(exp);
    cur_make_data.ToPb(Out->mutable_cur_make_data());
    Out->set_total_refine_num(total_refine_num);
    for (const auto& Elem : produce_quality_stats)
    {
        Out->add_produce_quality_stats(Elem);    
    }
    for (const auto& Elem : recipes)
    {
        Elem.ToPb(Out->add_recipes());    
    }    
}

void FPbRoleAlchemyData::Reset()
{
    rank = int32();
    exp = int32();
    cur_make_data = FPbAlchemyMakeData();
    total_refine_num = int32();
    produce_quality_stats = TArray<int32>();
    recipes = TArray<FPbAlchemyRecipeData>();    
}

void FPbRoleAlchemyData::operator=(const idlepb::RoleAlchemyData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleAlchemyData::operator==(const FPbRoleAlchemyData& Right) const
{
    if (this->rank != Right.rank)
        return false;
    if (this->exp != Right.exp)
        return false;
    if (this->cur_make_data != Right.cur_make_data)
        return false;
    if (this->total_refine_num != Right.total_refine_num)
        return false;
    if (this->produce_quality_stats != Right.produce_quality_stats)
        return false;
    if (this->recipes != Right.recipes)
        return false;
    return true;
}

bool FPbRoleAlchemyData::operator!=(const FPbRoleAlchemyData& Right) const
{
    return !operator==(Right);
}

FPbForgePackageItem::FPbForgePackageItem()
{
    Reset();        
}

FPbForgePackageItem::FPbForgePackageItem(const idlepb::ForgePackageItem& Right)
{
    this->FromPb(Right);
}

void FPbForgePackageItem::FromPb(const idlepb::ForgePackageItem& Right)
{
    cfg_id = Right.cfg_id();
    num = Right.num();
}

void FPbForgePackageItem::ToPb(idlepb::ForgePackageItem* Out) const
{
    Out->set_cfg_id(cfg_id);
    Out->set_num(num);    
}

void FPbForgePackageItem::Reset()
{
    cfg_id = int32();
    num = int32();    
}

void FPbForgePackageItem::operator=(const idlepb::ForgePackageItem& Right)
{
    this->FromPb(Right);
}

bool FPbForgePackageItem::operator==(const FPbForgePackageItem& Right) const
{
    if (this->cfg_id != Right.cfg_id)
        return false;
    if (this->num != Right.num)
        return false;
    return true;
}

bool FPbForgePackageItem::operator!=(const FPbForgePackageItem& Right) const
{
    return !operator==(Right);
}

FPbForgeMakeData::FPbForgeMakeData()
{
    Reset();        
}

FPbForgeMakeData::FPbForgeMakeData(const idlepb::ForgeMakeData& Right)
{
    this->FromPb(Right);
}

void FPbForgeMakeData::FromPb(const idlepb::ForgeMakeData& Right)
{
    recipe_id = Right.recipe_id();
    recipe_degree = Right.recipe_degree();
    material_id = Right.material_id();
    material_quality = static_cast<EPbItemQuality>(Right.material_quality());
    target_num = Right.target_num();
    cur_num = Right.cur_num();
    next_done_time = Right.next_done_time();
    items.Empty();
    for (const auto& Elem : Right.items())
    {
        items.Emplace(Elem);
    }
    last_produce_item_cfg_id = Right.last_produce_item_cfg_id();
    last_produce_item_num = Right.last_produce_item_num();
    total_start_time = Right.total_start_time();
    total_done_time = Right.total_done_time();
    add_exp = Right.add_exp();
    cur_successed_num = Right.cur_successed_num();
    cur_failed_num = Right.cur_failed_num();
    ext_material_id = Right.ext_material_id();
    auto_sell_poor = Right.auto_sell_poor();
    auto_sell_middle = Right.auto_sell_middle();
}

void FPbForgeMakeData::ToPb(idlepb::ForgeMakeData* Out) const
{
    Out->set_recipe_id(recipe_id);
    Out->set_recipe_degree(recipe_degree);
    Out->set_material_id(material_id);
    Out->set_material_quality(static_cast<idlepb::ItemQuality>(material_quality));
    Out->set_target_num(target_num);
    Out->set_cur_num(cur_num);
    Out->set_next_done_time(next_done_time);
    for (const auto& Elem : items)
    {
        Elem.ToPb(Out->add_items());    
    }
    Out->set_last_produce_item_cfg_id(last_produce_item_cfg_id);
    Out->set_last_produce_item_num(last_produce_item_num);
    Out->set_total_start_time(total_start_time);
    Out->set_total_done_time(total_done_time);
    Out->set_add_exp(add_exp);
    Out->set_cur_successed_num(cur_successed_num);
    Out->set_cur_failed_num(cur_failed_num);
    Out->set_ext_material_id(ext_material_id);
    Out->set_auto_sell_poor(auto_sell_poor);
    Out->set_auto_sell_middle(auto_sell_middle);    
}

void FPbForgeMakeData::Reset()
{
    recipe_id = int32();
    recipe_degree = int32();
    material_id = int32();
    material_quality = EPbItemQuality();
    target_num = int32();
    cur_num = int32();
    next_done_time = int64();
    items = TArray<FPbForgePackageItem>();
    last_produce_item_cfg_id = int32();
    last_produce_item_num = int32();
    total_start_time = int64();
    total_done_time = int64();
    add_exp = int32();
    cur_successed_num = int32();
    cur_failed_num = int32();
    ext_material_id = int32();
    auto_sell_poor = bool();
    auto_sell_middle = bool();    
}

void FPbForgeMakeData::operator=(const idlepb::ForgeMakeData& Right)
{
    this->FromPb(Right);
}

bool FPbForgeMakeData::operator==(const FPbForgeMakeData& Right) const
{
    if (this->recipe_id != Right.recipe_id)
        return false;
    if (this->recipe_degree != Right.recipe_degree)
        return false;
    if (this->material_id != Right.material_id)
        return false;
    if (this->material_quality != Right.material_quality)
        return false;
    if (this->target_num != Right.target_num)
        return false;
    if (this->cur_num != Right.cur_num)
        return false;
    if (this->next_done_time != Right.next_done_time)
        return false;
    if (this->items != Right.items)
        return false;
    if (this->last_produce_item_cfg_id != Right.last_produce_item_cfg_id)
        return false;
    if (this->last_produce_item_num != Right.last_produce_item_num)
        return false;
    if (this->total_start_time != Right.total_start_time)
        return false;
    if (this->total_done_time != Right.total_done_time)
        return false;
    if (this->add_exp != Right.add_exp)
        return false;
    if (this->cur_successed_num != Right.cur_successed_num)
        return false;
    if (this->cur_failed_num != Right.cur_failed_num)
        return false;
    if (this->ext_material_id != Right.ext_material_id)
        return false;
    if (this->auto_sell_poor != Right.auto_sell_poor)
        return false;
    if (this->auto_sell_middle != Right.auto_sell_middle)
        return false;
    return true;
}

bool FPbForgeMakeData::operator!=(const FPbForgeMakeData& Right) const
{
    return !operator==(Right);
}

FPbForgeRecipeData::FPbForgeRecipeData()
{
    Reset();        
}

FPbForgeRecipeData::FPbForgeRecipeData(const idlepb::ForgeRecipeData& Right)
{
    this->FromPb(Right);
}

void FPbForgeRecipeData::FromPb(const idlepb::ForgeRecipeData& Right)
{
    recipe_id = Right.recipe_id();
    big_chance = Right.big_chance();
    small_chance = Right.small_chance();
}

void FPbForgeRecipeData::ToPb(idlepb::ForgeRecipeData* Out) const
{
    Out->set_recipe_id(recipe_id);
    Out->set_big_chance(big_chance);
    Out->set_small_chance(small_chance);    
}

void FPbForgeRecipeData::Reset()
{
    recipe_id = int32();
    big_chance = int32();
    small_chance = int32();    
}

void FPbForgeRecipeData::operator=(const idlepb::ForgeRecipeData& Right)
{
    this->FromPb(Right);
}

bool FPbForgeRecipeData::operator==(const FPbForgeRecipeData& Right) const
{
    if (this->recipe_id != Right.recipe_id)
        return false;
    if (this->big_chance != Right.big_chance)
        return false;
    if (this->small_chance != Right.small_chance)
        return false;
    return true;
}

bool FPbForgeRecipeData::operator!=(const FPbForgeRecipeData& Right) const
{
    return !operator==(Right);
}

FPbLostEquipmentData::FPbLostEquipmentData()
{
    Reset();        
}

FPbLostEquipmentData::FPbLostEquipmentData(const idlepb::LostEquipmentData& Right)
{
    this->FromPb(Right);
}

void FPbLostEquipmentData::FromPb(const idlepb::LostEquipmentData& Right)
{
    uid = Right.uid();
    tag = Right.tag();
    lost_date = Right.lost_date();
    item_data = Right.item_data();
}

void FPbLostEquipmentData::ToPb(idlepb::LostEquipmentData* Out) const
{
    Out->set_uid(uid);
    Out->set_tag(tag);
    Out->set_lost_date(lost_date);
    item_data.ToPb(Out->mutable_item_data());    
}

void FPbLostEquipmentData::Reset()
{
    uid = int32();
    tag = int32();
    lost_date = int64();
    item_data = FPbItemData();    
}

void FPbLostEquipmentData::operator=(const idlepb::LostEquipmentData& Right)
{
    this->FromPb(Right);
}

bool FPbLostEquipmentData::operator==(const FPbLostEquipmentData& Right) const
{
    if (this->uid != Right.uid)
        return false;
    if (this->tag != Right.tag)
        return false;
    if (this->lost_date != Right.lost_date)
        return false;
    if (this->item_data != Right.item_data)
        return false;
    return true;
}

bool FPbLostEquipmentData::operator!=(const FPbLostEquipmentData& Right) const
{
    return !operator==(Right);
}

FPbRoleForgeData::FPbRoleForgeData()
{
    Reset();        
}

FPbRoleForgeData::FPbRoleForgeData(const idlepb::RoleForgeData& Right)
{
    this->FromPb(Right);
}

void FPbRoleForgeData::FromPb(const idlepb::RoleForgeData& Right)
{
    rank = Right.rank();
    exp = Right.exp();
    cur_make_data = Right.cur_make_data();
    total_refine_num = Right.total_refine_num();
    produce_equip_quality_stats.Empty();
    for (const auto& Elem : Right.produce_equip_quality_stats())
    {
        produce_equip_quality_stats.Emplace(Elem);
    }
    produce_skillequip_quality_stats.Empty();
    for (const auto& Elem : Right.produce_skillequip_quality_stats())
    {
        produce_skillequip_quality_stats.Emplace(Elem);
    }
    recipes.Empty();
    for (const auto& Elem : Right.recipes())
    {
        recipes.Emplace(Elem);
    }
    lost_equipment_data.Empty();
    for (const auto& Elem : Right.lost_equipment_data())
    {
        lost_equipment_data.Emplace(Elem);
    }
    destroy_num = Right.destroy_num();
}

void FPbRoleForgeData::ToPb(idlepb::RoleForgeData* Out) const
{
    Out->set_rank(rank);
    Out->set_exp(exp);
    cur_make_data.ToPb(Out->mutable_cur_make_data());
    Out->set_total_refine_num(total_refine_num);
    for (const auto& Elem : produce_equip_quality_stats)
    {
        Out->add_produce_equip_quality_stats(Elem);    
    }
    for (const auto& Elem : produce_skillequip_quality_stats)
    {
        Out->add_produce_skillequip_quality_stats(Elem);    
    }
    for (const auto& Elem : recipes)
    {
        Elem.ToPb(Out->add_recipes());    
    }
    for (const auto& Elem : lost_equipment_data)
    {
        Elem.ToPb(Out->add_lost_equipment_data());    
    }
    Out->set_destroy_num(destroy_num);    
}

void FPbRoleForgeData::Reset()
{
    rank = int32();
    exp = int32();
    cur_make_data = FPbForgeMakeData();
    total_refine_num = int32();
    produce_equip_quality_stats = TArray<int32>();
    produce_skillequip_quality_stats = TArray<int32>();
    recipes = TArray<FPbForgeRecipeData>();
    lost_equipment_data = TArray<FPbLostEquipmentData>();
    destroy_num = int32();    
}

void FPbRoleForgeData::operator=(const idlepb::RoleForgeData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleForgeData::operator==(const FPbRoleForgeData& Right) const
{
    if (this->rank != Right.rank)
        return false;
    if (this->exp != Right.exp)
        return false;
    if (this->cur_make_data != Right.cur_make_data)
        return false;
    if (this->total_refine_num != Right.total_refine_num)
        return false;
    if (this->produce_equip_quality_stats != Right.produce_equip_quality_stats)
        return false;
    if (this->produce_skillequip_quality_stats != Right.produce_skillequip_quality_stats)
        return false;
    if (this->recipes != Right.recipes)
        return false;
    if (this->lost_equipment_data != Right.lost_equipment_data)
        return false;
    if (this->destroy_num != Right.destroy_num)
        return false;
    return true;
}

bool FPbRoleForgeData::operator!=(const FPbRoleForgeData& Right) const
{
    return !operator==(Right);
}

FPbPillPropertyData::FPbPillPropertyData()
{
    Reset();        
}

FPbPillPropertyData::FPbPillPropertyData(const idlepb::PillPropertyData& Right)
{
    this->FromPb(Right);
}

void FPbPillPropertyData::FromPb(const idlepb::PillPropertyData& Right)
{
    item_id = Right.item_id();
    consumed_num = Right.consumed_num();
}

void FPbPillPropertyData::ToPb(idlepb::PillPropertyData* Out) const
{
    Out->set_item_id(item_id);
    Out->set_consumed_num(consumed_num);    
}

void FPbPillPropertyData::Reset()
{
    item_id = int32();
    consumed_num = int32();    
}

void FPbPillPropertyData::operator=(const idlepb::PillPropertyData& Right)
{
    this->FromPb(Right);
}

bool FPbPillPropertyData::operator==(const FPbPillPropertyData& Right) const
{
    if (this->item_id != Right.item_id)
        return false;
    if (this->consumed_num != Right.consumed_num)
        return false;
    return true;
}

bool FPbPillPropertyData::operator!=(const FPbPillPropertyData& Right) const
{
    return !operator==(Right);
}

FPbGongFaData::FPbGongFaData()
{
    Reset();        
}

FPbGongFaData::FPbGongFaData(const idlepb::GongFaData& Right)
{
    this->FromPb(Right);
}

void FPbGongFaData::FromPb(const idlepb::GongFaData& Right)
{
    cfg_id = Right.cfg_id();
    level = Right.level();
    begin_time = Right.begin_time();
    state = static_cast<EPbGongFaState>(Right.state());
    combat_power = Right.combat_power();
}

void FPbGongFaData::ToPb(idlepb::GongFaData* Out) const
{
    Out->set_cfg_id(cfg_id);
    Out->set_level(level);
    Out->set_begin_time(begin_time);
    Out->set_state(static_cast<idlepb::GongFaState>(state));
    Out->set_combat_power(combat_power);    
}

void FPbGongFaData::Reset()
{
    cfg_id = int32();
    level = int32();
    begin_time = int64();
    state = EPbGongFaState();
    combat_power = float();    
}

void FPbGongFaData::operator=(const idlepb::GongFaData& Right)
{
    this->FromPb(Right);
}

bool FPbGongFaData::operator==(const FPbGongFaData& Right) const
{
    if (this->cfg_id != Right.cfg_id)
        return false;
    if (this->level != Right.level)
        return false;
    if (this->begin_time != Right.begin_time)
        return false;
    if (this->state != Right.state)
        return false;
    if (this->combat_power != Right.combat_power)
        return false;
    return true;
}

bool FPbGongFaData::operator!=(const FPbGongFaData& Right) const
{
    return !operator==(Right);
}

FPbRoleGongFaData::FPbRoleGongFaData()
{
    Reset();        
}

FPbRoleGongFaData::FPbRoleGongFaData(const idlepb::RoleGongFaData& Right)
{
    this->FromPb(Right);
}

void FPbRoleGongFaData::FromPb(const idlepb::RoleGongFaData& Right)
{
    data.Empty();
    for (const auto& Elem : Right.data())
    {
        data.Emplace(Elem);
    }
    active_max_effect.Empty();
    for (const auto& Elem : Right.active_max_effect())
    {
        active_max_effect.Emplace(Elem);
    }
    gongfa_point_use_num = Right.gongfa_point_use_num();
}

void FPbRoleGongFaData::ToPb(idlepb::RoleGongFaData* Out) const
{
    for (const auto& Elem : data)
    {
        Elem.ToPb(Out->add_data());    
    }
    for (const auto& Elem : active_max_effect)
    {
        Out->add_active_max_effect(Elem);    
    }
    Out->set_gongfa_point_use_num(gongfa_point_use_num);    
}

void FPbRoleGongFaData::Reset()
{
    data = TArray<FPbGongFaData>();
    active_max_effect = TArray<int32>();
    gongfa_point_use_num = int32();    
}

void FPbRoleGongFaData::operator=(const idlepb::RoleGongFaData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleGongFaData::operator==(const FPbRoleGongFaData& Right) const
{
    if (this->data != Right.data)
        return false;
    if (this->active_max_effect != Right.active_max_effect)
        return false;
    if (this->gongfa_point_use_num != Right.gongfa_point_use_num)
        return false;
    return true;
}

bool FPbRoleGongFaData::operator!=(const FPbRoleGongFaData& Right) const
{
    return !operator==(Right);
}

FPbCollectionEntrySaveData::FPbCollectionEntrySaveData()
{
    Reset();        
}

FPbCollectionEntrySaveData::FPbCollectionEntrySaveData(const idlepb::CollectionEntrySaveData& Right)
{
    this->FromPb(Right);
}

void FPbCollectionEntrySaveData::FromPb(const idlepb::CollectionEntrySaveData& Right)
{
    id = Right.id();
    level = Right.level();
    star = Right.star();
    is_activated = Right.is_activated();
    piece_num = Right.piece_num();
}

void FPbCollectionEntrySaveData::ToPb(idlepb::CollectionEntrySaveData* Out) const
{
    Out->set_id(id);
    Out->set_level(level);
    Out->set_star(star);
    Out->set_is_activated(is_activated);
    Out->set_piece_num(piece_num);    
}

void FPbCollectionEntrySaveData::Reset()
{
    id = int32();
    level = int32();
    star = int32();
    is_activated = bool();
    piece_num = int32();    
}

void FPbCollectionEntrySaveData::operator=(const idlepb::CollectionEntrySaveData& Right)
{
    this->FromPb(Right);
}

bool FPbCollectionEntrySaveData::operator==(const FPbCollectionEntrySaveData& Right) const
{
    if (this->id != Right.id)
        return false;
    if (this->level != Right.level)
        return false;
    if (this->star != Right.star)
        return false;
    if (this->is_activated != Right.is_activated)
        return false;
    if (this->piece_num != Right.piece_num)
        return false;
    return true;
}

bool FPbCollectionEntrySaveData::operator!=(const FPbCollectionEntrySaveData& Right) const
{
    return !operator==(Right);
}

FPbCommonCollectionPieceData::FPbCommonCollectionPieceData()
{
    Reset();        
}

FPbCommonCollectionPieceData::FPbCommonCollectionPieceData(const idlepb::CommonCollectionPieceData& Right)
{
    this->FromPb(Right);
}

void FPbCommonCollectionPieceData::FromPb(const idlepb::CommonCollectionPieceData& Right)
{
    quality = static_cast<EPbItemQuality>(Right.quality());
    piece_num = Right.piece_num();
}

void FPbCommonCollectionPieceData::ToPb(idlepb::CommonCollectionPieceData* Out) const
{
    Out->set_quality(static_cast<idlepb::ItemQuality>(quality));
    Out->set_piece_num(piece_num);    
}

void FPbCommonCollectionPieceData::Reset()
{
    quality = EPbItemQuality();
    piece_num = int32();    
}

void FPbCommonCollectionPieceData::operator=(const idlepb::CommonCollectionPieceData& Right)
{
    this->FromPb(Right);
}

bool FPbCommonCollectionPieceData::operator==(const FPbCommonCollectionPieceData& Right) const
{
    if (this->quality != Right.quality)
        return false;
    if (this->piece_num != Right.piece_num)
        return false;
    return true;
}

bool FPbCommonCollectionPieceData::operator!=(const FPbCommonCollectionPieceData& Right) const
{
    return !operator==(Right);
}

FPbCollectionZoneActiveAwardData::FPbCollectionZoneActiveAwardData()
{
    Reset();        
}

FPbCollectionZoneActiveAwardData::FPbCollectionZoneActiveAwardData(const idlepb::CollectionZoneActiveAwardData& Right)
{
    this->FromPb(Right);
}

void FPbCollectionZoneActiveAwardData::FromPb(const idlepb::CollectionZoneActiveAwardData& Right)
{
    zone_type = static_cast<EPbCollectionZoneType>(Right.zone_type());
    num = Right.num();
}

void FPbCollectionZoneActiveAwardData::ToPb(idlepb::CollectionZoneActiveAwardData* Out) const
{
    Out->set_zone_type(static_cast<idlepb::CollectionZoneType>(zone_type));
    Out->set_num(num);    
}

void FPbCollectionZoneActiveAwardData::Reset()
{
    zone_type = EPbCollectionZoneType();
    num = int32();    
}

void FPbCollectionZoneActiveAwardData::operator=(const idlepb::CollectionZoneActiveAwardData& Right)
{
    this->FromPb(Right);
}

bool FPbCollectionZoneActiveAwardData::operator==(const FPbCollectionZoneActiveAwardData& Right) const
{
    if (this->zone_type != Right.zone_type)
        return false;
    if (this->num != Right.num)
        return false;
    return true;
}

bool FPbCollectionZoneActiveAwardData::operator!=(const FPbCollectionZoneActiveAwardData& Right) const
{
    return !operator==(Right);
}

FPbRoleCollectionSaveData::FPbRoleCollectionSaveData()
{
    Reset();        
}

FPbRoleCollectionSaveData::FPbRoleCollectionSaveData(const idlepb::RoleCollectionSaveData& Right)
{
    this->FromPb(Right);
}

void FPbRoleCollectionSaveData::FromPb(const idlepb::RoleCollectionSaveData& Right)
{
    all_entries.Empty();
    for (const auto& Elem : Right.all_entries())
    {
        all_entries.Emplace(Elem);
    }
    common_pieces.Empty();
    for (const auto& Elem : Right.common_pieces())
    {
        common_pieces.Emplace(Elem);
    }
    draw_award_done_histories.Empty();
    for (const auto& Elem : Right.draw_award_done_histories())
    {
        draw_award_done_histories.Emplace(Elem);
    }
    zone_active_awards.Empty();
    for (const auto& Elem : Right.zone_active_awards())
    {
        zone_active_awards.Emplace(Elem);
    }
    next_reset_enhance_ticks = Right.next_reset_enhance_ticks();
}

void FPbRoleCollectionSaveData::ToPb(idlepb::RoleCollectionSaveData* Out) const
{
    for (const auto& Elem : all_entries)
    {
        Elem.ToPb(Out->add_all_entries());    
    }
    for (const auto& Elem : common_pieces)
    {
        Elem.ToPb(Out->add_common_pieces());    
    }
    for (const auto& Elem : draw_award_done_histories)
    {
        Out->add_draw_award_done_histories(Elem);    
    }
    for (const auto& Elem : zone_active_awards)
    {
        Elem.ToPb(Out->add_zone_active_awards());    
    }
    Out->set_next_reset_enhance_ticks(next_reset_enhance_ticks);    
}

void FPbRoleCollectionSaveData::Reset()
{
    all_entries = TArray<FPbCollectionEntrySaveData>();
    common_pieces = TArray<FPbCommonCollectionPieceData>();
    draw_award_done_histories = TArray<int32>();
    zone_active_awards = TArray<FPbCollectionZoneActiveAwardData>();
    next_reset_enhance_ticks = int64();    
}

void FPbRoleCollectionSaveData::operator=(const idlepb::RoleCollectionSaveData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleCollectionSaveData::operator==(const FPbRoleCollectionSaveData& Right) const
{
    if (this->all_entries != Right.all_entries)
        return false;
    if (this->common_pieces != Right.common_pieces)
        return false;
    if (this->draw_award_done_histories != Right.draw_award_done_histories)
        return false;
    if (this->zone_active_awards != Right.zone_active_awards)
        return false;
    if (this->next_reset_enhance_ticks != Right.next_reset_enhance_ticks)
        return false;
    return true;
}

bool FPbRoleCollectionSaveData::operator!=(const FPbRoleCollectionSaveData& Right) const
{
    return !operator==(Right);
}

FPbFuZengTuple::FPbFuZengTuple()
{
    Reset();        
}

FPbFuZengTuple::FPbFuZengTuple(const idlepb::FuZengTuple& Right)
{
    this->FromPb(Right);
}

void FPbFuZengTuple::FromPb(const idlepb::FuZengTuple& Right)
{
    cfg_id = Right.cfg_id();
    num.Empty();
    for (const auto& Elem : Right.num())
    {
        num.Emplace(Elem);
    }
}

void FPbFuZengTuple::ToPb(idlepb::FuZengTuple* Out) const
{
    Out->set_cfg_id(cfg_id);
    for (const auto& Elem : num)
    {
        Out->add_num(Elem);    
    }    
}

void FPbFuZengTuple::Reset()
{
    cfg_id = int32();
    num = TArray<int64>();    
}

void FPbFuZengTuple::operator=(const idlepb::FuZengTuple& Right)
{
    this->FromPb(Right);
}

bool FPbFuZengTuple::operator==(const FPbFuZengTuple& Right) const
{
    if (this->cfg_id != Right.cfg_id)
        return false;
    if (this->num != Right.num)
        return false;
    return true;
}

bool FPbFuZengTuple::operator!=(const FPbFuZengTuple& Right) const
{
    return !operator==(Right);
}

FPbFuZengData::FPbFuZengData()
{
    Reset();        
}

FPbFuZengData::FPbFuZengData(const idlepb::FuZengData& Right)
{
    this->FromPb(Right);
}

void FPbFuZengData::FromPb(const idlepb::FuZengData& Right)
{
    type = static_cast<EPbFuZengType>(Right.type());
    received_record.Empty();
    for (const auto& Elem : Right.received_record())
    {
        received_record.Emplace(Elem);
    }
    max_num = Right.max_num();
}

void FPbFuZengData::ToPb(idlepb::FuZengData* Out) const
{
    Out->set_type(static_cast<idlepb::FuZengType>(type));
    for (const auto& Elem : received_record)
    {
        Elem.ToPb(Out->add_received_record());    
    }
    Out->set_max_num(max_num);    
}

void FPbFuZengData::Reset()
{
    type = EPbFuZengType();
    received_record = TArray<FPbFuZengTuple>();
    max_num = int64();    
}

void FPbFuZengData::operator=(const idlepb::FuZengData& Right)
{
    this->FromPb(Right);
}

bool FPbFuZengData::operator==(const FPbFuZengData& Right) const
{
    if (this->type != Right.type)
        return false;
    if (this->received_record != Right.received_record)
        return false;
    if (this->max_num != Right.max_num)
        return false;
    return true;
}

bool FPbFuZengData::operator!=(const FPbFuZengData& Right) const
{
    return !operator==(Right);
}

FPbRoleFuZengData::FPbRoleFuZengData()
{
    Reset();        
}

FPbRoleFuZengData::FPbRoleFuZengData(const idlepb::RoleFuZengData& Right)
{
    this->FromPb(Right);
}

void FPbRoleFuZengData::FromPb(const idlepb::RoleFuZengData& Right)
{
    data.Empty();
    for (const auto& Elem : Right.data())
    {
        data.Emplace(Elem);
    }
}

void FPbRoleFuZengData::ToPb(idlepb::RoleFuZengData* Out) const
{
    for (const auto& Elem : data)
    {
        Elem.ToPb(Out->add_data());    
    }    
}

void FPbRoleFuZengData::Reset()
{
    data = TArray<FPbFuZengData>();    
}

void FPbRoleFuZengData::operator=(const idlepb::RoleFuZengData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleFuZengData::operator==(const FPbRoleFuZengData& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbRoleFuZengData::operator!=(const FPbRoleFuZengData& Right) const
{
    return !operator==(Right);
}

FPbRoleFightModeData::FPbRoleFightModeData()
{
    Reset();        
}

FPbRoleFightModeData::FPbRoleFightModeData(const idlepb::RoleFightModeData& Right)
{
    this->FromPb(Right);
}

void FPbRoleFightModeData::FromPb(const idlepb::RoleFightModeData& Right)
{
    cur_mode = static_cast<EPbFightMode>(Right.cur_mode());
    last_attack_ticks = Right.last_attack_ticks();
    last_defence_ticks = Right.last_defence_ticks();
}

void FPbRoleFightModeData::ToPb(idlepb::RoleFightModeData* Out) const
{
    Out->set_cur_mode(static_cast<idlepb::FightMode>(cur_mode));
    Out->set_last_attack_ticks(last_attack_ticks);
    Out->set_last_defence_ticks(last_defence_ticks);    
}

void FPbRoleFightModeData::Reset()
{
    cur_mode = EPbFightMode();
    last_attack_ticks = int64();
    last_defence_ticks = int64();    
}

void FPbRoleFightModeData::operator=(const idlepb::RoleFightModeData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleFightModeData::operator==(const FPbRoleFightModeData& Right) const
{
    if (this->cur_mode != Right.cur_mode)
        return false;
    if (this->last_attack_ticks != Right.last_attack_ticks)
        return false;
    if (this->last_defence_ticks != Right.last_defence_ticks)
        return false;
    return true;
}

bool FPbRoleFightModeData::operator!=(const FPbRoleFightModeData& Right) const
{
    return !operator==(Right);
}

FPbRoleNormalSettings::FPbRoleNormalSettings()
{
    Reset();        
}

FPbRoleNormalSettings::FPbRoleNormalSettings(const idlepb::RoleNormalSettings& Right)
{
    this->FromPb(Right);
}

void FPbRoleNormalSettings::FromPb(const idlepb::RoleNormalSettings& Right)
{
    attack_lock_type = static_cast<EPbAttackLockType>(Right.attack_lock_type());
    attack_unlock_type = static_cast<EPbAttackUnlockType>(Right.attack_unlock_type());
    show_unlock_button = Right.show_unlock_button();
}

void FPbRoleNormalSettings::ToPb(idlepb::RoleNormalSettings* Out) const
{
    Out->set_attack_lock_type(static_cast<idlepb::AttackLockType>(attack_lock_type));
    Out->set_attack_unlock_type(static_cast<idlepb::AttackUnlockType>(attack_unlock_type));
    Out->set_show_unlock_button(show_unlock_button);    
}

void FPbRoleNormalSettings::Reset()
{
    attack_lock_type = EPbAttackLockType();
    attack_unlock_type = EPbAttackUnlockType();
    show_unlock_button = bool();    
}

void FPbRoleNormalSettings::operator=(const idlepb::RoleNormalSettings& Right)
{
    this->FromPb(Right);
}

bool FPbRoleNormalSettings::operator==(const FPbRoleNormalSettings& Right) const
{
    if (this->attack_lock_type != Right.attack_lock_type)
        return false;
    if (this->attack_unlock_type != Right.attack_unlock_type)
        return false;
    if (this->show_unlock_button != Right.show_unlock_button)
        return false;
    return true;
}

bool FPbRoleNormalSettings::operator!=(const FPbRoleNormalSettings& Right) const
{
    return !operator==(Right);
}

FPbRoleData::FPbRoleData()
{
    Reset();        
}

FPbRoleData::FPbRoleData(const idlepb::RoleData& Right)
{
    this->FromPb(Right);
}

void FPbRoleData::FromPb(const idlepb::RoleData& Right)
{
    user_id = Right.user_id();
    role_id = Right.role_id();
    role_name = UTF8_TO_TCHAR(Right.role_name().c_str());
    currency_data = Right.currency_data();
    physics_data = Right.physics_data();
    magic_data = Right.magic_data();
    cultivation_dir = static_cast<EPbCultivationDirection>(Right.cultivation_dir());
    last_exp_cycle_timestamp = Right.last_exp_cycle_timestamp();
    daily_counter = Right.daily_counter();
    model_config = Right.model_config();
    last_world_cfgid = Right.last_world_cfgid();
    last_world_pos = Right.last_world_pos();
    next_teleport_time = Right.next_teleport_time();
    last_unlock_arena_id = Right.last_unlock_arena_id();
    combat_power = Right.combat_power();
    login_count = Right.login_count();
    unlocked_modules.Empty();
    for (const auto& Elem : Right.unlocked_modules())
    {
        unlocked_modules.Emplace(Elem);
    }
    create_time = Right.create_time();
    pill_property_data.Empty();
    for (const auto& Elem : Right.pill_property_data())
    {
        pill_property_data.Emplace(Elem);
    }
    fight_mode = Right.fight_mode();
    qi_collector_rank = Right.qi_collector_rank();
    normal_settings = Right.normal_settings();
    offline_time = Right.offline_time();
    weekly_counter = Right.weekly_counter();
    last_arena_world_cfgid = Right.last_arena_world_cfgid();
    last_arena_world_pos = Right.last_arena_world_pos();
    game_stats = Right.game_stats();
    last_all_arena_world_cfgid = Right.last_all_arena_world_cfgid();
    last_all_arena_world_pos = Right.last_all_arena_world_pos();
}

void FPbRoleData::ToPb(idlepb::RoleData* Out) const
{
    Out->set_user_id(user_id);
    Out->set_role_id(role_id);
    Out->set_role_name(TCHAR_TO_UTF8(*role_name));
    currency_data.ToPb(Out->mutable_currency_data());
    physics_data.ToPb(Out->mutable_physics_data());
    magic_data.ToPb(Out->mutable_magic_data());
    Out->set_cultivation_dir(static_cast<idlepb::CultivationDirection>(cultivation_dir));
    Out->set_last_exp_cycle_timestamp(last_exp_cycle_timestamp);
    daily_counter.ToPb(Out->mutable_daily_counter());
    model_config.ToPb(Out->mutable_model_config());
    Out->set_last_world_cfgid(last_world_cfgid);
    last_world_pos.ToPb(Out->mutable_last_world_pos());
    Out->set_next_teleport_time(next_teleport_time);
    Out->set_last_unlock_arena_id(last_unlock_arena_id);
    Out->set_combat_power(combat_power);
    Out->set_login_count(login_count);
    for (const auto& Elem : unlocked_modules)
    {
        Out->add_unlocked_modules(Elem);    
    }
    Out->set_create_time(create_time);
    for (const auto& Elem : pill_property_data)
    {
        Elem.ToPb(Out->add_pill_property_data());    
    }
    fight_mode.ToPb(Out->mutable_fight_mode());
    Out->set_qi_collector_rank(qi_collector_rank);
    normal_settings.ToPb(Out->mutable_normal_settings());
    Out->set_offline_time(offline_time);
    weekly_counter.ToPb(Out->mutable_weekly_counter());
    Out->set_last_arena_world_cfgid(last_arena_world_cfgid);
    last_arena_world_pos.ToPb(Out->mutable_last_arena_world_pos());
    game_stats.ToPb(Out->mutable_game_stats());
    Out->set_last_all_arena_world_cfgid(last_all_arena_world_cfgid);
    last_all_arena_world_pos.ToPb(Out->mutable_last_all_arena_world_pos());    
}

void FPbRoleData::Reset()
{
    user_id = int64();
    role_id = int64();
    role_name = FString();
    currency_data = FPbCurrencyData();
    physics_data = FPbCultivationData();
    magic_data = FPbCultivationData();
    cultivation_dir = EPbCultivationDirection();
    last_exp_cycle_timestamp = int64();
    daily_counter = FPbRoleDailyCounter();
    model_config = FPbCharacterModelConfig();
    last_world_cfgid = int32();
    last_world_pos = FPbVector3();
    next_teleport_time = int64();
    last_unlock_arena_id = int32();
    combat_power = int64();
    login_count = int32();
    unlocked_modules = TArray<int32>();
    create_time = int64();
    pill_property_data = TArray<FPbPillPropertyData>();
    fight_mode = FPbRoleFightModeData();
    qi_collector_rank = int32();
    normal_settings = FPbRoleNormalSettings();
    offline_time = int64();
    weekly_counter = FPbRoleWeeklyCounter();
    last_arena_world_cfgid = int32();
    last_arena_world_pos = FPbVector3();
    game_stats = FPbGameStatsData();
    last_all_arena_world_cfgid = int32();
    last_all_arena_world_pos = FPbVector3();    
}

void FPbRoleData::operator=(const idlepb::RoleData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleData::operator==(const FPbRoleData& Right) const
{
    if (this->user_id != Right.user_id)
        return false;
    if (this->role_id != Right.role_id)
        return false;
    if (this->role_name != Right.role_name)
        return false;
    if (this->currency_data != Right.currency_data)
        return false;
    if (this->physics_data != Right.physics_data)
        return false;
    if (this->magic_data != Right.magic_data)
        return false;
    if (this->cultivation_dir != Right.cultivation_dir)
        return false;
    if (this->last_exp_cycle_timestamp != Right.last_exp_cycle_timestamp)
        return false;
    if (this->daily_counter != Right.daily_counter)
        return false;
    if (this->model_config != Right.model_config)
        return false;
    if (this->last_world_cfgid != Right.last_world_cfgid)
        return false;
    if (this->last_world_pos != Right.last_world_pos)
        return false;
    if (this->next_teleport_time != Right.next_teleport_time)
        return false;
    if (this->last_unlock_arena_id != Right.last_unlock_arena_id)
        return false;
    if (this->combat_power != Right.combat_power)
        return false;
    if (this->login_count != Right.login_count)
        return false;
    if (this->unlocked_modules != Right.unlocked_modules)
        return false;
    if (this->create_time != Right.create_time)
        return false;
    if (this->pill_property_data != Right.pill_property_data)
        return false;
    if (this->fight_mode != Right.fight_mode)
        return false;
    if (this->qi_collector_rank != Right.qi_collector_rank)
        return false;
    if (this->normal_settings != Right.normal_settings)
        return false;
    if (this->offline_time != Right.offline_time)
        return false;
    if (this->weekly_counter != Right.weekly_counter)
        return false;
    if (this->last_arena_world_cfgid != Right.last_arena_world_cfgid)
        return false;
    if (this->last_arena_world_pos != Right.last_arena_world_pos)
        return false;
    if (this->game_stats != Right.game_stats)
        return false;
    if (this->last_all_arena_world_cfgid != Right.last_all_arena_world_cfgid)
        return false;
    if (this->last_all_arena_world_pos != Right.last_all_arena_world_pos)
        return false;
    return true;
}

bool FPbRoleData::operator!=(const FPbRoleData& Right) const
{
    return !operator==(Right);
}

FPbSimpleAbilityData::FPbSimpleAbilityData()
{
    Reset();        
}

FPbSimpleAbilityData::FPbSimpleAbilityData(const idlepb::SimpleAbilityData& Right)
{
    this->FromPb(Right);
}

void FPbSimpleAbilityData::FromPb(const idlepb::SimpleAbilityData& Right)
{
    id = Right.id();
    grade = Right.grade();
    study_grade = Right.study_grade();
}

void FPbSimpleAbilityData::ToPb(idlepb::SimpleAbilityData* Out) const
{
    Out->set_id(id);
    Out->set_grade(grade);
    Out->set_study_grade(study_grade);    
}

void FPbSimpleAbilityData::Reset()
{
    id = int32();
    grade = int32();
    study_grade = int32();    
}

void FPbSimpleAbilityData::operator=(const idlepb::SimpleAbilityData& Right)
{
    this->FromPb(Right);
}

bool FPbSimpleAbilityData::operator==(const FPbSimpleAbilityData& Right) const
{
    if (this->id != Right.id)
        return false;
    if (this->grade != Right.grade)
        return false;
    if (this->study_grade != Right.study_grade)
        return false;
    return true;
}

bool FPbSimpleAbilityData::operator!=(const FPbSimpleAbilityData& Right) const
{
    return !operator==(Right);
}

FPbSimpleGongFaData::FPbSimpleGongFaData()
{
    Reset();        
}

FPbSimpleGongFaData::FPbSimpleGongFaData(const idlepb::SimpleGongFaData& Right)
{
    this->FromPb(Right);
}

void FPbSimpleGongFaData::FromPb(const idlepb::SimpleGongFaData& Right)
{
    id = Right.id();
    level = Right.level();
    is_full = Right.is_full();
}

void FPbSimpleGongFaData::ToPb(idlepb::SimpleGongFaData* Out) const
{
    Out->set_id(id);
    Out->set_level(level);
    Out->set_is_full(is_full);    
}

void FPbSimpleGongFaData::Reset()
{
    id = int32();
    level = int32();
    is_full = bool();    
}

void FPbSimpleGongFaData::operator=(const idlepb::SimpleGongFaData& Right)
{
    this->FromPb(Right);
}

bool FPbSimpleGongFaData::operator==(const FPbSimpleGongFaData& Right) const
{
    if (this->id != Right.id)
        return false;
    if (this->level != Right.level)
        return false;
    if (this->is_full != Right.is_full)
        return false;
    return true;
}

bool FPbSimpleGongFaData::operator!=(const FPbSimpleGongFaData& Right) const
{
    return !operator==(Right);
}

FPbRoleInfo::FPbRoleInfo()
{
    Reset();        
}

FPbRoleInfo::FPbRoleInfo(const idlepb::RoleInfo& Right)
{
    this->FromPb(Right);
}

void FPbRoleInfo::FromPb(const idlepb::RoleInfo& Right)
{
    user_id = Right.user_id();
    role_id = Right.role_id();
    role_name = UTF8_TO_TCHAR(Right.role_name().c_str());
    create_time = Right.create_time();
    cultivation_main_dir = static_cast<EPbCultivationDirection>(Right.cultivation_main_dir());
    cultivation_main_rank = Right.cultivation_main_rank();
    cultivation_second_dir = static_cast<EPbCultivationDirection>(Right.cultivation_second_dir());
    cultivation_second_rank = Right.cultivation_second_rank();
    character_model = Right.character_model();
    title.Empty();
    for (const auto& Elem : Right.title())
    {
        title.Emplace(Elem);
    }
    combat_power = Right.combat_power();
    equipments.Empty();
    for (const auto& Elem : Right.equipments())
    {
        equipments.Emplace(Elem);
    }
    unlocked_equipment_slots.Empty();
    for (const auto& Elem : Right.unlocked_equipment_slots())
    {
        unlocked_equipment_slots.Emplace(Elem);
    }
    sept_id = Right.sept_id();
    sept_position = static_cast<EPbSeptPosition>(Right.sept_position());
    sept_name = UTF8_TO_TCHAR(Right.sept_name().c_str());
    sept_logo = Right.sept_logo();
    all_stats_data = Right.all_stats_data();
    slotted_abilities.Empty();
    for (const auto& Elem : Right.slotted_abilities())
    {
        slotted_abilities.Emplace(Elem);
    }
    unslotted_abilities.Empty();
    for (const auto& Elem : Right.unslotted_abilities())
    {
        unslotted_abilities.Emplace(Elem);
    }
    gong_fa_entries.Empty();
    for (const auto& Elem : Right.gong_fa_entries())
    {
        gong_fa_entries.Emplace(Elem);
    }
}

void FPbRoleInfo::ToPb(idlepb::RoleInfo* Out) const
{
    Out->set_user_id(user_id);
    Out->set_role_id(role_id);
    Out->set_role_name(TCHAR_TO_UTF8(*role_name));
    Out->set_create_time(create_time);
    Out->set_cultivation_main_dir(static_cast<idlepb::CultivationDirection>(cultivation_main_dir));
    Out->set_cultivation_main_rank(cultivation_main_rank);
    Out->set_cultivation_second_dir(static_cast<idlepb::CultivationDirection>(cultivation_second_dir));
    Out->set_cultivation_second_rank(cultivation_second_rank);
    character_model.ToPb(Out->mutable_character_model());
    for (const auto& Elem : title)
    {
        Out->add_title(Elem);    
    }
    Out->set_combat_power(combat_power);
    for (const auto& Elem : equipments)
    {
        Elem.ToPb(Out->add_equipments());    
    }
    for (const auto& Elem : unlocked_equipment_slots)
    {
        Out->add_unlocked_equipment_slots(Elem);    
    }
    Out->set_sept_id(sept_id);
    Out->set_sept_position(static_cast<idlepb::SeptPosition>(sept_position));
    Out->set_sept_name(TCHAR_TO_UTF8(*sept_name));
    Out->set_sept_logo(sept_logo);
    all_stats_data.ToPb(Out->mutable_all_stats_data());
    for (const auto& Elem : slotted_abilities)
    {
        Elem.ToPb(Out->add_slotted_abilities());    
    }
    for (const auto& Elem : unslotted_abilities)
    {
        Elem.ToPb(Out->add_unslotted_abilities());    
    }
    for (const auto& Elem : gong_fa_entries)
    {
        Elem.ToPb(Out->add_gong_fa_entries());    
    }    
}

void FPbRoleInfo::Reset()
{
    user_id = int64();
    role_id = int64();
    role_name = FString();
    create_time = int64();
    cultivation_main_dir = EPbCultivationDirection();
    cultivation_main_rank = int32();
    cultivation_second_dir = EPbCultivationDirection();
    cultivation_second_rank = int32();
    character_model = FPbCharacterModelConfig();
    title = TArray<int32>();
    combat_power = int64();
    equipments = TArray<FPbItemData>();
    unlocked_equipment_slots = TArray<int32>();
    sept_id = int64();
    sept_position = EPbSeptPosition();
    sept_name = FString();
    sept_logo = int32();
    all_stats_data = FPbGameStatsAllModuleData();
    slotted_abilities = TArray<FPbSimpleAbilityData>();
    unslotted_abilities = TArray<FPbSimpleAbilityData>();
    gong_fa_entries = TArray<FPbSimpleGongFaData>();    
}

void FPbRoleInfo::operator=(const idlepb::RoleInfo& Right)
{
    this->FromPb(Right);
}

bool FPbRoleInfo::operator==(const FPbRoleInfo& Right) const
{
    if (this->user_id != Right.user_id)
        return false;
    if (this->role_id != Right.role_id)
        return false;
    if (this->role_name != Right.role_name)
        return false;
    if (this->create_time != Right.create_time)
        return false;
    if (this->cultivation_main_dir != Right.cultivation_main_dir)
        return false;
    if (this->cultivation_main_rank != Right.cultivation_main_rank)
        return false;
    if (this->cultivation_second_dir != Right.cultivation_second_dir)
        return false;
    if (this->cultivation_second_rank != Right.cultivation_second_rank)
        return false;
    if (this->character_model != Right.character_model)
        return false;
    if (this->title != Right.title)
        return false;
    if (this->combat_power != Right.combat_power)
        return false;
    if (this->equipments != Right.equipments)
        return false;
    if (this->unlocked_equipment_slots != Right.unlocked_equipment_slots)
        return false;
    if (this->sept_id != Right.sept_id)
        return false;
    if (this->sept_position != Right.sept_position)
        return false;
    if (this->sept_name != Right.sept_name)
        return false;
    if (this->sept_logo != Right.sept_logo)
        return false;
    if (this->all_stats_data != Right.all_stats_data)
        return false;
    if (this->slotted_abilities != Right.slotted_abilities)
        return false;
    if (this->unslotted_abilities != Right.unslotted_abilities)
        return false;
    if (this->gong_fa_entries != Right.gong_fa_entries)
        return false;
    return true;
}

bool FPbRoleInfo::operator!=(const FPbRoleInfo& Right) const
{
    return !operator==(Right);
}

FPbRoleInventoryData::FPbRoleInventoryData()
{
    Reset();        
}

FPbRoleInventoryData::FPbRoleInventoryData(const idlepb::RoleInventoryData& Right)
{
    this->FromPb(Right);
}

void FPbRoleInventoryData::FromPb(const idlepb::RoleInventoryData& Right)
{
    next_item_id = Right.next_item_id();
    items.Empty();
    for (const auto& Elem : Right.items())
    {
        items.Emplace(Elem);
    }
    hp_pill_cooldown_expire_time = Right.hp_pill_cooldown_expire_time();
    mp_pill_cooldown_expire_time = Right.mp_pill_cooldown_expire_time();
    unlocked_equipment_slots.Empty();
    for (const auto& Elem : Right.unlocked_equipment_slots())
    {
        unlocked_equipment_slots.Emplace(Elem);
    }
    inventory_space_num = Right.inventory_space_num();
}

void FPbRoleInventoryData::ToPb(idlepb::RoleInventoryData* Out) const
{
    Out->set_next_item_id(next_item_id);
    for (const auto& Elem : items)
    {
        Elem.ToPb(Out->add_items());    
    }
    Out->set_hp_pill_cooldown_expire_time(hp_pill_cooldown_expire_time);
    Out->set_mp_pill_cooldown_expire_time(mp_pill_cooldown_expire_time);
    for (const auto& Elem : unlocked_equipment_slots)
    {
        Out->add_unlocked_equipment_slots(Elem);    
    }
    Out->set_inventory_space_num(inventory_space_num);    
}

void FPbRoleInventoryData::Reset()
{
    next_item_id = int64();
    items = TArray<FPbItemData>();
    hp_pill_cooldown_expire_time = int64();
    mp_pill_cooldown_expire_time = int64();
    unlocked_equipment_slots = TArray<int32>();
    inventory_space_num = int32();    
}

void FPbRoleInventoryData::operator=(const idlepb::RoleInventoryData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleInventoryData::operator==(const FPbRoleInventoryData& Right) const
{
    if (this->next_item_id != Right.next_item_id)
        return false;
    if (this->items != Right.items)
        return false;
    if (this->hp_pill_cooldown_expire_time != Right.hp_pill_cooldown_expire_time)
        return false;
    if (this->mp_pill_cooldown_expire_time != Right.mp_pill_cooldown_expire_time)
        return false;
    if (this->unlocked_equipment_slots != Right.unlocked_equipment_slots)
        return false;
    if (this->inventory_space_num != Right.inventory_space_num)
        return false;
    return true;
}

bool FPbRoleInventoryData::operator!=(const FPbRoleInventoryData& Right) const
{
    return !operator==(Right);
}

FPbRoleTemporaryPackageData::FPbRoleTemporaryPackageData()
{
    Reset();        
}

FPbRoleTemporaryPackageData::FPbRoleTemporaryPackageData(const idlepb::RoleTemporaryPackageData& Right)
{
    this->FromPb(Right);
}

void FPbRoleTemporaryPackageData::FromPb(const idlepb::RoleTemporaryPackageData& Right)
{
    items.Empty();
    for (const auto& Elem : Right.items())
    {
        items.Emplace(Elem);
    }
    last_extract_time = Right.last_extract_time();
    next_item_id = Right.next_item_id();
}

void FPbRoleTemporaryPackageData::ToPb(idlepb::RoleTemporaryPackageData* Out) const
{
    for (const auto& Elem : items)
    {
        Elem.ToPb(Out->add_items());    
    }
    Out->set_last_extract_time(last_extract_time);
    Out->set_next_item_id(next_item_id);    
}

void FPbRoleTemporaryPackageData::Reset()
{
    items = TArray<FPbTemporaryPackageItem>();
    last_extract_time = int64();
    next_item_id = int64();    
}

void FPbRoleTemporaryPackageData::operator=(const idlepb::RoleTemporaryPackageData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleTemporaryPackageData::operator==(const FPbRoleTemporaryPackageData& Right) const
{
    if (this->items != Right.items)
        return false;
    if (this->last_extract_time != Right.last_extract_time)
        return false;
    if (this->next_item_id != Right.next_item_id)
        return false;
    return true;
}

bool FPbRoleTemporaryPackageData::operator!=(const FPbRoleTemporaryPackageData& Right) const
{
    return !operator==(Right);
}

FPbRoleArenaExplorationStatisticalData::FPbRoleArenaExplorationStatisticalData()
{
    Reset();        
}

FPbRoleArenaExplorationStatisticalData::FPbRoleArenaExplorationStatisticalData(const idlepb::RoleArenaExplorationStatisticalData& Right)
{
    this->FromPb(Right);
}

void FPbRoleArenaExplorationStatisticalData::FromPb(const idlepb::RoleArenaExplorationStatisticalData& Right)
{
    items.Empty();
    for (const auto& Elem : Right.items())
    {
        items.Emplace(Elem);
    }
}

void FPbRoleArenaExplorationStatisticalData::ToPb(idlepb::RoleArenaExplorationStatisticalData* Out) const
{
    for (const auto& Elem : items)
    {
        Elem.ToPb(Out->add_items());    
    }    
}

void FPbRoleArenaExplorationStatisticalData::Reset()
{
    items = TArray<FPbArenaExplorationStatisticalItem>();    
}

void FPbRoleArenaExplorationStatisticalData::operator=(const idlepb::RoleArenaExplorationStatisticalData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleArenaExplorationStatisticalData::operator==(const FPbRoleArenaExplorationStatisticalData& Right) const
{
    if (this->items != Right.items)
        return false;
    return true;
}

bool FPbRoleArenaExplorationStatisticalData::operator!=(const FPbRoleArenaExplorationStatisticalData& Right) const
{
    return !operator==(Right);
}

FPbQuestProgress::FPbQuestProgress()
{
    Reset();        
}

FPbQuestProgress::FPbQuestProgress(const idlepb::QuestProgress& Right)
{
    this->FromPb(Right);
}

void FPbQuestProgress::FromPb(const idlepb::QuestProgress& Right)
{
    id = Right.id();
    progress.Empty();
    for (const auto& Elem : Right.progress())
    {
        progress.Emplace(Elem);
    }
    state = Right.state();
}

void FPbQuestProgress::ToPb(idlepb::QuestProgress* Out) const
{
    Out->set_id(id);
    for (const auto& Elem : progress)
    {
        Out->add_progress(Elem);    
    }
    Out->set_state(state);    
}

void FPbQuestProgress::Reset()
{
    id = int32();
    progress = TArray<int32>();
    state = int32();    
}

void FPbQuestProgress::operator=(const idlepb::QuestProgress& Right)
{
    this->FromPb(Right);
}

bool FPbQuestProgress::operator==(const FPbQuestProgress& Right) const
{
    if (this->id != Right.id)
        return false;
    if (this->progress != Right.progress)
        return false;
    if (this->state != Right.state)
        return false;
    return true;
}

bool FPbQuestProgress::operator!=(const FPbQuestProgress& Right) const
{
    return !operator==(Right);
}

FPbRoleQuestData::FPbRoleQuestData()
{
    Reset();        
}

FPbRoleQuestData::FPbRoleQuestData(const idlepb::RoleQuestData& Right)
{
    this->FromPb(Right);
}

void FPbRoleQuestData::FromPb(const idlepb::RoleQuestData& Right)
{
    accepted_quests.Empty();
    for (const auto& Elem : Right.accepted_quests())
    {
        accepted_quests.Emplace(Elem);
    }
    finished_quests.Empty();
    for (const auto& Elem : Right.finished_quests())
    {
        finished_quests.Emplace(Elem);
    }
    quests_progress.Empty();
    for (const auto& Elem : Right.quests_progress())
    {
        quests_progress.Emplace(Elem);
    }
}

void FPbRoleQuestData::ToPb(idlepb::RoleQuestData* Out) const
{
    for (const auto& Elem : accepted_quests)
    {
        Out->add_accepted_quests(Elem);    
    }
    for (const auto& Elem : finished_quests)
    {
        Out->add_finished_quests(Elem);    
    }
    for (const auto& Elem : quests_progress)
    {
        Elem.ToPb(Out->add_quests_progress());    
    }    
}

void FPbRoleQuestData::Reset()
{
    accepted_quests = TArray<int32>();
    finished_quests = TArray<int32>();
    quests_progress = TArray<FPbQuestProgress>();    
}

void FPbRoleQuestData::operator=(const idlepb::RoleQuestData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleQuestData::operator==(const FPbRoleQuestData& Right) const
{
    if (this->accepted_quests != Right.accepted_quests)
        return false;
    if (this->finished_quests != Right.finished_quests)
        return false;
    if (this->quests_progress != Right.quests_progress)
        return false;
    return true;
}

bool FPbRoleQuestData::operator!=(const FPbRoleQuestData& Right) const
{
    return !operator==(Right);
}

FPbRoleShopData::FPbRoleShopData()
{
    Reset();        
}

FPbRoleShopData::FPbRoleShopData(const idlepb::RoleShopData& Right)
{
    this->FromPb(Right);
}

void FPbRoleShopData::FromPb(const idlepb::RoleShopData& Right)
{
    items.Empty();
    for (const auto& Elem : Right.items())
    {
        items.Emplace(Elem);
    }
    today_manual_refresh_num = Right.today_manual_refresh_num();
    last_auto_refresh_time = Right.last_auto_refresh_time();
    last_reset_time = Right.last_reset_time();
    guarantee_refresh_num = Right.guarantee_refresh_num();
}

void FPbRoleShopData::ToPb(idlepb::RoleShopData* Out) const
{
    for (const auto& Elem : items)
    {
        Elem.ToPb(Out->add_items());    
    }
    Out->set_today_manual_refresh_num(today_manual_refresh_num);
    Out->set_last_auto_refresh_time(last_auto_refresh_time);
    Out->set_last_reset_time(last_reset_time);
    Out->set_guarantee_refresh_num(guarantee_refresh_num);    
}

void FPbRoleShopData::Reset()
{
    items = TArray<FPbShopItem>();
    today_manual_refresh_num = int32();
    last_auto_refresh_time = int64();
    last_reset_time = int64();
    guarantee_refresh_num = int32();    
}

void FPbRoleShopData::operator=(const idlepb::RoleShopData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleShopData::operator==(const FPbRoleShopData& Right) const
{
    if (this->items != Right.items)
        return false;
    if (this->today_manual_refresh_num != Right.today_manual_refresh_num)
        return false;
    if (this->last_auto_refresh_time != Right.last_auto_refresh_time)
        return false;
    if (this->last_reset_time != Right.last_reset_time)
        return false;
    if (this->guarantee_refresh_num != Right.guarantee_refresh_num)
        return false;
    return true;
}

bool FPbRoleShopData::operator!=(const FPbRoleShopData& Right) const
{
    return !operator==(Right);
}

FPbRoleDeluxeShopData::FPbRoleDeluxeShopData()
{
    Reset();        
}

FPbRoleDeluxeShopData::FPbRoleDeluxeShopData(const idlepb::RoleDeluxeShopData& Right)
{
    this->FromPb(Right);
}

void FPbRoleDeluxeShopData::FromPb(const idlepb::RoleDeluxeShopData& Right)
{
    items.Empty();
    for (const auto& Elem : Right.items())
    {
        items.Emplace(Elem);
    }
    today_manual_refresh_num_item = Right.today_manual_refresh_num_item();
    today_manual_refresh_num_gold = Right.today_manual_refresh_num_gold();
    last_auto_refresh_time = Right.last_auto_refresh_time();
    last_reset_time = Right.last_reset_time();
}

void FPbRoleDeluxeShopData::ToPb(idlepb::RoleDeluxeShopData* Out) const
{
    for (const auto& Elem : items)
    {
        Elem.ToPb(Out->add_items());    
    }
    Out->set_today_manual_refresh_num_item(today_manual_refresh_num_item);
    Out->set_today_manual_refresh_num_gold(today_manual_refresh_num_gold);
    Out->set_last_auto_refresh_time(last_auto_refresh_time);
    Out->set_last_reset_time(last_reset_time);    
}

void FPbRoleDeluxeShopData::Reset()
{
    items = TArray<FPbDeluxeShopItem>();
    today_manual_refresh_num_item = int32();
    today_manual_refresh_num_gold = int32();
    last_auto_refresh_time = int64();
    last_reset_time = int64();    
}

void FPbRoleDeluxeShopData::operator=(const idlepb::RoleDeluxeShopData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleDeluxeShopData::operator==(const FPbRoleDeluxeShopData& Right) const
{
    if (this->items != Right.items)
        return false;
    if (this->today_manual_refresh_num_item != Right.today_manual_refresh_num_item)
        return false;
    if (this->today_manual_refresh_num_gold != Right.today_manual_refresh_num_gold)
        return false;
    if (this->last_auto_refresh_time != Right.last_auto_refresh_time)
        return false;
    if (this->last_reset_time != Right.last_reset_time)
        return false;
    return true;
}

bool FPbRoleDeluxeShopData::operator!=(const FPbRoleDeluxeShopData& Right) const
{
    return !operator==(Right);
}

FPbMailAttachment::FPbMailAttachment()
{
    Reset();        
}

FPbMailAttachment::FPbMailAttachment(const idlepb::MailAttachment& Right)
{
    this->FromPb(Right);
}

void FPbMailAttachment::FromPb(const idlepb::MailAttachment& Right)
{
    id = Right.id();
    num = Right.num();
    received = Right.received();
}

void FPbMailAttachment::ToPb(idlepb::MailAttachment* Out) const
{
    Out->set_id(id);
    Out->set_num(num);
    Out->set_received(received);    
}

void FPbMailAttachment::Reset()
{
    id = int32();
    num = int32();
    received = bool();    
}

void FPbMailAttachment::operator=(const idlepb::MailAttachment& Right)
{
    this->FromPb(Right);
}

bool FPbMailAttachment::operator==(const FPbMailAttachment& Right) const
{
    if (this->id != Right.id)
        return false;
    if (this->num != Right.num)
        return false;
    if (this->received != Right.received)
        return false;
    return true;
}

bool FPbMailAttachment::operator!=(const FPbMailAttachment& Right) const
{
    return !operator==(Right);
}

FPbMail::FPbMail()
{
    Reset();        
}

FPbMail::FPbMail(const idlepb::Mail& Right)
{
    this->FromPb(Right);
}

void FPbMail::FromPb(const idlepb::Mail& Right)
{
    id = Right.id();
    type = static_cast<EPbMailType>(Right.type());
    title = UTF8_TO_TCHAR(Right.title().c_str());
    subtitle = UTF8_TO_TCHAR(Right.subtitle().c_str());
    body_text = UTF8_TO_TCHAR(Right.body_text().c_str());
    sender = UTF8_TO_TCHAR(Right.sender().c_str());
    attachments.Empty();
    for (const auto& Elem : Right.attachments())
    {
        attachments.Emplace(Elem);
    }
    begin_date = Right.begin_date();
    keep_time = Right.keep_time();
    is_read = Right.is_read();
    is_received = Right.is_received();
    equipments.Empty();
    for (const auto& Elem : Right.equipments())
    {
        equipments.Emplace(Elem);
    }
}

void FPbMail::ToPb(idlepb::Mail* Out) const
{
    Out->set_id(id);
    Out->set_type(static_cast<idlepb::MailType>(type));
    Out->set_title(TCHAR_TO_UTF8(*title));
    Out->set_subtitle(TCHAR_TO_UTF8(*subtitle));
    Out->set_body_text(TCHAR_TO_UTF8(*body_text));
    Out->set_sender(TCHAR_TO_UTF8(*sender));
    for (const auto& Elem : attachments)
    {
        Elem.ToPb(Out->add_attachments());    
    }
    Out->set_begin_date(begin_date);
    Out->set_keep_time(keep_time);
    Out->set_is_read(is_read);
    Out->set_is_received(is_received);
    for (const auto& Elem : equipments)
    {
        Elem.ToPb(Out->add_equipments());    
    }    
}

void FPbMail::Reset()
{
    id = int32();
    type = EPbMailType();
    title = FString();
    subtitle = FString();
    body_text = FString();
    sender = FString();
    attachments = TArray<FPbMailAttachment>();
    begin_date = int64();
    keep_time = int32();
    is_read = bool();
    is_received = bool();
    equipments = TArray<FPbItemData>();    
}

void FPbMail::operator=(const idlepb::Mail& Right)
{
    this->FromPb(Right);
}

bool FPbMail::operator==(const FPbMail& Right) const
{
    if (this->id != Right.id)
        return false;
    if (this->type != Right.type)
        return false;
    if (this->title != Right.title)
        return false;
    if (this->subtitle != Right.subtitle)
        return false;
    if (this->body_text != Right.body_text)
        return false;
    if (this->sender != Right.sender)
        return false;
    if (this->attachments != Right.attachments)
        return false;
    if (this->begin_date != Right.begin_date)
        return false;
    if (this->keep_time != Right.keep_time)
        return false;
    if (this->is_read != Right.is_read)
        return false;
    if (this->is_received != Right.is_received)
        return false;
    if (this->equipments != Right.equipments)
        return false;
    return true;
}

bool FPbMail::operator!=(const FPbMail& Right) const
{
    return !operator==(Right);
}

FPbRoleMailData::FPbRoleMailData()
{
    Reset();        
}

FPbRoleMailData::FPbRoleMailData(const idlepb::RoleMailData& Right)
{
    this->FromPb(Right);
}

void FPbRoleMailData::FromPb(const idlepb::RoleMailData& Right)
{
    mail_box.Empty();
    for (const auto& Elem : Right.mail_box())
    {
        mail_box.Emplace(Elem);
    }
    total_num = Right.total_num();
    system_mail_counter.Empty();
    for (const auto& Elem : Right.system_mail_counter())
    {
        system_mail_counter.Emplace(Elem);
    }
}

void FPbRoleMailData::ToPb(idlepb::RoleMailData* Out) const
{
    for (const auto& Elem : mail_box)
    {
        Elem.ToPb(Out->add_mail_box());    
    }
    Out->set_total_num(total_num);
    for (const auto& Elem : system_mail_counter)
    {
        Elem.ToPb(Out->add_system_mail_counter());    
    }    
}

void FPbRoleMailData::Reset()
{
    mail_box = TArray<FPbMail>();
    total_num = int32();
    system_mail_counter = TArray<FPbMapValueInt32>();    
}

void FPbRoleMailData::operator=(const idlepb::RoleMailData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleMailData::operator==(const FPbRoleMailData& Right) const
{
    if (this->mail_box != Right.mail_box)
        return false;
    if (this->total_num != Right.total_num)
        return false;
    if (this->system_mail_counter != Right.system_mail_counter)
        return false;
    return true;
}

bool FPbRoleMailData::operator!=(const FPbRoleMailData& Right) const
{
    return !operator==(Right);
}

FPbOfflineAwardSummary::FPbOfflineAwardSummary()
{
    Reset();        
}

FPbOfflineAwardSummary::FPbOfflineAwardSummary(const idlepb::OfflineAwardSummary& Right)
{
    this->FromPb(Right);
}

void FPbOfflineAwardSummary::FromPb(const idlepb::OfflineAwardSummary& Right)
{
    dir = static_cast<EPbCultivationDirection>(Right.dir());
    time_during = Right.time_during();
    add_exp = Right.add_exp();
    add_attr = Right.add_attr();
}

void FPbOfflineAwardSummary::ToPb(idlepb::OfflineAwardSummary* Out) const
{
    Out->set_dir(static_cast<idlepb::CultivationDirection>(dir));
    Out->set_time_during(time_during);
    Out->set_add_exp(add_exp);
    Out->set_add_attr(add_attr);    
}

void FPbOfflineAwardSummary::Reset()
{
    dir = EPbCultivationDirection();
    time_during = int64();
    add_exp = int64();
    add_attr = int64();    
}

void FPbOfflineAwardSummary::operator=(const idlepb::OfflineAwardSummary& Right)
{
    this->FromPb(Right);
}

bool FPbOfflineAwardSummary::operator==(const FPbOfflineAwardSummary& Right) const
{
    if (this->dir != Right.dir)
        return false;
    if (this->time_during != Right.time_during)
        return false;
    if (this->add_exp != Right.add_exp)
        return false;
    if (this->add_attr != Right.add_attr)
        return false;
    return true;
}

bool FPbOfflineAwardSummary::operator!=(const FPbOfflineAwardSummary& Right) const
{
    return !operator==(Right);
}

FPbRoleOfflineData::FPbRoleOfflineData()
{
    Reset();        
}

FPbRoleOfflineData::FPbRoleOfflineData(const idlepb::RoleOfflineData& Right)
{
    this->FromPb(Right);
}

void FPbRoleOfflineData::FromPb(const idlepb::RoleOfflineData& Right)
{
    last_exp_value = Right.last_exp_value();
    last_attr_value = Right.last_attr_value();
    last_award_summary = Right.last_award_summary();
}

void FPbRoleOfflineData::ToPb(idlepb::RoleOfflineData* Out) const
{
    Out->set_last_exp_value(last_exp_value);
    Out->set_last_attr_value(last_attr_value);
    last_award_summary.ToPb(Out->mutable_last_award_summary());    
}

void FPbRoleOfflineData::Reset()
{
    last_exp_value = int64();
    last_attr_value = int64();
    last_award_summary = FPbOfflineAwardSummary();    
}

void FPbRoleOfflineData::operator=(const idlepb::RoleOfflineData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleOfflineData::operator==(const FPbRoleOfflineData& Right) const
{
    if (this->last_exp_value != Right.last_exp_value)
        return false;
    if (this->last_attr_value != Right.last_attr_value)
        return false;
    if (this->last_award_summary != Right.last_award_summary)
        return false;
    return true;
}

bool FPbRoleOfflineData::operator!=(const FPbRoleOfflineData& Right) const
{
    return !operator==(Right);
}

FPbPillElixirData::FPbPillElixirData()
{
    Reset();        
}

FPbPillElixirData::FPbPillElixirData(const idlepb::PillElixirData& Right)
{
    this->FromPb(Right);
}

void FPbPillElixirData::FromPb(const idlepb::PillElixirData& Right)
{
    item_id = Right.item_id();
    holding_num = Right.holding_num();
}

void FPbPillElixirData::ToPb(idlepb::PillElixirData* Out) const
{
    Out->set_item_id(item_id);
    Out->set_holding_num(holding_num);    
}

void FPbPillElixirData::Reset()
{
    item_id = int32();
    holding_num = int32();    
}

void FPbPillElixirData::operator=(const idlepb::PillElixirData& Right)
{
    this->FromPb(Right);
}

bool FPbPillElixirData::operator==(const FPbPillElixirData& Right) const
{
    if (this->item_id != Right.item_id)
        return false;
    if (this->holding_num != Right.holding_num)
        return false;
    return true;
}

bool FPbPillElixirData::operator!=(const FPbPillElixirData& Right) const
{
    return !operator==(Right);
}

FPbRolePillElixirData::FPbRolePillElixirData()
{
    Reset();        
}

FPbRolePillElixirData::FPbRolePillElixirData(const idlepb::RolePillElixirData& Right)
{
    this->FromPb(Right);
}

void FPbRolePillElixirData::FromPb(const idlepb::RolePillElixirData& Right)
{
    pill_data.Empty();
    for (const auto& Elem : Right.pill_data())
    {
        pill_data.Emplace(Elem);
    }
    limit_double = Right.limit_double();
    limit_exp = Right.limit_exp();
    limit_property = Right.limit_property();
}

void FPbRolePillElixirData::ToPb(idlepb::RolePillElixirData* Out) const
{
    for (const auto& Elem : pill_data)
    {
        Elem.ToPb(Out->add_pill_data());    
    }
    Out->set_limit_double(limit_double);
    Out->set_limit_exp(limit_exp);
    Out->set_limit_property(limit_property);    
}

void FPbRolePillElixirData::Reset()
{
    pill_data = TArray<FPbPillElixirData>();
    limit_double = int32();
    limit_exp = int32();
    limit_property = int32();    
}

void FPbRolePillElixirData::operator=(const idlepb::RolePillElixirData& Right)
{
    this->FromPb(Right);
}

bool FPbRolePillElixirData::operator==(const FPbRolePillElixirData& Right) const
{
    if (this->pill_data != Right.pill_data)
        return false;
    if (this->limit_double != Right.limit_double)
        return false;
    if (this->limit_exp != Right.limit_exp)
        return false;
    if (this->limit_property != Right.limit_property)
        return false;
    return true;
}

bool FPbRolePillElixirData::operator!=(const FPbRolePillElixirData& Right) const
{
    return !operator==(Right);
}

FPbAbilityEffectDefData::FPbAbilityEffectDefData()
{
    Reset();        
}

FPbAbilityEffectDefData::FPbAbilityEffectDefData(const idlepb::AbilityEffectDefData& Right)
{
    this->FromPb(Right);
}

void FPbAbilityEffectDefData::FromPb(const idlepb::AbilityEffectDefData& Right)
{
    type = Right.type();
    duration = Right.duration();
    period = Right.period();
    duration_policy = Right.duration_policy();
    x = Right.x();
    y = Right.y();
    z = Right.z();
    m = Right.m();
    n = Right.n();
}

void FPbAbilityEffectDefData::ToPb(idlepb::AbilityEffectDefData* Out) const
{
    Out->set_type(type);
    Out->set_duration(duration);
    Out->set_period(period);
    Out->set_duration_policy(duration_policy);
    Out->set_x(x);
    Out->set_y(y);
    Out->set_z(z);
    Out->set_m(m);
    Out->set_n(n);    
}

void FPbAbilityEffectDefData::Reset()
{
    type = int32();
    duration = float();
    period = float();
    duration_policy = int32();
    x = float();
    y = float();
    z = float();
    m = float();
    n = float();    
}

void FPbAbilityEffectDefData::operator=(const idlepb::AbilityEffectDefData& Right)
{
    this->FromPb(Right);
}

bool FPbAbilityEffectDefData::operator==(const FPbAbilityEffectDefData& Right) const
{
    if (this->type != Right.type)
        return false;
    if (this->duration != Right.duration)
        return false;
    if (this->period != Right.period)
        return false;
    if (this->duration_policy != Right.duration_policy)
        return false;
    if (this->x != Right.x)
        return false;
    if (this->y != Right.y)
        return false;
    if (this->z != Right.z)
        return false;
    if (this->m != Right.m)
        return false;
    if (this->n != Right.n)
        return false;
    return true;
}

bool FPbAbilityEffectDefData::operator!=(const FPbAbilityEffectDefData& Right) const
{
    return !operator==(Right);
}

FPbAbilityData::FPbAbilityData()
{
    Reset();        
}

FPbAbilityData::FPbAbilityData(const idlepb::AbilityData& Right)
{
    this->FromPb(Right);
}

void FPbAbilityData::FromPb(const idlepb::AbilityData& Right)
{
    id = Right.id();
    grade = Right.grade();
    activetime_utc = Right.activetime_utc();
    activetime_world = Right.activetime_world();
    unique_id = Right.unique_id();
    study_grade = Right.study_grade();
    cooldown = Right.cooldown();
    target_num = Right.target_num();
    target_distance = Right.target_distance();
    target_catchdistance = Right.target_catchdistance();
    attack_count = Right.attack_count();
    phy_coefficient = Right.phy_coefficient();
    phy_damage = Right.phy_damage();
    mana_coefficient = Right.mana_coefficient();
    mana_damage = Right.mana_damage();
    item_id = Right.item_id();
    item_maxdamage = Right.item_maxdamage();
    item_cfgid = Right.item_cfgid();
    effect_defs.Empty();
    for (const auto& Elem : Right.effect_defs())
    {
        effect_defs.Emplace(Elem);
    }
}

void FPbAbilityData::ToPb(idlepb::AbilityData* Out) const
{
    Out->set_id(id);
    Out->set_grade(grade);
    Out->set_activetime_utc(activetime_utc);
    Out->set_activetime_world(activetime_world);
    Out->set_unique_id(unique_id);
    Out->set_study_grade(study_grade);
    Out->set_cooldown(cooldown);
    Out->set_target_num(target_num);
    Out->set_target_distance(target_distance);
    Out->set_target_catchdistance(target_catchdistance);
    Out->set_attack_count(attack_count);
    Out->set_phy_coefficient(phy_coefficient);
    Out->set_phy_damage(phy_damage);
    Out->set_mana_coefficient(mana_coefficient);
    Out->set_mana_damage(mana_damage);
    Out->set_item_id(item_id);
    Out->set_item_maxdamage(item_maxdamage);
    Out->set_item_cfgid(item_cfgid);
    for (const auto& Elem : effect_defs)
    {
        Elem.ToPb(Out->add_effect_defs());    
    }    
}

void FPbAbilityData::Reset()
{
    id = int32();
    grade = int32();
    activetime_utc = int64();
    activetime_world = float();
    unique_id = int32();
    study_grade = int32();
    cooldown = float();
    target_num = float();
    target_distance = float();
    target_catchdistance = float();
    attack_count = float();
    phy_coefficient = float();
    phy_damage = float();
    mana_coefficient = float();
    mana_damage = float();
    item_id = int64();
    item_maxdamage = float();
    item_cfgid = int32();
    effect_defs = TArray<FPbAbilityEffectDefData>();    
}

void FPbAbilityData::operator=(const idlepb::AbilityData& Right)
{
    this->FromPb(Right);
}

bool FPbAbilityData::operator==(const FPbAbilityData& Right) const
{
    if (this->id != Right.id)
        return false;
    if (this->grade != Right.grade)
        return false;
    if (this->activetime_utc != Right.activetime_utc)
        return false;
    if (this->activetime_world != Right.activetime_world)
        return false;
    if (this->unique_id != Right.unique_id)
        return false;
    if (this->study_grade != Right.study_grade)
        return false;
    if (this->cooldown != Right.cooldown)
        return false;
    if (this->target_num != Right.target_num)
        return false;
    if (this->target_distance != Right.target_distance)
        return false;
    if (this->target_catchdistance != Right.target_catchdistance)
        return false;
    if (this->attack_count != Right.attack_count)
        return false;
    if (this->phy_coefficient != Right.phy_coefficient)
        return false;
    if (this->phy_damage != Right.phy_damage)
        return false;
    if (this->mana_coefficient != Right.mana_coefficient)
        return false;
    if (this->mana_damage != Right.mana_damage)
        return false;
    if (this->item_id != Right.item_id)
        return false;
    if (this->item_maxdamage != Right.item_maxdamage)
        return false;
    if (this->item_cfgid != Right.item_cfgid)
        return false;
    if (this->effect_defs != Right.effect_defs)
        return false;
    return true;
}

bool FPbAbilityData::operator!=(const FPbAbilityData& Right) const
{
    return !operator==(Right);
}

FPbPlayerAbilityData::FPbPlayerAbilityData()
{
    Reset();        
}

FPbPlayerAbilityData::FPbPlayerAbilityData(const idlepb::PlayerAbilityData& Right)
{
    this->FromPb(Right);
}

void FPbPlayerAbilityData::FromPb(const idlepb::PlayerAbilityData& Right)
{
    abilities.Empty();
    for (const auto& Elem : Right.abilities())
    {
        abilities.Emplace(Elem);
    }
    slotted_abilites.Empty();
    for (const auto& Elem : Right.slotted_abilites())
    {
        slotted_abilites.Emplace(Elem);
    }
    active_queue.Empty();
    for (const auto& Elem : Right.active_queue())
    {
        active_queue.Emplace(Elem);
    }
    is_shiled_first = Right.is_shiled_first();
    revert_all_skill_cooldown = Right.revert_all_skill_cooldown();
}

void FPbPlayerAbilityData::ToPb(idlepb::PlayerAbilityData* Out) const
{
    for (const auto& Elem : abilities)
    {
        Elem.ToPb(Out->add_abilities());    
    }
    for (const auto& Elem : slotted_abilites)
    {
        Elem.ToPb(Out->add_slotted_abilites());    
    }
    for (const auto& Elem : active_queue)
    {
        Out->add_active_queue(Elem);    
    }
    Out->set_is_shiled_first(is_shiled_first);
    Out->set_revert_all_skill_cooldown(revert_all_skill_cooldown);    
}

void FPbPlayerAbilityData::Reset()
{
    abilities = TArray<FPbAbilityData>();
    slotted_abilites = TArray<FPbMapValueInt32>();
    active_queue = TArray<int32>();
    is_shiled_first = bool();
    revert_all_skill_cooldown = int64();    
}

void FPbPlayerAbilityData::operator=(const idlepb::PlayerAbilityData& Right)
{
    this->FromPb(Right);
}

bool FPbPlayerAbilityData::operator==(const FPbPlayerAbilityData& Right) const
{
    if (this->abilities != Right.abilities)
        return false;
    if (this->slotted_abilites != Right.slotted_abilites)
        return false;
    if (this->active_queue != Right.active_queue)
        return false;
    if (this->is_shiled_first != Right.is_shiled_first)
        return false;
    if (this->revert_all_skill_cooldown != Right.revert_all_skill_cooldown)
        return false;
    return true;
}

bool FPbPlayerAbilityData::operator!=(const FPbPlayerAbilityData& Right) const
{
    return !operator==(Right);
}

FPbRoleZasData::FPbRoleZasData()
{
    Reset();        
}

FPbRoleZasData::FPbRoleZasData(const idlepb::RoleZasData& Right)
{
    this->FromPb(Right);
}

void FPbRoleZasData::FromPb(const idlepb::RoleZasData& Right)
{
    zas_version = Right.zas_version();
    zas_ability = Right.zas_ability();
    shentong_upgrade_point_use_num = Right.shentong_upgrade_point_use_num();
}

void FPbRoleZasData::ToPb(idlepb::RoleZasData* Out) const
{
    Out->set_zas_version(zas_version);
    zas_ability.ToPb(Out->mutable_zas_ability());
    Out->set_shentong_upgrade_point_use_num(shentong_upgrade_point_use_num);    
}

void FPbRoleZasData::Reset()
{
    zas_version = int32();
    zas_ability = FPbPlayerAbilityData();
    shentong_upgrade_point_use_num = int32();    
}

void FPbRoleZasData::operator=(const idlepb::RoleZasData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleZasData::operator==(const FPbRoleZasData& Right) const
{
    if (this->zas_version != Right.zas_version)
        return false;
    if (this->zas_ability != Right.zas_ability)
        return false;
    if (this->shentong_upgrade_point_use_num != Right.shentong_upgrade_point_use_num)
        return false;
    return true;
}

bool FPbRoleZasData::operator!=(const FPbRoleZasData& Right) const
{
    return !operator==(Right);
}

FPbAbilityPKResult::FPbAbilityPKResult()
{
    Reset();        
}

FPbAbilityPKResult::FPbAbilityPKResult(const idlepb::AbilityPKResult& Right)
{
    this->FromPb(Right);
}

void FPbAbilityPKResult::FromPb(const idlepb::AbilityPKResult& Right)
{
    instigator = Right.instigator();
    target = Right.target();
    damage = Right.damage();
    additional_damage = Right.additional_damage();
    is_hit = Right.is_hit();
    is_critical = Right.is_critical();
    is_countered = Right.is_countered();
    is_extremedamage = Right.is_extremedamage();
    countereddamage = Right.countereddamage();
    currentattackcount = Right.currentattackcount();
    shield_suckdamage = Right.shield_suckdamage();
    is_countered_critical = Right.is_countered_critical();
    countered_shield_suckdamage = Right.countered_shield_suckdamage();
}

void FPbAbilityPKResult::ToPb(idlepb::AbilityPKResult* Out) const
{
    Out->set_instigator(instigator);
    Out->set_target(target);
    Out->set_damage(damage);
    Out->set_additional_damage(additional_damage);
    Out->set_is_hit(is_hit);
    Out->set_is_critical(is_critical);
    Out->set_is_countered(is_countered);
    Out->set_is_extremedamage(is_extremedamage);
    Out->set_countereddamage(countereddamage);
    Out->set_currentattackcount(currentattackcount);
    Out->set_shield_suckdamage(shield_suckdamage);
    Out->set_is_countered_critical(is_countered_critical);
    Out->set_countered_shield_suckdamage(countered_shield_suckdamage);    
}

void FPbAbilityPKResult::Reset()
{
    instigator = int64();
    target = int64();
    damage = float();
    additional_damage = float();
    is_hit = bool();
    is_critical = bool();
    is_countered = bool();
    is_extremedamage = bool();
    countereddamage = float();
    currentattackcount = int32();
    shield_suckdamage = float();
    is_countered_critical = bool();
    countered_shield_suckdamage = float();    
}

void FPbAbilityPKResult::operator=(const idlepb::AbilityPKResult& Right)
{
    this->FromPb(Right);
}

bool FPbAbilityPKResult::operator==(const FPbAbilityPKResult& Right) const
{
    if (this->instigator != Right.instigator)
        return false;
    if (this->target != Right.target)
        return false;
    if (this->damage != Right.damage)
        return false;
    if (this->additional_damage != Right.additional_damage)
        return false;
    if (this->is_hit != Right.is_hit)
        return false;
    if (this->is_critical != Right.is_critical)
        return false;
    if (this->is_countered != Right.is_countered)
        return false;
    if (this->is_extremedamage != Right.is_extremedamage)
        return false;
    if (this->countereddamage != Right.countereddamage)
        return false;
    if (this->currentattackcount != Right.currentattackcount)
        return false;
    if (this->shield_suckdamage != Right.shield_suckdamage)
        return false;
    if (this->is_countered_critical != Right.is_countered_critical)
        return false;
    if (this->countered_shield_suckdamage != Right.countered_shield_suckdamage)
        return false;
    return true;
}

bool FPbAbilityPKResult::operator!=(const FPbAbilityPKResult& Right) const
{
    return !operator==(Right);
}

bool CheckEPbAbilityActiveErrorCodeValid(int32 Val)
{
    return idlepb::AbilityActiveErrorCode_IsValid(Val);
}

const TCHAR* GetEPbAbilityActiveErrorCodeDescription(EPbAbilityActiveErrorCode Val)
{
    switch (Val)
    {
        case EPbAbilityActiveErrorCode::AbilityActiveErrorCode_Success: return TEXT("正常");
        case EPbAbilityActiveErrorCode::AbilityActiveErrorCode_Timeout: return TEXT("超时");
        case EPbAbilityActiveErrorCode::AbilityActiveErrorCode_InvalidAbility: return TEXT("无效技能");
        case EPbAbilityActiveErrorCode::AbilityActiveErrorCode_Cooldown: return TEXT("CD不满足");
        case EPbAbilityActiveErrorCode::AbilityActiveErrorCode_CostNotEnough: return TEXT("消耗不够");
        case EPbAbilityActiveErrorCode::AbilityActiveErrorCode_Silent: return TEXT("沉默状态");
        case EPbAbilityActiveErrorCode::AbilityActiveErrorCode_Freezing: return TEXT("冰冻状态");
        case EPbAbilityActiveErrorCode::AbilityActiveErrorCode_Death: return TEXT("死亡状态");
        case EPbAbilityActiveErrorCode::AbilityActiveErrorCode_OwnerCheck: return TEXT("Owner非法");
        case EPbAbilityActiveErrorCode::AbilityActiveErrorCode_CommonCooldown: return TEXT("公共CD不满足");
    }
    return TEXT("UNKNOWN");
}

FPbAbilityActiveResult::FPbAbilityActiveResult()
{
    Reset();        
}

FPbAbilityActiveResult::FPbAbilityActiveResult(const idlepb::AbilityActiveResult& Right)
{
    this->FromPb(Right);
}

void FPbAbilityActiveResult::FromPb(const idlepb::AbilityActiveResult& Right)
{
    eid = Right.eid();
    ability = Right.ability();
    ability_unique_id = Right.ability_unique_id();
    error = static_cast<EPbAbilityActiveErrorCode>(Right.error());
    results.Empty();
    for (const auto& Elem : Right.results())
    {
        results.Emplace(Elem);
    }
    effects.Empty();
    for (const auto& Elem : Right.effects())
    {
        effects.Emplace(Elem);
    }
}

void FPbAbilityActiveResult::ToPb(idlepb::AbilityActiveResult* Out) const
{
    Out->set_eid(eid);
    Out->set_ability(ability);
    Out->set_ability_unique_id(ability_unique_id);
    Out->set_error(static_cast<idlepb::AbilityActiveErrorCode>(error));
    for (const auto& Elem : results)
    {
        Elem.ToPb(Out->add_results());    
    }
    for (const auto& Elem : effects)
    {
        Out->add_effects(Elem);    
    }    
}

void FPbAbilityActiveResult::Reset()
{
    eid = int64();
    ability = int32();
    ability_unique_id = int32();
    error = EPbAbilityActiveErrorCode();
    results = TArray<FPbAbilityPKResult>();
    effects = TArray<int32>();    
}

void FPbAbilityActiveResult::operator=(const idlepb::AbilityActiveResult& Right)
{
    this->FromPb(Right);
}

bool FPbAbilityActiveResult::operator==(const FPbAbilityActiveResult& Right) const
{
    if (this->eid != Right.eid)
        return false;
    if (this->ability != Right.ability)
        return false;
    if (this->ability_unique_id != Right.ability_unique_id)
        return false;
    if (this->error != Right.error)
        return false;
    if (this->results != Right.results)
        return false;
    if (this->effects != Right.effects)
        return false;
    return true;
}

bool FPbAbilityActiveResult::operator!=(const FPbAbilityActiveResult& Right) const
{
    return !operator==(Right);
}

FPbShanhetuItem::FPbShanhetuItem()
{
    Reset();        
}

FPbShanhetuItem::FPbShanhetuItem(const idlepb::ShanhetuItem& Right)
{
    this->FromPb(Right);
}

void FPbShanhetuItem::FromPb(const idlepb::ShanhetuItem& Right)
{
    item_id = Right.item_id();
    num = Right.num();
    weight = Right.weight();
    score = Right.score();
}

void FPbShanhetuItem::ToPb(idlepb::ShanhetuItem* Out) const
{
    Out->set_item_id(item_id);
    Out->set_num(num);
    Out->set_weight(weight);
    Out->set_score(score);    
}

void FPbShanhetuItem::Reset()
{
    item_id = int32();
    num = int32();
    weight = int32();
    score = int32();    
}

void FPbShanhetuItem::operator=(const idlepb::ShanhetuItem& Right)
{
    this->FromPb(Right);
}

bool FPbShanhetuItem::operator==(const FPbShanhetuItem& Right) const
{
    if (this->item_id != Right.item_id)
        return false;
    if (this->num != Right.num)
        return false;
    if (this->weight != Right.weight)
        return false;
    if (this->score != Right.score)
        return false;
    return true;
}

bool FPbShanhetuItem::operator!=(const FPbShanhetuItem& Right) const
{
    return !operator==(Right);
}

FPbShanhetuRecord::FPbShanhetuRecord()
{
    Reset();        
}

FPbShanhetuRecord::FPbShanhetuRecord(const idlepb::ShanhetuRecord& Right)
{
    this->FromPb(Right);
}

void FPbShanhetuRecord::FromPb(const idlepb::ShanhetuRecord& Right)
{
    uid = Right.uid();
    item_id = Right.item_id();
    scale_id = Right.scale_id();
    score = Right.score();
    items.Empty();
    for (const auto& Elem : Right.items())
    {
        items.Emplace(Elem);
    }
    use_time = Right.use_time();
}

void FPbShanhetuRecord::ToPb(idlepb::ShanhetuRecord* Out) const
{
    Out->set_uid(uid);
    Out->set_item_id(item_id);
    Out->set_scale_id(scale_id);
    Out->set_score(score);
    for (const auto& Elem : items)
    {
        Elem.ToPb(Out->add_items());    
    }
    Out->set_use_time(use_time);    
}

void FPbShanhetuRecord::Reset()
{
    uid = int64();
    item_id = int32();
    scale_id = int32();
    score = int32();
    items = TArray<FPbShanhetuItem>();
    use_time = int64();    
}

void FPbShanhetuRecord::operator=(const idlepb::ShanhetuRecord& Right)
{
    this->FromPb(Right);
}

bool FPbShanhetuRecord::operator==(const FPbShanhetuRecord& Right) const
{
    if (this->uid != Right.uid)
        return false;
    if (this->item_id != Right.item_id)
        return false;
    if (this->scale_id != Right.scale_id)
        return false;
    if (this->score != Right.score)
        return false;
    if (this->items != Right.items)
        return false;
    if (this->use_time != Right.use_time)
        return false;
    return true;
}

bool FPbShanhetuRecord::operator!=(const FPbShanhetuRecord& Right) const
{
    return !operator==(Right);
}

FPbShanhetuBlock::FPbShanhetuBlock()
{
    Reset();        
}

FPbShanhetuBlock::FPbShanhetuBlock(const idlepb::ShanhetuBlock& Right)
{
    this->FromPb(Right);
}

void FPbShanhetuBlock::FromPb(const idlepb::ShanhetuBlock& Right)
{
    type = Right.type();
    quality = Right.quality();
    item = Right.item();
    event_cfg_id = Right.event_cfg_id();
}

void FPbShanhetuBlock::ToPb(idlepb::ShanhetuBlock* Out) const
{
    Out->set_type(type);
    Out->set_quality(quality);
    item.ToPb(Out->mutable_item());
    Out->set_event_cfg_id(event_cfg_id);    
}

void FPbShanhetuBlock::Reset()
{
    type = int32();
    quality = int32();
    item = FPbShanhetuItem();
    event_cfg_id = int32();    
}

void FPbShanhetuBlock::operator=(const idlepb::ShanhetuBlock& Right)
{
    this->FromPb(Right);
}

bool FPbShanhetuBlock::operator==(const FPbShanhetuBlock& Right) const
{
    if (this->type != Right.type)
        return false;
    if (this->quality != Right.quality)
        return false;
    if (this->item != Right.item)
        return false;
    if (this->event_cfg_id != Right.event_cfg_id)
        return false;
    return true;
}

bool FPbShanhetuBlock::operator!=(const FPbShanhetuBlock& Right) const
{
    return !operator==(Right);
}

FPbShanhetuBlockRow::FPbShanhetuBlockRow()
{
    Reset();        
}

FPbShanhetuBlockRow::FPbShanhetuBlockRow(const idlepb::ShanhetuBlockRow& Right)
{
    this->FromPb(Right);
}

void FPbShanhetuBlockRow::FromPb(const idlepb::ShanhetuBlockRow& Right)
{
    blocks.Empty();
    for (const auto& Elem : Right.blocks())
    {
        blocks.Emplace(Elem);
    }
}

void FPbShanhetuBlockRow::ToPb(idlepb::ShanhetuBlockRow* Out) const
{
    for (const auto& Elem : blocks)
    {
        Elem.ToPb(Out->add_blocks());    
    }    
}

void FPbShanhetuBlockRow::Reset()
{
    blocks = TArray<FPbShanhetuBlock>();    
}

void FPbShanhetuBlockRow::operator=(const idlepb::ShanhetuBlockRow& Right)
{
    this->FromPb(Right);
}

bool FPbShanhetuBlockRow::operator==(const FPbShanhetuBlockRow& Right) const
{
    if (this->blocks != Right.blocks)
        return false;
    return true;
}

bool FPbShanhetuBlockRow::operator!=(const FPbShanhetuBlockRow& Right) const
{
    return !operator==(Right);
}

FPbShanhetuMap::FPbShanhetuMap()
{
    Reset();        
}

FPbShanhetuMap::FPbShanhetuMap(const idlepb::ShanhetuMap& Right)
{
    this->FromPb(Right);
}

void FPbShanhetuMap::FromPb(const idlepb::ShanhetuMap& Right)
{
    done = Right.done();
    current_row = Right.current_row();
    record = Right.record();
    map.Empty();
    for (const auto& Elem : Right.map())
    {
        map.Emplace(Elem);
    }
}

void FPbShanhetuMap::ToPb(idlepb::ShanhetuMap* Out) const
{
    Out->set_done(done);
    Out->set_current_row(current_row);
    record.ToPb(Out->mutable_record());
    for (const auto& Elem : map)
    {
        Elem.ToPb(Out->add_map());    
    }    
}

void FPbShanhetuMap::Reset()
{
    done = bool();
    current_row = int32();
    record = FPbShanhetuRecord();
    map = TArray<FPbShanhetuBlockRow>();    
}

void FPbShanhetuMap::operator=(const idlepb::ShanhetuMap& Right)
{
    this->FromPb(Right);
}

bool FPbShanhetuMap::operator==(const FPbShanhetuMap& Right) const
{
    if (this->done != Right.done)
        return false;
    if (this->current_row != Right.current_row)
        return false;
    if (this->record != Right.record)
        return false;
    if (this->map != Right.map)
        return false;
    return true;
}

bool FPbShanhetuMap::operator!=(const FPbShanhetuMap& Right) const
{
    return !operator==(Right);
}

FPbRoleShanhetuData::FPbRoleShanhetuData()
{
    Reset();        
}

FPbRoleShanhetuData::FPbRoleShanhetuData(const idlepb::RoleShanhetuData& Right)
{
    this->FromPb(Right);
}

void FPbRoleShanhetuData::FromPb(const idlepb::RoleShanhetuData& Right)
{
    auto_skip_green = Right.auto_skip_green();
    auto_skip_blue = Right.auto_skip_blue();
    auto_skip_perpo = Right.auto_skip_perpo();
    auto_skip_gold = Right.auto_skip_gold();
    auto_skip_red = Right.auto_skip_red();
    auto_select = Right.auto_select();
    current_map = Right.current_map();
    total_num = Right.total_num();
    records.Empty();
    for (const auto& Elem : Right.records())
    {
        records.Emplace(Elem);
    }
    last_update_date = Right.last_update_date();
}

void FPbRoleShanhetuData::ToPb(idlepb::RoleShanhetuData* Out) const
{
    Out->set_auto_skip_green(auto_skip_green);
    Out->set_auto_skip_blue(auto_skip_blue);
    Out->set_auto_skip_perpo(auto_skip_perpo);
    Out->set_auto_skip_gold(auto_skip_gold);
    Out->set_auto_skip_red(auto_skip_red);
    Out->set_auto_select(auto_select);
    current_map.ToPb(Out->mutable_current_map());
    Out->set_total_num(total_num);
    for (const auto& Elem : records)
    {
        Elem.ToPb(Out->add_records());    
    }
    Out->set_last_update_date(last_update_date);    
}

void FPbRoleShanhetuData::Reset()
{
    auto_skip_green = bool();
    auto_skip_blue = bool();
    auto_skip_perpo = bool();
    auto_skip_gold = bool();
    auto_skip_red = bool();
    auto_select = int32();
    current_map = FPbShanhetuMap();
    total_num = int64();
    records = TArray<FPbShanhetuRecord>();
    last_update_date = int64();    
}

void FPbRoleShanhetuData::operator=(const idlepb::RoleShanhetuData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleShanhetuData::operator==(const FPbRoleShanhetuData& Right) const
{
    if (this->auto_skip_green != Right.auto_skip_green)
        return false;
    if (this->auto_skip_blue != Right.auto_skip_blue)
        return false;
    if (this->auto_skip_perpo != Right.auto_skip_perpo)
        return false;
    if (this->auto_skip_gold != Right.auto_skip_gold)
        return false;
    if (this->auto_skip_red != Right.auto_skip_red)
        return false;
    if (this->auto_select != Right.auto_select)
        return false;
    if (this->current_map != Right.current_map)
        return false;
    if (this->total_num != Right.total_num)
        return false;
    if (this->records != Right.records)
        return false;
    if (this->last_update_date != Right.last_update_date)
        return false;
    return true;
}

bool FPbRoleShanhetuData::operator!=(const FPbRoleShanhetuData& Right) const
{
    return !operator==(Right);
}

FPbRoleLeaderboardData::FPbRoleLeaderboardData()
{
    Reset();        
}

FPbRoleLeaderboardData::FPbRoleLeaderboardData(const idlepb::RoleLeaderboardData& Right)
{
    this->FromPb(Right);
}

void FPbRoleLeaderboardData::FromPb(const idlepb::RoleLeaderboardData& Right)
{
    role_id = Right.role_id();
    blike_num = Right.blike_num();
    last_reset_time = Right.last_reset_time();
    rank_message = UTF8_TO_TCHAR(Right.rank_message().c_str());
    weapon = Right.weapon();
    ammor = Right.ammor();
    jewlery = Right.jewlery();
    skill_weapon = Right.skill_weapon();
    shanhetu_history = Right.shanhetu_history();
    shanhetu_week = Right.shanhetu_week();
    monster_tower_blike_num = Right.monster_tower_blike_num();
    has_received_challange_reward.Empty();
    for (const auto& Elem : Right.has_received_challange_reward())
    {
        has_received_challange_reward.Emplace(Elem);
    }
    fuze_rank = Right.fuze_rank();
    fuze_days = Right.fuze_days();
    fuze_exp = Right.fuze_exp();
    fuze_leaderboard_rank = Right.fuze_leaderboard_rank();
}

void FPbRoleLeaderboardData::ToPb(idlepb::RoleLeaderboardData* Out) const
{
    Out->set_role_id(role_id);
    Out->set_blike_num(blike_num);
    Out->set_last_reset_time(last_reset_time);
    Out->set_rank_message(TCHAR_TO_UTF8(*rank_message));
    weapon.ToPb(Out->mutable_weapon());
    ammor.ToPb(Out->mutable_ammor());
    jewlery.ToPb(Out->mutable_jewlery());
    skill_weapon.ToPb(Out->mutable_skill_weapon());
    shanhetu_history.ToPb(Out->mutable_shanhetu_history());
    shanhetu_week.ToPb(Out->mutable_shanhetu_week());
    Out->set_monster_tower_blike_num(monster_tower_blike_num);
    for (const auto& Elem : has_received_challange_reward)
    {
        Out->add_has_received_challange_reward(Elem);    
    }
    Out->set_fuze_rank(fuze_rank);
    Out->set_fuze_days(fuze_days);
    Out->set_fuze_exp(fuze_exp);
    Out->set_fuze_leaderboard_rank(fuze_leaderboard_rank);    
}

void FPbRoleLeaderboardData::Reset()
{
    role_id = int64();
    blike_num = int32();
    last_reset_time = int64();
    rank_message = FString();
    weapon = FPbItemData();
    ammor = FPbItemData();
    jewlery = FPbItemData();
    skill_weapon = FPbItemData();
    shanhetu_history = FPbShanhetuRecord();
    shanhetu_week = FPbShanhetuRecord();
    monster_tower_blike_num = int32();
    has_received_challange_reward = TArray<int32>();
    fuze_rank = int32();
    fuze_days = int32();
    fuze_exp = int64();
    fuze_leaderboard_rank = int32();    
}

void FPbRoleLeaderboardData::operator=(const idlepb::RoleLeaderboardData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleLeaderboardData::operator==(const FPbRoleLeaderboardData& Right) const
{
    if (this->role_id != Right.role_id)
        return false;
    if (this->blike_num != Right.blike_num)
        return false;
    if (this->last_reset_time != Right.last_reset_time)
        return false;
    if (this->rank_message != Right.rank_message)
        return false;
    if (this->weapon != Right.weapon)
        return false;
    if (this->ammor != Right.ammor)
        return false;
    if (this->jewlery != Right.jewlery)
        return false;
    if (this->skill_weapon != Right.skill_weapon)
        return false;
    if (this->shanhetu_history != Right.shanhetu_history)
        return false;
    if (this->shanhetu_week != Right.shanhetu_week)
        return false;
    if (this->monster_tower_blike_num != Right.monster_tower_blike_num)
        return false;
    if (this->has_received_challange_reward != Right.has_received_challange_reward)
        return false;
    if (this->fuze_rank != Right.fuze_rank)
        return false;
    if (this->fuze_days != Right.fuze_days)
        return false;
    if (this->fuze_exp != Right.fuze_exp)
        return false;
    if (this->fuze_leaderboard_rank != Right.fuze_leaderboard_rank)
        return false;
    return true;
}

bool FPbRoleLeaderboardData::operator!=(const FPbRoleLeaderboardData& Right) const
{
    return !operator==(Right);
}

FPbRoleMonsterTowerData::FPbRoleMonsterTowerData()
{
    Reset();        
}

FPbRoleMonsterTowerData::FPbRoleMonsterTowerData(const idlepb::RoleMonsterTowerData& Right)
{
    this->FromPb(Right);
}

void FPbRoleMonsterTowerData::FromPb(const idlepb::RoleMonsterTowerData& Right)
{
    last_floor = Right.last_floor();
    idle_during_ticks = Right.idle_during_ticks();
}

void FPbRoleMonsterTowerData::ToPb(idlepb::RoleMonsterTowerData* Out) const
{
    Out->set_last_floor(last_floor);
    Out->set_idle_during_ticks(idle_during_ticks);    
}

void FPbRoleMonsterTowerData::Reset()
{
    last_floor = int32();
    idle_during_ticks = int64();    
}

void FPbRoleMonsterTowerData::operator=(const idlepb::RoleMonsterTowerData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleMonsterTowerData::operator==(const FPbRoleMonsterTowerData& Right) const
{
    if (this->last_floor != Right.last_floor)
        return false;
    if (this->idle_during_ticks != Right.idle_during_ticks)
        return false;
    return true;
}

bool FPbRoleMonsterTowerData::operator!=(const FPbRoleMonsterTowerData& Right) const
{
    return !operator==(Right);
}

FPbRoleDungeonKillAllData::FPbRoleDungeonKillAllData()
{
    Reset();        
}

FPbRoleDungeonKillAllData::FPbRoleDungeonKillAllData(const idlepb::RoleDungeonKillAllData& Right)
{
    this->FromPb(Right);
}

void FPbRoleDungeonKillAllData::FromPb(const idlepb::RoleDungeonKillAllData& Right)
{
    done_uid.Empty();
    for (const auto& Elem : Right.done_uid())
    {
        done_uid.Emplace(Elem);
    }
}

void FPbRoleDungeonKillAllData::ToPb(idlepb::RoleDungeonKillAllData* Out) const
{
    for (const auto& Elem : done_uid)
    {
        Out->add_done_uid(Elem);    
    }    
}

void FPbRoleDungeonKillAllData::Reset()
{
    done_uid = TArray<int32>();    
}

void FPbRoleDungeonKillAllData::operator=(const idlepb::RoleDungeonKillAllData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleDungeonKillAllData::operator==(const FPbRoleDungeonKillAllData& Right) const
{
    if (this->done_uid != Right.done_uid)
        return false;
    return true;
}

bool FPbRoleDungeonKillAllData::operator!=(const FPbRoleDungeonKillAllData& Right) const
{
    return !operator==(Right);
}

FPbRoleDungeonSurviveData::FPbRoleDungeonSurviveData()
{
    Reset();        
}

FPbRoleDungeonSurviveData::FPbRoleDungeonSurviveData(const idlepb::RoleDungeonSurviveData& Right)
{
    this->FromPb(Right);
}

void FPbRoleDungeonSurviveData::FromPb(const idlepb::RoleDungeonSurviveData& Right)
{
    done_uid.Empty();
    for (const auto& Elem : Right.done_uid())
    {
        done_uid.Emplace(Elem);
    }
}

void FPbRoleDungeonSurviveData::ToPb(idlepb::RoleDungeonSurviveData* Out) const
{
    for (const auto& Elem : done_uid)
    {
        Out->add_done_uid(Elem);    
    }    
}

void FPbRoleDungeonSurviveData::Reset()
{
    done_uid = TArray<int32>();    
}

void FPbRoleDungeonSurviveData::operator=(const idlepb::RoleDungeonSurviveData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleDungeonSurviveData::operator==(const FPbRoleDungeonSurviveData& Right) const
{
    if (this->done_uid != Right.done_uid)
        return false;
    return true;
}

bool FPbRoleDungeonSurviveData::operator!=(const FPbRoleDungeonSurviveData& Right) const
{
    return !operator==(Right);
}

FPbBossInvasionRewardEntry::FPbBossInvasionRewardEntry()
{
    Reset();        
}

FPbBossInvasionRewardEntry::FPbBossInvasionRewardEntry(const idlepb::BossInvasionRewardEntry& Right)
{
    this->FromPb(Right);
}

void FPbBossInvasionRewardEntry::FromPb(const idlepb::BossInvasionRewardEntry& Right)
{
    unique_id = Right.unique_id();
    arena_cfg_id = Right.arena_cfg_id();
    rank = Right.rank();
}

void FPbBossInvasionRewardEntry::ToPb(idlepb::BossInvasionRewardEntry* Out) const
{
    Out->set_unique_id(unique_id);
    Out->set_arena_cfg_id(arena_cfg_id);
    Out->set_rank(rank);    
}

void FPbBossInvasionRewardEntry::Reset()
{
    unique_id = int64();
    arena_cfg_id = int32();
    rank = int32();    
}

void FPbBossInvasionRewardEntry::operator=(const idlepb::BossInvasionRewardEntry& Right)
{
    this->FromPb(Right);
}

bool FPbBossInvasionRewardEntry::operator==(const FPbBossInvasionRewardEntry& Right) const
{
    if (this->unique_id != Right.unique_id)
        return false;
    if (this->arena_cfg_id != Right.arena_cfg_id)
        return false;
    if (this->rank != Right.rank)
        return false;
    return true;
}

bool FPbBossInvasionRewardEntry::operator!=(const FPbBossInvasionRewardEntry& Right) const
{
    return !operator==(Right);
}

FPbBossInvasionKillRewardData::FPbBossInvasionKillRewardData()
{
    Reset();        
}

FPbBossInvasionKillRewardData::FPbBossInvasionKillRewardData(const idlepb::BossInvasionKillRewardData& Right)
{
    this->FromPb(Right);
}

void FPbBossInvasionKillRewardData::FromPb(const idlepb::BossInvasionKillRewardData& Right)
{
    start_ticks = Right.start_ticks();
    rewards.Empty();
    for (const auto& Elem : Right.rewards())
    {
        rewards.Emplace(Elem);
    }
    is_draw_done = Right.is_draw_done();
    drawed_unique_id = Right.drawed_unique_id();
}

void FPbBossInvasionKillRewardData::ToPb(idlepb::BossInvasionKillRewardData* Out) const
{
    Out->set_start_ticks(start_ticks);
    for (const auto& Elem : rewards)
    {
        Elem.ToPb(Out->add_rewards());    
    }
    Out->set_is_draw_done(is_draw_done);
    Out->set_drawed_unique_id(drawed_unique_id);    
}

void FPbBossInvasionKillRewardData::Reset()
{
    start_ticks = int64();
    rewards = TArray<FPbBossInvasionRewardEntry>();
    is_draw_done = bool();
    drawed_unique_id = int64();    
}

void FPbBossInvasionKillRewardData::operator=(const idlepb::BossInvasionKillRewardData& Right)
{
    this->FromPb(Right);
}

bool FPbBossInvasionKillRewardData::operator==(const FPbBossInvasionKillRewardData& Right) const
{
    if (this->start_ticks != Right.start_ticks)
        return false;
    if (this->rewards != Right.rewards)
        return false;
    if (this->is_draw_done != Right.is_draw_done)
        return false;
    if (this->drawed_unique_id != Right.drawed_unique_id)
        return false;
    return true;
}

bool FPbBossInvasionKillRewardData::operator!=(const FPbBossInvasionKillRewardData& Right) const
{
    return !operator==(Right);
}

FPbBossInvasionDamageRewardData::FPbBossInvasionDamageRewardData()
{
    Reset();        
}

FPbBossInvasionDamageRewardData::FPbBossInvasionDamageRewardData(const idlepb::BossInvasionDamageRewardData& Right)
{
    this->FromPb(Right);
}

void FPbBossInvasionDamageRewardData::FromPb(const idlepb::BossInvasionDamageRewardData& Right)
{
    start_ticks = Right.start_ticks();
    rewards.Empty();
    for (const auto& Elem : Right.rewards())
    {
        rewards.Emplace(Elem);
    }
}

void FPbBossInvasionDamageRewardData::ToPb(idlepb::BossInvasionDamageRewardData* Out) const
{
    Out->set_start_ticks(start_ticks);
    for (const auto& Elem : rewards)
    {
        Elem.ToPb(Out->add_rewards());    
    }    
}

void FPbBossInvasionDamageRewardData::Reset()
{
    start_ticks = int64();
    rewards = TArray<FPbBossInvasionRewardEntry>();    
}

void FPbBossInvasionDamageRewardData::operator=(const idlepb::BossInvasionDamageRewardData& Right)
{
    this->FromPb(Right);
}

bool FPbBossInvasionDamageRewardData::operator==(const FPbBossInvasionDamageRewardData& Right) const
{
    if (this->start_ticks != Right.start_ticks)
        return false;
    if (this->rewards != Right.rewards)
        return false;
    return true;
}

bool FPbBossInvasionDamageRewardData::operator!=(const FPbBossInvasionDamageRewardData& Right) const
{
    return !operator==(Right);
}

FPbRoleBossInvasionData::FPbRoleBossInvasionData()
{
    Reset();        
}

FPbRoleBossInvasionData::FPbRoleBossInvasionData(const idlepb::RoleBossInvasionData& Right)
{
    this->FromPb(Right);
}

void FPbRoleBossInvasionData::FromPb(const idlepb::RoleBossInvasionData& Right)
{
    last_reset_ticks = Right.last_reset_ticks();
    kill_reward = Right.kill_reward();
    damage_reward.Empty();
    for (const auto& Elem : Right.damage_reward())
    {
        damage_reward.Emplace(Elem);
    }
    drawed_unique_id = Right.drawed_unique_id();
}

void FPbRoleBossInvasionData::ToPb(idlepb::RoleBossInvasionData* Out) const
{
    Out->set_last_reset_ticks(last_reset_ticks);
    kill_reward.ToPb(Out->mutable_kill_reward());
    for (const auto& Elem : damage_reward)
    {
        Elem.ToPb(Out->add_damage_reward());    
    }
    Out->set_drawed_unique_id(drawed_unique_id);    
}

void FPbRoleBossInvasionData::Reset()
{
    last_reset_ticks = int64();
    kill_reward = FPbBossInvasionKillRewardData();
    damage_reward = TArray<FPbBossInvasionDamageRewardData>();
    drawed_unique_id = int64();    
}

void FPbRoleBossInvasionData::operator=(const idlepb::RoleBossInvasionData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleBossInvasionData::operator==(const FPbRoleBossInvasionData& Right) const
{
    if (this->last_reset_ticks != Right.last_reset_ticks)
        return false;
    if (this->kill_reward != Right.kill_reward)
        return false;
    if (this->damage_reward != Right.damage_reward)
        return false;
    if (this->drawed_unique_id != Right.drawed_unique_id)
        return false;
    return true;
}

bool FPbRoleBossInvasionData::operator!=(const FPbRoleBossInvasionData& Right) const
{
    return !operator==(Right);
}

FPbRoleMasiveData::FPbRoleMasiveData()
{
    Reset();        
}

FPbRoleMasiveData::FPbRoleMasiveData(const idlepb::RoleMasiveData& Right)
{
    this->FromPb(Right);
}

void FPbRoleMasiveData::FromPb(const idlepb::RoleMasiveData& Right)
{
    user_vars.Empty();
    for (const auto& Elem : Right.user_vars())
    {
        user_vars.Emplace(Elem);
    }
    next_self_unique_id = Right.next_self_unique_id();
}

void FPbRoleMasiveData::ToPb(idlepb::RoleMasiveData* Out) const
{
    for (const auto& Elem : user_vars)
    {
        Elem.ToPb(Out->add_user_vars());    
    }
    Out->set_next_self_unique_id(next_self_unique_id);    
}

void FPbRoleMasiveData::Reset()
{
    user_vars = TArray<FPbStringKeyInt32ValueEntry>();
    next_self_unique_id = int64();    
}

void FPbRoleMasiveData::operator=(const idlepb::RoleMasiveData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleMasiveData::operator==(const FPbRoleMasiveData& Right) const
{
    if (this->user_vars != Right.user_vars)
        return false;
    if (this->next_self_unique_id != Right.next_self_unique_id)
        return false;
    return true;
}

bool FPbRoleMasiveData::operator!=(const FPbRoleMasiveData& Right) const
{
    return !operator==(Right);
}

FPbCheckTask::FPbCheckTask()
{
    Reset();        
}

FPbCheckTask::FPbCheckTask(const idlepb::CheckTask& Right)
{
    this->FromPb(Right);
}

void FPbCheckTask::FromPb(const idlepb::CheckTask& Right)
{
    task_id = Right.task_id();
    type = Right.type();
    need_num = Right.need_num();
    point = Right.point();
    progress = Right.progress();
    is_submitted = Right.is_submitted();
}

void FPbCheckTask::ToPb(idlepb::CheckTask* Out) const
{
    Out->set_task_id(task_id);
    Out->set_type(type);
    Out->set_need_num(need_num);
    Out->set_point(point);
    Out->set_progress(progress);
    Out->set_is_submitted(is_submitted);    
}

void FPbCheckTask::Reset()
{
    task_id = int32();
    type = int32();
    need_num = int32();
    point = int32();
    progress = int32();
    is_submitted = bool();    
}

void FPbCheckTask::operator=(const idlepb::CheckTask& Right)
{
    this->FromPb(Right);
}

bool FPbCheckTask::operator==(const FPbCheckTask& Right) const
{
    if (this->task_id != Right.task_id)
        return false;
    if (this->type != Right.type)
        return false;
    if (this->need_num != Right.need_num)
        return false;
    if (this->point != Right.point)
        return false;
    if (this->progress != Right.progress)
        return false;
    if (this->is_submitted != Right.is_submitted)
        return false;
    return true;
}

bool FPbCheckTask::operator!=(const FPbCheckTask& Right) const
{
    return !operator==(Right);
}

FPbRoleChecklistData::FPbRoleChecklistData()
{
    Reset();        
}

FPbRoleChecklistData::FPbRoleChecklistData(const idlepb::RoleChecklistData& Right)
{
    this->FromPb(Right);
}

void FPbRoleChecklistData::FromPb(const idlepb::RoleChecklistData& Right)
{
    day_point = Right.day_point();
    week_point = Right.week_point();
    day_tasks.Empty();
    for (const auto& Elem : Right.day_tasks())
    {
        day_tasks.Emplace(Elem);
    }
    week_tasks.Empty();
    for (const auto& Elem : Right.week_tasks())
    {
        week_tasks.Emplace(Elem);
    }
    day_received_time = Right.day_received_time();
    week_received_time = Right.week_received_time();
    last_reset_day_time = Right.last_reset_day_time();
    last_reset_week_time = Right.last_reset_week_time();
    boss_invasion_time = Right.boss_invasion_time();
    degree_locked_day = Right.degree_locked_day();
    degree_locked_week = Right.degree_locked_week();
}

void FPbRoleChecklistData::ToPb(idlepb::RoleChecklistData* Out) const
{
    Out->set_day_point(day_point);
    Out->set_week_point(week_point);
    for (const auto& Elem : day_tasks)
    {
        Elem.ToPb(Out->add_day_tasks());    
    }
    for (const auto& Elem : week_tasks)
    {
        Elem.ToPb(Out->add_week_tasks());    
    }
    Out->set_day_received_time(day_received_time);
    Out->set_week_received_time(week_received_time);
    Out->set_last_reset_day_time(last_reset_day_time);
    Out->set_last_reset_week_time(last_reset_week_time);
    Out->set_boss_invasion_time(boss_invasion_time);
    Out->set_degree_locked_day(degree_locked_day);
    Out->set_degree_locked_week(degree_locked_week);    
}

void FPbRoleChecklistData::Reset()
{
    day_point = int32();
    week_point = int32();
    day_tasks = TArray<FPbCheckTask>();
    week_tasks = TArray<FPbCheckTask>();
    day_received_time = int32();
    week_received_time = int32();
    last_reset_day_time = int64();
    last_reset_week_time = int64();
    boss_invasion_time = int64();
    degree_locked_day = int32();
    degree_locked_week = int32();    
}

void FPbRoleChecklistData::operator=(const idlepb::RoleChecklistData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleChecklistData::operator==(const FPbRoleChecklistData& Right) const
{
    if (this->day_point != Right.day_point)
        return false;
    if (this->week_point != Right.week_point)
        return false;
    if (this->day_tasks != Right.day_tasks)
        return false;
    if (this->week_tasks != Right.week_tasks)
        return false;
    if (this->day_received_time != Right.day_received_time)
        return false;
    if (this->week_received_time != Right.week_received_time)
        return false;
    if (this->last_reset_day_time != Right.last_reset_day_time)
        return false;
    if (this->last_reset_week_time != Right.last_reset_week_time)
        return false;
    if (this->boss_invasion_time != Right.boss_invasion_time)
        return false;
    if (this->degree_locked_day != Right.degree_locked_day)
        return false;
    if (this->degree_locked_week != Right.degree_locked_week)
        return false;
    return true;
}

bool FPbRoleChecklistData::operator!=(const FPbRoleChecklistData& Right) const
{
    return !operator==(Right);
}

FPbRoleCommonItemExchangeData::FPbRoleCommonItemExchangeData()
{
    Reset();        
}

FPbRoleCommonItemExchangeData::FPbRoleCommonItemExchangeData(const idlepb::RoleCommonItemExchangeData& Right)
{
    this->FromPb(Right);
}

void FPbRoleCommonItemExchangeData::FromPb(const idlepb::RoleCommonItemExchangeData& Right)
{
    last_reset_day = Right.last_reset_day();
    last_reset_week = Right.last_reset_week();
    item_exchange_day.Empty();
    for (const auto& Elem : Right.item_exchange_day())
    {
        item_exchange_day.Emplace(Elem);
    }
    item_exchange_week.Empty();
    for (const auto& Elem : Right.item_exchange_week())
    {
        item_exchange_week.Emplace(Elem);
    }
}

void FPbRoleCommonItemExchangeData::ToPb(idlepb::RoleCommonItemExchangeData* Out) const
{
    Out->set_last_reset_day(last_reset_day);
    Out->set_last_reset_week(last_reset_week);
    for (const auto& Elem : item_exchange_day)
    {
        Elem.ToPb(Out->add_item_exchange_day());    
    }
    for (const auto& Elem : item_exchange_week)
    {
        Elem.ToPb(Out->add_item_exchange_week());    
    }    
}

void FPbRoleCommonItemExchangeData::Reset()
{
    last_reset_day = int64();
    last_reset_week = int64();
    item_exchange_day = TArray<FPbMapValueInt32>();
    item_exchange_week = TArray<FPbMapValueInt32>();    
}

void FPbRoleCommonItemExchangeData::operator=(const idlepb::RoleCommonItemExchangeData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleCommonItemExchangeData::operator==(const FPbRoleCommonItemExchangeData& Right) const
{
    if (this->last_reset_day != Right.last_reset_day)
        return false;
    if (this->last_reset_week != Right.last_reset_week)
        return false;
    if (this->item_exchange_day != Right.item_exchange_day)
        return false;
    if (this->item_exchange_week != Right.item_exchange_week)
        return false;
    return true;
}

bool FPbRoleCommonItemExchangeData::operator!=(const FPbRoleCommonItemExchangeData& Right) const
{
    return !operator==(Right);
}

FPbRoleTreasuryChestData::FPbRoleTreasuryChestData()
{
    Reset();        
}

FPbRoleTreasuryChestData::FPbRoleTreasuryChestData(const idlepb::RoleTreasuryChestData& Right)
{
    this->FromPb(Right);
}

void FPbRoleTreasuryChestData::FromPb(const idlepb::RoleTreasuryChestData& Right)
{
    today_open_times.Empty();
    for (const auto& Elem : Right.today_open_times())
    {
        today_open_times.Emplace(Elem);
    }
    guarantee_count.Empty();
    for (const auto& Elem : Right.guarantee_count())
    {
        guarantee_count.Emplace(Elem);
    }
}

void FPbRoleTreasuryChestData::ToPb(idlepb::RoleTreasuryChestData* Out) const
{
    for (const auto& Elem : today_open_times)
    {
        Out->add_today_open_times(Elem);    
    }
    for (const auto& Elem : guarantee_count)
    {
        Out->add_guarantee_count(Elem);    
    }    
}

void FPbRoleTreasuryChestData::Reset()
{
    today_open_times = TArray<int32>();
    guarantee_count = TArray<int32>();    
}

void FPbRoleTreasuryChestData::operator=(const idlepb::RoleTreasuryChestData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleTreasuryChestData::operator==(const FPbRoleTreasuryChestData& Right) const
{
    if (this->today_open_times != Right.today_open_times)
        return false;
    if (this->guarantee_count != Right.guarantee_count)
        return false;
    return true;
}

bool FPbRoleTreasuryChestData::operator!=(const FPbRoleTreasuryChestData& Right) const
{
    return !operator==(Right);
}

FPbRoleTreasuryGachaData::FPbRoleTreasuryGachaData()
{
    Reset();        
}

FPbRoleTreasuryGachaData::FPbRoleTreasuryGachaData(const idlepb::RoleTreasuryGachaData& Right)
{
    this->FromPb(Right);
}

void FPbRoleTreasuryGachaData::FromPb(const idlepb::RoleTreasuryGachaData& Right)
{
    today_open_times.Empty();
    for (const auto& Elem : Right.today_open_times())
    {
        today_open_times.Emplace(Elem);
    }
    free_open_times.Empty();
    for (const auto& Elem : Right.free_open_times())
    {
        free_open_times.Emplace(Elem);
    }
    guarantee_count.Empty();
    for (const auto& Elem : Right.guarantee_count())
    {
        guarantee_count.Emplace(Elem);
    }
    total_open_time = Right.total_open_time();
}

void FPbRoleTreasuryGachaData::ToPb(idlepb::RoleTreasuryGachaData* Out) const
{
    for (const auto& Elem : today_open_times)
    {
        Out->add_today_open_times(Elem);    
    }
    for (const auto& Elem : free_open_times)
    {
        Out->add_free_open_times(Elem);    
    }
    for (const auto& Elem : guarantee_count)
    {
        Out->add_guarantee_count(Elem);    
    }
    Out->set_total_open_time(total_open_time);    
}

void FPbRoleTreasuryGachaData::Reset()
{
    today_open_times = TArray<int32>();
    free_open_times = TArray<int32>();
    guarantee_count = TArray<int32>();
    total_open_time = int32();    
}

void FPbRoleTreasuryGachaData::operator=(const idlepb::RoleTreasuryGachaData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleTreasuryGachaData::operator==(const FPbRoleTreasuryGachaData& Right) const
{
    if (this->today_open_times != Right.today_open_times)
        return false;
    if (this->free_open_times != Right.free_open_times)
        return false;
    if (this->guarantee_count != Right.guarantee_count)
        return false;
    if (this->total_open_time != Right.total_open_time)
        return false;
    return true;
}

bool FPbRoleTreasuryGachaData::operator!=(const FPbRoleTreasuryGachaData& Right) const
{
    return !operator==(Right);
}

FPbTreasuryShopItem::FPbTreasuryShopItem()
{
    Reset();        
}

FPbTreasuryShopItem::FPbTreasuryShopItem(const idlepb::TreasuryShopItem& Right)
{
    this->FromPb(Right);
}

void FPbTreasuryShopItem::FromPb(const idlepb::TreasuryShopItem& Right)
{
    index = Right.index();
    item_id = Right.item_id();
    num = Right.num();
    price = Right.price();
    count = Right.count();
    bought_count = Right.bought_count();
    cfg_id = Right.cfg_id();
}

void FPbTreasuryShopItem::ToPb(idlepb::TreasuryShopItem* Out) const
{
    Out->set_index(index);
    Out->set_item_id(item_id);
    Out->set_num(num);
    Out->set_price(price);
    Out->set_count(count);
    Out->set_bought_count(bought_count);
    Out->set_cfg_id(cfg_id);    
}

void FPbTreasuryShopItem::Reset()
{
    index = int32();
    item_id = int32();
    num = int32();
    price = int32();
    count = int32();
    bought_count = int32();
    cfg_id = int32();    
}

void FPbTreasuryShopItem::operator=(const idlepb::TreasuryShopItem& Right)
{
    this->FromPb(Right);
}

bool FPbTreasuryShopItem::operator==(const FPbTreasuryShopItem& Right) const
{
    if (this->index != Right.index)
        return false;
    if (this->item_id != Right.item_id)
        return false;
    if (this->num != Right.num)
        return false;
    if (this->price != Right.price)
        return false;
    if (this->count != Right.count)
        return false;
    if (this->bought_count != Right.bought_count)
        return false;
    if (this->cfg_id != Right.cfg_id)
        return false;
    return true;
}

bool FPbTreasuryShopItem::operator!=(const FPbTreasuryShopItem& Right) const
{
    return !operator==(Right);
}

FPbRoleTreasuryShopData::FPbRoleTreasuryShopData()
{
    Reset();        
}

FPbRoleTreasuryShopData::FPbRoleTreasuryShopData(const idlepb::RoleTreasuryShopData& Right)
{
    this->FromPb(Right);
}

void FPbRoleTreasuryShopData::FromPb(const idlepb::RoleTreasuryShopData& Right)
{
    shop_items.Empty();
    for (const auto& Elem : Right.shop_items())
    {
        shop_items.Emplace(Elem);
    }
    today_refresh_time = Right.today_refresh_time();
    shop_refresh_flag = Right.shop_refresh_flag();
}

void FPbRoleTreasuryShopData::ToPb(idlepb::RoleTreasuryShopData* Out) const
{
    for (const auto& Elem : shop_items)
    {
        Elem.ToPb(Out->add_shop_items());    
    }
    Out->set_today_refresh_time(today_refresh_time);
    Out->set_shop_refresh_flag(shop_refresh_flag);    
}

void FPbRoleTreasuryShopData::Reset()
{
    shop_items = TArray<FPbTreasuryShopItem>();
    today_refresh_time = int32();
    shop_refresh_flag = bool();    
}

void FPbRoleTreasuryShopData::operator=(const idlepb::RoleTreasuryShopData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleTreasuryShopData::operator==(const FPbRoleTreasuryShopData& Right) const
{
    if (this->shop_items != Right.shop_items)
        return false;
    if (this->today_refresh_time != Right.today_refresh_time)
        return false;
    if (this->shop_refresh_flag != Right.shop_refresh_flag)
        return false;
    return true;
}

bool FPbRoleTreasuryShopData::operator!=(const FPbRoleTreasuryShopData& Right) const
{
    return !operator==(Right);
}

FPbRoleTreasurySaveData::FPbRoleTreasurySaveData()
{
    Reset();        
}

FPbRoleTreasurySaveData::FPbRoleTreasurySaveData(const idlepb::RoleTreasurySaveData& Right)
{
    this->FromPb(Right);
}

void FPbRoleTreasurySaveData::FromPb(const idlepb::RoleTreasurySaveData& Right)
{
    treasury_chest_data = Right.treasury_chest_data();
    treasury_gacha_data = Right.treasury_gacha_data();
    treasury_shop_data = Right.treasury_shop_data();
    last_reset_time = Right.last_reset_time();
}

void FPbRoleTreasurySaveData::ToPb(idlepb::RoleTreasurySaveData* Out) const
{
    treasury_chest_data.ToPb(Out->mutable_treasury_chest_data());
    treasury_gacha_data.ToPb(Out->mutable_treasury_gacha_data());
    treasury_shop_data.ToPb(Out->mutable_treasury_shop_data());
    Out->set_last_reset_time(last_reset_time);    
}

void FPbRoleTreasurySaveData::Reset()
{
    treasury_chest_data = FPbRoleTreasuryChestData();
    treasury_gacha_data = FPbRoleTreasuryGachaData();
    treasury_shop_data = FPbRoleTreasuryShopData();
    last_reset_time = int64();    
}

void FPbRoleTreasurySaveData::operator=(const idlepb::RoleTreasurySaveData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleTreasurySaveData::operator==(const FPbRoleTreasurySaveData& Right) const
{
    if (this->treasury_chest_data != Right.treasury_chest_data)
        return false;
    if (this->treasury_gacha_data != Right.treasury_gacha_data)
        return false;
    if (this->treasury_shop_data != Right.treasury_shop_data)
        return false;
    if (this->last_reset_time != Right.last_reset_time)
        return false;
    return true;
}

bool FPbRoleTreasurySaveData::operator!=(const FPbRoleTreasurySaveData& Right) const
{
    return !operator==(Right);
}

FPbArenaCheckListData::FPbArenaCheckListData()
{
    Reset();        
}

FPbArenaCheckListData::FPbArenaCheckListData(const idlepb::ArenaCheckListData& Right)
{
    this->FromPb(Right);
}

void FPbArenaCheckListData::FromPb(const idlepb::ArenaCheckListData& Right)
{
    checklist_id = Right.checklist_id();
    checklist_num = Right.checklist_num();
    checklist_state = static_cast<EPbArenaCheckListState>(Right.checklist_state());
}

void FPbArenaCheckListData::ToPb(idlepb::ArenaCheckListData* Out) const
{
    Out->set_checklist_id(checklist_id);
    Out->set_checklist_num(checklist_num);
    Out->set_checklist_state(static_cast<idlepb::ArenaCheckListState>(checklist_state));    
}

void FPbArenaCheckListData::Reset()
{
    checklist_id = int32();
    checklist_num = int32();
    checklist_state = EPbArenaCheckListState();    
}

void FPbArenaCheckListData::operator=(const idlepb::ArenaCheckListData& Right)
{
    this->FromPb(Right);
}

bool FPbArenaCheckListData::operator==(const FPbArenaCheckListData& Right) const
{
    if (this->checklist_id != Right.checklist_id)
        return false;
    if (this->checklist_num != Right.checklist_num)
        return false;
    if (this->checklist_state != Right.checklist_state)
        return false;
    return true;
}

bool FPbArenaCheckListData::operator!=(const FPbArenaCheckListData& Right) const
{
    return !operator==(Right);
}

FPbArenaCheckListRewardData::FPbArenaCheckListRewardData()
{
    Reset();        
}

FPbArenaCheckListRewardData::FPbArenaCheckListRewardData(const idlepb::ArenaCheckListRewardData& Right)
{
    this->FromPb(Right);
}

void FPbArenaCheckListRewardData::FromPb(const idlepb::ArenaCheckListRewardData& Right)
{
    reward_id = Right.reward_id();
    reward_state = static_cast<EPbArenaCheckListRewardState>(Right.reward_state());
}

void FPbArenaCheckListRewardData::ToPb(idlepb::ArenaCheckListRewardData* Out) const
{
    Out->set_reward_id(reward_id);
    Out->set_reward_state(static_cast<idlepb::ArenaCheckListRewardState>(reward_state));    
}

void FPbArenaCheckListRewardData::Reset()
{
    reward_id = int32();
    reward_state = EPbArenaCheckListRewardState();    
}

void FPbArenaCheckListRewardData::operator=(const idlepb::ArenaCheckListRewardData& Right)
{
    this->FromPb(Right);
}

bool FPbArenaCheckListRewardData::operator==(const FPbArenaCheckListRewardData& Right) const
{
    if (this->reward_id != Right.reward_id)
        return false;
    if (this->reward_state != Right.reward_state)
        return false;
    return true;
}

bool FPbArenaCheckListRewardData::operator!=(const FPbArenaCheckListRewardData& Right) const
{
    return !operator==(Right);
}

FPbRoleArenaCheckListData::FPbRoleArenaCheckListData()
{
    Reset();        
}

FPbRoleArenaCheckListData::FPbRoleArenaCheckListData(const idlepb::RoleArenaCheckListData& Right)
{
    this->FromPb(Right);
}

void FPbRoleArenaCheckListData::FromPb(const idlepb::RoleArenaCheckListData& Right)
{
    arena_check_data.Empty();
    for (const auto& Elem : Right.arena_check_data())
    {
        arena_check_data.Emplace(Elem);
    }
    check_reward_data.Empty();
    for (const auto& Elem : Right.check_reward_data())
    {
        check_reward_data.Emplace(Elem);
    }
}

void FPbRoleArenaCheckListData::ToPb(idlepb::RoleArenaCheckListData* Out) const
{
    for (const auto& Elem : arena_check_data)
    {
        Elem.ToPb(Out->add_arena_check_data());    
    }
    for (const auto& Elem : check_reward_data)
    {
        Elem.ToPb(Out->add_check_reward_data());    
    }    
}

void FPbRoleArenaCheckListData::Reset()
{
    arena_check_data = TArray<FPbArenaCheckListData>();
    check_reward_data = TArray<FPbArenaCheckListRewardData>();    
}

void FPbRoleArenaCheckListData::operator=(const idlepb::RoleArenaCheckListData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleArenaCheckListData::operator==(const FPbRoleArenaCheckListData& Right) const
{
    if (this->arena_check_data != Right.arena_check_data)
        return false;
    if (this->check_reward_data != Right.check_reward_data)
        return false;
    return true;
}

bool FPbRoleArenaCheckListData::operator!=(const FPbRoleArenaCheckListData& Right) const
{
    return !operator==(Right);
}

FPbRoleSeptInviteEntry::FPbRoleSeptInviteEntry()
{
    Reset();        
}

FPbRoleSeptInviteEntry::FPbRoleSeptInviteEntry(const idlepb::RoleSeptInviteEntry& Right)
{
    this->FromPb(Right);
}

void FPbRoleSeptInviteEntry::FromPb(const idlepb::RoleSeptInviteEntry& Right)
{
    position = static_cast<EPbSeptPosition>(Right.position());
    num = Right.num();
}

void FPbRoleSeptInviteEntry::ToPb(idlepb::RoleSeptInviteEntry* Out) const
{
    Out->set_position(static_cast<idlepb::SeptPosition>(position));
    Out->set_num(num);    
}

void FPbRoleSeptInviteEntry::Reset()
{
    position = EPbSeptPosition();
    num = int32();    
}

void FPbRoleSeptInviteEntry::operator=(const idlepb::RoleSeptInviteEntry& Right)
{
    this->FromPb(Right);
}

bool FPbRoleSeptInviteEntry::operator==(const FPbRoleSeptInviteEntry& Right) const
{
    if (this->position != Right.position)
        return false;
    if (this->num != Right.num)
        return false;
    return true;
}

bool FPbRoleSeptInviteEntry::operator!=(const FPbRoleSeptInviteEntry& Right) const
{
    return !operator==(Right);
}

FPbSeptQuest::FPbSeptQuest()
{
    Reset();        
}

FPbSeptQuest::FPbSeptQuest(const idlepb::SeptQuest& Right)
{
    this->FromPb(Right);
}

void FPbSeptQuest::FromPb(const idlepb::SeptQuest& Right)
{
    uid = Right.uid();
    quest_id = Right.quest_id();
    begin_time = Right.begin_time();
    received = Right.received();
    level = Right.level();
    money_num = Right.money_num();
}

void FPbSeptQuest::ToPb(idlepb::SeptQuest* Out) const
{
    Out->set_uid(uid);
    Out->set_quest_id(quest_id);
    Out->set_begin_time(begin_time);
    Out->set_received(received);
    Out->set_level(level);
    Out->set_money_num(money_num);    
}

void FPbSeptQuest::Reset()
{
    uid = int32();
    quest_id = int32();
    begin_time = int64();
    received = bool();
    level = int32();
    money_num = int32();    
}

void FPbSeptQuest::operator=(const idlepb::SeptQuest& Right)
{
    this->FromPb(Right);
}

bool FPbSeptQuest::operator==(const FPbSeptQuest& Right) const
{
    if (this->uid != Right.uid)
        return false;
    if (this->quest_id != Right.quest_id)
        return false;
    if (this->begin_time != Right.begin_time)
        return false;
    if (this->received != Right.received)
        return false;
    if (this->level != Right.level)
        return false;
    if (this->money_num != Right.money_num)
        return false;
    return true;
}

bool FPbSeptQuest::operator!=(const FPbSeptQuest& Right) const
{
    return !operator==(Right);
}

FPbRoleSeptQuestData::FPbRoleSeptQuestData()
{
    Reset();        
}

FPbRoleSeptQuestData::FPbRoleSeptQuestData(const idlepb::RoleSeptQuestData& Right)
{
    this->FromPb(Right);
}

void FPbRoleSeptQuestData::FromPb(const idlepb::RoleSeptQuestData& Right)
{
    quests.Empty();
    for (const auto& Elem : Right.quests())
    {
        quests.Emplace(Elem);
    }
    today_manual_refresh_num = Right.today_manual_refresh_num();
    level = Right.level();
    current_exp = Right.current_exp();
    total_num = Right.total_num();
}

void FPbRoleSeptQuestData::ToPb(idlepb::RoleSeptQuestData* Out) const
{
    for (const auto& Elem : quests)
    {
        Elem.ToPb(Out->add_quests());    
    }
    Out->set_today_manual_refresh_num(today_manual_refresh_num);
    Out->set_level(level);
    Out->set_current_exp(current_exp);
    Out->set_total_num(total_num);    
}

void FPbRoleSeptQuestData::Reset()
{
    quests = TArray<FPbSeptQuest>();
    today_manual_refresh_num = int32();
    level = int32();
    current_exp = int32();
    total_num = int32();    
}

void FPbRoleSeptQuestData::operator=(const idlepb::RoleSeptQuestData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleSeptQuestData::operator==(const FPbRoleSeptQuestData& Right) const
{
    if (this->quests != Right.quests)
        return false;
    if (this->today_manual_refresh_num != Right.today_manual_refresh_num)
        return false;
    if (this->level != Right.level)
        return false;
    if (this->current_exp != Right.current_exp)
        return false;
    if (this->total_num != Right.total_num)
        return false;
    return true;
}

bool FPbRoleSeptQuestData::operator!=(const FPbRoleSeptQuestData& Right) const
{
    return !operator==(Right);
}

FPbRoleSeptShopData::FPbRoleSeptShopData()
{
    Reset();        
}

FPbRoleSeptShopData::FPbRoleSeptShopData(const idlepb::RoleSeptShopData& Right)
{
    this->FromPb(Right);
}

void FPbRoleSeptShopData::FromPb(const idlepb::RoleSeptShopData& Right)
{
    last_reset_time_sept_shop = Right.last_reset_time_sept_shop();
    exchange_history.Empty();
    for (const auto& Elem : Right.exchange_history())
    {
        exchange_history.Emplace(Elem);
    }
}

void FPbRoleSeptShopData::ToPb(idlepb::RoleSeptShopData* Out) const
{
    Out->set_last_reset_time_sept_shop(last_reset_time_sept_shop);
    for (const auto& Elem : exchange_history)
    {
        Elem.ToPb(Out->add_exchange_history());    
    }    
}

void FPbRoleSeptShopData::Reset()
{
    last_reset_time_sept_shop = int64();
    exchange_history = TArray<FPbSimpleItemData>();    
}

void FPbRoleSeptShopData::operator=(const idlepb::RoleSeptShopData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleSeptShopData::operator==(const FPbRoleSeptShopData& Right) const
{
    if (this->last_reset_time_sept_shop != Right.last_reset_time_sept_shop)
        return false;
    if (this->exchange_history != Right.exchange_history)
        return false;
    return true;
}

bool FPbRoleSeptShopData::operator!=(const FPbRoleSeptShopData& Right) const
{
    return !operator==(Right);
}

FPbRoleSeptData::FPbRoleSeptData()
{
    Reset();        
}

FPbRoleSeptData::FPbRoleSeptData(const idlepb::RoleSeptData& Right)
{
    this->FromPb(Right);
}

void FPbRoleSeptData::FromPb(const idlepb::RoleSeptData& Right)
{
    next_join_ticks = Right.next_join_ticks();
    daily_invite_entries.Empty();
    for (const auto& Elem : Right.daily_invite_entries())
    {
        daily_invite_entries.Emplace(Elem);
    }
    sept_shop_data = Right.sept_shop_data();
    sept_quest_data = Right.sept_quest_data();
    sept_demon_cumulative_time = Right.sept_demon_cumulative_time();
    sept_demon_stage_reward_num = Right.sept_demon_stage_reward_num();
    sept_demon_stage_reward_use_num = Right.sept_demon_stage_reward_use_num();
    sept_demon_damage_reward_received.Empty();
    for (const auto& Elem : Right.sept_demon_damage_reward_received())
    {
        sept_demon_damage_reward_received.Emplace(Elem);
    }
    sept_demon_damage_reward_finished.Empty();
    for (const auto& Elem : Right.sept_demon_damage_reward_finished())
    {
        sept_demon_damage_reward_finished.Emplace(Elem);
    }
}

void FPbRoleSeptData::ToPb(idlepb::RoleSeptData* Out) const
{
    Out->set_next_join_ticks(next_join_ticks);
    for (const auto& Elem : daily_invite_entries)
    {
        Elem.ToPb(Out->add_daily_invite_entries());    
    }
    sept_shop_data.ToPb(Out->mutable_sept_shop_data());
    sept_quest_data.ToPb(Out->mutable_sept_quest_data());
    Out->set_sept_demon_cumulative_time(sept_demon_cumulative_time);
    Out->set_sept_demon_stage_reward_num(sept_demon_stage_reward_num);
    Out->set_sept_demon_stage_reward_use_num(sept_demon_stage_reward_use_num);
    for (const auto& Elem : sept_demon_damage_reward_received)
    {
        Out->add_sept_demon_damage_reward_received(Elem);    
    }
    for (const auto& Elem : sept_demon_damage_reward_finished)
    {
        Out->add_sept_demon_damage_reward_finished(Elem);    
    }    
}

void FPbRoleSeptData::Reset()
{
    next_join_ticks = int64();
    daily_invite_entries = TArray<FPbRoleSeptInviteEntry>();
    sept_shop_data = FPbRoleSeptShopData();
    sept_quest_data = FPbRoleSeptQuestData();
    sept_demon_cumulative_time = int32();
    sept_demon_stage_reward_num = int32();
    sept_demon_stage_reward_use_num = int32();
    sept_demon_damage_reward_received = TArray<int32>();
    sept_demon_damage_reward_finished = TArray<int32>();    
}

void FPbRoleSeptData::operator=(const idlepb::RoleSeptData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleSeptData::operator==(const FPbRoleSeptData& Right) const
{
    if (this->next_join_ticks != Right.next_join_ticks)
        return false;
    if (this->daily_invite_entries != Right.daily_invite_entries)
        return false;
    if (this->sept_shop_data != Right.sept_shop_data)
        return false;
    if (this->sept_quest_data != Right.sept_quest_data)
        return false;
    if (this->sept_demon_cumulative_time != Right.sept_demon_cumulative_time)
        return false;
    if (this->sept_demon_stage_reward_num != Right.sept_demon_stage_reward_num)
        return false;
    if (this->sept_demon_stage_reward_use_num != Right.sept_demon_stage_reward_use_num)
        return false;
    if (this->sept_demon_damage_reward_received != Right.sept_demon_damage_reward_received)
        return false;
    if (this->sept_demon_damage_reward_finished != Right.sept_demon_damage_reward_finished)
        return false;
    return true;
}

bool FPbRoleSeptData::operator!=(const FPbRoleSeptData& Right) const
{
    return !operator==(Right);
}

FPbSeptDemonWorldData::FPbSeptDemonWorldData()
{
    Reset();        
}

FPbSeptDemonWorldData::FPbSeptDemonWorldData(const idlepb::SeptDemonWorldData& Right)
{
    this->FromPb(Right);
}

void FPbSeptDemonWorldData::FromPb(const idlepb::SeptDemonWorldData& Right)
{
    is_started = Right.is_started();
    cur_end_ticks = Right.cur_end_ticks();
    next_open_ticks = Right.next_open_ticks();
    cur_stage = Right.cur_stage();
    cur_stage_hp = Right.cur_stage_hp();
    cur_stage_maxhp = Right.cur_stage_maxhp();
    last_stage = Right.last_stage();
    player_ids.Empty();
    for (const auto& Elem : Right.player_ids())
    {
        player_ids.Emplace(Elem);
    }
}

void FPbSeptDemonWorldData::ToPb(idlepb::SeptDemonWorldData* Out) const
{
    Out->set_is_started(is_started);
    Out->set_cur_end_ticks(cur_end_ticks);
    Out->set_next_open_ticks(next_open_ticks);
    Out->set_cur_stage(cur_stage);
    Out->set_cur_stage_hp(cur_stage_hp);
    Out->set_cur_stage_maxhp(cur_stage_maxhp);
    Out->set_last_stage(last_stage);
    for (const auto& Elem : player_ids)
    {
        Out->add_player_ids(Elem);    
    }    
}

void FPbSeptDemonWorldData::Reset()
{
    is_started = bool();
    cur_end_ticks = int64();
    next_open_ticks = int64();
    cur_stage = int32();
    cur_stage_hp = float();
    cur_stage_maxhp = float();
    last_stage = int32();
    player_ids = TArray<int64>();    
}

void FPbSeptDemonWorldData::operator=(const idlepb::SeptDemonWorldData& Right)
{
    this->FromPb(Right);
}

bool FPbSeptDemonWorldData::operator==(const FPbSeptDemonWorldData& Right) const
{
    if (this->is_started != Right.is_started)
        return false;
    if (this->cur_end_ticks != Right.cur_end_ticks)
        return false;
    if (this->next_open_ticks != Right.next_open_ticks)
        return false;
    if (this->cur_stage != Right.cur_stage)
        return false;
    if (this->cur_stage_hp != Right.cur_stage_hp)
        return false;
    if (this->cur_stage_maxhp != Right.cur_stage_maxhp)
        return false;
    if (this->last_stage != Right.last_stage)
        return false;
    if (this->player_ids != Right.player_ids)
        return false;
    return true;
}

bool FPbSeptDemonWorldData::operator!=(const FPbSeptDemonWorldData& Right) const
{
    return !operator==(Right);
}

FPbSimpleCounter::FPbSimpleCounter()
{
    Reset();        
}

FPbSimpleCounter::FPbSimpleCounter(const idlepb::SimpleCounter& Right)
{
    this->FromPb(Right);
}

void FPbSimpleCounter::FromPb(const idlepb::SimpleCounter& Right)
{
    id = Right.id();
    num = Right.num();
}

void FPbSimpleCounter::ToPb(idlepb::SimpleCounter* Out) const
{
    Out->set_id(id);
    Out->set_num(num);    
}

void FPbSimpleCounter::Reset()
{
    id = int32();
    num = int64();    
}

void FPbSimpleCounter::operator=(const idlepb::SimpleCounter& Right)
{
    this->FromPb(Right);
}

bool FPbSimpleCounter::operator==(const FPbSimpleCounter& Right) const
{
    if (this->id != Right.id)
        return false;
    if (this->num != Right.num)
        return false;
    return true;
}

bool FPbSimpleCounter::operator!=(const FPbSimpleCounter& Right) const
{
    return !operator==(Right);
}

FPbFunctionCounter::FPbFunctionCounter()
{
    Reset();        
}

FPbFunctionCounter::FPbFunctionCounter(const idlepb::FunctionCounter& Right)
{
    this->FromPb(Right);
}

void FPbFunctionCounter::FromPb(const idlepb::FunctionCounter& Right)
{
    function_type = Right.function_type();
    counters.Empty();
    for (const auto& Elem : Right.counters())
    {
        counters.Emplace(Elem);
    }
}

void FPbFunctionCounter::ToPb(idlepb::FunctionCounter* Out) const
{
    Out->set_function_type(function_type);
    for (const auto& Elem : counters)
    {
        Elem.ToPb(Out->add_counters());    
    }    
}

void FPbFunctionCounter::Reset()
{
    function_type = int32();
    counters = TArray<FPbSimpleCounter>();    
}

void FPbFunctionCounter::operator=(const idlepb::FunctionCounter& Right)
{
    this->FromPb(Right);
}

bool FPbFunctionCounter::operator==(const FPbFunctionCounter& Right) const
{
    if (this->function_type != Right.function_type)
        return false;
    if (this->counters != Right.counters)
        return false;
    return true;
}

bool FPbFunctionCounter::operator!=(const FPbFunctionCounter& Right) const
{
    return !operator==(Right);
}

FPbRoleLifeCounterData::FPbRoleLifeCounterData()
{
    Reset();        
}

FPbRoleLifeCounterData::FPbRoleLifeCounterData(const idlepb::RoleLifeCounterData& Right)
{
    this->FromPb(Right);
}

void FPbRoleLifeCounterData::FromPb(const idlepb::RoleLifeCounterData& Right)
{
    function_counter.Empty();
    for (const auto& Elem : Right.function_counter())
    {
        function_counter.Emplace(Elem);
    }
}

void FPbRoleLifeCounterData::ToPb(idlepb::RoleLifeCounterData* Out) const
{
    for (const auto& Elem : function_counter)
    {
        Elem.ToPb(Out->add_function_counter());    
    }    
}

void FPbRoleLifeCounterData::Reset()
{
    function_counter = TArray<FPbFunctionCounter>();    
}

void FPbRoleLifeCounterData::operator=(const idlepb::RoleLifeCounterData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleLifeCounterData::operator==(const FPbRoleLifeCounterData& Right) const
{
    if (this->function_counter != Right.function_counter)
        return false;
    return true;
}

bool FPbRoleLifeCounterData::operator!=(const FPbRoleLifeCounterData& Right) const
{
    return !operator==(Right);
}

FPbFarmlandManagementInfo::FPbFarmlandManagementInfo()
{
    Reset();        
}

FPbFarmlandManagementInfo::FPbFarmlandManagementInfo(const idlepb::FarmlandManagementInfo& Right)
{
    this->FromPb(Right);
}

void FPbFarmlandManagementInfo::FromPb(const idlepb::FarmlandManagementInfo& Right)
{
    plant_uid = Right.plant_uid();
    target_stage = Right.target_stage();
    auto_seed = Right.auto_seed();
    auto_harvest = Right.auto_harvest();
}

void FPbFarmlandManagementInfo::ToPb(idlepb::FarmlandManagementInfo* Out) const
{
    Out->set_plant_uid(plant_uid);
    Out->set_target_stage(target_stage);
    Out->set_auto_seed(auto_seed);
    Out->set_auto_harvest(auto_harvest);    
}

void FPbFarmlandManagementInfo::Reset()
{
    plant_uid = int32();
    target_stage = int32();
    auto_seed = bool();
    auto_harvest = bool();    
}

void FPbFarmlandManagementInfo::operator=(const idlepb::FarmlandManagementInfo& Right)
{
    this->FromPb(Right);
}

bool FPbFarmlandManagementInfo::operator==(const FPbFarmlandManagementInfo& Right) const
{
    if (this->plant_uid != Right.plant_uid)
        return false;
    if (this->target_stage != Right.target_stage)
        return false;
    if (this->auto_seed != Right.auto_seed)
        return false;
    if (this->auto_harvest != Right.auto_harvest)
        return false;
    return true;
}

bool FPbFarmlandManagementInfo::operator!=(const FPbFarmlandManagementInfo& Right) const
{
    return !operator==(Right);
}

FPbFarmlandPlantData::FPbFarmlandPlantData()
{
    Reset();        
}

FPbFarmlandPlantData::FPbFarmlandPlantData(const idlepb::FarmlandPlantData& Right)
{
    this->FromPb(Right);
}

void FPbFarmlandPlantData::FromPb(const idlepb::FarmlandPlantData& Right)
{
    plant_uid = Right.plant_uid();
    x = Right.x();
    y = Right.y();
    rotation = Right.rotation();
    config_id = Right.config_id();
    shenling = Right.shenling();
    begin_date = Right.begin_date();
    speed_up = Right.speed_up();
}

void FPbFarmlandPlantData::ToPb(idlepb::FarmlandPlantData* Out) const
{
    Out->set_plant_uid(plant_uid);
    Out->set_x(x);
    Out->set_y(y);
    Out->set_rotation(rotation);
    Out->set_config_id(config_id);
    Out->set_shenling(shenling);
    Out->set_begin_date(begin_date);
    Out->set_speed_up(speed_up);    
}

void FPbFarmlandPlantData::Reset()
{
    plant_uid = int32();
    x = int32();
    y = int32();
    rotation = int32();
    config_id = int32();
    shenling = int32();
    begin_date = int64();
    speed_up = int32();    
}

void FPbFarmlandPlantData::operator=(const idlepb::FarmlandPlantData& Right)
{
    this->FromPb(Right);
}

bool FPbFarmlandPlantData::operator==(const FPbFarmlandPlantData& Right) const
{
    if (this->plant_uid != Right.plant_uid)
        return false;
    if (this->x != Right.x)
        return false;
    if (this->y != Right.y)
        return false;
    if (this->rotation != Right.rotation)
        return false;
    if (this->config_id != Right.config_id)
        return false;
    if (this->shenling != Right.shenling)
        return false;
    if (this->begin_date != Right.begin_date)
        return false;
    if (this->speed_up != Right.speed_up)
        return false;
    return true;
}

bool FPbFarmlandPlantData::operator!=(const FPbFarmlandPlantData& Right) const
{
    return !operator==(Right);
}

FPbRoleFarmlandData::FPbRoleFarmlandData()
{
    Reset();        
}

FPbRoleFarmlandData::FPbRoleFarmlandData(const idlepb::RoleFarmlandData& Right)
{
    this->FromPb(Right);
}

void FPbRoleFarmlandData::FromPb(const idlepb::RoleFarmlandData& Right)
{
    current_plants.Empty();
    for (const auto& Elem : Right.current_plants())
    {
        current_plants.Emplace(Elem);
    }
    managment_plan.Empty();
    for (const auto& Elem : Right.managment_plan())
    {
        managment_plan.Emplace(Elem);
    }
    unlock_blocks.Empty();
    for (const auto& Elem : Right.unlock_blocks())
    {
        unlock_blocks.Emplace(Elem);
    }
    farmer_grade = Right.farmer_grade();
    farmer_friendship_exp = Right.farmer_friendship_exp();
    ripe_items.Empty();
    for (const auto& Elem : Right.ripe_items())
    {
        ripe_items.Emplace(Elem);
    }
}

void FPbRoleFarmlandData::ToPb(idlepb::RoleFarmlandData* Out) const
{
    for (const auto& Elem : current_plants)
    {
        Elem.ToPb(Out->add_current_plants());    
    }
    for (const auto& Elem : managment_plan)
    {
        Elem.ToPb(Out->add_managment_plan());    
    }
    for (const auto& Elem : unlock_blocks)
    {
        Elem.ToPb(Out->add_unlock_blocks());    
    }
    Out->set_farmer_grade(farmer_grade);
    Out->set_farmer_friendship_exp(farmer_friendship_exp);
    for (const auto& Elem : ripe_items)
    {
        Elem.ToPb(Out->add_ripe_items());    
    }    
}

void FPbRoleFarmlandData::Reset()
{
    current_plants = TArray<FPbFarmlandPlantData>();
    managment_plan = TArray<FPbFarmlandManagementInfo>();
    unlock_blocks = TArray<FPbVector2>();
    farmer_grade = int32();
    farmer_friendship_exp = int32();
    ripe_items = TArray<FPbSimpleItemData>();    
}

void FPbRoleFarmlandData::operator=(const idlepb::RoleFarmlandData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleFarmlandData::operator==(const FPbRoleFarmlandData& Right) const
{
    if (this->current_plants != Right.current_plants)
        return false;
    if (this->managment_plan != Right.managment_plan)
        return false;
    if (this->unlock_blocks != Right.unlock_blocks)
        return false;
    if (this->farmer_grade != Right.farmer_grade)
        return false;
    if (this->farmer_friendship_exp != Right.farmer_friendship_exp)
        return false;
    if (this->ripe_items != Right.ripe_items)
        return false;
    return true;
}

bool FPbRoleFarmlandData::operator!=(const FPbRoleFarmlandData& Right) const
{
    return !operator==(Right);
}

FPbRoleAvatarData::FPbRoleAvatarData()
{
    Reset();        
}

FPbRoleAvatarData::FPbRoleAvatarData(const idlepb::RoleAvatarData& Right)
{
    this->FromPb(Right);
}

void FPbRoleAvatarData::FromPb(const idlepb::RoleAvatarData& Right)
{
    rank = Right.rank();
    current_world_index = Right.current_world_index();
    begin_time = Right.begin_time();
    last_draw_time = Right.last_draw_time();
    temp_package.Empty();
    for (const auto& Elem : Right.temp_package())
    {
        temp_package.Emplace(Elem);
    }
    last_wrold_index = Right.last_wrold_index();
}

void FPbRoleAvatarData::ToPb(idlepb::RoleAvatarData* Out) const
{
    Out->set_rank(rank);
    Out->set_current_world_index(current_world_index);
    Out->set_begin_time(begin_time);
    Out->set_last_draw_time(last_draw_time);
    for (const auto& Elem : temp_package)
    {
        Elem.ToPb(Out->add_temp_package());    
    }
    Out->set_last_wrold_index(last_wrold_index);    
}

void FPbRoleAvatarData::Reset()
{
    rank = int32();
    current_world_index = int32();
    begin_time = int64();
    last_draw_time = int64();
    temp_package = TArray<FPbSimpleItemData>();
    last_wrold_index = int32();    
}

void FPbRoleAvatarData::operator=(const idlepb::RoleAvatarData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleAvatarData::operator==(const FPbRoleAvatarData& Right) const
{
    if (this->rank != Right.rank)
        return false;
    if (this->current_world_index != Right.current_world_index)
        return false;
    if (this->begin_time != Right.begin_time)
        return false;
    if (this->last_draw_time != Right.last_draw_time)
        return false;
    if (this->temp_package != Right.temp_package)
        return false;
    if (this->last_wrold_index != Right.last_wrold_index)
        return false;
    return true;
}

bool FPbRoleAvatarData::operator!=(const FPbRoleAvatarData& Right) const
{
    return !operator==(Right);
}

FPbBiographyRoleLog::FPbBiographyRoleLog()
{
    Reset();        
}

FPbBiographyRoleLog::FPbBiographyRoleLog(const idlepb::BiographyRoleLog& Right)
{
    this->FromPb(Right);
}

void FPbBiographyRoleLog::FromPb(const idlepb::BiographyRoleLog& Right)
{
    dao_year = Right.dao_year();
    log_type = Right.log_type();
    poem_seed = Right.poem_seed();
    zone_name = UTF8_TO_TCHAR(Right.zone_name().c_str());
    role_name = UTF8_TO_TCHAR(Right.role_name().c_str());
    content = UTF8_TO_TCHAR(Right.content().c_str());
}

void FPbBiographyRoleLog::ToPb(idlepb::BiographyRoleLog* Out) const
{
    Out->set_dao_year(dao_year);
    Out->set_log_type(log_type);
    Out->set_poem_seed(poem_seed);
    Out->set_zone_name(TCHAR_TO_UTF8(*zone_name));
    Out->set_role_name(TCHAR_TO_UTF8(*role_name));
    Out->set_content(TCHAR_TO_UTF8(*content));    
}

void FPbBiographyRoleLog::Reset()
{
    dao_year = int32();
    log_type = int32();
    poem_seed = int32();
    zone_name = FString();
    role_name = FString();
    content = FString();    
}

void FPbBiographyRoleLog::operator=(const idlepb::BiographyRoleLog& Right)
{
    this->FromPb(Right);
}

bool FPbBiographyRoleLog::operator==(const FPbBiographyRoleLog& Right) const
{
    if (this->dao_year != Right.dao_year)
        return false;
    if (this->log_type != Right.log_type)
        return false;
    if (this->poem_seed != Right.poem_seed)
        return false;
    if (this->zone_name != Right.zone_name)
        return false;
    if (this->role_name != Right.role_name)
        return false;
    if (this->content != Right.content)
        return false;
    return true;
}

bool FPbBiographyRoleLog::operator!=(const FPbBiographyRoleLog& Right) const
{
    return !operator==(Right);
}

FPbRoleBiographyData::FPbRoleBiographyData()
{
    Reset();        
}

FPbRoleBiographyData::FPbRoleBiographyData(const idlepb::RoleBiographyData& Right)
{
    this->FromPb(Right);
}

void FPbRoleBiographyData::FromPb(const idlepb::RoleBiographyData& Right)
{
    received_cfg_ids.Empty();
    for (const auto& Elem : Right.received_cfg_ids())
    {
        received_cfg_ids.Emplace(Elem);
    }
    received_event_cfg_ids.Empty();
    for (const auto& Elem : Right.received_event_cfg_ids())
    {
        received_event_cfg_ids.Emplace(Elem);
    }
    role_logs.Empty();
    for (const auto& Elem : Right.role_logs())
    {
        role_logs.Emplace(Elem);
    }
}

void FPbRoleBiographyData::ToPb(idlepb::RoleBiographyData* Out) const
{
    for (const auto& Elem : received_cfg_ids)
    {
        Out->add_received_cfg_ids(Elem);    
    }
    for (const auto& Elem : received_event_cfg_ids)
    {
        Out->add_received_event_cfg_ids(Elem);    
    }
    for (const auto& Elem : role_logs)
    {
        Elem.ToPb(Out->add_role_logs());    
    }    
}

void FPbRoleBiographyData::Reset()
{
    received_cfg_ids = TArray<int32>();
    received_event_cfg_ids = TArray<int32>();
    role_logs = TArray<FPbBiographyRoleLog>();    
}

void FPbRoleBiographyData::operator=(const idlepb::RoleBiographyData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleBiographyData::operator==(const FPbRoleBiographyData& Right) const
{
    if (this->received_cfg_ids != Right.received_cfg_ids)
        return false;
    if (this->received_event_cfg_ids != Right.received_event_cfg_ids)
        return false;
    if (this->role_logs != Right.role_logs)
        return false;
    return true;
}

bool FPbRoleBiographyData::operator!=(const FPbRoleBiographyData& Right) const
{
    return !operator==(Right);
}

FPbSimpleRoleInfo::FPbSimpleRoleInfo()
{
    Reset();        
}

FPbSimpleRoleInfo::FPbSimpleRoleInfo(const idlepb::SimpleRoleInfo& Right)
{
    this->FromPb(Right);
}

void FPbSimpleRoleInfo::FromPb(const idlepb::SimpleRoleInfo& Right)
{
    role_id = Right.role_id();
    role_name = UTF8_TO_TCHAR(Right.role_name().c_str());
    model_config = Right.model_config();
    rank = Right.rank();
    role_cultivation_direction = static_cast<EPbCultivationDirection>(Right.role_cultivation_direction());
    sept_name = UTF8_TO_TCHAR(Right.sept_name().c_str());
    sept_position = static_cast<EPbSeptPosition>(Right.sept_position());
    last_online_timespan = Right.last_online_timespan();
    server_id = Right.server_id();
}

void FPbSimpleRoleInfo::ToPb(idlepb::SimpleRoleInfo* Out) const
{
    Out->set_role_id(role_id);
    Out->set_role_name(TCHAR_TO_UTF8(*role_name));
    model_config.ToPb(Out->mutable_model_config());
    Out->set_rank(rank);
    Out->set_role_cultivation_direction(static_cast<idlepb::CultivationDirection>(role_cultivation_direction));
    Out->set_sept_name(TCHAR_TO_UTF8(*sept_name));
    Out->set_sept_position(static_cast<idlepb::SeptPosition>(sept_position));
    Out->set_last_online_timespan(last_online_timespan);
    Out->set_server_id(server_id);    
}

void FPbSimpleRoleInfo::Reset()
{
    role_id = int64();
    role_name = FString();
    model_config = FPbCharacterModelConfig();
    rank = int32();
    role_cultivation_direction = EPbCultivationDirection();
    sept_name = FString();
    sept_position = EPbSeptPosition();
    last_online_timespan = int64();
    server_id = int64();    
}

void FPbSimpleRoleInfo::operator=(const idlepb::SimpleRoleInfo& Right)
{
    this->FromPb(Right);
}

bool FPbSimpleRoleInfo::operator==(const FPbSimpleRoleInfo& Right) const
{
    if (this->role_id != Right.role_id)
        return false;
    if (this->role_name != Right.role_name)
        return false;
    if (this->model_config != Right.model_config)
        return false;
    if (this->rank != Right.rank)
        return false;
    if (this->role_cultivation_direction != Right.role_cultivation_direction)
        return false;
    if (this->sept_name != Right.sept_name)
        return false;
    if (this->sept_position != Right.sept_position)
        return false;
    if (this->last_online_timespan != Right.last_online_timespan)
        return false;
    if (this->server_id != Right.server_id)
        return false;
    return true;
}

bool FPbSimpleRoleInfo::operator!=(const FPbSimpleRoleInfo& Right) const
{
    return !operator==(Right);
}

FPbChatMessage::FPbChatMessage()
{
    Reset();        
}

FPbChatMessage::FPbChatMessage(const idlepb::ChatMessage& Right)
{
    this->FromPb(Right);
}

void FPbChatMessage::FromPb(const idlepb::ChatMessage& Right)
{
    role_id = Right.role_id();
    text = UTF8_TO_TCHAR(Right.text().c_str());
    role_info = Right.role_info();
    type = static_cast<EPbChatMessageType>(Right.type());
    time = Right.time();
}

void FPbChatMessage::ToPb(idlepb::ChatMessage* Out) const
{
    Out->set_role_id(role_id);
    Out->set_text(TCHAR_TO_UTF8(*text));
    role_info.ToPb(Out->mutable_role_info());
    Out->set_type(static_cast<idlepb::ChatMessageType>(type));
    Out->set_time(time);    
}

void FPbChatMessage::Reset()
{
    role_id = int64();
    text = FString();
    role_info = FPbSimpleRoleInfo();
    type = EPbChatMessageType();
    time = int64();    
}

void FPbChatMessage::operator=(const idlepb::ChatMessage& Right)
{
    this->FromPb(Right);
}

bool FPbChatMessage::operator==(const FPbChatMessage& Right) const
{
    if (this->role_id != Right.role_id)
        return false;
    if (this->text != Right.text)
        return false;
    if (this->role_info != Right.role_info)
        return false;
    if (this->type != Right.type)
        return false;
    if (this->time != Right.time)
        return false;
    return true;
}

bool FPbChatMessage::operator!=(const FPbChatMessage& Right) const
{
    return !operator==(Right);
}

FPbPrivateChatRecord::FPbPrivateChatRecord()
{
    Reset();        
}

FPbPrivateChatRecord::FPbPrivateChatRecord(const idlepb::PrivateChatRecord& Right)
{
    this->FromPb(Right);
}

void FPbPrivateChatRecord::FromPb(const idlepb::PrivateChatRecord& Right)
{
    role_id = Right.role_id();
    chat_record.Empty();
    for (const auto& Elem : Right.chat_record())
    {
        chat_record.Emplace(Elem);
    }
    unread_num = Right.unread_num();
}

void FPbPrivateChatRecord::ToPb(idlepb::PrivateChatRecord* Out) const
{
    Out->set_role_id(role_id);
    for (const auto& Elem : chat_record)
    {
        Elem.ToPb(Out->add_chat_record());    
    }
    Out->set_unread_num(unread_num);    
}

void FPbPrivateChatRecord::Reset()
{
    role_id = int64();
    chat_record = TArray<FPbChatMessage>();
    unread_num = int32();    
}

void FPbPrivateChatRecord::operator=(const idlepb::PrivateChatRecord& Right)
{
    this->FromPb(Right);
}

bool FPbPrivateChatRecord::operator==(const FPbPrivateChatRecord& Right) const
{
    if (this->role_id != Right.role_id)
        return false;
    if (this->chat_record != Right.chat_record)
        return false;
    if (this->unread_num != Right.unread_num)
        return false;
    return true;
}

bool FPbPrivateChatRecord::operator!=(const FPbPrivateChatRecord& Right) const
{
    return !operator==(Right);
}

FPbRolePrivateChatRecord::FPbRolePrivateChatRecord()
{
    Reset();        
}

FPbRolePrivateChatRecord::FPbRolePrivateChatRecord(const idlepb::RolePrivateChatRecord& Right)
{
    this->FromPb(Right);
}

void FPbRolePrivateChatRecord::FromPb(const idlepb::RolePrivateChatRecord& Right)
{
    role_id = Right.role_id();
    data.Empty();
    for (const auto& Elem : Right.data())
    {
        data.Emplace(Elem);
    }
}

void FPbRolePrivateChatRecord::ToPb(idlepb::RolePrivateChatRecord* Out) const
{
    Out->set_role_id(role_id);
    for (const auto& Elem : data)
    {
        Elem.ToPb(Out->add_data());    
    }    
}

void FPbRolePrivateChatRecord::Reset()
{
    role_id = int64();
    data = TArray<FPbPrivateChatRecord>();    
}

void FPbRolePrivateChatRecord::operator=(const idlepb::RolePrivateChatRecord& Right)
{
    this->FromPb(Right);
}

bool FPbRolePrivateChatRecord::operator==(const FPbRolePrivateChatRecord& Right) const
{
    if (this->role_id != Right.role_id)
        return false;
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbRolePrivateChatRecord::operator!=(const FPbRolePrivateChatRecord& Right) const
{
    return !operator==(Right);
}

FPbChatData::FPbChatData()
{
    Reset();        
}

FPbChatData::FPbChatData(const idlepb::ChatData& Right)
{
    this->FromPb(Right);
}

void FPbChatData::FromPb(const idlepb::ChatData& Right)
{
    colony_servers.Empty();
    for (const auto& Elem : Right.colony_servers())
    {
        colony_servers.Emplace(Elem);
    }
    quad_servers.Empty();
    for (const auto& Elem : Right.quad_servers())
    {
        quad_servers.Emplace(Elem);
    }
    local_server.Empty();
    for (const auto& Elem : Right.local_server())
    {
        local_server.Emplace(Elem);
    }
}

void FPbChatData::ToPb(idlepb::ChatData* Out) const
{
    for (const auto& Elem : colony_servers)
    {
        Elem.ToPb(Out->add_colony_servers());    
    }
    for (const auto& Elem : quad_servers)
    {
        Elem.ToPb(Out->add_quad_servers());    
    }
    for (const auto& Elem : local_server)
    {
        Elem.ToPb(Out->add_local_server());    
    }    
}

void FPbChatData::Reset()
{
    colony_servers = TArray<FPbChatMessage>();
    quad_servers = TArray<FPbChatMessage>();
    local_server = TArray<FPbChatMessage>();    
}

void FPbChatData::operator=(const idlepb::ChatData& Right)
{
    this->FromPb(Right);
}

bool FPbChatData::operator==(const FPbChatData& Right) const
{
    if (this->colony_servers != Right.colony_servers)
        return false;
    if (this->quad_servers != Right.quad_servers)
        return false;
    if (this->local_server != Right.local_server)
        return false;
    return true;
}

bool FPbChatData::operator!=(const FPbChatData& Right) const
{
    return !operator==(Right);
}

bool CheckEPbFriendRelationshipTypeValid(int32 Val)
{
    return idlepb::FriendRelationshipType_IsValid(Val);
}

const TCHAR* GetEPbFriendRelationshipTypeDescription(EPbFriendRelationshipType Val)
{
    switch (Val)
    {
        case EPbFriendRelationshipType::FRT_None: return TEXT("无关系");
        case EPbFriendRelationshipType::FRT_Friend: return TEXT("一般好友关系");
        case EPbFriendRelationshipType::FRT_Partner: return TEXT("道侣关系");
        case EPbFriendRelationshipType::FRT_Blocked: return TEXT("被对方拉黑");
    }
    return TEXT("UNKNOWN");
}

FPbFriendListItem::FPbFriendListItem()
{
    Reset();        
}

FPbFriendListItem::FPbFriendListItem(const idlepb::FriendListItem& Right)
{
    this->FromPb(Right);
}

void FPbFriendListItem::FromPb(const idlepb::FriendListItem& Right)
{
    role_id = Right.role_id();
    relationship = Right.relationship();
    type = static_cast<EPbFriendRelationshipType>(Right.type());
}

void FPbFriendListItem::ToPb(idlepb::FriendListItem* Out) const
{
    Out->set_role_id(role_id);
    Out->set_relationship(relationship);
    Out->set_type(static_cast<idlepb::FriendRelationshipType>(type));    
}

void FPbFriendListItem::Reset()
{
    role_id = int64();
    relationship = int32();
    type = EPbFriendRelationshipType();    
}

void FPbFriendListItem::operator=(const idlepb::FriendListItem& Right)
{
    this->FromPb(Right);
}

bool FPbFriendListItem::operator==(const FPbFriendListItem& Right) const
{
    if (this->role_id != Right.role_id)
        return false;
    if (this->relationship != Right.relationship)
        return false;
    if (this->type != Right.type)
        return false;
    return true;
}

bool FPbFriendListItem::operator!=(const FPbFriendListItem& Right) const
{
    return !operator==(Right);
}

FPbRoleFriendData::FPbRoleFriendData()
{
    Reset();        
}

FPbRoleFriendData::FPbRoleFriendData(const idlepb::RoleFriendData& Right)
{
    this->FromPb(Right);
}

void FPbRoleFriendData::FromPb(const idlepb::RoleFriendData& Right)
{
    friend_list.Empty();
    for (const auto& Elem : Right.friend_list())
    {
        friend_list.Emplace(Elem);
    }
    request_list.Empty();
    for (const auto& Elem : Right.request_list())
    {
        request_list.Emplace(Elem);
    }
    block_list.Empty();
    for (const auto& Elem : Right.block_list())
    {
        block_list.Emplace(Elem);
    }
    my_request.Empty();
    for (const auto& Elem : Right.my_request())
    {
        my_request.Emplace(Elem);
    }
    history_list.Empty();
    for (const auto& Elem : Right.history_list())
    {
        history_list.Emplace(Elem);
    }
}

void FPbRoleFriendData::ToPb(idlepb::RoleFriendData* Out) const
{
    for (const auto& Elem : friend_list)
    {
        Elem.ToPb(Out->add_friend_list());    
    }
    for (const auto& Elem : request_list)
    {
        Out->add_request_list(Elem);    
    }
    for (const auto& Elem : block_list)
    {
        Out->add_block_list(Elem);    
    }
    for (const auto& Elem : my_request)
    {
        Out->add_my_request(Elem);    
    }
    for (const auto& Elem : history_list)
    {
        Elem.ToPb(Out->add_history_list());    
    }    
}

void FPbRoleFriendData::Reset()
{
    friend_list = TArray<FPbFriendListItem>();
    request_list = TArray<int64>();
    block_list = TArray<int64>();
    my_request = TArray<int64>();
    history_list = TArray<FPbFriendListItem>();    
}

void FPbRoleFriendData::operator=(const idlepb::RoleFriendData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleFriendData::operator==(const FPbRoleFriendData& Right) const
{
    if (this->friend_list != Right.friend_list)
        return false;
    if (this->request_list != Right.request_list)
        return false;
    if (this->block_list != Right.block_list)
        return false;
    if (this->my_request != Right.my_request)
        return false;
    if (this->history_list != Right.history_list)
        return false;
    return true;
}

bool FPbRoleFriendData::operator!=(const FPbRoleFriendData& Right) const
{
    return !operator==(Right);
}

FPbRoleOfflineFunctionData::FPbRoleOfflineFunctionData()
{
    Reset();        
}

FPbRoleOfflineFunctionData::FPbRoleOfflineFunctionData(const idlepb::RoleOfflineFunctionData& Right)
{
    this->FromPb(Right);
}

void FPbRoleOfflineFunctionData::FromPb(const idlepb::RoleOfflineFunctionData& Right)
{
    role_id = Right.role_id();
    mails.Empty();
    for (const auto& Elem : Right.mails())
    {
        mails.Emplace(Elem);
    }
    friend_data = Right.friend_data();
    private_chat_data.Empty();
    for (const auto& Elem : Right.private_chat_data())
    {
        private_chat_data.Emplace(Elem);
    }
    role_logs.Empty();
    for (const auto& Elem : Right.role_logs())
    {
        role_logs.Emplace(Elem);
    }
    leaderboard_data = Right.leaderboard_data();
    role_name = UTF8_TO_TCHAR(Right.role_name().c_str());
    rank = Right.rank();
    degree = Right.degree();
    total_exp = Right.total_exp();
}

void FPbRoleOfflineFunctionData::ToPb(idlepb::RoleOfflineFunctionData* Out) const
{
    Out->set_role_id(role_id);
    for (const auto& Elem : mails)
    {
        Elem.ToPb(Out->add_mails());    
    }
    friend_data.ToPb(Out->mutable_friend_data());
    for (const auto& Elem : private_chat_data)
    {
        Elem.ToPb(Out->add_private_chat_data());    
    }
    for (const auto& Elem : role_logs)
    {
        Elem.ToPb(Out->add_role_logs());    
    }
    leaderboard_data.ToPb(Out->mutable_leaderboard_data());
    Out->set_role_name(TCHAR_TO_UTF8(*role_name));
    Out->set_rank(rank);
    Out->set_degree(degree);
    Out->set_total_exp(total_exp);    
}

void FPbRoleOfflineFunctionData::Reset()
{
    role_id = int64();
    mails = TArray<FPbMail>();
    friend_data = FPbRoleFriendData();
    private_chat_data = TArray<FPbPrivateChatRecord>();
    role_logs = TArray<FPbBiographyRoleLog>();
    leaderboard_data = FPbRoleLeaderboardData();
    role_name = FString();
    rank = int32();
    degree = int32();
    total_exp = int64();    
}

void FPbRoleOfflineFunctionData::operator=(const idlepb::RoleOfflineFunctionData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleOfflineFunctionData::operator==(const FPbRoleOfflineFunctionData& Right) const
{
    if (this->role_id != Right.role_id)
        return false;
    if (this->mails != Right.mails)
        return false;
    if (this->friend_data != Right.friend_data)
        return false;
    if (this->private_chat_data != Right.private_chat_data)
        return false;
    if (this->role_logs != Right.role_logs)
        return false;
    if (this->leaderboard_data != Right.leaderboard_data)
        return false;
    if (this->role_name != Right.role_name)
        return false;
    if (this->rank != Right.rank)
        return false;
    if (this->degree != Right.degree)
        return false;
    if (this->total_exp != Right.total_exp)
        return false;
    return true;
}

bool FPbRoleOfflineFunctionData::operator!=(const FPbRoleOfflineFunctionData& Right) const
{
    return !operator==(Right);
}

FPbServerCounterData::FPbServerCounterData()
{
    Reset();        
}

FPbServerCounterData::FPbServerCounterData(const idlepb::ServerCounterData& Right)
{
    this->FromPb(Right);
}

void FPbServerCounterData::FromPb(const idlepb::ServerCounterData& Right)
{
    function_counter.Empty();
    for (const auto& Elem : Right.function_counter())
    {
        function_counter.Emplace(Elem);
    }
}

void FPbServerCounterData::ToPb(idlepb::ServerCounterData* Out) const
{
    for (const auto& Elem : function_counter)
    {
        Elem.ToPb(Out->add_function_counter());    
    }    
}

void FPbServerCounterData::Reset()
{
    function_counter = TArray<FPbFunctionCounter>();    
}

void FPbServerCounterData::operator=(const idlepb::ServerCounterData& Right)
{
    this->FromPb(Right);
}

bool FPbServerCounterData::operator==(const FPbServerCounterData& Right) const
{
    if (this->function_counter != Right.function_counter)
        return false;
    return true;
}

bool FPbServerCounterData::operator!=(const FPbServerCounterData& Right) const
{
    return !operator==(Right);
}

FPbSocialFunctionCommonSaveData::FPbSocialFunctionCommonSaveData()
{
    Reset();        
}

FPbSocialFunctionCommonSaveData::FPbSocialFunctionCommonSaveData(const idlepb::SocialFunctionCommonSaveData& Right)
{
    this->FromPb(Right);
}

void FPbSocialFunctionCommonSaveData::FromPb(const idlepb::SocialFunctionCommonSaveData& Right)
{
    offline_role_datas.Empty();
    for (const auto& Elem : Right.offline_role_datas())
    {
        offline_role_datas.Emplace(Elem);
    }
    server_counter_data = Right.server_counter_data();
    role_list.Empty();
    for (const auto& Elem : Right.role_list())
    {
        role_list.Emplace(Elem);
    }
}

void FPbSocialFunctionCommonSaveData::ToPb(idlepb::SocialFunctionCommonSaveData* Out) const
{
    for (const auto& Elem : offline_role_datas)
    {
        Elem.ToPb(Out->add_offline_role_datas());    
    }
    server_counter_data.ToPb(Out->mutable_server_counter_data());
    for (const auto& Elem : role_list)
    {
        Elem.ToPb(Out->add_role_list());    
    }    
}

void FPbSocialFunctionCommonSaveData::Reset()
{
    offline_role_datas = TArray<FPbRoleOfflineFunctionData>();
    server_counter_data = FPbServerCounterData();
    role_list = TArray<FPbStringInt64Pair>();    
}

void FPbSocialFunctionCommonSaveData::operator=(const idlepb::SocialFunctionCommonSaveData& Right)
{
    this->FromPb(Right);
}

bool FPbSocialFunctionCommonSaveData::operator==(const FPbSocialFunctionCommonSaveData& Right) const
{
    if (this->offline_role_datas != Right.offline_role_datas)
        return false;
    if (this->server_counter_data != Right.server_counter_data)
        return false;
    if (this->role_list != Right.role_list)
        return false;
    return true;
}

bool FPbSocialFunctionCommonSaveData::operator!=(const FPbSocialFunctionCommonSaveData& Right) const
{
    return !operator==(Right);
}

FPbRoleSaveData::FPbRoleSaveData()
{
    Reset();        
}

FPbRoleSaveData::FPbRoleSaveData(const idlepb::RoleSaveData& Right)
{
    this->FromPb(Right);
}

void FPbRoleSaveData::FromPb(const idlepb::RoleSaveData& Right)
{
    role_data = Right.role_data();
    all_stats_data = Right.all_stats_data();
    hp = Right.hp();
    mp = Right.mp();
    quest = Right.quest();
    shop = Right.shop();
    temporary_package = Right.temporary_package();
    inventory = Right.inventory();
    offline_data = Right.offline_data();
    alchemy_data = Right.alchemy_data();
    deluxe_shop = Right.deluxe_shop();
    leaderboard_data = Right.leaderboard_data();
    mail_data = Right.mail_data();
    forge_data = Right.forge_data();
    pillelixir_data = Right.pillelixir_data();
    common_cultivation_data = Right.common_cultivation_data();
    zas_data = Right.zas_data();
    monster_tower_data = Right.monster_tower_data();
    shanhetu_data = Right.shanhetu_data();
    boss_invasion_data = Right.boss_invasion_data();
    massive_data = Right.massive_data();
    checklist_data = Right.checklist_data();
    common_item_exchange_data = Right.common_item_exchange_data();
    sept_data = Right.sept_data();
    treasury_chest_data = Right.treasury_chest_data();
    gongfa_data = Right.gongfa_data();
    fuzeng_data = Right.fuzeng_data();
    collection_data = Right.collection_data();
    life_counter_data = Right.life_counter_data();
    appearance_data = Right.appearance_data();
    arena_check_list_data = Right.arena_check_list_data();
    dungeon_kill_all_data = Right.dungeon_kill_all_data();
    farmland_data = Right.farmland_data();
    dungeon_survive_data = Right.dungeon_survive_data();
    friend_data = Right.friend_data();
    avatar_data = Right.avatar_data();
    arena_statistical_data = Right.arena_statistical_data();
    biography_data = Right.biography_data();
    vip_shop_data = Right.vip_shop_data();
}

void FPbRoleSaveData::ToPb(idlepb::RoleSaveData* Out) const
{
    role_data.ToPb(Out->mutable_role_data());
    all_stats_data.ToPb(Out->mutable_all_stats_data());
    Out->set_hp(hp);
    Out->set_mp(mp);
    quest.ToPb(Out->mutable_quest());
    shop.ToPb(Out->mutable_shop());
    temporary_package.ToPb(Out->mutable_temporary_package());
    inventory.ToPb(Out->mutable_inventory());
    offline_data.ToPb(Out->mutable_offline_data());
    alchemy_data.ToPb(Out->mutable_alchemy_data());
    deluxe_shop.ToPb(Out->mutable_deluxe_shop());
    leaderboard_data.ToPb(Out->mutable_leaderboard_data());
    mail_data.ToPb(Out->mutable_mail_data());
    forge_data.ToPb(Out->mutable_forge_data());
    pillelixir_data.ToPb(Out->mutable_pillelixir_data());
    common_cultivation_data.ToPb(Out->mutable_common_cultivation_data());
    zas_data.ToPb(Out->mutable_zas_data());
    monster_tower_data.ToPb(Out->mutable_monster_tower_data());
    shanhetu_data.ToPb(Out->mutable_shanhetu_data());
    boss_invasion_data.ToPb(Out->mutable_boss_invasion_data());
    massive_data.ToPb(Out->mutable_massive_data());
    checklist_data.ToPb(Out->mutable_checklist_data());
    common_item_exchange_data.ToPb(Out->mutable_common_item_exchange_data());
    sept_data.ToPb(Out->mutable_sept_data());
    treasury_chest_data.ToPb(Out->mutable_treasury_chest_data());
    gongfa_data.ToPb(Out->mutable_gongfa_data());
    fuzeng_data.ToPb(Out->mutable_fuzeng_data());
    collection_data.ToPb(Out->mutable_collection_data());
    life_counter_data.ToPb(Out->mutable_life_counter_data());
    appearance_data.ToPb(Out->mutable_appearance_data());
    arena_check_list_data.ToPb(Out->mutable_arena_check_list_data());
    dungeon_kill_all_data.ToPb(Out->mutable_dungeon_kill_all_data());
    farmland_data.ToPb(Out->mutable_farmland_data());
    dungeon_survive_data.ToPb(Out->mutable_dungeon_survive_data());
    friend_data.ToPb(Out->mutable_friend_data());
    avatar_data.ToPb(Out->mutable_avatar_data());
    arena_statistical_data.ToPb(Out->mutable_arena_statistical_data());
    biography_data.ToPb(Out->mutable_biography_data());
    vip_shop_data.ToPb(Out->mutable_vip_shop_data());    
}

void FPbRoleSaveData::Reset()
{
    role_data = FPbRoleData();
    all_stats_data = FPbGameStatsAllModuleData();
    hp = float();
    mp = float();
    quest = FPbRoleQuestData();
    shop = FPbRoleShopData();
    temporary_package = FPbRoleTemporaryPackageData();
    inventory = FPbRoleInventoryData();
    offline_data = FPbRoleOfflineData();
    alchemy_data = FPbRoleAlchemyData();
    deluxe_shop = FPbRoleDeluxeShopData();
    leaderboard_data = FPbRoleLeaderboardData();
    mail_data = FPbRoleMailData();
    forge_data = FPbRoleForgeData();
    pillelixir_data = FPbRolePillElixirData();
    common_cultivation_data = FPbCommonCultivationData();
    zas_data = FPbRoleZasData();
    monster_tower_data = FPbRoleMonsterTowerData();
    shanhetu_data = FPbRoleShanhetuData();
    boss_invasion_data = FPbRoleBossInvasionData();
    massive_data = FPbRoleMasiveData();
    checklist_data = FPbRoleChecklistData();
    common_item_exchange_data = FPbRoleCommonItemExchangeData();
    sept_data = FPbRoleSeptData();
    treasury_chest_data = FPbRoleTreasurySaveData();
    gongfa_data = FPbRoleGongFaData();
    fuzeng_data = FPbRoleFuZengData();
    collection_data = FPbRoleCollectionSaveData();
    life_counter_data = FPbRoleLifeCounterData();
    appearance_data = FPbRoleAppearanceData();
    arena_check_list_data = FPbRoleArenaCheckListData();
    dungeon_kill_all_data = FPbRoleDungeonKillAllData();
    farmland_data = FPbRoleFarmlandData();
    dungeon_survive_data = FPbRoleDungeonSurviveData();
    friend_data = FPbRoleFriendData();
    avatar_data = FPbRoleAvatarData();
    arena_statistical_data = FPbRoleArenaExplorationStatisticalData();
    biography_data = FPbRoleBiographyData();
    vip_shop_data = FPbRoleVipShopData();    
}

void FPbRoleSaveData::operator=(const idlepb::RoleSaveData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleSaveData::operator==(const FPbRoleSaveData& Right) const
{
    if (this->role_data != Right.role_data)
        return false;
    if (this->all_stats_data != Right.all_stats_data)
        return false;
    if (this->hp != Right.hp)
        return false;
    if (this->mp != Right.mp)
        return false;
    if (this->quest != Right.quest)
        return false;
    if (this->shop != Right.shop)
        return false;
    if (this->temporary_package != Right.temporary_package)
        return false;
    if (this->inventory != Right.inventory)
        return false;
    if (this->offline_data != Right.offline_data)
        return false;
    if (this->alchemy_data != Right.alchemy_data)
        return false;
    if (this->deluxe_shop != Right.deluxe_shop)
        return false;
    if (this->leaderboard_data != Right.leaderboard_data)
        return false;
    if (this->mail_data != Right.mail_data)
        return false;
    if (this->forge_data != Right.forge_data)
        return false;
    if (this->pillelixir_data != Right.pillelixir_data)
        return false;
    if (this->common_cultivation_data != Right.common_cultivation_data)
        return false;
    if (this->zas_data != Right.zas_data)
        return false;
    if (this->monster_tower_data != Right.monster_tower_data)
        return false;
    if (this->shanhetu_data != Right.shanhetu_data)
        return false;
    if (this->boss_invasion_data != Right.boss_invasion_data)
        return false;
    if (this->massive_data != Right.massive_data)
        return false;
    if (this->checklist_data != Right.checklist_data)
        return false;
    if (this->common_item_exchange_data != Right.common_item_exchange_data)
        return false;
    if (this->sept_data != Right.sept_data)
        return false;
    if (this->treasury_chest_data != Right.treasury_chest_data)
        return false;
    if (this->gongfa_data != Right.gongfa_data)
        return false;
    if (this->fuzeng_data != Right.fuzeng_data)
        return false;
    if (this->collection_data != Right.collection_data)
        return false;
    if (this->life_counter_data != Right.life_counter_data)
        return false;
    if (this->appearance_data != Right.appearance_data)
        return false;
    if (this->arena_check_list_data != Right.arena_check_list_data)
        return false;
    if (this->dungeon_kill_all_data != Right.dungeon_kill_all_data)
        return false;
    if (this->farmland_data != Right.farmland_data)
        return false;
    if (this->dungeon_survive_data != Right.dungeon_survive_data)
        return false;
    if (this->friend_data != Right.friend_data)
        return false;
    if (this->avatar_data != Right.avatar_data)
        return false;
    if (this->arena_statistical_data != Right.arena_statistical_data)
        return false;
    if (this->biography_data != Right.biography_data)
        return false;
    if (this->vip_shop_data != Right.vip_shop_data)
        return false;
    return true;
}

bool FPbRoleSaveData::operator!=(const FPbRoleSaveData& Right) const
{
    return !operator==(Right);
}

FPbBattleHistoryRecord::FPbBattleHistoryRecord()
{
    Reset();        
}

FPbBattleHistoryRecord::FPbBattleHistoryRecord(const idlepb::BattleHistoryRecord& Right)
{
    this->FromPb(Right);
}

void FPbBattleHistoryRecord::FromPb(const idlepb::BattleHistoryRecord& Right)
{
    index = Right.index();
    world_seconds = Right.world_seconds();
    source_entity_id = Right.source_entity_id();
    target_entity_id = Right.target_entity_id();
    message_type_id = Right.message_type_id();
    message_body.Empty(message_body.Num());
    message_body.Append(reinterpret_cast<const uint8*>(Right.message_body().c_str()), Right.message_body().size());
}

void FPbBattleHistoryRecord::ToPb(idlepb::BattleHistoryRecord* Out) const
{
    Out->set_index(index);
    Out->set_world_seconds(world_seconds);
    Out->set_source_entity_id(source_entity_id);
    Out->set_target_entity_id(target_entity_id);
    Out->set_message_type_id(message_type_id);
    Out->set_message_body(message_body.GetData(), message_body.Num());    
}

void FPbBattleHistoryRecord::Reset()
{
    index = int32();
    world_seconds = float();
    source_entity_id = int64();
    target_entity_id = int64();
    message_type_id = int64();
    message_body = TArray<uint8>();    
}

void FPbBattleHistoryRecord::operator=(const idlepb::BattleHistoryRecord& Right)
{
    this->FromPb(Right);
}

bool FPbBattleHistoryRecord::operator==(const FPbBattleHistoryRecord& Right) const
{
    if (this->index != Right.index)
        return false;
    if (this->world_seconds != Right.world_seconds)
        return false;
    if (this->source_entity_id != Right.source_entity_id)
        return false;
    if (this->target_entity_id != Right.target_entity_id)
        return false;
    if (this->message_type_id != Right.message_type_id)
        return false;
    if (this->message_body != Right.message_body)
        return false;
    return true;
}

bool FPbBattleHistoryRecord::operator!=(const FPbBattleHistoryRecord& Right) const
{
    return !operator==(Right);
}

FPbBattleRoleInfo::FPbBattleRoleInfo()
{
    Reset();        
}

FPbBattleRoleInfo::FPbBattleRoleInfo(const idlepb::BattleRoleInfo& Right)
{
    this->FromPb(Right);
}

void FPbBattleRoleInfo::FromPb(const idlepb::BattleRoleInfo& Right)
{
    role_id = Right.role_id();
    role_name = UTF8_TO_TCHAR(Right.role_name().c_str());
    zone_id = Right.zone_id();
    score_delta = Right.score_delta();
    score = Right.score();
    rank_delta = Right.rank_delta();
    rank = Right.rank();
    combat_power = Right.combat_power();
    model_config = Right.model_config();
    cultivation_main_dir = static_cast<EPbCultivationDirection>(Right.cultivation_main_dir());
    cultivation_main_rank = Right.cultivation_main_rank();
    npc_cfg_id = Right.npc_cfg_id();
}

void FPbBattleRoleInfo::ToPb(idlepb::BattleRoleInfo* Out) const
{
    Out->set_role_id(role_id);
    Out->set_role_name(TCHAR_TO_UTF8(*role_name));
    Out->set_zone_id(zone_id);
    Out->set_score_delta(score_delta);
    Out->set_score(score);
    Out->set_rank_delta(rank_delta);
    Out->set_rank(rank);
    Out->set_combat_power(combat_power);
    model_config.ToPb(Out->mutable_model_config());
    Out->set_cultivation_main_dir(static_cast<idlepb::CultivationDirection>(cultivation_main_dir));
    Out->set_cultivation_main_rank(cultivation_main_rank);
    Out->set_npc_cfg_id(npc_cfg_id);    
}

void FPbBattleRoleInfo::Reset()
{
    role_id = int64();
    role_name = FString();
    zone_id = int32();
    score_delta = int32();
    score = int32();
    rank_delta = int32();
    rank = int32();
    combat_power = int64();
    model_config = FPbCharacterModelConfig();
    cultivation_main_dir = EPbCultivationDirection();
    cultivation_main_rank = int32();
    npc_cfg_id = int32();    
}

void FPbBattleRoleInfo::operator=(const idlepb::BattleRoleInfo& Right)
{
    this->FromPb(Right);
}

bool FPbBattleRoleInfo::operator==(const FPbBattleRoleInfo& Right) const
{
    if (this->role_id != Right.role_id)
        return false;
    if (this->role_name != Right.role_name)
        return false;
    if (this->zone_id != Right.zone_id)
        return false;
    if (this->score_delta != Right.score_delta)
        return false;
    if (this->score != Right.score)
        return false;
    if (this->rank_delta != Right.rank_delta)
        return false;
    if (this->rank != Right.rank)
        return false;
    if (this->combat_power != Right.combat_power)
        return false;
    if (this->model_config != Right.model_config)
        return false;
    if (this->cultivation_main_dir != Right.cultivation_main_dir)
        return false;
    if (this->cultivation_main_rank != Right.cultivation_main_rank)
        return false;
    if (this->npc_cfg_id != Right.npc_cfg_id)
        return false;
    return true;
}

bool FPbBattleRoleInfo::operator!=(const FPbBattleRoleInfo& Right) const
{
    return !operator==(Right);
}

FPbBattleInfo::FPbBattleInfo()
{
    Reset();        
}

FPbBattleInfo::FPbBattleInfo(const idlepb::BattleInfo& Right)
{
    this->FromPb(Right);
}

void FPbBattleInfo::FromPb(const idlepb::BattleInfo& Right)
{
    world_id = Right.world_id();
    attacker_win = Right.attacker_win();
    attacker = Right.attacker();
    defender = Right.defender();
    begin_ticks = Right.begin_ticks();
    end_ticks = Right.end_ticks();
    solo_type = static_cast<EPbSoloType>(Right.solo_type());
}

void FPbBattleInfo::ToPb(idlepb::BattleInfo* Out) const
{
    Out->set_world_id(world_id);
    Out->set_attacker_win(attacker_win);
    attacker.ToPb(Out->mutable_attacker());
    defender.ToPb(Out->mutable_defender());
    Out->set_begin_ticks(begin_ticks);
    Out->set_end_ticks(end_ticks);
    Out->set_solo_type(static_cast<idlepb::SoloType>(solo_type));    
}

void FPbBattleInfo::Reset()
{
    world_id = int64();
    attacker_win = bool();
    attacker = FPbBattleRoleInfo();
    defender = FPbBattleRoleInfo();
    begin_ticks = int64();
    end_ticks = int64();
    solo_type = EPbSoloType();    
}

void FPbBattleInfo::operator=(const idlepb::BattleInfo& Right)
{
    this->FromPb(Right);
}

bool FPbBattleInfo::operator==(const FPbBattleInfo& Right) const
{
    if (this->world_id != Right.world_id)
        return false;
    if (this->attacker_win != Right.attacker_win)
        return false;
    if (this->attacker != Right.attacker)
        return false;
    if (this->defender != Right.defender)
        return false;
    if (this->begin_ticks != Right.begin_ticks)
        return false;
    if (this->end_ticks != Right.end_ticks)
        return false;
    if (this->solo_type != Right.solo_type)
        return false;
    return true;
}

bool FPbBattleInfo::operator!=(const FPbBattleInfo& Right) const
{
    return !operator==(Right);
}

FPbBattleHistory::FPbBattleHistory()
{
    Reset();        
}

FPbBattleHistory::FPbBattleHistory(const idlepb::BattleHistory& Right)
{
    this->FromPb(Right);
}

void FPbBattleHistory::FromPb(const idlepb::BattleHistory& Right)
{
    info = Right.info();
    records.Empty();
    for (const auto& Elem : Right.records())
    {
        records.Emplace(Elem);
    }
}

void FPbBattleHistory::ToPb(idlepb::BattleHistory* Out) const
{
    info.ToPb(Out->mutable_info());
    for (const auto& Elem : records)
    {
        Elem.ToPb(Out->add_records());    
    }    
}

void FPbBattleHistory::Reset()
{
    info = FPbBattleInfo();
    records = TArray<FPbBattleHistoryRecord>();    
}

void FPbBattleHistory::operator=(const idlepb::BattleHistory& Right)
{
    this->FromPb(Right);
}

bool FPbBattleHistory::operator==(const FPbBattleHistory& Right) const
{
    if (this->info != Right.info)
        return false;
    if (this->records != Right.records)
        return false;
    return true;
}

bool FPbBattleHistory::operator!=(const FPbBattleHistory& Right) const
{
    return !operator==(Right);
}

FPbRoleBattleInfo::FPbRoleBattleInfo()
{
    Reset();        
}

FPbRoleBattleInfo::FPbRoleBattleInfo(const idlepb::RoleBattleInfo& Right)
{
    this->FromPb(Right);
}

void FPbRoleBattleInfo::FromPb(const idlepb::RoleBattleInfo& Right)
{
    base = Right.base();
    can_revenge = Right.can_revenge();
    round_num = Right.round_num();
}

void FPbRoleBattleInfo::ToPb(idlepb::RoleBattleInfo* Out) const
{
    base.ToPb(Out->mutable_base());
    Out->set_can_revenge(can_revenge);
    Out->set_round_num(round_num);    
}

void FPbRoleBattleInfo::Reset()
{
    base = FPbBattleInfo();
    can_revenge = bool();
    round_num = int32();    
}

void FPbRoleBattleInfo::operator=(const idlepb::RoleBattleInfo& Right)
{
    this->FromPb(Right);
}

bool FPbRoleBattleInfo::operator==(const FPbRoleBattleInfo& Right) const
{
    if (this->base != Right.base)
        return false;
    if (this->can_revenge != Right.can_revenge)
        return false;
    if (this->round_num != Right.round_num)
        return false;
    return true;
}

bool FPbRoleBattleInfo::operator!=(const FPbRoleBattleInfo& Right) const
{
    return !operator==(Right);
}

FPbRoleBattleHistorySaveData::FPbRoleBattleHistorySaveData()
{
    Reset();        
}

FPbRoleBattleHistorySaveData::FPbRoleBattleHistorySaveData(const idlepb::RoleBattleHistorySaveData& Right)
{
    this->FromPb(Right);
}

void FPbRoleBattleHistorySaveData::FromPb(const idlepb::RoleBattleHistorySaveData& Right)
{
    histories.Empty();
    for (const auto& Elem : Right.histories())
    {
        histories.Emplace(Elem);
    }
}

void FPbRoleBattleHistorySaveData::ToPb(idlepb::RoleBattleHistorySaveData* Out) const
{
    for (const auto& Elem : histories)
    {
        Elem.ToPb(Out->add_histories());    
    }    
}

void FPbRoleBattleHistorySaveData::Reset()
{
    histories = TArray<FPbRoleBattleInfo>();    
}

void FPbRoleBattleHistorySaveData::operator=(const idlepb::RoleBattleHistorySaveData& Right)
{
    this->FromPb(Right);
}

bool FPbRoleBattleHistorySaveData::operator==(const FPbRoleBattleHistorySaveData& Right) const
{
    if (this->histories != Right.histories)
        return false;
    return true;
}

bool FPbRoleBattleHistorySaveData::operator!=(const FPbRoleBattleHistorySaveData& Right) const
{
    return !operator==(Right);
}

FPbCompressedData::FPbCompressedData()
{
    Reset();        
}

FPbCompressedData::FPbCompressedData(const idlepb::CompressedData& Right)
{
    this->FromPb(Right);
}

void FPbCompressedData::FromPb(const idlepb::CompressedData& Right)
{
    original_size = Right.original_size();
    compressed_data.Empty(compressed_data.Num());
    compressed_data.Append(reinterpret_cast<const uint8*>(Right.compressed_data().c_str()), Right.compressed_data().size());
}

void FPbCompressedData::ToPb(idlepb::CompressedData* Out) const
{
    Out->set_original_size(original_size);
    Out->set_compressed_data(compressed_data.GetData(), compressed_data.Num());    
}

void FPbCompressedData::Reset()
{
    original_size = int32();
    compressed_data = TArray<uint8>();    
}

void FPbCompressedData::operator=(const idlepb::CompressedData& Right)
{
    this->FromPb(Right);
}

bool FPbCompressedData::operator==(const FPbCompressedData& Right) const
{
    if (this->original_size != Right.original_size)
        return false;
    if (this->compressed_data != Right.compressed_data)
        return false;
    return true;
}

bool FPbCompressedData::operator!=(const FPbCompressedData& Right) const
{
    return !operator==(Right);
}

FPbDoBreathingExerciseResult::FPbDoBreathingExerciseResult()
{
    Reset();        
}

FPbDoBreathingExerciseResult::FPbDoBreathingExerciseResult(const idlepb::DoBreathingExerciseResult& Right)
{
    this->FromPb(Right);
}

void FPbDoBreathingExerciseResult::FromPb(const idlepb::DoBreathingExerciseResult& Right)
{
    ok = Right.ok();
    perfect = Right.perfect();
    exp = Right.exp();
    rate = Right.rate();
}

void FPbDoBreathingExerciseResult::ToPb(idlepb::DoBreathingExerciseResult* Out) const
{
    Out->set_ok(ok);
    Out->set_perfect(perfect);
    Out->set_exp(exp);
    Out->set_rate(rate);    
}

void FPbDoBreathingExerciseResult::Reset()
{
    ok = bool();
    perfect = bool();
    exp = float();
    rate = int32();    
}

void FPbDoBreathingExerciseResult::operator=(const idlepb::DoBreathingExerciseResult& Right)
{
    this->FromPb(Right);
}

bool FPbDoBreathingExerciseResult::operator==(const FPbDoBreathingExerciseResult& Right) const
{
    if (this->ok != Right.ok)
        return false;
    if (this->perfect != Right.perfect)
        return false;
    if (this->exp != Right.exp)
        return false;
    if (this->rate != Right.rate)
        return false;
    return true;
}

bool FPbDoBreathingExerciseResult::operator!=(const FPbDoBreathingExerciseResult& Right) const
{
    return !operator==(Right);
}

bool CheckEPbLoginGameRetCodeValid(int32 Val)
{
    return idlepb::LoginGameRetCode_IsValid(Val);
}

const TCHAR* GetEPbLoginGameRetCodeDescription(EPbLoginGameRetCode Val)
{
    switch (Val)
    {
        case EPbLoginGameRetCode::LoginGameRetCode_Ok: return TEXT("正常登陆");
        case EPbLoginGameRetCode::LoginGameRetCode_Unknown: return TEXT("未知错误");
        case EPbLoginGameRetCode::LoginGameRetCode_NoRole: return TEXT("没有角色");
        case EPbLoginGameRetCode::LoginGameRetCode_DuplicateLogin: return TEXT("已经在线");
        case EPbLoginGameRetCode::LoginGameRetCode_AccountInvalid: return TEXT("帐号非法");
        case EPbLoginGameRetCode::LoginGameRetCode_VersionError: return TEXT("版本错误");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbGotoTypeValid(int32 Val)
{
    return idlepb::GotoType_IsValid(Val);
}

const TCHAR* GetEPbGotoTypeDescription(EPbGotoType Val)
{
    switch (Val)
    {
        case EPbGotoType::GotoType_None: return TEXT("未知类型");
        case EPbGotoType::GotoType_Relive: return TEXT("复活");
        case EPbGotoType::GotoType_Teleport: return TEXT("传送");
    }
    return TEXT("UNKNOWN");
}

FPbSystemNoticeParams::FPbSystemNoticeParams()
{
    Reset();        
}

FPbSystemNoticeParams::FPbSystemNoticeParams(const idlepb::SystemNoticeParams& Right)
{
    this->FromPb(Right);
}

void FPbSystemNoticeParams::FromPb(const idlepb::SystemNoticeParams& Right)
{
    s1 = UTF8_TO_TCHAR(Right.s1().c_str());
    s2 = UTF8_TO_TCHAR(Right.s2().c_str());
    s3 = UTF8_TO_TCHAR(Right.s3().c_str());
    s4 = UTF8_TO_TCHAR(Right.s4().c_str());
    n1 = Right.n1();
    n2 = Right.n2();
    n3 = Right.n3();
    n4 = Right.n4();
}

void FPbSystemNoticeParams::ToPb(idlepb::SystemNoticeParams* Out) const
{
    Out->set_s1(TCHAR_TO_UTF8(*s1));
    Out->set_s2(TCHAR_TO_UTF8(*s2));
    Out->set_s3(TCHAR_TO_UTF8(*s3));
    Out->set_s4(TCHAR_TO_UTF8(*s4));
    Out->set_n1(n1);
    Out->set_n2(n2);
    Out->set_n3(n3);
    Out->set_n4(n4);    
}

void FPbSystemNoticeParams::Reset()
{
    s1 = FString();
    s2 = FString();
    s3 = FString();
    s4 = FString();
    n1 = int64();
    n2 = int64();
    n3 = int64();
    n4 = int64();    
}

void FPbSystemNoticeParams::operator=(const idlepb::SystemNoticeParams& Right)
{
    this->FromPb(Right);
}

bool FPbSystemNoticeParams::operator==(const FPbSystemNoticeParams& Right) const
{
    if (this->s1 != Right.s1)
        return false;
    if (this->s2 != Right.s2)
        return false;
    if (this->s3 != Right.s3)
        return false;
    if (this->s4 != Right.s4)
        return false;
    if (this->n1 != Right.n1)
        return false;
    if (this->n2 != Right.n2)
        return false;
    if (this->n3 != Right.n3)
        return false;
    if (this->n4 != Right.n4)
        return false;
    return true;
}

bool FPbSystemNoticeParams::operator!=(const FPbSystemNoticeParams& Right) const
{
    return !operator==(Right);
}

FPbDropItem::FPbDropItem()
{
    Reset();        
}

FPbDropItem::FPbDropItem(const idlepb::DropItem& Right)
{
    this->FromPb(Right);
}

void FPbDropItem::FromPb(const idlepb::DropItem& Right)
{
    item_id = Right.item_id();
    item_num = Right.item_num();
}

void FPbDropItem::ToPb(idlepb::DropItem* Out) const
{
    Out->set_item_id(item_id);
    Out->set_item_num(item_num);    
}

void FPbDropItem::Reset()
{
    item_id = int32();
    item_num = int32();    
}

void FPbDropItem::operator=(const idlepb::DropItem& Right)
{
    this->FromPb(Right);
}

bool FPbDropItem::operator==(const FPbDropItem& Right) const
{
    if (this->item_id != Right.item_id)
        return false;
    if (this->item_num != Right.item_num)
        return false;
    return true;
}

bool FPbDropItem::operator!=(const FPbDropItem& Right) const
{
    return !operator==(Right);
}

bool CheckEPbTravelWorldTypeValid(int32 Val)
{
    return idlepb::TravelWorldType_IsValid(Val);
}

const TCHAR* GetEPbTravelWorldTypeDescription(EPbTravelWorldType Val)
{
    switch (Val)
    {
        case EPbTravelWorldType::TravelWorldType_Normal: return TEXT("普通");
        case EPbTravelWorldType::TravelWorldType_Force: return TEXT("强制传送(客户端必须切换关卡)");
        case EPbTravelWorldType::TravelWorldType_ClientNoOpen: return TEXT("客户端不要切换关卡");
        case EPbTravelWorldType::TravelWorldType_ClientCityNoOpen: return TEXT("客户端在主城的话就不切换，其它强制切换");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbBiographyEventTypeValid(int32 Val)
{
    return idlepb::BiographyEventType_IsValid(Val);
}

const TCHAR* GetEPbBiographyEventTypeDescription(EPbBiographyEventType Val)
{
    switch (Val)
    {
        case EPbBiographyEventType::BET_Degree: return TEXT("境界排名");
        case EPbBiographyEventType::BET_FullDegree: return TEXT("境界圆满排名");
        case EPbBiographyEventType::BET_KillMonster: return TEXT("击杀妖兽数量");
        case EPbBiographyEventType::BET_SeptDonation: return TEXT("限时宗门建设值");
        case EPbBiographyEventType::BET_CombatPower: return TEXT("限时玩家战力排行");
        case EPbBiographyEventType::BET_ImmortalRoad: return TEXT("飞升之路");
    }
    return TEXT("UNKNOWN");
}

FPbBiographyEventLeaderboardItem::FPbBiographyEventLeaderboardItem()
{
    Reset();        
}

FPbBiographyEventLeaderboardItem::FPbBiographyEventLeaderboardItem(const idlepb::BiographyEventLeaderboardItem& Right)
{
    this->FromPb(Right);
}

void FPbBiographyEventLeaderboardItem::FromPb(const idlepb::BiographyEventLeaderboardItem& Right)
{
    uid = Right.uid();
    name = UTF8_TO_TCHAR(Right.name().c_str());
    param_d1 = Right.param_d1();
    param_n1 = Right.param_n1();
    params_n1.Empty();
    for (const auto& Elem : Right.params_n1())
    {
        params_n1.Emplace(Elem);
    }
    params_d1.Empty();
    for (const auto& Elem : Right.params_d1())
    {
        params_d1.Emplace(Elem);
    }
}

void FPbBiographyEventLeaderboardItem::ToPb(idlepb::BiographyEventLeaderboardItem* Out) const
{
    Out->set_uid(uid);
    Out->set_name(TCHAR_TO_UTF8(*name));
    Out->set_param_d1(param_d1);
    Out->set_param_n1(param_n1);
    for (const auto& Elem : params_n1)
    {
        Out->add_params_n1(Elem);    
    }
    for (const auto& Elem : params_d1)
    {
        Out->add_params_d1(Elem);    
    }    
}

void FPbBiographyEventLeaderboardItem::Reset()
{
    uid = int64();
    name = FString();
    param_d1 = int32();
    param_n1 = int64();
    params_n1 = TArray<int64>();
    params_d1 = TArray<int32>();    
}

void FPbBiographyEventLeaderboardItem::operator=(const idlepb::BiographyEventLeaderboardItem& Right)
{
    this->FromPb(Right);
}

bool FPbBiographyEventLeaderboardItem::operator==(const FPbBiographyEventLeaderboardItem& Right) const
{
    if (this->uid != Right.uid)
        return false;
    if (this->name != Right.name)
        return false;
    if (this->param_d1 != Right.param_d1)
        return false;
    if (this->param_n1 != Right.param_n1)
        return false;
    if (this->params_n1 != Right.params_n1)
        return false;
    if (this->params_d1 != Right.params_d1)
        return false;
    return true;
}

bool FPbBiographyEventLeaderboardItem::operator!=(const FPbBiographyEventLeaderboardItem& Right) const
{
    return !operator==(Right);
}

FPbBiographyEventLeaderboardList::FPbBiographyEventLeaderboardList()
{
    Reset();        
}

FPbBiographyEventLeaderboardList::FPbBiographyEventLeaderboardList(const idlepb::BiographyEventLeaderboardList& Right)
{
    this->FromPb(Right);
}

void FPbBiographyEventLeaderboardList::FromPb(const idlepb::BiographyEventLeaderboardList& Right)
{
    list_data.Empty();
    for (const auto& Elem : Right.list_data())
    {
        list_data.Emplace(Elem);
    }
    cfg_id = Right.cfg_id();
    begin_date = Right.begin_date();
    finished = Right.finished();
}

void FPbBiographyEventLeaderboardList::ToPb(idlepb::BiographyEventLeaderboardList* Out) const
{
    for (const auto& Elem : list_data)
    {
        Elem.ToPb(Out->add_list_data());    
    }
    Out->set_cfg_id(cfg_id);
    Out->set_begin_date(begin_date);
    Out->set_finished(finished);    
}

void FPbBiographyEventLeaderboardList::Reset()
{
    list_data = TArray<FPbBiographyEventLeaderboardItem>();
    cfg_id = int32();
    begin_date = int64();
    finished = bool();    
}

void FPbBiographyEventLeaderboardList::operator=(const idlepb::BiographyEventLeaderboardList& Right)
{
    this->FromPb(Right);
}

bool FPbBiographyEventLeaderboardList::operator==(const FPbBiographyEventLeaderboardList& Right) const
{
    if (this->list_data != Right.list_data)
        return false;
    if (this->cfg_id != Right.cfg_id)
        return false;
    if (this->begin_date != Right.begin_date)
        return false;
    if (this->finished != Right.finished)
        return false;
    return true;
}

bool FPbBiographyEventLeaderboardList::operator!=(const FPbBiographyEventLeaderboardList& Right) const
{
    return !operator==(Right);
}

FPbLeaderboardListItem::FPbLeaderboardListItem()
{
    Reset();        
}

FPbLeaderboardListItem::FPbLeaderboardListItem(const idlepb::LeaderboardListItem& Right)
{
    this->FromPb(Right);
}

void FPbLeaderboardListItem::FromPb(const idlepb::LeaderboardListItem& Right)
{
    role_id = Right.role_id();
    user_name = UTF8_TO_TCHAR(Right.user_name().c_str());
    property_num = Right.property_num();
    time = Right.time();
    d1 = Right.d1();
}

void FPbLeaderboardListItem::ToPb(idlepb::LeaderboardListItem* Out) const
{
    Out->set_role_id(role_id);
    Out->set_user_name(TCHAR_TO_UTF8(*user_name));
    Out->set_property_num(property_num);
    Out->set_time(time);
    Out->set_d1(d1);    
}

void FPbLeaderboardListItem::Reset()
{
    role_id = int64();
    user_name = FString();
    property_num = int64();
    time = int64();
    d1 = int32();    
}

void FPbLeaderboardListItem::operator=(const idlepb::LeaderboardListItem& Right)
{
    this->FromPb(Right);
}

bool FPbLeaderboardListItem::operator==(const FPbLeaderboardListItem& Right) const
{
    if (this->role_id != Right.role_id)
        return false;
    if (this->user_name != Right.user_name)
        return false;
    if (this->property_num != Right.property_num)
        return false;
    if (this->time != Right.time)
        return false;
    if (this->d1 != Right.d1)
        return false;
    return true;
}

bool FPbLeaderboardListItem::operator!=(const FPbLeaderboardListItem& Right) const
{
    return !operator==(Right);
}

FPbSeptDataOnLeaderboard::FPbSeptDataOnLeaderboard()
{
    Reset();        
}

FPbSeptDataOnLeaderboard::FPbSeptDataOnLeaderboard(const idlepb::SeptDataOnLeaderboard& Right)
{
    this->FromPb(Right);
}

void FPbSeptDataOnLeaderboard::FromPb(const idlepb::SeptDataOnLeaderboard& Right)
{
    sept_id = Right.sept_id();
    sept_name = UTF8_TO_TCHAR(Right.sept_name().c_str());
    logo_index = Right.logo_index();
    property_num = Right.property_num();
}

void FPbSeptDataOnLeaderboard::ToPb(idlepb::SeptDataOnLeaderboard* Out) const
{
    Out->set_sept_id(sept_id);
    Out->set_sept_name(TCHAR_TO_UTF8(*sept_name));
    Out->set_logo_index(logo_index);
    Out->set_property_num(property_num);    
}

void FPbSeptDataOnLeaderboard::Reset()
{
    sept_id = int64();
    sept_name = FString();
    logo_index = int32();
    property_num = int64();    
}

void FPbSeptDataOnLeaderboard::operator=(const idlepb::SeptDataOnLeaderboard& Right)
{
    this->FromPb(Right);
}

bool FPbSeptDataOnLeaderboard::operator==(const FPbSeptDataOnLeaderboard& Right) const
{
    if (this->sept_id != Right.sept_id)
        return false;
    if (this->sept_name != Right.sept_name)
        return false;
    if (this->logo_index != Right.logo_index)
        return false;
    if (this->property_num != Right.property_num)
        return false;
    return true;
}

bool FPbSeptDataOnLeaderboard::operator!=(const FPbSeptDataOnLeaderboard& Right) const
{
    return !operator==(Right);
}

FPbLeaderboardList::FPbLeaderboardList()
{
    Reset();        
}

FPbLeaderboardList::FPbLeaderboardList(const idlepb::LeaderboardList& Right)
{
    this->FromPb(Right);
}

void FPbLeaderboardList::FromPb(const idlepb::LeaderboardList& Right)
{
    list_data.Empty();
    for (const auto& Elem : Right.list_data())
    {
        list_data.Emplace(Elem);
    }
    type_id = Right.type_id();
}

void FPbLeaderboardList::ToPb(idlepb::LeaderboardList* Out) const
{
    for (const auto& Elem : list_data)
    {
        Elem.ToPb(Out->add_list_data());    
    }
    Out->set_type_id(type_id);    
}

void FPbLeaderboardList::Reset()
{
    list_data = TArray<FPbLeaderboardListItem>();
    type_id = int32();    
}

void FPbLeaderboardList::operator=(const idlepb::LeaderboardList& Right)
{
    this->FromPb(Right);
}

bool FPbLeaderboardList::operator==(const FPbLeaderboardList& Right) const
{
    if (this->list_data != Right.list_data)
        return false;
    if (this->type_id != Right.type_id)
        return false;
    return true;
}

bool FPbLeaderboardList::operator!=(const FPbLeaderboardList& Right) const
{
    return !operator==(Right);
}

FPbLeaderboardSaveData::FPbLeaderboardSaveData()
{
    Reset();        
}

FPbLeaderboardSaveData::FPbLeaderboardSaveData(const idlepb::LeaderboardSaveData& Right)
{
    this->FromPb(Right);
}

void FPbLeaderboardSaveData::FromPb(const idlepb::LeaderboardSaveData& Right)
{
    lists_data.Empty();
    for (const auto& Elem : Right.lists_data())
    {
        lists_data.Emplace(Elem);
    }
    sept_list.Empty();
    for (const auto& Elem : Right.sept_list())
    {
        sept_list.Emplace(Elem);
    }
    monster_tower_challange.Empty();
    for (const auto& Elem : Right.monster_tower_challange())
    {
        monster_tower_challange.Emplace(Elem);
    }
    last_reset_week_time = Right.last_reset_week_time();
    last_reset_day_time = Right.last_reset_day_time();
    biography_lists.Empty();
    for (const auto& Elem : Right.biography_lists())
    {
        biography_lists.Emplace(Elem);
    }
    fuze_exp = Right.fuze_exp();
    fuze_mail_list.Empty();
    for (const auto& Elem : Right.fuze_mail_list())
    {
        fuze_mail_list.Emplace(Elem);
    }
}

void FPbLeaderboardSaveData::ToPb(idlepb::LeaderboardSaveData* Out) const
{
    for (const auto& Elem : lists_data)
    {
        Elem.ToPb(Out->add_lists_data());    
    }
    for (const auto& Elem : sept_list)
    {
        Elem.ToPb(Out->add_sept_list());    
    }
    for (const auto& Elem : monster_tower_challange)
    {
        Elem.ToPb(Out->add_monster_tower_challange());    
    }
    Out->set_last_reset_week_time(last_reset_week_time);
    Out->set_last_reset_day_time(last_reset_day_time);
    for (const auto& Elem : biography_lists)
    {
        Elem.ToPb(Out->add_biography_lists());    
    }
    Out->set_fuze_exp(fuze_exp);
    for (const auto& Elem : fuze_mail_list)
    {
        Out->add_fuze_mail_list(Elem);    
    }    
}

void FPbLeaderboardSaveData::Reset()
{
    lists_data = TArray<FPbLeaderboardList>();
    sept_list = TArray<FPbSeptDataOnLeaderboard>();
    monster_tower_challange = TArray<FPbLeaderboardList>();
    last_reset_week_time = int64();
    last_reset_day_time = int64();
    biography_lists = TArray<FPbBiographyEventLeaderboardList>();
    fuze_exp = int64();
    fuze_mail_list = TArray<int64>();    
}

void FPbLeaderboardSaveData::operator=(const idlepb::LeaderboardSaveData& Right)
{
    this->FromPb(Right);
}

bool FPbLeaderboardSaveData::operator==(const FPbLeaderboardSaveData& Right) const
{
    if (this->lists_data != Right.lists_data)
        return false;
    if (this->sept_list != Right.sept_list)
        return false;
    if (this->monster_tower_challange != Right.monster_tower_challange)
        return false;
    if (this->last_reset_week_time != Right.last_reset_week_time)
        return false;
    if (this->last_reset_day_time != Right.last_reset_day_time)
        return false;
    if (this->biography_lists != Right.biography_lists)
        return false;
    if (this->fuze_exp != Right.fuze_exp)
        return false;
    if (this->fuze_mail_list != Right.fuze_mail_list)
        return false;
    return true;
}

bool FPbLeaderboardSaveData::operator!=(const FPbLeaderboardSaveData& Right) const
{
    return !operator==(Right);
}

bool CheckEPbRoleDirtyFlagValid(int32 Val)
{
    return idlepb::RoleDirtyFlag_IsValid(Val);
}

const TCHAR* GetEPbRoleDirtyFlagDescription(EPbRoleDirtyFlag Val)
{
    switch (Val)
    {
        case EPbRoleDirtyFlag::RoleDirtyFlag_Save: return TEXT("存档");
    }
    return TEXT("UNKNOWN");
}

FPbSeptDemonDamageHistoryEntry::FPbSeptDemonDamageHistoryEntry()
{
    Reset();        
}

FPbSeptDemonDamageHistoryEntry::FPbSeptDemonDamageHistoryEntry(const idlepb::SeptDemonDamageHistoryEntry& Right)
{
    this->FromPb(Right);
}

void FPbSeptDemonDamageHistoryEntry::FromPb(const idlepb::SeptDemonDamageHistoryEntry& Right)
{
    role_id = Right.role_id();
    role_name = UTF8_TO_TCHAR(Right.role_name().c_str());
    total_damage = Right.total_damage();
    rank = Right.rank();
    role_model = Right.role_model();
}

void FPbSeptDemonDamageHistoryEntry::ToPb(idlepb::SeptDemonDamageHistoryEntry* Out) const
{
    Out->set_role_id(role_id);
    Out->set_role_name(TCHAR_TO_UTF8(*role_name));
    Out->set_total_damage(total_damage);
    Out->set_rank(rank);
    role_model.ToPb(Out->mutable_role_model());    
}

void FPbSeptDemonDamageHistoryEntry::Reset()
{
    role_id = int64();
    role_name = FString();
    total_damage = float();
    rank = int32();
    role_model = FPbCharacterModelConfig();    
}

void FPbSeptDemonDamageHistoryEntry::operator=(const idlepb::SeptDemonDamageHistoryEntry& Right)
{
    this->FromPb(Right);
}

bool FPbSeptDemonDamageHistoryEntry::operator==(const FPbSeptDemonDamageHistoryEntry& Right) const
{
    if (this->role_id != Right.role_id)
        return false;
    if (this->role_name != Right.role_name)
        return false;
    if (this->total_damage != Right.total_damage)
        return false;
    if (this->rank != Right.rank)
        return false;
    if (this->role_model != Right.role_model)
        return false;
    return true;
}

bool FPbSeptDemonDamageHistoryEntry::operator!=(const FPbSeptDemonDamageHistoryEntry& Right) const
{
    return !operator==(Right);
}

FPbSeptDemonDamageHistoryData::FPbSeptDemonDamageHistoryData()
{
    Reset();        
}

FPbSeptDemonDamageHistoryData::FPbSeptDemonDamageHistoryData(const idlepb::SeptDemonDamageHistoryData& Right)
{
    this->FromPb(Right);
}

void FPbSeptDemonDamageHistoryData::FromPb(const idlepb::SeptDemonDamageHistoryData& Right)
{
    all_entries.Empty();
    for (const auto& Elem : Right.all_entries())
    {
        all_entries.Emplace(Elem);
    }
}

void FPbSeptDemonDamageHistoryData::ToPb(idlepb::SeptDemonDamageHistoryData* Out) const
{
    for (const auto& Elem : all_entries)
    {
        Elem.ToPb(Out->add_all_entries());    
    }    
}

void FPbSeptDemonDamageHistoryData::Reset()
{
    all_entries = TArray<FPbSeptDemonDamageHistoryEntry>();    
}

void FPbSeptDemonDamageHistoryData::operator=(const idlepb::SeptDemonDamageHistoryData& Right)
{
    this->FromPb(Right);
}

bool FPbSeptDemonDamageHistoryData::operator==(const FPbSeptDemonDamageHistoryData& Right) const
{
    if (this->all_entries != Right.all_entries)
        return false;
    return true;
}

bool FPbSeptDemonDamageHistoryData::operator!=(const FPbSeptDemonDamageHistoryData& Right) const
{
    return !operator==(Right);
}

FPbSelfSeptInfo::FPbSelfSeptInfo()
{
    Reset();        
}

FPbSelfSeptInfo::FPbSelfSeptInfo(const idlepb::SelfSeptInfo& Right)
{
    this->FromPb(Right);
}

void FPbSelfSeptInfo::FromPb(const idlepb::SelfSeptInfo& Right)
{
    sept_id = Right.sept_id();
    sept_name = UTF8_TO_TCHAR(Right.sept_name().c_str());
    sept_position = static_cast<EPbSeptPosition>(Right.sept_position());
    next_join_ticks = Right.next_join_ticks();
    land_fighting = Right.land_fighting();
}

void FPbSelfSeptInfo::ToPb(idlepb::SelfSeptInfo* Out) const
{
    Out->set_sept_id(sept_id);
    Out->set_sept_name(TCHAR_TO_UTF8(*sept_name));
    Out->set_sept_position(static_cast<idlepb::SeptPosition>(sept_position));
    Out->set_next_join_ticks(next_join_ticks);
    Out->set_land_fighting(land_fighting);    
}

void FPbSelfSeptInfo::Reset()
{
    sept_id = int64();
    sept_name = FString();
    sept_position = EPbSeptPosition();
    next_join_ticks = int64();
    land_fighting = bool();    
}

void FPbSelfSeptInfo::operator=(const idlepb::SelfSeptInfo& Right)
{
    this->FromPb(Right);
}

bool FPbSelfSeptInfo::operator==(const FPbSelfSeptInfo& Right) const
{
    if (this->sept_id != Right.sept_id)
        return false;
    if (this->sept_name != Right.sept_name)
        return false;
    if (this->sept_position != Right.sept_position)
        return false;
    if (this->next_join_ticks != Right.next_join_ticks)
        return false;
    if (this->land_fighting != Right.land_fighting)
        return false;
    return true;
}

bool FPbSelfSeptInfo::operator!=(const FPbSelfSeptInfo& Right) const
{
    return !operator==(Right);
}

FPbCreatePlayerParams::FPbCreatePlayerParams()
{
    Reset();        
}

FPbCreatePlayerParams::FPbCreatePlayerParams(const idlepb::CreatePlayerParams& Right)
{
    this->FromPb(Right);
}

void FPbCreatePlayerParams::FromPb(const idlepb::CreatePlayerParams& Right)
{
    role_id = Right.role_id();
    role_name = UTF8_TO_TCHAR(Right.role_name().c_str());
    physics_rank_data = Right.physics_rank_data();
    magic_rank_data = Right.magic_rank_data();
    model_config = Right.model_config();
    ability_data = Right.ability_data();
    fight_mode = static_cast<EPbFightMode>(Right.fight_mode());
    is_dummy = Right.is_dummy();
    normal_settings = Right.normal_settings();
    self_sept_info = Right.self_sept_info();
    combat_power = Right.combat_power();
    all_stats_data = Right.all_stats_data();
}

void FPbCreatePlayerParams::ToPb(idlepb::CreatePlayerParams* Out) const
{
    Out->set_role_id(role_id);
    Out->set_role_name(TCHAR_TO_UTF8(*role_name));
    physics_rank_data.ToPb(Out->mutable_physics_rank_data());
    magic_rank_data.ToPb(Out->mutable_magic_rank_data());
    model_config.ToPb(Out->mutable_model_config());
    ability_data.ToPb(Out->mutable_ability_data());
    Out->set_fight_mode(static_cast<idlepb::FightMode>(fight_mode));
    Out->set_is_dummy(is_dummy);
    normal_settings.ToPb(Out->mutable_normal_settings());
    self_sept_info.ToPb(Out->mutable_self_sept_info());
    Out->set_combat_power(combat_power);
    all_stats_data.ToPb(Out->mutable_all_stats_data());    
}

void FPbCreatePlayerParams::Reset()
{
    role_id = int64();
    role_name = FString();
    physics_rank_data = FPbRankData();
    magic_rank_data = FPbRankData();
    model_config = FPbCharacterModelConfig();
    ability_data = FPbPlayerAbilityData();
    fight_mode = EPbFightMode();
    is_dummy = bool();
    normal_settings = FPbRoleNormalSettings();
    self_sept_info = FPbSelfSeptInfo();
    combat_power = int64();
    all_stats_data = FPbGameStatsAllModuleData();    
}

void FPbCreatePlayerParams::operator=(const idlepb::CreatePlayerParams& Right)
{
    this->FromPb(Right);
}

bool FPbCreatePlayerParams::operator==(const FPbCreatePlayerParams& Right) const
{
    if (this->role_id != Right.role_id)
        return false;
    if (this->role_name != Right.role_name)
        return false;
    if (this->physics_rank_data != Right.physics_rank_data)
        return false;
    if (this->magic_rank_data != Right.magic_rank_data)
        return false;
    if (this->model_config != Right.model_config)
        return false;
    if (this->ability_data != Right.ability_data)
        return false;
    if (this->fight_mode != Right.fight_mode)
        return false;
    if (this->is_dummy != Right.is_dummy)
        return false;
    if (this->normal_settings != Right.normal_settings)
        return false;
    if (this->self_sept_info != Right.self_sept_info)
        return false;
    if (this->combat_power != Right.combat_power)
        return false;
    if (this->all_stats_data != Right.all_stats_data)
        return false;
    return true;
}

bool FPbCreatePlayerParams::operator!=(const FPbCreatePlayerParams& Right) const
{
    return !operator==(Right);
}

FPbWorldRuntimeData::FPbWorldRuntimeData()
{
    Reset();        
}

FPbWorldRuntimeData::FPbWorldRuntimeData(const idlepb::WorldRuntimeData& Right)
{
    this->FromPb(Right);
}

void FPbWorldRuntimeData::FromPb(const idlepb::WorldRuntimeData& Right)
{
    world_id = Right.world_id();
    world_seconds = Right.world_seconds();
    time_dilation = Right.time_dilation();
}

void FPbWorldRuntimeData::ToPb(idlepb::WorldRuntimeData* Out) const
{
    Out->set_world_id(world_id);
    Out->set_world_seconds(world_seconds);
    Out->set_time_dilation(time_dilation);    
}

void FPbWorldRuntimeData::Reset()
{
    world_id = int64();
    world_seconds = float();
    time_dilation = float();    
}

void FPbWorldRuntimeData::operator=(const idlepb::WorldRuntimeData& Right)
{
    this->FromPb(Right);
}

bool FPbWorldRuntimeData::operator==(const FPbWorldRuntimeData& Right) const
{
    if (this->world_id != Right.world_id)
        return false;
    if (this->world_seconds != Right.world_seconds)
        return false;
    if (this->time_dilation != Right.time_dilation)
        return false;
    return true;
}

bool FPbWorldRuntimeData::operator!=(const FPbWorldRuntimeData& Right) const
{
    return !operator==(Right);
}

FPbNotifyGiftPackageResult::FPbNotifyGiftPackageResult()
{
    Reset();        
}

FPbNotifyGiftPackageResult::FPbNotifyGiftPackageResult(const idlepb::NotifyGiftPackageResult& Right)
{
    this->FromPb(Right);
}

void FPbNotifyGiftPackageResult::FromPb(const idlepb::NotifyGiftPackageResult& Right)
{
    items.Empty();
    for (const auto& Elem : Right.items())
    {
        items.Emplace(Elem);
    }
    gift_item_id = Right.gift_item_id();
    config_id.Empty();
    for (const auto& Elem : Right.config_id())
    {
        config_id.Emplace(Elem);
    }
}

void FPbNotifyGiftPackageResult::ToPb(idlepb::NotifyGiftPackageResult* Out) const
{
    for (const auto& Elem : items)
    {
        Elem.ToPb(Out->add_items());    
    }
    Out->set_gift_item_id(gift_item_id);
    for (const auto& Elem : config_id)
    {
        Out->add_config_id(Elem);    
    }    
}

void FPbNotifyGiftPackageResult::Reset()
{
    items = TArray<FPbSimpleItemData>();
    gift_item_id = int32();
    config_id = TArray<int32>();    
}

void FPbNotifyGiftPackageResult::operator=(const idlepb::NotifyGiftPackageResult& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyGiftPackageResult::operator==(const FPbNotifyGiftPackageResult& Right) const
{
    if (this->items != Right.items)
        return false;
    if (this->gift_item_id != Right.gift_item_id)
        return false;
    if (this->config_id != Right.config_id)
        return false;
    return true;
}

bool FPbNotifyGiftPackageResult::operator!=(const FPbNotifyGiftPackageResult& Right) const
{
    return !operator==(Right);
}

FPbNotifyUsePillProperty::FPbNotifyUsePillProperty()
{
    Reset();        
}

FPbNotifyUsePillProperty::FPbNotifyUsePillProperty(const idlepb::NotifyUsePillProperty& Right)
{
    this->FromPb(Right);
}

void FPbNotifyUsePillProperty::FromPb(const idlepb::NotifyUsePillProperty& Right)
{
    item_id = Right.item_id();
    num = Right.num();
    property_type = Right.property_type();
    property_num = Right.property_num();
}

void FPbNotifyUsePillProperty::ToPb(idlepb::NotifyUsePillProperty* Out) const
{
    Out->set_item_id(item_id);
    Out->set_num(num);
    Out->set_property_type(property_type);
    Out->set_property_num(property_num);    
}

void FPbNotifyUsePillProperty::Reset()
{
    item_id = int32();
    num = int32();
    property_type = int32();
    property_num = float();    
}

void FPbNotifyUsePillProperty::operator=(const idlepb::NotifyUsePillProperty& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyUsePillProperty::operator==(const FPbNotifyUsePillProperty& Right) const
{
    if (this->item_id != Right.item_id)
        return false;
    if (this->num != Right.num)
        return false;
    if (this->property_type != Right.property_type)
        return false;
    if (this->property_num != Right.property_num)
        return false;
    return true;
}

bool FPbNotifyUsePillProperty::operator!=(const FPbNotifyUsePillProperty& Right) const
{
    return !operator==(Right);
}

FPbEntityCultivationDirData::FPbEntityCultivationDirData()
{
    Reset();        
}

FPbEntityCultivationDirData::FPbEntityCultivationDirData(const idlepb::EntityCultivationDirData& Right)
{
    this->FromPb(Right);
}

void FPbEntityCultivationDirData::FromPb(const idlepb::EntityCultivationDirData& Right)
{
    dir = static_cast<EPbCultivationDirection>(Right.dir());
    rank = Right.rank();
    layer = Right.layer();
    stage = Right.stage();
    degree = Right.degree();
}

void FPbEntityCultivationDirData::ToPb(idlepb::EntityCultivationDirData* Out) const
{
    Out->set_dir(static_cast<idlepb::CultivationDirection>(dir));
    Out->set_rank(rank);
    Out->set_layer(layer);
    Out->set_stage(stage);
    Out->set_degree(degree);    
}

void FPbEntityCultivationDirData::Reset()
{
    dir = EPbCultivationDirection();
    rank = int32();
    layer = int32();
    stage = int32();
    degree = int32();    
}

void FPbEntityCultivationDirData::operator=(const idlepb::EntityCultivationDirData& Right)
{
    this->FromPb(Right);
}

bool FPbEntityCultivationDirData::operator==(const FPbEntityCultivationDirData& Right) const
{
    if (this->dir != Right.dir)
        return false;
    if (this->rank != Right.rank)
        return false;
    if (this->layer != Right.layer)
        return false;
    if (this->stage != Right.stage)
        return false;
    if (this->degree != Right.degree)
        return false;
    return true;
}

bool FPbEntityCultivationDirData::operator!=(const FPbEntityCultivationDirData& Right) const
{
    return !operator==(Right);
}

FPbEntityCultivationData::FPbEntityCultivationData()
{
    Reset();        
}

FPbEntityCultivationData::FPbEntityCultivationData(const idlepb::EntityCultivationData& Right)
{
    this->FromPb(Right);
}

void FPbEntityCultivationData::FromPb(const idlepb::EntityCultivationData& Right)
{
    major = Right.major();
    minor = Right.minor();
}

void FPbEntityCultivationData::ToPb(idlepb::EntityCultivationData* Out) const
{
    major.ToPb(Out->mutable_major());
    minor.ToPb(Out->mutable_minor());    
}

void FPbEntityCultivationData::Reset()
{
    major = FPbEntityCultivationDirData();
    minor = FPbEntityCultivationDirData();    
}

void FPbEntityCultivationData::operator=(const idlepb::EntityCultivationData& Right)
{
    this->FromPb(Right);
}

bool FPbEntityCultivationData::operator==(const FPbEntityCultivationData& Right) const
{
    if (this->major != Right.major)
        return false;
    if (this->minor != Right.minor)
        return false;
    return true;
}

bool FPbEntityCultivationData::operator!=(const FPbEntityCultivationData& Right) const
{
    return !operator==(Right);
}

FPbSwordPkTopListEntry::FPbSwordPkTopListEntry()
{
    Reset();        
}

FPbSwordPkTopListEntry::FPbSwordPkTopListEntry(const idlepb::SwordPkTopListEntry& Right)
{
    this->FromPb(Right);
}

void FPbSwordPkTopListEntry::FromPb(const idlepb::SwordPkTopListEntry& Right)
{
    role_id = Right.role_id();
    role_name = UTF8_TO_TCHAR(Right.role_name().c_str());
    role_model = Right.role_model();
    score = Right.score();
    score_update_ticks = Right.score_update_ticks();
    rank = Right.rank();
    combat_power = Right.combat_power();
}

void FPbSwordPkTopListEntry::ToPb(idlepb::SwordPkTopListEntry* Out) const
{
    Out->set_role_id(role_id);
    Out->set_role_name(TCHAR_TO_UTF8(*role_name));
    role_model.ToPb(Out->mutable_role_model());
    Out->set_score(score);
    Out->set_score_update_ticks(score_update_ticks);
    Out->set_rank(rank);
    Out->set_combat_power(combat_power);    
}

void FPbSwordPkTopListEntry::Reset()
{
    role_id = int64();
    role_name = FString();
    role_model = FPbCharacterModelConfig();
    score = int32();
    score_update_ticks = int64();
    rank = int32();
    combat_power = int64();    
}

void FPbSwordPkTopListEntry::operator=(const idlepb::SwordPkTopListEntry& Right)
{
    this->FromPb(Right);
}

bool FPbSwordPkTopListEntry::operator==(const FPbSwordPkTopListEntry& Right) const
{
    if (this->role_id != Right.role_id)
        return false;
    if (this->role_name != Right.role_name)
        return false;
    if (this->role_model != Right.role_model)
        return false;
    if (this->score != Right.score)
        return false;
    if (this->score_update_ticks != Right.score_update_ticks)
        return false;
    if (this->rank != Right.rank)
        return false;
    if (this->combat_power != Right.combat_power)
        return false;
    return true;
}

bool FPbSwordPkTopListEntry::operator!=(const FPbSwordPkTopListEntry& Right) const
{
    return !operator==(Right);
}

FPbSwordPkGlobalSaveData::FPbSwordPkGlobalSaveData()
{
    Reset();        
}

FPbSwordPkGlobalSaveData::FPbSwordPkGlobalSaveData(const idlepb::SwordPkGlobalSaveData& Right)
{
    this->FromPb(Right);
}

void FPbSwordPkGlobalSaveData::FromPb(const idlepb::SwordPkGlobalSaveData& Right)
{
    round_num = Right.round_num();
    begin_local_ticks = Right.begin_local_ticks();
    end_local_ticks = Right.end_local_ticks();
    is_over = Right.is_over();
    next_daily_reward_local_ticks = Right.next_daily_reward_local_ticks();
    top_list.Empty();
    for (const auto& Elem : Right.top_list())
    {
        top_list.Emplace(Elem);
    }
}

void FPbSwordPkGlobalSaveData::ToPb(idlepb::SwordPkGlobalSaveData* Out) const
{
    Out->set_round_num(round_num);
    Out->set_begin_local_ticks(begin_local_ticks);
    Out->set_end_local_ticks(end_local_ticks);
    Out->set_is_over(is_over);
    Out->set_next_daily_reward_local_ticks(next_daily_reward_local_ticks);
    for (const auto& Elem : top_list)
    {
        Elem.ToPb(Out->add_top_list());    
    }    
}

void FPbSwordPkGlobalSaveData::Reset()
{
    round_num = int32();
    begin_local_ticks = int64();
    end_local_ticks = int64();
    is_over = bool();
    next_daily_reward_local_ticks = int64();
    top_list = TArray<FPbSwordPkTopListEntry>();    
}

void FPbSwordPkGlobalSaveData::operator=(const idlepb::SwordPkGlobalSaveData& Right)
{
    this->FromPb(Right);
}

bool FPbSwordPkGlobalSaveData::operator==(const FPbSwordPkGlobalSaveData& Right) const
{
    if (this->round_num != Right.round_num)
        return false;
    if (this->begin_local_ticks != Right.begin_local_ticks)
        return false;
    if (this->end_local_ticks != Right.end_local_ticks)
        return false;
    if (this->is_over != Right.is_over)
        return false;
    if (this->next_daily_reward_local_ticks != Right.next_daily_reward_local_ticks)
        return false;
    if (this->top_list != Right.top_list)
        return false;
    return true;
}

bool FPbSwordPkGlobalSaveData::operator!=(const FPbSwordPkGlobalSaveData& Right) const
{
    return !operator==(Right);
}