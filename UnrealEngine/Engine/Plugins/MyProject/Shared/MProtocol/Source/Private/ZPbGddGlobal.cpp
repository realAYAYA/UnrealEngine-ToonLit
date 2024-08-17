#include "ZPbGddGlobal.h"
#include "gdd_global.pb.h"



FPbCommonGlobalConfig::FPbCommonGlobalConfig()
{
    Reset();        
}

FPbCommonGlobalConfig::FPbCommonGlobalConfig(const idlepb::CommonGlobalConfig& Right)
{
    this->FromPb(Right);
}

void FPbCommonGlobalConfig::FromPb(const idlepb::CommonGlobalConfig& Right)
{
    ts_rpc_max_seconds = Right.ts_rpc_max_seconds();
}

void FPbCommonGlobalConfig::ToPb(idlepb::CommonGlobalConfig* Out) const
{
    Out->set_ts_rpc_max_seconds(ts_rpc_max_seconds);    
}

void FPbCommonGlobalConfig::Reset()
{
    ts_rpc_max_seconds = float();    
}

void FPbCommonGlobalConfig::operator=(const idlepb::CommonGlobalConfig& Right)
{
    this->FromPb(Right);
}

bool FPbCommonGlobalConfig::operator==(const FPbCommonGlobalConfig& Right) const
{
    if (this->ts_rpc_max_seconds != Right.ts_rpc_max_seconds)
        return false;
    return true;
}

bool FPbCommonGlobalConfig::operator!=(const FPbCommonGlobalConfig& Right) const
{
    return !operator==(Right);
}

FPbCollectionGlobalConfigLevelUpEntry::FPbCollectionGlobalConfigLevelUpEntry()
{
    Reset();        
}

FPbCollectionGlobalConfigLevelUpEntry::FPbCollectionGlobalConfigLevelUpEntry(const idlepb::CollectionGlobalConfigLevelUpEntry& Right)
{
    this->FromPb(Right);
}

void FPbCollectionGlobalConfigLevelUpEntry::FromPb(const idlepb::CollectionGlobalConfigLevelUpEntry& Right)
{
    degree_limit = Right.degree_limit();
    stage_limit = Right.stage_limit();
    cost_item_id = Right.cost_item_id();
    cost_item_num = Right.cost_item_num();
    cost_money = Right.cost_money();
}

void FPbCollectionGlobalConfigLevelUpEntry::ToPb(idlepb::CollectionGlobalConfigLevelUpEntry* Out) const
{
    Out->set_degree_limit(degree_limit);
    Out->set_stage_limit(stage_limit);
    Out->set_cost_item_id(cost_item_id);
    Out->set_cost_item_num(cost_item_num);
    Out->set_cost_money(cost_money);    
}

void FPbCollectionGlobalConfigLevelUpEntry::Reset()
{
    degree_limit = int32();
    stage_limit = int32();
    cost_item_id = int32();
    cost_item_num = int32();
    cost_money = int32();    
}

void FPbCollectionGlobalConfigLevelUpEntry::operator=(const idlepb::CollectionGlobalConfigLevelUpEntry& Right)
{
    this->FromPb(Right);
}

bool FPbCollectionGlobalConfigLevelUpEntry::operator==(const FPbCollectionGlobalConfigLevelUpEntry& Right) const
{
    if (this->degree_limit != Right.degree_limit)
        return false;
    if (this->stage_limit != Right.stage_limit)
        return false;
    if (this->cost_item_id != Right.cost_item_id)
        return false;
    if (this->cost_item_num != Right.cost_item_num)
        return false;
    if (this->cost_money != Right.cost_money)
        return false;
    return true;
}

bool FPbCollectionGlobalConfigLevelUpEntry::operator!=(const FPbCollectionGlobalConfigLevelUpEntry& Right) const
{
    return !operator==(Right);
}

FPbCollectionGlobalConfigUpgradeStarCostRequestEntry::FPbCollectionGlobalConfigUpgradeStarCostRequestEntry()
{
    Reset();        
}

FPbCollectionGlobalConfigUpgradeStarCostRequestEntry::FPbCollectionGlobalConfigUpgradeStarCostRequestEntry(const idlepb::CollectionGlobalConfigUpgradeStarCostRequestEntry& Right)
{
    this->FromPb(Right);
}

void FPbCollectionGlobalConfigUpgradeStarCostRequestEntry::FromPb(const idlepb::CollectionGlobalConfigUpgradeStarCostRequestEntry& Right)
{
    cost_item_num = Right.cost_item_num();
    cost_self_piece_num = Right.cost_self_piece_num();
    cost_common_piece_num.Empty();
    for (const auto& Elem : Right.cost_common_piece_num())
    {
        cost_common_piece_num.Emplace(Elem);
    }
}

void FPbCollectionGlobalConfigUpgradeStarCostRequestEntry::ToPb(idlepb::CollectionGlobalConfigUpgradeStarCostRequestEntry* Out) const
{
    Out->set_cost_item_num(cost_item_num);
    Out->set_cost_self_piece_num(cost_self_piece_num);
    for (const auto& Elem : cost_common_piece_num)
    {
        Out->add_cost_common_piece_num(Elem);    
    }    
}

void FPbCollectionGlobalConfigUpgradeStarCostRequestEntry::Reset()
{
    cost_item_num = int32();
    cost_self_piece_num = int32();
    cost_common_piece_num = TArray<int32>();    
}

void FPbCollectionGlobalConfigUpgradeStarCostRequestEntry::operator=(const idlepb::CollectionGlobalConfigUpgradeStarCostRequestEntry& Right)
{
    this->FromPb(Right);
}

bool FPbCollectionGlobalConfigUpgradeStarCostRequestEntry::operator==(const FPbCollectionGlobalConfigUpgradeStarCostRequestEntry& Right) const
{
    if (this->cost_item_num != Right.cost_item_num)
        return false;
    if (this->cost_self_piece_num != Right.cost_self_piece_num)
        return false;
    if (this->cost_common_piece_num != Right.cost_common_piece_num)
        return false;
    return true;
}

bool FPbCollectionGlobalConfigUpgradeStarCostRequestEntry::operator!=(const FPbCollectionGlobalConfigUpgradeStarCostRequestEntry& Right) const
{
    return !operator==(Right);
}

FPbCollectionGlobalConfigUpgradeStarCostEntry::FPbCollectionGlobalConfigUpgradeStarCostEntry()
{
    Reset();        
}

FPbCollectionGlobalConfigUpgradeStarCostEntry::FPbCollectionGlobalConfigUpgradeStarCostEntry(const idlepb::CollectionGlobalConfigUpgradeStarCostEntry& Right)
{
    this->FromPb(Right);
}

void FPbCollectionGlobalConfigUpgradeStarCostEntry::FromPb(const idlepb::CollectionGlobalConfigUpgradeStarCostEntry& Right)
{
    request.Empty();
    for (const auto& Elem : Right.request())
    {
        request.Emplace(Elem);
    }
}

void FPbCollectionGlobalConfigUpgradeStarCostEntry::ToPb(idlepb::CollectionGlobalConfigUpgradeStarCostEntry* Out) const
{
    for (const auto& Elem : request)
    {
        Elem.ToPb(Out->add_request());    
    }    
}

void FPbCollectionGlobalConfigUpgradeStarCostEntry::Reset()
{
    request = TArray<FPbCollectionGlobalConfigUpgradeStarCostRequestEntry>();    
}

void FPbCollectionGlobalConfigUpgradeStarCostEntry::operator=(const idlepb::CollectionGlobalConfigUpgradeStarCostEntry& Right)
{
    this->FromPb(Right);
}

bool FPbCollectionGlobalConfigUpgradeStarCostEntry::operator==(const FPbCollectionGlobalConfigUpgradeStarCostEntry& Right) const
{
    if (this->request != Right.request)
        return false;
    return true;
}

bool FPbCollectionGlobalConfigUpgradeStarCostEntry::operator!=(const FPbCollectionGlobalConfigUpgradeStarCostEntry& Right) const
{
    return !operator==(Right);
}

FPbCollectionGlobalConfigUpgradeStar::FPbCollectionGlobalConfigUpgradeStar()
{
    Reset();        
}

FPbCollectionGlobalConfigUpgradeStar::FPbCollectionGlobalConfigUpgradeStar(const idlepb::CollectionGlobalConfigUpgradeStar& Right)
{
    this->FromPb(Right);
}

void FPbCollectionGlobalConfigUpgradeStar::FromPb(const idlepb::CollectionGlobalConfigUpgradeStar& Right)
{
    cost_item_id = Right.cost_item_id();
    common_piece_by_quality.Empty();
    for (const auto& Elem : Right.common_piece_by_quality())
    {
        common_piece_by_quality.Emplace(Elem);
    }
    cost_by_quality.Empty();
    for (const auto& Elem : Right.cost_by_quality())
    {
        cost_by_quality.Emplace(Elem);
    }
    cost_by_quality_skill.Empty();
    for (const auto& Elem : Right.cost_by_quality_skill())
    {
        cost_by_quality_skill.Emplace(Elem);
    }
}

void FPbCollectionGlobalConfigUpgradeStar::ToPb(idlepb::CollectionGlobalConfigUpgradeStar* Out) const
{
    Out->set_cost_item_id(cost_item_id);
    for (const auto& Elem : common_piece_by_quality)
    {
        Out->add_common_piece_by_quality(Elem);    
    }
    for (const auto& Elem : cost_by_quality)
    {
        Elem.ToPb(Out->add_cost_by_quality());    
    }
    for (const auto& Elem : cost_by_quality_skill)
    {
        Elem.ToPb(Out->add_cost_by_quality_skill());    
    }    
}

void FPbCollectionGlobalConfigUpgradeStar::Reset()
{
    cost_item_id = int32();
    common_piece_by_quality = TArray<int32>();
    cost_by_quality = TArray<FPbCollectionGlobalConfigUpgradeStarCostEntry>();
    cost_by_quality_skill = TArray<FPbCollectionGlobalConfigUpgradeStarCostEntry>();    
}

void FPbCollectionGlobalConfigUpgradeStar::operator=(const idlepb::CollectionGlobalConfigUpgradeStar& Right)
{
    this->FromPb(Right);
}

bool FPbCollectionGlobalConfigUpgradeStar::operator==(const FPbCollectionGlobalConfigUpgradeStar& Right) const
{
    if (this->cost_item_id != Right.cost_item_id)
        return false;
    if (this->common_piece_by_quality != Right.common_piece_by_quality)
        return false;
    if (this->cost_by_quality != Right.cost_by_quality)
        return false;
    if (this->cost_by_quality_skill != Right.cost_by_quality_skill)
        return false;
    return true;
}

bool FPbCollectionGlobalConfigUpgradeStar::operator!=(const FPbCollectionGlobalConfigUpgradeStar& Right) const
{
    return !operator==(Right);
}

FPbCollectionGlobalConfigReset::FPbCollectionGlobalConfigReset()
{
    Reset();        
}

FPbCollectionGlobalConfigReset::FPbCollectionGlobalConfigReset(const idlepb::CollectionGlobalConfigReset& Right)
{
    this->FromPb(Right);
}

void FPbCollectionGlobalConfigReset::FromPb(const idlepb::CollectionGlobalConfigReset& Right)
{
    cost_ji_yuan = Right.cost_ji_yuan();
    cold_time_seconds = Right.cold_time_seconds();
}

void FPbCollectionGlobalConfigReset::ToPb(idlepb::CollectionGlobalConfigReset* Out) const
{
    Out->set_cost_ji_yuan(cost_ji_yuan);
    Out->set_cold_time_seconds(cold_time_seconds);    
}

void FPbCollectionGlobalConfigReset::Reset()
{
    cost_ji_yuan = int32();
    cold_time_seconds = int32();    
}

void FPbCollectionGlobalConfigReset::operator=(const idlepb::CollectionGlobalConfigReset& Right)
{
    this->FromPb(Right);
}

bool FPbCollectionGlobalConfigReset::operator==(const FPbCollectionGlobalConfigReset& Right) const
{
    if (this->cost_ji_yuan != Right.cost_ji_yuan)
        return false;
    if (this->cold_time_seconds != Right.cold_time_seconds)
        return false;
    return true;
}

bool FPbCollectionGlobalConfigReset::operator!=(const FPbCollectionGlobalConfigReset& Right) const
{
    return !operator==(Right);
}

FPbCollectionGlobalConfig::FPbCollectionGlobalConfig()
{
    Reset();        
}

FPbCollectionGlobalConfig::FPbCollectionGlobalConfig(const idlepb::CollectionGlobalConfig& Right)
{
    this->FromPb(Right);
}

void FPbCollectionGlobalConfig::FromPb(const idlepb::CollectionGlobalConfig& Right)
{
    active_by_quality.Empty();
    for (const auto& Elem : Right.active_by_quality())
    {
        active_by_quality.Emplace(Elem);
    }
    levelup.Empty();
    for (const auto& Elem : Right.levelup())
    {
        levelup.Emplace(Elem);
    }
    upgrade_star = Right.upgrade_star();
    reset = Right.reset();
}

void FPbCollectionGlobalConfig::ToPb(idlepb::CollectionGlobalConfig* Out) const
{
    for (const auto& Elem : active_by_quality)
    {
        Out->add_active_by_quality(Elem);    
    }
    for (const auto& Elem : levelup)
    {
        Elem.ToPb(Out->add_levelup());    
    }
    upgrade_star.ToPb(Out->mutable_upgrade_star());
    reset.ToPb(Out->mutable_reset());    
}

void FPbCollectionGlobalConfig::Reset()
{
    active_by_quality = TArray<int32>();
    levelup = TArray<FPbCollectionGlobalConfigLevelUpEntry>();
    upgrade_star = FPbCollectionGlobalConfigUpgradeStar();
    reset = FPbCollectionGlobalConfigReset();    
}

void FPbCollectionGlobalConfig::operator=(const idlepb::CollectionGlobalConfig& Right)
{
    this->FromPb(Right);
}

bool FPbCollectionGlobalConfig::operator==(const FPbCollectionGlobalConfig& Right) const
{
    if (this->active_by_quality != Right.active_by_quality)
        return false;
    if (this->levelup != Right.levelup)
        return false;
    if (this->upgrade_star != Right.upgrade_star)
        return false;
    if (this->reset != Right.reset)
        return false;
    return true;
}

bool FPbCollectionGlobalConfig::operator!=(const FPbCollectionGlobalConfig& Right) const
{
    return !operator==(Right);
}

FPbCurrencyGlobalConfigItem2Currency::FPbCurrencyGlobalConfigItem2Currency()
{
    Reset();        
}

FPbCurrencyGlobalConfigItem2Currency::FPbCurrencyGlobalConfigItem2Currency(const idlepb::CurrencyGlobalConfigItem2Currency& Right)
{
    this->FromPb(Right);
}

void FPbCurrencyGlobalConfigItem2Currency::FromPb(const idlepb::CurrencyGlobalConfigItem2Currency& Right)
{
    item_cfg_id = Right.item_cfg_id();
    currency_type = static_cast<EPbCurrencyType>(Right.currency_type());
}

void FPbCurrencyGlobalConfigItem2Currency::ToPb(idlepb::CurrencyGlobalConfigItem2Currency* Out) const
{
    Out->set_item_cfg_id(item_cfg_id);
    Out->set_currency_type(static_cast<idlepb::CurrencyType>(currency_type));    
}

void FPbCurrencyGlobalConfigItem2Currency::Reset()
{
    item_cfg_id = int32();
    currency_type = EPbCurrencyType();    
}

void FPbCurrencyGlobalConfigItem2Currency::operator=(const idlepb::CurrencyGlobalConfigItem2Currency& Right)
{
    this->FromPb(Right);
}

bool FPbCurrencyGlobalConfigItem2Currency::operator==(const FPbCurrencyGlobalConfigItem2Currency& Right) const
{
    if (this->item_cfg_id != Right.item_cfg_id)
        return false;
    if (this->currency_type != Right.currency_type)
        return false;
    return true;
}

bool FPbCurrencyGlobalConfigItem2Currency::operator!=(const FPbCurrencyGlobalConfigItem2Currency& Right) const
{
    return !operator==(Right);
}

FPbCurrencyGlobalConfig::FPbCurrencyGlobalConfig()
{
    Reset();        
}

FPbCurrencyGlobalConfig::FPbCurrencyGlobalConfig(const idlepb::CurrencyGlobalConfig& Right)
{
    this->FromPb(Right);
}

void FPbCurrencyGlobalConfig::FromPb(const idlepb::CurrencyGlobalConfig& Right)
{
    item2currency.Empty();
    for (const auto& Elem : Right.item2currency())
    {
        item2currency.Emplace(Elem);
    }
}

void FPbCurrencyGlobalConfig::ToPb(idlepb::CurrencyGlobalConfig* Out) const
{
    for (const auto& Elem : item2currency)
    {
        Elem.ToPb(Out->add_item2currency());    
    }    
}

void FPbCurrencyGlobalConfig::Reset()
{
    item2currency = TArray<FPbCurrencyGlobalConfigItem2Currency>();    
}

void FPbCurrencyGlobalConfig::operator=(const idlepb::CurrencyGlobalConfig& Right)
{
    this->FromPb(Right);
}

bool FPbCurrencyGlobalConfig::operator==(const FPbCurrencyGlobalConfig& Right) const
{
    if (this->item2currency != Right.item2currency)
        return false;
    return true;
}

bool FPbCurrencyGlobalConfig::operator!=(const FPbCurrencyGlobalConfig& Right) const
{
    return !operator==(Right);
}

FPbPlayerGlobalConfigRoleInitAttributes::FPbPlayerGlobalConfigRoleInitAttributes()
{
    Reset();        
}

FPbPlayerGlobalConfigRoleInitAttributes::FPbPlayerGlobalConfigRoleInitAttributes(const idlepb::PlayerGlobalConfigRoleInitAttributes& Right)
{
    this->FromPb(Right);
}

void FPbPlayerGlobalConfigRoleInitAttributes::FromPb(const idlepb::PlayerGlobalConfigRoleInitAttributes& Right)
{
    hp = Right.hp();
    mp = Right.mp();
    phy_att = Right.phy_att();
    phy_def = Right.phy_def();
    mag_att = Right.mag_att();
    mag_def = Right.mag_def();
    hp_recover_percent = Right.hp_recover_percent();
    mp_recover_percent = Right.mp_recover_percent();
    crit_coef = Right.crit_coef();
    crit_block = Right.crit_block();
    mind = Right.mind();
}

void FPbPlayerGlobalConfigRoleInitAttributes::ToPb(idlepb::PlayerGlobalConfigRoleInitAttributes* Out) const
{
    Out->set_hp(hp);
    Out->set_mp(mp);
    Out->set_phy_att(phy_att);
    Out->set_phy_def(phy_def);
    Out->set_mag_att(mag_att);
    Out->set_mag_def(mag_def);
    Out->set_hp_recover_percent(hp_recover_percent);
    Out->set_mp_recover_percent(mp_recover_percent);
    Out->set_crit_coef(crit_coef);
    Out->set_crit_block(crit_block);
    Out->set_mind(mind);    
}

void FPbPlayerGlobalConfigRoleInitAttributes::Reset()
{
    hp = float();
    mp = float();
    phy_att = float();
    phy_def = float();
    mag_att = float();
    mag_def = float();
    hp_recover_percent = float();
    mp_recover_percent = float();
    crit_coef = float();
    crit_block = float();
    mind = float();    
}

void FPbPlayerGlobalConfigRoleInitAttributes::operator=(const idlepb::PlayerGlobalConfigRoleInitAttributes& Right)
{
    this->FromPb(Right);
}

bool FPbPlayerGlobalConfigRoleInitAttributes::operator==(const FPbPlayerGlobalConfigRoleInitAttributes& Right) const
{
    if (this->hp != Right.hp)
        return false;
    if (this->mp != Right.mp)
        return false;
    if (this->phy_att != Right.phy_att)
        return false;
    if (this->phy_def != Right.phy_def)
        return false;
    if (this->mag_att != Right.mag_att)
        return false;
    if (this->mag_def != Right.mag_def)
        return false;
    if (this->hp_recover_percent != Right.hp_recover_percent)
        return false;
    if (this->mp_recover_percent != Right.mp_recover_percent)
        return false;
    if (this->crit_coef != Right.crit_coef)
        return false;
    if (this->crit_block != Right.crit_block)
        return false;
    if (this->mind != Right.mind)
        return false;
    return true;
}

bool FPbPlayerGlobalConfigRoleInitAttributes::operator!=(const FPbPlayerGlobalConfigRoleInitAttributes& Right) const
{
    return !operator==(Right);
}

FPbPlayerGlobalConfigConstants::FPbPlayerGlobalConfigConstants()
{
    Reset();        
}

FPbPlayerGlobalConfigConstants::FPbPlayerGlobalConfigConstants(const idlepb::PlayerGlobalConfigConstants& Right)
{
    this->FromPb(Right);
}

void FPbPlayerGlobalConfigConstants::FromPb(const idlepb::PlayerGlobalConfigConstants& Right)
{
    init_radius = Right.init_radius();
    auto_move_stop_time = Right.auto_move_stop_time();
    auto_move_walk_time = Right.auto_move_walk_time();
    move_speed = Right.move_speed();
    attack_interval_time = Right.attack_interval_time();
    auto_heal_trigger_ratio_hp = Right.auto_heal_trigger_ratio_hp();
    auto_heal_trigger_ratio_mp = Right.auto_heal_trigger_ratio_mp();
    player_lock_distance_hand = Right.player_lock_distance_hand();
    player_lock_distance_auto = Right.player_lock_distance_auto();
    player_lock_distance_max = Right.player_lock_distance_max();
    player_lock_distance_near = Right.player_lock_distance_near();
    player_lock_distance_screen = Right.player_lock_distance_screen();
    enter_scale_size_distance_2d = Right.enter_scale_size_distance_2d();
    player_reborn_time = Right.player_reborn_time();
    player_reduce_time = Right.player_reduce_time();
    player_reduce_num_max = Right.player_reduce_num_max();
    player_escape_time = Right.player_escape_time();
    battle_status_seconds = Right.battle_status_seconds();
    max_explore_time = Right.max_explore_time();
    player_teleport_cooldown = Right.player_teleport_cooldown();
    switch_cultivation_direction_min_rank = Right.switch_cultivation_direction_min_rank();
    server_take_control_idle_seconds = Right.server_take_control_idle_seconds();
    player_location_correction_distance_near = Right.player_location_correction_distance_near();
    player_location_correction_distance_far = Right.player_location_correction_distance_far();
    player_correction_cost_speed = Right.player_correction_cost_speed();
    mini_map_world_width = Right.mini_map_world_width();
    mini_map_world_height = Right.mini_map_world_height();
    activate_entities_distance = Right.activate_entities_distance();
    select_box_auto_disappear_time = Right.select_box_auto_disappear_time();
    intervene_to_auto_seconds = Right.intervene_to_auto_seconds();
    set_pause_move_function_time = Right.set_pause_move_function_time();
}

void FPbPlayerGlobalConfigConstants::ToPb(idlepb::PlayerGlobalConfigConstants* Out) const
{
    Out->set_init_radius(init_radius);
    Out->set_auto_move_stop_time(auto_move_stop_time);
    Out->set_auto_move_walk_time(auto_move_walk_time);
    Out->set_move_speed(move_speed);
    Out->set_attack_interval_time(attack_interval_time);
    Out->set_auto_heal_trigger_ratio_hp(auto_heal_trigger_ratio_hp);
    Out->set_auto_heal_trigger_ratio_mp(auto_heal_trigger_ratio_mp);
    Out->set_player_lock_distance_hand(player_lock_distance_hand);
    Out->set_player_lock_distance_auto(player_lock_distance_auto);
    Out->set_player_lock_distance_max(player_lock_distance_max);
    Out->set_player_lock_distance_near(player_lock_distance_near);
    Out->set_player_lock_distance_screen(player_lock_distance_screen);
    Out->set_enter_scale_size_distance_2d(enter_scale_size_distance_2d);
    Out->set_player_reborn_time(player_reborn_time);
    Out->set_player_reduce_time(player_reduce_time);
    Out->set_player_reduce_num_max(player_reduce_num_max);
    Out->set_player_escape_time(player_escape_time);
    Out->set_battle_status_seconds(battle_status_seconds);
    Out->set_max_explore_time(max_explore_time);
    Out->set_player_teleport_cooldown(player_teleport_cooldown);
    Out->set_switch_cultivation_direction_min_rank(switch_cultivation_direction_min_rank);
    Out->set_server_take_control_idle_seconds(server_take_control_idle_seconds);
    Out->set_player_location_correction_distance_near(player_location_correction_distance_near);
    Out->set_player_location_correction_distance_far(player_location_correction_distance_far);
    Out->set_player_correction_cost_speed(player_correction_cost_speed);
    Out->set_mini_map_world_width(mini_map_world_width);
    Out->set_mini_map_world_height(mini_map_world_height);
    Out->set_activate_entities_distance(activate_entities_distance);
    Out->set_select_box_auto_disappear_time(select_box_auto_disappear_time);
    Out->set_intervene_to_auto_seconds(intervene_to_auto_seconds);
    Out->set_set_pause_move_function_time(set_pause_move_function_time);    
}

void FPbPlayerGlobalConfigConstants::Reset()
{
    init_radius = float();
    auto_move_stop_time = float();
    auto_move_walk_time = float();
    move_speed = int32();
    attack_interval_time = float();
    auto_heal_trigger_ratio_hp = float();
    auto_heal_trigger_ratio_mp = float();
    player_lock_distance_hand = float();
    player_lock_distance_auto = float();
    player_lock_distance_max = float();
    player_lock_distance_near = float();
    player_lock_distance_screen = float();
    enter_scale_size_distance_2d = float();
    player_reborn_time = float();
    player_reduce_time = float();
    player_reduce_num_max = float();
    player_escape_time = float();
    battle_status_seconds = float();
    max_explore_time = int32();
    player_teleport_cooldown = float();
    switch_cultivation_direction_min_rank = int32();
    server_take_control_idle_seconds = float();
    player_location_correction_distance_near = float();
    player_location_correction_distance_far = float();
    player_correction_cost_speed = float();
    mini_map_world_width = float();
    mini_map_world_height = float();
    activate_entities_distance = float();
    select_box_auto_disappear_time = float();
    intervene_to_auto_seconds = float();
    set_pause_move_function_time = float();    
}

void FPbPlayerGlobalConfigConstants::operator=(const idlepb::PlayerGlobalConfigConstants& Right)
{
    this->FromPb(Right);
}

bool FPbPlayerGlobalConfigConstants::operator==(const FPbPlayerGlobalConfigConstants& Right) const
{
    if (this->init_radius != Right.init_radius)
        return false;
    if (this->auto_move_stop_time != Right.auto_move_stop_time)
        return false;
    if (this->auto_move_walk_time != Right.auto_move_walk_time)
        return false;
    if (this->move_speed != Right.move_speed)
        return false;
    if (this->attack_interval_time != Right.attack_interval_time)
        return false;
    if (this->auto_heal_trigger_ratio_hp != Right.auto_heal_trigger_ratio_hp)
        return false;
    if (this->auto_heal_trigger_ratio_mp != Right.auto_heal_trigger_ratio_mp)
        return false;
    if (this->player_lock_distance_hand != Right.player_lock_distance_hand)
        return false;
    if (this->player_lock_distance_auto != Right.player_lock_distance_auto)
        return false;
    if (this->player_lock_distance_max != Right.player_lock_distance_max)
        return false;
    if (this->player_lock_distance_near != Right.player_lock_distance_near)
        return false;
    if (this->player_lock_distance_screen != Right.player_lock_distance_screen)
        return false;
    if (this->enter_scale_size_distance_2d != Right.enter_scale_size_distance_2d)
        return false;
    if (this->player_reborn_time != Right.player_reborn_time)
        return false;
    if (this->player_reduce_time != Right.player_reduce_time)
        return false;
    if (this->player_reduce_num_max != Right.player_reduce_num_max)
        return false;
    if (this->player_escape_time != Right.player_escape_time)
        return false;
    if (this->battle_status_seconds != Right.battle_status_seconds)
        return false;
    if (this->max_explore_time != Right.max_explore_time)
        return false;
    if (this->player_teleport_cooldown != Right.player_teleport_cooldown)
        return false;
    if (this->switch_cultivation_direction_min_rank != Right.switch_cultivation_direction_min_rank)
        return false;
    if (this->server_take_control_idle_seconds != Right.server_take_control_idle_seconds)
        return false;
    if (this->player_location_correction_distance_near != Right.player_location_correction_distance_near)
        return false;
    if (this->player_location_correction_distance_far != Right.player_location_correction_distance_far)
        return false;
    if (this->player_correction_cost_speed != Right.player_correction_cost_speed)
        return false;
    if (this->mini_map_world_width != Right.mini_map_world_width)
        return false;
    if (this->mini_map_world_height != Right.mini_map_world_height)
        return false;
    if (this->activate_entities_distance != Right.activate_entities_distance)
        return false;
    if (this->select_box_auto_disappear_time != Right.select_box_auto_disappear_time)
        return false;
    if (this->intervene_to_auto_seconds != Right.intervene_to_auto_seconds)
        return false;
    if (this->set_pause_move_function_time != Right.set_pause_move_function_time)
        return false;
    return true;
}

bool FPbPlayerGlobalConfigConstants::operator!=(const FPbPlayerGlobalConfigConstants& Right) const
{
    return !operator==(Right);
}

FPbPlayerGlobalConfigAbility::FPbPlayerGlobalConfigAbility()
{
    Reset();        
}

FPbPlayerGlobalConfigAbility::FPbPlayerGlobalConfigAbility(const idlepb::PlayerGlobalConfigAbility& Right)
{
    this->FromPb(Right);
}

void FPbPlayerGlobalConfigAbility::FromPb(const idlepb::PlayerGlobalConfigAbility& Right)
{
    open_rank = Right.open_rank();
    open_secondary_rank = Right.open_secondary_rank();
    slots_unlock_rank.Empty();
    for (const auto& Elem : Right.slots_unlock_rank())
    {
        slots_unlock_rank.Emplace(Elem);
    }
}

void FPbPlayerGlobalConfigAbility::ToPb(idlepb::PlayerGlobalConfigAbility* Out) const
{
    Out->set_open_rank(open_rank);
    Out->set_open_secondary_rank(open_secondary_rank);
    for (const auto& Elem : slots_unlock_rank)
    {
        Out->add_slots_unlock_rank(Elem);    
    }    
}

void FPbPlayerGlobalConfigAbility::Reset()
{
    open_rank = int32();
    open_secondary_rank = int32();
    slots_unlock_rank = TArray<int32>();    
}

void FPbPlayerGlobalConfigAbility::operator=(const idlepb::PlayerGlobalConfigAbility& Right)
{
    this->FromPb(Right);
}

bool FPbPlayerGlobalConfigAbility::operator==(const FPbPlayerGlobalConfigAbility& Right) const
{
    if (this->open_rank != Right.open_rank)
        return false;
    if (this->open_secondary_rank != Right.open_secondary_rank)
        return false;
    if (this->slots_unlock_rank != Right.slots_unlock_rank)
        return false;
    return true;
}

bool FPbPlayerGlobalConfigAbility::operator!=(const FPbPlayerGlobalConfigAbility& Right) const
{
    return !operator==(Right);
}

FPbPlayerGlobalConfigBreathingExercise::FPbPlayerGlobalConfigBreathingExercise()
{
    Reset();        
}

FPbPlayerGlobalConfigBreathingExercise::FPbPlayerGlobalConfigBreathingExercise(const idlepb::PlayerGlobalConfigBreathingExercise& Right)
{
    this->FromPb(Right);
}

void FPbPlayerGlobalConfigBreathingExercise::FromPb(const idlepb::PlayerGlobalConfigBreathingExercise& Right)
{
    speed = Right.speed();
    cancel_pct = Right.cancel_pct();
    high_min_pct = Right.high_min_pct();
    perfect_min_pct = Right.perfect_min_pct();
    perfect_max_pct = Right.perfect_max_pct();
    rate2 = Right.rate2();
    rate5 = Right.rate5();
    rate10 = Right.rate10();
    basic_ling_qi = Right.basic_ling_qi();
}

void FPbPlayerGlobalConfigBreathingExercise::ToPb(idlepb::PlayerGlobalConfigBreathingExercise* Out) const
{
    Out->set_speed(speed);
    Out->set_cancel_pct(cancel_pct);
    Out->set_high_min_pct(high_min_pct);
    Out->set_perfect_min_pct(perfect_min_pct);
    Out->set_perfect_max_pct(perfect_max_pct);
    Out->set_rate2(rate2);
    Out->set_rate5(rate5);
    Out->set_rate10(rate10);
    Out->set_basic_ling_qi(basic_ling_qi);    
}

void FPbPlayerGlobalConfigBreathingExercise::Reset()
{
    speed = float();
    cancel_pct = float();
    high_min_pct = float();
    perfect_min_pct = float();
    perfect_max_pct = float();
    rate2 = int32();
    rate5 = int32();
    rate10 = int32();
    basic_ling_qi = float();    
}

void FPbPlayerGlobalConfigBreathingExercise::operator=(const idlepb::PlayerGlobalConfigBreathingExercise& Right)
{
    this->FromPb(Right);
}

bool FPbPlayerGlobalConfigBreathingExercise::operator==(const FPbPlayerGlobalConfigBreathingExercise& Right) const
{
    if (this->speed != Right.speed)
        return false;
    if (this->cancel_pct != Right.cancel_pct)
        return false;
    if (this->high_min_pct != Right.high_min_pct)
        return false;
    if (this->perfect_min_pct != Right.perfect_min_pct)
        return false;
    if (this->perfect_max_pct != Right.perfect_max_pct)
        return false;
    if (this->rate2 != Right.rate2)
        return false;
    if (this->rate5 != Right.rate5)
        return false;
    if (this->rate10 != Right.rate10)
        return false;
    if (this->basic_ling_qi != Right.basic_ling_qi)
        return false;
    return true;
}

bool FPbPlayerGlobalConfigBreathingExercise::operator!=(const FPbPlayerGlobalConfigBreathingExercise& Right) const
{
    return !operator==(Right);
}

FPbPlayerGlobalConfigThunderTestDegreeConfig::FPbPlayerGlobalConfigThunderTestDegreeConfig()
{
    Reset();        
}

FPbPlayerGlobalConfigThunderTestDegreeConfig::FPbPlayerGlobalConfigThunderTestDegreeConfig(const idlepb::PlayerGlobalConfigThunderTestDegreeConfig& Right)
{
    this->FromPb(Right);
}

void FPbPlayerGlobalConfigThunderTestDegreeConfig::FromPb(const idlepb::PlayerGlobalConfigThunderTestDegreeConfig& Right)
{
    degree = Right.degree();
    val = Right.val();
}

void FPbPlayerGlobalConfigThunderTestDegreeConfig::ToPb(idlepb::PlayerGlobalConfigThunderTestDegreeConfig* Out) const
{
    Out->set_degree(degree);
    Out->set_val(val);    
}

void FPbPlayerGlobalConfigThunderTestDegreeConfig::Reset()
{
    degree = int32();
    val = float();    
}

void FPbPlayerGlobalConfigThunderTestDegreeConfig::operator=(const idlepb::PlayerGlobalConfigThunderTestDegreeConfig& Right)
{
    this->FromPb(Right);
}

bool FPbPlayerGlobalConfigThunderTestDegreeConfig::operator==(const FPbPlayerGlobalConfigThunderTestDegreeConfig& Right) const
{
    if (this->degree != Right.degree)
        return false;
    if (this->val != Right.val)
        return false;
    return true;
}

bool FPbPlayerGlobalConfigThunderTestDegreeConfig::operator!=(const FPbPlayerGlobalConfigThunderTestDegreeConfig& Right) const
{
    return !operator==(Right);
}

FPbPlayerGlobalConfigThunderTest::FPbPlayerGlobalConfigThunderTest()
{
    Reset();        
}

FPbPlayerGlobalConfigThunderTest::FPbPlayerGlobalConfigThunderTest(const idlepb::PlayerGlobalConfigThunderTest& Right)
{
    this->FromPb(Right);
}

void FPbPlayerGlobalConfigThunderTest::FromPb(const idlepb::PlayerGlobalConfigThunderTest& Right)
{
    damage_coef.Empty();
    for (const auto& Elem : Right.damage_coef())
    {
        damage_coef.Emplace(Elem);
    }
    damage_base.Empty();
    for (const auto& Elem : Right.damage_base())
    {
        damage_base.Emplace(Elem);
    }
}

void FPbPlayerGlobalConfigThunderTest::ToPb(idlepb::PlayerGlobalConfigThunderTest* Out) const
{
    for (const auto& Elem : damage_coef)
    {
        Out->add_damage_coef(Elem);    
    }
    for (const auto& Elem : damage_base)
    {
        Elem.ToPb(Out->add_damage_base());    
    }    
}

void FPbPlayerGlobalConfigThunderTest::Reset()
{
    damage_coef = TArray<float>();
    damage_base = TArray<FPbPlayerGlobalConfigThunderTestDegreeConfig>();    
}

void FPbPlayerGlobalConfigThunderTest::operator=(const idlepb::PlayerGlobalConfigThunderTest& Right)
{
    this->FromPb(Right);
}

bool FPbPlayerGlobalConfigThunderTest::operator==(const FPbPlayerGlobalConfigThunderTest& Right) const
{
    if (this->damage_coef != Right.damage_coef)
        return false;
    if (this->damage_base != Right.damage_base)
        return false;
    return true;
}

bool FPbPlayerGlobalConfigThunderTest::operator!=(const FPbPlayerGlobalConfigThunderTest& Right) const
{
    return !operator==(Right);
}

FPbPlayerGlobalConfigAlchemy::FPbPlayerGlobalConfigAlchemy()
{
    Reset();        
}

FPbPlayerGlobalConfigAlchemy::FPbPlayerGlobalConfigAlchemy(const idlepb::PlayerGlobalConfigAlchemy& Right)
{
    this->FromPb(Right);
}

void FPbPlayerGlobalConfigAlchemy::FromPb(const idlepb::PlayerGlobalConfigAlchemy& Right)
{
    each_refining_seconds = Right.each_refining_seconds();
    big_chance_value = Right.big_chance_value();
    small_chance_value = Right.small_chance_value();
    max_daily_count = Right.max_daily_count();
}

void FPbPlayerGlobalConfigAlchemy::ToPb(idlepb::PlayerGlobalConfigAlchemy* Out) const
{
    Out->set_each_refining_seconds(each_refining_seconds);
    Out->set_big_chance_value(big_chance_value);
    Out->set_small_chance_value(small_chance_value);
    Out->set_max_daily_count(max_daily_count);    
}

void FPbPlayerGlobalConfigAlchemy::Reset()
{
    each_refining_seconds = float();
    big_chance_value = int32();
    small_chance_value = int32();
    max_daily_count = int32();    
}

void FPbPlayerGlobalConfigAlchemy::operator=(const idlepb::PlayerGlobalConfigAlchemy& Right)
{
    this->FromPb(Right);
}

bool FPbPlayerGlobalConfigAlchemy::operator==(const FPbPlayerGlobalConfigAlchemy& Right) const
{
    if (this->each_refining_seconds != Right.each_refining_seconds)
        return false;
    if (this->big_chance_value != Right.big_chance_value)
        return false;
    if (this->small_chance_value != Right.small_chance_value)
        return false;
    if (this->max_daily_count != Right.max_daily_count)
        return false;
    return true;
}

bool FPbPlayerGlobalConfigAlchemy::operator!=(const FPbPlayerGlobalConfigAlchemy& Right) const
{
    return !operator==(Right);
}

FPbPlayerGlobalConfigForgeDestroyBackItemConfig::FPbPlayerGlobalConfigForgeDestroyBackItemConfig()
{
    Reset();        
}

FPbPlayerGlobalConfigForgeDestroyBackItemConfig::FPbPlayerGlobalConfigForgeDestroyBackItemConfig(const idlepb::PlayerGlobalConfigForgeDestroyBackItemConfig& Right)
{
    this->FromPb(Right);
}

void FPbPlayerGlobalConfigForgeDestroyBackItemConfig::FromPb(const idlepb::PlayerGlobalConfigForgeDestroyBackItemConfig& Right)
{
    degree = Right.degree();
    item_id = Right.item_id();
    item_num = Right.item_num();
}

void FPbPlayerGlobalConfigForgeDestroyBackItemConfig::ToPb(idlepb::PlayerGlobalConfigForgeDestroyBackItemConfig* Out) const
{
    Out->set_degree(degree);
    Out->set_item_id(item_id);
    Out->set_item_num(item_num);    
}

void FPbPlayerGlobalConfigForgeDestroyBackItemConfig::Reset()
{
    degree = int32();
    item_id = int32();
    item_num = int32();    
}

void FPbPlayerGlobalConfigForgeDestroyBackItemConfig::operator=(const idlepb::PlayerGlobalConfigForgeDestroyBackItemConfig& Right)
{
    this->FromPb(Right);
}

bool FPbPlayerGlobalConfigForgeDestroyBackItemConfig::operator==(const FPbPlayerGlobalConfigForgeDestroyBackItemConfig& Right) const
{
    if (this->degree != Right.degree)
        return false;
    if (this->item_id != Right.item_id)
        return false;
    if (this->item_num != Right.item_num)
        return false;
    return true;
}

bool FPbPlayerGlobalConfigForgeDestroyBackItemConfig::operator!=(const FPbPlayerGlobalConfigForgeDestroyBackItemConfig& Right) const
{
    return !operator==(Right);
}

FPbPlayerGlobalConfigForge::FPbPlayerGlobalConfigForge()
{
    Reset();        
}

FPbPlayerGlobalConfigForge::FPbPlayerGlobalConfigForge(const idlepb::PlayerGlobalConfigForge& Right)
{
    this->FromPb(Right);
}

void FPbPlayerGlobalConfigForge::FromPb(const idlepb::PlayerGlobalConfigForge& Right)
{
    each_refining_seconds = Right.each_refining_seconds();
    big_chance_value = Right.big_chance_value();
    small_chance_value = Right.small_chance_value();
    max_daily_count = Right.max_daily_count();
    max_daily_extra_materials_use_count = Right.max_daily_extra_materials_use_count();
    revert_cost_gold = Right.revert_cost_gold();
    destroy_cost_gold = Right.destroy_cost_gold();
    destroy_get_item_num.Empty();
    for (const auto& Elem : Right.destroy_get_item_num())
    {
        destroy_get_item_num.Emplace(Elem);
    }
    found_cost_gold = Right.found_cost_gold();
    found_time = Right.found_time();
}

void FPbPlayerGlobalConfigForge::ToPb(idlepb::PlayerGlobalConfigForge* Out) const
{
    Out->set_each_refining_seconds(each_refining_seconds);
    Out->set_big_chance_value(big_chance_value);
    Out->set_small_chance_value(small_chance_value);
    Out->set_max_daily_count(max_daily_count);
    Out->set_max_daily_extra_materials_use_count(max_daily_extra_materials_use_count);
    Out->set_revert_cost_gold(revert_cost_gold);
    Out->set_destroy_cost_gold(destroy_cost_gold);
    for (const auto& Elem : destroy_get_item_num)
    {
        Elem.ToPb(Out->add_destroy_get_item_num());    
    }
    Out->set_found_cost_gold(found_cost_gold);
    Out->set_found_time(found_time);    
}

void FPbPlayerGlobalConfigForge::Reset()
{
    each_refining_seconds = float();
    big_chance_value = int32();
    small_chance_value = int32();
    max_daily_count = int32();
    max_daily_extra_materials_use_count = int32();
    revert_cost_gold = int32();
    destroy_cost_gold = int32();
    destroy_get_item_num = TArray<FPbPlayerGlobalConfigForgeDestroyBackItemConfig>();
    found_cost_gold = int32();
    found_time = int32();    
}

void FPbPlayerGlobalConfigForge::operator=(const idlepb::PlayerGlobalConfigForge& Right)
{
    this->FromPb(Right);
}

bool FPbPlayerGlobalConfigForge::operator==(const FPbPlayerGlobalConfigForge& Right) const
{
    if (this->each_refining_seconds != Right.each_refining_seconds)
        return false;
    if (this->big_chance_value != Right.big_chance_value)
        return false;
    if (this->small_chance_value != Right.small_chance_value)
        return false;
    if (this->max_daily_count != Right.max_daily_count)
        return false;
    if (this->max_daily_extra_materials_use_count != Right.max_daily_extra_materials_use_count)
        return false;
    if (this->revert_cost_gold != Right.revert_cost_gold)
        return false;
    if (this->destroy_cost_gold != Right.destroy_cost_gold)
        return false;
    if (this->destroy_get_item_num != Right.destroy_get_item_num)
        return false;
    if (this->found_cost_gold != Right.found_cost_gold)
        return false;
    if (this->found_time != Right.found_time)
        return false;
    return true;
}

bool FPbPlayerGlobalConfigForge::operator!=(const FPbPlayerGlobalConfigForge& Right) const
{
    return !operator==(Right);
}

FPbPlayerGlobalConfigFightMode::FPbPlayerGlobalConfigFightMode()
{
    Reset();        
}

FPbPlayerGlobalConfigFightMode::FPbPlayerGlobalConfigFightMode(const idlepb::PlayerGlobalConfigFightMode& Right)
{
    this->FromPb(Right);
}

void FPbPlayerGlobalConfigFightMode::FromPb(const idlepb::PlayerGlobalConfigFightMode& Right)
{
    all_mode_require_rank = Right.all_mode_require_rank();
    all_mode_to_peace_mode_need_seconds = Right.all_mode_to_peace_mode_need_seconds();
    mode_change_need_seconds = Right.mode_change_need_seconds();
    hate_sustain_seconds = Right.hate_sustain_seconds();
    world_speed_unlock_rank = Right.world_speed_unlock_rank();
}

void FPbPlayerGlobalConfigFightMode::ToPb(idlepb::PlayerGlobalConfigFightMode* Out) const
{
    Out->set_all_mode_require_rank(all_mode_require_rank);
    Out->set_all_mode_to_peace_mode_need_seconds(all_mode_to_peace_mode_need_seconds);
    Out->set_mode_change_need_seconds(mode_change_need_seconds);
    Out->set_hate_sustain_seconds(hate_sustain_seconds);
    Out->set_world_speed_unlock_rank(world_speed_unlock_rank);    
}

void FPbPlayerGlobalConfigFightMode::Reset()
{
    all_mode_require_rank = int32();
    all_mode_to_peace_mode_need_seconds = float();
    mode_change_need_seconds = float();
    hate_sustain_seconds = float();
    world_speed_unlock_rank = int32();    
}

void FPbPlayerGlobalConfigFightMode::operator=(const idlepb::PlayerGlobalConfigFightMode& Right)
{
    this->FromPb(Right);
}

bool FPbPlayerGlobalConfigFightMode::operator==(const FPbPlayerGlobalConfigFightMode& Right) const
{
    if (this->all_mode_require_rank != Right.all_mode_require_rank)
        return false;
    if (this->all_mode_to_peace_mode_need_seconds != Right.all_mode_to_peace_mode_need_seconds)
        return false;
    if (this->mode_change_need_seconds != Right.mode_change_need_seconds)
        return false;
    if (this->hate_sustain_seconds != Right.hate_sustain_seconds)
        return false;
    if (this->world_speed_unlock_rank != Right.world_speed_unlock_rank)
        return false;
    return true;
}

bool FPbPlayerGlobalConfigFightMode::operator!=(const FPbPlayerGlobalConfigFightMode& Right) const
{
    return !operator==(Right);
}

FPbPlayerGlobalConfigInventory::FPbPlayerGlobalConfigInventory()
{
    Reset();        
}

FPbPlayerGlobalConfigInventory::FPbPlayerGlobalConfigInventory(const idlepb::PlayerGlobalConfigInventory& Right)
{
    this->FromPb(Right);
}

void FPbPlayerGlobalConfigInventory::FromPb(const idlepb::PlayerGlobalConfigInventory& Right)
{
    init_space = Right.init_space();
    stage_up_add_space = Right.stage_up_add_space();
    full_mail_id = Right.full_mail_id();
}

void FPbPlayerGlobalConfigInventory::ToPb(idlepb::PlayerGlobalConfigInventory* Out) const
{
    Out->set_init_space(init_space);
    Out->set_stage_up_add_space(stage_up_add_space);
    Out->set_full_mail_id(full_mail_id);    
}

void FPbPlayerGlobalConfigInventory::Reset()
{
    init_space = int32();
    stage_up_add_space = int32();
    full_mail_id = int32();    
}

void FPbPlayerGlobalConfigInventory::operator=(const idlepb::PlayerGlobalConfigInventory& Right)
{
    this->FromPb(Right);
}

bool FPbPlayerGlobalConfigInventory::operator==(const FPbPlayerGlobalConfigInventory& Right) const
{
    if (this->init_space != Right.init_space)
        return false;
    if (this->stage_up_add_space != Right.stage_up_add_space)
        return false;
    if (this->full_mail_id != Right.full_mail_id)
        return false;
    return true;
}

bool FPbPlayerGlobalConfigInventory::operator!=(const FPbPlayerGlobalConfigInventory& Right) const
{
    return !operator==(Right);
}

FPbPlayerGlobalConfig::FPbPlayerGlobalConfig()
{
    Reset();        
}

FPbPlayerGlobalConfig::FPbPlayerGlobalConfig(const idlepb::PlayerGlobalConfig& Right)
{
    this->FromPb(Right);
}

void FPbPlayerGlobalConfig::FromPb(const idlepb::PlayerGlobalConfig& Right)
{
    constants = Right.constants();
    new_role_init_attrs = Right.new_role_init_attrs();
    ability = Right.ability();
    breathing_exercise = Right.breathing_exercise();
    thunder_test = Right.thunder_test();
    alchemy = Right.alchemy();
    forge = Right.forge();
    fight_mode = Right.fight_mode();
    inventory = Right.inventory();
}

void FPbPlayerGlobalConfig::ToPb(idlepb::PlayerGlobalConfig* Out) const
{
    constants.ToPb(Out->mutable_constants());
    new_role_init_attrs.ToPb(Out->mutable_new_role_init_attrs());
    ability.ToPb(Out->mutable_ability());
    breathing_exercise.ToPb(Out->mutable_breathing_exercise());
    thunder_test.ToPb(Out->mutable_thunder_test());
    alchemy.ToPb(Out->mutable_alchemy());
    forge.ToPb(Out->mutable_forge());
    fight_mode.ToPb(Out->mutable_fight_mode());
    inventory.ToPb(Out->mutable_inventory());    
}

void FPbPlayerGlobalConfig::Reset()
{
    constants = FPbPlayerGlobalConfigConstants();
    new_role_init_attrs = FPbPlayerGlobalConfigRoleInitAttributes();
    ability = FPbPlayerGlobalConfigAbility();
    breathing_exercise = FPbPlayerGlobalConfigBreathingExercise();
    thunder_test = FPbPlayerGlobalConfigThunderTest();
    alchemy = FPbPlayerGlobalConfigAlchemy();
    forge = FPbPlayerGlobalConfigForge();
    fight_mode = FPbPlayerGlobalConfigFightMode();
    inventory = FPbPlayerGlobalConfigInventory();    
}

void FPbPlayerGlobalConfig::operator=(const idlepb::PlayerGlobalConfig& Right)
{
    this->FromPb(Right);
}

bool FPbPlayerGlobalConfig::operator==(const FPbPlayerGlobalConfig& Right) const
{
    if (this->constants != Right.constants)
        return false;
    if (this->new_role_init_attrs != Right.new_role_init_attrs)
        return false;
    if (this->ability != Right.ability)
        return false;
    if (this->breathing_exercise != Right.breathing_exercise)
        return false;
    if (this->thunder_test != Right.thunder_test)
        return false;
    if (this->alchemy != Right.alchemy)
        return false;
    if (this->forge != Right.forge)
        return false;
    if (this->fight_mode != Right.fight_mode)
        return false;
    if (this->inventory != Right.inventory)
        return false;
    return true;
}

bool FPbPlayerGlobalConfig::operator!=(const FPbPlayerGlobalConfig& Right) const
{
    return !operator==(Right);
}

FPbNpcGlobalConfigConstants::FPbNpcGlobalConfigConstants()
{
    Reset();        
}

FPbNpcGlobalConfigConstants::FPbNpcGlobalConfigConstants(const idlepb::NpcGlobalConfigConstants& Right)
{
    this->FromPb(Right);
}

void FPbNpcGlobalConfigConstants::FromPb(const idlepb::NpcGlobalConfigConstants& Right)
{
    auto_move_stop_time = Right.auto_move_stop_time();
    auto_move_walk_time = Right.auto_move_walk_time();
    walk_speed = Right.walk_speed();
    attack_interval_time = Right.attack_interval_time();
    npc_lock_distance_auto = Right.npc_lock_distance_auto();
    npc_lock_distance_max = Right.npc_lock_distance_max();
    monster_location_correction_distance_near = Right.monster_location_correction_distance_near();
    monster_location_correction_distance_far = Right.monster_location_correction_distance_far();
    monster_correction_cost_speed = Right.monster_correction_cost_speed();
    phy_default_ability_fullid = Right.phy_default_ability_fullid();
    mag_default_ability_fullid = Right.mag_default_ability_fullid();
    default_ability_weight = Right.default_ability_weight();
}

void FPbNpcGlobalConfigConstants::ToPb(idlepb::NpcGlobalConfigConstants* Out) const
{
    Out->set_auto_move_stop_time(auto_move_stop_time);
    Out->set_auto_move_walk_time(auto_move_walk_time);
    Out->set_walk_speed(walk_speed);
    Out->set_attack_interval_time(attack_interval_time);
    Out->set_npc_lock_distance_auto(npc_lock_distance_auto);
    Out->set_npc_lock_distance_max(npc_lock_distance_max);
    Out->set_monster_location_correction_distance_near(monster_location_correction_distance_near);
    Out->set_monster_location_correction_distance_far(monster_location_correction_distance_far);
    Out->set_monster_correction_cost_speed(monster_correction_cost_speed);
    Out->set_phy_default_ability_fullid(phy_default_ability_fullid);
    Out->set_mag_default_ability_fullid(mag_default_ability_fullid);
    Out->set_default_ability_weight(default_ability_weight);    
}

void FPbNpcGlobalConfigConstants::Reset()
{
    auto_move_stop_time = float();
    auto_move_walk_time = float();
    walk_speed = int32();
    attack_interval_time = float();
    npc_lock_distance_auto = float();
    npc_lock_distance_max = float();
    monster_location_correction_distance_near = float();
    monster_location_correction_distance_far = float();
    monster_correction_cost_speed = float();
    phy_default_ability_fullid = float();
    mag_default_ability_fullid = float();
    default_ability_weight = int32();    
}

void FPbNpcGlobalConfigConstants::operator=(const idlepb::NpcGlobalConfigConstants& Right)
{
    this->FromPb(Right);
}

bool FPbNpcGlobalConfigConstants::operator==(const FPbNpcGlobalConfigConstants& Right) const
{
    if (this->auto_move_stop_time != Right.auto_move_stop_time)
        return false;
    if (this->auto_move_walk_time != Right.auto_move_walk_time)
        return false;
    if (this->walk_speed != Right.walk_speed)
        return false;
    if (this->attack_interval_time != Right.attack_interval_time)
        return false;
    if (this->npc_lock_distance_auto != Right.npc_lock_distance_auto)
        return false;
    if (this->npc_lock_distance_max != Right.npc_lock_distance_max)
        return false;
    if (this->monster_location_correction_distance_near != Right.monster_location_correction_distance_near)
        return false;
    if (this->monster_location_correction_distance_far != Right.monster_location_correction_distance_far)
        return false;
    if (this->monster_correction_cost_speed != Right.monster_correction_cost_speed)
        return false;
    if (this->phy_default_ability_fullid != Right.phy_default_ability_fullid)
        return false;
    if (this->mag_default_ability_fullid != Right.mag_default_ability_fullid)
        return false;
    if (this->default_ability_weight != Right.default_ability_weight)
        return false;
    return true;
}

bool FPbNpcGlobalConfigConstants::operator!=(const FPbNpcGlobalConfigConstants& Right) const
{
    return !operator==(Right);
}

FPbNpcGlobalConfig::FPbNpcGlobalConfig()
{
    Reset();        
}

FPbNpcGlobalConfig::FPbNpcGlobalConfig(const idlepb::NpcGlobalConfig& Right)
{
    this->FromPb(Right);
}

void FPbNpcGlobalConfig::FromPb(const idlepb::NpcGlobalConfig& Right)
{
    constants = Right.constants();
}

void FPbNpcGlobalConfig::ToPb(idlepb::NpcGlobalConfig* Out) const
{
    constants.ToPb(Out->mutable_constants());    
}

void FPbNpcGlobalConfig::Reset()
{
    constants = FPbNpcGlobalConfigConstants();    
}

void FPbNpcGlobalConfig::operator=(const idlepb::NpcGlobalConfig& Right)
{
    this->FromPb(Right);
}

bool FPbNpcGlobalConfig::operator==(const FPbNpcGlobalConfig& Right) const
{
    if (this->constants != Right.constants)
        return false;
    return true;
}

bool FPbNpcGlobalConfig::operator!=(const FPbNpcGlobalConfig& Right) const
{
    return !operator==(Right);
}

FPbWorldGlobalConfigDungeonCommon::FPbWorldGlobalConfigDungeonCommon()
{
    Reset();        
}

FPbWorldGlobalConfigDungeonCommon::FPbWorldGlobalConfigDungeonCommon(const idlepb::WorldGlobalConfigDungeonCommon& Right)
{
    this->FromPb(Right);
}

void FPbWorldGlobalConfigDungeonCommon::FromPb(const idlepb::WorldGlobalConfigDungeonCommon& Right)
{
    begin_delay_seconds = Right.begin_delay_seconds();
    end_delay_seconds = Right.end_delay_seconds();
}

void FPbWorldGlobalConfigDungeonCommon::ToPb(idlepb::WorldGlobalConfigDungeonCommon* Out) const
{
    Out->set_begin_delay_seconds(begin_delay_seconds);
    Out->set_end_delay_seconds(end_delay_seconds);    
}

void FPbWorldGlobalConfigDungeonCommon::Reset()
{
    begin_delay_seconds = float();
    end_delay_seconds = float();    
}

void FPbWorldGlobalConfigDungeonCommon::operator=(const idlepb::WorldGlobalConfigDungeonCommon& Right)
{
    this->FromPb(Right);
}

bool FPbWorldGlobalConfigDungeonCommon::operator==(const FPbWorldGlobalConfigDungeonCommon& Right) const
{
    if (this->begin_delay_seconds != Right.begin_delay_seconds)
        return false;
    if (this->end_delay_seconds != Right.end_delay_seconds)
        return false;
    return true;
}

bool FPbWorldGlobalConfigDungeonCommon::operator!=(const FPbWorldGlobalConfigDungeonCommon& Right) const
{
    return !operator==(Right);
}

FPbWorldGlobalConfig::FPbWorldGlobalConfig()
{
    Reset();        
}

FPbWorldGlobalConfig::FPbWorldGlobalConfig(const idlepb::WorldGlobalConfig& Right)
{
    this->FromPb(Right);
}

void FPbWorldGlobalConfig::FromPb(const idlepb::WorldGlobalConfig& Right)
{
    dungeon_common = Right.dungeon_common();
}

void FPbWorldGlobalConfig::ToPb(idlepb::WorldGlobalConfig* Out) const
{
    dungeon_common.ToPb(Out->mutable_dungeon_common());    
}

void FPbWorldGlobalConfig::Reset()
{
    dungeon_common = FPbWorldGlobalConfigDungeonCommon();    
}

void FPbWorldGlobalConfig::operator=(const idlepb::WorldGlobalConfig& Right)
{
    this->FromPb(Right);
}

bool FPbWorldGlobalConfig::operator==(const FPbWorldGlobalConfig& Right) const
{
    if (this->dungeon_common != Right.dungeon_common)
        return false;
    return true;
}

bool FPbWorldGlobalConfig::operator!=(const FPbWorldGlobalConfig& Right) const
{
    return !operator==(Right);
}

FPbAppearanceGlobalConfigShopRefreshRule::FPbAppearanceGlobalConfigShopRefreshRule()
{
    Reset();        
}

FPbAppearanceGlobalConfigShopRefreshRule::FPbAppearanceGlobalConfigShopRefreshRule(const idlepb::AppearanceGlobalConfigShopRefreshRule& Right)
{
    this->FromPb(Right);
}

void FPbAppearanceGlobalConfigShopRefreshRule::FromPb(const idlepb::AppearanceGlobalConfigShopRefreshRule& Right)
{
    type = Right.type();
    num.Empty();
    for (const auto& Elem : Right.num())
    {
        num.Emplace(Elem);
    }
    weight.Empty();
    for (const auto& Elem : Right.weight())
    {
        weight.Emplace(Elem);
    }
}

void FPbAppearanceGlobalConfigShopRefreshRule::ToPb(idlepb::AppearanceGlobalConfigShopRefreshRule* Out) const
{
    Out->set_type(type);
    for (const auto& Elem : num)
    {
        Out->add_num(Elem);    
    }
    for (const auto& Elem : weight)
    {
        Out->add_weight(Elem);    
    }    
}

void FPbAppearanceGlobalConfigShopRefreshRule::Reset()
{
    type = int32();
    num = TArray<int32>();
    weight = TArray<int32>();    
}

void FPbAppearanceGlobalConfigShopRefreshRule::operator=(const idlepb::AppearanceGlobalConfigShopRefreshRule& Right)
{
    this->FromPb(Right);
}

bool FPbAppearanceGlobalConfigShopRefreshRule::operator==(const FPbAppearanceGlobalConfigShopRefreshRule& Right) const
{
    if (this->type != Right.type)
        return false;
    if (this->num != Right.num)
        return false;
    if (this->weight != Right.weight)
        return false;
    return true;
}

bool FPbAppearanceGlobalConfigShopRefreshRule::operator!=(const FPbAppearanceGlobalConfigShopRefreshRule& Right) const
{
    return !operator==(Right);
}

FPbAppearanceGlobalConfigModelTypeInfo::FPbAppearanceGlobalConfigModelTypeInfo()
{
    Reset();        
}

FPbAppearanceGlobalConfigModelTypeInfo::FPbAppearanceGlobalConfigModelTypeInfo(const idlepb::AppearanceGlobalConfigModelTypeInfo& Right)
{
    this->FromPb(Right);
}

void FPbAppearanceGlobalConfigModelTypeInfo::FromPb(const idlepb::AppearanceGlobalConfigModelTypeInfo& Right)
{
    model_type = Right.model_type();
    name_english = UTF8_TO_TCHAR(Right.name_english().c_str());
    name_chinese = UTF8_TO_TCHAR(Right.name_chinese().c_str());
}

void FPbAppearanceGlobalConfigModelTypeInfo::ToPb(idlepb::AppearanceGlobalConfigModelTypeInfo* Out) const
{
    Out->set_model_type(model_type);
    Out->set_name_english(TCHAR_TO_UTF8(*name_english));
    Out->set_name_chinese(TCHAR_TO_UTF8(*name_chinese));    
}

void FPbAppearanceGlobalConfigModelTypeInfo::Reset()
{
    model_type = int32();
    name_english = FString();
    name_chinese = FString();    
}

void FPbAppearanceGlobalConfigModelTypeInfo::operator=(const idlepb::AppearanceGlobalConfigModelTypeInfo& Right)
{
    this->FromPb(Right);
}

bool FPbAppearanceGlobalConfigModelTypeInfo::operator==(const FPbAppearanceGlobalConfigModelTypeInfo& Right) const
{
    if (this->model_type != Right.model_type)
        return false;
    if (this->name_english != Right.name_english)
        return false;
    if (this->name_chinese != Right.name_chinese)
        return false;
    return true;
}

bool FPbAppearanceGlobalConfigModelTypeInfo::operator!=(const FPbAppearanceGlobalConfigModelTypeInfo& Right) const
{
    return !operator==(Right);
}

FPbAppearanceGlobalConfig::FPbAppearanceGlobalConfig()
{
    Reset();        
}

FPbAppearanceGlobalConfig::FPbAppearanceGlobalConfig(const idlepb::AppearanceGlobalConfig& Right)
{
    this->FromPb(Right);
}

void FPbAppearanceGlobalConfig::FromPb(const idlepb::AppearanceGlobalConfig& Right)
{
    refund_item_id = Right.refund_item_id();
    shop_item_id = Right.shop_item_id();
    shop_refresh_time = Right.shop_refresh_time();
    sk_type_change_cd = Right.sk_type_change_cd();
    sk_type_change_item_id = Right.sk_type_change_item_id();
    shop_refresh_rule.Empty();
    for (const auto& Elem : Right.shop_refresh_rule())
    {
        shop_refresh_rule.Emplace(Elem);
    }
    model_type_info.Empty();
    for (const auto& Elem : Right.model_type_info())
    {
        model_type_info.Emplace(Elem);
    }
}

void FPbAppearanceGlobalConfig::ToPb(idlepb::AppearanceGlobalConfig* Out) const
{
    Out->set_refund_item_id(refund_item_id);
    Out->set_shop_item_id(shop_item_id);
    Out->set_shop_refresh_time(shop_refresh_time);
    Out->set_sk_type_change_cd(sk_type_change_cd);
    Out->set_sk_type_change_item_id(sk_type_change_item_id);
    for (const auto& Elem : shop_refresh_rule)
    {
        Elem.ToPb(Out->add_shop_refresh_rule());    
    }
    for (const auto& Elem : model_type_info)
    {
        Elem.ToPb(Out->add_model_type_info());    
    }    
}

void FPbAppearanceGlobalConfig::Reset()
{
    refund_item_id = int32();
    shop_item_id = int32();
    shop_refresh_time = int32();
    sk_type_change_cd = int32();
    sk_type_change_item_id = int32();
    shop_refresh_rule = TArray<FPbAppearanceGlobalConfigShopRefreshRule>();
    model_type_info = TArray<FPbAppearanceGlobalConfigModelTypeInfo>();    
}

void FPbAppearanceGlobalConfig::operator=(const idlepb::AppearanceGlobalConfig& Right)
{
    this->FromPb(Right);
}

bool FPbAppearanceGlobalConfig::operator==(const FPbAppearanceGlobalConfig& Right) const
{
    if (this->refund_item_id != Right.refund_item_id)
        return false;
    if (this->shop_item_id != Right.shop_item_id)
        return false;
    if (this->shop_refresh_time != Right.shop_refresh_time)
        return false;
    if (this->sk_type_change_cd != Right.sk_type_change_cd)
        return false;
    if (this->sk_type_change_item_id != Right.sk_type_change_item_id)
        return false;
    if (this->shop_refresh_rule != Right.shop_refresh_rule)
        return false;
    if (this->model_type_info != Right.model_type_info)
        return false;
    return true;
}

bool FPbAppearanceGlobalConfig::operator!=(const FPbAppearanceGlobalConfig& Right) const
{
    return !operator==(Right);
}

FPbFarmGlobalConfig::FPbFarmGlobalConfig()
{
    Reset();        
}

FPbFarmGlobalConfig::FPbFarmGlobalConfig(const idlepb::FarmGlobalConfig& Right)
{
    this->FromPb(Right);
}

void FPbFarmGlobalConfig::FromPb(const idlepb::FarmGlobalConfig& Right)
{
    farmland_shape = UTF8_TO_TCHAR(Right.farmland_shape().c_str());
    default_unlock_farmland_index.Empty();
    for (const auto& Elem : Right.default_unlock_farmland_index())
    {
        default_unlock_farmland_index.Emplace(Elem);
    }
    unlock_farmland_cost_item_id = Right.unlock_farmland_cost_item_id();
    unlock_farmland_cost_item_num.Empty();
    for (const auto& Elem : Right.unlock_farmland_cost_item_num())
    {
        unlock_farmland_cost_item_num.Emplace(Elem);
    }
    farmland_seed_shape = UTF8_TO_TCHAR(Right.farmland_seed_shape().c_str());
    watering_times_per_day = Right.watering_times_per_day();
    cost_per_watering.Empty();
    for (const auto& Elem : Right.cost_per_watering())
    {
        cost_per_watering.Emplace(Elem);
    }
    add_speed_per_watering = Right.add_speed_per_watering();
    add_speed_from_item = UTF8_TO_TCHAR(Right.add_speed_from_item().c_str());
    min_seed_plant_time_percent = Right.min_seed_plant_time_percent();
}

void FPbFarmGlobalConfig::ToPb(idlepb::FarmGlobalConfig* Out) const
{
    Out->set_farmland_shape(TCHAR_TO_UTF8(*farmland_shape));
    for (const auto& Elem : default_unlock_farmland_index)
    {
        Out->add_default_unlock_farmland_index(Elem);    
    }
    Out->set_unlock_farmland_cost_item_id(unlock_farmland_cost_item_id);
    for (const auto& Elem : unlock_farmland_cost_item_num)
    {
        Out->add_unlock_farmland_cost_item_num(Elem);    
    }
    Out->set_farmland_seed_shape(TCHAR_TO_UTF8(*farmland_seed_shape));
    Out->set_watering_times_per_day(watering_times_per_day);
    for (const auto& Elem : cost_per_watering)
    {
        Out->add_cost_per_watering(Elem);    
    }
    Out->set_add_speed_per_watering(add_speed_per_watering);
    Out->set_add_speed_from_item(TCHAR_TO_UTF8(*add_speed_from_item));
    Out->set_min_seed_plant_time_percent(min_seed_plant_time_percent);    
}

void FPbFarmGlobalConfig::Reset()
{
    farmland_shape = FString();
    default_unlock_farmland_index = TArray<int32>();
    unlock_farmland_cost_item_id = int32();
    unlock_farmland_cost_item_num = TArray<int32>();
    farmland_seed_shape = FString();
    watering_times_per_day = int32();
    cost_per_watering = TArray<int32>();
    add_speed_per_watering = int32();
    add_speed_from_item = FString();
    min_seed_plant_time_percent = float();    
}

void FPbFarmGlobalConfig::operator=(const idlepb::FarmGlobalConfig& Right)
{
    this->FromPb(Right);
}

bool FPbFarmGlobalConfig::operator==(const FPbFarmGlobalConfig& Right) const
{
    if (this->farmland_shape != Right.farmland_shape)
        return false;
    if (this->default_unlock_farmland_index != Right.default_unlock_farmland_index)
        return false;
    if (this->unlock_farmland_cost_item_id != Right.unlock_farmland_cost_item_id)
        return false;
    if (this->unlock_farmland_cost_item_num != Right.unlock_farmland_cost_item_num)
        return false;
    if (this->farmland_seed_shape != Right.farmland_seed_shape)
        return false;
    if (this->watering_times_per_day != Right.watering_times_per_day)
        return false;
    if (this->cost_per_watering != Right.cost_per_watering)
        return false;
    if (this->add_speed_per_watering != Right.add_speed_per_watering)
        return false;
    if (this->add_speed_from_item != Right.add_speed_from_item)
        return false;
    if (this->min_seed_plant_time_percent != Right.min_seed_plant_time_percent)
        return false;
    return true;
}

bool FPbFarmGlobalConfig::operator!=(const FPbFarmGlobalConfig& Right) const
{
    return !operator==(Right);
}

FPbFriendsGlobalConfig::FPbFriendsGlobalConfig()
{
    Reset();        
}

FPbFriendsGlobalConfig::FPbFriendsGlobalConfig(const idlepb::FriendsGlobalConfig& Right)
{
    this->FromPb(Right);
}

void FPbFriendsGlobalConfig::FromPb(const idlepb::FriendsGlobalConfig& Right)
{
    max_friend_num = Right.max_friend_num();
    daily_search_count = Right.daily_search_count();
    max_apply_count = Right.max_apply_count();
    max_block_list = Right.max_block_list();
}

void FPbFriendsGlobalConfig::ToPb(idlepb::FriendsGlobalConfig* Out) const
{
    Out->set_max_friend_num(max_friend_num);
    Out->set_daily_search_count(daily_search_count);
    Out->set_max_apply_count(max_apply_count);
    Out->set_max_block_list(max_block_list);    
}

void FPbFriendsGlobalConfig::Reset()
{
    max_friend_num = int32();
    daily_search_count = int32();
    max_apply_count = int32();
    max_block_list = int32();    
}

void FPbFriendsGlobalConfig::operator=(const idlepb::FriendsGlobalConfig& Right)
{
    this->FromPb(Right);
}

bool FPbFriendsGlobalConfig::operator==(const FPbFriendsGlobalConfig& Right) const
{
    if (this->max_friend_num != Right.max_friend_num)
        return false;
    if (this->daily_search_count != Right.daily_search_count)
        return false;
    if (this->max_apply_count != Right.max_apply_count)
        return false;
    if (this->max_block_list != Right.max_block_list)
        return false;
    return true;
}

bool FPbFriendsGlobalConfig::operator!=(const FPbFriendsGlobalConfig& Right) const
{
    return !operator==(Right);
}

FPbAvatarStageCountDiff::FPbAvatarStageCountDiff()
{
    Reset();        
}

FPbAvatarStageCountDiff::FPbAvatarStageCountDiff(const idlepb::AvatarStageCountDiff& Right)
{
    this->FromPb(Right);
}

void FPbAvatarStageCountDiff::FromPb(const idlepb::AvatarStageCountDiff& Right)
{
    diff_num = Right.diff_num();
    coef = Right.coef();
}

void FPbAvatarStageCountDiff::ToPb(idlepb::AvatarStageCountDiff* Out) const
{
    Out->set_diff_num(diff_num);
    Out->set_coef(coef);    
}

void FPbAvatarStageCountDiff::Reset()
{
    diff_num = int32();
    coef = float();    
}

void FPbAvatarStageCountDiff::operator=(const idlepb::AvatarStageCountDiff& Right)
{
    this->FromPb(Right);
}

bool FPbAvatarStageCountDiff::operator==(const FPbAvatarStageCountDiff& Right) const
{
    if (this->diff_num != Right.diff_num)
        return false;
    if (this->coef != Right.coef)
        return false;
    return true;
}

bool FPbAvatarStageCountDiff::operator!=(const FPbAvatarStageCountDiff& Right) const
{
    return !operator==(Right);
}

FPbAnotherMeGlobalConfig::FPbAnotherMeGlobalConfig()
{
    Reset();        
}

FPbAnotherMeGlobalConfig::FPbAnotherMeGlobalConfig(const idlepb::AnotherMeGlobalConfig& Right)
{
    this->FromPb(Right);
}

void FPbAnotherMeGlobalConfig::FromPb(const idlepb::AnotherMeGlobalConfig& Right)
{
    temp_package_max = Right.temp_package_max();
    idle_time_max = Right.idle_time_max();
    idle_time_duration = Right.idle_time_duration();
    default_draw_time = Right.default_draw_time();
    stage_count_diff.Empty();
    for (const auto& Elem : Right.stage_count_diff())
    {
        stage_count_diff.Emplace(Elem);
    }
}

void FPbAnotherMeGlobalConfig::ToPb(idlepb::AnotherMeGlobalConfig* Out) const
{
    Out->set_temp_package_max(temp_package_max);
    Out->set_idle_time_max(idle_time_max);
    Out->set_idle_time_duration(idle_time_duration);
    Out->set_default_draw_time(default_draw_time);
    for (const auto& Elem : stage_count_diff)
    {
        Elem.ToPb(Out->add_stage_count_diff());    
    }    
}

void FPbAnotherMeGlobalConfig::Reset()
{
    temp_package_max = int32();
    idle_time_max = int32();
    idle_time_duration = int32();
    default_draw_time = int32();
    stage_count_diff = TArray<FPbAvatarStageCountDiff>();    
}

void FPbAnotherMeGlobalConfig::operator=(const idlepb::AnotherMeGlobalConfig& Right)
{
    this->FromPb(Right);
}

bool FPbAnotherMeGlobalConfig::operator==(const FPbAnotherMeGlobalConfig& Right) const
{
    if (this->temp_package_max != Right.temp_package_max)
        return false;
    if (this->idle_time_max != Right.idle_time_max)
        return false;
    if (this->idle_time_duration != Right.idle_time_duration)
        return false;
    if (this->default_draw_time != Right.default_draw_time)
        return false;
    if (this->stage_count_diff != Right.stage_count_diff)
        return false;
    return true;
}

bool FPbAnotherMeGlobalConfig::operator!=(const FPbAnotherMeGlobalConfig& Right) const
{
    return !operator==(Right);
}

FPbFuZeRewardNum::FPbFuZeRewardNum()
{
    Reset();        
}

FPbFuZeRewardNum::FPbFuZeRewardNum(const idlepb::FuZeRewardNum& Right)
{
    this->FromPb(Right);
}

void FPbFuZeRewardNum::FromPb(const idlepb::FuZeRewardNum& Right)
{
    rank = Right.rank();
    num = Right.num();
}

void FPbFuZeRewardNum::ToPb(idlepb::FuZeRewardNum* Out) const
{
    Out->set_rank(rank);
    Out->set_num(num);    
}

void FPbFuZeRewardNum::Reset()
{
    rank = int32();
    num = int32();    
}

void FPbFuZeRewardNum::operator=(const idlepb::FuZeRewardNum& Right)
{
    this->FromPb(Right);
}

bool FPbFuZeRewardNum::operator==(const FPbFuZeRewardNum& Right) const
{
    if (this->rank != Right.rank)
        return false;
    if (this->num != Right.num)
        return false;
    return true;
}

bool FPbFuZeRewardNum::operator!=(const FPbFuZeRewardNum& Right) const
{
    return !operator==(Right);
}

FPbFuZeRewardItemId::FPbFuZeRewardItemId()
{
    Reset();        
}

FPbFuZeRewardItemId::FPbFuZeRewardItemId(const idlepb::FuZeRewardItemId& Right)
{
    this->FromPb(Right);
}

void FPbFuZeRewardItemId::FromPb(const idlepb::FuZeRewardItemId& Right)
{
    degree = Right.degree();
    item_id = Right.item_id();
}

void FPbFuZeRewardItemId::ToPb(idlepb::FuZeRewardItemId* Out) const
{
    Out->set_degree(degree);
    Out->set_item_id(item_id);    
}

void FPbFuZeRewardItemId::Reset()
{
    degree = int32();
    item_id = int32();    
}

void FPbFuZeRewardItemId::operator=(const idlepb::FuZeRewardItemId& Right)
{
    this->FromPb(Right);
}

bool FPbFuZeRewardItemId::operator==(const FPbFuZeRewardItemId& Right) const
{
    if (this->degree != Right.degree)
        return false;
    if (this->item_id != Right.item_id)
        return false;
    return true;
}

bool FPbFuZeRewardItemId::operator!=(const FPbFuZeRewardItemId& Right) const
{
    return !operator==(Right);
}

FPbFuZeGlobalConfig::FPbFuZeGlobalConfig()
{
    Reset();        
}

FPbFuZeGlobalConfig::FPbFuZeGlobalConfig(const idlepb::FuZeGlobalConfig& Right)
{
    this->FromPb(Right);
}

void FPbFuZeGlobalConfig::FromPb(const idlepb::FuZeGlobalConfig& Right)
{
    reward_num.Empty();
    for (const auto& Elem : Right.reward_num())
    {
        reward_num.Emplace(Elem);
    }
    reward_id.Empty();
    for (const auto& Elem : Right.reward_id())
    {
        reward_id.Emplace(Elem);
    }
    fuze_rank_num = Right.fuze_rank_num();
    fuze_rank_ratio = Right.fuze_rank_ratio();
    fenqi_exp_ratio = Right.fenqi_exp_ratio();
    fuze_rank_min = Right.fuze_rank_min();
}

void FPbFuZeGlobalConfig::ToPb(idlepb::FuZeGlobalConfig* Out) const
{
    for (const auto& Elem : reward_num)
    {
        Elem.ToPb(Out->add_reward_num());    
    }
    for (const auto& Elem : reward_id)
    {
        Elem.ToPb(Out->add_reward_id());    
    }
    Out->set_fuze_rank_num(fuze_rank_num);
    Out->set_fuze_rank_ratio(fuze_rank_ratio);
    Out->set_fenqi_exp_ratio(fenqi_exp_ratio);
    Out->set_fuze_rank_min(fuze_rank_min);    
}

void FPbFuZeGlobalConfig::Reset()
{
    reward_num = TArray<FPbFuZeRewardNum>();
    reward_id = TArray<FPbFuZeRewardItemId>();
    fuze_rank_num = int32();
    fuze_rank_ratio = float();
    fenqi_exp_ratio = float();
    fuze_rank_min = int32();    
}

void FPbFuZeGlobalConfig::operator=(const idlepb::FuZeGlobalConfig& Right)
{
    this->FromPb(Right);
}

bool FPbFuZeGlobalConfig::operator==(const FPbFuZeGlobalConfig& Right) const
{
    if (this->reward_num != Right.reward_num)
        return false;
    if (this->reward_id != Right.reward_id)
        return false;
    if (this->fuze_rank_num != Right.fuze_rank_num)
        return false;
    if (this->fuze_rank_ratio != Right.fuze_rank_ratio)
        return false;
    if (this->fenqi_exp_ratio != Right.fenqi_exp_ratio)
        return false;
    if (this->fuze_rank_min != Right.fuze_rank_min)
        return false;
    return true;
}

bool FPbFuZeGlobalConfig::operator!=(const FPbFuZeGlobalConfig& Right) const
{
    return !operator==(Right);
}