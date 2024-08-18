#include "PbGame.h"
#include "game.pb.h"



FPbPing::FPbPing()
{
    Reset();        
}

FPbPing::FPbPing(const idlepb::Ping& Right)
{
    this->FromPb(Right);
}

void FPbPing::FromPb(const idlepb::Ping& Right)
{
    req_ticks = Right.req_ticks();
}

void FPbPing::ToPb(idlepb::Ping* Out) const
{
    Out->set_req_ticks(req_ticks);    
}

void FPbPing::Reset()
{
    req_ticks = int64();    
}

void FPbPing::operator=(const idlepb::Ping& Right)
{
    this->FromPb(Right);
}

bool FPbPing::operator==(const FPbPing& Right) const
{
    if (this->req_ticks != Right.req_ticks)
        return false;
    return true;
}

bool FPbPing::operator!=(const FPbPing& Right) const
{
    return !operator==(Right);
}

FPbPong::FPbPong()
{
    Reset();        
}

FPbPong::FPbPong(const idlepb::Pong& Right)
{
    this->FromPb(Right);
}

void FPbPong::FromPb(const idlepb::Pong& Right)
{
    req_ticks = Right.req_ticks();
    rsp_ticks = Right.rsp_ticks();
}

void FPbPong::ToPb(idlepb::Pong* Out) const
{
    Out->set_req_ticks(req_ticks);
    Out->set_rsp_ticks(rsp_ticks);    
}

void FPbPong::Reset()
{
    req_ticks = int64();
    rsp_ticks = int64();    
}

void FPbPong::operator=(const idlepb::Pong& Right)
{
    this->FromPb(Right);
}

bool FPbPong::operator==(const FPbPong& Right) const
{
    if (this->req_ticks != Right.req_ticks)
        return false;
    if (this->rsp_ticks != Right.rsp_ticks)
        return false;
    return true;
}

bool FPbPong::operator!=(const FPbPong& Right) const
{
    return !operator==(Right);
}

FPbDoGmCommand::FPbDoGmCommand()
{
    Reset();        
}

FPbDoGmCommand::FPbDoGmCommand(const idlepb::DoGmCommand& Right)
{
    this->FromPb(Right);
}

void FPbDoGmCommand::FromPb(const idlepb::DoGmCommand& Right)
{
    command = UTF8_TO_TCHAR(Right.command().c_str());
}

void FPbDoGmCommand::ToPb(idlepb::DoGmCommand* Out) const
{
    Out->set_command(TCHAR_TO_UTF8(*command));    
}

void FPbDoGmCommand::Reset()
{
    command = FString();    
}

void FPbDoGmCommand::operator=(const idlepb::DoGmCommand& Right)
{
    this->FromPb(Right);
}

bool FPbDoGmCommand::operator==(const FPbDoGmCommand& Right) const
{
    if (this->command != Right.command)
        return false;
    return true;
}

bool FPbDoGmCommand::operator!=(const FPbDoGmCommand& Right) const
{
    return !operator==(Right);
}

FPbReportError::FPbReportError()
{
    Reset();        
}

FPbReportError::FPbReportError(const idlepb::ReportError& Right)
{
    this->FromPb(Right);
}

void FPbReportError::FromPb(const idlepb::ReportError& Right)
{
    text = UTF8_TO_TCHAR(Right.text().c_str());
}

void FPbReportError::ToPb(idlepb::ReportError* Out) const
{
    Out->set_text(TCHAR_TO_UTF8(*text));    
}

void FPbReportError::Reset()
{
    text = FString();    
}

void FPbReportError::operator=(const idlepb::ReportError& Right)
{
    this->FromPb(Right);
}

bool FPbReportError::operator==(const FPbReportError& Right) const
{
    if (this->text != Right.text)
        return false;
    return true;
}

bool FPbReportError::operator!=(const FPbReportError& Right) const
{
    return !operator==(Right);
}

FPbLoginGameReq::FPbLoginGameReq()
{
    Reset();        
}

FPbLoginGameReq::FPbLoginGameReq(const idlepb::LoginGameReq& Right)
{
    this->FromPb(Right);
}

void FPbLoginGameReq::FromPb(const idlepb::LoginGameReq& Right)
{
    account = UTF8_TO_TCHAR(Right.account().c_str());
    client_version = UTF8_TO_TCHAR(Right.client_version().c_str());
}

void FPbLoginGameReq::ToPb(idlepb::LoginGameReq* Out) const
{
    Out->set_account(TCHAR_TO_UTF8(*account));
    Out->set_client_version(TCHAR_TO_UTF8(*client_version));    
}

void FPbLoginGameReq::Reset()
{
    account = FString();
    client_version = FString();    
}

void FPbLoginGameReq::operator=(const idlepb::LoginGameReq& Right)
{
    this->FromPb(Right);
}

bool FPbLoginGameReq::operator==(const FPbLoginGameReq& Right) const
{
    if (this->account != Right.account)
        return false;
    if (this->client_version != Right.client_version)
        return false;
    return true;
}

bool FPbLoginGameReq::operator!=(const FPbLoginGameReq& Right) const
{
    return !operator==(Right);
}

FPbLoginGameAck::FPbLoginGameAck()
{
    Reset();        
}

FPbLoginGameAck::FPbLoginGameAck(const idlepb::LoginGameAck& Right)
{
    this->FromPb(Right);
}

void FPbLoginGameAck::FromPb(const idlepb::LoginGameAck& Right)
{
    ret = static_cast<EPbLoginGameRetCode>(Right.ret());
    role_data = Right.role_data();
    is_relogin = Right.is_relogin();
    offline_award_summary = Right.offline_award_summary();
    sept_info = Right.sept_info();
}

void FPbLoginGameAck::ToPb(idlepb::LoginGameAck* Out) const
{
    Out->set_ret(static_cast<idlepb::LoginGameRetCode>(ret));
    role_data.ToPb(Out->mutable_role_data());
    Out->set_is_relogin(is_relogin);
    offline_award_summary.ToPb(Out->mutable_offline_award_summary());
    sept_info.ToPb(Out->mutable_sept_info());    
}

void FPbLoginGameAck::Reset()
{
    ret = EPbLoginGameRetCode();
    role_data = FPbRoleData();
    is_relogin = bool();
    offline_award_summary = FPbOfflineAwardSummary();
    sept_info = FPbSelfSeptInfo();    
}

void FPbLoginGameAck::operator=(const idlepb::LoginGameAck& Right)
{
    this->FromPb(Right);
}

bool FPbLoginGameAck::operator==(const FPbLoginGameAck& Right) const
{
    if (this->ret != Right.ret)
        return false;
    if (this->role_data != Right.role_data)
        return false;
    if (this->is_relogin != Right.is_relogin)
        return false;
    if (this->offline_award_summary != Right.offline_award_summary)
        return false;
    if (this->sept_info != Right.sept_info)
        return false;
    return true;
}

bool FPbLoginGameAck::operator!=(const FPbLoginGameAck& Right) const
{
    return !operator==(Right);
}

FPbRefreshInventoryData::FPbRefreshInventoryData()
{
    Reset();        
}

FPbRefreshInventoryData::FPbRefreshInventoryData(const idlepb::RefreshInventoryData& Right)
{
    this->FromPb(Right);
}

void FPbRefreshInventoryData::FromPb(const idlepb::RefreshInventoryData& Right)
{
    items.Empty();
    for (const auto& Elem : Right.items())
    {
        items.Emplace(Elem);
    }
}

void FPbRefreshInventoryData::ToPb(idlepb::RefreshInventoryData* Out) const
{
    for (const auto& Elem : items)
    {
        Elem.ToPb(Out->add_items());    
    }    
}

void FPbRefreshInventoryData::Reset()
{
    items = TArray<FPbItemData>();    
}

void FPbRefreshInventoryData::operator=(const idlepb::RefreshInventoryData& Right)
{
    this->FromPb(Right);
}

bool FPbRefreshInventoryData::operator==(const FPbRefreshInventoryData& Right) const
{
    if (this->items != Right.items)
        return false;
    return true;
}

bool FPbRefreshInventoryData::operator!=(const FPbRefreshInventoryData& Right) const
{
    return !operator==(Right);
}

FPbSetCurrentCultivationDirectionReq::FPbSetCurrentCultivationDirectionReq()
{
    Reset();        
}

FPbSetCurrentCultivationDirectionReq::FPbSetCurrentCultivationDirectionReq(const idlepb::SetCurrentCultivationDirectionReq& Right)
{
    this->FromPb(Right);
}

void FPbSetCurrentCultivationDirectionReq::FromPb(const idlepb::SetCurrentCultivationDirectionReq& Right)
{
    dir = static_cast<EPbCultivationDirection>(Right.dir());
}

void FPbSetCurrentCultivationDirectionReq::ToPb(idlepb::SetCurrentCultivationDirectionReq* Out) const
{
    Out->set_dir(static_cast<idlepb::CultivationDirection>(dir));    
}

void FPbSetCurrentCultivationDirectionReq::Reset()
{
    dir = EPbCultivationDirection();    
}

void FPbSetCurrentCultivationDirectionReq::operator=(const idlepb::SetCurrentCultivationDirectionReq& Right)
{
    this->FromPb(Right);
}

bool FPbSetCurrentCultivationDirectionReq::operator==(const FPbSetCurrentCultivationDirectionReq& Right) const
{
    if (this->dir != Right.dir)
        return false;
    return true;
}

bool FPbSetCurrentCultivationDirectionReq::operator!=(const FPbSetCurrentCultivationDirectionReq& Right) const
{
    return !operator==(Right);
}

FPbSetCurrentCultivationDirectionAck::FPbSetCurrentCultivationDirectionAck()
{
    Reset();        
}

FPbSetCurrentCultivationDirectionAck::FPbSetCurrentCultivationDirectionAck(const idlepb::SetCurrentCultivationDirectionAck& Right)
{
    this->FromPb(Right);
}

void FPbSetCurrentCultivationDirectionAck::FromPb(const idlepb::SetCurrentCultivationDirectionAck& Right)
{
    dir = static_cast<EPbCultivationDirection>(Right.dir());
}

void FPbSetCurrentCultivationDirectionAck::ToPb(idlepb::SetCurrentCultivationDirectionAck* Out) const
{
    Out->set_dir(static_cast<idlepb::CultivationDirection>(dir));    
}

void FPbSetCurrentCultivationDirectionAck::Reset()
{
    dir = EPbCultivationDirection();    
}

void FPbSetCurrentCultivationDirectionAck::operator=(const idlepb::SetCurrentCultivationDirectionAck& Right)
{
    this->FromPb(Right);
}

bool FPbSetCurrentCultivationDirectionAck::operator==(const FPbSetCurrentCultivationDirectionAck& Right) const
{
    if (this->dir != Right.dir)
        return false;
    return true;
}

bool FPbSetCurrentCultivationDirectionAck::operator!=(const FPbSetCurrentCultivationDirectionAck& Right) const
{
    return !operator==(Right);
}

FPbRefreshCurrentCultivationDirection::FPbRefreshCurrentCultivationDirection()
{
    Reset();        
}

FPbRefreshCurrentCultivationDirection::FPbRefreshCurrentCultivationDirection(const idlepb::RefreshCurrentCultivationDirection& Right)
{
    this->FromPb(Right);
}

void FPbRefreshCurrentCultivationDirection::FromPb(const idlepb::RefreshCurrentCultivationDirection& Right)
{
    dir = static_cast<EPbCultivationDirection>(Right.dir());
}

void FPbRefreshCurrentCultivationDirection::ToPb(idlepb::RefreshCurrentCultivationDirection* Out) const
{
    Out->set_dir(static_cast<idlepb::CultivationDirection>(dir));    
}

void FPbRefreshCurrentCultivationDirection::Reset()
{
    dir = EPbCultivationDirection();    
}

void FPbRefreshCurrentCultivationDirection::operator=(const idlepb::RefreshCurrentCultivationDirection& Right)
{
    this->FromPb(Right);
}

bool FPbRefreshCurrentCultivationDirection::operator==(const FPbRefreshCurrentCultivationDirection& Right) const
{
    if (this->dir != Right.dir)
        return false;
    return true;
}

bool FPbRefreshCurrentCultivationDirection::operator!=(const FPbRefreshCurrentCultivationDirection& Right) const
{
    return !operator==(Right);
}

FPbRefreshCultivationRankData::FPbRefreshCultivationRankData()
{
    Reset();        
}

FPbRefreshCultivationRankData::FPbRefreshCultivationRankData(const idlepb::RefreshCultivationRankData& Right)
{
    this->FromPb(Right);
}

void FPbRefreshCultivationRankData::FromPb(const idlepb::RefreshCultivationRankData& Right)
{
    rank_data = Right.rank_data();
    dir = static_cast<EPbCultivationDirection>(Right.dir());
    last_exp_cycle_timestamp = Right.last_exp_cycle_timestamp();
}

void FPbRefreshCultivationRankData::ToPb(idlepb::RefreshCultivationRankData* Out) const
{
    rank_data.ToPb(Out->mutable_rank_data());
    Out->set_dir(static_cast<idlepb::CultivationDirection>(dir));
    Out->set_last_exp_cycle_timestamp(last_exp_cycle_timestamp);    
}

void FPbRefreshCultivationRankData::Reset()
{
    rank_data = FPbRankData();
    dir = EPbCultivationDirection();
    last_exp_cycle_timestamp = int64();    
}

void FPbRefreshCultivationRankData::operator=(const idlepb::RefreshCultivationRankData& Right)
{
    this->FromPb(Right);
}

bool FPbRefreshCultivationRankData::operator==(const FPbRefreshCultivationRankData& Right) const
{
    if (this->rank_data != Right.rank_data)
        return false;
    if (this->dir != Right.dir)
        return false;
    if (this->last_exp_cycle_timestamp != Right.last_exp_cycle_timestamp)
        return false;
    return true;
}

bool FPbRefreshCultivationRankData::operator!=(const FPbRefreshCultivationRankData& Right) const
{
    return !operator==(Right);
}

FPbRefreshCultivationData::FPbRefreshCultivationData()
{
    Reset();        
}

FPbRefreshCultivationData::FPbRefreshCultivationData(const idlepb::RefreshCultivationData& Right)
{
    this->FromPb(Right);
}

void FPbRefreshCultivationData::FromPb(const idlepb::RefreshCultivationData& Right)
{
    cultivation_data = Right.cultivation_data();
    dir = static_cast<EPbCultivationDirection>(Right.dir());
}

void FPbRefreshCultivationData::ToPb(idlepb::RefreshCultivationData* Out) const
{
    cultivation_data.ToPb(Out->mutable_cultivation_data());
    Out->set_dir(static_cast<idlepb::CultivationDirection>(dir));    
}

void FPbRefreshCultivationData::Reset()
{
    cultivation_data = FPbCultivationData();
    dir = EPbCultivationDirection();    
}

void FPbRefreshCultivationData::operator=(const idlepb::RefreshCultivationData& Right)
{
    this->FromPb(Right);
}

bool FPbRefreshCultivationData::operator==(const FPbRefreshCultivationData& Right) const
{
    if (this->cultivation_data != Right.cultivation_data)
        return false;
    if (this->dir != Right.dir)
        return false;
    return true;
}

bool FPbRefreshCultivationData::operator!=(const FPbRefreshCultivationData& Right) const
{
    return !operator==(Right);
}

FPbRefreshCurrencyData::FPbRefreshCurrencyData()
{
    Reset();        
}

FPbRefreshCurrencyData::FPbRefreshCurrencyData(const idlepb::RefreshCurrencyData& Right)
{
    this->FromPb(Right);
}

void FPbRefreshCurrencyData::FromPb(const idlepb::RefreshCurrencyData& Right)
{
    data = Right.data();
}

void FPbRefreshCurrencyData::ToPb(idlepb::RefreshCurrencyData* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbRefreshCurrencyData::Reset()
{
    data = FPbCurrencyData();    
}

void FPbRefreshCurrencyData::operator=(const idlepb::RefreshCurrencyData& Right)
{
    this->FromPb(Right);
}

bool FPbRefreshCurrencyData::operator==(const FPbRefreshCurrencyData& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbRefreshCurrencyData::operator!=(const FPbRefreshCurrencyData& Right) const
{
    return !operator==(Right);
}

FPbRefreshDailyCounterData::FPbRefreshDailyCounterData()
{
    Reset();        
}

FPbRefreshDailyCounterData::FPbRefreshDailyCounterData(const idlepb::RefreshDailyCounterData& Right)
{
    this->FromPb(Right);
}

void FPbRefreshDailyCounterData::FromPb(const idlepb::RefreshDailyCounterData& Right)
{
    daily_counter = Right.daily_counter();
    weekly_counter = Right.weekly_counter();
}

void FPbRefreshDailyCounterData::ToPb(idlepb::RefreshDailyCounterData* Out) const
{
    daily_counter.ToPb(Out->mutable_daily_counter());
    weekly_counter.ToPb(Out->mutable_weekly_counter());    
}

void FPbRefreshDailyCounterData::Reset()
{
    daily_counter = FPbRoleDailyCounter();
    weekly_counter = FPbRoleWeeklyCounter();    
}

void FPbRefreshDailyCounterData::operator=(const idlepb::RefreshDailyCounterData& Right)
{
    this->FromPb(Right);
}

bool FPbRefreshDailyCounterData::operator==(const FPbRefreshDailyCounterData& Right) const
{
    if (this->daily_counter != Right.daily_counter)
        return false;
    if (this->weekly_counter != Right.weekly_counter)
        return false;
    return true;
}

bool FPbRefreshDailyCounterData::operator!=(const FPbRefreshDailyCounterData& Right) const
{
    return !operator==(Right);
}

FPbRefreshLastUnlockArenaId::FPbRefreshLastUnlockArenaId()
{
    Reset();        
}

FPbRefreshLastUnlockArenaId::FPbRefreshLastUnlockArenaId(const idlepb::RefreshLastUnlockArenaId& Right)
{
    this->FromPb(Right);
}

void FPbRefreshLastUnlockArenaId::FromPb(const idlepb::RefreshLastUnlockArenaId& Right)
{
    last_unlock_arena_id = Right.last_unlock_arena_id();
}

void FPbRefreshLastUnlockArenaId::ToPb(idlepb::RefreshLastUnlockArenaId* Out) const
{
    Out->set_last_unlock_arena_id(last_unlock_arena_id);    
}

void FPbRefreshLastUnlockArenaId::Reset()
{
    last_unlock_arena_id = int32();    
}

void FPbRefreshLastUnlockArenaId::operator=(const idlepb::RefreshLastUnlockArenaId& Right)
{
    this->FromPb(Right);
}

bool FPbRefreshLastUnlockArenaId::operator==(const FPbRefreshLastUnlockArenaId& Right) const
{
    if (this->last_unlock_arena_id != Right.last_unlock_arena_id)
        return false;
    return true;
}

bool FPbRefreshLastUnlockArenaId::operator!=(const FPbRefreshLastUnlockArenaId& Right) const
{
    return !operator==(Right);
}

FPbRefreshUnlockedEquipmentSlots::FPbRefreshUnlockedEquipmentSlots()
{
    Reset();        
}

FPbRefreshUnlockedEquipmentSlots::FPbRefreshUnlockedEquipmentSlots(const idlepb::RefreshUnlockedEquipmentSlots& Right)
{
    this->FromPb(Right);
}

void FPbRefreshUnlockedEquipmentSlots::FromPb(const idlepb::RefreshUnlockedEquipmentSlots& Right)
{
    slots.Empty();
    for (const auto& Elem : Right.slots())
    {
        slots.Emplace(Elem);
    }
}

void FPbRefreshUnlockedEquipmentSlots::ToPb(idlepb::RefreshUnlockedEquipmentSlots* Out) const
{
    for (const auto& Elem : slots)
    {
        Out->add_slots(Elem);    
    }    
}

void FPbRefreshUnlockedEquipmentSlots::Reset()
{
    slots = TArray<int32>();    
}

void FPbRefreshUnlockedEquipmentSlots::operator=(const idlepb::RefreshUnlockedEquipmentSlots& Right)
{
    this->FromPb(Right);
}

bool FPbRefreshUnlockedEquipmentSlots::operator==(const FPbRefreshUnlockedEquipmentSlots& Right) const
{
    if (this->slots != Right.slots)
        return false;
    return true;
}

bool FPbRefreshUnlockedEquipmentSlots::operator!=(const FPbRefreshUnlockedEquipmentSlots& Right) const
{
    return !operator==(Right);
}

FPbUnlockEquipmentSlotReq::FPbUnlockEquipmentSlotReq()
{
    Reset();        
}

FPbUnlockEquipmentSlotReq::FPbUnlockEquipmentSlotReq(const idlepb::UnlockEquipmentSlotReq& Right)
{
    this->FromPb(Right);
}

void FPbUnlockEquipmentSlotReq::FromPb(const idlepb::UnlockEquipmentSlotReq& Right)
{
    index = Right.index();
}

void FPbUnlockEquipmentSlotReq::ToPb(idlepb::UnlockEquipmentSlotReq* Out) const
{
    Out->set_index(index);    
}

void FPbUnlockEquipmentSlotReq::Reset()
{
    index = int32();    
}

void FPbUnlockEquipmentSlotReq::operator=(const idlepb::UnlockEquipmentSlotReq& Right)
{
    this->FromPb(Right);
}

bool FPbUnlockEquipmentSlotReq::operator==(const FPbUnlockEquipmentSlotReq& Right) const
{
    if (this->index != Right.index)
        return false;
    return true;
}

bool FPbUnlockEquipmentSlotReq::operator!=(const FPbUnlockEquipmentSlotReq& Right) const
{
    return !operator==(Right);
}

FPbUnlockEquipmentSlotAck::FPbUnlockEquipmentSlotAck()
{
    Reset();        
}

FPbUnlockEquipmentSlotAck::FPbUnlockEquipmentSlotAck(const idlepb::UnlockEquipmentSlotAck& Right)
{
    this->FromPb(Right);
}

void FPbUnlockEquipmentSlotAck::FromPb(const idlepb::UnlockEquipmentSlotAck& Right)
{
    ok = Right.ok();
}

void FPbUnlockEquipmentSlotAck::ToPb(idlepb::UnlockEquipmentSlotAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbUnlockEquipmentSlotAck::Reset()
{
    ok = bool();    
}

void FPbUnlockEquipmentSlotAck::operator=(const idlepb::UnlockEquipmentSlotAck& Right)
{
    this->FromPb(Right);
}

bool FPbUnlockEquipmentSlotAck::operator==(const FPbUnlockEquipmentSlotAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbUnlockEquipmentSlotAck::operator!=(const FPbUnlockEquipmentSlotAck& Right) const
{
    return !operator==(Right);
}

FPbThunderTestRoundData::FPbThunderTestRoundData()
{
    Reset();        
}

FPbThunderTestRoundData::FPbThunderTestRoundData(const idlepb::ThunderTestRoundData& Right)
{
    this->FromPb(Right);
}

void FPbThunderTestRoundData::FromPb(const idlepb::ThunderTestRoundData& Right)
{
    round = Right.round();
    damage = Right.damage();
    hp = Right.hp();
    mp = Right.mp();
}

void FPbThunderTestRoundData::ToPb(idlepb::ThunderTestRoundData* Out) const
{
    Out->set_round(round);
    Out->set_damage(damage);
    Out->set_hp(hp);
    Out->set_mp(mp);    
}

void FPbThunderTestRoundData::Reset()
{
    round = int32();
    damage = float();
    hp = float();
    mp = float();    
}

void FPbThunderTestRoundData::operator=(const idlepb::ThunderTestRoundData& Right)
{
    this->FromPb(Right);
}

bool FPbThunderTestRoundData::operator==(const FPbThunderTestRoundData& Right) const
{
    if (this->round != Right.round)
        return false;
    if (this->damage != Right.damage)
        return false;
    if (this->hp != Right.hp)
        return false;
    if (this->mp != Right.mp)
        return false;
    return true;
}

bool FPbThunderTestRoundData::operator!=(const FPbThunderTestRoundData& Right) const
{
    return !operator==(Right);
}

FPbThunderTestData::FPbThunderTestData()
{
    Reset();        
}

FPbThunderTestData::FPbThunderTestData(const idlepb::ThunderTestData& Right)
{
    this->FromPb(Right);
}

void FPbThunderTestData::FromPb(const idlepb::ThunderTestData& Right)
{
    hp = Right.hp();
    mp = Right.mp();
    rounds.Empty();
    for (const auto& Elem : Right.rounds())
    {
        rounds.Emplace(Elem);
    }
}

void FPbThunderTestData::ToPb(idlepb::ThunderTestData* Out) const
{
    Out->set_hp(hp);
    Out->set_mp(mp);
    for (const auto& Elem : rounds)
    {
        Elem.ToPb(Out->add_rounds());    
    }    
}

void FPbThunderTestData::Reset()
{
    hp = float();
    mp = float();
    rounds = TArray<FPbThunderTestRoundData>();    
}

void FPbThunderTestData::operator=(const idlepb::ThunderTestData& Right)
{
    this->FromPb(Right);
}

bool FPbThunderTestData::operator==(const FPbThunderTestData& Right) const
{
    if (this->hp != Right.hp)
        return false;
    if (this->mp != Right.mp)
        return false;
    if (this->rounds != Right.rounds)
        return false;
    return true;
}

bool FPbThunderTestData::operator!=(const FPbThunderTestData& Right) const
{
    return !operator==(Right);
}

FPbDoBreakthroughReq::FPbDoBreakthroughReq()
{
    Reset();        
}

FPbDoBreakthroughReq::FPbDoBreakthroughReq(const idlepb::DoBreakthroughReq& Right)
{
    this->FromPb(Right);
}

void FPbDoBreakthroughReq::FromPb(const idlepb::DoBreakthroughReq& Right)
{
    item_id = Right.item_id();
}

void FPbDoBreakthroughReq::ToPb(idlepb::DoBreakthroughReq* Out) const
{
    Out->set_item_id(item_id);    
}

void FPbDoBreakthroughReq::Reset()
{
    item_id = int64();    
}

void FPbDoBreakthroughReq::operator=(const idlepb::DoBreakthroughReq& Right)
{
    this->FromPb(Right);
}

bool FPbDoBreakthroughReq::operator==(const FPbDoBreakthroughReq& Right) const
{
    if (this->item_id != Right.item_id)
        return false;
    return true;
}

bool FPbDoBreakthroughReq::operator!=(const FPbDoBreakthroughReq& Right) const
{
    return !operator==(Right);
}

FPbDoBreakthroughAck::FPbDoBreakthroughAck()
{
    Reset();        
}

FPbDoBreakthroughAck::FPbDoBreakthroughAck(const idlepb::DoBreakthroughAck& Right)
{
    this->FromPb(Right);
}

void FPbDoBreakthroughAck::FromPb(const idlepb::DoBreakthroughAck& Right)
{
    success = Right.success();
    old_type = static_cast<EPbBreakthroughType>(Right.old_type());
    new_type = static_cast<EPbBreakthroughType>(Right.new_type());
    is_bottleneck = Right.is_bottleneck();
    thunder_test_data = Right.thunder_test_data();
}

void FPbDoBreakthroughAck::ToPb(idlepb::DoBreakthroughAck* Out) const
{
    Out->set_success(success);
    Out->set_old_type(static_cast<idlepb::BreakthroughType>(old_type));
    Out->set_new_type(static_cast<idlepb::BreakthroughType>(new_type));
    Out->set_is_bottleneck(is_bottleneck);
    thunder_test_data.ToPb(Out->mutable_thunder_test_data());    
}

void FPbDoBreakthroughAck::Reset()
{
    success = bool();
    old_type = EPbBreakthroughType();
    new_type = EPbBreakthroughType();
    is_bottleneck = bool();
    thunder_test_data = FPbThunderTestData();    
}

void FPbDoBreakthroughAck::operator=(const idlepb::DoBreakthroughAck& Right)
{
    this->FromPb(Right);
}

bool FPbDoBreakthroughAck::operator==(const FPbDoBreakthroughAck& Right) const
{
    if (this->success != Right.success)
        return false;
    if (this->old_type != Right.old_type)
        return false;
    if (this->new_type != Right.new_type)
        return false;
    if (this->is_bottleneck != Right.is_bottleneck)
        return false;
    if (this->thunder_test_data != Right.thunder_test_data)
        return false;
    return true;
}

bool FPbDoBreakthroughAck::operator!=(const FPbDoBreakthroughAck& Right) const
{
    return !operator==(Right);
}

FPbRefreshItems::FPbRefreshItems()
{
    Reset();        
}

FPbRefreshItems::FPbRefreshItems(const idlepb::RefreshItems& Right)
{
    this->FromPb(Right);
}

void FPbRefreshItems::FromPb(const idlepb::RefreshItems& Right)
{
    items.Empty();
    for (const auto& Elem : Right.items())
    {
        items.Emplace(Elem);
    }
    junks.Empty();
    for (const auto& Elem : Right.junks())
    {
        junks.Emplace(Elem);
    }
    others.Empty();
    for (const auto& Elem : Right.others())
    {
        others.Emplace(Elem);
    }
    quiet_items.Empty();
    for (const auto& Elem : Right.quiet_items())
    {
        quiet_items.Emplace(Elem);
    }
}

void FPbRefreshItems::ToPb(idlepb::RefreshItems* Out) const
{
    for (const auto& Elem : items)
    {
        Elem.ToPb(Out->add_items());    
    }
    for (const auto& Elem : junks)
    {
        Out->add_junks(Elem);    
    }
    for (const auto& Elem : others)
    {
        Elem.ToPb(Out->add_others());    
    }
    for (const auto& Elem : quiet_items)
    {
        Elem.ToPb(Out->add_quiet_items());    
    }    
}

void FPbRefreshItems::Reset()
{
    items = TArray<FPbItemData>();
    junks = TArray<int64>();
    others = TArray<FPbSimpleItemData>();
    quiet_items = TArray<FPbItemData>();    
}

void FPbRefreshItems::operator=(const idlepb::RefreshItems& Right)
{
    this->FromPb(Right);
}

bool FPbRefreshItems::operator==(const FPbRefreshItems& Right) const
{
    if (this->items != Right.items)
        return false;
    if (this->junks != Right.junks)
        return false;
    if (this->others != Right.others)
        return false;
    if (this->quiet_items != Right.quiet_items)
        return false;
    return true;
}

bool FPbRefreshItems::operator!=(const FPbRefreshItems& Right) const
{
    return !operator==(Right);
}

FPbRefreshTemporaryPackageItems::FPbRefreshTemporaryPackageItems()
{
    Reset();        
}

FPbRefreshTemporaryPackageItems::FPbRefreshTemporaryPackageItems(const idlepb::RefreshTemporaryPackageItems& Right)
{
    this->FromPb(Right);
}

void FPbRefreshTemporaryPackageItems::FromPb(const idlepb::RefreshTemporaryPackageItems& Right)
{
    items.Empty();
    for (const auto& Elem : Right.items())
    {
        items.Emplace(Elem);
    }
    total_num = Right.total_num();
    last_extract_time = Right.last_extract_time();
}

void FPbRefreshTemporaryPackageItems::ToPb(idlepb::RefreshTemporaryPackageItems* Out) const
{
    for (const auto& Elem : items)
    {
        Elem.ToPb(Out->add_items());    
    }
    Out->set_total_num(total_num);
    Out->set_last_extract_time(last_extract_time);    
}

void FPbRefreshTemporaryPackageItems::Reset()
{
    items = TArray<FPbTemporaryPackageItem>();
    total_num = int32();
    last_extract_time = int64();    
}

void FPbRefreshTemporaryPackageItems::operator=(const idlepb::RefreshTemporaryPackageItems& Right)
{
    this->FromPb(Right);
}

bool FPbRefreshTemporaryPackageItems::operator==(const FPbRefreshTemporaryPackageItems& Right) const
{
    if (this->items != Right.items)
        return false;
    if (this->total_num != Right.total_num)
        return false;
    if (this->last_extract_time != Right.last_extract_time)
        return false;
    return true;
}

bool FPbRefreshTemporaryPackageItems::operator!=(const FPbRefreshTemporaryPackageItems& Right) const
{
    return !operator==(Right);
}

FPbExtractTemporaryPackageItemsReq::FPbExtractTemporaryPackageItemsReq()
{
    Reset();        
}

FPbExtractTemporaryPackageItemsReq::FPbExtractTemporaryPackageItemsReq(const idlepb::ExtractTemporaryPackageItemsReq& Right)
{
    this->FromPb(Right);
}

void FPbExtractTemporaryPackageItemsReq::FromPb(const idlepb::ExtractTemporaryPackageItemsReq& Right)
{
}

void FPbExtractTemporaryPackageItemsReq::ToPb(idlepb::ExtractTemporaryPackageItemsReq* Out) const
{    
}

void FPbExtractTemporaryPackageItemsReq::Reset()
{    
}

void FPbExtractTemporaryPackageItemsReq::operator=(const idlepb::ExtractTemporaryPackageItemsReq& Right)
{
    this->FromPb(Right);
}

bool FPbExtractTemporaryPackageItemsReq::operator==(const FPbExtractTemporaryPackageItemsReq& Right) const
{
    return true;
}

bool FPbExtractTemporaryPackageItemsReq::operator!=(const FPbExtractTemporaryPackageItemsReq& Right) const
{
    return !operator==(Right);
}

FPbExtractTemporaryPackageItemsAck::FPbExtractTemporaryPackageItemsAck()
{
    Reset();        
}

FPbExtractTemporaryPackageItemsAck::FPbExtractTemporaryPackageItemsAck(const idlepb::ExtractTemporaryPackageItemsAck& Right)
{
    this->FromPb(Right);
}

void FPbExtractTemporaryPackageItemsAck::FromPb(const idlepb::ExtractTemporaryPackageItemsAck& Right)
{
}

void FPbExtractTemporaryPackageItemsAck::ToPb(idlepb::ExtractTemporaryPackageItemsAck* Out) const
{    
}

void FPbExtractTemporaryPackageItemsAck::Reset()
{    
}

void FPbExtractTemporaryPackageItemsAck::operator=(const idlepb::ExtractTemporaryPackageItemsAck& Right)
{
    this->FromPb(Right);
}

bool FPbExtractTemporaryPackageItemsAck::operator==(const FPbExtractTemporaryPackageItemsAck& Right) const
{
    return true;
}

bool FPbExtractTemporaryPackageItemsAck::operator!=(const FPbExtractTemporaryPackageItemsAck& Right) const
{
    return !operator==(Right);
}

FPbGetTemporaryPackageDataReq::FPbGetTemporaryPackageDataReq()
{
    Reset();        
}

FPbGetTemporaryPackageDataReq::FPbGetTemporaryPackageDataReq(const idlepb::GetTemporaryPackageDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetTemporaryPackageDataReq::FromPb(const idlepb::GetTemporaryPackageDataReq& Right)
{
}

void FPbGetTemporaryPackageDataReq::ToPb(idlepb::GetTemporaryPackageDataReq* Out) const
{    
}

void FPbGetTemporaryPackageDataReq::Reset()
{    
}

void FPbGetTemporaryPackageDataReq::operator=(const idlepb::GetTemporaryPackageDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetTemporaryPackageDataReq::operator==(const FPbGetTemporaryPackageDataReq& Right) const
{
    return true;
}

bool FPbGetTemporaryPackageDataReq::operator!=(const FPbGetTemporaryPackageDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetTemporaryPackageDataAck::FPbGetTemporaryPackageDataAck()
{
    Reset();        
}

FPbGetTemporaryPackageDataAck::FPbGetTemporaryPackageDataAck(const idlepb::GetTemporaryPackageDataAck& Right)
{
    this->FromPb(Right);
}

void FPbGetTemporaryPackageDataAck::FromPb(const idlepb::GetTemporaryPackageDataAck& Right)
{
    items.Empty();
    for (const auto& Elem : Right.items())
    {
        items.Emplace(Elem);
    }
    last_extract_time = Right.last_extract_time();
}

void FPbGetTemporaryPackageDataAck::ToPb(idlepb::GetTemporaryPackageDataAck* Out) const
{
    for (const auto& Elem : items)
    {
        Elem.ToPb(Out->add_items());    
    }
    Out->set_last_extract_time(last_extract_time);    
}

void FPbGetTemporaryPackageDataAck::Reset()
{
    items = TArray<FPbTemporaryPackageItem>();
    last_extract_time = int64();    
}

void FPbGetTemporaryPackageDataAck::operator=(const idlepb::GetTemporaryPackageDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetTemporaryPackageDataAck::operator==(const FPbGetTemporaryPackageDataAck& Right) const
{
    if (this->items != Right.items)
        return false;
    if (this->last_extract_time != Right.last_extract_time)
        return false;
    return true;
}

bool FPbGetTemporaryPackageDataAck::operator!=(const FPbGetTemporaryPackageDataAck& Right) const
{
    return !operator==(Right);
}

FPbGetArenaExplorationStatisticalDataReq::FPbGetArenaExplorationStatisticalDataReq()
{
    Reset();        
}

FPbGetArenaExplorationStatisticalDataReq::FPbGetArenaExplorationStatisticalDataReq(const idlepb::GetArenaExplorationStatisticalDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetArenaExplorationStatisticalDataReq::FromPb(const idlepb::GetArenaExplorationStatisticalDataReq& Right)
{
}

void FPbGetArenaExplorationStatisticalDataReq::ToPb(idlepb::GetArenaExplorationStatisticalDataReq* Out) const
{    
}

void FPbGetArenaExplorationStatisticalDataReq::Reset()
{    
}

void FPbGetArenaExplorationStatisticalDataReq::operator=(const idlepb::GetArenaExplorationStatisticalDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetArenaExplorationStatisticalDataReq::operator==(const FPbGetArenaExplorationStatisticalDataReq& Right) const
{
    return true;
}

bool FPbGetArenaExplorationStatisticalDataReq::operator!=(const FPbGetArenaExplorationStatisticalDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetArenaExplorationStatisticalDataAck::FPbGetArenaExplorationStatisticalDataAck()
{
    Reset();        
}

FPbGetArenaExplorationStatisticalDataAck::FPbGetArenaExplorationStatisticalDataAck(const idlepb::GetArenaExplorationStatisticalDataAck& Right)
{
    this->FromPb(Right);
}

void FPbGetArenaExplorationStatisticalDataAck::FromPb(const idlepb::GetArenaExplorationStatisticalDataAck& Right)
{
    data = Right.data();
}

void FPbGetArenaExplorationStatisticalDataAck::ToPb(idlepb::GetArenaExplorationStatisticalDataAck* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbGetArenaExplorationStatisticalDataAck::Reset()
{
    data = FPbRoleArenaExplorationStatisticalData();    
}

void FPbGetArenaExplorationStatisticalDataAck::operator=(const idlepb::GetArenaExplorationStatisticalDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetArenaExplorationStatisticalDataAck::operator==(const FPbGetArenaExplorationStatisticalDataAck& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbGetArenaExplorationStatisticalDataAck::operator!=(const FPbGetArenaExplorationStatisticalDataAck& Right) const
{
    return !operator==(Right);
}

FPbDoBreathingExerciseReq::FPbDoBreathingExerciseReq()
{
    Reset();        
}

FPbDoBreathingExerciseReq::FPbDoBreathingExerciseReq(const idlepb::DoBreathingExerciseReq& Right)
{
    this->FromPb(Right);
}

void FPbDoBreathingExerciseReq::FromPb(const idlepb::DoBreathingExerciseReq& Right)
{
    percet = Right.percet();
}

void FPbDoBreathingExerciseReq::ToPb(idlepb::DoBreathingExerciseReq* Out) const
{
    Out->set_percet(percet);    
}

void FPbDoBreathingExerciseReq::Reset()
{
    percet = float();    
}

void FPbDoBreathingExerciseReq::operator=(const idlepb::DoBreathingExerciseReq& Right)
{
    this->FromPb(Right);
}

bool FPbDoBreathingExerciseReq::operator==(const FPbDoBreathingExerciseReq& Right) const
{
    if (this->percet != Right.percet)
        return false;
    return true;
}

bool FPbDoBreathingExerciseReq::operator!=(const FPbDoBreathingExerciseReq& Right) const
{
    return !operator==(Right);
}

FPbDoBreathingExerciseAck::FPbDoBreathingExerciseAck()
{
    Reset();        
}

FPbDoBreathingExerciseAck::FPbDoBreathingExerciseAck(const idlepb::DoBreathingExerciseAck& Right)
{
    this->FromPb(Right);
}

void FPbDoBreathingExerciseAck::FromPb(const idlepb::DoBreathingExerciseAck& Right)
{
    result = Right.result();
}

void FPbDoBreathingExerciseAck::ToPb(idlepb::DoBreathingExerciseAck* Out) const
{
    result.ToPb(Out->mutable_result());    
}

void FPbDoBreathingExerciseAck::Reset()
{
    result = FPbDoBreathingExerciseResult();    
}

void FPbDoBreathingExerciseAck::operator=(const idlepb::DoBreathingExerciseAck& Right)
{
    this->FromPb(Right);
}

bool FPbDoBreathingExerciseAck::operator==(const FPbDoBreathingExerciseAck& Right) const
{
    if (this->result != Right.result)
        return false;
    return true;
}

bool FPbDoBreathingExerciseAck::operator!=(const FPbDoBreathingExerciseAck& Right) const
{
    return !operator==(Right);
}

FPbOneClickMergeBreathingReq::FPbOneClickMergeBreathingReq()
{
    Reset();        
}

FPbOneClickMergeBreathingReq::FPbOneClickMergeBreathingReq(const idlepb::OneClickMergeBreathingReq& Right)
{
    this->FromPb(Right);
}

void FPbOneClickMergeBreathingReq::FromPb(const idlepb::OneClickMergeBreathingReq& Right)
{
}

void FPbOneClickMergeBreathingReq::ToPb(idlepb::OneClickMergeBreathingReq* Out) const
{    
}

void FPbOneClickMergeBreathingReq::Reset()
{    
}

void FPbOneClickMergeBreathingReq::operator=(const idlepb::OneClickMergeBreathingReq& Right)
{
    this->FromPb(Right);
}

bool FPbOneClickMergeBreathingReq::operator==(const FPbOneClickMergeBreathingReq& Right) const
{
    return true;
}

bool FPbOneClickMergeBreathingReq::operator!=(const FPbOneClickMergeBreathingReq& Right) const
{
    return !operator==(Right);
}

FPbOneClickMergeBreathingAck::FPbOneClickMergeBreathingAck()
{
    Reset();        
}

FPbOneClickMergeBreathingAck::FPbOneClickMergeBreathingAck(const idlepb::OneClickMergeBreathingAck& Right)
{
    this->FromPb(Right);
}

void FPbOneClickMergeBreathingAck::FromPb(const idlepb::OneClickMergeBreathingAck& Right)
{
    exp.Empty();
    for (const auto& Elem : Right.exp())
    {
        exp.Emplace(Elem);
    }
    ret.Empty();
    for (const auto& Elem : Right.ret())
    {
        ret.Emplace(Elem);
    }
}

void FPbOneClickMergeBreathingAck::ToPb(idlepb::OneClickMergeBreathingAck* Out) const
{
    for (const auto& Elem : exp)
    {
        Out->add_exp(Elem);    
    }
    for (const auto& Elem : ret)
    {
        Out->add_ret(Elem);    
    }    
}

void FPbOneClickMergeBreathingAck::Reset()
{
    exp = TArray<float>();
    ret = TArray<float>();    
}

void FPbOneClickMergeBreathingAck::operator=(const idlepb::OneClickMergeBreathingAck& Right)
{
    this->FromPb(Right);
}

bool FPbOneClickMergeBreathingAck::operator==(const FPbOneClickMergeBreathingAck& Right) const
{
    if (this->exp != Right.exp)
        return false;
    if (this->ret != Right.ret)
        return false;
    return true;
}

bool FPbOneClickMergeBreathingAck::operator!=(const FPbOneClickMergeBreathingAck& Right) const
{
    return !operator==(Right);
}

FPbRequestCommonCultivationDataReq::FPbRequestCommonCultivationDataReq()
{
    Reset();        
}

FPbRequestCommonCultivationDataReq::FPbRequestCommonCultivationDataReq(const idlepb::RequestCommonCultivationDataReq& Right)
{
    this->FromPb(Right);
}

void FPbRequestCommonCultivationDataReq::FromPb(const idlepb::RequestCommonCultivationDataReq& Right)
{
}

void FPbRequestCommonCultivationDataReq::ToPb(idlepb::RequestCommonCultivationDataReq* Out) const
{    
}

void FPbRequestCommonCultivationDataReq::Reset()
{    
}

void FPbRequestCommonCultivationDataReq::operator=(const idlepb::RequestCommonCultivationDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbRequestCommonCultivationDataReq::operator==(const FPbRequestCommonCultivationDataReq& Right) const
{
    return true;
}

bool FPbRequestCommonCultivationDataReq::operator!=(const FPbRequestCommonCultivationDataReq& Right) const
{
    return !operator==(Right);
}

FPbRequestCommonCultivationDataAck::FPbRequestCommonCultivationDataAck()
{
    Reset();        
}

FPbRequestCommonCultivationDataAck::FPbRequestCommonCultivationDataAck(const idlepb::RequestCommonCultivationDataAck& Right)
{
    this->FromPb(Right);
}

void FPbRequestCommonCultivationDataAck::FromPb(const idlepb::RequestCommonCultivationDataAck& Right)
{
    data = Right.data();
}

void FPbRequestCommonCultivationDataAck::ToPb(idlepb::RequestCommonCultivationDataAck* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbRequestCommonCultivationDataAck::Reset()
{
    data = FPbCommonCultivationData();    
}

void FPbRequestCommonCultivationDataAck::operator=(const idlepb::RequestCommonCultivationDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbRequestCommonCultivationDataAck::operator==(const FPbRequestCommonCultivationDataAck& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbRequestCommonCultivationDataAck::operator!=(const FPbRequestCommonCultivationDataAck& Right) const
{
    return !operator==(Right);
}

FPbReceiveBreathingExerciseRewardReq::FPbReceiveBreathingExerciseRewardReq()
{
    Reset();        
}

FPbReceiveBreathingExerciseRewardReq::FPbReceiveBreathingExerciseRewardReq(const idlepb::ReceiveBreathingExerciseRewardReq& Right)
{
    this->FromPb(Right);
}

void FPbReceiveBreathingExerciseRewardReq::FromPb(const idlepb::ReceiveBreathingExerciseRewardReq& Right)
{
    index = Right.index();
}

void FPbReceiveBreathingExerciseRewardReq::ToPb(idlepb::ReceiveBreathingExerciseRewardReq* Out) const
{
    Out->set_index(index);    
}

void FPbReceiveBreathingExerciseRewardReq::Reset()
{
    index = int32();    
}

void FPbReceiveBreathingExerciseRewardReq::operator=(const idlepb::ReceiveBreathingExerciseRewardReq& Right)
{
    this->FromPb(Right);
}

bool FPbReceiveBreathingExerciseRewardReq::operator==(const FPbReceiveBreathingExerciseRewardReq& Right) const
{
    if (this->index != Right.index)
        return false;
    return true;
}

bool FPbReceiveBreathingExerciseRewardReq::operator!=(const FPbReceiveBreathingExerciseRewardReq& Right) const
{
    return !operator==(Right);
}

FPbReceiveBreathingExerciseRewardAck::FPbReceiveBreathingExerciseRewardAck()
{
    Reset();        
}

FPbReceiveBreathingExerciseRewardAck::FPbReceiveBreathingExerciseRewardAck(const idlepb::ReceiveBreathingExerciseRewardAck& Right)
{
    this->FromPb(Right);
}

void FPbReceiveBreathingExerciseRewardAck::FromPb(const idlepb::ReceiveBreathingExerciseRewardAck& Right)
{
    ok = Right.ok();
}

void FPbReceiveBreathingExerciseRewardAck::ToPb(idlepb::ReceiveBreathingExerciseRewardAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbReceiveBreathingExerciseRewardAck::Reset()
{
    ok = bool();    
}

void FPbReceiveBreathingExerciseRewardAck::operator=(const idlepb::ReceiveBreathingExerciseRewardAck& Right)
{
    this->FromPb(Right);
}

bool FPbReceiveBreathingExerciseRewardAck::operator==(const FPbReceiveBreathingExerciseRewardAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbReceiveBreathingExerciseRewardAck::operator!=(const FPbReceiveBreathingExerciseRewardAck& Right) const
{
    return !operator==(Right);
}

bool CheckEPbUseItemResultValid(int32 Val)
{
    return idlepb::UseItemResult_IsValid(Val);
}

const TCHAR* GetEPbUseItemResultDescription(EPbUseItemResult Val)
{
    switch (Val)
    {
        case EPbUseItemResult::UIR_Success: return TEXT("成功");
        case EPbUseItemResult::UIR_UnKnown: return TEXT("未知");
        case EPbUseItemResult::UIR_BadParam: return TEXT("参数非法");
        case EPbUseItemResult::UIR_NotEnoughNum: return TEXT("数量不足");
        case EPbUseItemResult::UIR_InventoryIsFull: return TEXT("背包已满");
        case EPbUseItemResult::UIR_LowRank: return TEXT("境界不足");
        case EPbUseItemResult::UIR_BadDir: return TEXT("修炼方向不对");
        case EPbUseItemResult::UIR_BadConfig: return TEXT("配置出错");
        case EPbUseItemResult::UIR_UseNumIsFull: return TEXT("达到上限");
        case EPbUseItemResult::UIR_BadTime: return TEXT("时机不对");
        case EPbUseItemResult::UIR_BadData: return TEXT("内存错误");
        case EPbUseItemResult::UIR_BadType: return TEXT("类型不对");
    }
    return TEXT("UNKNOWN");
}

FPbUseItemReq::FPbUseItemReq()
{
    Reset();        
}

FPbUseItemReq::FPbUseItemReq(const idlepb::UseItemReq& Right)
{
    this->FromPb(Right);
}

void FPbUseItemReq::FromPb(const idlepb::UseItemReq& Right)
{
    id = Right.id();
    cfg_id = Right.cfg_id();
    num = Right.num();
}

void FPbUseItemReq::ToPb(idlepb::UseItemReq* Out) const
{
    Out->set_id(id);
    Out->set_cfg_id(cfg_id);
    Out->set_num(num);    
}

void FPbUseItemReq::Reset()
{
    id = int64();
    cfg_id = int32();
    num = int32();    
}

void FPbUseItemReq::operator=(const idlepb::UseItemReq& Right)
{
    this->FromPb(Right);
}

bool FPbUseItemReq::operator==(const FPbUseItemReq& Right) const
{
    if (this->id != Right.id)
        return false;
    if (this->cfg_id != Right.cfg_id)
        return false;
    if (this->num != Right.num)
        return false;
    return true;
}

bool FPbUseItemReq::operator!=(const FPbUseItemReq& Right) const
{
    return !operator==(Right);
}

FPbUseItemAck::FPbUseItemAck()
{
    Reset();        
}

FPbUseItemAck::FPbUseItemAck(const idlepb::UseItemAck& Right)
{
    this->FromPb(Right);
}

void FPbUseItemAck::FromPb(const idlepb::UseItemAck& Right)
{
    error_code = static_cast<EPbUseItemResult>(Right.error_code());
}

void FPbUseItemAck::ToPb(idlepb::UseItemAck* Out) const
{
    Out->set_error_code(static_cast<idlepb::UseItemResult>(error_code));    
}

void FPbUseItemAck::Reset()
{
    error_code = EPbUseItemResult();    
}

void FPbUseItemAck::operator=(const idlepb::UseItemAck& Right)
{
    this->FromPb(Right);
}

bool FPbUseItemAck::operator==(const FPbUseItemAck& Right) const
{
    if (this->error_code != Right.error_code)
        return false;
    return true;
}

bool FPbUseItemAck::operator!=(const FPbUseItemAck& Right) const
{
    return !operator==(Right);
}

FPbUseSelectGiftReq::FPbUseSelectGiftReq()
{
    Reset();        
}

FPbUseSelectGiftReq::FPbUseSelectGiftReq(const idlepb::UseSelectGiftReq& Right)
{
    this->FromPb(Right);
}

void FPbUseSelectGiftReq::FromPb(const idlepb::UseSelectGiftReq& Right)
{
    uid = Right.uid();
    choose_id = Right.choose_id();
    num = Right.num();
}

void FPbUseSelectGiftReq::ToPb(idlepb::UseSelectGiftReq* Out) const
{
    Out->set_uid(uid);
    Out->set_choose_id(choose_id);
    Out->set_num(num);    
}

void FPbUseSelectGiftReq::Reset()
{
    uid = int64();
    choose_id = int32();
    num = int32();    
}

void FPbUseSelectGiftReq::operator=(const idlepb::UseSelectGiftReq& Right)
{
    this->FromPb(Right);
}

bool FPbUseSelectGiftReq::operator==(const FPbUseSelectGiftReq& Right) const
{
    if (this->uid != Right.uid)
        return false;
    if (this->choose_id != Right.choose_id)
        return false;
    if (this->num != Right.num)
        return false;
    return true;
}

bool FPbUseSelectGiftReq::operator!=(const FPbUseSelectGiftReq& Right) const
{
    return !operator==(Right);
}

FPbUseSelectGiftAck::FPbUseSelectGiftAck()
{
    Reset();        
}

FPbUseSelectGiftAck::FPbUseSelectGiftAck(const idlepb::UseSelectGiftAck& Right)
{
    this->FromPb(Right);
}

void FPbUseSelectGiftAck::FromPb(const idlepb::UseSelectGiftAck& Right)
{
    error_code = static_cast<EPbUseItemResult>(Right.error_code());
}

void FPbUseSelectGiftAck::ToPb(idlepb::UseSelectGiftAck* Out) const
{
    Out->set_error_code(static_cast<idlepb::UseItemResult>(error_code));    
}

void FPbUseSelectGiftAck::Reset()
{
    error_code = EPbUseItemResult();    
}

void FPbUseSelectGiftAck::operator=(const idlepb::UseSelectGiftAck& Right)
{
    this->FromPb(Right);
}

bool FPbUseSelectGiftAck::operator==(const FPbUseSelectGiftAck& Right) const
{
    if (this->error_code != Right.error_code)
        return false;
    return true;
}

bool FPbUseSelectGiftAck::operator!=(const FPbUseSelectGiftAck& Right) const
{
    return !operator==(Right);
}

FPbSellItemInfo::FPbSellItemInfo()
{
    Reset();        
}

FPbSellItemInfo::FPbSellItemInfo(const idlepb::SellItemInfo& Right)
{
    this->FromPb(Right);
}

void FPbSellItemInfo::FromPb(const idlepb::SellItemInfo& Right)
{
    item_id = Right.item_id();
    num = Right.num();
    ok = Right.ok();
}

void FPbSellItemInfo::ToPb(idlepb::SellItemInfo* Out) const
{
    Out->set_item_id(item_id);
    Out->set_num(num);
    Out->set_ok(ok);    
}

void FPbSellItemInfo::Reset()
{
    item_id = int64();
    num = int32();
    ok = bool();    
}

void FPbSellItemInfo::operator=(const idlepb::SellItemInfo& Right)
{
    this->FromPb(Right);
}

bool FPbSellItemInfo::operator==(const FPbSellItemInfo& Right) const
{
    if (this->item_id != Right.item_id)
        return false;
    if (this->num != Right.num)
        return false;
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbSellItemInfo::operator!=(const FPbSellItemInfo& Right) const
{
    return !operator==(Right);
}

FPbSellItemReq::FPbSellItemReq()
{
    Reset();        
}

FPbSellItemReq::FPbSellItemReq(const idlepb::SellItemReq& Right)
{
    this->FromPb(Right);
}

void FPbSellItemReq::FromPb(const idlepb::SellItemReq& Right)
{
    items.Empty();
    for (const auto& Elem : Right.items())
    {
        items.Emplace(Elem);
    }
}

void FPbSellItemReq::ToPb(idlepb::SellItemReq* Out) const
{
    for (const auto& Elem : items)
    {
        Elem.ToPb(Out->add_items());    
    }    
}

void FPbSellItemReq::Reset()
{
    items = TArray<FPbSellItemInfo>();    
}

void FPbSellItemReq::operator=(const idlepb::SellItemReq& Right)
{
    this->FromPb(Right);
}

bool FPbSellItemReq::operator==(const FPbSellItemReq& Right) const
{
    if (this->items != Right.items)
        return false;
    return true;
}

bool FPbSellItemReq::operator!=(const FPbSellItemReq& Right) const
{
    return !operator==(Right);
}

FPbSellItemAck::FPbSellItemAck()
{
    Reset();        
}

FPbSellItemAck::FPbSellItemAck(const idlepb::SellItemAck& Right)
{
    this->FromPb(Right);
}

void FPbSellItemAck::FromPb(const idlepb::SellItemAck& Right)
{
    items.Empty();
    for (const auto& Elem : Right.items())
    {
        items.Emplace(Elem);
    }
}

void FPbSellItemAck::ToPb(idlepb::SellItemAck* Out) const
{
    for (const auto& Elem : items)
    {
        Elem.ToPb(Out->add_items());    
    }    
}

void FPbSellItemAck::Reset()
{
    items = TArray<FPbSellItemInfo>();    
}

void FPbSellItemAck::operator=(const idlepb::SellItemAck& Right)
{
    this->FromPb(Right);
}

bool FPbSellItemAck::operator==(const FPbSellItemAck& Right) const
{
    if (this->items != Right.items)
        return false;
    return true;
}

bool FPbSellItemAck::operator!=(const FPbSellItemAck& Right) const
{
    return !operator==(Right);
}

FPbRefreshAlchemyData::FPbRefreshAlchemyData()
{
    Reset();        
}

FPbRefreshAlchemyData::FPbRefreshAlchemyData(const idlepb::RefreshAlchemyData& Right)
{
    this->FromPb(Right);
}

void FPbRefreshAlchemyData::FromPb(const idlepb::RefreshAlchemyData& Right)
{
    data = Right.data();
}

void FPbRefreshAlchemyData::ToPb(idlepb::RefreshAlchemyData* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbRefreshAlchemyData::Reset()
{
    data = FPbRoleAlchemyData();    
}

void FPbRefreshAlchemyData::operator=(const idlepb::RefreshAlchemyData& Right)
{
    this->FromPb(Right);
}

bool FPbRefreshAlchemyData::operator==(const FPbRefreshAlchemyData& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbRefreshAlchemyData::operator!=(const FPbRefreshAlchemyData& Right) const
{
    return !operator==(Right);
}

FPbNotifyAlchemyRefineResult::FPbNotifyAlchemyRefineResult()
{
    Reset();        
}

FPbNotifyAlchemyRefineResult::FPbNotifyAlchemyRefineResult(const idlepb::NotifyAlchemyRefineResult& Right)
{
    this->FromPb(Right);
}

void FPbNotifyAlchemyRefineResult::FromPb(const idlepb::NotifyAlchemyRefineResult& Right)
{
    quality = static_cast<EPbItemQuality>(Right.quality());
    item_cfg_id = Right.item_cfg_id();
    item_num = Right.item_num();
    exp = Right.exp();
    chance_type = static_cast<EPbAlchemyChanceType>(Right.chance_type());
}

void FPbNotifyAlchemyRefineResult::ToPb(idlepb::NotifyAlchemyRefineResult* Out) const
{
    Out->set_quality(static_cast<idlepb::ItemQuality>(quality));
    Out->set_item_cfg_id(item_cfg_id);
    Out->set_item_num(item_num);
    Out->set_exp(exp);
    Out->set_chance_type(static_cast<idlepb::AlchemyChanceType>(chance_type));    
}

void FPbNotifyAlchemyRefineResult::Reset()
{
    quality = EPbItemQuality();
    item_cfg_id = int32();
    item_num = int32();
    exp = int32();
    chance_type = EPbAlchemyChanceType();    
}

void FPbNotifyAlchemyRefineResult::operator=(const idlepb::NotifyAlchemyRefineResult& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyAlchemyRefineResult::operator==(const FPbNotifyAlchemyRefineResult& Right) const
{
    if (this->quality != Right.quality)
        return false;
    if (this->item_cfg_id != Right.item_cfg_id)
        return false;
    if (this->item_num != Right.item_num)
        return false;
    if (this->exp != Right.exp)
        return false;
    if (this->chance_type != Right.chance_type)
        return false;
    return true;
}

bool FPbNotifyAlchemyRefineResult::operator!=(const FPbNotifyAlchemyRefineResult& Right) const
{
    return !operator==(Right);
}

FPbRefreshForgeData::FPbRefreshForgeData()
{
    Reset();        
}

FPbRefreshForgeData::FPbRefreshForgeData(const idlepb::RefreshForgeData& Right)
{
    this->FromPb(Right);
}

void FPbRefreshForgeData::FromPb(const idlepb::RefreshForgeData& Right)
{
    data = Right.data();
}

void FPbRefreshForgeData::ToPb(idlepb::RefreshForgeData* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbRefreshForgeData::Reset()
{
    data = FPbRoleForgeData();    
}

void FPbRefreshForgeData::operator=(const idlepb::RefreshForgeData& Right)
{
    this->FromPb(Right);
}

bool FPbRefreshForgeData::operator==(const FPbRefreshForgeData& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbRefreshForgeData::operator!=(const FPbRefreshForgeData& Right) const
{
    return !operator==(Right);
}

FPbNotifyForgeRefineResult::FPbNotifyForgeRefineResult()
{
    Reset();        
}

FPbNotifyForgeRefineResult::FPbNotifyForgeRefineResult(const idlepb::NotifyForgeRefineResult& Right)
{
    this->FromPb(Right);
}

void FPbNotifyForgeRefineResult::FromPb(const idlepb::NotifyForgeRefineResult& Right)
{
    quality = static_cast<EPbItemQuality>(Right.quality());
    item_cfg_id = Right.item_cfg_id();
    item_num = Right.item_num();
    exp = Right.exp();
    chance_type = static_cast<EPbForgeChanceType>(Right.chance_type());
}

void FPbNotifyForgeRefineResult::ToPb(idlepb::NotifyForgeRefineResult* Out) const
{
    Out->set_quality(static_cast<idlepb::ItemQuality>(quality));
    Out->set_item_cfg_id(item_cfg_id);
    Out->set_item_num(item_num);
    Out->set_exp(exp);
    Out->set_chance_type(static_cast<idlepb::ForgeChanceType>(chance_type));    
}

void FPbNotifyForgeRefineResult::Reset()
{
    quality = EPbItemQuality();
    item_cfg_id = int32();
    item_num = int32();
    exp = int32();
    chance_type = EPbForgeChanceType();    
}

void FPbNotifyForgeRefineResult::operator=(const idlepb::NotifyForgeRefineResult& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyForgeRefineResult::operator==(const FPbNotifyForgeRefineResult& Right) const
{
    if (this->quality != Right.quality)
        return false;
    if (this->item_cfg_id != Right.item_cfg_id)
        return false;
    if (this->item_num != Right.item_num)
        return false;
    if (this->exp != Right.exp)
        return false;
    if (this->chance_type != Right.chance_type)
        return false;
    return true;
}

bool FPbNotifyForgeRefineResult::operator!=(const FPbNotifyForgeRefineResult& Right) const
{
    return !operator==(Right);
}

FPbEquipmentPutOnReq::FPbEquipmentPutOnReq()
{
    Reset();        
}

FPbEquipmentPutOnReq::FPbEquipmentPutOnReq(const idlepb::EquipmentPutOnReq& Right)
{
    this->FromPb(Right);
}

void FPbEquipmentPutOnReq::FromPb(const idlepb::EquipmentPutOnReq& Right)
{
    slot_idx = Right.slot_idx();
    item_id = Right.item_id();
}

void FPbEquipmentPutOnReq::ToPb(idlepb::EquipmentPutOnReq* Out) const
{
    Out->set_slot_idx(slot_idx);
    Out->set_item_id(item_id);    
}

void FPbEquipmentPutOnReq::Reset()
{
    slot_idx = int32();
    item_id = int64();    
}

void FPbEquipmentPutOnReq::operator=(const idlepb::EquipmentPutOnReq& Right)
{
    this->FromPb(Right);
}

bool FPbEquipmentPutOnReq::operator==(const FPbEquipmentPutOnReq& Right) const
{
    if (this->slot_idx != Right.slot_idx)
        return false;
    if (this->item_id != Right.item_id)
        return false;
    return true;
}

bool FPbEquipmentPutOnReq::operator!=(const FPbEquipmentPutOnReq& Right) const
{
    return !operator==(Right);
}

FPbEquipmentPutOnAck::FPbEquipmentPutOnAck()
{
    Reset();        
}

FPbEquipmentPutOnAck::FPbEquipmentPutOnAck(const idlepb::EquipmentPutOnAck& Right)
{
    this->FromPb(Right);
}

void FPbEquipmentPutOnAck::FromPb(const idlepb::EquipmentPutOnAck& Right)
{
    ok = Right.ok();
}

void FPbEquipmentPutOnAck::ToPb(idlepb::EquipmentPutOnAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbEquipmentPutOnAck::Reset()
{
    ok = bool();    
}

void FPbEquipmentPutOnAck::operator=(const idlepb::EquipmentPutOnAck& Right)
{
    this->FromPb(Right);
}

bool FPbEquipmentPutOnAck::operator==(const FPbEquipmentPutOnAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbEquipmentPutOnAck::operator!=(const FPbEquipmentPutOnAck& Right) const
{
    return !operator==(Right);
}

FPbEquipmentTakeOffReq::FPbEquipmentTakeOffReq()
{
    Reset();        
}

FPbEquipmentTakeOffReq::FPbEquipmentTakeOffReq(const idlepb::EquipmentTakeOffReq& Right)
{
    this->FromPb(Right);
}

void FPbEquipmentTakeOffReq::FromPb(const idlepb::EquipmentTakeOffReq& Right)
{
    slot_idx = Right.slot_idx();
}

void FPbEquipmentTakeOffReq::ToPb(idlepb::EquipmentTakeOffReq* Out) const
{
    Out->set_slot_idx(slot_idx);    
}

void FPbEquipmentTakeOffReq::Reset()
{
    slot_idx = int32();    
}

void FPbEquipmentTakeOffReq::operator=(const idlepb::EquipmentTakeOffReq& Right)
{
    this->FromPb(Right);
}

bool FPbEquipmentTakeOffReq::operator==(const FPbEquipmentTakeOffReq& Right) const
{
    if (this->slot_idx != Right.slot_idx)
        return false;
    return true;
}

bool FPbEquipmentTakeOffReq::operator!=(const FPbEquipmentTakeOffReq& Right) const
{
    return !operator==(Right);
}

FPbEquipmentTakeOffAck::FPbEquipmentTakeOffAck()
{
    Reset();        
}

FPbEquipmentTakeOffAck::FPbEquipmentTakeOffAck(const idlepb::EquipmentTakeOffAck& Right)
{
    this->FromPb(Right);
}

void FPbEquipmentTakeOffAck::FromPb(const idlepb::EquipmentTakeOffAck& Right)
{
    ok = Right.ok();
}

void FPbEquipmentTakeOffAck::ToPb(idlepb::EquipmentTakeOffAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbEquipmentTakeOffAck::Reset()
{
    ok = bool();    
}

void FPbEquipmentTakeOffAck::operator=(const idlepb::EquipmentTakeOffAck& Right)
{
    this->FromPb(Right);
}

bool FPbEquipmentTakeOffAck::operator==(const FPbEquipmentTakeOffAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbEquipmentTakeOffAck::operator!=(const FPbEquipmentTakeOffAck& Right) const
{
    return !operator==(Right);
}

FPbGetInventoryDataReq::FPbGetInventoryDataReq()
{
    Reset();        
}

FPbGetInventoryDataReq::FPbGetInventoryDataReq(const idlepb::GetInventoryDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetInventoryDataReq::FromPb(const idlepb::GetInventoryDataReq& Right)
{
}

void FPbGetInventoryDataReq::ToPb(idlepb::GetInventoryDataReq* Out) const
{    
}

void FPbGetInventoryDataReq::Reset()
{    
}

void FPbGetInventoryDataReq::operator=(const idlepb::GetInventoryDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetInventoryDataReq::operator==(const FPbGetInventoryDataReq& Right) const
{
    return true;
}

bool FPbGetInventoryDataReq::operator!=(const FPbGetInventoryDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetInventoryDataAck::FPbGetInventoryDataAck()
{
    Reset();        
}

FPbGetInventoryDataAck::FPbGetInventoryDataAck(const idlepb::GetInventoryDataAck& Right)
{
    this->FromPb(Right);
}

void FPbGetInventoryDataAck::FromPb(const idlepb::GetInventoryDataAck& Right)
{
    items.Empty();
    for (const auto& Elem : Right.items())
    {
        items.Emplace(Elem);
    }
    unlocked_equipment_slots.Empty();
    for (const auto& Elem : Right.unlocked_equipment_slots())
    {
        unlocked_equipment_slots.Emplace(Elem);
    }
    inventory_space_num = Right.inventory_space_num();
}

void FPbGetInventoryDataAck::ToPb(idlepb::GetInventoryDataAck* Out) const
{
    for (const auto& Elem : items)
    {
        Elem.ToPb(Out->add_items());    
    }
    for (const auto& Elem : unlocked_equipment_slots)
    {
        Out->add_unlocked_equipment_slots(Elem);    
    }
    Out->set_inventory_space_num(inventory_space_num);    
}

void FPbGetInventoryDataAck::Reset()
{
    items = TArray<FPbItemData>();
    unlocked_equipment_slots = TArray<int32>();
    inventory_space_num = int32();    
}

void FPbGetInventoryDataAck::operator=(const idlepb::GetInventoryDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetInventoryDataAck::operator==(const FPbGetInventoryDataAck& Right) const
{
    if (this->items != Right.items)
        return false;
    if (this->unlocked_equipment_slots != Right.unlocked_equipment_slots)
        return false;
    if (this->inventory_space_num != Right.inventory_space_num)
        return false;
    return true;
}

bool FPbGetInventoryDataAck::operator!=(const FPbGetInventoryDataAck& Right) const
{
    return !operator==(Right);
}

FPbAlchemyRefineStartReq::FPbAlchemyRefineStartReq()
{
    Reset();        
}

FPbAlchemyRefineStartReq::FPbAlchemyRefineStartReq(const idlepb::AlchemyRefineStartReq& Right)
{
    this->FromPb(Right);
}

void FPbAlchemyRefineStartReq::FromPb(const idlepb::AlchemyRefineStartReq& Right)
{
    recipe_id = Right.recipe_id();
    material_id = Right.material_id();
    target_num = Right.target_num();
}

void FPbAlchemyRefineStartReq::ToPb(idlepb::AlchemyRefineStartReq* Out) const
{
    Out->set_recipe_id(recipe_id);
    Out->set_material_id(material_id);
    Out->set_target_num(target_num);    
}

void FPbAlchemyRefineStartReq::Reset()
{
    recipe_id = int32();
    material_id = int32();
    target_num = int32();    
}

void FPbAlchemyRefineStartReq::operator=(const idlepb::AlchemyRefineStartReq& Right)
{
    this->FromPb(Right);
}

bool FPbAlchemyRefineStartReq::operator==(const FPbAlchemyRefineStartReq& Right) const
{
    if (this->recipe_id != Right.recipe_id)
        return false;
    if (this->material_id != Right.material_id)
        return false;
    if (this->target_num != Right.target_num)
        return false;
    return true;
}

bool FPbAlchemyRefineStartReq::operator!=(const FPbAlchemyRefineStartReq& Right) const
{
    return !operator==(Right);
}

FPbAlchemyRefineStartAck::FPbAlchemyRefineStartAck()
{
    Reset();        
}

FPbAlchemyRefineStartAck::FPbAlchemyRefineStartAck(const idlepb::AlchemyRefineStartAck& Right)
{
    this->FromPb(Right);
}

void FPbAlchemyRefineStartAck::FromPb(const idlepb::AlchemyRefineStartAck& Right)
{
    ok = Right.ok();
}

void FPbAlchemyRefineStartAck::ToPb(idlepb::AlchemyRefineStartAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbAlchemyRefineStartAck::Reset()
{
    ok = bool();    
}

void FPbAlchemyRefineStartAck::operator=(const idlepb::AlchemyRefineStartAck& Right)
{
    this->FromPb(Right);
}

bool FPbAlchemyRefineStartAck::operator==(const FPbAlchemyRefineStartAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbAlchemyRefineStartAck::operator!=(const FPbAlchemyRefineStartAck& Right) const
{
    return !operator==(Right);
}

FPbAlchemyRefineCancelReq::FPbAlchemyRefineCancelReq()
{
    Reset();        
}

FPbAlchemyRefineCancelReq::FPbAlchemyRefineCancelReq(const idlepb::AlchemyRefineCancelReq& Right)
{
    this->FromPb(Right);
}

void FPbAlchemyRefineCancelReq::FromPb(const idlepb::AlchemyRefineCancelReq& Right)
{
}

void FPbAlchemyRefineCancelReq::ToPb(idlepb::AlchemyRefineCancelReq* Out) const
{    
}

void FPbAlchemyRefineCancelReq::Reset()
{    
}

void FPbAlchemyRefineCancelReq::operator=(const idlepb::AlchemyRefineCancelReq& Right)
{
    this->FromPb(Right);
}

bool FPbAlchemyRefineCancelReq::operator==(const FPbAlchemyRefineCancelReq& Right) const
{
    return true;
}

bool FPbAlchemyRefineCancelReq::operator!=(const FPbAlchemyRefineCancelReq& Right) const
{
    return !operator==(Right);
}

FPbAlchemyRefineCancelAck::FPbAlchemyRefineCancelAck()
{
    Reset();        
}

FPbAlchemyRefineCancelAck::FPbAlchemyRefineCancelAck(const idlepb::AlchemyRefineCancelAck& Right)
{
    this->FromPb(Right);
}

void FPbAlchemyRefineCancelAck::FromPb(const idlepb::AlchemyRefineCancelAck& Right)
{
    ok = Right.ok();
}

void FPbAlchemyRefineCancelAck::ToPb(idlepb::AlchemyRefineCancelAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbAlchemyRefineCancelAck::Reset()
{
    ok = bool();    
}

void FPbAlchemyRefineCancelAck::operator=(const idlepb::AlchemyRefineCancelAck& Right)
{
    this->FromPb(Right);
}

bool FPbAlchemyRefineCancelAck::operator==(const FPbAlchemyRefineCancelAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbAlchemyRefineCancelAck::operator!=(const FPbAlchemyRefineCancelAck& Right) const
{
    return !operator==(Right);
}

FPbAlchemyRefineExtractReq::FPbAlchemyRefineExtractReq()
{
    Reset();        
}

FPbAlchemyRefineExtractReq::FPbAlchemyRefineExtractReq(const idlepb::AlchemyRefineExtractReq& Right)
{
    this->FromPb(Right);
}

void FPbAlchemyRefineExtractReq::FromPb(const idlepb::AlchemyRefineExtractReq& Right)
{
}

void FPbAlchemyRefineExtractReq::ToPb(idlepb::AlchemyRefineExtractReq* Out) const
{    
}

void FPbAlchemyRefineExtractReq::Reset()
{    
}

void FPbAlchemyRefineExtractReq::operator=(const idlepb::AlchemyRefineExtractReq& Right)
{
    this->FromPb(Right);
}

bool FPbAlchemyRefineExtractReq::operator==(const FPbAlchemyRefineExtractReq& Right) const
{
    return true;
}

bool FPbAlchemyRefineExtractReq::operator!=(const FPbAlchemyRefineExtractReq& Right) const
{
    return !operator==(Right);
}

FPbAlchemyRefineExtractAck::FPbAlchemyRefineExtractAck()
{
    Reset();        
}

FPbAlchemyRefineExtractAck::FPbAlchemyRefineExtractAck(const idlepb::AlchemyRefineExtractAck& Right)
{
    this->FromPb(Right);
}

void FPbAlchemyRefineExtractAck::FromPb(const idlepb::AlchemyRefineExtractAck& Right)
{
    ok = Right.ok();
}

void FPbAlchemyRefineExtractAck::ToPb(idlepb::AlchemyRefineExtractAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbAlchemyRefineExtractAck::Reset()
{
    ok = bool();    
}

void FPbAlchemyRefineExtractAck::operator=(const idlepb::AlchemyRefineExtractAck& Right)
{
    this->FromPb(Right);
}

bool FPbAlchemyRefineExtractAck::operator==(const FPbAlchemyRefineExtractAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbAlchemyRefineExtractAck::operator!=(const FPbAlchemyRefineExtractAck& Right) const
{
    return !operator==(Right);
}

FPbCreateCharacterReq::FPbCreateCharacterReq()
{
    Reset();        
}

FPbCreateCharacterReq::FPbCreateCharacterReq(const idlepb::CreateCharacterReq& Right)
{
    this->FromPb(Right);
}

void FPbCreateCharacterReq::FromPb(const idlepb::CreateCharacterReq& Right)
{
    hero_name = UTF8_TO_TCHAR(Right.hero_name().c_str());
    data = Right.data();
    skeleton_type = Right.skeleton_type();
    model_data.Empty();
    for (const auto& Elem : Right.model_data())
    {
        model_data.Emplace(Elem);
    }
}

void FPbCreateCharacterReq::ToPb(idlepb::CreateCharacterReq* Out) const
{
    Out->set_hero_name(TCHAR_TO_UTF8(*hero_name));
    data.ToPb(Out->mutable_data());
    Out->set_skeleton_type(skeleton_type);
    for (const auto& Elem : model_data)
    {
        Out->add_model_data(Elem);    
    }    
}

void FPbCreateCharacterReq::Reset()
{
    hero_name = FString();
    data = FPbCharacterModelConfig();
    skeleton_type = int32();
    model_data = TArray<int32>();    
}

void FPbCreateCharacterReq::operator=(const idlepb::CreateCharacterReq& Right)
{
    this->FromPb(Right);
}

bool FPbCreateCharacterReq::operator==(const FPbCreateCharacterReq& Right) const
{
    if (this->hero_name != Right.hero_name)
        return false;
    if (this->data != Right.data)
        return false;
    if (this->skeleton_type != Right.skeleton_type)
        return false;
    if (this->model_data != Right.model_data)
        return false;
    return true;
}

bool FPbCreateCharacterReq::operator!=(const FPbCreateCharacterReq& Right) const
{
    return !operator==(Right);
}

FPbCreateCharacterAck::FPbCreateCharacterAck()
{
    Reset();        
}

FPbCreateCharacterAck::FPbCreateCharacterAck(const idlepb::CreateCharacterAck& Right)
{
    this->FromPb(Right);
}

void FPbCreateCharacterAck::FromPb(const idlepb::CreateCharacterAck& Right)
{
    ok = Right.ok();
}

void FPbCreateCharacterAck::ToPb(idlepb::CreateCharacterAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbCreateCharacterAck::Reset()
{
    ok = bool();    
}

void FPbCreateCharacterAck::operator=(const idlepb::CreateCharacterAck& Right)
{
    this->FromPb(Right);
}

bool FPbCreateCharacterAck::operator==(const FPbCreateCharacterAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbCreateCharacterAck::operator!=(const FPbCreateCharacterAck& Right) const
{
    return !operator==(Right);
}

FPbSystemNotice::FPbSystemNotice()
{
    Reset();        
}

FPbSystemNotice::FPbSystemNotice(const idlepb::SystemNotice& Right)
{
    this->FromPb(Right);
}

void FPbSystemNotice::FromPb(const idlepb::SystemNotice& Right)
{
    style = Right.style();
    text = UTF8_TO_TCHAR(Right.text().c_str());
    s1 = UTF8_TO_TCHAR(Right.s1().c_str());
    s2 = UTF8_TO_TCHAR(Right.s2().c_str());
    s3 = UTF8_TO_TCHAR(Right.s3().c_str());
    n1 = Right.n1();
    n2 = Right.n2();
    n3 = Right.n3();
}

void FPbSystemNotice::ToPb(idlepb::SystemNotice* Out) const
{
    Out->set_style(style);
    Out->set_text(TCHAR_TO_UTF8(*text));
    Out->set_s1(TCHAR_TO_UTF8(*s1));
    Out->set_s2(TCHAR_TO_UTF8(*s2));
    Out->set_s3(TCHAR_TO_UTF8(*s3));
    Out->set_n1(n1);
    Out->set_n2(n2);
    Out->set_n3(n3);    
}

void FPbSystemNotice::Reset()
{
    style = int32();
    text = FString();
    s1 = FString();
    s2 = FString();
    s3 = FString();
    n1 = int64();
    n2 = int64();
    n3 = int64();    
}

void FPbSystemNotice::operator=(const idlepb::SystemNotice& Right)
{
    this->FromPb(Right);
}

bool FPbSystemNotice::operator==(const FPbSystemNotice& Right) const
{
    if (this->style != Right.style)
        return false;
    if (this->text != Right.text)
        return false;
    if (this->s1 != Right.s1)
        return false;
    if (this->s2 != Right.s2)
        return false;
    if (this->s3 != Right.s3)
        return false;
    if (this->n1 != Right.n1)
        return false;
    if (this->n2 != Right.n2)
        return false;
    if (this->n3 != Right.n3)
        return false;
    return true;
}

bool FPbSystemNotice::operator!=(const FPbSystemNotice& Right) const
{
    return !operator==(Right);
}

FPbGetRoleShopDataReq::FPbGetRoleShopDataReq()
{
    Reset();        
}

FPbGetRoleShopDataReq::FPbGetRoleShopDataReq(const idlepb::GetRoleShopDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleShopDataReq::FromPb(const idlepb::GetRoleShopDataReq& Right)
{
}

void FPbGetRoleShopDataReq::ToPb(idlepb::GetRoleShopDataReq* Out) const
{    
}

void FPbGetRoleShopDataReq::Reset()
{    
}

void FPbGetRoleShopDataReq::operator=(const idlepb::GetRoleShopDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleShopDataReq::operator==(const FPbGetRoleShopDataReq& Right) const
{
    return true;
}

bool FPbGetRoleShopDataReq::operator!=(const FPbGetRoleShopDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetRoleShopDataAck::FPbGetRoleShopDataAck()
{
    Reset();        
}

FPbGetRoleShopDataAck::FPbGetRoleShopDataAck(const idlepb::GetRoleShopDataAck& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleShopDataAck::FromPb(const idlepb::GetRoleShopDataAck& Right)
{
    data = Right.data();
}

void FPbGetRoleShopDataAck::ToPb(idlepb::GetRoleShopDataAck* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbGetRoleShopDataAck::Reset()
{
    data = FPbRoleShopData();    
}

void FPbGetRoleShopDataAck::operator=(const idlepb::GetRoleShopDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleShopDataAck::operator==(const FPbGetRoleShopDataAck& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbGetRoleShopDataAck::operator!=(const FPbGetRoleShopDataAck& Right) const
{
    return !operator==(Right);
}

FPbRefreshShopReq::FPbRefreshShopReq()
{
    Reset();        
}

FPbRefreshShopReq::FPbRefreshShopReq(const idlepb::RefreshShopReq& Right)
{
    this->FromPb(Right);
}

void FPbRefreshShopReq::FromPb(const idlepb::RefreshShopReq& Right)
{
}

void FPbRefreshShopReq::ToPb(idlepb::RefreshShopReq* Out) const
{    
}

void FPbRefreshShopReq::Reset()
{    
}

void FPbRefreshShopReq::operator=(const idlepb::RefreshShopReq& Right)
{
    this->FromPb(Right);
}

bool FPbRefreshShopReq::operator==(const FPbRefreshShopReq& Right) const
{
    return true;
}

bool FPbRefreshShopReq::operator!=(const FPbRefreshShopReq& Right) const
{
    return !operator==(Right);
}

FPbRefreshShopAck::FPbRefreshShopAck()
{
    Reset();        
}

FPbRefreshShopAck::FPbRefreshShopAck(const idlepb::RefreshShopAck& Right)
{
    this->FromPb(Right);
}

void FPbRefreshShopAck::FromPb(const idlepb::RefreshShopAck& Right)
{
    ok = Right.ok();
    data = Right.data();
}

void FPbRefreshShopAck::ToPb(idlepb::RefreshShopAck* Out) const
{
    Out->set_ok(ok);
    data.ToPb(Out->mutable_data());    
}

void FPbRefreshShopAck::Reset()
{
    ok = bool();
    data = FPbRoleShopData();    
}

void FPbRefreshShopAck::operator=(const idlepb::RefreshShopAck& Right)
{
    this->FromPb(Right);
}

bool FPbRefreshShopAck::operator==(const FPbRefreshShopAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbRefreshShopAck::operator!=(const FPbRefreshShopAck& Right) const
{
    return !operator==(Right);
}

FPbBuyShopItemReq::FPbBuyShopItemReq()
{
    Reset();        
}

FPbBuyShopItemReq::FPbBuyShopItemReq(const idlepb::BuyShopItemReq& Right)
{
    this->FromPb(Right);
}

void FPbBuyShopItemReq::FromPb(const idlepb::BuyShopItemReq& Right)
{
    index = Right.index();
}

void FPbBuyShopItemReq::ToPb(idlepb::BuyShopItemReq* Out) const
{
    Out->set_index(index);    
}

void FPbBuyShopItemReq::Reset()
{
    index = int32();    
}

void FPbBuyShopItemReq::operator=(const idlepb::BuyShopItemReq& Right)
{
    this->FromPb(Right);
}

bool FPbBuyShopItemReq::operator==(const FPbBuyShopItemReq& Right) const
{
    if (this->index != Right.index)
        return false;
    return true;
}

bool FPbBuyShopItemReq::operator!=(const FPbBuyShopItemReq& Right) const
{
    return !operator==(Right);
}

FPbBuyShopItemAck::FPbBuyShopItemAck()
{
    Reset();        
}

FPbBuyShopItemAck::FPbBuyShopItemAck(const idlepb::BuyShopItemAck& Right)
{
    this->FromPb(Right);
}

void FPbBuyShopItemAck::FromPb(const idlepb::BuyShopItemAck& Right)
{
    ok = Right.ok();
}

void FPbBuyShopItemAck::ToPb(idlepb::BuyShopItemAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbBuyShopItemAck::Reset()
{
    ok = bool();    
}

void FPbBuyShopItemAck::operator=(const idlepb::BuyShopItemAck& Right)
{
    this->FromPb(Right);
}

bool FPbBuyShopItemAck::operator==(const FPbBuyShopItemAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbBuyShopItemAck::operator!=(const FPbBuyShopItemAck& Right) const
{
    return !operator==(Right);
}

FPbGetRoleDeluxeShopDataReq::FPbGetRoleDeluxeShopDataReq()
{
    Reset();        
}

FPbGetRoleDeluxeShopDataReq::FPbGetRoleDeluxeShopDataReq(const idlepb::GetRoleDeluxeShopDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleDeluxeShopDataReq::FromPb(const idlepb::GetRoleDeluxeShopDataReq& Right)
{
}

void FPbGetRoleDeluxeShopDataReq::ToPb(idlepb::GetRoleDeluxeShopDataReq* Out) const
{    
}

void FPbGetRoleDeluxeShopDataReq::Reset()
{    
}

void FPbGetRoleDeluxeShopDataReq::operator=(const idlepb::GetRoleDeluxeShopDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleDeluxeShopDataReq::operator==(const FPbGetRoleDeluxeShopDataReq& Right) const
{
    return true;
}

bool FPbGetRoleDeluxeShopDataReq::operator!=(const FPbGetRoleDeluxeShopDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetRoleDeluxeShopDataAck::FPbGetRoleDeluxeShopDataAck()
{
    Reset();        
}

FPbGetRoleDeluxeShopDataAck::FPbGetRoleDeluxeShopDataAck(const idlepb::GetRoleDeluxeShopDataAck& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleDeluxeShopDataAck::FromPb(const idlepb::GetRoleDeluxeShopDataAck& Right)
{
    data = Right.data();
}

void FPbGetRoleDeluxeShopDataAck::ToPb(idlepb::GetRoleDeluxeShopDataAck* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbGetRoleDeluxeShopDataAck::Reset()
{
    data = FPbRoleDeluxeShopData();    
}

void FPbGetRoleDeluxeShopDataAck::operator=(const idlepb::GetRoleDeluxeShopDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleDeluxeShopDataAck::operator==(const FPbGetRoleDeluxeShopDataAck& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbGetRoleDeluxeShopDataAck::operator!=(const FPbGetRoleDeluxeShopDataAck& Right) const
{
    return !operator==(Right);
}

FPbRefreshDeluxeShopReq::FPbRefreshDeluxeShopReq()
{
    Reset();        
}

FPbRefreshDeluxeShopReq::FPbRefreshDeluxeShopReq(const idlepb::RefreshDeluxeShopReq& Right)
{
    this->FromPb(Right);
}

void FPbRefreshDeluxeShopReq::FromPb(const idlepb::RefreshDeluxeShopReq& Right)
{
}

void FPbRefreshDeluxeShopReq::ToPb(idlepb::RefreshDeluxeShopReq* Out) const
{    
}

void FPbRefreshDeluxeShopReq::Reset()
{    
}

void FPbRefreshDeluxeShopReq::operator=(const idlepb::RefreshDeluxeShopReq& Right)
{
    this->FromPb(Right);
}

bool FPbRefreshDeluxeShopReq::operator==(const FPbRefreshDeluxeShopReq& Right) const
{
    return true;
}

bool FPbRefreshDeluxeShopReq::operator!=(const FPbRefreshDeluxeShopReq& Right) const
{
    return !operator==(Right);
}

FPbRefreshDeluxeShopAck::FPbRefreshDeluxeShopAck()
{
    Reset();        
}

FPbRefreshDeluxeShopAck::FPbRefreshDeluxeShopAck(const idlepb::RefreshDeluxeShopAck& Right)
{
    this->FromPb(Right);
}

void FPbRefreshDeluxeShopAck::FromPb(const idlepb::RefreshDeluxeShopAck& Right)
{
    ok = Right.ok();
    data = Right.data();
}

void FPbRefreshDeluxeShopAck::ToPb(idlepb::RefreshDeluxeShopAck* Out) const
{
    Out->set_ok(ok);
    data.ToPb(Out->mutable_data());    
}

void FPbRefreshDeluxeShopAck::Reset()
{
    ok = bool();
    data = FPbRoleDeluxeShopData();    
}

void FPbRefreshDeluxeShopAck::operator=(const idlepb::RefreshDeluxeShopAck& Right)
{
    this->FromPb(Right);
}

bool FPbRefreshDeluxeShopAck::operator==(const FPbRefreshDeluxeShopAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbRefreshDeluxeShopAck::operator!=(const FPbRefreshDeluxeShopAck& Right) const
{
    return !operator==(Right);
}

FPbBuyDeluxeShopItemReq::FPbBuyDeluxeShopItemReq()
{
    Reset();        
}

FPbBuyDeluxeShopItemReq::FPbBuyDeluxeShopItemReq(const idlepb::BuyDeluxeShopItemReq& Right)
{
    this->FromPb(Right);
}

void FPbBuyDeluxeShopItemReq::FromPb(const idlepb::BuyDeluxeShopItemReq& Right)
{
    index = Right.index();
}

void FPbBuyDeluxeShopItemReq::ToPb(idlepb::BuyDeluxeShopItemReq* Out) const
{
    Out->set_index(index);    
}

void FPbBuyDeluxeShopItemReq::Reset()
{
    index = int32();    
}

void FPbBuyDeluxeShopItemReq::operator=(const idlepb::BuyDeluxeShopItemReq& Right)
{
    this->FromPb(Right);
}

bool FPbBuyDeluxeShopItemReq::operator==(const FPbBuyDeluxeShopItemReq& Right) const
{
    if (this->index != Right.index)
        return false;
    return true;
}

bool FPbBuyDeluxeShopItemReq::operator!=(const FPbBuyDeluxeShopItemReq& Right) const
{
    return !operator==(Right);
}

FPbBuyDeluxeShopItemAck::FPbBuyDeluxeShopItemAck()
{
    Reset();        
}

FPbBuyDeluxeShopItemAck::FPbBuyDeluxeShopItemAck(const idlepb::BuyDeluxeShopItemAck& Right)
{
    this->FromPb(Right);
}

void FPbBuyDeluxeShopItemAck::FromPb(const idlepb::BuyDeluxeShopItemAck& Right)
{
    ok = Right.ok();
}

void FPbBuyDeluxeShopItemAck::ToPb(idlepb::BuyDeluxeShopItemAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbBuyDeluxeShopItemAck::Reset()
{
    ok = bool();    
}

void FPbBuyDeluxeShopItemAck::operator=(const idlepb::BuyDeluxeShopItemAck& Right)
{
    this->FromPb(Right);
}

bool FPbBuyDeluxeShopItemAck::operator==(const FPbBuyDeluxeShopItemAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbBuyDeluxeShopItemAck::operator!=(const FPbBuyDeluxeShopItemAck& Right) const
{
    return !operator==(Right);
}

FPbUnlockDeluxeShopReq::FPbUnlockDeluxeShopReq()
{
    Reset();        
}

FPbUnlockDeluxeShopReq::FPbUnlockDeluxeShopReq(const idlepb::UnlockDeluxeShopReq& Right)
{
    this->FromPb(Right);
}

void FPbUnlockDeluxeShopReq::FromPb(const idlepb::UnlockDeluxeShopReq& Right)
{
}

void FPbUnlockDeluxeShopReq::ToPb(idlepb::UnlockDeluxeShopReq* Out) const
{    
}

void FPbUnlockDeluxeShopReq::Reset()
{    
}

void FPbUnlockDeluxeShopReq::operator=(const idlepb::UnlockDeluxeShopReq& Right)
{
    this->FromPb(Right);
}

bool FPbUnlockDeluxeShopReq::operator==(const FPbUnlockDeluxeShopReq& Right) const
{
    return true;
}

bool FPbUnlockDeluxeShopReq::operator!=(const FPbUnlockDeluxeShopReq& Right) const
{
    return !operator==(Right);
}

FPbUnlockDeluxeShopAck::FPbUnlockDeluxeShopAck()
{
    Reset();        
}

FPbUnlockDeluxeShopAck::FPbUnlockDeluxeShopAck(const idlepb::UnlockDeluxeShopAck& Right)
{
    this->FromPb(Right);
}

void FPbUnlockDeluxeShopAck::FromPb(const idlepb::UnlockDeluxeShopAck& Right)
{
    ok = Right.ok();
    data = Right.data();
}

void FPbUnlockDeluxeShopAck::ToPb(idlepb::UnlockDeluxeShopAck* Out) const
{
    Out->set_ok(ok);
    data.ToPb(Out->mutable_data());    
}

void FPbUnlockDeluxeShopAck::Reset()
{
    ok = bool();
    data = FPbRoleDeluxeShopData();    
}

void FPbUnlockDeluxeShopAck::operator=(const idlepb::UnlockDeluxeShopAck& Right)
{
    this->FromPb(Right);
}

bool FPbUnlockDeluxeShopAck::operator==(const FPbUnlockDeluxeShopAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbUnlockDeluxeShopAck::operator!=(const FPbUnlockDeluxeShopAck& Right) const
{
    return !operator==(Right);
}

FPbRefreshDeluxeShopUnlocked::FPbRefreshDeluxeShopUnlocked()
{
    Reset();        
}

FPbRefreshDeluxeShopUnlocked::FPbRefreshDeluxeShopUnlocked(const idlepb::RefreshDeluxeShopUnlocked& Right)
{
    this->FromPb(Right);
}

void FPbRefreshDeluxeShopUnlocked::FromPb(const idlepb::RefreshDeluxeShopUnlocked& Right)
{
    is_unlocked = Right.is_unlocked();
}

void FPbRefreshDeluxeShopUnlocked::ToPb(idlepb::RefreshDeluxeShopUnlocked* Out) const
{
    Out->set_is_unlocked(is_unlocked);    
}

void FPbRefreshDeluxeShopUnlocked::Reset()
{
    is_unlocked = bool();    
}

void FPbRefreshDeluxeShopUnlocked::operator=(const idlepb::RefreshDeluxeShopUnlocked& Right)
{
    this->FromPb(Right);
}

bool FPbRefreshDeluxeShopUnlocked::operator==(const FPbRefreshDeluxeShopUnlocked& Right) const
{
    if (this->is_unlocked != Right.is_unlocked)
        return false;
    return true;
}

bool FPbRefreshDeluxeShopUnlocked::operator!=(const FPbRefreshDeluxeShopUnlocked& Right) const
{
    return !operator==(Right);
}

FPbUnlockArenaReq::FPbUnlockArenaReq()
{
    Reset();        
}

FPbUnlockArenaReq::FPbUnlockArenaReq(const idlepb::UnlockArenaReq& Right)
{
    this->FromPb(Right);
}

void FPbUnlockArenaReq::FromPb(const idlepb::UnlockArenaReq& Right)
{
    arena_id = Right.arena_id();
}

void FPbUnlockArenaReq::ToPb(idlepb::UnlockArenaReq* Out) const
{
    Out->set_arena_id(arena_id);    
}

void FPbUnlockArenaReq::Reset()
{
    arena_id = int32();    
}

void FPbUnlockArenaReq::operator=(const idlepb::UnlockArenaReq& Right)
{
    this->FromPb(Right);
}

bool FPbUnlockArenaReq::operator==(const FPbUnlockArenaReq& Right) const
{
    if (this->arena_id != Right.arena_id)
        return false;
    return true;
}

bool FPbUnlockArenaReq::operator!=(const FPbUnlockArenaReq& Right) const
{
    return !operator==(Right);
}

FPbUnlockArenaAck::FPbUnlockArenaAck()
{
    Reset();        
}

FPbUnlockArenaAck::FPbUnlockArenaAck(const idlepb::UnlockArenaAck& Right)
{
    this->FromPb(Right);
}

void FPbUnlockArenaAck::FromPb(const idlepb::UnlockArenaAck& Right)
{
    ok = Right.ok();
}

void FPbUnlockArenaAck::ToPb(idlepb::UnlockArenaAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbUnlockArenaAck::Reset()
{
    ok = bool();    
}

void FPbUnlockArenaAck::operator=(const idlepb::UnlockArenaAck& Right)
{
    this->FromPb(Right);
}

bool FPbUnlockArenaAck::operator==(const FPbUnlockArenaAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbUnlockArenaAck::operator!=(const FPbUnlockArenaAck& Right) const
{
    return !operator==(Right);
}

FPbNotifyUnlockArenaChallengeResult::FPbNotifyUnlockArenaChallengeResult()
{
    Reset();        
}

FPbNotifyUnlockArenaChallengeResult::FPbNotifyUnlockArenaChallengeResult(const idlepb::NotifyUnlockArenaChallengeResult& Right)
{
    this->FromPb(Right);
}

void FPbNotifyUnlockArenaChallengeResult::FromPb(const idlepb::NotifyUnlockArenaChallengeResult& Right)
{
    arena_id = Right.arena_id();
    door_id = Right.door_id();
    ok = Right.ok();
}

void FPbNotifyUnlockArenaChallengeResult::ToPb(idlepb::NotifyUnlockArenaChallengeResult* Out) const
{
    Out->set_arena_id(arena_id);
    Out->set_door_id(door_id);
    Out->set_ok(ok);    
}

void FPbNotifyUnlockArenaChallengeResult::Reset()
{
    arena_id = int32();
    door_id = int32();
    ok = bool();    
}

void FPbNotifyUnlockArenaChallengeResult::operator=(const idlepb::NotifyUnlockArenaChallengeResult& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyUnlockArenaChallengeResult::operator==(const FPbNotifyUnlockArenaChallengeResult& Right) const
{
    if (this->arena_id != Right.arena_id)
        return false;
    if (this->door_id != Right.door_id)
        return false;
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbNotifyUnlockArenaChallengeResult::operator!=(const FPbNotifyUnlockArenaChallengeResult& Right) const
{
    return !operator==(Right);
}

FPbRequestRefreshRoleCombatPower::FPbRequestRefreshRoleCombatPower()
{
    Reset();        
}

FPbRequestRefreshRoleCombatPower::FPbRequestRefreshRoleCombatPower(const idlepb::RequestRefreshRoleCombatPower& Right)
{
    this->FromPb(Right);
}

void FPbRequestRefreshRoleCombatPower::FromPb(const idlepb::RequestRefreshRoleCombatPower& Right)
{
}

void FPbRequestRefreshRoleCombatPower::ToPb(idlepb::RequestRefreshRoleCombatPower* Out) const
{    
}

void FPbRequestRefreshRoleCombatPower::Reset()
{    
}

void FPbRequestRefreshRoleCombatPower::operator=(const idlepb::RequestRefreshRoleCombatPower& Right)
{
    this->FromPb(Right);
}

bool FPbRequestRefreshRoleCombatPower::operator==(const FPbRequestRefreshRoleCombatPower& Right) const
{
    return true;
}

bool FPbRequestRefreshRoleCombatPower::operator!=(const FPbRequestRefreshRoleCombatPower& Right) const
{
    return !operator==(Right);
}

FPbNotifyRoleCombatPower::FPbNotifyRoleCombatPower()
{
    Reset();        
}

FPbNotifyRoleCombatPower::FPbNotifyRoleCombatPower(const idlepb::NotifyRoleCombatPower& Right)
{
    this->FromPb(Right);
}

void FPbNotifyRoleCombatPower::FromPb(const idlepb::NotifyRoleCombatPower& Right)
{
    old_value = Right.old_value();
    new_value = Right.new_value();
    show_notice = Right.show_notice();
}

void FPbNotifyRoleCombatPower::ToPb(idlepb::NotifyRoleCombatPower* Out) const
{
    Out->set_old_value(old_value);
    Out->set_new_value(new_value);
    Out->set_show_notice(show_notice);    
}

void FPbNotifyRoleCombatPower::Reset()
{
    old_value = int64();
    new_value = int64();
    show_notice = bool();    
}

void FPbNotifyRoleCombatPower::operator=(const idlepb::NotifyRoleCombatPower& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyRoleCombatPower::operator==(const FPbNotifyRoleCombatPower& Right) const
{
    if (this->old_value != Right.old_value)
        return false;
    if (this->new_value != Right.new_value)
        return false;
    if (this->show_notice != Right.show_notice)
        return false;
    return true;
}

bool FPbNotifyRoleCombatPower::operator!=(const FPbNotifyRoleCombatPower& Right) const
{
    return !operator==(Right);
}

FPbGameSystemChatMessage::FPbGameSystemChatMessage()
{
    Reset();        
}

FPbGameSystemChatMessage::FPbGameSystemChatMessage(const idlepb::GameSystemChatMessage& Right)
{
    this->FromPb(Right);
}

void FPbGameSystemChatMessage::FromPb(const idlepb::GameSystemChatMessage& Right)
{
    chat_type = Right.chat_type();
    chat_content.Empty(chat_content.Num());
    chat_content.Append(reinterpret_cast<const uint8*>(Right.chat_content().c_str()), Right.chat_content().size());
}

void FPbGameSystemChatMessage::ToPb(idlepb::GameSystemChatMessage* Out) const
{
    Out->set_chat_type(chat_type);
    Out->set_chat_content(chat_content.GetData(), chat_content.Num());    
}

void FPbGameSystemChatMessage::Reset()
{
    chat_type = int32();
    chat_content = TArray<uint8>();    
}

void FPbGameSystemChatMessage::operator=(const idlepb::GameSystemChatMessage& Right)
{
    this->FromPb(Right);
}

bool FPbGameSystemChatMessage::operator==(const FPbGameSystemChatMessage& Right) const
{
    if (this->chat_type != Right.chat_type)
        return false;
    if (this->chat_content != Right.chat_content)
        return false;
    return true;
}

bool FPbGameSystemChatMessage::operator!=(const FPbGameSystemChatMessage& Right) const
{
    return !operator==(Right);
}

FPbReplicateQuestProgressChange::FPbReplicateQuestProgressChange()
{
    Reset();        
}

FPbReplicateQuestProgressChange::FPbReplicateQuestProgressChange(const idlepb::ReplicateQuestProgressChange& Right)
{
    this->FromPb(Right);
}

void FPbReplicateQuestProgressChange::FromPb(const idlepb::ReplicateQuestProgressChange& Right)
{
    quest_id = Right.quest_id();
    type = static_cast<EPbQuestRequirementType>(Right.type());
    target_id = Right.target_id();
    amount = Right.amount();
}

void FPbReplicateQuestProgressChange::ToPb(idlepb::ReplicateQuestProgressChange* Out) const
{
    Out->set_quest_id(quest_id);
    Out->set_type(static_cast<idlepb::QuestRequirementType>(type));
    Out->set_target_id(target_id);
    Out->set_amount(amount);    
}

void FPbReplicateQuestProgressChange::Reset()
{
    quest_id = int32();
    type = EPbQuestRequirementType();
    target_id = int32();
    amount = int32();    
}

void FPbReplicateQuestProgressChange::operator=(const idlepb::ReplicateQuestProgressChange& Right)
{
    this->FromPb(Right);
}

bool FPbReplicateQuestProgressChange::operator==(const FPbReplicateQuestProgressChange& Right) const
{
    if (this->quest_id != Right.quest_id)
        return false;
    if (this->type != Right.type)
        return false;
    if (this->target_id != Right.target_id)
        return false;
    if (this->amount != Right.amount)
        return false;
    return true;
}

bool FPbReplicateQuestProgressChange::operator!=(const FPbReplicateQuestProgressChange& Right) const
{
    return !operator==(Right);
}

FPbQuestOpReq::FPbQuestOpReq()
{
    Reset();        
}

FPbQuestOpReq::FPbQuestOpReq(const idlepb::QuestOpReq& Right)
{
    this->FromPb(Right);
}

void FPbQuestOpReq::FromPb(const idlepb::QuestOpReq& Right)
{
    operation = static_cast<EPbQuestOpType>(Right.operation());
    quest_id = Right.quest_id();
}

void FPbQuestOpReq::ToPb(idlepb::QuestOpReq* Out) const
{
    Out->set_operation(static_cast<idlepb::QuestOpType>(operation));
    Out->set_quest_id(quest_id);    
}

void FPbQuestOpReq::Reset()
{
    operation = EPbQuestOpType();
    quest_id = int32();    
}

void FPbQuestOpReq::operator=(const idlepb::QuestOpReq& Right)
{
    this->FromPb(Right);
}

bool FPbQuestOpReq::operator==(const FPbQuestOpReq& Right) const
{
    if (this->operation != Right.operation)
        return false;
    if (this->quest_id != Right.quest_id)
        return false;
    return true;
}

bool FPbQuestOpReq::operator!=(const FPbQuestOpReq& Right) const
{
    return !operator==(Right);
}

FPbQuestOpAck::FPbQuestOpAck()
{
    Reset();        
}

FPbQuestOpAck::FPbQuestOpAck(const idlepb::QuestOpAck& Right)
{
    this->FromPb(Right);
}

void FPbQuestOpAck::FromPb(const idlepb::QuestOpAck& Right)
{
    ok = Right.ok();
    init_progress.Empty();
    for (const auto& Elem : Right.init_progress())
    {
        init_progress.Emplace(Elem);
    }
}

void FPbQuestOpAck::ToPb(idlepb::QuestOpAck* Out) const
{
    Out->set_ok(ok);
    for (const auto& Elem : init_progress)
    {
        Elem.ToPb(Out->add_init_progress());    
    }    
}

void FPbQuestOpAck::Reset()
{
    ok = bool();
    init_progress = TArray<FPbReplicateQuestProgressChange>();    
}

void FPbQuestOpAck::operator=(const idlepb::QuestOpAck& Right)
{
    this->FromPb(Right);
}

bool FPbQuestOpAck::operator==(const FPbQuestOpAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    if (this->init_progress != Right.init_progress)
        return false;
    return true;
}

bool FPbQuestOpAck::operator!=(const FPbQuestOpAck& Right) const
{
    return !operator==(Right);
}

FPbGetQuestDataReq::FPbGetQuestDataReq()
{
    Reset();        
}

FPbGetQuestDataReq::FPbGetQuestDataReq(const idlepb::GetQuestDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetQuestDataReq::FromPb(const idlepb::GetQuestDataReq& Right)
{
}

void FPbGetQuestDataReq::ToPb(idlepb::GetQuestDataReq* Out) const
{    
}

void FPbGetQuestDataReq::Reset()
{    
}

void FPbGetQuestDataReq::operator=(const idlepb::GetQuestDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetQuestDataReq::operator==(const FPbGetQuestDataReq& Right) const
{
    return true;
}

bool FPbGetQuestDataReq::operator!=(const FPbGetQuestDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetQuestDataAck::FPbGetQuestDataAck()
{
    Reset();        
}

FPbGetQuestDataAck::FPbGetQuestDataAck(const idlepb::GetQuestDataAck& Right)
{
    this->FromPb(Right);
}

void FPbGetQuestDataAck::FromPb(const idlepb::GetQuestDataAck& Right)
{
    data = Right.data();
}

void FPbGetQuestDataAck::ToPb(idlepb::GetQuestDataAck* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbGetQuestDataAck::Reset()
{
    data = FPbRoleQuestData();    
}

void FPbGetQuestDataAck::operator=(const idlepb::GetQuestDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetQuestDataAck::operator==(const FPbGetQuestDataAck& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbGetQuestDataAck::operator!=(const FPbGetQuestDataAck& Right) const
{
    return !operator==(Right);
}

FPbGetRoleLeaderboardDataReq::FPbGetRoleLeaderboardDataReq()
{
    Reset();        
}

FPbGetRoleLeaderboardDataReq::FPbGetRoleLeaderboardDataReq(const idlepb::GetRoleLeaderboardDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleLeaderboardDataReq::FromPb(const idlepb::GetRoleLeaderboardDataReq& Right)
{
    role_id = Right.role_id();
}

void FPbGetRoleLeaderboardDataReq::ToPb(idlepb::GetRoleLeaderboardDataReq* Out) const
{
    Out->set_role_id(role_id);    
}

void FPbGetRoleLeaderboardDataReq::Reset()
{
    role_id = int64();    
}

void FPbGetRoleLeaderboardDataReq::operator=(const idlepb::GetRoleLeaderboardDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleLeaderboardDataReq::operator==(const FPbGetRoleLeaderboardDataReq& Right) const
{
    if (this->role_id != Right.role_id)
        return false;
    return true;
}

bool FPbGetRoleLeaderboardDataReq::operator!=(const FPbGetRoleLeaderboardDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetRoleLeaderboardDataAck::FPbGetRoleLeaderboardDataAck()
{
    Reset();        
}

FPbGetRoleLeaderboardDataAck::FPbGetRoleLeaderboardDataAck(const idlepb::GetRoleLeaderboardDataAck& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleLeaderboardDataAck::FromPb(const idlepb::GetRoleLeaderboardDataAck& Right)
{
    data = Right.data();
    param_n1 = Right.param_n1();
    leaderboard_rank.Empty();
    for (const auto& Elem : Right.leaderboard_rank())
    {
        leaderboard_rank.Emplace(Elem);
    }
}

void FPbGetRoleLeaderboardDataAck::ToPb(idlepb::GetRoleLeaderboardDataAck* Out) const
{
    data.ToPb(Out->mutable_data());
    Out->set_param_n1(param_n1);
    for (const auto& Elem : leaderboard_rank)
    {
        Out->add_leaderboard_rank(Elem);    
    }    
}

void FPbGetRoleLeaderboardDataAck::Reset()
{
    data = FPbRoleLeaderboardData();
    param_n1 = int64();
    leaderboard_rank = TArray<int32>();    
}

void FPbGetRoleLeaderboardDataAck::operator=(const idlepb::GetRoleLeaderboardDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleLeaderboardDataAck::operator==(const FPbGetRoleLeaderboardDataAck& Right) const
{
    if (this->data != Right.data)
        return false;
    if (this->param_n1 != Right.param_n1)
        return false;
    if (this->leaderboard_rank != Right.leaderboard_rank)
        return false;
    return true;
}

bool FPbGetRoleLeaderboardDataAck::operator!=(const FPbGetRoleLeaderboardDataAck& Right) const
{
    return !operator==(Right);
}

FPbGetLeaderboardPreviewReq::FPbGetLeaderboardPreviewReq()
{
    Reset();        
}

FPbGetLeaderboardPreviewReq::FPbGetLeaderboardPreviewReq(const idlepb::GetLeaderboardPreviewReq& Right)
{
    this->FromPb(Right);
}

void FPbGetLeaderboardPreviewReq::FromPb(const idlepb::GetLeaderboardPreviewReq& Right)
{
}

void FPbGetLeaderboardPreviewReq::ToPb(idlepb::GetLeaderboardPreviewReq* Out) const
{    
}

void FPbGetLeaderboardPreviewReq::Reset()
{    
}

void FPbGetLeaderboardPreviewReq::operator=(const idlepb::GetLeaderboardPreviewReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetLeaderboardPreviewReq::operator==(const FPbGetLeaderboardPreviewReq& Right) const
{
    return true;
}

bool FPbGetLeaderboardPreviewReq::operator!=(const FPbGetLeaderboardPreviewReq& Right) const
{
    return !operator==(Right);
}

FPbGetLeaderboardPreviewAck::FPbGetLeaderboardPreviewAck()
{
    Reset();        
}

FPbGetLeaderboardPreviewAck::FPbGetLeaderboardPreviewAck(const idlepb::GetLeaderboardPreviewAck& Right)
{
    this->FromPb(Right);
}

void FPbGetLeaderboardPreviewAck::FromPb(const idlepb::GetLeaderboardPreviewAck& Right)
{
    champions.Empty();
    for (const auto& Elem : Right.champions())
    {
        champions.Emplace(Elem);
    }
    role_model_configs.Empty();
    for (const auto& Elem : Right.role_model_configs())
    {
        role_model_configs.Emplace(Elem);
    }
    my_data = Right.my_data();
    equipments.Empty();
    for (const auto& Elem : Right.equipments())
    {
        equipments.Emplace(Elem);
    }
    sept = Right.sept();
    last_refresh_time = Right.last_refresh_time();
}

void FPbGetLeaderboardPreviewAck::ToPb(idlepb::GetLeaderboardPreviewAck* Out) const
{
    for (const auto& Elem : champions)
    {
        Elem.ToPb(Out->add_champions());    
    }
    for (const auto& Elem : role_model_configs)
    {
        Elem.ToPb(Out->add_role_model_configs());    
    }
    my_data.ToPb(Out->mutable_my_data());
    for (const auto& Elem : equipments)
    {
        Elem.ToPb(Out->add_equipments());    
    }
    sept.ToPb(Out->mutable_sept());
    Out->set_last_refresh_time(last_refresh_time);    
}

void FPbGetLeaderboardPreviewAck::Reset()
{
    champions = TArray<FPbLeaderboardListItem>();
    role_model_configs = TArray<FPbCharacterModelConfig>();
    my_data = FPbRoleLeaderboardData();
    equipments = TArray<FPbItemData>();
    sept = FPbSeptDataOnLeaderboard();
    last_refresh_time = int64();    
}

void FPbGetLeaderboardPreviewAck::operator=(const idlepb::GetLeaderboardPreviewAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetLeaderboardPreviewAck::operator==(const FPbGetLeaderboardPreviewAck& Right) const
{
    if (this->champions != Right.champions)
        return false;
    if (this->role_model_configs != Right.role_model_configs)
        return false;
    if (this->my_data != Right.my_data)
        return false;
    if (this->equipments != Right.equipments)
        return false;
    if (this->sept != Right.sept)
        return false;
    if (this->last_refresh_time != Right.last_refresh_time)
        return false;
    return true;
}

bool FPbGetLeaderboardPreviewAck::operator!=(const FPbGetLeaderboardPreviewAck& Right) const
{
    return !operator==(Right);
}

FPbGetLeaderboardDataReq::FPbGetLeaderboardDataReq()
{
    Reset();        
}

FPbGetLeaderboardDataReq::FPbGetLeaderboardDataReq(const idlepb::GetLeaderboardDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetLeaderboardDataReq::FromPb(const idlepb::GetLeaderboardDataReq& Right)
{
    type = static_cast<EPbLeaderboardType>(Right.type());
}

void FPbGetLeaderboardDataReq::ToPb(idlepb::GetLeaderboardDataReq* Out) const
{
    Out->set_type(static_cast<idlepb::LeaderboardType>(type));    
}

void FPbGetLeaderboardDataReq::Reset()
{
    type = EPbLeaderboardType();    
}

void FPbGetLeaderboardDataReq::operator=(const idlepb::GetLeaderboardDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetLeaderboardDataReq::operator==(const FPbGetLeaderboardDataReq& Right) const
{
    if (this->type != Right.type)
        return false;
    return true;
}

bool FPbGetLeaderboardDataReq::operator!=(const FPbGetLeaderboardDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetLeaderboardDataAck::FPbGetLeaderboardDataAck()
{
    Reset();        
}

FPbGetLeaderboardDataAck::FPbGetLeaderboardDataAck(const idlepb::GetLeaderboardDataAck& Right)
{
    this->FromPb(Right);
}

void FPbGetLeaderboardDataAck::FromPb(const idlepb::GetLeaderboardDataAck& Right)
{
    last_refresh_time = Right.last_refresh_time();
    data.Empty();
    for (const auto& Elem : Right.data())
    {
        data.Emplace(Elem);
    }
    my_rank = Right.my_rank();
    rank1_message = UTF8_TO_TCHAR(Right.rank1_message().c_str());
    role_model_configs.Empty();
    for (const auto& Elem : Right.role_model_configs())
    {
        role_model_configs.Emplace(Elem);
    }
    equipments.Empty();
    for (const auto& Elem : Right.equipments())
    {
        equipments.Emplace(Elem);
    }
    shanghetu_records.Empty();
    for (const auto& Elem : Right.shanghetu_records())
    {
        shanghetu_records.Emplace(Elem);
    }
    top3_clicklike_num.Empty();
    for (const auto& Elem : Right.top3_clicklike_num())
    {
        top3_clicklike_num.Emplace(Elem);
    }
    septs.Empty();
    for (const auto& Elem : Right.septs())
    {
        septs.Emplace(Elem);
    }
}

void FPbGetLeaderboardDataAck::ToPb(idlepb::GetLeaderboardDataAck* Out) const
{
    Out->set_last_refresh_time(last_refresh_time);
    for (const auto& Elem : data)
    {
        Elem.ToPb(Out->add_data());    
    }
    Out->set_my_rank(my_rank);
    Out->set_rank1_message(TCHAR_TO_UTF8(*rank1_message));
    for (const auto& Elem : role_model_configs)
    {
        Elem.ToPb(Out->add_role_model_configs());    
    }
    for (const auto& Elem : equipments)
    {
        Elem.ToPb(Out->add_equipments());    
    }
    for (const auto& Elem : shanghetu_records)
    {
        Elem.ToPb(Out->add_shanghetu_records());    
    }
    for (const auto& Elem : top3_clicklike_num)
    {
        Out->add_top3_clicklike_num(Elem);    
    }
    for (const auto& Elem : septs)
    {
        Elem.ToPb(Out->add_septs());    
    }    
}

void FPbGetLeaderboardDataAck::Reset()
{
    last_refresh_time = int64();
    data = TArray<FPbLeaderboardListItem>();
    my_rank = int32();
    rank1_message = FString();
    role_model_configs = TArray<FPbCharacterModelConfig>();
    equipments = TArray<FPbItemData>();
    shanghetu_records = TArray<FPbShanhetuRecord>();
    top3_clicklike_num = TArray<int32>();
    septs = TArray<FPbSeptDataOnLeaderboard>();    
}

void FPbGetLeaderboardDataAck::operator=(const idlepb::GetLeaderboardDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetLeaderboardDataAck::operator==(const FPbGetLeaderboardDataAck& Right) const
{
    if (this->last_refresh_time != Right.last_refresh_time)
        return false;
    if (this->data != Right.data)
        return false;
    if (this->my_rank != Right.my_rank)
        return false;
    if (this->rank1_message != Right.rank1_message)
        return false;
    if (this->role_model_configs != Right.role_model_configs)
        return false;
    if (this->equipments != Right.equipments)
        return false;
    if (this->shanghetu_records != Right.shanghetu_records)
        return false;
    if (this->top3_clicklike_num != Right.top3_clicklike_num)
        return false;
    if (this->septs != Right.septs)
        return false;
    return true;
}

bool FPbGetLeaderboardDataAck::operator!=(const FPbGetLeaderboardDataAck& Right) const
{
    return !operator==(Right);
}

FPbLeaderboardClickLikeReq::FPbLeaderboardClickLikeReq()
{
    Reset();        
}

FPbLeaderboardClickLikeReq::FPbLeaderboardClickLikeReq(const idlepb::LeaderboardClickLikeReq& Right)
{
    this->FromPb(Right);
}

void FPbLeaderboardClickLikeReq::FromPb(const idlepb::LeaderboardClickLikeReq& Right)
{
    role_id = Right.role_id();
    type = static_cast<EPbLeaderboardType>(Right.type());
}

void FPbLeaderboardClickLikeReq::ToPb(idlepb::LeaderboardClickLikeReq* Out) const
{
    Out->set_role_id(role_id);
    Out->set_type(static_cast<idlepb::LeaderboardType>(type));    
}

void FPbLeaderboardClickLikeReq::Reset()
{
    role_id = int64();
    type = EPbLeaderboardType();    
}

void FPbLeaderboardClickLikeReq::operator=(const idlepb::LeaderboardClickLikeReq& Right)
{
    this->FromPb(Right);
}

bool FPbLeaderboardClickLikeReq::operator==(const FPbLeaderboardClickLikeReq& Right) const
{
    if (this->role_id != Right.role_id)
        return false;
    if (this->type != Right.type)
        return false;
    return true;
}

bool FPbLeaderboardClickLikeReq::operator!=(const FPbLeaderboardClickLikeReq& Right) const
{
    return !operator==(Right);
}

FPbLeaderboardClickLikeAck::FPbLeaderboardClickLikeAck()
{
    Reset();        
}

FPbLeaderboardClickLikeAck::FPbLeaderboardClickLikeAck(const idlepb::LeaderboardClickLikeAck& Right)
{
    this->FromPb(Right);
}

void FPbLeaderboardClickLikeAck::FromPb(const idlepb::LeaderboardClickLikeAck& Right)
{
    ok = Right.ok();
}

void FPbLeaderboardClickLikeAck::ToPb(idlepb::LeaderboardClickLikeAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbLeaderboardClickLikeAck::Reset()
{
    ok = bool();    
}

void FPbLeaderboardClickLikeAck::operator=(const idlepb::LeaderboardClickLikeAck& Right)
{
    this->FromPb(Right);
}

bool FPbLeaderboardClickLikeAck::operator==(const FPbLeaderboardClickLikeAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbLeaderboardClickLikeAck::operator!=(const FPbLeaderboardClickLikeAck& Right) const
{
    return !operator==(Right);
}

FPbLeaderboardUpdateMessageReq::FPbLeaderboardUpdateMessageReq()
{
    Reset();        
}

FPbLeaderboardUpdateMessageReq::FPbLeaderboardUpdateMessageReq(const idlepb::LeaderboardUpdateMessageReq& Right)
{
    this->FromPb(Right);
}

void FPbLeaderboardUpdateMessageReq::FromPb(const idlepb::LeaderboardUpdateMessageReq& Right)
{
    new_message = UTF8_TO_TCHAR(Right.new_message().c_str());
}

void FPbLeaderboardUpdateMessageReq::ToPb(idlepb::LeaderboardUpdateMessageReq* Out) const
{
    Out->set_new_message(TCHAR_TO_UTF8(*new_message));    
}

void FPbLeaderboardUpdateMessageReq::Reset()
{
    new_message = FString();    
}

void FPbLeaderboardUpdateMessageReq::operator=(const idlepb::LeaderboardUpdateMessageReq& Right)
{
    this->FromPb(Right);
}

bool FPbLeaderboardUpdateMessageReq::operator==(const FPbLeaderboardUpdateMessageReq& Right) const
{
    if (this->new_message != Right.new_message)
        return false;
    return true;
}

bool FPbLeaderboardUpdateMessageReq::operator!=(const FPbLeaderboardUpdateMessageReq& Right) const
{
    return !operator==(Right);
}

FPbLeaderboardUpdateMessageAck::FPbLeaderboardUpdateMessageAck()
{
    Reset();        
}

FPbLeaderboardUpdateMessageAck::FPbLeaderboardUpdateMessageAck(const idlepb::LeaderboardUpdateMessageAck& Right)
{
    this->FromPb(Right);
}

void FPbLeaderboardUpdateMessageAck::FromPb(const idlepb::LeaderboardUpdateMessageAck& Right)
{
    ok = Right.ok();
}

void FPbLeaderboardUpdateMessageAck::ToPb(idlepb::LeaderboardUpdateMessageAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbLeaderboardUpdateMessageAck::Reset()
{
    ok = bool();    
}

void FPbLeaderboardUpdateMessageAck::operator=(const idlepb::LeaderboardUpdateMessageAck& Right)
{
    this->FromPb(Right);
}

bool FPbLeaderboardUpdateMessageAck::operator==(const FPbLeaderboardUpdateMessageAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbLeaderboardUpdateMessageAck::operator!=(const FPbLeaderboardUpdateMessageAck& Right) const
{
    return !operator==(Right);
}

FPbGetMonsterTowerChallengeListReq::FPbGetMonsterTowerChallengeListReq()
{
    Reset();        
}

FPbGetMonsterTowerChallengeListReq::FPbGetMonsterTowerChallengeListReq(const idlepb::GetMonsterTowerChallengeListReq& Right)
{
    this->FromPb(Right);
}

void FPbGetMonsterTowerChallengeListReq::FromPb(const idlepb::GetMonsterTowerChallengeListReq& Right)
{
    challenge_id = Right.challenge_id();
}

void FPbGetMonsterTowerChallengeListReq::ToPb(idlepb::GetMonsterTowerChallengeListReq* Out) const
{
    Out->set_challenge_id(challenge_id);    
}

void FPbGetMonsterTowerChallengeListReq::Reset()
{
    challenge_id = int32();    
}

void FPbGetMonsterTowerChallengeListReq::operator=(const idlepb::GetMonsterTowerChallengeListReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetMonsterTowerChallengeListReq::operator==(const FPbGetMonsterTowerChallengeListReq& Right) const
{
    if (this->challenge_id != Right.challenge_id)
        return false;
    return true;
}

bool FPbGetMonsterTowerChallengeListReq::operator!=(const FPbGetMonsterTowerChallengeListReq& Right) const
{
    return !operator==(Right);
}

FPbGetMonsterTowerChallengeListAck::FPbGetMonsterTowerChallengeListAck()
{
    Reset();        
}

FPbGetMonsterTowerChallengeListAck::FPbGetMonsterTowerChallengeListAck(const idlepb::GetMonsterTowerChallengeListAck& Right)
{
    this->FromPb(Right);
}

void FPbGetMonsterTowerChallengeListAck::FromPb(const idlepb::GetMonsterTowerChallengeListAck& Right)
{
    data.Empty();
    for (const auto& Elem : Right.data())
    {
        data.Emplace(Elem);
    }
    model_configs.Empty();
    for (const auto& Elem : Right.model_configs())
    {
        model_configs.Emplace(Elem);
    }
    all_list_progress.Empty();
    for (const auto& Elem : Right.all_list_progress())
    {
        all_list_progress.Emplace(Elem);
    }
}

void FPbGetMonsterTowerChallengeListAck::ToPb(idlepb::GetMonsterTowerChallengeListAck* Out) const
{
    for (const auto& Elem : data)
    {
        Elem.ToPb(Out->add_data());    
    }
    for (const auto& Elem : model_configs)
    {
        Elem.ToPb(Out->add_model_configs());    
    }
    for (const auto& Elem : all_list_progress)
    {
        Out->add_all_list_progress(Elem);    
    }    
}

void FPbGetMonsterTowerChallengeListAck::Reset()
{
    data = TArray<FPbLeaderboardListItem>();
    model_configs = TArray<FPbCharacterModelConfig>();
    all_list_progress = TArray<int32>();    
}

void FPbGetMonsterTowerChallengeListAck::operator=(const idlepb::GetMonsterTowerChallengeListAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetMonsterTowerChallengeListAck::operator==(const FPbGetMonsterTowerChallengeListAck& Right) const
{
    if (this->data != Right.data)
        return false;
    if (this->model_configs != Right.model_configs)
        return false;
    if (this->all_list_progress != Right.all_list_progress)
        return false;
    return true;
}

bool FPbGetMonsterTowerChallengeListAck::operator!=(const FPbGetMonsterTowerChallengeListAck& Right) const
{
    return !operator==(Right);
}

FPbGetMonsterTowerChallengeRewardReq::FPbGetMonsterTowerChallengeRewardReq()
{
    Reset();        
}

FPbGetMonsterTowerChallengeRewardReq::FPbGetMonsterTowerChallengeRewardReq(const idlepb::GetMonsterTowerChallengeRewardReq& Right)
{
    this->FromPb(Right);
}

void FPbGetMonsterTowerChallengeRewardReq::FromPb(const idlepb::GetMonsterTowerChallengeRewardReq& Right)
{
    challenge_id = Right.challenge_id();
}

void FPbGetMonsterTowerChallengeRewardReq::ToPb(idlepb::GetMonsterTowerChallengeRewardReq* Out) const
{
    Out->set_challenge_id(challenge_id);    
}

void FPbGetMonsterTowerChallengeRewardReq::Reset()
{
    challenge_id = int32();    
}

void FPbGetMonsterTowerChallengeRewardReq::operator=(const idlepb::GetMonsterTowerChallengeRewardReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetMonsterTowerChallengeRewardReq::operator==(const FPbGetMonsterTowerChallengeRewardReq& Right) const
{
    if (this->challenge_id != Right.challenge_id)
        return false;
    return true;
}

bool FPbGetMonsterTowerChallengeRewardReq::operator!=(const FPbGetMonsterTowerChallengeRewardReq& Right) const
{
    return !operator==(Right);
}

FPbGetMonsterTowerChallengeRewardAck::FPbGetMonsterTowerChallengeRewardAck()
{
    Reset();        
}

FPbGetMonsterTowerChallengeRewardAck::FPbGetMonsterTowerChallengeRewardAck(const idlepb::GetMonsterTowerChallengeRewardAck& Right)
{
    this->FromPb(Right);
}

void FPbGetMonsterTowerChallengeRewardAck::FromPb(const idlepb::GetMonsterTowerChallengeRewardAck& Right)
{
    ok = Right.ok();
}

void FPbGetMonsterTowerChallengeRewardAck::ToPb(idlepb::GetMonsterTowerChallengeRewardAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbGetMonsterTowerChallengeRewardAck::Reset()
{
    ok = bool();    
}

void FPbGetMonsterTowerChallengeRewardAck::operator=(const idlepb::GetMonsterTowerChallengeRewardAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetMonsterTowerChallengeRewardAck::operator==(const FPbGetMonsterTowerChallengeRewardAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbGetMonsterTowerChallengeRewardAck::operator!=(const FPbGetMonsterTowerChallengeRewardAck& Right) const
{
    return !operator==(Right);
}

FPbGetFuZeRewardReq::FPbGetFuZeRewardReq()
{
    Reset();        
}

FPbGetFuZeRewardReq::FPbGetFuZeRewardReq(const idlepb::GetFuZeRewardReq& Right)
{
    this->FromPb(Right);
}

void FPbGetFuZeRewardReq::FromPb(const idlepb::GetFuZeRewardReq& Right)
{
}

void FPbGetFuZeRewardReq::ToPb(idlepb::GetFuZeRewardReq* Out) const
{    
}

void FPbGetFuZeRewardReq::Reset()
{    
}

void FPbGetFuZeRewardReq::operator=(const idlepb::GetFuZeRewardReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetFuZeRewardReq::operator==(const FPbGetFuZeRewardReq& Right) const
{
    return true;
}

bool FPbGetFuZeRewardReq::operator!=(const FPbGetFuZeRewardReq& Right) const
{
    return !operator==(Right);
}

FPbGetFuZeRewardAck::FPbGetFuZeRewardAck()
{
    Reset();        
}

FPbGetFuZeRewardAck::FPbGetFuZeRewardAck(const idlepb::GetFuZeRewardAck& Right)
{
    this->FromPb(Right);
}

void FPbGetFuZeRewardAck::FromPb(const idlepb::GetFuZeRewardAck& Right)
{
    ok = Right.ok();
}

void FPbGetFuZeRewardAck::ToPb(idlepb::GetFuZeRewardAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbGetFuZeRewardAck::Reset()
{
    ok = bool();    
}

void FPbGetFuZeRewardAck::operator=(const idlepb::GetFuZeRewardAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetFuZeRewardAck::operator==(const FPbGetFuZeRewardAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbGetFuZeRewardAck::operator!=(const FPbGetFuZeRewardAck& Right) const
{
    return !operator==(Right);
}

FPbGetRoleMailDataReq::FPbGetRoleMailDataReq()
{
    Reset();        
}

FPbGetRoleMailDataReq::FPbGetRoleMailDataReq(const idlepb::GetRoleMailDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleMailDataReq::FromPb(const idlepb::GetRoleMailDataReq& Right)
{
    only_num = Right.only_num();
}

void FPbGetRoleMailDataReq::ToPb(idlepb::GetRoleMailDataReq* Out) const
{
    Out->set_only_num(only_num);    
}

void FPbGetRoleMailDataReq::Reset()
{
    only_num = bool();    
}

void FPbGetRoleMailDataReq::operator=(const idlepb::GetRoleMailDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleMailDataReq::operator==(const FPbGetRoleMailDataReq& Right) const
{
    if (this->only_num != Right.only_num)
        return false;
    return true;
}

bool FPbGetRoleMailDataReq::operator!=(const FPbGetRoleMailDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetRoleMailDataAck::FPbGetRoleMailDataAck()
{
    Reset();        
}

FPbGetRoleMailDataAck::FPbGetRoleMailDataAck(const idlepb::GetRoleMailDataAck& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleMailDataAck::FromPb(const idlepb::GetRoleMailDataAck& Right)
{
    unread_mail_num = Right.unread_mail_num();
    mail_box.Empty();
    for (const auto& Elem : Right.mail_box())
    {
        mail_box.Emplace(Elem);
    }
}

void FPbGetRoleMailDataAck::ToPb(idlepb::GetRoleMailDataAck* Out) const
{
    Out->set_unread_mail_num(unread_mail_num);
    for (const auto& Elem : mail_box)
    {
        Elem.ToPb(Out->add_mail_box());    
    }    
}

void FPbGetRoleMailDataAck::Reset()
{
    unread_mail_num = int32();
    mail_box = TArray<FPbMail>();    
}

void FPbGetRoleMailDataAck::operator=(const idlepb::GetRoleMailDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleMailDataAck::operator==(const FPbGetRoleMailDataAck& Right) const
{
    if (this->unread_mail_num != Right.unread_mail_num)
        return false;
    if (this->mail_box != Right.mail_box)
        return false;
    return true;
}

bool FPbGetRoleMailDataAck::operator!=(const FPbGetRoleMailDataAck& Right) const
{
    return !operator==(Right);
}

FPbUpdateRoleMail::FPbUpdateRoleMail()
{
    Reset();        
}

FPbUpdateRoleMail::FPbUpdateRoleMail(const idlepb::UpdateRoleMail& Right)
{
    this->FromPb(Right);
}

void FPbUpdateRoleMail::FromPb(const idlepb::UpdateRoleMail& Right)
{
}

void FPbUpdateRoleMail::ToPb(idlepb::UpdateRoleMail* Out) const
{    
}

void FPbUpdateRoleMail::Reset()
{    
}

void FPbUpdateRoleMail::operator=(const idlepb::UpdateRoleMail& Right)
{
    this->FromPb(Right);
}

bool FPbUpdateRoleMail::operator==(const FPbUpdateRoleMail& Right) const
{
    return true;
}

bool FPbUpdateRoleMail::operator!=(const FPbUpdateRoleMail& Right) const
{
    return !operator==(Right);
}

FPbReadMailReq::FPbReadMailReq()
{
    Reset();        
}

FPbReadMailReq::FPbReadMailReq(const idlepb::ReadMailReq& Right)
{
    this->FromPb(Right);
}

void FPbReadMailReq::FromPb(const idlepb::ReadMailReq& Right)
{
    index = Right.index();
}

void FPbReadMailReq::ToPb(idlepb::ReadMailReq* Out) const
{
    Out->set_index(index);    
}

void FPbReadMailReq::Reset()
{
    index = int32();    
}

void FPbReadMailReq::operator=(const idlepb::ReadMailReq& Right)
{
    this->FromPb(Right);
}

bool FPbReadMailReq::operator==(const FPbReadMailReq& Right) const
{
    if (this->index != Right.index)
        return false;
    return true;
}

bool FPbReadMailReq::operator!=(const FPbReadMailReq& Right) const
{
    return !operator==(Right);
}

FPbReadMailAck::FPbReadMailAck()
{
    Reset();        
}

FPbReadMailAck::FPbReadMailAck(const idlepb::ReadMailAck& Right)
{
    this->FromPb(Right);
}

void FPbReadMailAck::FromPb(const idlepb::ReadMailAck& Right)
{
    ok = Right.ok();
    mail = Right.mail();
}

void FPbReadMailAck::ToPb(idlepb::ReadMailAck* Out) const
{
    Out->set_ok(ok);
    mail.ToPb(Out->mutable_mail());    
}

void FPbReadMailAck::Reset()
{
    ok = bool();
    mail = FPbMail();    
}

void FPbReadMailAck::operator=(const idlepb::ReadMailAck& Right)
{
    this->FromPb(Right);
}

bool FPbReadMailAck::operator==(const FPbReadMailAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    if (this->mail != Right.mail)
        return false;
    return true;
}

bool FPbReadMailAck::operator!=(const FPbReadMailAck& Right) const
{
    return !operator==(Right);
}

FPbGetMailAttachmentReq::FPbGetMailAttachmentReq()
{
    Reset();        
}

FPbGetMailAttachmentReq::FPbGetMailAttachmentReq(const idlepb::GetMailAttachmentReq& Right)
{
    this->FromPb(Right);
}

void FPbGetMailAttachmentReq::FromPb(const idlepb::GetMailAttachmentReq& Right)
{
    index = Right.index();
}

void FPbGetMailAttachmentReq::ToPb(idlepb::GetMailAttachmentReq* Out) const
{
    Out->set_index(index);    
}

void FPbGetMailAttachmentReq::Reset()
{
    index = int32();    
}

void FPbGetMailAttachmentReq::operator=(const idlepb::GetMailAttachmentReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetMailAttachmentReq::operator==(const FPbGetMailAttachmentReq& Right) const
{
    if (this->index != Right.index)
        return false;
    return true;
}

bool FPbGetMailAttachmentReq::operator!=(const FPbGetMailAttachmentReq& Right) const
{
    return !operator==(Right);
}

FPbGetMailAttachmentAck::FPbGetMailAttachmentAck()
{
    Reset();        
}

FPbGetMailAttachmentAck::FPbGetMailAttachmentAck(const idlepb::GetMailAttachmentAck& Right)
{
    this->FromPb(Right);
}

void FPbGetMailAttachmentAck::FromPb(const idlepb::GetMailAttachmentAck& Right)
{
    result = static_cast<EPbMailOperation>(Right.result());
    mail_data = Right.mail_data();
}

void FPbGetMailAttachmentAck::ToPb(idlepb::GetMailAttachmentAck* Out) const
{
    Out->set_result(static_cast<idlepb::MailOperation>(result));
    mail_data.ToPb(Out->mutable_mail_data());    
}

void FPbGetMailAttachmentAck::Reset()
{
    result = EPbMailOperation();
    mail_data = FPbMail();    
}

void FPbGetMailAttachmentAck::operator=(const idlepb::GetMailAttachmentAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetMailAttachmentAck::operator==(const FPbGetMailAttachmentAck& Right) const
{
    if (this->result != Right.result)
        return false;
    if (this->mail_data != Right.mail_data)
        return false;
    return true;
}

bool FPbGetMailAttachmentAck::operator!=(const FPbGetMailAttachmentAck& Right) const
{
    return !operator==(Right);
}

FPbDeleteMailReq::FPbDeleteMailReq()
{
    Reset();        
}

FPbDeleteMailReq::FPbDeleteMailReq(const idlepb::DeleteMailReq& Right)
{
    this->FromPb(Right);
}

void FPbDeleteMailReq::FromPb(const idlepb::DeleteMailReq& Right)
{
    index = Right.index();
}

void FPbDeleteMailReq::ToPb(idlepb::DeleteMailReq* Out) const
{
    Out->set_index(index);    
}

void FPbDeleteMailReq::Reset()
{
    index = int32();    
}

void FPbDeleteMailReq::operator=(const idlepb::DeleteMailReq& Right)
{
    this->FromPb(Right);
}

bool FPbDeleteMailReq::operator==(const FPbDeleteMailReq& Right) const
{
    if (this->index != Right.index)
        return false;
    return true;
}

bool FPbDeleteMailReq::operator!=(const FPbDeleteMailReq& Right) const
{
    return !operator==(Right);
}

FPbDeleteMailAck::FPbDeleteMailAck()
{
    Reset();        
}

FPbDeleteMailAck::FPbDeleteMailAck(const idlepb::DeleteMailAck& Right)
{
    this->FromPb(Right);
}

void FPbDeleteMailAck::FromPb(const idlepb::DeleteMailAck& Right)
{
    ok = Right.ok();
}

void FPbDeleteMailAck::ToPb(idlepb::DeleteMailAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbDeleteMailAck::Reset()
{
    ok = bool();    
}

void FPbDeleteMailAck::operator=(const idlepb::DeleteMailAck& Right)
{
    this->FromPb(Right);
}

bool FPbDeleteMailAck::operator==(const FPbDeleteMailAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbDeleteMailAck::operator!=(const FPbDeleteMailAck& Right) const
{
    return !operator==(Right);
}

FPbOneClickGetMailAttachmentReq::FPbOneClickGetMailAttachmentReq()
{
    Reset();        
}

FPbOneClickGetMailAttachmentReq::FPbOneClickGetMailAttachmentReq(const idlepb::OneClickGetMailAttachmentReq& Right)
{
    this->FromPb(Right);
}

void FPbOneClickGetMailAttachmentReq::FromPb(const idlepb::OneClickGetMailAttachmentReq& Right)
{
}

void FPbOneClickGetMailAttachmentReq::ToPb(idlepb::OneClickGetMailAttachmentReq* Out) const
{    
}

void FPbOneClickGetMailAttachmentReq::Reset()
{    
}

void FPbOneClickGetMailAttachmentReq::operator=(const idlepb::OneClickGetMailAttachmentReq& Right)
{
    this->FromPb(Right);
}

bool FPbOneClickGetMailAttachmentReq::operator==(const FPbOneClickGetMailAttachmentReq& Right) const
{
    return true;
}

bool FPbOneClickGetMailAttachmentReq::operator!=(const FPbOneClickGetMailAttachmentReq& Right) const
{
    return !operator==(Right);
}

FPbOneClickGetMailAttachmentAck::FPbOneClickGetMailAttachmentAck()
{
    Reset();        
}

FPbOneClickGetMailAttachmentAck::FPbOneClickGetMailAttachmentAck(const idlepb::OneClickGetMailAttachmentAck& Right)
{
    this->FromPb(Right);
}

void FPbOneClickGetMailAttachmentAck::FromPb(const idlepb::OneClickGetMailAttachmentAck& Right)
{
    result = static_cast<EPbMailOperation>(Right.result());
    unread_mail_num = Right.unread_mail_num();
    mail_box.Empty();
    for (const auto& Elem : Right.mail_box())
    {
        mail_box.Emplace(Elem);
    }
}

void FPbOneClickGetMailAttachmentAck::ToPb(idlepb::OneClickGetMailAttachmentAck* Out) const
{
    Out->set_result(static_cast<idlepb::MailOperation>(result));
    Out->set_unread_mail_num(unread_mail_num);
    for (const auto& Elem : mail_box)
    {
        Elem.ToPb(Out->add_mail_box());    
    }    
}

void FPbOneClickGetMailAttachmentAck::Reset()
{
    result = EPbMailOperation();
    unread_mail_num = int32();
    mail_box = TArray<FPbMail>();    
}

void FPbOneClickGetMailAttachmentAck::operator=(const idlepb::OneClickGetMailAttachmentAck& Right)
{
    this->FromPb(Right);
}

bool FPbOneClickGetMailAttachmentAck::operator==(const FPbOneClickGetMailAttachmentAck& Right) const
{
    if (this->result != Right.result)
        return false;
    if (this->unread_mail_num != Right.unread_mail_num)
        return false;
    if (this->mail_box != Right.mail_box)
        return false;
    return true;
}

bool FPbOneClickGetMailAttachmentAck::operator!=(const FPbOneClickGetMailAttachmentAck& Right) const
{
    return !operator==(Right);
}

FPbOneClickReadMailReq::FPbOneClickReadMailReq()
{
    Reset();        
}

FPbOneClickReadMailReq::FPbOneClickReadMailReq(const idlepb::OneClickReadMailReq& Right)
{
    this->FromPb(Right);
}

void FPbOneClickReadMailReq::FromPb(const idlepb::OneClickReadMailReq& Right)
{
}

void FPbOneClickReadMailReq::ToPb(idlepb::OneClickReadMailReq* Out) const
{    
}

void FPbOneClickReadMailReq::Reset()
{    
}

void FPbOneClickReadMailReq::operator=(const idlepb::OneClickReadMailReq& Right)
{
    this->FromPb(Right);
}

bool FPbOneClickReadMailReq::operator==(const FPbOneClickReadMailReq& Right) const
{
    return true;
}

bool FPbOneClickReadMailReq::operator!=(const FPbOneClickReadMailReq& Right) const
{
    return !operator==(Right);
}

FPbOneClickReadMailAck::FPbOneClickReadMailAck()
{
    Reset();        
}

FPbOneClickReadMailAck::FPbOneClickReadMailAck(const idlepb::OneClickReadMailAck& Right)
{
    this->FromPb(Right);
}

void FPbOneClickReadMailAck::FromPb(const idlepb::OneClickReadMailAck& Right)
{
    ok = Right.ok();
}

void FPbOneClickReadMailAck::ToPb(idlepb::OneClickReadMailAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbOneClickReadMailAck::Reset()
{
    ok = bool();    
}

void FPbOneClickReadMailAck::operator=(const idlepb::OneClickReadMailAck& Right)
{
    this->FromPb(Right);
}

bool FPbOneClickReadMailAck::operator==(const FPbOneClickReadMailAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbOneClickReadMailAck::operator!=(const FPbOneClickReadMailAck& Right) const
{
    return !operator==(Right);
}

FPbOneClickDeleteMailReq::FPbOneClickDeleteMailReq()
{
    Reset();        
}

FPbOneClickDeleteMailReq::FPbOneClickDeleteMailReq(const idlepb::OneClickDeleteMailReq& Right)
{
    this->FromPb(Right);
}

void FPbOneClickDeleteMailReq::FromPb(const idlepb::OneClickDeleteMailReq& Right)
{
}

void FPbOneClickDeleteMailReq::ToPb(idlepb::OneClickDeleteMailReq* Out) const
{    
}

void FPbOneClickDeleteMailReq::Reset()
{    
}

void FPbOneClickDeleteMailReq::operator=(const idlepb::OneClickDeleteMailReq& Right)
{
    this->FromPb(Right);
}

bool FPbOneClickDeleteMailReq::operator==(const FPbOneClickDeleteMailReq& Right) const
{
    return true;
}

bool FPbOneClickDeleteMailReq::operator!=(const FPbOneClickDeleteMailReq& Right) const
{
    return !operator==(Right);
}

FPbOneClickDeleteMailAck::FPbOneClickDeleteMailAck()
{
    Reset();        
}

FPbOneClickDeleteMailAck::FPbOneClickDeleteMailAck(const idlepb::OneClickDeleteMailAck& Right)
{
    this->FromPb(Right);
}

void FPbOneClickDeleteMailAck::FromPb(const idlepb::OneClickDeleteMailAck& Right)
{
    deleted_index.Empty();
    for (const auto& Elem : Right.deleted_index())
    {
        deleted_index.Emplace(Elem);
    }
}

void FPbOneClickDeleteMailAck::ToPb(idlepb::OneClickDeleteMailAck* Out) const
{
    for (const auto& Elem : deleted_index)
    {
        Out->add_deleted_index(Elem);    
    }    
}

void FPbOneClickDeleteMailAck::Reset()
{
    deleted_index = TArray<int32>();    
}

void FPbOneClickDeleteMailAck::operator=(const idlepb::OneClickDeleteMailAck& Right)
{
    this->FromPb(Right);
}

bool FPbOneClickDeleteMailAck::operator==(const FPbOneClickDeleteMailAck& Right) const
{
    if (this->deleted_index != Right.deleted_index)
        return false;
    return true;
}

bool FPbOneClickDeleteMailAck::operator!=(const FPbOneClickDeleteMailAck& Right) const
{
    return !operator==(Right);
}

FPbUnlockFunctionModuleReq::FPbUnlockFunctionModuleReq()
{
    Reset();        
}

FPbUnlockFunctionModuleReq::FPbUnlockFunctionModuleReq(const idlepb::UnlockFunctionModuleReq& Right)
{
    this->FromPb(Right);
}

void FPbUnlockFunctionModuleReq::FromPb(const idlepb::UnlockFunctionModuleReq& Right)
{
    type = static_cast<EPbFunctionModuleType>(Right.type());
}

void FPbUnlockFunctionModuleReq::ToPb(idlepb::UnlockFunctionModuleReq* Out) const
{
    Out->set_type(static_cast<idlepb::FunctionModuleType>(type));    
}

void FPbUnlockFunctionModuleReq::Reset()
{
    type = EPbFunctionModuleType();    
}

void FPbUnlockFunctionModuleReq::operator=(const idlepb::UnlockFunctionModuleReq& Right)
{
    this->FromPb(Right);
}

bool FPbUnlockFunctionModuleReq::operator==(const FPbUnlockFunctionModuleReq& Right) const
{
    if (this->type != Right.type)
        return false;
    return true;
}

bool FPbUnlockFunctionModuleReq::operator!=(const FPbUnlockFunctionModuleReq& Right) const
{
    return !operator==(Right);
}

FPbUnlockFunctionModuleAck::FPbUnlockFunctionModuleAck()
{
    Reset();        
}

FPbUnlockFunctionModuleAck::FPbUnlockFunctionModuleAck(const idlepb::UnlockFunctionModuleAck& Right)
{
    this->FromPb(Right);
}

void FPbUnlockFunctionModuleAck::FromPb(const idlepb::UnlockFunctionModuleAck& Right)
{
    ok = Right.ok();
}

void FPbUnlockFunctionModuleAck::ToPb(idlepb::UnlockFunctionModuleAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbUnlockFunctionModuleAck::Reset()
{
    ok = bool();    
}

void FPbUnlockFunctionModuleAck::operator=(const idlepb::UnlockFunctionModuleAck& Right)
{
    this->FromPb(Right);
}

bool FPbUnlockFunctionModuleAck::operator==(const FPbUnlockFunctionModuleAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbUnlockFunctionModuleAck::operator!=(const FPbUnlockFunctionModuleAck& Right) const
{
    return !operator==(Right);
}

FPbNotifyUnlockedModuels::FPbNotifyUnlockedModuels()
{
    Reset();        
}

FPbNotifyUnlockedModuels::FPbNotifyUnlockedModuels(const idlepb::NotifyUnlockedModuels& Right)
{
    this->FromPb(Right);
}

void FPbNotifyUnlockedModuels::FromPb(const idlepb::NotifyUnlockedModuels& Right)
{
    unlocked_modules.Empty();
    for (const auto& Elem : Right.unlocked_modules())
    {
        unlocked_modules.Emplace(Elem);
    }
}

void FPbNotifyUnlockedModuels::ToPb(idlepb::NotifyUnlockedModuels* Out) const
{
    for (const auto& Elem : unlocked_modules)
    {
        Out->add_unlocked_modules(Elem);    
    }    
}

void FPbNotifyUnlockedModuels::Reset()
{
    unlocked_modules = TArray<int32>();    
}

void FPbNotifyUnlockedModuels::operator=(const idlepb::NotifyUnlockedModuels& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyUnlockedModuels::operator==(const FPbNotifyUnlockedModuels& Right) const
{
    if (this->unlocked_modules != Right.unlocked_modules)
        return false;
    return true;
}

bool FPbNotifyUnlockedModuels::operator!=(const FPbNotifyUnlockedModuels& Right) const
{
    return !operator==(Right);
}

FPbUpdateChat::FPbUpdateChat()
{
    Reset();        
}

FPbUpdateChat::FPbUpdateChat(const idlepb::UpdateChat& Right)
{
    this->FromPb(Right);
}

void FPbUpdateChat::FromPb(const idlepb::UpdateChat& Right)
{
    channel = static_cast<EPbChatMessageChannel>(Right.channel());
    chat_message = Right.chat_message();
}

void FPbUpdateChat::ToPb(idlepb::UpdateChat* Out) const
{
    Out->set_channel(static_cast<idlepb::ChatMessageChannel>(channel));
    chat_message.ToPb(Out->mutable_chat_message());    
}

void FPbUpdateChat::Reset()
{
    channel = EPbChatMessageChannel();
    chat_message = FPbChatMessage();    
}

void FPbUpdateChat::operator=(const idlepb::UpdateChat& Right)
{
    this->FromPb(Right);
}

bool FPbUpdateChat::operator==(const FPbUpdateChat& Right) const
{
    if (this->channel != Right.channel)
        return false;
    if (this->chat_message != Right.chat_message)
        return false;
    return true;
}

bool FPbUpdateChat::operator!=(const FPbUpdateChat& Right) const
{
    return !operator==(Right);
}

FPbSendChatMessageReq::FPbSendChatMessageReq()
{
    Reset();        
}

FPbSendChatMessageReq::FPbSendChatMessageReq(const idlepb::SendChatMessageReq& Right)
{
    this->FromPb(Right);
}

void FPbSendChatMessageReq::FromPb(const idlepb::SendChatMessageReq& Right)
{
    role_id = Right.role_id();
    channel = static_cast<EPbChatMessageChannel>(Right.channel());
    text = UTF8_TO_TCHAR(Right.text().c_str());
    type = static_cast<EPbChatMessageType>(Right.type());
}

void FPbSendChatMessageReq::ToPb(idlepb::SendChatMessageReq* Out) const
{
    Out->set_role_id(role_id);
    Out->set_channel(static_cast<idlepb::ChatMessageChannel>(channel));
    Out->set_text(TCHAR_TO_UTF8(*text));
    Out->set_type(static_cast<idlepb::ChatMessageType>(type));    
}

void FPbSendChatMessageReq::Reset()
{
    role_id = int64();
    channel = EPbChatMessageChannel();
    text = FString();
    type = EPbChatMessageType();    
}

void FPbSendChatMessageReq::operator=(const idlepb::SendChatMessageReq& Right)
{
    this->FromPb(Right);
}

bool FPbSendChatMessageReq::operator==(const FPbSendChatMessageReq& Right) const
{
    if (this->role_id != Right.role_id)
        return false;
    if (this->channel != Right.channel)
        return false;
    if (this->text != Right.text)
        return false;
    if (this->type != Right.type)
        return false;
    return true;
}

bool FPbSendChatMessageReq::operator!=(const FPbSendChatMessageReq& Right) const
{
    return !operator==(Right);
}

FPbSendChatMessageAck::FPbSendChatMessageAck()
{
    Reset();        
}

FPbSendChatMessageAck::FPbSendChatMessageAck(const idlepb::SendChatMessageAck& Right)
{
    this->FromPb(Right);
}

void FPbSendChatMessageAck::FromPb(const idlepb::SendChatMessageAck& Right)
{
    ok = Right.ok();
}

void FPbSendChatMessageAck::ToPb(idlepb::SendChatMessageAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbSendChatMessageAck::Reset()
{
    ok = bool();    
}

void FPbSendChatMessageAck::operator=(const idlepb::SendChatMessageAck& Right)
{
    this->FromPb(Right);
}

bool FPbSendChatMessageAck::operator==(const FPbSendChatMessageAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbSendChatMessageAck::operator!=(const FPbSendChatMessageAck& Right) const
{
    return !operator==(Right);
}

FPbGetChatRecordReq::FPbGetChatRecordReq()
{
    Reset();        
}

FPbGetChatRecordReq::FPbGetChatRecordReq(const idlepb::GetChatRecordReq& Right)
{
    this->FromPb(Right);
}

void FPbGetChatRecordReq::FromPb(const idlepb::GetChatRecordReq& Right)
{
}

void FPbGetChatRecordReq::ToPb(idlepb::GetChatRecordReq* Out) const
{    
}

void FPbGetChatRecordReq::Reset()
{    
}

void FPbGetChatRecordReq::operator=(const idlepb::GetChatRecordReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetChatRecordReq::operator==(const FPbGetChatRecordReq& Right) const
{
    return true;
}

bool FPbGetChatRecordReq::operator!=(const FPbGetChatRecordReq& Right) const
{
    return !operator==(Right);
}

FPbGetChatRecordAck::FPbGetChatRecordAck()
{
    Reset();        
}

FPbGetChatRecordAck::FPbGetChatRecordAck(const idlepb::GetChatRecordAck& Right)
{
    this->FromPb(Right);
}

void FPbGetChatRecordAck::FromPb(const idlepb::GetChatRecordAck& Right)
{
    public_chat_record = Right.public_chat_record();
    private_chat_record = Right.private_chat_record();
    sept_record.Empty();
    for (const auto& Elem : Right.sept_record())
    {
        sept_record.Emplace(Elem);
    }
}

void FPbGetChatRecordAck::ToPb(idlepb::GetChatRecordAck* Out) const
{
    public_chat_record.ToPb(Out->mutable_public_chat_record());
    private_chat_record.ToPb(Out->mutable_private_chat_record());
    for (const auto& Elem : sept_record)
    {
        Elem.ToPb(Out->add_sept_record());    
    }    
}

void FPbGetChatRecordAck::Reset()
{
    public_chat_record = FPbChatData();
    private_chat_record = FPbRolePrivateChatRecord();
    sept_record = TArray<FPbChatMessage>();    
}

void FPbGetChatRecordAck::operator=(const idlepb::GetChatRecordAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetChatRecordAck::operator==(const FPbGetChatRecordAck& Right) const
{
    if (this->public_chat_record != Right.public_chat_record)
        return false;
    if (this->private_chat_record != Right.private_chat_record)
        return false;
    if (this->sept_record != Right.sept_record)
        return false;
    return true;
}

bool FPbGetChatRecordAck::operator!=(const FPbGetChatRecordAck& Right) const
{
    return !operator==(Right);
}

FPbDeletePrivateChatRecordReq::FPbDeletePrivateChatRecordReq()
{
    Reset();        
}

FPbDeletePrivateChatRecordReq::FPbDeletePrivateChatRecordReq(const idlepb::DeletePrivateChatRecordReq& Right)
{
    this->FromPb(Right);
}

void FPbDeletePrivateChatRecordReq::FromPb(const idlepb::DeletePrivateChatRecordReq& Right)
{
    role_id = Right.role_id();
}

void FPbDeletePrivateChatRecordReq::ToPb(idlepb::DeletePrivateChatRecordReq* Out) const
{
    Out->set_role_id(role_id);    
}

void FPbDeletePrivateChatRecordReq::Reset()
{
    role_id = int64();    
}

void FPbDeletePrivateChatRecordReq::operator=(const idlepb::DeletePrivateChatRecordReq& Right)
{
    this->FromPb(Right);
}

bool FPbDeletePrivateChatRecordReq::operator==(const FPbDeletePrivateChatRecordReq& Right) const
{
    if (this->role_id != Right.role_id)
        return false;
    return true;
}

bool FPbDeletePrivateChatRecordReq::operator!=(const FPbDeletePrivateChatRecordReq& Right) const
{
    return !operator==(Right);
}

FPbDeletePrivateChatRecordAck::FPbDeletePrivateChatRecordAck()
{
    Reset();        
}

FPbDeletePrivateChatRecordAck::FPbDeletePrivateChatRecordAck(const idlepb::DeletePrivateChatRecordAck& Right)
{
    this->FromPb(Right);
}

void FPbDeletePrivateChatRecordAck::FromPb(const idlepb::DeletePrivateChatRecordAck& Right)
{
    ok = Right.ok();
}

void FPbDeletePrivateChatRecordAck::ToPb(idlepb::DeletePrivateChatRecordAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbDeletePrivateChatRecordAck::Reset()
{
    ok = bool();    
}

void FPbDeletePrivateChatRecordAck::operator=(const idlepb::DeletePrivateChatRecordAck& Right)
{
    this->FromPb(Right);
}

bool FPbDeletePrivateChatRecordAck::operator==(const FPbDeletePrivateChatRecordAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbDeletePrivateChatRecordAck::operator!=(const FPbDeletePrivateChatRecordAck& Right) const
{
    return !operator==(Right);
}

FPbClearChatUnreadNumReq::FPbClearChatUnreadNumReq()
{
    Reset();        
}

FPbClearChatUnreadNumReq::FPbClearChatUnreadNumReq(const idlepb::ClearChatUnreadNumReq& Right)
{
    this->FromPb(Right);
}

void FPbClearChatUnreadNumReq::FromPb(const idlepb::ClearChatUnreadNumReq& Right)
{
    role_id = Right.role_id();
}

void FPbClearChatUnreadNumReq::ToPb(idlepb::ClearChatUnreadNumReq* Out) const
{
    Out->set_role_id(role_id);    
}

void FPbClearChatUnreadNumReq::Reset()
{
    role_id = int64();    
}

void FPbClearChatUnreadNumReq::operator=(const idlepb::ClearChatUnreadNumReq& Right)
{
    this->FromPb(Right);
}

bool FPbClearChatUnreadNumReq::operator==(const FPbClearChatUnreadNumReq& Right) const
{
    if (this->role_id != Right.role_id)
        return false;
    return true;
}

bool FPbClearChatUnreadNumReq::operator!=(const FPbClearChatUnreadNumReq& Right) const
{
    return !operator==(Right);
}

FPbClearChatUnreadNumAck::FPbClearChatUnreadNumAck()
{
    Reset();        
}

FPbClearChatUnreadNumAck::FPbClearChatUnreadNumAck(const idlepb::ClearChatUnreadNumAck& Right)
{
    this->FromPb(Right);
}

void FPbClearChatUnreadNumAck::FromPb(const idlepb::ClearChatUnreadNumAck& Right)
{
}

void FPbClearChatUnreadNumAck::ToPb(idlepb::ClearChatUnreadNumAck* Out) const
{    
}

void FPbClearChatUnreadNumAck::Reset()
{    
}

void FPbClearChatUnreadNumAck::operator=(const idlepb::ClearChatUnreadNumAck& Right)
{
    this->FromPb(Right);
}

bool FPbClearChatUnreadNumAck::operator==(const FPbClearChatUnreadNumAck& Right) const
{
    return true;
}

bool FPbClearChatUnreadNumAck::operator!=(const FPbClearChatUnreadNumAck& Right) const
{
    return !operator==(Right);
}

FPbGetRoleInfoCacheReq::FPbGetRoleInfoCacheReq()
{
    Reset();        
}

FPbGetRoleInfoCacheReq::FPbGetRoleInfoCacheReq(const idlepb::GetRoleInfoCacheReq& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleInfoCacheReq::FromPb(const idlepb::GetRoleInfoCacheReq& Right)
{
    role_ids.Empty();
    for (const auto& Elem : Right.role_ids())
    {
        role_ids.Emplace(Elem);
    }
}

void FPbGetRoleInfoCacheReq::ToPb(idlepb::GetRoleInfoCacheReq* Out) const
{
    for (const auto& Elem : role_ids)
    {
        Out->add_role_ids(Elem);    
    }    
}

void FPbGetRoleInfoCacheReq::Reset()
{
    role_ids = TArray<int64>();    
}

void FPbGetRoleInfoCacheReq::operator=(const idlepb::GetRoleInfoCacheReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleInfoCacheReq::operator==(const FPbGetRoleInfoCacheReq& Right) const
{
    if (this->role_ids != Right.role_ids)
        return false;
    return true;
}

bool FPbGetRoleInfoCacheReq::operator!=(const FPbGetRoleInfoCacheReq& Right) const
{
    return !operator==(Right);
}

FPbGetRoleInfoCacheAck::FPbGetRoleInfoCacheAck()
{
    Reset();        
}

FPbGetRoleInfoCacheAck::FPbGetRoleInfoCacheAck(const idlepb::GetRoleInfoCacheAck& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleInfoCacheAck::FromPb(const idlepb::GetRoleInfoCacheAck& Right)
{
    role_infos.Empty();
    for (const auto& Elem : Right.role_infos())
    {
        role_infos.Emplace(Elem);
    }
}

void FPbGetRoleInfoCacheAck::ToPb(idlepb::GetRoleInfoCacheAck* Out) const
{
    for (const auto& Elem : role_infos)
    {
        Elem.ToPb(Out->add_role_infos());    
    }    
}

void FPbGetRoleInfoCacheAck::Reset()
{
    role_infos = TArray<FPbSimpleRoleInfo>();    
}

void FPbGetRoleInfoCacheAck::operator=(const idlepb::GetRoleInfoCacheAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleInfoCacheAck::operator==(const FPbGetRoleInfoCacheAck& Right) const
{
    if (this->role_infos != Right.role_infos)
        return false;
    return true;
}

bool FPbGetRoleInfoCacheAck::operator!=(const FPbGetRoleInfoCacheAck& Right) const
{
    return !operator==(Right);
}

FPbForgeRefineStartReq::FPbForgeRefineStartReq()
{
    Reset();        
}

FPbForgeRefineStartReq::FPbForgeRefineStartReq(const idlepb::ForgeRefineStartReq& Right)
{
    this->FromPb(Right);
}

void FPbForgeRefineStartReq::FromPb(const idlepb::ForgeRefineStartReq& Right)
{
    recipe_id = Right.recipe_id();
    material_id = Right.material_id();
    ext_material_id = Right.ext_material_id();
    target_num = Right.target_num();
    auto_sell_poor = Right.auto_sell_poor();
    auto_sell_middle = Right.auto_sell_middle();
}

void FPbForgeRefineStartReq::ToPb(idlepb::ForgeRefineStartReq* Out) const
{
    Out->set_recipe_id(recipe_id);
    Out->set_material_id(material_id);
    Out->set_ext_material_id(ext_material_id);
    Out->set_target_num(target_num);
    Out->set_auto_sell_poor(auto_sell_poor);
    Out->set_auto_sell_middle(auto_sell_middle);    
}

void FPbForgeRefineStartReq::Reset()
{
    recipe_id = int32();
    material_id = int32();
    ext_material_id = int32();
    target_num = int32();
    auto_sell_poor = bool();
    auto_sell_middle = bool();    
}

void FPbForgeRefineStartReq::operator=(const idlepb::ForgeRefineStartReq& Right)
{
    this->FromPb(Right);
}

bool FPbForgeRefineStartReq::operator==(const FPbForgeRefineStartReq& Right) const
{
    if (this->recipe_id != Right.recipe_id)
        return false;
    if (this->material_id != Right.material_id)
        return false;
    if (this->ext_material_id != Right.ext_material_id)
        return false;
    if (this->target_num != Right.target_num)
        return false;
    if (this->auto_sell_poor != Right.auto_sell_poor)
        return false;
    if (this->auto_sell_middle != Right.auto_sell_middle)
        return false;
    return true;
}

bool FPbForgeRefineStartReq::operator!=(const FPbForgeRefineStartReq& Right) const
{
    return !operator==(Right);
}

FPbForgeRefineStartAck::FPbForgeRefineStartAck()
{
    Reset();        
}

FPbForgeRefineStartAck::FPbForgeRefineStartAck(const idlepb::ForgeRefineStartAck& Right)
{
    this->FromPb(Right);
}

void FPbForgeRefineStartAck::FromPb(const idlepb::ForgeRefineStartAck& Right)
{
    ok = Right.ok();
}

void FPbForgeRefineStartAck::ToPb(idlepb::ForgeRefineStartAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbForgeRefineStartAck::Reset()
{
    ok = bool();    
}

void FPbForgeRefineStartAck::operator=(const idlepb::ForgeRefineStartAck& Right)
{
    this->FromPb(Right);
}

bool FPbForgeRefineStartAck::operator==(const FPbForgeRefineStartAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbForgeRefineStartAck::operator!=(const FPbForgeRefineStartAck& Right) const
{
    return !operator==(Right);
}

FPbForgeRefineCancelReq::FPbForgeRefineCancelReq()
{
    Reset();        
}

FPbForgeRefineCancelReq::FPbForgeRefineCancelReq(const idlepb::ForgeRefineCancelReq& Right)
{
    this->FromPb(Right);
}

void FPbForgeRefineCancelReq::FromPb(const idlepb::ForgeRefineCancelReq& Right)
{
}

void FPbForgeRefineCancelReq::ToPb(idlepb::ForgeRefineCancelReq* Out) const
{    
}

void FPbForgeRefineCancelReq::Reset()
{    
}

void FPbForgeRefineCancelReq::operator=(const idlepb::ForgeRefineCancelReq& Right)
{
    this->FromPb(Right);
}

bool FPbForgeRefineCancelReq::operator==(const FPbForgeRefineCancelReq& Right) const
{
    return true;
}

bool FPbForgeRefineCancelReq::operator!=(const FPbForgeRefineCancelReq& Right) const
{
    return !operator==(Right);
}

FPbForgeRefineCancelAck::FPbForgeRefineCancelAck()
{
    Reset();        
}

FPbForgeRefineCancelAck::FPbForgeRefineCancelAck(const idlepb::ForgeRefineCancelAck& Right)
{
    this->FromPb(Right);
}

void FPbForgeRefineCancelAck::FromPb(const idlepb::ForgeRefineCancelAck& Right)
{
    ok = Right.ok();
}

void FPbForgeRefineCancelAck::ToPb(idlepb::ForgeRefineCancelAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbForgeRefineCancelAck::Reset()
{
    ok = bool();    
}

void FPbForgeRefineCancelAck::operator=(const idlepb::ForgeRefineCancelAck& Right)
{
    this->FromPb(Right);
}

bool FPbForgeRefineCancelAck::operator==(const FPbForgeRefineCancelAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbForgeRefineCancelAck::operator!=(const FPbForgeRefineCancelAck& Right) const
{
    return !operator==(Right);
}

FPbForgeRefineExtractReq::FPbForgeRefineExtractReq()
{
    Reset();        
}

FPbForgeRefineExtractReq::FPbForgeRefineExtractReq(const idlepb::ForgeRefineExtractReq& Right)
{
    this->FromPb(Right);
}

void FPbForgeRefineExtractReq::FromPb(const idlepb::ForgeRefineExtractReq& Right)
{
}

void FPbForgeRefineExtractReq::ToPb(idlepb::ForgeRefineExtractReq* Out) const
{    
}

void FPbForgeRefineExtractReq::Reset()
{    
}

void FPbForgeRefineExtractReq::operator=(const idlepb::ForgeRefineExtractReq& Right)
{
    this->FromPb(Right);
}

bool FPbForgeRefineExtractReq::operator==(const FPbForgeRefineExtractReq& Right) const
{
    return true;
}

bool FPbForgeRefineExtractReq::operator!=(const FPbForgeRefineExtractReq& Right) const
{
    return !operator==(Right);
}

FPbForgeRefineExtractAck::FPbForgeRefineExtractAck()
{
    Reset();        
}

FPbForgeRefineExtractAck::FPbForgeRefineExtractAck(const idlepb::ForgeRefineExtractAck& Right)
{
    this->FromPb(Right);
}

void FPbForgeRefineExtractAck::FromPb(const idlepb::ForgeRefineExtractAck& Right)
{
    ok = Right.ok();
    items.Empty();
    for (const auto& Elem : Right.items())
    {
        items.Emplace(Elem);
    }
}

void FPbForgeRefineExtractAck::ToPb(idlepb::ForgeRefineExtractAck* Out) const
{
    Out->set_ok(ok);
    for (const auto& Elem : items)
    {
        Out->add_items(Elem);    
    }    
}

void FPbForgeRefineExtractAck::Reset()
{
    ok = bool();
    items = TArray<int64>();    
}

void FPbForgeRefineExtractAck::operator=(const idlepb::ForgeRefineExtractAck& Right)
{
    this->FromPb(Right);
}

bool FPbForgeRefineExtractAck::operator==(const FPbForgeRefineExtractAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    if (this->items != Right.items)
        return false;
    return true;
}

bool FPbForgeRefineExtractAck::operator!=(const FPbForgeRefineExtractAck& Right) const
{
    return !operator==(Right);
}

FPbGetForgeLostEquipmentDataReq::FPbGetForgeLostEquipmentDataReq()
{
    Reset();        
}

FPbGetForgeLostEquipmentDataReq::FPbGetForgeLostEquipmentDataReq(const idlepb::GetForgeLostEquipmentDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetForgeLostEquipmentDataReq::FromPb(const idlepb::GetForgeLostEquipmentDataReq& Right)
{
}

void FPbGetForgeLostEquipmentDataReq::ToPb(idlepb::GetForgeLostEquipmentDataReq* Out) const
{    
}

void FPbGetForgeLostEquipmentDataReq::Reset()
{    
}

void FPbGetForgeLostEquipmentDataReq::operator=(const idlepb::GetForgeLostEquipmentDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetForgeLostEquipmentDataReq::operator==(const FPbGetForgeLostEquipmentDataReq& Right) const
{
    return true;
}

bool FPbGetForgeLostEquipmentDataReq::operator!=(const FPbGetForgeLostEquipmentDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetForgeLostEquipmentDataAck::FPbGetForgeLostEquipmentDataAck()
{
    Reset();        
}

FPbGetForgeLostEquipmentDataAck::FPbGetForgeLostEquipmentDataAck(const idlepb::GetForgeLostEquipmentDataAck& Right)
{
    this->FromPb(Right);
}

void FPbGetForgeLostEquipmentDataAck::FromPb(const idlepb::GetForgeLostEquipmentDataAck& Right)
{
    data.Empty();
    for (const auto& Elem : Right.data())
    {
        data.Emplace(Elem);
    }
}

void FPbGetForgeLostEquipmentDataAck::ToPb(idlepb::GetForgeLostEquipmentDataAck* Out) const
{
    for (const auto& Elem : data)
    {
        Elem.ToPb(Out->add_data());    
    }    
}

void FPbGetForgeLostEquipmentDataAck::Reset()
{
    data = TArray<FPbLostEquipmentData>();    
}

void FPbGetForgeLostEquipmentDataAck::operator=(const idlepb::GetForgeLostEquipmentDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetForgeLostEquipmentDataAck::operator==(const FPbGetForgeLostEquipmentDataAck& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbGetForgeLostEquipmentDataAck::operator!=(const FPbGetForgeLostEquipmentDataAck& Right) const
{
    return !operator==(Right);
}

FPbForgeDestroyReq::FPbForgeDestroyReq()
{
    Reset();        
}

FPbForgeDestroyReq::FPbForgeDestroyReq(const idlepb::ForgeDestroyReq& Right)
{
    this->FromPb(Right);
}

void FPbForgeDestroyReq::FromPb(const idlepb::ForgeDestroyReq& Right)
{
    uid = Right.uid();
}

void FPbForgeDestroyReq::ToPb(idlepb::ForgeDestroyReq* Out) const
{
    Out->set_uid(uid);    
}

void FPbForgeDestroyReq::Reset()
{
    uid = int64();    
}

void FPbForgeDestroyReq::operator=(const idlepb::ForgeDestroyReq& Right)
{
    this->FromPb(Right);
}

bool FPbForgeDestroyReq::operator==(const FPbForgeDestroyReq& Right) const
{
    if (this->uid != Right.uid)
        return false;
    return true;
}

bool FPbForgeDestroyReq::operator!=(const FPbForgeDestroyReq& Right) const
{
    return !operator==(Right);
}

FPbForgeDestroyAck::FPbForgeDestroyAck()
{
    Reset();        
}

FPbForgeDestroyAck::FPbForgeDestroyAck(const idlepb::ForgeDestroyAck& Right)
{
    this->FromPb(Right);
}

void FPbForgeDestroyAck::FromPb(const idlepb::ForgeDestroyAck& Right)
{
    ok = Right.ok();
}

void FPbForgeDestroyAck::ToPb(idlepb::ForgeDestroyAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbForgeDestroyAck::Reset()
{
    ok = bool();    
}

void FPbForgeDestroyAck::operator=(const idlepb::ForgeDestroyAck& Right)
{
    this->FromPb(Right);
}

bool FPbForgeDestroyAck::operator==(const FPbForgeDestroyAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbForgeDestroyAck::operator!=(const FPbForgeDestroyAck& Right) const
{
    return !operator==(Right);
}

FPbForgeFindBackReq::FPbForgeFindBackReq()
{
    Reset();        
}

FPbForgeFindBackReq::FPbForgeFindBackReq(const idlepb::ForgeFindBackReq& Right)
{
    this->FromPb(Right);
}

void FPbForgeFindBackReq::FromPb(const idlepb::ForgeFindBackReq& Right)
{
    uid = Right.uid();
}

void FPbForgeFindBackReq::ToPb(idlepb::ForgeFindBackReq* Out) const
{
    Out->set_uid(uid);    
}

void FPbForgeFindBackReq::Reset()
{
    uid = int32();    
}

void FPbForgeFindBackReq::operator=(const idlepb::ForgeFindBackReq& Right)
{
    this->FromPb(Right);
}

bool FPbForgeFindBackReq::operator==(const FPbForgeFindBackReq& Right) const
{
    if (this->uid != Right.uid)
        return false;
    return true;
}

bool FPbForgeFindBackReq::operator!=(const FPbForgeFindBackReq& Right) const
{
    return !operator==(Right);
}

FPbForgeFindBackAck::FPbForgeFindBackAck()
{
    Reset();        
}

FPbForgeFindBackAck::FPbForgeFindBackAck(const idlepb::ForgeFindBackAck& Right)
{
    this->FromPb(Right);
}

void FPbForgeFindBackAck::FromPb(const idlepb::ForgeFindBackAck& Right)
{
    ok = Right.ok();
}

void FPbForgeFindBackAck::ToPb(idlepb::ForgeFindBackAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbForgeFindBackAck::Reset()
{
    ok = bool();    
}

void FPbForgeFindBackAck::operator=(const idlepb::ForgeFindBackAck& Right)
{
    this->FromPb(Right);
}

bool FPbForgeFindBackAck::operator==(const FPbForgeFindBackAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbForgeFindBackAck::operator!=(const FPbForgeFindBackAck& Right) const
{
    return !operator==(Right);
}

FPbRequestPillElixirDataReq::FPbRequestPillElixirDataReq()
{
    Reset();        
}

FPbRequestPillElixirDataReq::FPbRequestPillElixirDataReq(const idlepb::RequestPillElixirDataReq& Right)
{
    this->FromPb(Right);
}

void FPbRequestPillElixirDataReq::FromPb(const idlepb::RequestPillElixirDataReq& Right)
{
}

void FPbRequestPillElixirDataReq::ToPb(idlepb::RequestPillElixirDataReq* Out) const
{    
}

void FPbRequestPillElixirDataReq::Reset()
{    
}

void FPbRequestPillElixirDataReq::operator=(const idlepb::RequestPillElixirDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbRequestPillElixirDataReq::operator==(const FPbRequestPillElixirDataReq& Right) const
{
    return true;
}

bool FPbRequestPillElixirDataReq::operator!=(const FPbRequestPillElixirDataReq& Right) const
{
    return !operator==(Right);
}

FPbRequestPillElixirDataAck::FPbRequestPillElixirDataAck()
{
    Reset();        
}

FPbRequestPillElixirDataAck::FPbRequestPillElixirDataAck(const idlepb::RequestPillElixirDataAck& Right)
{
    this->FromPb(Right);
}

void FPbRequestPillElixirDataAck::FromPb(const idlepb::RequestPillElixirDataAck& Right)
{
    data = Right.data();
}

void FPbRequestPillElixirDataAck::ToPb(idlepb::RequestPillElixirDataAck* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbRequestPillElixirDataAck::Reset()
{
    data = FPbRolePillElixirData();    
}

void FPbRequestPillElixirDataAck::operator=(const idlepb::RequestPillElixirDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbRequestPillElixirDataAck::operator==(const FPbRequestPillElixirDataAck& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbRequestPillElixirDataAck::operator!=(const FPbRequestPillElixirDataAck& Right) const
{
    return !operator==(Right);
}

FPbGetOnePillElixirDataReq::FPbGetOnePillElixirDataReq()
{
    Reset();        
}

FPbGetOnePillElixirDataReq::FPbGetOnePillElixirDataReq(const idlepb::GetOnePillElixirDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetOnePillElixirDataReq::FromPb(const idlepb::GetOnePillElixirDataReq& Right)
{
    item_cfg_id = Right.item_cfg_id();
}

void FPbGetOnePillElixirDataReq::ToPb(idlepb::GetOnePillElixirDataReq* Out) const
{
    Out->set_item_cfg_id(item_cfg_id);    
}

void FPbGetOnePillElixirDataReq::Reset()
{
    item_cfg_id = int32();    
}

void FPbGetOnePillElixirDataReq::operator=(const idlepb::GetOnePillElixirDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetOnePillElixirDataReq::operator==(const FPbGetOnePillElixirDataReq& Right) const
{
    if (this->item_cfg_id != Right.item_cfg_id)
        return false;
    return true;
}

bool FPbGetOnePillElixirDataReq::operator!=(const FPbGetOnePillElixirDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetOnePillElixirDataAck::FPbGetOnePillElixirDataAck()
{
    Reset();        
}

FPbGetOnePillElixirDataAck::FPbGetOnePillElixirDataAck(const idlepb::GetOnePillElixirDataAck& Right)
{
    this->FromPb(Right);
}

void FPbGetOnePillElixirDataAck::FromPb(const idlepb::GetOnePillElixirDataAck& Right)
{
    data = Right.data();
}

void FPbGetOnePillElixirDataAck::ToPb(idlepb::GetOnePillElixirDataAck* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbGetOnePillElixirDataAck::Reset()
{
    data = FPbPillElixirData();    
}

void FPbGetOnePillElixirDataAck::operator=(const idlepb::GetOnePillElixirDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetOnePillElixirDataAck::operator==(const FPbGetOnePillElixirDataAck& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbGetOnePillElixirDataAck::operator!=(const FPbGetOnePillElixirDataAck& Right) const
{
    return !operator==(Right);
}

FPbRequestModifyPillElixirFilterReq::FPbRequestModifyPillElixirFilterReq()
{
    Reset();        
}

FPbRequestModifyPillElixirFilterReq::FPbRequestModifyPillElixirFilterReq(const idlepb::RequestModifyPillElixirFilterReq& Right)
{
    this->FromPb(Right);
}

void FPbRequestModifyPillElixirFilterReq::FromPb(const idlepb::RequestModifyPillElixirFilterReq& Right)
{
    limit_double = Right.limit_double();
    limit_exp = Right.limit_exp();
    limit_property = Right.limit_property();
}

void FPbRequestModifyPillElixirFilterReq::ToPb(idlepb::RequestModifyPillElixirFilterReq* Out) const
{
    Out->set_limit_double(limit_double);
    Out->set_limit_exp(limit_exp);
    Out->set_limit_property(limit_property);    
}

void FPbRequestModifyPillElixirFilterReq::Reset()
{
    limit_double = int32();
    limit_exp = int32();
    limit_property = int32();    
}

void FPbRequestModifyPillElixirFilterReq::operator=(const idlepb::RequestModifyPillElixirFilterReq& Right)
{
    this->FromPb(Right);
}

bool FPbRequestModifyPillElixirFilterReq::operator==(const FPbRequestModifyPillElixirFilterReq& Right) const
{
    if (this->limit_double != Right.limit_double)
        return false;
    if (this->limit_exp != Right.limit_exp)
        return false;
    if (this->limit_property != Right.limit_property)
        return false;
    return true;
}

bool FPbRequestModifyPillElixirFilterReq::operator!=(const FPbRequestModifyPillElixirFilterReq& Right) const
{
    return !operator==(Right);
}

FPbRequestModifyPillElixirFilterAck::FPbRequestModifyPillElixirFilterAck()
{
    Reset();        
}

FPbRequestModifyPillElixirFilterAck::FPbRequestModifyPillElixirFilterAck(const idlepb::RequestModifyPillElixirFilterAck& Right)
{
    this->FromPb(Right);
}

void FPbRequestModifyPillElixirFilterAck::FromPb(const idlepb::RequestModifyPillElixirFilterAck& Right)
{
    ok = Right.ok();
}

void FPbRequestModifyPillElixirFilterAck::ToPb(idlepb::RequestModifyPillElixirFilterAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbRequestModifyPillElixirFilterAck::Reset()
{
    ok = bool();    
}

void FPbRequestModifyPillElixirFilterAck::operator=(const idlepb::RequestModifyPillElixirFilterAck& Right)
{
    this->FromPb(Right);
}

bool FPbRequestModifyPillElixirFilterAck::operator==(const FPbRequestModifyPillElixirFilterAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbRequestModifyPillElixirFilterAck::operator!=(const FPbRequestModifyPillElixirFilterAck& Right) const
{
    return !operator==(Right);
}

FPbUsePillElixirReport::FPbUsePillElixirReport()
{
    Reset();        
}

FPbUsePillElixirReport::FPbUsePillElixirReport(const idlepb::UsePillElixirReport& Right)
{
    this->FromPb(Right);
}

void FPbUsePillElixirReport::FromPb(const idlepb::UsePillElixirReport& Right)
{
    item_id = Right.item_id();
    num = Right.num();
    property_num = Right.property_num();
}

void FPbUsePillElixirReport::ToPb(idlepb::UsePillElixirReport* Out) const
{
    Out->set_item_id(item_id);
    Out->set_num(num);
    Out->set_property_num(property_num);    
}

void FPbUsePillElixirReport::Reset()
{
    item_id = int32();
    num = int32();
    property_num = float();    
}

void FPbUsePillElixirReport::operator=(const idlepb::UsePillElixirReport& Right)
{
    this->FromPb(Right);
}

bool FPbUsePillElixirReport::operator==(const FPbUsePillElixirReport& Right) const
{
    if (this->item_id != Right.item_id)
        return false;
    if (this->num != Right.num)
        return false;
    if (this->property_num != Right.property_num)
        return false;
    return true;
}

bool FPbUsePillElixirReport::operator!=(const FPbUsePillElixirReport& Right) const
{
    return !operator==(Right);
}

FPbUsePillElixirReq::FPbUsePillElixirReq()
{
    Reset();        
}

FPbUsePillElixirReq::FPbUsePillElixirReq(const idlepb::UsePillElixirReq& Right)
{
    this->FromPb(Right);
}

void FPbUsePillElixirReq::FromPb(const idlepb::UsePillElixirReq& Right)
{
    item_id = Right.item_id();
}

void FPbUsePillElixirReq::ToPb(idlepb::UsePillElixirReq* Out) const
{
    Out->set_item_id(item_id);    
}

void FPbUsePillElixirReq::Reset()
{
    item_id = int32();    
}

void FPbUsePillElixirReq::operator=(const idlepb::UsePillElixirReq& Right)
{
    this->FromPb(Right);
}

bool FPbUsePillElixirReq::operator==(const FPbUsePillElixirReq& Right) const
{
    if (this->item_id != Right.item_id)
        return false;
    return true;
}

bool FPbUsePillElixirReq::operator!=(const FPbUsePillElixirReq& Right) const
{
    return !operator==(Right);
}

FPbUsePillElixirAck::FPbUsePillElixirAck()
{
    Reset();        
}

FPbUsePillElixirAck::FPbUsePillElixirAck(const idlepb::UsePillElixirAck& Right)
{
    this->FromPb(Right);
}

void FPbUsePillElixirAck::FromPb(const idlepb::UsePillElixirAck& Right)
{
    ok = Right.ok();
}

void FPbUsePillElixirAck::ToPb(idlepb::UsePillElixirAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbUsePillElixirAck::Reset()
{
    ok = bool();    
}

void FPbUsePillElixirAck::operator=(const idlepb::UsePillElixirAck& Right)
{
    this->FromPb(Right);
}

bool FPbUsePillElixirAck::operator==(const FPbUsePillElixirAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbUsePillElixirAck::operator!=(const FPbUsePillElixirAck& Right) const
{
    return !operator==(Right);
}

FPbOneClickUsePillElixirReq::FPbOneClickUsePillElixirReq()
{
    Reset();        
}

FPbOneClickUsePillElixirReq::FPbOneClickUsePillElixirReq(const idlepb::OneClickUsePillElixirReq& Right)
{
    this->FromPb(Right);
}

void FPbOneClickUsePillElixirReq::FromPb(const idlepb::OneClickUsePillElixirReq& Right)
{
}

void FPbOneClickUsePillElixirReq::ToPb(idlepb::OneClickUsePillElixirReq* Out) const
{    
}

void FPbOneClickUsePillElixirReq::Reset()
{    
}

void FPbOneClickUsePillElixirReq::operator=(const idlepb::OneClickUsePillElixirReq& Right)
{
    this->FromPb(Right);
}

bool FPbOneClickUsePillElixirReq::operator==(const FPbOneClickUsePillElixirReq& Right) const
{
    return true;
}

bool FPbOneClickUsePillElixirReq::operator!=(const FPbOneClickUsePillElixirReq& Right) const
{
    return !operator==(Right);
}

FPbOneClickUsePillElixirAck::FPbOneClickUsePillElixirAck()
{
    Reset();        
}

FPbOneClickUsePillElixirAck::FPbOneClickUsePillElixirAck(const idlepb::OneClickUsePillElixirAck& Right)
{
    this->FromPb(Right);
}

void FPbOneClickUsePillElixirAck::FromPb(const idlepb::OneClickUsePillElixirAck& Right)
{
    report.Empty();
    for (const auto& Elem : Right.report())
    {
        report.Emplace(Elem);
    }
}

void FPbOneClickUsePillElixirAck::ToPb(idlepb::OneClickUsePillElixirAck* Out) const
{
    for (const auto& Elem : report)
    {
        Elem.ToPb(Out->add_report());    
    }    
}

void FPbOneClickUsePillElixirAck::Reset()
{
    report = TArray<FPbUsePillElixirReport>();    
}

void FPbOneClickUsePillElixirAck::operator=(const idlepb::OneClickUsePillElixirAck& Right)
{
    this->FromPb(Right);
}

bool FPbOneClickUsePillElixirAck::operator==(const FPbOneClickUsePillElixirAck& Right) const
{
    if (this->report != Right.report)
        return false;
    return true;
}

bool FPbOneClickUsePillElixirAck::operator!=(const FPbOneClickUsePillElixirAck& Right) const
{
    return !operator==(Right);
}

FPbTradePillElixirReq::FPbTradePillElixirReq()
{
    Reset();        
}

FPbTradePillElixirReq::FPbTradePillElixirReq(const idlepb::TradePillElixirReq& Right)
{
    this->FromPb(Right);
}

void FPbTradePillElixirReq::FromPb(const idlepb::TradePillElixirReq& Right)
{
    item_id = Right.item_id();
    num = Right.num();
}

void FPbTradePillElixirReq::ToPb(idlepb::TradePillElixirReq* Out) const
{
    Out->set_item_id(item_id);
    Out->set_num(num);    
}

void FPbTradePillElixirReq::Reset()
{
    item_id = int32();
    num = int32();    
}

void FPbTradePillElixirReq::operator=(const idlepb::TradePillElixirReq& Right)
{
    this->FromPb(Right);
}

bool FPbTradePillElixirReq::operator==(const FPbTradePillElixirReq& Right) const
{
    if (this->item_id != Right.item_id)
        return false;
    if (this->num != Right.num)
        return false;
    return true;
}

bool FPbTradePillElixirReq::operator!=(const FPbTradePillElixirReq& Right) const
{
    return !operator==(Right);
}

FPbTradePillElixirAck::FPbTradePillElixirAck()
{
    Reset();        
}

FPbTradePillElixirAck::FPbTradePillElixirAck(const idlepb::TradePillElixirAck& Right)
{
    this->FromPb(Right);
}

void FPbTradePillElixirAck::FromPb(const idlepb::TradePillElixirAck& Right)
{
    ok = Right.ok();
}

void FPbTradePillElixirAck::ToPb(idlepb::TradePillElixirAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbTradePillElixirAck::Reset()
{
    ok = bool();    
}

void FPbTradePillElixirAck::operator=(const idlepb::TradePillElixirAck& Right)
{
    this->FromPb(Right);
}

bool FPbTradePillElixirAck::operator==(const FPbTradePillElixirAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbTradePillElixirAck::operator!=(const FPbTradePillElixirAck& Right) const
{
    return !operator==(Right);
}

FPbNotifyAutoModeStatus::FPbNotifyAutoModeStatus()
{
    Reset();        
}

FPbNotifyAutoModeStatus::FPbNotifyAutoModeStatus(const idlepb::NotifyAutoModeStatus& Right)
{
    this->FromPb(Right);
}

void FPbNotifyAutoModeStatus::FromPb(const idlepb::NotifyAutoModeStatus& Right)
{
    enable = Right.enable();
}

void FPbNotifyAutoModeStatus::ToPb(idlepb::NotifyAutoModeStatus* Out) const
{
    Out->set_enable(enable);    
}

void FPbNotifyAutoModeStatus::Reset()
{
    enable = bool();    
}

void FPbNotifyAutoModeStatus::operator=(const idlepb::NotifyAutoModeStatus& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyAutoModeStatus::operator==(const FPbNotifyAutoModeStatus& Right) const
{
    if (this->enable != Right.enable)
        return false;
    return true;
}

bool FPbNotifyAutoModeStatus::operator!=(const FPbNotifyAutoModeStatus& Right) const
{
    return !operator==(Right);
}

FPbSetAutoMode::FPbSetAutoMode()
{
    Reset();        
}

FPbSetAutoMode::FPbSetAutoMode(const idlepb::SetAutoMode& Right)
{
    this->FromPb(Right);
}

void FPbSetAutoMode::FromPb(const idlepb::SetAutoMode& Right)
{
    enable = Right.enable();
}

void FPbSetAutoMode::ToPb(idlepb::SetAutoMode* Out) const
{
    Out->set_enable(enable);    
}

void FPbSetAutoMode::Reset()
{
    enable = bool();    
}

void FPbSetAutoMode::operator=(const idlepb::SetAutoMode& Right)
{
    this->FromPb(Right);
}

bool FPbSetAutoMode::operator==(const FPbSetAutoMode& Right) const
{
    if (this->enable != Right.enable)
        return false;
    return true;
}

bool FPbSetAutoMode::operator!=(const FPbSetAutoMode& Right) const
{
    return !operator==(Right);
}

FPbReinforceEquipmentReq::FPbReinforceEquipmentReq()
{
    Reset();        
}

FPbReinforceEquipmentReq::FPbReinforceEquipmentReq(const idlepb::ReinforceEquipmentReq& Right)
{
    this->FromPb(Right);
}

void FPbReinforceEquipmentReq::FromPb(const idlepb::ReinforceEquipmentReq& Right)
{
    id = Right.id();
}

void FPbReinforceEquipmentReq::ToPb(idlepb::ReinforceEquipmentReq* Out) const
{
    Out->set_id(id);    
}

void FPbReinforceEquipmentReq::Reset()
{
    id = int64();    
}

void FPbReinforceEquipmentReq::operator=(const idlepb::ReinforceEquipmentReq& Right)
{
    this->FromPb(Right);
}

bool FPbReinforceEquipmentReq::operator==(const FPbReinforceEquipmentReq& Right) const
{
    if (this->id != Right.id)
        return false;
    return true;
}

bool FPbReinforceEquipmentReq::operator!=(const FPbReinforceEquipmentReq& Right) const
{
    return !operator==(Right);
}

FPbReinforceEquipmentAck::FPbReinforceEquipmentAck()
{
    Reset();        
}

FPbReinforceEquipmentAck::FPbReinforceEquipmentAck(const idlepb::ReinforceEquipmentAck& Right)
{
    this->FromPb(Right);
}

void FPbReinforceEquipmentAck::FromPb(const idlepb::ReinforceEquipmentAck& Right)
{
    ok = Right.ok();
}

void FPbReinforceEquipmentAck::ToPb(idlepb::ReinforceEquipmentAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbReinforceEquipmentAck::Reset()
{
    ok = bool();    
}

void FPbReinforceEquipmentAck::operator=(const idlepb::ReinforceEquipmentAck& Right)
{
    this->FromPb(Right);
}

bool FPbReinforceEquipmentAck::operator==(const FPbReinforceEquipmentAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbReinforceEquipmentAck::operator!=(const FPbReinforceEquipmentAck& Right) const
{
    return !operator==(Right);
}

FPbQiWenEquipmentReq::FPbQiWenEquipmentReq()
{
    Reset();        
}

FPbQiWenEquipmentReq::FPbQiWenEquipmentReq(const idlepb::QiWenEquipmentReq& Right)
{
    this->FromPb(Right);
}

void FPbQiWenEquipmentReq::FromPb(const idlepb::QiWenEquipmentReq& Right)
{
    id = Right.id();
    select_abc = Right.select_abc();
    commit_materials.Empty();
    for (const auto& Elem : Right.commit_materials())
    {
        commit_materials.Emplace(Elem);
    }
}

void FPbQiWenEquipmentReq::ToPb(idlepb::QiWenEquipmentReq* Out) const
{
    Out->set_id(id);
    Out->set_select_abc(select_abc);
    for (const auto& Elem : commit_materials)
    {
        Out->add_commit_materials(Elem);    
    }    
}

void FPbQiWenEquipmentReq::Reset()
{
    id = int64();
    select_abc = int32();
    commit_materials = TArray<int64>();    
}

void FPbQiWenEquipmentReq::operator=(const idlepb::QiWenEquipmentReq& Right)
{
    this->FromPb(Right);
}

bool FPbQiWenEquipmentReq::operator==(const FPbQiWenEquipmentReq& Right) const
{
    if (this->id != Right.id)
        return false;
    if (this->select_abc != Right.select_abc)
        return false;
    if (this->commit_materials != Right.commit_materials)
        return false;
    return true;
}

bool FPbQiWenEquipmentReq::operator!=(const FPbQiWenEquipmentReq& Right) const
{
    return !operator==(Right);
}

FPbQiWenEquipmentAck::FPbQiWenEquipmentAck()
{
    Reset();        
}

FPbQiWenEquipmentAck::FPbQiWenEquipmentAck(const idlepb::QiWenEquipmentAck& Right)
{
    this->FromPb(Right);
}

void FPbQiWenEquipmentAck::FromPb(const idlepb::QiWenEquipmentAck& Right)
{
    ok = Right.ok();
}

void FPbQiWenEquipmentAck::ToPb(idlepb::QiWenEquipmentAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbQiWenEquipmentAck::Reset()
{
    ok = bool();    
}

void FPbQiWenEquipmentAck::operator=(const idlepb::QiWenEquipmentAck& Right)
{
    this->FromPb(Right);
}

bool FPbQiWenEquipmentAck::operator==(const FPbQiWenEquipmentAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbQiWenEquipmentAck::operator!=(const FPbQiWenEquipmentAck& Right) const
{
    return !operator==(Right);
}

FPbRefineEquipmentReq::FPbRefineEquipmentReq()
{
    Reset();        
}

FPbRefineEquipmentReq::FPbRefineEquipmentReq(const idlepb::RefineEquipmentReq& Right)
{
    this->FromPb(Right);
}

void FPbRefineEquipmentReq::FromPb(const idlepb::RefineEquipmentReq& Right)
{
    id = Right.id();
}

void FPbRefineEquipmentReq::ToPb(idlepb::RefineEquipmentReq* Out) const
{
    Out->set_id(id);    
}

void FPbRefineEquipmentReq::Reset()
{
    id = int64();    
}

void FPbRefineEquipmentReq::operator=(const idlepb::RefineEquipmentReq& Right)
{
    this->FromPb(Right);
}

bool FPbRefineEquipmentReq::operator==(const FPbRefineEquipmentReq& Right) const
{
    if (this->id != Right.id)
        return false;
    return true;
}

bool FPbRefineEquipmentReq::operator!=(const FPbRefineEquipmentReq& Right) const
{
    return !operator==(Right);
}

FPbRefineEquipmentAck::FPbRefineEquipmentAck()
{
    Reset();        
}

FPbRefineEquipmentAck::FPbRefineEquipmentAck(const idlepb::RefineEquipmentAck& Right)
{
    this->FromPb(Right);
}

void FPbRefineEquipmentAck::FromPb(const idlepb::RefineEquipmentAck& Right)
{
    ok = Right.ok();
}

void FPbRefineEquipmentAck::ToPb(idlepb::RefineEquipmentAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbRefineEquipmentAck::Reset()
{
    ok = bool();    
}

void FPbRefineEquipmentAck::operator=(const idlepb::RefineEquipmentAck& Right)
{
    this->FromPb(Right);
}

bool FPbRefineEquipmentAck::operator==(const FPbRefineEquipmentAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbRefineEquipmentAck::operator!=(const FPbRefineEquipmentAck& Right) const
{
    return !operator==(Right);
}

FPbResetEquipmentReq::FPbResetEquipmentReq()
{
    Reset();        
}

FPbResetEquipmentReq::FPbResetEquipmentReq(const idlepb::ResetEquipmentReq& Right)
{
    this->FromPb(Right);
}

void FPbResetEquipmentReq::FromPb(const idlepb::ResetEquipmentReq& Right)
{
    id.Empty();
    for (const auto& Elem : Right.id())
    {
        id.Emplace(Elem);
    }
}

void FPbResetEquipmentReq::ToPb(idlepb::ResetEquipmentReq* Out) const
{
    for (const auto& Elem : id)
    {
        Out->add_id(Elem);    
    }    
}

void FPbResetEquipmentReq::Reset()
{
    id = TArray<int64>();    
}

void FPbResetEquipmentReq::operator=(const idlepb::ResetEquipmentReq& Right)
{
    this->FromPb(Right);
}

bool FPbResetEquipmentReq::operator==(const FPbResetEquipmentReq& Right) const
{
    if (this->id != Right.id)
        return false;
    return true;
}

bool FPbResetEquipmentReq::operator!=(const FPbResetEquipmentReq& Right) const
{
    return !operator==(Right);
}

FPbResetEquipmentAck::FPbResetEquipmentAck()
{
    Reset();        
}

FPbResetEquipmentAck::FPbResetEquipmentAck(const idlepb::ResetEquipmentAck& Right)
{
    this->FromPb(Right);
}

void FPbResetEquipmentAck::FromPb(const idlepb::ResetEquipmentAck& Right)
{
    ok = Right.ok();
    items.Empty();
    for (const auto& Elem : Right.items())
    {
        items.Emplace(Elem);
    }
}

void FPbResetEquipmentAck::ToPb(idlepb::ResetEquipmentAck* Out) const
{
    Out->set_ok(ok);
    for (const auto& Elem : items)
    {
        Elem.ToPb(Out->add_items());    
    }    
}

void FPbResetEquipmentAck::Reset()
{
    ok = bool();
    items = TArray<FPbSimpleItemData>();    
}

void FPbResetEquipmentAck::operator=(const idlepb::ResetEquipmentAck& Right)
{
    this->FromPb(Right);
}

bool FPbResetEquipmentAck::operator==(const FPbResetEquipmentAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    if (this->items != Right.items)
        return false;
    return true;
}

bool FPbResetEquipmentAck::operator!=(const FPbResetEquipmentAck& Right) const
{
    return !operator==(Right);
}

FPbInheritEquipmentReq::FPbInheritEquipmentReq()
{
    Reset();        
}

FPbInheritEquipmentReq::FPbInheritEquipmentReq(const idlepb::InheritEquipmentReq& Right)
{
    this->FromPb(Right);
}

void FPbInheritEquipmentReq::FromPb(const idlepb::InheritEquipmentReq& Right)
{
    equipment_from = Right.equipment_from();
    equipment_to = Right.equipment_to();
}

void FPbInheritEquipmentReq::ToPb(idlepb::InheritEquipmentReq* Out) const
{
    Out->set_equipment_from(equipment_from);
    Out->set_equipment_to(equipment_to);    
}

void FPbInheritEquipmentReq::Reset()
{
    equipment_from = int64();
    equipment_to = int64();    
}

void FPbInheritEquipmentReq::operator=(const idlepb::InheritEquipmentReq& Right)
{
    this->FromPb(Right);
}

bool FPbInheritEquipmentReq::operator==(const FPbInheritEquipmentReq& Right) const
{
    if (this->equipment_from != Right.equipment_from)
        return false;
    if (this->equipment_to != Right.equipment_to)
        return false;
    return true;
}

bool FPbInheritEquipmentReq::operator!=(const FPbInheritEquipmentReq& Right) const
{
    return !operator==(Right);
}

FPbInheritEquipmentAck::FPbInheritEquipmentAck()
{
    Reset();        
}

FPbInheritEquipmentAck::FPbInheritEquipmentAck(const idlepb::InheritEquipmentAck& Right)
{
    this->FromPb(Right);
}

void FPbInheritEquipmentAck::FromPb(const idlepb::InheritEquipmentAck& Right)
{
    ok = Right.ok();
    items.Empty();
    for (const auto& Elem : Right.items())
    {
        items.Emplace(Elem);
    }
}

void FPbInheritEquipmentAck::ToPb(idlepb::InheritEquipmentAck* Out) const
{
    Out->set_ok(ok);
    for (const auto& Elem : items)
    {
        Elem.ToPb(Out->add_items());    
    }    
}

void FPbInheritEquipmentAck::Reset()
{
    ok = bool();
    items = TArray<FPbSimpleItemData>();    
}

void FPbInheritEquipmentAck::operator=(const idlepb::InheritEquipmentAck& Right)
{
    this->FromPb(Right);
}

bool FPbInheritEquipmentAck::operator==(const FPbInheritEquipmentAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    if (this->items != Right.items)
        return false;
    return true;
}

bool FPbInheritEquipmentAck::operator!=(const FPbInheritEquipmentAck& Right) const
{
    return !operator==(Right);
}

FPbLockItemReq::FPbLockItemReq()
{
    Reset();        
}

FPbLockItemReq::FPbLockItemReq(const idlepb::LockItemReq& Right)
{
    this->FromPb(Right);
}

void FPbLockItemReq::FromPb(const idlepb::LockItemReq& Right)
{
    id = Right.id();
}

void FPbLockItemReq::ToPb(idlepb::LockItemReq* Out) const
{
    Out->set_id(id);    
}

void FPbLockItemReq::Reset()
{
    id = int64();    
}

void FPbLockItemReq::operator=(const idlepb::LockItemReq& Right)
{
    this->FromPb(Right);
}

bool FPbLockItemReq::operator==(const FPbLockItemReq& Right) const
{
    if (this->id != Right.id)
        return false;
    return true;
}

bool FPbLockItemReq::operator!=(const FPbLockItemReq& Right) const
{
    return !operator==(Right);
}

FPbLockItemAck::FPbLockItemAck()
{
    Reset();        
}

FPbLockItemAck::FPbLockItemAck(const idlepb::LockItemAck& Right)
{
    this->FromPb(Right);
}

void FPbLockItemAck::FromPb(const idlepb::LockItemAck& Right)
{
    ok = Right.ok();
}

void FPbLockItemAck::ToPb(idlepb::LockItemAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbLockItemAck::Reset()
{
    ok = bool();    
}

void FPbLockItemAck::operator=(const idlepb::LockItemAck& Right)
{
    this->FromPb(Right);
}

bool FPbLockItemAck::operator==(const FPbLockItemAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbLockItemAck::operator!=(const FPbLockItemAck& Right) const
{
    return !operator==(Right);
}

FPbCollectionActivatedSuit::FPbCollectionActivatedSuit()
{
    Reset();        
}

FPbCollectionActivatedSuit::FPbCollectionActivatedSuit(const idlepb::CollectionActivatedSuit& Right)
{
    this->FromPb(Right);
}

void FPbCollectionActivatedSuit::FromPb(const idlepb::CollectionActivatedSuit& Right)
{
    id = Right.id();
    index = Right.index();
    combat_power = Right.combat_power();
}

void FPbCollectionActivatedSuit::ToPb(idlepb::CollectionActivatedSuit* Out) const
{
    Out->set_id(id);
    Out->set_index(index);
    Out->set_combat_power(combat_power);    
}

void FPbCollectionActivatedSuit::Reset()
{
    id = int32();
    index = int32();
    combat_power = float();    
}

void FPbCollectionActivatedSuit::operator=(const idlepb::CollectionActivatedSuit& Right)
{
    this->FromPb(Right);
}

bool FPbCollectionActivatedSuit::operator==(const FPbCollectionActivatedSuit& Right) const
{
    if (this->id != Right.id)
        return false;
    if (this->index != Right.index)
        return false;
    if (this->combat_power != Right.combat_power)
        return false;
    return true;
}

bool FPbCollectionActivatedSuit::operator!=(const FPbCollectionActivatedSuit& Right) const
{
    return !operator==(Right);
}

FPbGetRoleCollectionDataReq::FPbGetRoleCollectionDataReq()
{
    Reset();        
}

FPbGetRoleCollectionDataReq::FPbGetRoleCollectionDataReq(const idlepb::GetRoleCollectionDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleCollectionDataReq::FromPb(const idlepb::GetRoleCollectionDataReq& Right)
{
}

void FPbGetRoleCollectionDataReq::ToPb(idlepb::GetRoleCollectionDataReq* Out) const
{    
}

void FPbGetRoleCollectionDataReq::Reset()
{    
}

void FPbGetRoleCollectionDataReq::operator=(const idlepb::GetRoleCollectionDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleCollectionDataReq::operator==(const FPbGetRoleCollectionDataReq& Right) const
{
    return true;
}

bool FPbGetRoleCollectionDataReq::operator!=(const FPbGetRoleCollectionDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetRoleCollectionDataRsp::FPbGetRoleCollectionDataRsp()
{
    Reset();        
}

FPbGetRoleCollectionDataRsp::FPbGetRoleCollectionDataRsp(const idlepb::GetRoleCollectionDataRsp& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleCollectionDataRsp::FromPb(const idlepb::GetRoleCollectionDataRsp& Right)
{
    entries.Empty();
    for (const auto& Elem : Right.entries())
    {
        entries.Emplace(Elem);
    }
    common_pieces.Empty();
    for (const auto& Elem : Right.common_pieces())
    {
        common_pieces.Emplace(Elem);
    }
    actived_suite.Empty();
    for (const auto& Elem : Right.actived_suite())
    {
        actived_suite.Emplace(Elem);
    }
    draw_award_done_histories.Empty();
    for (const auto& Elem : Right.draw_award_done_histories())
    {
        draw_award_done_histories.Emplace(Elem);
    }
    can_award_histories.Empty();
    for (const auto& Elem : Right.can_award_histories())
    {
        can_award_histories.Emplace(Elem);
    }
    zone_active_awards.Empty();
    for (const auto& Elem : Right.zone_active_awards())
    {
        zone_active_awards.Emplace(Elem);
    }
    next_reset_enhance_ticks = Right.next_reset_enhance_ticks();
}

void FPbGetRoleCollectionDataRsp::ToPb(idlepb::GetRoleCollectionDataRsp* Out) const
{
    for (const auto& Elem : entries)
    {
        Elem.ToPb(Out->add_entries());    
    }
    for (const auto& Elem : common_pieces)
    {
        Elem.ToPb(Out->add_common_pieces());    
    }
    for (const auto& Elem : actived_suite)
    {
        Elem.ToPb(Out->add_actived_suite());    
    }
    for (const auto& Elem : draw_award_done_histories)
    {
        Out->add_draw_award_done_histories(Elem);    
    }
    for (const auto& Elem : can_award_histories)
    {
        Out->add_can_award_histories(Elem);    
    }
    for (const auto& Elem : zone_active_awards)
    {
        Elem.ToPb(Out->add_zone_active_awards());    
    }
    Out->set_next_reset_enhance_ticks(next_reset_enhance_ticks);    
}

void FPbGetRoleCollectionDataRsp::Reset()
{
    entries = TArray<FPbCollectionEntry>();
    common_pieces = TArray<FPbCommonCollectionPieceData>();
    actived_suite = TArray<FPbCollectionActivatedSuit>();
    draw_award_done_histories = TArray<int32>();
    can_award_histories = TArray<int32>();
    zone_active_awards = TArray<FPbCollectionZoneActiveAwardData>();
    next_reset_enhance_ticks = int64();    
}

void FPbGetRoleCollectionDataRsp::operator=(const idlepb::GetRoleCollectionDataRsp& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleCollectionDataRsp::operator==(const FPbGetRoleCollectionDataRsp& Right) const
{
    if (this->entries != Right.entries)
        return false;
    if (this->common_pieces != Right.common_pieces)
        return false;
    if (this->actived_suite != Right.actived_suite)
        return false;
    if (this->draw_award_done_histories != Right.draw_award_done_histories)
        return false;
    if (this->can_award_histories != Right.can_award_histories)
        return false;
    if (this->zone_active_awards != Right.zone_active_awards)
        return false;
    if (this->next_reset_enhance_ticks != Right.next_reset_enhance_ticks)
        return false;
    return true;
}

bool FPbGetRoleCollectionDataRsp::operator!=(const FPbGetRoleCollectionDataRsp& Right) const
{
    return !operator==(Right);
}

bool CheckEPbRoleCollectionOpTypeValid(int32 Val)
{
    return idlepb::RoleCollectionOpType_IsValid(Val);
}

const TCHAR* GetEPbRoleCollectionOpTypeDescription(EPbRoleCollectionOpType Val)
{
    switch (Val)
    {
        case EPbRoleCollectionOpType::RCOT_PieceFusion: return TEXT("碎片合成");
        case EPbRoleCollectionOpType::RCOT_UpgradeLevel: return TEXT("注灵");
        case EPbRoleCollectionOpType::RCOT_UpgradeStar: return TEXT("升星");
        case EPbRoleCollectionOpType::RCOT_DrawHistoryAward: return TEXT("领取渊源奖励");
        case EPbRoleCollectionOpType::RCOT_DrawZoneActiveAward: return TEXT("领取累计收集奖励");
        case EPbRoleCollectionOpType::RCOT_ResetEnhance: return TEXT("重置强化");
    }
    return TEXT("UNKNOWN");
}

FPbRoleCollectionOpReq::FPbRoleCollectionOpReq()
{
    Reset();        
}

FPbRoleCollectionOpReq::FPbRoleCollectionOpReq(const idlepb::RoleCollectionOpReq& Right)
{
    this->FromPb(Right);
}

void FPbRoleCollectionOpReq::FromPb(const idlepb::RoleCollectionOpReq& Right)
{
    op_type = static_cast<EPbRoleCollectionOpType>(Right.op_type());
    id = Right.id();
    consume_list.Empty();
    for (const auto& Elem : Right.consume_list())
    {
        consume_list.Emplace(Elem);
    }
    is_preview = Right.is_preview();
}

void FPbRoleCollectionOpReq::ToPb(idlepb::RoleCollectionOpReq* Out) const
{
    Out->set_op_type(static_cast<idlepb::RoleCollectionOpType>(op_type));
    Out->set_id(id);
    for (const auto& Elem : consume_list)
    {
        Elem.ToPb(Out->add_consume_list());    
    }
    Out->set_is_preview(is_preview);    
}

void FPbRoleCollectionOpReq::Reset()
{
    op_type = EPbRoleCollectionOpType();
    id = int32();
    consume_list = TArray<FPbMapValueInt32>();
    is_preview = bool();    
}

void FPbRoleCollectionOpReq::operator=(const idlepb::RoleCollectionOpReq& Right)
{
    this->FromPb(Right);
}

bool FPbRoleCollectionOpReq::operator==(const FPbRoleCollectionOpReq& Right) const
{
    if (this->op_type != Right.op_type)
        return false;
    if (this->id != Right.id)
        return false;
    if (this->consume_list != Right.consume_list)
        return false;
    if (this->is_preview != Right.is_preview)
        return false;
    return true;
}

bool FPbRoleCollectionOpReq::operator!=(const FPbRoleCollectionOpReq& Right) const
{
    return !operator==(Right);
}

FPbRoleCollectionOpAck::FPbRoleCollectionOpAck()
{
    Reset();        
}

FPbRoleCollectionOpAck::FPbRoleCollectionOpAck(const idlepb::RoleCollectionOpAck& Right)
{
    this->FromPb(Right);
}

void FPbRoleCollectionOpAck::FromPb(const idlepb::RoleCollectionOpAck& Right)
{
    op_type = static_cast<EPbRoleCollectionOpType>(Right.op_type());
    ok = Right.ok();
    items.Empty();
    for (const auto& Elem : Right.items())
    {
        items.Emplace(Elem);
    }
}

void FPbRoleCollectionOpAck::ToPb(idlepb::RoleCollectionOpAck* Out) const
{
    Out->set_op_type(static_cast<idlepb::RoleCollectionOpType>(op_type));
    Out->set_ok(ok);
    for (const auto& Elem : items)
    {
        Elem.ToPb(Out->add_items());    
    }    
}

void FPbRoleCollectionOpAck::Reset()
{
    op_type = EPbRoleCollectionOpType();
    ok = bool();
    items = TArray<FPbSimpleItemData>();    
}

void FPbRoleCollectionOpAck::operator=(const idlepb::RoleCollectionOpAck& Right)
{
    this->FromPb(Right);
}

bool FPbRoleCollectionOpAck::operator==(const FPbRoleCollectionOpAck& Right) const
{
    if (this->op_type != Right.op_type)
        return false;
    if (this->ok != Right.ok)
        return false;
    if (this->items != Right.items)
        return false;
    return true;
}

bool FPbRoleCollectionOpAck::operator!=(const FPbRoleCollectionOpAck& Right) const
{
    return !operator==(Right);
}

FPbNotifyRoleCollectionData::FPbNotifyRoleCollectionData()
{
    Reset();        
}

FPbNotifyRoleCollectionData::FPbNotifyRoleCollectionData(const idlepb::NotifyRoleCollectionData& Right)
{
    this->FromPb(Right);
}

void FPbNotifyRoleCollectionData::FromPb(const idlepb::NotifyRoleCollectionData& Right)
{
    entry = Right.entry();
}

void FPbNotifyRoleCollectionData::ToPb(idlepb::NotifyRoleCollectionData* Out) const
{
    entry.ToPb(Out->mutable_entry());    
}

void FPbNotifyRoleCollectionData::Reset()
{
    entry = FPbCollectionEntry();    
}

void FPbNotifyRoleCollectionData::operator=(const idlepb::NotifyRoleCollectionData& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyRoleCollectionData::operator==(const FPbNotifyRoleCollectionData& Right) const
{
    if (this->entry != Right.entry)
        return false;
    return true;
}

bool FPbNotifyRoleCollectionData::operator!=(const FPbNotifyRoleCollectionData& Right) const
{
    return !operator==(Right);
}

FPbNotifyCommonCollectionPieceData::FPbNotifyCommonCollectionPieceData()
{
    Reset();        
}

FPbNotifyCommonCollectionPieceData::FPbNotifyCommonCollectionPieceData(const idlepb::NotifyCommonCollectionPieceData& Right)
{
    this->FromPb(Right);
}

void FPbNotifyCommonCollectionPieceData::FromPb(const idlepb::NotifyCommonCollectionPieceData& Right)
{
    common_pieces.Empty();
    for (const auto& Elem : Right.common_pieces())
    {
        common_pieces.Emplace(Elem);
    }
}

void FPbNotifyCommonCollectionPieceData::ToPb(idlepb::NotifyCommonCollectionPieceData* Out) const
{
    for (const auto& Elem : common_pieces)
    {
        Elem.ToPb(Out->add_common_pieces());    
    }    
}

void FPbNotifyCommonCollectionPieceData::Reset()
{
    common_pieces = TArray<FPbCommonCollectionPieceData>();    
}

void FPbNotifyCommonCollectionPieceData::operator=(const idlepb::NotifyCommonCollectionPieceData& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyCommonCollectionPieceData::operator==(const FPbNotifyCommonCollectionPieceData& Right) const
{
    if (this->common_pieces != Right.common_pieces)
        return false;
    return true;
}

bool FPbNotifyCommonCollectionPieceData::operator!=(const FPbNotifyCommonCollectionPieceData& Right) const
{
    return !operator==(Right);
}

FPbNotifyCollectionActivatedSuit::FPbNotifyCollectionActivatedSuit()
{
    Reset();        
}

FPbNotifyCollectionActivatedSuit::FPbNotifyCollectionActivatedSuit(const idlepb::NotifyCollectionActivatedSuit& Right)
{
    this->FromPb(Right);
}

void FPbNotifyCollectionActivatedSuit::FromPb(const idlepb::NotifyCollectionActivatedSuit& Right)
{
    actived_suite.Empty();
    for (const auto& Elem : Right.actived_suite())
    {
        actived_suite.Emplace(Elem);
    }
}

void FPbNotifyCollectionActivatedSuit::ToPb(idlepb::NotifyCollectionActivatedSuit* Out) const
{
    for (const auto& Elem : actived_suite)
    {
        Elem.ToPb(Out->add_actived_suite());    
    }    
}

void FPbNotifyCollectionActivatedSuit::Reset()
{
    actived_suite = TArray<FPbCollectionActivatedSuit>();    
}

void FPbNotifyCollectionActivatedSuit::operator=(const idlepb::NotifyCollectionActivatedSuit& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyCollectionActivatedSuit::operator==(const FPbNotifyCollectionActivatedSuit& Right) const
{
    if (this->actived_suite != Right.actived_suite)
        return false;
    return true;
}

bool FPbNotifyCollectionActivatedSuit::operator!=(const FPbNotifyCollectionActivatedSuit& Right) const
{
    return !operator==(Right);
}

FPbShareSelfRoleCollectionReq::FPbShareSelfRoleCollectionReq()
{
    Reset();        
}

FPbShareSelfRoleCollectionReq::FPbShareSelfRoleCollectionReq(const idlepb::ShareSelfRoleCollectionReq& Right)
{
    this->FromPb(Right);
}

void FPbShareSelfRoleCollectionReq::FromPb(const idlepb::ShareSelfRoleCollectionReq& Right)
{
    id = Right.id();
}

void FPbShareSelfRoleCollectionReq::ToPb(idlepb::ShareSelfRoleCollectionReq* Out) const
{
    Out->set_id(id);    
}

void FPbShareSelfRoleCollectionReq::Reset()
{
    id = int32();    
}

void FPbShareSelfRoleCollectionReq::operator=(const idlepb::ShareSelfRoleCollectionReq& Right)
{
    this->FromPb(Right);
}

bool FPbShareSelfRoleCollectionReq::operator==(const FPbShareSelfRoleCollectionReq& Right) const
{
    if (this->id != Right.id)
        return false;
    return true;
}

bool FPbShareSelfRoleCollectionReq::operator!=(const FPbShareSelfRoleCollectionReq& Right) const
{
    return !operator==(Right);
}

FPbShareSelfRoleCollectionRsp::FPbShareSelfRoleCollectionRsp()
{
    Reset();        
}

FPbShareSelfRoleCollectionRsp::FPbShareSelfRoleCollectionRsp(const idlepb::ShareSelfRoleCollectionRsp& Right)
{
    this->FromPb(Right);
}

void FPbShareSelfRoleCollectionRsp::FromPb(const idlepb::ShareSelfRoleCollectionRsp& Right)
{
    share_id = Right.share_id();
}

void FPbShareSelfRoleCollectionRsp::ToPb(idlepb::ShareSelfRoleCollectionRsp* Out) const
{
    Out->set_share_id(share_id);    
}

void FPbShareSelfRoleCollectionRsp::Reset()
{
    share_id = int64();    
}

void FPbShareSelfRoleCollectionRsp::operator=(const idlepb::ShareSelfRoleCollectionRsp& Right)
{
    this->FromPb(Right);
}

bool FPbShareSelfRoleCollectionRsp::operator==(const FPbShareSelfRoleCollectionRsp& Right) const
{
    if (this->share_id != Right.share_id)
        return false;
    return true;
}

bool FPbShareSelfRoleCollectionRsp::operator!=(const FPbShareSelfRoleCollectionRsp& Right) const
{
    return !operator==(Right);
}

FPbGetShareRoleCollectionDataReq::FPbGetShareRoleCollectionDataReq()
{
    Reset();        
}

FPbGetShareRoleCollectionDataReq::FPbGetShareRoleCollectionDataReq(const idlepb::GetShareRoleCollectionDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetShareRoleCollectionDataReq::FromPb(const idlepb::GetShareRoleCollectionDataReq& Right)
{
    share_id = Right.share_id();
}

void FPbGetShareRoleCollectionDataReq::ToPb(idlepb::GetShareRoleCollectionDataReq* Out) const
{
    Out->set_share_id(share_id);    
}

void FPbGetShareRoleCollectionDataReq::Reset()
{
    share_id = int64();    
}

void FPbGetShareRoleCollectionDataReq::operator=(const idlepb::GetShareRoleCollectionDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetShareRoleCollectionDataReq::operator==(const FPbGetShareRoleCollectionDataReq& Right) const
{
    if (this->share_id != Right.share_id)
        return false;
    return true;
}

bool FPbGetShareRoleCollectionDataReq::operator!=(const FPbGetShareRoleCollectionDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetShareRoleCollectionDataRsp::FPbGetShareRoleCollectionDataRsp()
{
    Reset();        
}

FPbGetShareRoleCollectionDataRsp::FPbGetShareRoleCollectionDataRsp(const idlepb::GetShareRoleCollectionDataRsp& Right)
{
    this->FromPb(Right);
}

void FPbGetShareRoleCollectionDataRsp::FromPb(const idlepb::GetShareRoleCollectionDataRsp& Right)
{
    ok = Right.ok();
    collection_data = Right.collection_data();
}

void FPbGetShareRoleCollectionDataRsp::ToPb(idlepb::GetShareRoleCollectionDataRsp* Out) const
{
    Out->set_ok(ok);
    collection_data.ToPb(Out->mutable_collection_data());    
}

void FPbGetShareRoleCollectionDataRsp::Reset()
{
    ok = bool();
    collection_data = FPbCollectionEntry();    
}

void FPbGetShareRoleCollectionDataRsp::operator=(const idlepb::GetShareRoleCollectionDataRsp& Right)
{
    this->FromPb(Right);
}

bool FPbGetShareRoleCollectionDataRsp::operator==(const FPbGetShareRoleCollectionDataRsp& Right) const
{
    if (this->ok != Right.ok)
        return false;
    if (this->collection_data != Right.collection_data)
        return false;
    return true;
}

bool FPbGetShareRoleCollectionDataRsp::operator!=(const FPbGetShareRoleCollectionDataRsp& Right) const
{
    return !operator==(Right);
}

FPbNotifyRoleCollectionHistories::FPbNotifyRoleCollectionHistories()
{
    Reset();        
}

FPbNotifyRoleCollectionHistories::FPbNotifyRoleCollectionHistories(const idlepb::NotifyRoleCollectionHistories& Right)
{
    this->FromPb(Right);
}

void FPbNotifyRoleCollectionHistories::FromPb(const idlepb::NotifyRoleCollectionHistories& Right)
{
    draw_award_done_histories.Empty();
    for (const auto& Elem : Right.draw_award_done_histories())
    {
        draw_award_done_histories.Emplace(Elem);
    }
    can_award_histories.Empty();
    for (const auto& Elem : Right.can_award_histories())
    {
        can_award_histories.Emplace(Elem);
    }
}

void FPbNotifyRoleCollectionHistories::ToPb(idlepb::NotifyRoleCollectionHistories* Out) const
{
    for (const auto& Elem : draw_award_done_histories)
    {
        Out->add_draw_award_done_histories(Elem);    
    }
    for (const auto& Elem : can_award_histories)
    {
        Out->add_can_award_histories(Elem);    
    }    
}

void FPbNotifyRoleCollectionHistories::Reset()
{
    draw_award_done_histories = TArray<int32>();
    can_award_histories = TArray<int32>();    
}

void FPbNotifyRoleCollectionHistories::operator=(const idlepb::NotifyRoleCollectionHistories& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyRoleCollectionHistories::operator==(const FPbNotifyRoleCollectionHistories& Right) const
{
    if (this->draw_award_done_histories != Right.draw_award_done_histories)
        return false;
    if (this->can_award_histories != Right.can_award_histories)
        return false;
    return true;
}

bool FPbNotifyRoleCollectionHistories::operator!=(const FPbNotifyRoleCollectionHistories& Right) const
{
    return !operator==(Right);
}

FPbNotifyCollectionZoneActiveAwards::FPbNotifyCollectionZoneActiveAwards()
{
    Reset();        
}

FPbNotifyCollectionZoneActiveAwards::FPbNotifyCollectionZoneActiveAwards(const idlepb::NotifyCollectionZoneActiveAwards& Right)
{
    this->FromPb(Right);
}

void FPbNotifyCollectionZoneActiveAwards::FromPb(const idlepb::NotifyCollectionZoneActiveAwards& Right)
{
    zone_active_awards.Empty();
    for (const auto& Elem : Right.zone_active_awards())
    {
        zone_active_awards.Emplace(Elem);
    }
}

void FPbNotifyCollectionZoneActiveAwards::ToPb(idlepb::NotifyCollectionZoneActiveAwards* Out) const
{
    for (const auto& Elem : zone_active_awards)
    {
        Elem.ToPb(Out->add_zone_active_awards());    
    }    
}

void FPbNotifyCollectionZoneActiveAwards::Reset()
{
    zone_active_awards = TArray<FPbCollectionZoneActiveAwardData>();    
}

void FPbNotifyCollectionZoneActiveAwards::operator=(const idlepb::NotifyCollectionZoneActiveAwards& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyCollectionZoneActiveAwards::operator==(const FPbNotifyCollectionZoneActiveAwards& Right) const
{
    if (this->zone_active_awards != Right.zone_active_awards)
        return false;
    return true;
}

bool FPbNotifyCollectionZoneActiveAwards::operator!=(const FPbNotifyCollectionZoneActiveAwards& Right) const
{
    return !operator==(Right);
}

FPbNotifyRoleCollectionNextResetEnhanceTicks::FPbNotifyRoleCollectionNextResetEnhanceTicks()
{
    Reset();        
}

FPbNotifyRoleCollectionNextResetEnhanceTicks::FPbNotifyRoleCollectionNextResetEnhanceTicks(const idlepb::NotifyRoleCollectionNextResetEnhanceTicks& Right)
{
    this->FromPb(Right);
}

void FPbNotifyRoleCollectionNextResetEnhanceTicks::FromPb(const idlepb::NotifyRoleCollectionNextResetEnhanceTicks& Right)
{
    next_reset_enhance_ticks = Right.next_reset_enhance_ticks();
}

void FPbNotifyRoleCollectionNextResetEnhanceTicks::ToPb(idlepb::NotifyRoleCollectionNextResetEnhanceTicks* Out) const
{
    Out->set_next_reset_enhance_ticks(next_reset_enhance_ticks);    
}

void FPbNotifyRoleCollectionNextResetEnhanceTicks::Reset()
{
    next_reset_enhance_ticks = int64();    
}

void FPbNotifyRoleCollectionNextResetEnhanceTicks::operator=(const idlepb::NotifyRoleCollectionNextResetEnhanceTicks& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyRoleCollectionNextResetEnhanceTicks::operator==(const FPbNotifyRoleCollectionNextResetEnhanceTicks& Right) const
{
    if (this->next_reset_enhance_ticks != Right.next_reset_enhance_ticks)
        return false;
    return true;
}

bool FPbNotifyRoleCollectionNextResetEnhanceTicks::operator!=(const FPbNotifyRoleCollectionNextResetEnhanceTicks& Right) const
{
    return !operator==(Right);
}

FPbRoleBattleHistoryList::FPbRoleBattleHistoryList()
{
    Reset();        
}

FPbRoleBattleHistoryList::FPbRoleBattleHistoryList(const idlepb::RoleBattleHistoryList& Right)
{
    this->FromPb(Right);
}

void FPbRoleBattleHistoryList::FromPb(const idlepb::RoleBattleHistoryList& Right)
{
    entries.Empty();
    for (const auto& Elem : Right.entries())
    {
        entries.Emplace(Elem);
    }
}

void FPbRoleBattleHistoryList::ToPb(idlepb::RoleBattleHistoryList* Out) const
{
    for (const auto& Elem : entries)
    {
        Elem.ToPb(Out->add_entries());    
    }    
}

void FPbRoleBattleHistoryList::Reset()
{
    entries = TArray<FPbRoleBattleInfo>();    
}

void FPbRoleBattleHistoryList::operator=(const idlepb::RoleBattleHistoryList& Right)
{
    this->FromPb(Right);
}

bool FPbRoleBattleHistoryList::operator==(const FPbRoleBattleHistoryList& Right) const
{
    if (this->entries != Right.entries)
        return false;
    return true;
}

bool FPbRoleBattleHistoryList::operator!=(const FPbRoleBattleHistoryList& Right) const
{
    return !operator==(Right);
}

FPbNotifySoloArenaChallengeOver::FPbNotifySoloArenaChallengeOver()
{
    Reset();        
}

FPbNotifySoloArenaChallengeOver::FPbNotifySoloArenaChallengeOver(const idlepb::NotifySoloArenaChallengeOver& Right)
{
    this->FromPb(Right);
}

void FPbNotifySoloArenaChallengeOver::FromPb(const idlepb::NotifySoloArenaChallengeOver& Right)
{
    win = Right.win();
    info = Right.info();
}

void FPbNotifySoloArenaChallengeOver::ToPb(idlepb::NotifySoloArenaChallengeOver* Out) const
{
    Out->set_win(win);
    info.ToPb(Out->mutable_info());    
}

void FPbNotifySoloArenaChallengeOver::Reset()
{
    win = bool();
    info = FPbBattleInfo();    
}

void FPbNotifySoloArenaChallengeOver::operator=(const idlepb::NotifySoloArenaChallengeOver& Right)
{
    this->FromPb(Right);
}

bool FPbNotifySoloArenaChallengeOver::operator==(const FPbNotifySoloArenaChallengeOver& Right) const
{
    if (this->win != Right.win)
        return false;
    if (this->info != Right.info)
        return false;
    return true;
}

bool FPbNotifySoloArenaChallengeOver::operator!=(const FPbNotifySoloArenaChallengeOver& Right) const
{
    return !operator==(Right);
}

FPbSoloArenaChallengeReq::FPbSoloArenaChallengeReq()
{
    Reset();        
}

FPbSoloArenaChallengeReq::FPbSoloArenaChallengeReq(const idlepb::SoloArenaChallengeReq& Right)
{
    this->FromPb(Right);
}

void FPbSoloArenaChallengeReq::FromPb(const idlepb::SoloArenaChallengeReq& Right)
{
    target_role_id = Right.target_role_id();
}

void FPbSoloArenaChallengeReq::ToPb(idlepb::SoloArenaChallengeReq* Out) const
{
    Out->set_target_role_id(target_role_id);    
}

void FPbSoloArenaChallengeReq::Reset()
{
    target_role_id = int64();    
}

void FPbSoloArenaChallengeReq::operator=(const idlepb::SoloArenaChallengeReq& Right)
{
    this->FromPb(Right);
}

bool FPbSoloArenaChallengeReq::operator==(const FPbSoloArenaChallengeReq& Right) const
{
    if (this->target_role_id != Right.target_role_id)
        return false;
    return true;
}

bool FPbSoloArenaChallengeReq::operator!=(const FPbSoloArenaChallengeReq& Right) const
{
    return !operator==(Right);
}

FPbSoloArenaChallengeAck::FPbSoloArenaChallengeAck()
{
    Reset();        
}

FPbSoloArenaChallengeAck::FPbSoloArenaChallengeAck(const idlepb::SoloArenaChallengeAck& Right)
{
    this->FromPb(Right);
}

void FPbSoloArenaChallengeAck::FromPb(const idlepb::SoloArenaChallengeAck& Right)
{
    ok = Right.ok();
}

void FPbSoloArenaChallengeAck::ToPb(idlepb::SoloArenaChallengeAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbSoloArenaChallengeAck::Reset()
{
    ok = bool();    
}

void FPbSoloArenaChallengeAck::operator=(const idlepb::SoloArenaChallengeAck& Right)
{
    this->FromPb(Right);
}

bool FPbSoloArenaChallengeAck::operator==(const FPbSoloArenaChallengeAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbSoloArenaChallengeAck::operator!=(const FPbSoloArenaChallengeAck& Right) const
{
    return !operator==(Right);
}

FPbSoloArenaQuickEndReq::FPbSoloArenaQuickEndReq()
{
    Reset();        
}

FPbSoloArenaQuickEndReq::FPbSoloArenaQuickEndReq(const idlepb::SoloArenaQuickEndReq& Right)
{
    this->FromPb(Right);
}

void FPbSoloArenaQuickEndReq::FromPb(const idlepb::SoloArenaQuickEndReq& Right)
{
    is_exit = Right.is_exit();
}

void FPbSoloArenaQuickEndReq::ToPb(idlepb::SoloArenaQuickEndReq* Out) const
{
    Out->set_is_exit(is_exit);    
}

void FPbSoloArenaQuickEndReq::Reset()
{
    is_exit = bool();    
}

void FPbSoloArenaQuickEndReq::operator=(const idlepb::SoloArenaQuickEndReq& Right)
{
    this->FromPb(Right);
}

bool FPbSoloArenaQuickEndReq::operator==(const FPbSoloArenaQuickEndReq& Right) const
{
    if (this->is_exit != Right.is_exit)
        return false;
    return true;
}

bool FPbSoloArenaQuickEndReq::operator!=(const FPbSoloArenaQuickEndReq& Right) const
{
    return !operator==(Right);
}

FPbSoloArenaQuickEndAck::FPbSoloArenaQuickEndAck()
{
    Reset();        
}

FPbSoloArenaQuickEndAck::FPbSoloArenaQuickEndAck(const idlepb::SoloArenaQuickEndAck& Right)
{
    this->FromPb(Right);
}

void FPbSoloArenaQuickEndAck::FromPb(const idlepb::SoloArenaQuickEndAck& Right)
{
}

void FPbSoloArenaQuickEndAck::ToPb(idlepb::SoloArenaQuickEndAck* Out) const
{    
}

void FPbSoloArenaQuickEndAck::Reset()
{    
}

void FPbSoloArenaQuickEndAck::operator=(const idlepb::SoloArenaQuickEndAck& Right)
{
    this->FromPb(Right);
}

bool FPbSoloArenaQuickEndAck::operator==(const FPbSoloArenaQuickEndAck& Right) const
{
    return true;
}

bool FPbSoloArenaQuickEndAck::operator!=(const FPbSoloArenaQuickEndAck& Right) const
{
    return !operator==(Right);
}

FPbGetSoloArenaHistoryListReq::FPbGetSoloArenaHistoryListReq()
{
    Reset();        
}

FPbGetSoloArenaHistoryListReq::FPbGetSoloArenaHistoryListReq(const idlepb::GetSoloArenaHistoryListReq& Right)
{
    this->FromPb(Right);
}

void FPbGetSoloArenaHistoryListReq::FromPb(const idlepb::GetSoloArenaHistoryListReq& Right)
{
    type = static_cast<EPbSoloType>(Right.type());
}

void FPbGetSoloArenaHistoryListReq::ToPb(idlepb::GetSoloArenaHistoryListReq* Out) const
{
    Out->set_type(static_cast<idlepb::SoloType>(type));    
}

void FPbGetSoloArenaHistoryListReq::Reset()
{
    type = EPbSoloType();    
}

void FPbGetSoloArenaHistoryListReq::operator=(const idlepb::GetSoloArenaHistoryListReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetSoloArenaHistoryListReq::operator==(const FPbGetSoloArenaHistoryListReq& Right) const
{
    if (this->type != Right.type)
        return false;
    return true;
}

bool FPbGetSoloArenaHistoryListReq::operator!=(const FPbGetSoloArenaHistoryListReq& Right) const
{
    return !operator==(Right);
}

FPbGetSoloArenaHistoryListAck::FPbGetSoloArenaHistoryListAck()
{
    Reset();        
}

FPbGetSoloArenaHistoryListAck::FPbGetSoloArenaHistoryListAck(const idlepb::GetSoloArenaHistoryListAck& Right)
{
    this->FromPb(Right);
}

void FPbGetSoloArenaHistoryListAck::FromPb(const idlepb::GetSoloArenaHistoryListAck& Right)
{
    data = Right.data();
}

void FPbGetSoloArenaHistoryListAck::ToPb(idlepb::GetSoloArenaHistoryListAck* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbGetSoloArenaHistoryListAck::Reset()
{
    data = FPbRoleBattleHistoryList();    
}

void FPbGetSoloArenaHistoryListAck::operator=(const idlepb::GetSoloArenaHistoryListAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetSoloArenaHistoryListAck::operator==(const FPbGetSoloArenaHistoryListAck& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbGetSoloArenaHistoryListAck::operator!=(const FPbGetSoloArenaHistoryListAck& Right) const
{
    return !operator==(Right);
}

FPbReplaySoloArenaHistoryReq::FPbReplaySoloArenaHistoryReq()
{
    Reset();        
}

FPbReplaySoloArenaHistoryReq::FPbReplaySoloArenaHistoryReq(const idlepb::ReplaySoloArenaHistoryReq& Right)
{
    this->FromPb(Right);
}

void FPbReplaySoloArenaHistoryReq::FromPb(const idlepb::ReplaySoloArenaHistoryReq& Right)
{
    history_world_id = Right.history_world_id();
}

void FPbReplaySoloArenaHistoryReq::ToPb(idlepb::ReplaySoloArenaHistoryReq* Out) const
{
    Out->set_history_world_id(history_world_id);    
}

void FPbReplaySoloArenaHistoryReq::Reset()
{
    history_world_id = int64();    
}

void FPbReplaySoloArenaHistoryReq::operator=(const idlepb::ReplaySoloArenaHistoryReq& Right)
{
    this->FromPb(Right);
}

bool FPbReplaySoloArenaHistoryReq::operator==(const FPbReplaySoloArenaHistoryReq& Right) const
{
    if (this->history_world_id != Right.history_world_id)
        return false;
    return true;
}

bool FPbReplaySoloArenaHistoryReq::operator!=(const FPbReplaySoloArenaHistoryReq& Right) const
{
    return !operator==(Right);
}

FPbReplaySoloArenaHistoryAck::FPbReplaySoloArenaHistoryAck()
{
    Reset();        
}

FPbReplaySoloArenaHistoryAck::FPbReplaySoloArenaHistoryAck(const idlepb::ReplaySoloArenaHistoryAck& Right)
{
    this->FromPb(Right);
}

void FPbReplaySoloArenaHistoryAck::FromPb(const idlepb::ReplaySoloArenaHistoryAck& Right)
{
    ok = Right.ok();
    data = Right.data();
}

void FPbReplaySoloArenaHistoryAck::ToPb(idlepb::ReplaySoloArenaHistoryAck* Out) const
{
    Out->set_ok(ok);
    data.ToPb(Out->mutable_data());    
}

void FPbReplaySoloArenaHistoryAck::Reset()
{
    ok = bool();
    data = FPbCompressedData();    
}

void FPbReplaySoloArenaHistoryAck::operator=(const idlepb::ReplaySoloArenaHistoryAck& Right)
{
    this->FromPb(Right);
}

bool FPbReplaySoloArenaHistoryAck::operator==(const FPbReplaySoloArenaHistoryAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbReplaySoloArenaHistoryAck::operator!=(const FPbReplaySoloArenaHistoryAck& Right) const
{
    return !operator==(Right);
}

FPbGetBattleHistoryInfoReq::FPbGetBattleHistoryInfoReq()
{
    Reset();        
}

FPbGetBattleHistoryInfoReq::FPbGetBattleHistoryInfoReq(const idlepb::GetBattleHistoryInfoReq& Right)
{
    this->FromPb(Right);
}

void FPbGetBattleHistoryInfoReq::FromPb(const idlepb::GetBattleHistoryInfoReq& Right)
{
    history_world_id = Right.history_world_id();
}

void FPbGetBattleHistoryInfoReq::ToPb(idlepb::GetBattleHistoryInfoReq* Out) const
{
    Out->set_history_world_id(history_world_id);    
}

void FPbGetBattleHistoryInfoReq::Reset()
{
    history_world_id = int64();    
}

void FPbGetBattleHistoryInfoReq::operator=(const idlepb::GetBattleHistoryInfoReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetBattleHistoryInfoReq::operator==(const FPbGetBattleHistoryInfoReq& Right) const
{
    if (this->history_world_id != Right.history_world_id)
        return false;
    return true;
}

bool FPbGetBattleHistoryInfoReq::operator!=(const FPbGetBattleHistoryInfoReq& Right) const
{
    return !operator==(Right);
}

FPbGetBattleHistoryInfoAck::FPbGetBattleHistoryInfoAck()
{
    Reset();        
}

FPbGetBattleHistoryInfoAck::FPbGetBattleHistoryInfoAck(const idlepb::GetBattleHistoryInfoAck& Right)
{
    this->FromPb(Right);
}

void FPbGetBattleHistoryInfoAck::FromPb(const idlepb::GetBattleHistoryInfoAck& Right)
{
    ok = Right.ok();
    info = Right.info();
}

void FPbGetBattleHistoryInfoAck::ToPb(idlepb::GetBattleHistoryInfoAck* Out) const
{
    Out->set_ok(ok);
    info.ToPb(Out->mutable_info());    
}

void FPbGetBattleHistoryInfoAck::Reset()
{
    ok = bool();
    info = FPbBattleInfo();    
}

void FPbGetBattleHistoryInfoAck::operator=(const idlepb::GetBattleHistoryInfoAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetBattleHistoryInfoAck::operator==(const FPbGetBattleHistoryInfoAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    if (this->info != Right.info)
        return false;
    return true;
}

bool FPbGetBattleHistoryInfoAck::operator!=(const FPbGetBattleHistoryInfoAck& Right) const
{
    return !operator==(Right);
}

FPbNotifyEnterOpenClientWorld::FPbNotifyEnterOpenClientWorld()
{
    Reset();        
}

FPbNotifyEnterOpenClientWorld::FPbNotifyEnterOpenClientWorld(const idlepb::NotifyEnterOpenClientWorld& Right)
{
    this->FromPb(Right);
}

void FPbNotifyEnterOpenClientWorld::FromPb(const idlepb::NotifyEnterOpenClientWorld& Right)
{
    world_cfg_id = Right.world_cfg_id();
}

void FPbNotifyEnterOpenClientWorld::ToPb(idlepb::NotifyEnterOpenClientWorld* Out) const
{
    Out->set_world_cfg_id(world_cfg_id);    
}

void FPbNotifyEnterOpenClientWorld::Reset()
{
    world_cfg_id = int32();    
}

void FPbNotifyEnterOpenClientWorld::operator=(const idlepb::NotifyEnterOpenClientWorld& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyEnterOpenClientWorld::operator==(const FPbNotifyEnterOpenClientWorld& Right) const
{
    if (this->world_cfg_id != Right.world_cfg_id)
        return false;
    return true;
}

bool FPbNotifyEnterOpenClientWorld::operator!=(const FPbNotifyEnterOpenClientWorld& Right) const
{
    return !operator==(Right);
}

FPbNotifyMonsterTowerData::FPbNotifyMonsterTowerData()
{
    Reset();        
}

FPbNotifyMonsterTowerData::FPbNotifyMonsterTowerData(const idlepb::NotifyMonsterTowerData& Right)
{
    this->FromPb(Right);
}

void FPbNotifyMonsterTowerData::FromPb(const idlepb::NotifyMonsterTowerData& Right)
{
    data = Right.data();
}

void FPbNotifyMonsterTowerData::ToPb(idlepb::NotifyMonsterTowerData* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbNotifyMonsterTowerData::Reset()
{
    data = FPbRoleMonsterTowerData();    
}

void FPbNotifyMonsterTowerData::operator=(const idlepb::NotifyMonsterTowerData& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyMonsterTowerData::operator==(const FPbNotifyMonsterTowerData& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbNotifyMonsterTowerData::operator!=(const FPbNotifyMonsterTowerData& Right) const
{
    return !operator==(Right);
}

FPbNotifyMonsterTowerChallengeOver::FPbNotifyMonsterTowerChallengeOver()
{
    Reset();        
}

FPbNotifyMonsterTowerChallengeOver::FPbNotifyMonsterTowerChallengeOver(const idlepb::NotifyMonsterTowerChallengeOver& Right)
{
    this->FromPb(Right);
}

void FPbNotifyMonsterTowerChallengeOver::FromPb(const idlepb::NotifyMonsterTowerChallengeOver& Right)
{
    floor = Right.floor();
    win = Right.win();
}

void FPbNotifyMonsterTowerChallengeOver::ToPb(idlepb::NotifyMonsterTowerChallengeOver* Out) const
{
    Out->set_floor(floor);
    Out->set_win(win);    
}

void FPbNotifyMonsterTowerChallengeOver::Reset()
{
    floor = int32();
    win = bool();    
}

void FPbNotifyMonsterTowerChallengeOver::operator=(const idlepb::NotifyMonsterTowerChallengeOver& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyMonsterTowerChallengeOver::operator==(const FPbNotifyMonsterTowerChallengeOver& Right) const
{
    if (this->floor != Right.floor)
        return false;
    if (this->win != Right.win)
        return false;
    return true;
}

bool FPbNotifyMonsterTowerChallengeOver::operator!=(const FPbNotifyMonsterTowerChallengeOver& Right) const
{
    return !operator==(Right);
}

FPbMonsterTowerChallengeReq::FPbMonsterTowerChallengeReq()
{
    Reset();        
}

FPbMonsterTowerChallengeReq::FPbMonsterTowerChallengeReq(const idlepb::MonsterTowerChallengeReq& Right)
{
    this->FromPb(Right);
}

void FPbMonsterTowerChallengeReq::FromPb(const idlepb::MonsterTowerChallengeReq& Right)
{
}

void FPbMonsterTowerChallengeReq::ToPb(idlepb::MonsterTowerChallengeReq* Out) const
{    
}

void FPbMonsterTowerChallengeReq::Reset()
{    
}

void FPbMonsterTowerChallengeReq::operator=(const idlepb::MonsterTowerChallengeReq& Right)
{
    this->FromPb(Right);
}

bool FPbMonsterTowerChallengeReq::operator==(const FPbMonsterTowerChallengeReq& Right) const
{
    return true;
}

bool FPbMonsterTowerChallengeReq::operator!=(const FPbMonsterTowerChallengeReq& Right) const
{
    return !operator==(Right);
}

FPbMonsterTowerChallengeAck::FPbMonsterTowerChallengeAck()
{
    Reset();        
}

FPbMonsterTowerChallengeAck::FPbMonsterTowerChallengeAck(const idlepb::MonsterTowerChallengeAck& Right)
{
    this->FromPb(Right);
}

void FPbMonsterTowerChallengeAck::FromPb(const idlepb::MonsterTowerChallengeAck& Right)
{
}

void FPbMonsterTowerChallengeAck::ToPb(idlepb::MonsterTowerChallengeAck* Out) const
{    
}

void FPbMonsterTowerChallengeAck::Reset()
{    
}

void FPbMonsterTowerChallengeAck::operator=(const idlepb::MonsterTowerChallengeAck& Right)
{
    this->FromPb(Right);
}

bool FPbMonsterTowerChallengeAck::operator==(const FPbMonsterTowerChallengeAck& Right) const
{
    return true;
}

bool FPbMonsterTowerChallengeAck::operator!=(const FPbMonsterTowerChallengeAck& Right) const
{
    return !operator==(Right);
}

FPbMonsterTowerDrawIdleAwardReq::FPbMonsterTowerDrawIdleAwardReq()
{
    Reset();        
}

FPbMonsterTowerDrawIdleAwardReq::FPbMonsterTowerDrawIdleAwardReq(const idlepb::MonsterTowerDrawIdleAwardReq& Right)
{
    this->FromPb(Right);
}

void FPbMonsterTowerDrawIdleAwardReq::FromPb(const idlepb::MonsterTowerDrawIdleAwardReq& Right)
{
}

void FPbMonsterTowerDrawIdleAwardReq::ToPb(idlepb::MonsterTowerDrawIdleAwardReq* Out) const
{    
}

void FPbMonsterTowerDrawIdleAwardReq::Reset()
{    
}

void FPbMonsterTowerDrawIdleAwardReq::operator=(const idlepb::MonsterTowerDrawIdleAwardReq& Right)
{
    this->FromPb(Right);
}

bool FPbMonsterTowerDrawIdleAwardReq::operator==(const FPbMonsterTowerDrawIdleAwardReq& Right) const
{
    return true;
}

bool FPbMonsterTowerDrawIdleAwardReq::operator!=(const FPbMonsterTowerDrawIdleAwardReq& Right) const
{
    return !operator==(Right);
}

FPbMonsterTowerDrawIdleAwardAck::FPbMonsterTowerDrawIdleAwardAck()
{
    Reset();        
}

FPbMonsterTowerDrawIdleAwardAck::FPbMonsterTowerDrawIdleAwardAck(const idlepb::MonsterTowerDrawIdleAwardAck& Right)
{
    this->FromPb(Right);
}

void FPbMonsterTowerDrawIdleAwardAck::FromPb(const idlepb::MonsterTowerDrawIdleAwardAck& Right)
{
}

void FPbMonsterTowerDrawIdleAwardAck::ToPb(idlepb::MonsterTowerDrawIdleAwardAck* Out) const
{    
}

void FPbMonsterTowerDrawIdleAwardAck::Reset()
{    
}

void FPbMonsterTowerDrawIdleAwardAck::operator=(const idlepb::MonsterTowerDrawIdleAwardAck& Right)
{
    this->FromPb(Right);
}

bool FPbMonsterTowerDrawIdleAwardAck::operator==(const FPbMonsterTowerDrawIdleAwardAck& Right) const
{
    return true;
}

bool FPbMonsterTowerDrawIdleAwardAck::operator!=(const FPbMonsterTowerDrawIdleAwardAck& Right) const
{
    return !operator==(Right);
}

FPbMonsterTowerClosedDoorTrainingReq::FPbMonsterTowerClosedDoorTrainingReq()
{
    Reset();        
}

FPbMonsterTowerClosedDoorTrainingReq::FPbMonsterTowerClosedDoorTrainingReq(const idlepb::MonsterTowerClosedDoorTrainingReq& Right)
{
    this->FromPb(Right);
}

void FPbMonsterTowerClosedDoorTrainingReq::FromPb(const idlepb::MonsterTowerClosedDoorTrainingReq& Right)
{
}

void FPbMonsterTowerClosedDoorTrainingReq::ToPb(idlepb::MonsterTowerClosedDoorTrainingReq* Out) const
{    
}

void FPbMonsterTowerClosedDoorTrainingReq::Reset()
{    
}

void FPbMonsterTowerClosedDoorTrainingReq::operator=(const idlepb::MonsterTowerClosedDoorTrainingReq& Right)
{
    this->FromPb(Right);
}

bool FPbMonsterTowerClosedDoorTrainingReq::operator==(const FPbMonsterTowerClosedDoorTrainingReq& Right) const
{
    return true;
}

bool FPbMonsterTowerClosedDoorTrainingReq::operator!=(const FPbMonsterTowerClosedDoorTrainingReq& Right) const
{
    return !operator==(Right);
}

FPbMonsterTowerClosedDoorTrainingAck::FPbMonsterTowerClosedDoorTrainingAck()
{
    Reset();        
}

FPbMonsterTowerClosedDoorTrainingAck::FPbMonsterTowerClosedDoorTrainingAck(const idlepb::MonsterTowerClosedDoorTrainingAck& Right)
{
    this->FromPb(Right);
}

void FPbMonsterTowerClosedDoorTrainingAck::FromPb(const idlepb::MonsterTowerClosedDoorTrainingAck& Right)
{
}

void FPbMonsterTowerClosedDoorTrainingAck::ToPb(idlepb::MonsterTowerClosedDoorTrainingAck* Out) const
{    
}

void FPbMonsterTowerClosedDoorTrainingAck::Reset()
{    
}

void FPbMonsterTowerClosedDoorTrainingAck::operator=(const idlepb::MonsterTowerClosedDoorTrainingAck& Right)
{
    this->FromPb(Right);
}

bool FPbMonsterTowerClosedDoorTrainingAck::operator==(const FPbMonsterTowerClosedDoorTrainingAck& Right) const
{
    return true;
}

bool FPbMonsterTowerClosedDoorTrainingAck::operator!=(const FPbMonsterTowerClosedDoorTrainingAck& Right) const
{
    return !operator==(Right);
}

FPbMonsterTowerQuickEndReq::FPbMonsterTowerQuickEndReq()
{
    Reset();        
}

FPbMonsterTowerQuickEndReq::FPbMonsterTowerQuickEndReq(const idlepb::MonsterTowerQuickEndReq& Right)
{
    this->FromPb(Right);
}

void FPbMonsterTowerQuickEndReq::FromPb(const idlepb::MonsterTowerQuickEndReq& Right)
{
    is_exit = Right.is_exit();
}

void FPbMonsterTowerQuickEndReq::ToPb(idlepb::MonsterTowerQuickEndReq* Out) const
{
    Out->set_is_exit(is_exit);    
}

void FPbMonsterTowerQuickEndReq::Reset()
{
    is_exit = bool();    
}

void FPbMonsterTowerQuickEndReq::operator=(const idlepb::MonsterTowerQuickEndReq& Right)
{
    this->FromPb(Right);
}

bool FPbMonsterTowerQuickEndReq::operator==(const FPbMonsterTowerQuickEndReq& Right) const
{
    if (this->is_exit != Right.is_exit)
        return false;
    return true;
}

bool FPbMonsterTowerQuickEndReq::operator!=(const FPbMonsterTowerQuickEndReq& Right) const
{
    return !operator==(Right);
}

FPbMonsterTowerQuickEndAck::FPbMonsterTowerQuickEndAck()
{
    Reset();        
}

FPbMonsterTowerQuickEndAck::FPbMonsterTowerQuickEndAck(const idlepb::MonsterTowerQuickEndAck& Right)
{
    this->FromPb(Right);
}

void FPbMonsterTowerQuickEndAck::FromPb(const idlepb::MonsterTowerQuickEndAck& Right)
{
}

void FPbMonsterTowerQuickEndAck::ToPb(idlepb::MonsterTowerQuickEndAck* Out) const
{    
}

void FPbMonsterTowerQuickEndAck::Reset()
{    
}

void FPbMonsterTowerQuickEndAck::operator=(const idlepb::MonsterTowerQuickEndAck& Right)
{
    this->FromPb(Right);
}

bool FPbMonsterTowerQuickEndAck::operator==(const FPbMonsterTowerQuickEndAck& Right) const
{
    return true;
}

bool FPbMonsterTowerQuickEndAck::operator!=(const FPbMonsterTowerQuickEndAck& Right) const
{
    return !operator==(Right);
}

FPbNotifyFightModeData::FPbNotifyFightModeData()
{
    Reset();        
}

FPbNotifyFightModeData::FPbNotifyFightModeData(const idlepb::NotifyFightModeData& Right)
{
    this->FromPb(Right);
}

void FPbNotifyFightModeData::FromPb(const idlepb::NotifyFightModeData& Right)
{
    data = Right.data();
}

void FPbNotifyFightModeData::ToPb(idlepb::NotifyFightModeData* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbNotifyFightModeData::Reset()
{
    data = FPbRoleFightModeData();    
}

void FPbNotifyFightModeData::operator=(const idlepb::NotifyFightModeData& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyFightModeData::operator==(const FPbNotifyFightModeData& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbNotifyFightModeData::operator!=(const FPbNotifyFightModeData& Right) const
{
    return !operator==(Right);
}

bool CheckEPbSetFightModeAckErrorCodeValid(int32 Val)
{
    return idlepb::SetFightModeAckErrorCode_IsValid(Val);
}

const TCHAR* GetEPbSetFightModeAckErrorCodeDescription(EPbSetFightModeAckErrorCode Val)
{
    switch (Val)
    {
        case EPbSetFightModeAckErrorCode::SetFightModeAckErrorCode_Ok: return TEXT("成功");
        case EPbSetFightModeAckErrorCode::SetFightModeAckErrorCode_Other: return TEXT("其它错误");
        case EPbSetFightModeAckErrorCode::SetFightModeAckErrorCode_RankInvalid: return TEXT("等级错误");
        case EPbSetFightModeAckErrorCode::SetFightModeAckErrorCode_ModeInvalid: return TEXT("该模式不能在当前地图使用");
        case EPbSetFightModeAckErrorCode::SetFightModeAckErrorCode_FightTime: return TEXT("战斗时间错误");
    }
    return TEXT("UNKNOWN");
}

FPbSetFightModeReq::FPbSetFightModeReq()
{
    Reset();        
}

FPbSetFightModeReq::FPbSetFightModeReq(const idlepb::SetFightModeReq& Right)
{
    this->FromPb(Right);
}

void FPbSetFightModeReq::FromPb(const idlepb::SetFightModeReq& Right)
{
    mode = static_cast<EPbFightMode>(Right.mode());
}

void FPbSetFightModeReq::ToPb(idlepb::SetFightModeReq* Out) const
{
    Out->set_mode(static_cast<idlepb::FightMode>(mode));    
}

void FPbSetFightModeReq::Reset()
{
    mode = EPbFightMode();    
}

void FPbSetFightModeReq::operator=(const idlepb::SetFightModeReq& Right)
{
    this->FromPb(Right);
}

bool FPbSetFightModeReq::operator==(const FPbSetFightModeReq& Right) const
{
    if (this->mode != Right.mode)
        return false;
    return true;
}

bool FPbSetFightModeReq::operator!=(const FPbSetFightModeReq& Right) const
{
    return !operator==(Right);
}

FPbSetFightModeAck::FPbSetFightModeAck()
{
    Reset();        
}

FPbSetFightModeAck::FPbSetFightModeAck(const idlepb::SetFightModeAck& Right)
{
    this->FromPb(Right);
}

void FPbSetFightModeAck::FromPb(const idlepb::SetFightModeAck& Right)
{
    ok = Right.ok();
    error_code = static_cast<EPbSetFightModeAckErrorCode>(Right.error_code());
}

void FPbSetFightModeAck::ToPb(idlepb::SetFightModeAck* Out) const
{
    Out->set_ok(ok);
    Out->set_error_code(static_cast<idlepb::SetFightModeAckErrorCode>(error_code));    
}

void FPbSetFightModeAck::Reset()
{
    ok = bool();
    error_code = EPbSetFightModeAckErrorCode();    
}

void FPbSetFightModeAck::operator=(const idlepb::SetFightModeAck& Right)
{
    this->FromPb(Right);
}

bool FPbSetFightModeAck::operator==(const FPbSetFightModeAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    if (this->error_code != Right.error_code)
        return false;
    return true;
}

bool FPbSetFightModeAck::operator!=(const FPbSetFightModeAck& Right) const
{
    return !operator==(Right);
}

FPbNotifyInventorySpaceNum::FPbNotifyInventorySpaceNum()
{
    Reset();        
}

FPbNotifyInventorySpaceNum::FPbNotifyInventorySpaceNum(const idlepb::NotifyInventorySpaceNum& Right)
{
    this->FromPb(Right);
}

void FPbNotifyInventorySpaceNum::FromPb(const idlepb::NotifyInventorySpaceNum& Right)
{
    num = Right.num();
}

void FPbNotifyInventorySpaceNum::ToPb(idlepb::NotifyInventorySpaceNum* Out) const
{
    Out->set_num(num);    
}

void FPbNotifyInventorySpaceNum::Reset()
{
    num = int32();    
}

void FPbNotifyInventorySpaceNum::operator=(const idlepb::NotifyInventorySpaceNum& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyInventorySpaceNum::operator==(const FPbNotifyInventorySpaceNum& Right) const
{
    if (this->num != Right.num)
        return false;
    return true;
}

bool FPbNotifyInventorySpaceNum::operator!=(const FPbNotifyInventorySpaceNum& Right) const
{
    return !operator==(Right);
}

FPbNotifyInventoryFullMailItem::FPbNotifyInventoryFullMailItem()
{
    Reset();        
}

FPbNotifyInventoryFullMailItem::FPbNotifyInventoryFullMailItem(const idlepb::NotifyInventoryFullMailItem& Right)
{
    this->FromPb(Right);
}

void FPbNotifyInventoryFullMailItem::FromPb(const idlepb::NotifyInventoryFullMailItem& Right)
{
}

void FPbNotifyInventoryFullMailItem::ToPb(idlepb::NotifyInventoryFullMailItem* Out) const
{    
}

void FPbNotifyInventoryFullMailItem::Reset()
{    
}

void FPbNotifyInventoryFullMailItem::operator=(const idlepb::NotifyInventoryFullMailItem& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyInventoryFullMailItem::operator==(const FPbNotifyInventoryFullMailItem& Right) const
{
    return true;
}

bool FPbNotifyInventoryFullMailItem::operator!=(const FPbNotifyInventoryFullMailItem& Right) const
{
    return !operator==(Right);
}

FPbNotifyQiCollectorRank::FPbNotifyQiCollectorRank()
{
    Reset();        
}

FPbNotifyQiCollectorRank::FPbNotifyQiCollectorRank(const idlepb::NotifyQiCollectorRank& Right)
{
    this->FromPb(Right);
}

void FPbNotifyQiCollectorRank::FromPb(const idlepb::NotifyQiCollectorRank& Right)
{
    rank = Right.rank();
}

void FPbNotifyQiCollectorRank::ToPb(idlepb::NotifyQiCollectorRank* Out) const
{
    Out->set_rank(rank);    
}

void FPbNotifyQiCollectorRank::Reset()
{
    rank = int32();    
}

void FPbNotifyQiCollectorRank::operator=(const idlepb::NotifyQiCollectorRank& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyQiCollectorRank::operator==(const FPbNotifyQiCollectorRank& Right) const
{
    if (this->rank != Right.rank)
        return false;
    return true;
}

bool FPbNotifyQiCollectorRank::operator!=(const FPbNotifyQiCollectorRank& Right) const
{
    return !operator==(Right);
}

FPbUpgradeQiCollectorReq::FPbUpgradeQiCollectorReq()
{
    Reset();        
}

FPbUpgradeQiCollectorReq::FPbUpgradeQiCollectorReq(const idlepb::UpgradeQiCollectorReq& Right)
{
    this->FromPb(Right);
}

void FPbUpgradeQiCollectorReq::FromPb(const idlepb::UpgradeQiCollectorReq& Right)
{
}

void FPbUpgradeQiCollectorReq::ToPb(idlepb::UpgradeQiCollectorReq* Out) const
{    
}

void FPbUpgradeQiCollectorReq::Reset()
{    
}

void FPbUpgradeQiCollectorReq::operator=(const idlepb::UpgradeQiCollectorReq& Right)
{
    this->FromPb(Right);
}

bool FPbUpgradeQiCollectorReq::operator==(const FPbUpgradeQiCollectorReq& Right) const
{
    return true;
}

bool FPbUpgradeQiCollectorReq::operator!=(const FPbUpgradeQiCollectorReq& Right) const
{
    return !operator==(Right);
}

FPbUpgradeQiCollectorAck::FPbUpgradeQiCollectorAck()
{
    Reset();        
}

FPbUpgradeQiCollectorAck::FPbUpgradeQiCollectorAck(const idlepb::UpgradeQiCollectorAck& Right)
{
    this->FromPb(Right);
}

void FPbUpgradeQiCollectorAck::FromPb(const idlepb::UpgradeQiCollectorAck& Right)
{
    ok = Right.ok();
}

void FPbUpgradeQiCollectorAck::ToPb(idlepb::UpgradeQiCollectorAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbUpgradeQiCollectorAck::Reset()
{
    ok = bool();    
}

void FPbUpgradeQiCollectorAck::operator=(const idlepb::UpgradeQiCollectorAck& Right)
{
    this->FromPb(Right);
}

bool FPbUpgradeQiCollectorAck::operator==(const FPbUpgradeQiCollectorAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbUpgradeQiCollectorAck::operator!=(const FPbUpgradeQiCollectorAck& Right) const
{
    return !operator==(Right);
}

FPbGetRoleAllStatsReq::FPbGetRoleAllStatsReq()
{
    Reset();        
}

FPbGetRoleAllStatsReq::FPbGetRoleAllStatsReq(const idlepb::GetRoleAllStatsReq& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleAllStatsReq::FromPb(const idlepb::GetRoleAllStatsReq& Right)
{
}

void FPbGetRoleAllStatsReq::ToPb(idlepb::GetRoleAllStatsReq* Out) const
{    
}

void FPbGetRoleAllStatsReq::Reset()
{    
}

void FPbGetRoleAllStatsReq::operator=(const idlepb::GetRoleAllStatsReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleAllStatsReq::operator==(const FPbGetRoleAllStatsReq& Right) const
{
    return true;
}

bool FPbGetRoleAllStatsReq::operator!=(const FPbGetRoleAllStatsReq& Right) const
{
    return !operator==(Right);
}

FPbGetRoleAllStatsAck::FPbGetRoleAllStatsAck()
{
    Reset();        
}

FPbGetRoleAllStatsAck::FPbGetRoleAllStatsAck(const idlepb::GetRoleAllStatsAck& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleAllStatsAck::FromPb(const idlepb::GetRoleAllStatsAck& Right)
{
    all_stats_data = Right.all_stats_data();
}

void FPbGetRoleAllStatsAck::ToPb(idlepb::GetRoleAllStatsAck* Out) const
{
    all_stats_data.ToPb(Out->mutable_all_stats_data());    
}

void FPbGetRoleAllStatsAck::Reset()
{
    all_stats_data = FPbGameStatsAllModuleData();    
}

void FPbGetRoleAllStatsAck::operator=(const idlepb::GetRoleAllStatsAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleAllStatsAck::operator==(const FPbGetRoleAllStatsAck& Right) const
{
    if (this->all_stats_data != Right.all_stats_data)
        return false;
    return true;
}

bool FPbGetRoleAllStatsAck::operator!=(const FPbGetRoleAllStatsAck& Right) const
{
    return !operator==(Right);
}

FPbGetShanhetuDataReq::FPbGetShanhetuDataReq()
{
    Reset();        
}

FPbGetShanhetuDataReq::FPbGetShanhetuDataReq(const idlepb::GetShanhetuDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetShanhetuDataReq::FromPb(const idlepb::GetShanhetuDataReq& Right)
{
}

void FPbGetShanhetuDataReq::ToPb(idlepb::GetShanhetuDataReq* Out) const
{    
}

void FPbGetShanhetuDataReq::Reset()
{    
}

void FPbGetShanhetuDataReq::operator=(const idlepb::GetShanhetuDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetShanhetuDataReq::operator==(const FPbGetShanhetuDataReq& Right) const
{
    return true;
}

bool FPbGetShanhetuDataReq::operator!=(const FPbGetShanhetuDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetShanhetuDataAck::FPbGetShanhetuDataAck()
{
    Reset();        
}

FPbGetShanhetuDataAck::FPbGetShanhetuDataAck(const idlepb::GetShanhetuDataAck& Right)
{
    this->FromPb(Right);
}

void FPbGetShanhetuDataAck::FromPb(const idlepb::GetShanhetuDataAck& Right)
{
    data = Right.data();
}

void FPbGetShanhetuDataAck::ToPb(idlepb::GetShanhetuDataAck* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbGetShanhetuDataAck::Reset()
{
    data = FPbRoleShanhetuData();    
}

void FPbGetShanhetuDataAck::operator=(const idlepb::GetShanhetuDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetShanhetuDataAck::operator==(const FPbGetShanhetuDataAck& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbGetShanhetuDataAck::operator!=(const FPbGetShanhetuDataAck& Right) const
{
    return !operator==(Right);
}

FPbSetShanhetuUseConfigReq::FPbSetShanhetuUseConfigReq()
{
    Reset();        
}

FPbSetShanhetuUseConfigReq::FPbSetShanhetuUseConfigReq(const idlepb::SetShanhetuUseConfigReq& Right)
{
    this->FromPb(Right);
}

void FPbSetShanhetuUseConfigReq::FromPb(const idlepb::SetShanhetuUseConfigReq& Right)
{
    auto_skip_green = Right.auto_skip_green();
    auto_skip_blue = Right.auto_skip_blue();
    auto_skip_perpo = Right.auto_skip_perpo();
    auto_skip_gold = Right.auto_skip_gold();
    auto_skip_red = Right.auto_skip_red();
    auto_select = Right.auto_select();
}

void FPbSetShanhetuUseConfigReq::ToPb(idlepb::SetShanhetuUseConfigReq* Out) const
{
    Out->set_auto_skip_green(auto_skip_green);
    Out->set_auto_skip_blue(auto_skip_blue);
    Out->set_auto_skip_perpo(auto_skip_perpo);
    Out->set_auto_skip_gold(auto_skip_gold);
    Out->set_auto_skip_red(auto_skip_red);
    Out->set_auto_select(auto_select);    
}

void FPbSetShanhetuUseConfigReq::Reset()
{
    auto_skip_green = bool();
    auto_skip_blue = bool();
    auto_skip_perpo = bool();
    auto_skip_gold = bool();
    auto_skip_red = bool();
    auto_select = int32();    
}

void FPbSetShanhetuUseConfigReq::operator=(const idlepb::SetShanhetuUseConfigReq& Right)
{
    this->FromPb(Right);
}

bool FPbSetShanhetuUseConfigReq::operator==(const FPbSetShanhetuUseConfigReq& Right) const
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
    return true;
}

bool FPbSetShanhetuUseConfigReq::operator!=(const FPbSetShanhetuUseConfigReq& Right) const
{
    return !operator==(Right);
}

FPbSetShanhetuUseConfigAck::FPbSetShanhetuUseConfigAck()
{
    Reset();        
}

FPbSetShanhetuUseConfigAck::FPbSetShanhetuUseConfigAck(const idlepb::SetShanhetuUseConfigAck& Right)
{
    this->FromPb(Right);
}

void FPbSetShanhetuUseConfigAck::FromPb(const idlepb::SetShanhetuUseConfigAck& Right)
{
    ok = Right.ok();
}

void FPbSetShanhetuUseConfigAck::ToPb(idlepb::SetShanhetuUseConfigAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbSetShanhetuUseConfigAck::Reset()
{
    ok = bool();    
}

void FPbSetShanhetuUseConfigAck::operator=(const idlepb::SetShanhetuUseConfigAck& Right)
{
    this->FromPb(Right);
}

bool FPbSetShanhetuUseConfigAck::operator==(const FPbSetShanhetuUseConfigAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbSetShanhetuUseConfigAck::operator!=(const FPbSetShanhetuUseConfigAck& Right) const
{
    return !operator==(Right);
}

FPbUseShanhetuReq::FPbUseShanhetuReq()
{
    Reset();        
}

FPbUseShanhetuReq::FPbUseShanhetuReq(const idlepb::UseShanhetuReq& Right)
{
    this->FromPb(Right);
}

void FPbUseShanhetuReq::FromPb(const idlepb::UseShanhetuReq& Right)
{
    item_id = Right.item_id();
    skip = Right.skip();
    num = Right.num();
}

void FPbUseShanhetuReq::ToPb(idlepb::UseShanhetuReq* Out) const
{
    Out->set_item_id(item_id);
    Out->set_skip(skip);
    Out->set_num(num);    
}

void FPbUseShanhetuReq::Reset()
{
    item_id = int32();
    skip = bool();
    num = int32();    
}

void FPbUseShanhetuReq::operator=(const idlepb::UseShanhetuReq& Right)
{
    this->FromPb(Right);
}

bool FPbUseShanhetuReq::operator==(const FPbUseShanhetuReq& Right) const
{
    if (this->item_id != Right.item_id)
        return false;
    if (this->skip != Right.skip)
        return false;
    if (this->num != Right.num)
        return false;
    return true;
}

bool FPbUseShanhetuReq::operator!=(const FPbUseShanhetuReq& Right) const
{
    return !operator==(Right);
}

FPbUseShanhetuAck::FPbUseShanhetuAck()
{
    Reset();        
}

FPbUseShanhetuAck::FPbUseShanhetuAck(const idlepb::UseShanhetuAck& Right)
{
    this->FromPb(Right);
}

void FPbUseShanhetuAck::FromPb(const idlepb::UseShanhetuAck& Right)
{
    ok = Right.ok();
    items.Empty();
    for (const auto& Elem : Right.items())
    {
        items.Emplace(Elem);
    }
    map = Right.map();
}

void FPbUseShanhetuAck::ToPb(idlepb::UseShanhetuAck* Out) const
{
    Out->set_ok(ok);
    for (const auto& Elem : items)
    {
        Elem.ToPb(Out->add_items());    
    }
    map.ToPb(Out->mutable_map());    
}

void FPbUseShanhetuAck::Reset()
{
    ok = bool();
    items = TArray<FPbSimpleItemData>();
    map = FPbShanhetuMap();    
}

void FPbUseShanhetuAck::operator=(const idlepb::UseShanhetuAck& Right)
{
    this->FromPb(Right);
}

bool FPbUseShanhetuAck::operator==(const FPbUseShanhetuAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    if (this->items != Right.items)
        return false;
    if (this->map != Right.map)
        return false;
    return true;
}

bool FPbUseShanhetuAck::operator!=(const FPbUseShanhetuAck& Right) const
{
    return !operator==(Right);
}

FPbStepShanhetuReq::FPbStepShanhetuReq()
{
    Reset();        
}

FPbStepShanhetuReq::FPbStepShanhetuReq(const idlepb::StepShanhetuReq& Right)
{
    this->FromPb(Right);
}

void FPbStepShanhetuReq::FromPb(const idlepb::StepShanhetuReq& Right)
{
    choose_event = Right.choose_event();
}

void FPbStepShanhetuReq::ToPb(idlepb::StepShanhetuReq* Out) const
{
    Out->set_choose_event(choose_event);    
}

void FPbStepShanhetuReq::Reset()
{
    choose_event = int32();    
}

void FPbStepShanhetuReq::operator=(const idlepb::StepShanhetuReq& Right)
{
    this->FromPb(Right);
}

bool FPbStepShanhetuReq::operator==(const FPbStepShanhetuReq& Right) const
{
    if (this->choose_event != Right.choose_event)
        return false;
    return true;
}

bool FPbStepShanhetuReq::operator!=(const FPbStepShanhetuReq& Right) const
{
    return !operator==(Right);
}

FPbStepShanhetuAck::FPbStepShanhetuAck()
{
    Reset();        
}

FPbStepShanhetuAck::FPbStepShanhetuAck(const idlepb::StepShanhetuAck& Right)
{
    this->FromPb(Right);
}

void FPbStepShanhetuAck::FromPb(const idlepb::StepShanhetuAck& Right)
{
    done = Right.done();
    current_row = Right.current_row();
    record = Right.record();
}

void FPbStepShanhetuAck::ToPb(idlepb::StepShanhetuAck* Out) const
{
    Out->set_done(done);
    Out->set_current_row(current_row);
    record.ToPb(Out->mutable_record());    
}

void FPbStepShanhetuAck::Reset()
{
    done = bool();
    current_row = int32();
    record = FPbShanhetuRecord();    
}

void FPbStepShanhetuAck::operator=(const idlepb::StepShanhetuAck& Right)
{
    this->FromPb(Right);
}

bool FPbStepShanhetuAck::operator==(const FPbStepShanhetuAck& Right) const
{
    if (this->done != Right.done)
        return false;
    if (this->current_row != Right.current_row)
        return false;
    if (this->record != Right.record)
        return false;
    return true;
}

bool FPbStepShanhetuAck::operator!=(const FPbStepShanhetuAck& Right) const
{
    return !operator==(Right);
}

FPbGetShanhetuUseRecordReq::FPbGetShanhetuUseRecordReq()
{
    Reset();        
}

FPbGetShanhetuUseRecordReq::FPbGetShanhetuUseRecordReq(const idlepb::GetShanhetuUseRecordReq& Right)
{
    this->FromPb(Right);
}

void FPbGetShanhetuUseRecordReq::FromPb(const idlepb::GetShanhetuUseRecordReq& Right)
{
    role_id = Right.role_id();
    uid = Right.uid();
}

void FPbGetShanhetuUseRecordReq::ToPb(idlepb::GetShanhetuUseRecordReq* Out) const
{
    Out->set_role_id(role_id);
    Out->set_uid(uid);    
}

void FPbGetShanhetuUseRecordReq::Reset()
{
    role_id = int64();
    uid = int64();    
}

void FPbGetShanhetuUseRecordReq::operator=(const idlepb::GetShanhetuUseRecordReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetShanhetuUseRecordReq::operator==(const FPbGetShanhetuUseRecordReq& Right) const
{
    if (this->role_id != Right.role_id)
        return false;
    if (this->uid != Right.uid)
        return false;
    return true;
}

bool FPbGetShanhetuUseRecordReq::operator!=(const FPbGetShanhetuUseRecordReq& Right) const
{
    return !operator==(Right);
}

FPbGetShanhetuUseRecordAck::FPbGetShanhetuUseRecordAck()
{
    Reset();        
}

FPbGetShanhetuUseRecordAck::FPbGetShanhetuUseRecordAck(const idlepb::GetShanhetuUseRecordAck& Right)
{
    this->FromPb(Right);
}

void FPbGetShanhetuUseRecordAck::FromPb(const idlepb::GetShanhetuUseRecordAck& Right)
{
    record = Right.record();
}

void FPbGetShanhetuUseRecordAck::ToPb(idlepb::GetShanhetuUseRecordAck* Out) const
{
    record.ToPb(Out->mutable_record());    
}

void FPbGetShanhetuUseRecordAck::Reset()
{
    record = FPbShanhetuRecord();    
}

void FPbGetShanhetuUseRecordAck::operator=(const idlepb::GetShanhetuUseRecordAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetShanhetuUseRecordAck::operator==(const FPbGetShanhetuUseRecordAck& Right) const
{
    if (this->record != Right.record)
        return false;
    return true;
}

bool FPbGetShanhetuUseRecordAck::operator!=(const FPbGetShanhetuUseRecordAck& Right) const
{
    return !operator==(Right);
}

FPbSetAttackLockTypeReq::FPbSetAttackLockTypeReq()
{
    Reset();        
}

FPbSetAttackLockTypeReq::FPbSetAttackLockTypeReq(const idlepb::SetAttackLockTypeReq& Right)
{
    this->FromPb(Right);
}

void FPbSetAttackLockTypeReq::FromPb(const idlepb::SetAttackLockTypeReq& Right)
{
    type = static_cast<EPbAttackLockType>(Right.type());
}

void FPbSetAttackLockTypeReq::ToPb(idlepb::SetAttackLockTypeReq* Out) const
{
    Out->set_type(static_cast<idlepb::AttackLockType>(type));    
}

void FPbSetAttackLockTypeReq::Reset()
{
    type = EPbAttackLockType();    
}

void FPbSetAttackLockTypeReq::operator=(const idlepb::SetAttackLockTypeReq& Right)
{
    this->FromPb(Right);
}

bool FPbSetAttackLockTypeReq::operator==(const FPbSetAttackLockTypeReq& Right) const
{
    if (this->type != Right.type)
        return false;
    return true;
}

bool FPbSetAttackLockTypeReq::operator!=(const FPbSetAttackLockTypeReq& Right) const
{
    return !operator==(Right);
}

FPbSetAttackLockTypeAck::FPbSetAttackLockTypeAck()
{
    Reset();        
}

FPbSetAttackLockTypeAck::FPbSetAttackLockTypeAck(const idlepb::SetAttackLockTypeAck& Right)
{
    this->FromPb(Right);
}

void FPbSetAttackLockTypeAck::FromPb(const idlepb::SetAttackLockTypeAck& Right)
{
    ok = Right.ok();
}

void FPbSetAttackLockTypeAck::ToPb(idlepb::SetAttackLockTypeAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbSetAttackLockTypeAck::Reset()
{
    ok = bool();    
}

void FPbSetAttackLockTypeAck::operator=(const idlepb::SetAttackLockTypeAck& Right)
{
    this->FromPb(Right);
}

bool FPbSetAttackLockTypeAck::operator==(const FPbSetAttackLockTypeAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbSetAttackLockTypeAck::operator!=(const FPbSetAttackLockTypeAck& Right) const
{
    return !operator==(Right);
}

FPbSetAttackUnlockTypeReq::FPbSetAttackUnlockTypeReq()
{
    Reset();        
}

FPbSetAttackUnlockTypeReq::FPbSetAttackUnlockTypeReq(const idlepb::SetAttackUnlockTypeReq& Right)
{
    this->FromPb(Right);
}

void FPbSetAttackUnlockTypeReq::FromPb(const idlepb::SetAttackUnlockTypeReq& Right)
{
    type = static_cast<EPbAttackUnlockType>(Right.type());
}

void FPbSetAttackUnlockTypeReq::ToPb(idlepb::SetAttackUnlockTypeReq* Out) const
{
    Out->set_type(static_cast<idlepb::AttackUnlockType>(type));    
}

void FPbSetAttackUnlockTypeReq::Reset()
{
    type = EPbAttackUnlockType();    
}

void FPbSetAttackUnlockTypeReq::operator=(const idlepb::SetAttackUnlockTypeReq& Right)
{
    this->FromPb(Right);
}

bool FPbSetAttackUnlockTypeReq::operator==(const FPbSetAttackUnlockTypeReq& Right) const
{
    if (this->type != Right.type)
        return false;
    return true;
}

bool FPbSetAttackUnlockTypeReq::operator!=(const FPbSetAttackUnlockTypeReq& Right) const
{
    return !operator==(Right);
}

FPbSetAttackUnlockTypeAck::FPbSetAttackUnlockTypeAck()
{
    Reset();        
}

FPbSetAttackUnlockTypeAck::FPbSetAttackUnlockTypeAck(const idlepb::SetAttackUnlockTypeAck& Right)
{
    this->FromPb(Right);
}

void FPbSetAttackUnlockTypeAck::FromPb(const idlepb::SetAttackUnlockTypeAck& Right)
{
    ok = Right.ok();
}

void FPbSetAttackUnlockTypeAck::ToPb(idlepb::SetAttackUnlockTypeAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbSetAttackUnlockTypeAck::Reset()
{
    ok = bool();    
}

void FPbSetAttackUnlockTypeAck::operator=(const idlepb::SetAttackUnlockTypeAck& Right)
{
    this->FromPb(Right);
}

bool FPbSetAttackUnlockTypeAck::operator==(const FPbSetAttackUnlockTypeAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbSetAttackUnlockTypeAck::operator!=(const FPbSetAttackUnlockTypeAck& Right) const
{
    return !operator==(Right);
}

FPbSetShowUnlockButtonReq::FPbSetShowUnlockButtonReq()
{
    Reset();        
}

FPbSetShowUnlockButtonReq::FPbSetShowUnlockButtonReq(const idlepb::SetShowUnlockButtonReq& Right)
{
    this->FromPb(Right);
}

void FPbSetShowUnlockButtonReq::FromPb(const idlepb::SetShowUnlockButtonReq& Right)
{
    enable = Right.enable();
}

void FPbSetShowUnlockButtonReq::ToPb(idlepb::SetShowUnlockButtonReq* Out) const
{
    Out->set_enable(enable);    
}

void FPbSetShowUnlockButtonReq::Reset()
{
    enable = bool();    
}

void FPbSetShowUnlockButtonReq::operator=(const idlepb::SetShowUnlockButtonReq& Right)
{
    this->FromPb(Right);
}

bool FPbSetShowUnlockButtonReq::operator==(const FPbSetShowUnlockButtonReq& Right) const
{
    if (this->enable != Right.enable)
        return false;
    return true;
}

bool FPbSetShowUnlockButtonReq::operator!=(const FPbSetShowUnlockButtonReq& Right) const
{
    return !operator==(Right);
}

FPbSetShowUnlockButtonAck::FPbSetShowUnlockButtonAck()
{
    Reset();        
}

FPbSetShowUnlockButtonAck::FPbSetShowUnlockButtonAck(const idlepb::SetShowUnlockButtonAck& Right)
{
    this->FromPb(Right);
}

void FPbSetShowUnlockButtonAck::FromPb(const idlepb::SetShowUnlockButtonAck& Right)
{
    ok = Right.ok();
}

void FPbSetShowUnlockButtonAck::ToPb(idlepb::SetShowUnlockButtonAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbSetShowUnlockButtonAck::Reset()
{
    ok = bool();    
}

void FPbSetShowUnlockButtonAck::operator=(const idlepb::SetShowUnlockButtonAck& Right)
{
    this->FromPb(Right);
}

bool FPbSetShowUnlockButtonAck::operator==(const FPbSetShowUnlockButtonAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbSetShowUnlockButtonAck::operator!=(const FPbSetShowUnlockButtonAck& Right) const
{
    return !operator==(Right);
}

FPbRefreshRoleNormalSetting::FPbRefreshRoleNormalSetting()
{
    Reset();        
}

FPbRefreshRoleNormalSetting::FPbRefreshRoleNormalSetting(const idlepb::RefreshRoleNormalSetting& Right)
{
    this->FromPb(Right);
}

void FPbRefreshRoleNormalSetting::FromPb(const idlepb::RefreshRoleNormalSetting& Right)
{
    settings = Right.settings();
}

void FPbRefreshRoleNormalSetting::ToPb(idlepb::RefreshRoleNormalSetting* Out) const
{
    settings.ToPb(Out->mutable_settings());    
}

void FPbRefreshRoleNormalSetting::Reset()
{
    settings = FPbRoleNormalSettings();    
}

void FPbRefreshRoleNormalSetting::operator=(const idlepb::RefreshRoleNormalSetting& Right)
{
    this->FromPb(Right);
}

bool FPbRefreshRoleNormalSetting::operator==(const FPbRefreshRoleNormalSetting& Right) const
{
    if (this->settings != Right.settings)
        return false;
    return true;
}

bool FPbRefreshRoleNormalSetting::operator!=(const FPbRefreshRoleNormalSetting& Right) const
{
    return !operator==(Right);
}

FPbGetUserVarReq::FPbGetUserVarReq()
{
    Reset();        
}

FPbGetUserVarReq::FPbGetUserVarReq(const idlepb::GetUserVarReq& Right)
{
    this->FromPb(Right);
}

void FPbGetUserVarReq::FromPb(const idlepb::GetUserVarReq& Right)
{
    var_name = UTF8_TO_TCHAR(Right.var_name().c_str());
}

void FPbGetUserVarReq::ToPb(idlepb::GetUserVarReq* Out) const
{
    Out->set_var_name(TCHAR_TO_UTF8(*var_name));    
}

void FPbGetUserVarReq::Reset()
{
    var_name = FString();    
}

void FPbGetUserVarReq::operator=(const idlepb::GetUserVarReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetUserVarReq::operator==(const FPbGetUserVarReq& Right) const
{
    if (this->var_name != Right.var_name)
        return false;
    return true;
}

bool FPbGetUserVarReq::operator!=(const FPbGetUserVarReq& Right) const
{
    return !operator==(Right);
}

FPbGetUserVarRsp::FPbGetUserVarRsp()
{
    Reset();        
}

FPbGetUserVarRsp::FPbGetUserVarRsp(const idlepb::GetUserVarRsp& Right)
{
    this->FromPb(Right);
}

void FPbGetUserVarRsp::FromPb(const idlepb::GetUserVarRsp& Right)
{
    var_value = Right.var_value();
}

void FPbGetUserVarRsp::ToPb(idlepb::GetUserVarRsp* Out) const
{
    Out->set_var_value(var_value);    
}

void FPbGetUserVarRsp::Reset()
{
    var_value = int32();    
}

void FPbGetUserVarRsp::operator=(const idlepb::GetUserVarRsp& Right)
{
    this->FromPb(Right);
}

bool FPbGetUserVarRsp::operator==(const FPbGetUserVarRsp& Right) const
{
    if (this->var_value != Right.var_value)
        return false;
    return true;
}

bool FPbGetUserVarRsp::operator!=(const FPbGetUserVarRsp& Right) const
{
    return !operator==(Right);
}

FPbGetUserVarsReq::FPbGetUserVarsReq()
{
    Reset();        
}

FPbGetUserVarsReq::FPbGetUserVarsReq(const idlepb::GetUserVarsReq& Right)
{
    this->FromPb(Right);
}

void FPbGetUserVarsReq::FromPb(const idlepb::GetUserVarsReq& Right)
{
    var_name.Empty();
    for (const auto& Elem : Right.var_name())
    {
        var_name.Emplace(UTF8_TO_TCHAR(Elem.c_str()));
    }
}

void FPbGetUserVarsReq::ToPb(idlepb::GetUserVarsReq* Out) const
{
    for (const auto& Elem : var_name)
    {
        Out->add_var_name(TCHAR_TO_UTF8(GetData(Elem)));    
    }    
}

void FPbGetUserVarsReq::Reset()
{
    var_name = TArray<FString>();    
}

void FPbGetUserVarsReq::operator=(const idlepb::GetUserVarsReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetUserVarsReq::operator==(const FPbGetUserVarsReq& Right) const
{
    if (this->var_name != Right.var_name)
        return false;
    return true;
}

bool FPbGetUserVarsReq::operator!=(const FPbGetUserVarsReq& Right) const
{
    return !operator==(Right);
}

FPbGetUserVarsRsp::FPbGetUserVarsRsp()
{
    Reset();        
}

FPbGetUserVarsRsp::FPbGetUserVarsRsp(const idlepb::GetUserVarsRsp& Right)
{
    this->FromPb(Right);
}

void FPbGetUserVarsRsp::FromPb(const idlepb::GetUserVarsRsp& Right)
{
    data.Empty();
    for (const auto& Elem : Right.data())
    {
        data.Emplace(Elem);
    }
}

void FPbGetUserVarsRsp::ToPb(idlepb::GetUserVarsRsp* Out) const
{
    for (const auto& Elem : data)
    {
        Elem.ToPb(Out->add_data());    
    }    
}

void FPbGetUserVarsRsp::Reset()
{
    data = TArray<FPbStringKeyInt32ValueEntry>();    
}

void FPbGetUserVarsRsp::operator=(const idlepb::GetUserVarsRsp& Right)
{
    this->FromPb(Right);
}

bool FPbGetUserVarsRsp::operator==(const FPbGetUserVarsRsp& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbGetUserVarsRsp::operator!=(const FPbGetUserVarsRsp& Right) const
{
    return !operator==(Right);
}

FPbSetUserVar::FPbSetUserVar()
{
    Reset();        
}

FPbSetUserVar::FPbSetUserVar(const idlepb::SetUserVar& Right)
{
    this->FromPb(Right);
}

void FPbSetUserVar::FromPb(const idlepb::SetUserVar& Right)
{
    var_name = UTF8_TO_TCHAR(Right.var_name().c_str());
    var_value = Right.var_value();
}

void FPbSetUserVar::ToPb(idlepb::SetUserVar* Out) const
{
    Out->set_var_name(TCHAR_TO_UTF8(*var_name));
    Out->set_var_value(var_value);    
}

void FPbSetUserVar::Reset()
{
    var_name = FString();
    var_value = int32();    
}

void FPbSetUserVar::operator=(const idlepb::SetUserVar& Right)
{
    this->FromPb(Right);
}

bool FPbSetUserVar::operator==(const FPbSetUserVar& Right) const
{
    if (this->var_name != Right.var_name)
        return false;
    if (this->var_value != Right.var_value)
        return false;
    return true;
}

bool FPbSetUserVar::operator!=(const FPbSetUserVar& Right) const
{
    return !operator==(Right);
}

FPbDelUserVar::FPbDelUserVar()
{
    Reset();        
}

FPbDelUserVar::FPbDelUserVar(const idlepb::DelUserVar& Right)
{
    this->FromPb(Right);
}

void FPbDelUserVar::FromPb(const idlepb::DelUserVar& Right)
{
    var_name = UTF8_TO_TCHAR(Right.var_name().c_str());
}

void FPbDelUserVar::ToPb(idlepb::DelUserVar* Out) const
{
    Out->set_var_name(TCHAR_TO_UTF8(*var_name));    
}

void FPbDelUserVar::Reset()
{
    var_name = FString();    
}

void FPbDelUserVar::operator=(const idlepb::DelUserVar& Right)
{
    this->FromPb(Right);
}

bool FPbDelUserVar::operator==(const FPbDelUserVar& Right) const
{
    if (this->var_name != Right.var_name)
        return false;
    return true;
}

bool FPbDelUserVar::operator!=(const FPbDelUserVar& Right) const
{
    return !operator==(Right);
}

FPbShareSelfItemReq::FPbShareSelfItemReq()
{
    Reset();        
}

FPbShareSelfItemReq::FPbShareSelfItemReq(const idlepb::ShareSelfItemReq& Right)
{
    this->FromPb(Right);
}

void FPbShareSelfItemReq::FromPb(const idlepb::ShareSelfItemReq& Right)
{
    item_id = Right.item_id();
}

void FPbShareSelfItemReq::ToPb(idlepb::ShareSelfItemReq* Out) const
{
    Out->set_item_id(item_id);    
}

void FPbShareSelfItemReq::Reset()
{
    item_id = int64();    
}

void FPbShareSelfItemReq::operator=(const idlepb::ShareSelfItemReq& Right)
{
    this->FromPb(Right);
}

bool FPbShareSelfItemReq::operator==(const FPbShareSelfItemReq& Right) const
{
    if (this->item_id != Right.item_id)
        return false;
    return true;
}

bool FPbShareSelfItemReq::operator!=(const FPbShareSelfItemReq& Right) const
{
    return !operator==(Right);
}

FPbShareSelfItemRsp::FPbShareSelfItemRsp()
{
    Reset();        
}

FPbShareSelfItemRsp::FPbShareSelfItemRsp(const idlepb::ShareSelfItemRsp& Right)
{
    this->FromPb(Right);
}

void FPbShareSelfItemRsp::FromPb(const idlepb::ShareSelfItemRsp& Right)
{
    share_id = Right.share_id();
}

void FPbShareSelfItemRsp::ToPb(idlepb::ShareSelfItemRsp* Out) const
{
    Out->set_share_id(share_id);    
}

void FPbShareSelfItemRsp::Reset()
{
    share_id = int64();    
}

void FPbShareSelfItemRsp::operator=(const idlepb::ShareSelfItemRsp& Right)
{
    this->FromPb(Right);
}

bool FPbShareSelfItemRsp::operator==(const FPbShareSelfItemRsp& Right) const
{
    if (this->share_id != Right.share_id)
        return false;
    return true;
}

bool FPbShareSelfItemRsp::operator!=(const FPbShareSelfItemRsp& Right) const
{
    return !operator==(Right);
}

FPbShareSelfItemsReq::FPbShareSelfItemsReq()
{
    Reset();        
}

FPbShareSelfItemsReq::FPbShareSelfItemsReq(const idlepb::ShareSelfItemsReq& Right)
{
    this->FromPb(Right);
}

void FPbShareSelfItemsReq::FromPb(const idlepb::ShareSelfItemsReq& Right)
{
    item_id.Empty();
    for (const auto& Elem : Right.item_id())
    {
        item_id.Emplace(Elem);
    }
}

void FPbShareSelfItemsReq::ToPb(idlepb::ShareSelfItemsReq* Out) const
{
    for (const auto& Elem : item_id)
    {
        Out->add_item_id(Elem);    
    }    
}

void FPbShareSelfItemsReq::Reset()
{
    item_id = TArray<int64>();    
}

void FPbShareSelfItemsReq::operator=(const idlepb::ShareSelfItemsReq& Right)
{
    this->FromPb(Right);
}

bool FPbShareSelfItemsReq::operator==(const FPbShareSelfItemsReq& Right) const
{
    if (this->item_id != Right.item_id)
        return false;
    return true;
}

bool FPbShareSelfItemsReq::operator!=(const FPbShareSelfItemsReq& Right) const
{
    return !operator==(Right);
}

FPbShareSelfItemsRsp::FPbShareSelfItemsRsp()
{
    Reset();        
}

FPbShareSelfItemsRsp::FPbShareSelfItemsRsp(const idlepb::ShareSelfItemsRsp& Right)
{
    this->FromPb(Right);
}

void FPbShareSelfItemsRsp::FromPb(const idlepb::ShareSelfItemsRsp& Right)
{
    share_id.Empty();
    for (const auto& Elem : Right.share_id())
    {
        share_id.Emplace(Elem);
    }
}

void FPbShareSelfItemsRsp::ToPb(idlepb::ShareSelfItemsRsp* Out) const
{
    for (const auto& Elem : share_id)
    {
        Elem.ToPb(Out->add_share_id());    
    }    
}

void FPbShareSelfItemsRsp::Reset()
{
    share_id = TArray<FPbInt64Pair>();    
}

void FPbShareSelfItemsRsp::operator=(const idlepb::ShareSelfItemsRsp& Right)
{
    this->FromPb(Right);
}

bool FPbShareSelfItemsRsp::operator==(const FPbShareSelfItemsRsp& Right) const
{
    if (this->share_id != Right.share_id)
        return false;
    return true;
}

bool FPbShareSelfItemsRsp::operator!=(const FPbShareSelfItemsRsp& Right) const
{
    return !operator==(Right);
}

FPbGetShareItemDataReq::FPbGetShareItemDataReq()
{
    Reset();        
}

FPbGetShareItemDataReq::FPbGetShareItemDataReq(const idlepb::GetShareItemDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetShareItemDataReq::FromPb(const idlepb::GetShareItemDataReq& Right)
{
    share_id = Right.share_id();
}

void FPbGetShareItemDataReq::ToPb(idlepb::GetShareItemDataReq* Out) const
{
    Out->set_share_id(share_id);    
}

void FPbGetShareItemDataReq::Reset()
{
    share_id = int64();    
}

void FPbGetShareItemDataReq::operator=(const idlepb::GetShareItemDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetShareItemDataReq::operator==(const FPbGetShareItemDataReq& Right) const
{
    if (this->share_id != Right.share_id)
        return false;
    return true;
}

bool FPbGetShareItemDataReq::operator!=(const FPbGetShareItemDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetShareItemDataRsp::FPbGetShareItemDataRsp()
{
    Reset();        
}

FPbGetShareItemDataRsp::FPbGetShareItemDataRsp(const idlepb::GetShareItemDataRsp& Right)
{
    this->FromPb(Right);
}

void FPbGetShareItemDataRsp::FromPb(const idlepb::GetShareItemDataRsp& Right)
{
    ok = Right.ok();
    item_data = Right.item_data();
}

void FPbGetShareItemDataRsp::ToPb(idlepb::GetShareItemDataRsp* Out) const
{
    Out->set_ok(ok);
    item_data.ToPb(Out->mutable_item_data());    
}

void FPbGetShareItemDataRsp::Reset()
{
    ok = bool();
    item_data = FPbItemData();    
}

void FPbGetShareItemDataRsp::operator=(const idlepb::GetShareItemDataRsp& Right)
{
    this->FromPb(Right);
}

bool FPbGetShareItemDataRsp::operator==(const FPbGetShareItemDataRsp& Right) const
{
    if (this->ok != Right.ok)
        return false;
    if (this->item_data != Right.item_data)
        return false;
    return true;
}

bool FPbGetShareItemDataRsp::operator!=(const FPbGetShareItemDataRsp& Right) const
{
    return !operator==(Right);
}

FPbGetChecklistDataReq::FPbGetChecklistDataReq()
{
    Reset();        
}

FPbGetChecklistDataReq::FPbGetChecklistDataReq(const idlepb::GetChecklistDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetChecklistDataReq::FromPb(const idlepb::GetChecklistDataReq& Right)
{
}

void FPbGetChecklistDataReq::ToPb(idlepb::GetChecklistDataReq* Out) const
{    
}

void FPbGetChecklistDataReq::Reset()
{    
}

void FPbGetChecklistDataReq::operator=(const idlepb::GetChecklistDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetChecklistDataReq::operator==(const FPbGetChecklistDataReq& Right) const
{
    return true;
}

bool FPbGetChecklistDataReq::operator!=(const FPbGetChecklistDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetChecklistDataAck::FPbGetChecklistDataAck()
{
    Reset();        
}

FPbGetChecklistDataAck::FPbGetChecklistDataAck(const idlepb::GetChecklistDataAck& Right)
{
    this->FromPb(Right);
}

void FPbGetChecklistDataAck::FromPb(const idlepb::GetChecklistDataAck& Right)
{
    data = Right.data();
}

void FPbGetChecklistDataAck::ToPb(idlepb::GetChecklistDataAck* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbGetChecklistDataAck::Reset()
{
    data = FPbRoleChecklistData();    
}

void FPbGetChecklistDataAck::operator=(const idlepb::GetChecklistDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetChecklistDataAck::operator==(const FPbGetChecklistDataAck& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbGetChecklistDataAck::operator!=(const FPbGetChecklistDataAck& Right) const
{
    return !operator==(Right);
}

FPbNotifyChecklist::FPbNotifyChecklist()
{
    Reset();        
}

FPbNotifyChecklist::FPbNotifyChecklist(const idlepb::NotifyChecklist& Right)
{
    this->FromPb(Right);
}

void FPbNotifyChecklist::FromPb(const idlepb::NotifyChecklist& Right)
{
}

void FPbNotifyChecklist::ToPb(idlepb::NotifyChecklist* Out) const
{    
}

void FPbNotifyChecklist::Reset()
{    
}

void FPbNotifyChecklist::operator=(const idlepb::NotifyChecklist& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyChecklist::operator==(const FPbNotifyChecklist& Right) const
{
    return true;
}

bool FPbNotifyChecklist::operator!=(const FPbNotifyChecklist& Right) const
{
    return !operator==(Right);
}

FPbChecklistOpReq::FPbChecklistOpReq()
{
    Reset();        
}

FPbChecklistOpReq::FPbChecklistOpReq(const idlepb::ChecklistOpReq& Right)
{
    this->FromPb(Right);
}

void FPbChecklistOpReq::FromPb(const idlepb::ChecklistOpReq& Right)
{
    sumbmit_or_receive = Right.sumbmit_or_receive();
    day_or_week = Right.day_or_week();
}

void FPbChecklistOpReq::ToPb(idlepb::ChecklistOpReq* Out) const
{
    Out->set_sumbmit_or_receive(sumbmit_or_receive);
    Out->set_day_or_week(day_or_week);    
}

void FPbChecklistOpReq::Reset()
{
    sumbmit_or_receive = bool();
    day_or_week = bool();    
}

void FPbChecklistOpReq::operator=(const idlepb::ChecklistOpReq& Right)
{
    this->FromPb(Right);
}

bool FPbChecklistOpReq::operator==(const FPbChecklistOpReq& Right) const
{
    if (this->sumbmit_or_receive != Right.sumbmit_or_receive)
        return false;
    if (this->day_or_week != Right.day_or_week)
        return false;
    return true;
}

bool FPbChecklistOpReq::operator!=(const FPbChecklistOpReq& Right) const
{
    return !operator==(Right);
}

FPbChecklistOpAck::FPbChecklistOpAck()
{
    Reset();        
}

FPbChecklistOpAck::FPbChecklistOpAck(const idlepb::ChecklistOpAck& Right)
{
    this->FromPb(Right);
}

void FPbChecklistOpAck::FromPb(const idlepb::ChecklistOpAck& Right)
{
    ok = Right.ok();
    data = Right.data();
}

void FPbChecklistOpAck::ToPb(idlepb::ChecklistOpAck* Out) const
{
    Out->set_ok(ok);
    data.ToPb(Out->mutable_data());    
}

void FPbChecklistOpAck::Reset()
{
    ok = bool();
    data = FPbRoleChecklistData();    
}

void FPbChecklistOpAck::operator=(const idlepb::ChecklistOpAck& Right)
{
    this->FromPb(Right);
}

bool FPbChecklistOpAck::operator==(const FPbChecklistOpAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbChecklistOpAck::operator!=(const FPbChecklistOpAck& Right) const
{
    return !operator==(Right);
}

FPbUpdateChecklistReq::FPbUpdateChecklistReq()
{
    Reset();        
}

FPbUpdateChecklistReq::FPbUpdateChecklistReq(const idlepb::UpdateChecklistReq& Right)
{
    this->FromPb(Right);
}

void FPbUpdateChecklistReq::FromPb(const idlepb::UpdateChecklistReq& Right)
{
    type = Right.type();
}

void FPbUpdateChecklistReq::ToPb(idlepb::UpdateChecklistReq* Out) const
{
    Out->set_type(type);    
}

void FPbUpdateChecklistReq::Reset()
{
    type = int32();    
}

void FPbUpdateChecklistReq::operator=(const idlepb::UpdateChecklistReq& Right)
{
    this->FromPb(Right);
}

bool FPbUpdateChecklistReq::operator==(const FPbUpdateChecklistReq& Right) const
{
    if (this->type != Right.type)
        return false;
    return true;
}

bool FPbUpdateChecklistReq::operator!=(const FPbUpdateChecklistReq& Right) const
{
    return !operator==(Right);
}

FPbUpdateChecklistAck::FPbUpdateChecklistAck()
{
    Reset();        
}

FPbUpdateChecklistAck::FPbUpdateChecklistAck(const idlepb::UpdateChecklistAck& Right)
{
    this->FromPb(Right);
}

void FPbUpdateChecklistAck::FromPb(const idlepb::UpdateChecklistAck& Right)
{
    ok = Right.ok();
}

void FPbUpdateChecklistAck::ToPb(idlepb::UpdateChecklistAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbUpdateChecklistAck::Reset()
{
    ok = bool();    
}

void FPbUpdateChecklistAck::operator=(const idlepb::UpdateChecklistAck& Right)
{
    this->FromPb(Right);
}

bool FPbUpdateChecklistAck::operator==(const FPbUpdateChecklistAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbUpdateChecklistAck::operator!=(const FPbUpdateChecklistAck& Right) const
{
    return !operator==(Right);
}

FPbGetCommonItemExchangeDataReq::FPbGetCommonItemExchangeDataReq()
{
    Reset();        
}

FPbGetCommonItemExchangeDataReq::FPbGetCommonItemExchangeDataReq(const idlepb::GetCommonItemExchangeDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetCommonItemExchangeDataReq::FromPb(const idlepb::GetCommonItemExchangeDataReq& Right)
{
    cfg_id = Right.cfg_id();
}

void FPbGetCommonItemExchangeDataReq::ToPb(idlepb::GetCommonItemExchangeDataReq* Out) const
{
    Out->set_cfg_id(cfg_id);    
}

void FPbGetCommonItemExchangeDataReq::Reset()
{
    cfg_id = int32();    
}

void FPbGetCommonItemExchangeDataReq::operator=(const idlepb::GetCommonItemExchangeDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetCommonItemExchangeDataReq::operator==(const FPbGetCommonItemExchangeDataReq& Right) const
{
    if (this->cfg_id != Right.cfg_id)
        return false;
    return true;
}

bool FPbGetCommonItemExchangeDataReq::operator!=(const FPbGetCommonItemExchangeDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetCommonItemExchangeDataAck::FPbGetCommonItemExchangeDataAck()
{
    Reset();        
}

FPbGetCommonItemExchangeDataAck::FPbGetCommonItemExchangeDataAck(const idlepb::GetCommonItemExchangeDataAck& Right)
{
    this->FromPb(Right);
}

void FPbGetCommonItemExchangeDataAck::FromPb(const idlepb::GetCommonItemExchangeDataAck& Right)
{
    today_exchange_num = Right.today_exchange_num();
}

void FPbGetCommonItemExchangeDataAck::ToPb(idlepb::GetCommonItemExchangeDataAck* Out) const
{
    Out->set_today_exchange_num(today_exchange_num);    
}

void FPbGetCommonItemExchangeDataAck::Reset()
{
    today_exchange_num = int32();    
}

void FPbGetCommonItemExchangeDataAck::operator=(const idlepb::GetCommonItemExchangeDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetCommonItemExchangeDataAck::operator==(const FPbGetCommonItemExchangeDataAck& Right) const
{
    if (this->today_exchange_num != Right.today_exchange_num)
        return false;
    return true;
}

bool FPbGetCommonItemExchangeDataAck::operator!=(const FPbGetCommonItemExchangeDataAck& Right) const
{
    return !operator==(Right);
}

FPbExchangeCommonItemReq::FPbExchangeCommonItemReq()
{
    Reset();        
}

FPbExchangeCommonItemReq::FPbExchangeCommonItemReq(const idlepb::ExchangeCommonItemReq& Right)
{
    this->FromPb(Right);
}

void FPbExchangeCommonItemReq::FromPb(const idlepb::ExchangeCommonItemReq& Right)
{
    cfg_id = Right.cfg_id();
    num = Right.num();
}

void FPbExchangeCommonItemReq::ToPb(idlepb::ExchangeCommonItemReq* Out) const
{
    Out->set_cfg_id(cfg_id);
    Out->set_num(num);    
}

void FPbExchangeCommonItemReq::Reset()
{
    cfg_id = int32();
    num = int32();    
}

void FPbExchangeCommonItemReq::operator=(const idlepb::ExchangeCommonItemReq& Right)
{
    this->FromPb(Right);
}

bool FPbExchangeCommonItemReq::operator==(const FPbExchangeCommonItemReq& Right) const
{
    if (this->cfg_id != Right.cfg_id)
        return false;
    if (this->num != Right.num)
        return false;
    return true;
}

bool FPbExchangeCommonItemReq::operator!=(const FPbExchangeCommonItemReq& Right) const
{
    return !operator==(Right);
}

FPbExchangeCommonItemAck::FPbExchangeCommonItemAck()
{
    Reset();        
}

FPbExchangeCommonItemAck::FPbExchangeCommonItemAck(const idlepb::ExchangeCommonItemAck& Right)
{
    this->FromPb(Right);
}

void FPbExchangeCommonItemAck::FromPb(const idlepb::ExchangeCommonItemAck& Right)
{
    ok = Right.ok();
    out_num = Right.out_num();
}

void FPbExchangeCommonItemAck::ToPb(idlepb::ExchangeCommonItemAck* Out) const
{
    Out->set_ok(ok);
    Out->set_out_num(out_num);    
}

void FPbExchangeCommonItemAck::Reset()
{
    ok = bool();
    out_num = int32();    
}

void FPbExchangeCommonItemAck::operator=(const idlepb::ExchangeCommonItemAck& Right)
{
    this->FromPb(Right);
}

bool FPbExchangeCommonItemAck::operator==(const FPbExchangeCommonItemAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    if (this->out_num != Right.out_num)
        return false;
    return true;
}

bool FPbExchangeCommonItemAck::operator!=(const FPbExchangeCommonItemAck& Right) const
{
    return !operator==(Right);
}

FPbSynthesisCommonItemReq::FPbSynthesisCommonItemReq()
{
    Reset();        
}

FPbSynthesisCommonItemReq::FPbSynthesisCommonItemReq(const idlepb::SynthesisCommonItemReq& Right)
{
    this->FromPb(Right);
}

void FPbSynthesisCommonItemReq::FromPb(const idlepb::SynthesisCommonItemReq& Right)
{
    cfg_id = Right.cfg_id();
    num = Right.num();
}

void FPbSynthesisCommonItemReq::ToPb(idlepb::SynthesisCommonItemReq* Out) const
{
    Out->set_cfg_id(cfg_id);
    Out->set_num(num);    
}

void FPbSynthesisCommonItemReq::Reset()
{
    cfg_id = int32();
    num = int32();    
}

void FPbSynthesisCommonItemReq::operator=(const idlepb::SynthesisCommonItemReq& Right)
{
    this->FromPb(Right);
}

bool FPbSynthesisCommonItemReq::operator==(const FPbSynthesisCommonItemReq& Right) const
{
    if (this->cfg_id != Right.cfg_id)
        return false;
    if (this->num != Right.num)
        return false;
    return true;
}

bool FPbSynthesisCommonItemReq::operator!=(const FPbSynthesisCommonItemReq& Right) const
{
    return !operator==(Right);
}

FPbSynthesisCommonItemAck::FPbSynthesisCommonItemAck()
{
    Reset();        
}

FPbSynthesisCommonItemAck::FPbSynthesisCommonItemAck(const idlepb::SynthesisCommonItemAck& Right)
{
    this->FromPb(Right);
}

void FPbSynthesisCommonItemAck::FromPb(const idlepb::SynthesisCommonItemAck& Right)
{
    ok = Right.ok();
}

void FPbSynthesisCommonItemAck::ToPb(idlepb::SynthesisCommonItemAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbSynthesisCommonItemAck::Reset()
{
    ok = bool();    
}

void FPbSynthesisCommonItemAck::operator=(const idlepb::SynthesisCommonItemAck& Right)
{
    this->FromPb(Right);
}

bool FPbSynthesisCommonItemAck::operator==(const FPbSynthesisCommonItemAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbSynthesisCommonItemAck::operator!=(const FPbSynthesisCommonItemAck& Right) const
{
    return !operator==(Right);
}

FPbGetRoleSeptShopDataReq::FPbGetRoleSeptShopDataReq()
{
    Reset();        
}

FPbGetRoleSeptShopDataReq::FPbGetRoleSeptShopDataReq(const idlepb::GetRoleSeptShopDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleSeptShopDataReq::FromPb(const idlepb::GetRoleSeptShopDataReq& Right)
{
}

void FPbGetRoleSeptShopDataReq::ToPb(idlepb::GetRoleSeptShopDataReq* Out) const
{    
}

void FPbGetRoleSeptShopDataReq::Reset()
{    
}

void FPbGetRoleSeptShopDataReq::operator=(const idlepb::GetRoleSeptShopDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleSeptShopDataReq::operator==(const FPbGetRoleSeptShopDataReq& Right) const
{
    return true;
}

bool FPbGetRoleSeptShopDataReq::operator!=(const FPbGetRoleSeptShopDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetRoleSeptShopDataAck::FPbGetRoleSeptShopDataAck()
{
    Reset();        
}

FPbGetRoleSeptShopDataAck::FPbGetRoleSeptShopDataAck(const idlepb::GetRoleSeptShopDataAck& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleSeptShopDataAck::FromPb(const idlepb::GetRoleSeptShopDataAck& Right)
{
    data = Right.data();
}

void FPbGetRoleSeptShopDataAck::ToPb(idlepb::GetRoleSeptShopDataAck* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbGetRoleSeptShopDataAck::Reset()
{
    data = FPbRoleSeptShopData();    
}

void FPbGetRoleSeptShopDataAck::operator=(const idlepb::GetRoleSeptShopDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleSeptShopDataAck::operator==(const FPbGetRoleSeptShopDataAck& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbGetRoleSeptShopDataAck::operator!=(const FPbGetRoleSeptShopDataAck& Right) const
{
    return !operator==(Right);
}

FPbGetRoleSeptQuestDataReq::FPbGetRoleSeptQuestDataReq()
{
    Reset();        
}

FPbGetRoleSeptQuestDataReq::FPbGetRoleSeptQuestDataReq(const idlepb::GetRoleSeptQuestDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleSeptQuestDataReq::FromPb(const idlepb::GetRoleSeptQuestDataReq& Right)
{
}

void FPbGetRoleSeptQuestDataReq::ToPb(idlepb::GetRoleSeptQuestDataReq* Out) const
{    
}

void FPbGetRoleSeptQuestDataReq::Reset()
{    
}

void FPbGetRoleSeptQuestDataReq::operator=(const idlepb::GetRoleSeptQuestDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleSeptQuestDataReq::operator==(const FPbGetRoleSeptQuestDataReq& Right) const
{
    return true;
}

bool FPbGetRoleSeptQuestDataReq::operator!=(const FPbGetRoleSeptQuestDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetRoleSeptQuestDataAck::FPbGetRoleSeptQuestDataAck()
{
    Reset();        
}

FPbGetRoleSeptQuestDataAck::FPbGetRoleSeptQuestDataAck(const idlepb::GetRoleSeptQuestDataAck& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleSeptQuestDataAck::FromPb(const idlepb::GetRoleSeptQuestDataAck& Right)
{
    data = Right.data();
}

void FPbGetRoleSeptQuestDataAck::ToPb(idlepb::GetRoleSeptQuestDataAck* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbGetRoleSeptQuestDataAck::Reset()
{
    data = FPbRoleSeptQuestData();    
}

void FPbGetRoleSeptQuestDataAck::operator=(const idlepb::GetRoleSeptQuestDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleSeptQuestDataAck::operator==(const FPbGetRoleSeptQuestDataAck& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbGetRoleSeptQuestDataAck::operator!=(const FPbGetRoleSeptQuestDataAck& Right) const
{
    return !operator==(Right);
}

FPbBuySeptShopItemReq::FPbBuySeptShopItemReq()
{
    Reset();        
}

FPbBuySeptShopItemReq::FPbBuySeptShopItemReq(const idlepb::BuySeptShopItemReq& Right)
{
    this->FromPb(Right);
}

void FPbBuySeptShopItemReq::FromPb(const idlepb::BuySeptShopItemReq& Right)
{
    id = Right.id();
    num = Right.num();
}

void FPbBuySeptShopItemReq::ToPb(idlepb::BuySeptShopItemReq* Out) const
{
    Out->set_id(id);
    Out->set_num(num);    
}

void FPbBuySeptShopItemReq::Reset()
{
    id = int32();
    num = int32();    
}

void FPbBuySeptShopItemReq::operator=(const idlepb::BuySeptShopItemReq& Right)
{
    this->FromPb(Right);
}

bool FPbBuySeptShopItemReq::operator==(const FPbBuySeptShopItemReq& Right) const
{
    if (this->id != Right.id)
        return false;
    if (this->num != Right.num)
        return false;
    return true;
}

bool FPbBuySeptShopItemReq::operator!=(const FPbBuySeptShopItemReq& Right) const
{
    return !operator==(Right);
}

FPbBuySeptShopItemAck::FPbBuySeptShopItemAck()
{
    Reset();        
}

FPbBuySeptShopItemAck::FPbBuySeptShopItemAck(const idlepb::BuySeptShopItemAck& Right)
{
    this->FromPb(Right);
}

void FPbBuySeptShopItemAck::FromPb(const idlepb::BuySeptShopItemAck& Right)
{
    ok = Right.ok();
}

void FPbBuySeptShopItemAck::ToPb(idlepb::BuySeptShopItemAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbBuySeptShopItemAck::Reset()
{
    ok = bool();    
}

void FPbBuySeptShopItemAck::operator=(const idlepb::BuySeptShopItemAck& Right)
{
    this->FromPb(Right);
}

bool FPbBuySeptShopItemAck::operator==(const FPbBuySeptShopItemAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbBuySeptShopItemAck::operator!=(const FPbBuySeptShopItemAck& Right) const
{
    return !operator==(Right);
}

FPbReqRoleSeptQuestOpReq::FPbReqRoleSeptQuestOpReq()
{
    Reset();        
}

FPbReqRoleSeptQuestOpReq::FPbReqRoleSeptQuestOpReq(const idlepb::ReqRoleSeptQuestOpReq& Right)
{
    this->FromPb(Right);
}

void FPbReqRoleSeptQuestOpReq::FromPb(const idlepb::ReqRoleSeptQuestOpReq& Right)
{
    uid = Right.uid();
}

void FPbReqRoleSeptQuestOpReq::ToPb(idlepb::ReqRoleSeptQuestOpReq* Out) const
{
    Out->set_uid(uid);    
}

void FPbReqRoleSeptQuestOpReq::Reset()
{
    uid = int32();    
}

void FPbReqRoleSeptQuestOpReq::operator=(const idlepb::ReqRoleSeptQuestOpReq& Right)
{
    this->FromPb(Right);
}

bool FPbReqRoleSeptQuestOpReq::operator==(const FPbReqRoleSeptQuestOpReq& Right) const
{
    if (this->uid != Right.uid)
        return false;
    return true;
}

bool FPbReqRoleSeptQuestOpReq::operator!=(const FPbReqRoleSeptQuestOpReq& Right) const
{
    return !operator==(Right);
}

FPbReqRoleSeptQuestOpAck::FPbReqRoleSeptQuestOpAck()
{
    Reset();        
}

FPbReqRoleSeptQuestOpAck::FPbReqRoleSeptQuestOpAck(const idlepb::ReqRoleSeptQuestOpAck& Right)
{
    this->FromPb(Right);
}

void FPbReqRoleSeptQuestOpAck::FromPb(const idlepb::ReqRoleSeptQuestOpAck& Right)
{
    ok = Right.ok();
}

void FPbReqRoleSeptQuestOpAck::ToPb(idlepb::ReqRoleSeptQuestOpAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbReqRoleSeptQuestOpAck::Reset()
{
    ok = bool();    
}

void FPbReqRoleSeptQuestOpAck::operator=(const idlepb::ReqRoleSeptQuestOpAck& Right)
{
    this->FromPb(Right);
}

bool FPbReqRoleSeptQuestOpAck::operator==(const FPbReqRoleSeptQuestOpAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbReqRoleSeptQuestOpAck::operator!=(const FPbReqRoleSeptQuestOpAck& Right) const
{
    return !operator==(Right);
}

FPbRefreshSeptQuestReq::FPbRefreshSeptQuestReq()
{
    Reset();        
}

FPbRefreshSeptQuestReq::FPbRefreshSeptQuestReq(const idlepb::RefreshSeptQuestReq& Right)
{
    this->FromPb(Right);
}

void FPbRefreshSeptQuestReq::FromPb(const idlepb::RefreshSeptQuestReq& Right)
{
}

void FPbRefreshSeptQuestReq::ToPb(idlepb::RefreshSeptQuestReq* Out) const
{    
}

void FPbRefreshSeptQuestReq::Reset()
{    
}

void FPbRefreshSeptQuestReq::operator=(const idlepb::RefreshSeptQuestReq& Right)
{
    this->FromPb(Right);
}

bool FPbRefreshSeptQuestReq::operator==(const FPbRefreshSeptQuestReq& Right) const
{
    return true;
}

bool FPbRefreshSeptQuestReq::operator!=(const FPbRefreshSeptQuestReq& Right) const
{
    return !operator==(Right);
}

FPbRefreshSeptQuestAck::FPbRefreshSeptQuestAck()
{
    Reset();        
}

FPbRefreshSeptQuestAck::FPbRefreshSeptQuestAck(const idlepb::RefreshSeptQuestAck& Right)
{
    this->FromPb(Right);
}

void FPbRefreshSeptQuestAck::FromPb(const idlepb::RefreshSeptQuestAck& Right)
{
    data = Right.data();
}

void FPbRefreshSeptQuestAck::ToPb(idlepb::RefreshSeptQuestAck* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbRefreshSeptQuestAck::Reset()
{
    data = FPbRoleSeptQuestData();    
}

void FPbRefreshSeptQuestAck::operator=(const idlepb::RefreshSeptQuestAck& Right)
{
    this->FromPb(Right);
}

bool FPbRefreshSeptQuestAck::operator==(const FPbRefreshSeptQuestAck& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbRefreshSeptQuestAck::operator!=(const FPbRefreshSeptQuestAck& Right) const
{
    return !operator==(Right);
}

FPbReqSeptQuestRankUpReq::FPbReqSeptQuestRankUpReq()
{
    Reset();        
}

FPbReqSeptQuestRankUpReq::FPbReqSeptQuestRankUpReq(const idlepb::ReqSeptQuestRankUpReq& Right)
{
    this->FromPb(Right);
}

void FPbReqSeptQuestRankUpReq::FromPb(const idlepb::ReqSeptQuestRankUpReq& Right)
{
}

void FPbReqSeptQuestRankUpReq::ToPb(idlepb::ReqSeptQuestRankUpReq* Out) const
{    
}

void FPbReqSeptQuestRankUpReq::Reset()
{    
}

void FPbReqSeptQuestRankUpReq::operator=(const idlepb::ReqSeptQuestRankUpReq& Right)
{
    this->FromPb(Right);
}

bool FPbReqSeptQuestRankUpReq::operator==(const FPbReqSeptQuestRankUpReq& Right) const
{
    return true;
}

bool FPbReqSeptQuestRankUpReq::operator!=(const FPbReqSeptQuestRankUpReq& Right) const
{
    return !operator==(Right);
}

FPbReqSeptQuestRankUpAck::FPbReqSeptQuestRankUpAck()
{
    Reset();        
}

FPbReqSeptQuestRankUpAck::FPbReqSeptQuestRankUpAck(const idlepb::ReqSeptQuestRankUpAck& Right)
{
    this->FromPb(Right);
}

void FPbReqSeptQuestRankUpAck::FromPb(const idlepb::ReqSeptQuestRankUpAck& Right)
{
    ok = Right.ok();
}

void FPbReqSeptQuestRankUpAck::ToPb(idlepb::ReqSeptQuestRankUpAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbReqSeptQuestRankUpAck::Reset()
{
    ok = bool();    
}

void FPbReqSeptQuestRankUpAck::operator=(const idlepb::ReqSeptQuestRankUpAck& Right)
{
    this->FromPb(Right);
}

bool FPbReqSeptQuestRankUpAck::operator==(const FPbReqSeptQuestRankUpAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbReqSeptQuestRankUpAck::operator!=(const FPbReqSeptQuestRankUpAck& Right) const
{
    return !operator==(Right);
}

FPbGetGongFaDataReq::FPbGetGongFaDataReq()
{
    Reset();        
}

FPbGetGongFaDataReq::FPbGetGongFaDataReq(const idlepb::GetGongFaDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetGongFaDataReq::FromPb(const idlepb::GetGongFaDataReq& Right)
{
}

void FPbGetGongFaDataReq::ToPb(idlepb::GetGongFaDataReq* Out) const
{    
}

void FPbGetGongFaDataReq::Reset()
{    
}

void FPbGetGongFaDataReq::operator=(const idlepb::GetGongFaDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetGongFaDataReq::operator==(const FPbGetGongFaDataReq& Right) const
{
    return true;
}

bool FPbGetGongFaDataReq::operator!=(const FPbGetGongFaDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetGongFaDataAck::FPbGetGongFaDataAck()
{
    Reset();        
}

FPbGetGongFaDataAck::FPbGetGongFaDataAck(const idlepb::GetGongFaDataAck& Right)
{
    this->FromPb(Right);
}

void FPbGetGongFaDataAck::FromPb(const idlepb::GetGongFaDataAck& Right)
{
    data = Right.data();
}

void FPbGetGongFaDataAck::ToPb(idlepb::GetGongFaDataAck* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbGetGongFaDataAck::Reset()
{
    data = FPbRoleGongFaData();    
}

void FPbGetGongFaDataAck::operator=(const idlepb::GetGongFaDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetGongFaDataAck::operator==(const FPbGetGongFaDataAck& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbGetGongFaDataAck::operator!=(const FPbGetGongFaDataAck& Right) const
{
    return !operator==(Right);
}

FPbGongFaOpReq::FPbGongFaOpReq()
{
    Reset();        
}

FPbGongFaOpReq::FPbGongFaOpReq(const idlepb::GongFaOpReq& Right)
{
    this->FromPb(Right);
}

void FPbGongFaOpReq::FromPb(const idlepb::GongFaOpReq& Right)
{
    gongfa_id = Right.gongfa_id();
}

void FPbGongFaOpReq::ToPb(idlepb::GongFaOpReq* Out) const
{
    Out->set_gongfa_id(gongfa_id);    
}

void FPbGongFaOpReq::Reset()
{
    gongfa_id = int32();    
}

void FPbGongFaOpReq::operator=(const idlepb::GongFaOpReq& Right)
{
    this->FromPb(Right);
}

bool FPbGongFaOpReq::operator==(const FPbGongFaOpReq& Right) const
{
    if (this->gongfa_id != Right.gongfa_id)
        return false;
    return true;
}

bool FPbGongFaOpReq::operator!=(const FPbGongFaOpReq& Right) const
{
    return !operator==(Right);
}

FPbGongFaOpAck::FPbGongFaOpAck()
{
    Reset();        
}

FPbGongFaOpAck::FPbGongFaOpAck(const idlepb::GongFaOpAck& Right)
{
    this->FromPb(Right);
}

void FPbGongFaOpAck::FromPb(const idlepb::GongFaOpAck& Right)
{
    ok = Right.ok();
    gongfa_data = Right.gongfa_data();
}

void FPbGongFaOpAck::ToPb(idlepb::GongFaOpAck* Out) const
{
    Out->set_ok(ok);
    gongfa_data.ToPb(Out->mutable_gongfa_data());    
}

void FPbGongFaOpAck::Reset()
{
    ok = bool();
    gongfa_data = FPbGongFaData();    
}

void FPbGongFaOpAck::operator=(const idlepb::GongFaOpAck& Right)
{
    this->FromPb(Right);
}

bool FPbGongFaOpAck::operator==(const FPbGongFaOpAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    if (this->gongfa_data != Right.gongfa_data)
        return false;
    return true;
}

bool FPbGongFaOpAck::operator!=(const FPbGongFaOpAck& Right) const
{
    return !operator==(Right);
}

FPbActivateGongFaMaxEffectReq::FPbActivateGongFaMaxEffectReq()
{
    Reset();        
}

FPbActivateGongFaMaxEffectReq::FPbActivateGongFaMaxEffectReq(const idlepb::ActivateGongFaMaxEffectReq& Right)
{
    this->FromPb(Right);
}

void FPbActivateGongFaMaxEffectReq::FromPb(const idlepb::ActivateGongFaMaxEffectReq& Right)
{
    cfg_id = Right.cfg_id();
}

void FPbActivateGongFaMaxEffectReq::ToPb(idlepb::ActivateGongFaMaxEffectReq* Out) const
{
    Out->set_cfg_id(cfg_id);    
}

void FPbActivateGongFaMaxEffectReq::Reset()
{
    cfg_id = int32();    
}

void FPbActivateGongFaMaxEffectReq::operator=(const idlepb::ActivateGongFaMaxEffectReq& Right)
{
    this->FromPb(Right);
}

bool FPbActivateGongFaMaxEffectReq::operator==(const FPbActivateGongFaMaxEffectReq& Right) const
{
    if (this->cfg_id != Right.cfg_id)
        return false;
    return true;
}

bool FPbActivateGongFaMaxEffectReq::operator!=(const FPbActivateGongFaMaxEffectReq& Right) const
{
    return !operator==(Right);
}

FPbActivateGongFaMaxEffectAck::FPbActivateGongFaMaxEffectAck()
{
    Reset();        
}

FPbActivateGongFaMaxEffectAck::FPbActivateGongFaMaxEffectAck(const idlepb::ActivateGongFaMaxEffectAck& Right)
{
    this->FromPb(Right);
}

void FPbActivateGongFaMaxEffectAck::FromPb(const idlepb::ActivateGongFaMaxEffectAck& Right)
{
    ok = Right.ok();
}

void FPbActivateGongFaMaxEffectAck::ToPb(idlepb::ActivateGongFaMaxEffectAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbActivateGongFaMaxEffectAck::Reset()
{
    ok = bool();    
}

void FPbActivateGongFaMaxEffectAck::operator=(const idlepb::ActivateGongFaMaxEffectAck& Right)
{
    this->FromPb(Right);
}

bool FPbActivateGongFaMaxEffectAck::operator==(const FPbActivateGongFaMaxEffectAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbActivateGongFaMaxEffectAck::operator!=(const FPbActivateGongFaMaxEffectAck& Right) const
{
    return !operator==(Right);
}

FPbReceiveFuZengRewardsReq::FPbReceiveFuZengRewardsReq()
{
    Reset();        
}

FPbReceiveFuZengRewardsReq::FPbReceiveFuZengRewardsReq(const idlepb::ReceiveFuZengRewardsReq& Right)
{
    this->FromPb(Right);
}

void FPbReceiveFuZengRewardsReq::FromPb(const idlepb::ReceiveFuZengRewardsReq& Right)
{
    cfg_id = Right.cfg_id();
    type = static_cast<EPbFuZengType>(Right.type());
}

void FPbReceiveFuZengRewardsReq::ToPb(idlepb::ReceiveFuZengRewardsReq* Out) const
{
    Out->set_cfg_id(cfg_id);
    Out->set_type(static_cast<idlepb::FuZengType>(type));    
}

void FPbReceiveFuZengRewardsReq::Reset()
{
    cfg_id = int32();
    type = EPbFuZengType();    
}

void FPbReceiveFuZengRewardsReq::operator=(const idlepb::ReceiveFuZengRewardsReq& Right)
{
    this->FromPb(Right);
}

bool FPbReceiveFuZengRewardsReq::operator==(const FPbReceiveFuZengRewardsReq& Right) const
{
    if (this->cfg_id != Right.cfg_id)
        return false;
    if (this->type != Right.type)
        return false;
    return true;
}

bool FPbReceiveFuZengRewardsReq::operator!=(const FPbReceiveFuZengRewardsReq& Right) const
{
    return !operator==(Right);
}

FPbReceiveFuZengRewardsAck::FPbReceiveFuZengRewardsAck()
{
    Reset();        
}

FPbReceiveFuZengRewardsAck::FPbReceiveFuZengRewardsAck(const idlepb::ReceiveFuZengRewardsAck& Right)
{
    this->FromPb(Right);
}

void FPbReceiveFuZengRewardsAck::FromPb(const idlepb::ReceiveFuZengRewardsAck& Right)
{
    ok = Right.ok();
    data = Right.data();
}

void FPbReceiveFuZengRewardsAck::ToPb(idlepb::ReceiveFuZengRewardsAck* Out) const
{
    Out->set_ok(ok);
    data.ToPb(Out->mutable_data());    
}

void FPbReceiveFuZengRewardsAck::Reset()
{
    ok = bool();
    data = FPbFuZengData();    
}

void FPbReceiveFuZengRewardsAck::operator=(const idlepb::ReceiveFuZengRewardsAck& Right)
{
    this->FromPb(Right);
}

bool FPbReceiveFuZengRewardsAck::operator==(const FPbReceiveFuZengRewardsAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbReceiveFuZengRewardsAck::operator!=(const FPbReceiveFuZengRewardsAck& Right) const
{
    return !operator==(Right);
}

FPbGetRoleFuZengDataReq::FPbGetRoleFuZengDataReq()
{
    Reset();        
}

FPbGetRoleFuZengDataReq::FPbGetRoleFuZengDataReq(const idlepb::GetRoleFuZengDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleFuZengDataReq::FromPb(const idlepb::GetRoleFuZengDataReq& Right)
{
}

void FPbGetRoleFuZengDataReq::ToPb(idlepb::GetRoleFuZengDataReq* Out) const
{    
}

void FPbGetRoleFuZengDataReq::Reset()
{    
}

void FPbGetRoleFuZengDataReq::operator=(const idlepb::GetRoleFuZengDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleFuZengDataReq::operator==(const FPbGetRoleFuZengDataReq& Right) const
{
    return true;
}

bool FPbGetRoleFuZengDataReq::operator!=(const FPbGetRoleFuZengDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetRoleFuZengDataAck::FPbGetRoleFuZengDataAck()
{
    Reset();        
}

FPbGetRoleFuZengDataAck::FPbGetRoleFuZengDataAck(const idlepb::GetRoleFuZengDataAck& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleFuZengDataAck::FromPb(const idlepb::GetRoleFuZengDataAck& Right)
{
    data = Right.data();
}

void FPbGetRoleFuZengDataAck::ToPb(idlepb::GetRoleFuZengDataAck* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbGetRoleFuZengDataAck::Reset()
{
    data = FPbRoleFuZengData();    
}

void FPbGetRoleFuZengDataAck::operator=(const idlepb::GetRoleFuZengDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleFuZengDataAck::operator==(const FPbGetRoleFuZengDataAck& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbGetRoleFuZengDataAck::operator!=(const FPbGetRoleFuZengDataAck& Right) const
{
    return !operator==(Right);
}

FPbNotifyFuZeng::FPbNotifyFuZeng()
{
    Reset();        
}

FPbNotifyFuZeng::FPbNotifyFuZeng(const idlepb::NotifyFuZeng& Right)
{
    this->FromPb(Right);
}

void FPbNotifyFuZeng::FromPb(const idlepb::NotifyFuZeng& Right)
{
    type = static_cast<EPbFuZengType>(Right.type());
    num = Right.num();
    cfg_id = Right.cfg_id();
}

void FPbNotifyFuZeng::ToPb(idlepb::NotifyFuZeng* Out) const
{
    Out->set_type(static_cast<idlepb::FuZengType>(type));
    Out->set_num(num);
    Out->set_cfg_id(cfg_id);    
}

void FPbNotifyFuZeng::Reset()
{
    type = EPbFuZengType();
    num = int64();
    cfg_id = int32();    
}

void FPbNotifyFuZeng::operator=(const idlepb::NotifyFuZeng& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyFuZeng::operator==(const FPbNotifyFuZeng& Right) const
{
    if (this->type != Right.type)
        return false;
    if (this->num != Right.num)
        return false;
    if (this->cfg_id != Right.cfg_id)
        return false;
    return true;
}

bool FPbNotifyFuZeng::operator!=(const FPbNotifyFuZeng& Right) const
{
    return !operator==(Right);
}

FPbGetRoleTreasuryDataReq::FPbGetRoleTreasuryDataReq()
{
    Reset();        
}

FPbGetRoleTreasuryDataReq::FPbGetRoleTreasuryDataReq(const idlepb::GetRoleTreasuryDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleTreasuryDataReq::FromPb(const idlepb::GetRoleTreasuryDataReq& Right)
{
    dirty_flag = Right.dirty_flag();
}

void FPbGetRoleTreasuryDataReq::ToPb(idlepb::GetRoleTreasuryDataReq* Out) const
{
    Out->set_dirty_flag(dirty_flag);    
}

void FPbGetRoleTreasuryDataReq::Reset()
{
    dirty_flag = bool();    
}

void FPbGetRoleTreasuryDataReq::operator=(const idlepb::GetRoleTreasuryDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleTreasuryDataReq::operator==(const FPbGetRoleTreasuryDataReq& Right) const
{
    if (this->dirty_flag != Right.dirty_flag)
        return false;
    return true;
}

bool FPbGetRoleTreasuryDataReq::operator!=(const FPbGetRoleTreasuryDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetRoleTreasuryDataAck::FPbGetRoleTreasuryDataAck()
{
    Reset();        
}

FPbGetRoleTreasuryDataAck::FPbGetRoleTreasuryDataAck(const idlepb::GetRoleTreasuryDataAck& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleTreasuryDataAck::FromPb(const idlepb::GetRoleTreasuryDataAck& Right)
{
    data = Right.data();
}

void FPbGetRoleTreasuryDataAck::ToPb(idlepb::GetRoleTreasuryDataAck* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbGetRoleTreasuryDataAck::Reset()
{
    data = FPbRoleTreasurySaveData();    
}

void FPbGetRoleTreasuryDataAck::operator=(const idlepb::GetRoleTreasuryDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleTreasuryDataAck::operator==(const FPbGetRoleTreasuryDataAck& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbGetRoleTreasuryDataAck::operator!=(const FPbGetRoleTreasuryDataAck& Right) const
{
    return !operator==(Right);
}

FPbOpenTreasuryChestReq::FPbOpenTreasuryChestReq()
{
    Reset();        
}

FPbOpenTreasuryChestReq::FPbOpenTreasuryChestReq(const idlepb::OpenTreasuryChestReq& Right)
{
    this->FromPb(Right);
}

void FPbOpenTreasuryChestReq::FromPb(const idlepb::OpenTreasuryChestReq& Right)
{
    chest_type = Right.chest_type();
    num = Right.num();
}

void FPbOpenTreasuryChestReq::ToPb(idlepb::OpenTreasuryChestReq* Out) const
{
    Out->set_chest_type(chest_type);
    Out->set_num(num);    
}

void FPbOpenTreasuryChestReq::Reset()
{
    chest_type = int32();
    num = int32();    
}

void FPbOpenTreasuryChestReq::operator=(const idlepb::OpenTreasuryChestReq& Right)
{
    this->FromPb(Right);
}

bool FPbOpenTreasuryChestReq::operator==(const FPbOpenTreasuryChestReq& Right) const
{
    if (this->chest_type != Right.chest_type)
        return false;
    if (this->num != Right.num)
        return false;
    return true;
}

bool FPbOpenTreasuryChestReq::operator!=(const FPbOpenTreasuryChestReq& Right) const
{
    return !operator==(Right);
}

FPbOpenTreasuryChestAck::FPbOpenTreasuryChestAck()
{
    Reset();        
}

FPbOpenTreasuryChestAck::FPbOpenTreasuryChestAck(const idlepb::OpenTreasuryChestAck& Right)
{
    this->FromPb(Right);
}

void FPbOpenTreasuryChestAck::FromPb(const idlepb::OpenTreasuryChestAck& Right)
{
    ok = Right.ok();
}

void FPbOpenTreasuryChestAck::ToPb(idlepb::OpenTreasuryChestAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbOpenTreasuryChestAck::Reset()
{
    ok = bool();    
}

void FPbOpenTreasuryChestAck::operator=(const idlepb::OpenTreasuryChestAck& Right)
{
    this->FromPb(Right);
}

bool FPbOpenTreasuryChestAck::operator==(const FPbOpenTreasuryChestAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbOpenTreasuryChestAck::operator!=(const FPbOpenTreasuryChestAck& Right) const
{
    return !operator==(Right);
}

FPbOneClickOpenTreasuryChestReq::FPbOneClickOpenTreasuryChestReq()
{
    Reset();        
}

FPbOneClickOpenTreasuryChestReq::FPbOneClickOpenTreasuryChestReq(const idlepb::OneClickOpenTreasuryChestReq& Right)
{
    this->FromPb(Right);
}

void FPbOneClickOpenTreasuryChestReq::FromPb(const idlepb::OneClickOpenTreasuryChestReq& Right)
{
}

void FPbOneClickOpenTreasuryChestReq::ToPb(idlepb::OneClickOpenTreasuryChestReq* Out) const
{    
}

void FPbOneClickOpenTreasuryChestReq::Reset()
{    
}

void FPbOneClickOpenTreasuryChestReq::operator=(const idlepb::OneClickOpenTreasuryChestReq& Right)
{
    this->FromPb(Right);
}

bool FPbOneClickOpenTreasuryChestReq::operator==(const FPbOneClickOpenTreasuryChestReq& Right) const
{
    return true;
}

bool FPbOneClickOpenTreasuryChestReq::operator!=(const FPbOneClickOpenTreasuryChestReq& Right) const
{
    return !operator==(Right);
}

FPbOneClickOpenTreasuryChestAck::FPbOneClickOpenTreasuryChestAck()
{
    Reset();        
}

FPbOneClickOpenTreasuryChestAck::FPbOneClickOpenTreasuryChestAck(const idlepb::OneClickOpenTreasuryChestAck& Right)
{
    this->FromPb(Right);
}

void FPbOneClickOpenTreasuryChestAck::FromPb(const idlepb::OneClickOpenTreasuryChestAck& Right)
{
    ok = Right.ok();
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

void FPbOneClickOpenTreasuryChestAck::ToPb(idlepb::OneClickOpenTreasuryChestAck* Out) const
{
    Out->set_ok(ok);
    for (const auto& Elem : today_open_times)
    {
        Out->add_today_open_times(Elem);    
    }
    for (const auto& Elem : guarantee_count)
    {
        Out->add_guarantee_count(Elem);    
    }    
}

void FPbOneClickOpenTreasuryChestAck::Reset()
{
    ok = bool();
    today_open_times = TArray<int32>();
    guarantee_count = TArray<int32>();    
}

void FPbOneClickOpenTreasuryChestAck::operator=(const idlepb::OneClickOpenTreasuryChestAck& Right)
{
    this->FromPb(Right);
}

bool FPbOneClickOpenTreasuryChestAck::operator==(const FPbOneClickOpenTreasuryChestAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    if (this->today_open_times != Right.today_open_times)
        return false;
    if (this->guarantee_count != Right.guarantee_count)
        return false;
    return true;
}

bool FPbOneClickOpenTreasuryChestAck::operator!=(const FPbOneClickOpenTreasuryChestAck& Right) const
{
    return !operator==(Right);
}

FPbOpenTreasuryGachaReq::FPbOpenTreasuryGachaReq()
{
    Reset();        
}

FPbOpenTreasuryGachaReq::FPbOpenTreasuryGachaReq(const idlepb::OpenTreasuryGachaReq& Right)
{
    this->FromPb(Right);
}

void FPbOpenTreasuryGachaReq::FromPb(const idlepb::OpenTreasuryGachaReq& Right)
{
    gacha_type = Right.gacha_type();
    num = Right.num();
}

void FPbOpenTreasuryGachaReq::ToPb(idlepb::OpenTreasuryGachaReq* Out) const
{
    Out->set_gacha_type(gacha_type);
    Out->set_num(num);    
}

void FPbOpenTreasuryGachaReq::Reset()
{
    gacha_type = int32();
    num = int32();    
}

void FPbOpenTreasuryGachaReq::operator=(const idlepb::OpenTreasuryGachaReq& Right)
{
    this->FromPb(Right);
}

bool FPbOpenTreasuryGachaReq::operator==(const FPbOpenTreasuryGachaReq& Right) const
{
    if (this->gacha_type != Right.gacha_type)
        return false;
    if (this->num != Right.num)
        return false;
    return true;
}

bool FPbOpenTreasuryGachaReq::operator!=(const FPbOpenTreasuryGachaReq& Right) const
{
    return !operator==(Right);
}

FPbOpenTreasuryGachaAck::FPbOpenTreasuryGachaAck()
{
    Reset();        
}

FPbOpenTreasuryGachaAck::FPbOpenTreasuryGachaAck(const idlepb::OpenTreasuryGachaAck& Right)
{
    this->FromPb(Right);
}

void FPbOpenTreasuryGachaAck::FromPb(const idlepb::OpenTreasuryGachaAck& Right)
{
    ok = Right.ok();
    free = Right.free();
}

void FPbOpenTreasuryGachaAck::ToPb(idlepb::OpenTreasuryGachaAck* Out) const
{
    Out->set_ok(ok);
    Out->set_free(free);    
}

void FPbOpenTreasuryGachaAck::Reset()
{
    ok = bool();
    free = bool();    
}

void FPbOpenTreasuryGachaAck::operator=(const idlepb::OpenTreasuryGachaAck& Right)
{
    this->FromPb(Right);
}

bool FPbOpenTreasuryGachaAck::operator==(const FPbOpenTreasuryGachaAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    if (this->free != Right.free)
        return false;
    return true;
}

bool FPbOpenTreasuryGachaAck::operator!=(const FPbOpenTreasuryGachaAck& Right) const
{
    return !operator==(Right);
}

FPbRefreshTreasuryShopReq::FPbRefreshTreasuryShopReq()
{
    Reset();        
}

FPbRefreshTreasuryShopReq::FPbRefreshTreasuryShopReq(const idlepb::RefreshTreasuryShopReq& Right)
{
    this->FromPb(Right);
}

void FPbRefreshTreasuryShopReq::FromPb(const idlepb::RefreshTreasuryShopReq& Right)
{
}

void FPbRefreshTreasuryShopReq::ToPb(idlepb::RefreshTreasuryShopReq* Out) const
{    
}

void FPbRefreshTreasuryShopReq::Reset()
{    
}

void FPbRefreshTreasuryShopReq::operator=(const idlepb::RefreshTreasuryShopReq& Right)
{
    this->FromPb(Right);
}

bool FPbRefreshTreasuryShopReq::operator==(const FPbRefreshTreasuryShopReq& Right) const
{
    return true;
}

bool FPbRefreshTreasuryShopReq::operator!=(const FPbRefreshTreasuryShopReq& Right) const
{
    return !operator==(Right);
}

FPbRefreshTreasuryShopAck::FPbRefreshTreasuryShopAck()
{
    Reset();        
}

FPbRefreshTreasuryShopAck::FPbRefreshTreasuryShopAck(const idlepb::RefreshTreasuryShopAck& Right)
{
    this->FromPb(Right);
}

void FPbRefreshTreasuryShopAck::FromPb(const idlepb::RefreshTreasuryShopAck& Right)
{
    ok = Right.ok();
    items.Empty();
    for (const auto& Elem : Right.items())
    {
        items.Emplace(Elem);
    }
}

void FPbRefreshTreasuryShopAck::ToPb(idlepb::RefreshTreasuryShopAck* Out) const
{
    Out->set_ok(ok);
    for (const auto& Elem : items)
    {
        Elem.ToPb(Out->add_items());    
    }    
}

void FPbRefreshTreasuryShopAck::Reset()
{
    ok = bool();
    items = TArray<FPbTreasuryShopItem>();    
}

void FPbRefreshTreasuryShopAck::operator=(const idlepb::RefreshTreasuryShopAck& Right)
{
    this->FromPb(Right);
}

bool FPbRefreshTreasuryShopAck::operator==(const FPbRefreshTreasuryShopAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    if (this->items != Right.items)
        return false;
    return true;
}

bool FPbRefreshTreasuryShopAck::operator!=(const FPbRefreshTreasuryShopAck& Right) const
{
    return !operator==(Right);
}

FPbTreasuryShopBuyReq::FPbTreasuryShopBuyReq()
{
    Reset();        
}

FPbTreasuryShopBuyReq::FPbTreasuryShopBuyReq(const idlepb::TreasuryShopBuyReq& Right)
{
    this->FromPb(Right);
}

void FPbTreasuryShopBuyReq::FromPb(const idlepb::TreasuryShopBuyReq& Right)
{
    index = Right.index();
}

void FPbTreasuryShopBuyReq::ToPb(idlepb::TreasuryShopBuyReq* Out) const
{
    Out->set_index(index);    
}

void FPbTreasuryShopBuyReq::Reset()
{
    index = int32();    
}

void FPbTreasuryShopBuyReq::operator=(const idlepb::TreasuryShopBuyReq& Right)
{
    this->FromPb(Right);
}

bool FPbTreasuryShopBuyReq::operator==(const FPbTreasuryShopBuyReq& Right) const
{
    if (this->index != Right.index)
        return false;
    return true;
}

bool FPbTreasuryShopBuyReq::operator!=(const FPbTreasuryShopBuyReq& Right) const
{
    return !operator==(Right);
}

FPbTreasuryShopBuyAck::FPbTreasuryShopBuyAck()
{
    Reset();        
}

FPbTreasuryShopBuyAck::FPbTreasuryShopBuyAck(const idlepb::TreasuryShopBuyAck& Right)
{
    this->FromPb(Right);
}

void FPbTreasuryShopBuyAck::FromPb(const idlepb::TreasuryShopBuyAck& Right)
{
    ok = Right.ok();
}

void FPbTreasuryShopBuyAck::ToPb(idlepb::TreasuryShopBuyAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbTreasuryShopBuyAck::Reset()
{
    ok = bool();    
}

void FPbTreasuryShopBuyAck::operator=(const idlepb::TreasuryShopBuyAck& Right)
{
    this->FromPb(Right);
}

bool FPbTreasuryShopBuyAck::operator==(const FPbTreasuryShopBuyAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbTreasuryShopBuyAck::operator!=(const FPbTreasuryShopBuyAck& Right) const
{
    return !operator==(Right);
}

FPbGetLifeCounterDataReq::FPbGetLifeCounterDataReq()
{
    Reset();        
}

FPbGetLifeCounterDataReq::FPbGetLifeCounterDataReq(const idlepb::GetLifeCounterDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetLifeCounterDataReq::FromPb(const idlepb::GetLifeCounterDataReq& Right)
{
}

void FPbGetLifeCounterDataReq::ToPb(idlepb::GetLifeCounterDataReq* Out) const
{    
}

void FPbGetLifeCounterDataReq::Reset()
{    
}

void FPbGetLifeCounterDataReq::operator=(const idlepb::GetLifeCounterDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetLifeCounterDataReq::operator==(const FPbGetLifeCounterDataReq& Right) const
{
    return true;
}

bool FPbGetLifeCounterDataReq::operator!=(const FPbGetLifeCounterDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetLifeCounterDataAck::FPbGetLifeCounterDataAck()
{
    Reset();        
}

FPbGetLifeCounterDataAck::FPbGetLifeCounterDataAck(const idlepb::GetLifeCounterDataAck& Right)
{
    this->FromPb(Right);
}

void FPbGetLifeCounterDataAck::FromPb(const idlepb::GetLifeCounterDataAck& Right)
{
    data = Right.data();
}

void FPbGetLifeCounterDataAck::ToPb(idlepb::GetLifeCounterDataAck* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbGetLifeCounterDataAck::Reset()
{
    data = FPbRoleLifeCounterData();    
}

void FPbGetLifeCounterDataAck::operator=(const idlepb::GetLifeCounterDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetLifeCounterDataAck::operator==(const FPbGetLifeCounterDataAck& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbGetLifeCounterDataAck::operator!=(const FPbGetLifeCounterDataAck& Right) const
{
    return !operator==(Right);
}

FPbUpdateLifeCounter::FPbUpdateLifeCounter()
{
    Reset();        
}

FPbUpdateLifeCounter::FPbUpdateLifeCounter(const idlepb::UpdateLifeCounter& Right)
{
    this->FromPb(Right);
}

void FPbUpdateLifeCounter::FromPb(const idlepb::UpdateLifeCounter& Right)
{
    function_type = Right.function_type();
    target_id = Right.target_id();
    new_num = Right.new_num();
}

void FPbUpdateLifeCounter::ToPb(idlepb::UpdateLifeCounter* Out) const
{
    Out->set_function_type(function_type);
    Out->set_target_id(target_id);
    Out->set_new_num(new_num);    
}

void FPbUpdateLifeCounter::Reset()
{
    function_type = int32();
    target_id = int32();
    new_num = int64();    
}

void FPbUpdateLifeCounter::operator=(const idlepb::UpdateLifeCounter& Right)
{
    this->FromPb(Right);
}

bool FPbUpdateLifeCounter::operator==(const FPbUpdateLifeCounter& Right) const
{
    if (this->function_type != Right.function_type)
        return false;
    if (this->target_id != Right.target_id)
        return false;
    if (this->new_num != Right.new_num)
        return false;
    return true;
}

bool FPbUpdateLifeCounter::operator!=(const FPbUpdateLifeCounter& Right) const
{
    return !operator==(Right);
}

FPbDoQuestFightReq::FPbDoQuestFightReq()
{
    Reset();        
}

FPbDoQuestFightReq::FPbDoQuestFightReq(const idlepb::DoQuestFightReq& Right)
{
    this->FromPb(Right);
}

void FPbDoQuestFightReq::FromPb(const idlepb::DoQuestFightReq& Right)
{
    quest_id = Right.quest_id();
}

void FPbDoQuestFightReq::ToPb(idlepb::DoQuestFightReq* Out) const
{
    Out->set_quest_id(quest_id);    
}

void FPbDoQuestFightReq::Reset()
{
    quest_id = int32();    
}

void FPbDoQuestFightReq::operator=(const idlepb::DoQuestFightReq& Right)
{
    this->FromPb(Right);
}

bool FPbDoQuestFightReq::operator==(const FPbDoQuestFightReq& Right) const
{
    if (this->quest_id != Right.quest_id)
        return false;
    return true;
}

bool FPbDoQuestFightReq::operator!=(const FPbDoQuestFightReq& Right) const
{
    return !operator==(Right);
}

FPbDoQuestFightAck::FPbDoQuestFightAck()
{
    Reset();        
}

FPbDoQuestFightAck::FPbDoQuestFightAck(const idlepb::DoQuestFightAck& Right)
{
    this->FromPb(Right);
}

void FPbDoQuestFightAck::FromPb(const idlepb::DoQuestFightAck& Right)
{
    ok = Right.ok();
}

void FPbDoQuestFightAck::ToPb(idlepb::DoQuestFightAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbDoQuestFightAck::Reset()
{
    ok = bool();    
}

void FPbDoQuestFightAck::operator=(const idlepb::DoQuestFightAck& Right)
{
    this->FromPb(Right);
}

bool FPbDoQuestFightAck::operator==(const FPbDoQuestFightAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbDoQuestFightAck::operator!=(const FPbDoQuestFightAck& Right) const
{
    return !operator==(Right);
}

FPbQuestFightQuickEndReq::FPbQuestFightQuickEndReq()
{
    Reset();        
}

FPbQuestFightQuickEndReq::FPbQuestFightQuickEndReq(const idlepb::QuestFightQuickEndReq& Right)
{
    this->FromPb(Right);
}

void FPbQuestFightQuickEndReq::FromPb(const idlepb::QuestFightQuickEndReq& Right)
{
    is_exit = Right.is_exit();
}

void FPbQuestFightQuickEndReq::ToPb(idlepb::QuestFightQuickEndReq* Out) const
{
    Out->set_is_exit(is_exit);    
}

void FPbQuestFightQuickEndReq::Reset()
{
    is_exit = bool();    
}

void FPbQuestFightQuickEndReq::operator=(const idlepb::QuestFightQuickEndReq& Right)
{
    this->FromPb(Right);
}

bool FPbQuestFightQuickEndReq::operator==(const FPbQuestFightQuickEndReq& Right) const
{
    if (this->is_exit != Right.is_exit)
        return false;
    return true;
}

bool FPbQuestFightQuickEndReq::operator!=(const FPbQuestFightQuickEndReq& Right) const
{
    return !operator==(Right);
}

FPbQuestFightQuickEndAck::FPbQuestFightQuickEndAck()
{
    Reset();        
}

FPbQuestFightQuickEndAck::FPbQuestFightQuickEndAck(const idlepb::QuestFightQuickEndAck& Right)
{
    this->FromPb(Right);
}

void FPbQuestFightQuickEndAck::FromPb(const idlepb::QuestFightQuickEndAck& Right)
{
}

void FPbQuestFightQuickEndAck::ToPb(idlepb::QuestFightQuickEndAck* Out) const
{    
}

void FPbQuestFightQuickEndAck::Reset()
{    
}

void FPbQuestFightQuickEndAck::operator=(const idlepb::QuestFightQuickEndAck& Right)
{
    this->FromPb(Right);
}

bool FPbQuestFightQuickEndAck::operator==(const FPbQuestFightQuickEndAck& Right) const
{
    return true;
}

bool FPbQuestFightQuickEndAck::operator!=(const FPbQuestFightQuickEndAck& Right) const
{
    return !operator==(Right);
}

FPbNotifyQuestFightChallengeOver::FPbNotifyQuestFightChallengeOver()
{
    Reset();        
}

FPbNotifyQuestFightChallengeOver::FPbNotifyQuestFightChallengeOver(const idlepb::NotifyQuestFightChallengeOver& Right)
{
    this->FromPb(Right);
}

void FPbNotifyQuestFightChallengeOver::FromPb(const idlepb::NotifyQuestFightChallengeOver& Right)
{
    quest_id = Right.quest_id();
    win = Right.win();
}

void FPbNotifyQuestFightChallengeOver::ToPb(idlepb::NotifyQuestFightChallengeOver* Out) const
{
    Out->set_quest_id(quest_id);
    Out->set_win(win);    
}

void FPbNotifyQuestFightChallengeOver::Reset()
{
    quest_id = int32();
    win = bool();    
}

void FPbNotifyQuestFightChallengeOver::operator=(const idlepb::NotifyQuestFightChallengeOver& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyQuestFightChallengeOver::operator==(const FPbNotifyQuestFightChallengeOver& Right) const
{
    if (this->quest_id != Right.quest_id)
        return false;
    if (this->win != Right.win)
        return false;
    return true;
}

bool FPbNotifyQuestFightChallengeOver::operator!=(const FPbNotifyQuestFightChallengeOver& Right) const
{
    return !operator==(Right);
}

FPbGetAppearanceDataReq::FPbGetAppearanceDataReq()
{
    Reset();        
}

FPbGetAppearanceDataReq::FPbGetAppearanceDataReq(const idlepb::GetAppearanceDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetAppearanceDataReq::FromPb(const idlepb::GetAppearanceDataReq& Right)
{
}

void FPbGetAppearanceDataReq::ToPb(idlepb::GetAppearanceDataReq* Out) const
{    
}

void FPbGetAppearanceDataReq::Reset()
{    
}

void FPbGetAppearanceDataReq::operator=(const idlepb::GetAppearanceDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetAppearanceDataReq::operator==(const FPbGetAppearanceDataReq& Right) const
{
    return true;
}

bool FPbGetAppearanceDataReq::operator!=(const FPbGetAppearanceDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetAppearanceDataAck::FPbGetAppearanceDataAck()
{
    Reset();        
}

FPbGetAppearanceDataAck::FPbGetAppearanceDataAck(const idlepb::GetAppearanceDataAck& Right)
{
    this->FromPb(Right);
}

void FPbGetAppearanceDataAck::FromPb(const idlepb::GetAppearanceDataAck& Right)
{
    data = Right.data();
}

void FPbGetAppearanceDataAck::ToPb(idlepb::GetAppearanceDataAck* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbGetAppearanceDataAck::Reset()
{
    data = FPbRoleAppearanceData();    
}

void FPbGetAppearanceDataAck::operator=(const idlepb::GetAppearanceDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetAppearanceDataAck::operator==(const FPbGetAppearanceDataAck& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbGetAppearanceDataAck::operator!=(const FPbGetAppearanceDataAck& Right) const
{
    return !operator==(Right);
}

FPbAppearanceAddReq::FPbAppearanceAddReq()
{
    Reset();        
}

FPbAppearanceAddReq::FPbAppearanceAddReq(const idlepb::AppearanceAddReq& Right)
{
    this->FromPb(Right);
}

void FPbAppearanceAddReq::FromPb(const idlepb::AppearanceAddReq& Right)
{
    item_id = Right.item_id();
}

void FPbAppearanceAddReq::ToPb(idlepb::AppearanceAddReq* Out) const
{
    Out->set_item_id(item_id);    
}

void FPbAppearanceAddReq::Reset()
{
    item_id = int32();    
}

void FPbAppearanceAddReq::operator=(const idlepb::AppearanceAddReq& Right)
{
    this->FromPb(Right);
}

bool FPbAppearanceAddReq::operator==(const FPbAppearanceAddReq& Right) const
{
    if (this->item_id != Right.item_id)
        return false;
    return true;
}

bool FPbAppearanceAddReq::operator!=(const FPbAppearanceAddReq& Right) const
{
    return !operator==(Right);
}

FPbAppearanceAddAck::FPbAppearanceAddAck()
{
    Reset();        
}

FPbAppearanceAddAck::FPbAppearanceAddAck(const idlepb::AppearanceAddAck& Right)
{
    this->FromPb(Right);
}

void FPbAppearanceAddAck::FromPb(const idlepb::AppearanceAddAck& Right)
{
    ok = Right.ok();
}

void FPbAppearanceAddAck::ToPb(idlepb::AppearanceAddAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbAppearanceAddAck::Reset()
{
    ok = bool();    
}

void FPbAppearanceAddAck::operator=(const idlepb::AppearanceAddAck& Right)
{
    this->FromPb(Right);
}

bool FPbAppearanceAddAck::operator==(const FPbAppearanceAddAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbAppearanceAddAck::operator!=(const FPbAppearanceAddAck& Right) const
{
    return !operator==(Right);
}

FPbAppearanceActiveReq::FPbAppearanceActiveReq()
{
    Reset();        
}

FPbAppearanceActiveReq::FPbAppearanceActiveReq(const idlepb::AppearanceActiveReq& Right)
{
    this->FromPb(Right);
}

void FPbAppearanceActiveReq::FromPb(const idlepb::AppearanceActiveReq& Right)
{
    group_id = Right.group_id();
}

void FPbAppearanceActiveReq::ToPb(idlepb::AppearanceActiveReq* Out) const
{
    Out->set_group_id(group_id);    
}

void FPbAppearanceActiveReq::Reset()
{
    group_id = int32();    
}

void FPbAppearanceActiveReq::operator=(const idlepb::AppearanceActiveReq& Right)
{
    this->FromPb(Right);
}

bool FPbAppearanceActiveReq::operator==(const FPbAppearanceActiveReq& Right) const
{
    if (this->group_id != Right.group_id)
        return false;
    return true;
}

bool FPbAppearanceActiveReq::operator!=(const FPbAppearanceActiveReq& Right) const
{
    return !operator==(Right);
}

FPbAppearanceActiveAck::FPbAppearanceActiveAck()
{
    Reset();        
}

FPbAppearanceActiveAck::FPbAppearanceActiveAck(const idlepb::AppearanceActiveAck& Right)
{
    this->FromPb(Right);
}

void FPbAppearanceActiveAck::FromPb(const idlepb::AppearanceActiveAck& Right)
{
    ok = Right.ok();
}

void FPbAppearanceActiveAck::ToPb(idlepb::AppearanceActiveAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbAppearanceActiveAck::Reset()
{
    ok = bool();    
}

void FPbAppearanceActiveAck::operator=(const idlepb::AppearanceActiveAck& Right)
{
    this->FromPb(Right);
}

bool FPbAppearanceActiveAck::operator==(const FPbAppearanceActiveAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbAppearanceActiveAck::operator!=(const FPbAppearanceActiveAck& Right) const
{
    return !operator==(Right);
}

FPbAppearanceWearReq::FPbAppearanceWearReq()
{
    Reset();        
}

FPbAppearanceWearReq::FPbAppearanceWearReq(const idlepb::AppearanceWearReq& Right)
{
    this->FromPb(Right);
}

void FPbAppearanceWearReq::FromPb(const idlepb::AppearanceWearReq& Right)
{
    group_id = Right.group_id();
}

void FPbAppearanceWearReq::ToPb(idlepb::AppearanceWearReq* Out) const
{
    Out->set_group_id(group_id);    
}

void FPbAppearanceWearReq::Reset()
{
    group_id = int32();    
}

void FPbAppearanceWearReq::operator=(const idlepb::AppearanceWearReq& Right)
{
    this->FromPb(Right);
}

bool FPbAppearanceWearReq::operator==(const FPbAppearanceWearReq& Right) const
{
    if (this->group_id != Right.group_id)
        return false;
    return true;
}

bool FPbAppearanceWearReq::operator!=(const FPbAppearanceWearReq& Right) const
{
    return !operator==(Right);
}

FPbAppearanceWearAck::FPbAppearanceWearAck()
{
    Reset();        
}

FPbAppearanceWearAck::FPbAppearanceWearAck(const idlepb::AppearanceWearAck& Right)
{
    this->FromPb(Right);
}

void FPbAppearanceWearAck::FromPb(const idlepb::AppearanceWearAck& Right)
{
    ok = Right.ok();
}

void FPbAppearanceWearAck::ToPb(idlepb::AppearanceWearAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbAppearanceWearAck::Reset()
{
    ok = bool();    
}

void FPbAppearanceWearAck::operator=(const idlepb::AppearanceWearAck& Right)
{
    this->FromPb(Right);
}

bool FPbAppearanceWearAck::operator==(const FPbAppearanceWearAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbAppearanceWearAck::operator!=(const FPbAppearanceWearAck& Right) const
{
    return !operator==(Right);
}

FPbAppearanceChangeSkTypeReq::FPbAppearanceChangeSkTypeReq()
{
    Reset();        
}

FPbAppearanceChangeSkTypeReq::FPbAppearanceChangeSkTypeReq(const idlepb::AppearanceChangeSkTypeReq& Right)
{
    this->FromPb(Right);
}

void FPbAppearanceChangeSkTypeReq::FromPb(const idlepb::AppearanceChangeSkTypeReq& Right)
{
    sk_type = Right.sk_type();
}

void FPbAppearanceChangeSkTypeReq::ToPb(idlepb::AppearanceChangeSkTypeReq* Out) const
{
    Out->set_sk_type(sk_type);    
}

void FPbAppearanceChangeSkTypeReq::Reset()
{
    sk_type = int32();    
}

void FPbAppearanceChangeSkTypeReq::operator=(const idlepb::AppearanceChangeSkTypeReq& Right)
{
    this->FromPb(Right);
}

bool FPbAppearanceChangeSkTypeReq::operator==(const FPbAppearanceChangeSkTypeReq& Right) const
{
    if (this->sk_type != Right.sk_type)
        return false;
    return true;
}

bool FPbAppearanceChangeSkTypeReq::operator!=(const FPbAppearanceChangeSkTypeReq& Right) const
{
    return !operator==(Right);
}

FPbAppearanceChangeSkTypeAck::FPbAppearanceChangeSkTypeAck()
{
    Reset();        
}

FPbAppearanceChangeSkTypeAck::FPbAppearanceChangeSkTypeAck(const idlepb::AppearanceChangeSkTypeAck& Right)
{
    this->FromPb(Right);
}

void FPbAppearanceChangeSkTypeAck::FromPb(const idlepb::AppearanceChangeSkTypeAck& Right)
{
    ok = Right.ok();
}

void FPbAppearanceChangeSkTypeAck::ToPb(idlepb::AppearanceChangeSkTypeAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbAppearanceChangeSkTypeAck::Reset()
{
    ok = bool();    
}

void FPbAppearanceChangeSkTypeAck::operator=(const idlepb::AppearanceChangeSkTypeAck& Right)
{
    this->FromPb(Right);
}

bool FPbAppearanceChangeSkTypeAck::operator==(const FPbAppearanceChangeSkTypeAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbAppearanceChangeSkTypeAck::operator!=(const FPbAppearanceChangeSkTypeAck& Right) const
{
    return !operator==(Right);
}

FPbAppearanceBuyReq::FPbAppearanceBuyReq()
{
    Reset();        
}

FPbAppearanceBuyReq::FPbAppearanceBuyReq(const idlepb::AppearanceBuyReq& Right)
{
    this->FromPb(Right);
}

void FPbAppearanceBuyReq::FromPb(const idlepb::AppearanceBuyReq& Right)
{
    shop_index = Right.shop_index();
    item_index = Right.item_index();
}

void FPbAppearanceBuyReq::ToPb(idlepb::AppearanceBuyReq* Out) const
{
    Out->set_shop_index(shop_index);
    Out->set_item_index(item_index);    
}

void FPbAppearanceBuyReq::Reset()
{
    shop_index = int32();
    item_index = int32();    
}

void FPbAppearanceBuyReq::operator=(const idlepb::AppearanceBuyReq& Right)
{
    this->FromPb(Right);
}

bool FPbAppearanceBuyReq::operator==(const FPbAppearanceBuyReq& Right) const
{
    if (this->shop_index != Right.shop_index)
        return false;
    if (this->item_index != Right.item_index)
        return false;
    return true;
}

bool FPbAppearanceBuyReq::operator!=(const FPbAppearanceBuyReq& Right) const
{
    return !operator==(Right);
}

FPbAppearanceBuyAck::FPbAppearanceBuyAck()
{
    Reset();        
}

FPbAppearanceBuyAck::FPbAppearanceBuyAck(const idlepb::AppearanceBuyAck& Right)
{
    this->FromPb(Right);
}

void FPbAppearanceBuyAck::FromPb(const idlepb::AppearanceBuyAck& Right)
{
    ok = Right.ok();
}

void FPbAppearanceBuyAck::ToPb(idlepb::AppearanceBuyAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbAppearanceBuyAck::Reset()
{
    ok = bool();    
}

void FPbAppearanceBuyAck::operator=(const idlepb::AppearanceBuyAck& Right)
{
    this->FromPb(Right);
}

bool FPbAppearanceBuyAck::operator==(const FPbAppearanceBuyAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbAppearanceBuyAck::operator!=(const FPbAppearanceBuyAck& Right) const
{
    return !operator==(Right);
}

FPbGetArenaCheckListDataReq::FPbGetArenaCheckListDataReq()
{
    Reset();        
}

FPbGetArenaCheckListDataReq::FPbGetArenaCheckListDataReq(const idlepb::GetArenaCheckListDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetArenaCheckListDataReq::FromPb(const idlepb::GetArenaCheckListDataReq& Right)
{
}

void FPbGetArenaCheckListDataReq::ToPb(idlepb::GetArenaCheckListDataReq* Out) const
{    
}

void FPbGetArenaCheckListDataReq::Reset()
{    
}

void FPbGetArenaCheckListDataReq::operator=(const idlepb::GetArenaCheckListDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetArenaCheckListDataReq::operator==(const FPbGetArenaCheckListDataReq& Right) const
{
    return true;
}

bool FPbGetArenaCheckListDataReq::operator!=(const FPbGetArenaCheckListDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetArenaCheckListDataAck::FPbGetArenaCheckListDataAck()
{
    Reset();        
}

FPbGetArenaCheckListDataAck::FPbGetArenaCheckListDataAck(const idlepb::GetArenaCheckListDataAck& Right)
{
    this->FromPb(Right);
}

void FPbGetArenaCheckListDataAck::FromPb(const idlepb::GetArenaCheckListDataAck& Right)
{
    data = Right.data();
}

void FPbGetArenaCheckListDataAck::ToPb(idlepb::GetArenaCheckListDataAck* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbGetArenaCheckListDataAck::Reset()
{
    data = FPbRoleArenaCheckListData();    
}

void FPbGetArenaCheckListDataAck::operator=(const idlepb::GetArenaCheckListDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetArenaCheckListDataAck::operator==(const FPbGetArenaCheckListDataAck& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbGetArenaCheckListDataAck::operator!=(const FPbGetArenaCheckListDataAck& Right) const
{
    return !operator==(Right);
}

FPbArenaCheckListSubmitReq::FPbArenaCheckListSubmitReq()
{
    Reset();        
}

FPbArenaCheckListSubmitReq::FPbArenaCheckListSubmitReq(const idlepb::ArenaCheckListSubmitReq& Right)
{
    this->FromPb(Right);
}

void FPbArenaCheckListSubmitReq::FromPb(const idlepb::ArenaCheckListSubmitReq& Right)
{
    check_list_id = Right.check_list_id();
}

void FPbArenaCheckListSubmitReq::ToPb(idlepb::ArenaCheckListSubmitReq* Out) const
{
    Out->set_check_list_id(check_list_id);    
}

void FPbArenaCheckListSubmitReq::Reset()
{
    check_list_id = int32();    
}

void FPbArenaCheckListSubmitReq::operator=(const idlepb::ArenaCheckListSubmitReq& Right)
{
    this->FromPb(Right);
}

bool FPbArenaCheckListSubmitReq::operator==(const FPbArenaCheckListSubmitReq& Right) const
{
    if (this->check_list_id != Right.check_list_id)
        return false;
    return true;
}

bool FPbArenaCheckListSubmitReq::operator!=(const FPbArenaCheckListSubmitReq& Right) const
{
    return !operator==(Right);
}

FPbArenaCheckListSubmitAck::FPbArenaCheckListSubmitAck()
{
    Reset();        
}

FPbArenaCheckListSubmitAck::FPbArenaCheckListSubmitAck(const idlepb::ArenaCheckListSubmitAck& Right)
{
    this->FromPb(Right);
}

void FPbArenaCheckListSubmitAck::FromPb(const idlepb::ArenaCheckListSubmitAck& Right)
{
    ok = Right.ok();
    data = Right.data();
}

void FPbArenaCheckListSubmitAck::ToPb(idlepb::ArenaCheckListSubmitAck* Out) const
{
    Out->set_ok(ok);
    data.ToPb(Out->mutable_data());    
}

void FPbArenaCheckListSubmitAck::Reset()
{
    ok = bool();
    data = FPbArenaCheckListData();    
}

void FPbArenaCheckListSubmitAck::operator=(const idlepb::ArenaCheckListSubmitAck& Right)
{
    this->FromPb(Right);
}

bool FPbArenaCheckListSubmitAck::operator==(const FPbArenaCheckListSubmitAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbArenaCheckListSubmitAck::operator!=(const FPbArenaCheckListSubmitAck& Right) const
{
    return !operator==(Right);
}

FPbArenaCheckListRewardSubmitReq::FPbArenaCheckListRewardSubmitReq()
{
    Reset();        
}

FPbArenaCheckListRewardSubmitReq::FPbArenaCheckListRewardSubmitReq(const idlepb::ArenaCheckListRewardSubmitReq& Right)
{
    this->FromPb(Right);
}

void FPbArenaCheckListRewardSubmitReq::FromPb(const idlepb::ArenaCheckListRewardSubmitReq& Right)
{
    reward_id = Right.reward_id();
}

void FPbArenaCheckListRewardSubmitReq::ToPb(idlepb::ArenaCheckListRewardSubmitReq* Out) const
{
    Out->set_reward_id(reward_id);    
}

void FPbArenaCheckListRewardSubmitReq::Reset()
{
    reward_id = int32();    
}

void FPbArenaCheckListRewardSubmitReq::operator=(const idlepb::ArenaCheckListRewardSubmitReq& Right)
{
    this->FromPb(Right);
}

bool FPbArenaCheckListRewardSubmitReq::operator==(const FPbArenaCheckListRewardSubmitReq& Right) const
{
    if (this->reward_id != Right.reward_id)
        return false;
    return true;
}

bool FPbArenaCheckListRewardSubmitReq::operator!=(const FPbArenaCheckListRewardSubmitReq& Right) const
{
    return !operator==(Right);
}

FPbArenaCheckListRewardSubmitAck::FPbArenaCheckListRewardSubmitAck()
{
    Reset();        
}

FPbArenaCheckListRewardSubmitAck::FPbArenaCheckListRewardSubmitAck(const idlepb::ArenaCheckListRewardSubmitAck& Right)
{
    this->FromPb(Right);
}

void FPbArenaCheckListRewardSubmitAck::FromPb(const idlepb::ArenaCheckListRewardSubmitAck& Right)
{
    ok = Right.ok();
    data = Right.data();
}

void FPbArenaCheckListRewardSubmitAck::ToPb(idlepb::ArenaCheckListRewardSubmitAck* Out) const
{
    Out->set_ok(ok);
    data.ToPb(Out->mutable_data());    
}

void FPbArenaCheckListRewardSubmitAck::Reset()
{
    ok = bool();
    data = FPbArenaCheckListRewardData();    
}

void FPbArenaCheckListRewardSubmitAck::operator=(const idlepb::ArenaCheckListRewardSubmitAck& Right)
{
    this->FromPb(Right);
}

bool FPbArenaCheckListRewardSubmitAck::operator==(const FPbArenaCheckListRewardSubmitAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbArenaCheckListRewardSubmitAck::operator!=(const FPbArenaCheckListRewardSubmitAck& Right) const
{
    return !operator==(Right);
}

FPbDungeonKillAllChallengeReq::FPbDungeonKillAllChallengeReq()
{
    Reset();        
}

FPbDungeonKillAllChallengeReq::FPbDungeonKillAllChallengeReq(const idlepb::DungeonKillAllChallengeReq& Right)
{
    this->FromPb(Right);
}

void FPbDungeonKillAllChallengeReq::FromPb(const idlepb::DungeonKillAllChallengeReq& Right)
{
    dungeon_uid_id = Right.dungeon_uid_id();
}

void FPbDungeonKillAllChallengeReq::ToPb(idlepb::DungeonKillAllChallengeReq* Out) const
{
    Out->set_dungeon_uid_id(dungeon_uid_id);    
}

void FPbDungeonKillAllChallengeReq::Reset()
{
    dungeon_uid_id = int32();    
}

void FPbDungeonKillAllChallengeReq::operator=(const idlepb::DungeonKillAllChallengeReq& Right)
{
    this->FromPb(Right);
}

bool FPbDungeonKillAllChallengeReq::operator==(const FPbDungeonKillAllChallengeReq& Right) const
{
    if (this->dungeon_uid_id != Right.dungeon_uid_id)
        return false;
    return true;
}

bool FPbDungeonKillAllChallengeReq::operator!=(const FPbDungeonKillAllChallengeReq& Right) const
{
    return !operator==(Right);
}

FPbDungeonKillAllChallengeAck::FPbDungeonKillAllChallengeAck()
{
    Reset();        
}

FPbDungeonKillAllChallengeAck::FPbDungeonKillAllChallengeAck(const idlepb::DungeonKillAllChallengeAck& Right)
{
    this->FromPb(Right);
}

void FPbDungeonKillAllChallengeAck::FromPb(const idlepb::DungeonKillAllChallengeAck& Right)
{
    ok = Right.ok();
}

void FPbDungeonKillAllChallengeAck::ToPb(idlepb::DungeonKillAllChallengeAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbDungeonKillAllChallengeAck::Reset()
{
    ok = bool();    
}

void FPbDungeonKillAllChallengeAck::operator=(const idlepb::DungeonKillAllChallengeAck& Right)
{
    this->FromPb(Right);
}

bool FPbDungeonKillAllChallengeAck::operator==(const FPbDungeonKillAllChallengeAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbDungeonKillAllChallengeAck::operator!=(const FPbDungeonKillAllChallengeAck& Right) const
{
    return !operator==(Right);
}

FPbDungeonKillAllQuickEndReq::FPbDungeonKillAllQuickEndReq()
{
    Reset();        
}

FPbDungeonKillAllQuickEndReq::FPbDungeonKillAllQuickEndReq(const idlepb::DungeonKillAllQuickEndReq& Right)
{
    this->FromPb(Right);
}

void FPbDungeonKillAllQuickEndReq::FromPb(const idlepb::DungeonKillAllQuickEndReq& Right)
{
    is_exit = Right.is_exit();
}

void FPbDungeonKillAllQuickEndReq::ToPb(idlepb::DungeonKillAllQuickEndReq* Out) const
{
    Out->set_is_exit(is_exit);    
}

void FPbDungeonKillAllQuickEndReq::Reset()
{
    is_exit = bool();    
}

void FPbDungeonKillAllQuickEndReq::operator=(const idlepb::DungeonKillAllQuickEndReq& Right)
{
    this->FromPb(Right);
}

bool FPbDungeonKillAllQuickEndReq::operator==(const FPbDungeonKillAllQuickEndReq& Right) const
{
    if (this->is_exit != Right.is_exit)
        return false;
    return true;
}

bool FPbDungeonKillAllQuickEndReq::operator!=(const FPbDungeonKillAllQuickEndReq& Right) const
{
    return !operator==(Right);
}

FPbDungeonKillAllQuickEndAck::FPbDungeonKillAllQuickEndAck()
{
    Reset();        
}

FPbDungeonKillAllQuickEndAck::FPbDungeonKillAllQuickEndAck(const idlepb::DungeonKillAllQuickEndAck& Right)
{
    this->FromPb(Right);
}

void FPbDungeonKillAllQuickEndAck::FromPb(const idlepb::DungeonKillAllQuickEndAck& Right)
{
    ok = Right.ok();
}

void FPbDungeonKillAllQuickEndAck::ToPb(idlepb::DungeonKillAllQuickEndAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbDungeonKillAllQuickEndAck::Reset()
{
    ok = bool();    
}

void FPbDungeonKillAllQuickEndAck::operator=(const idlepb::DungeonKillAllQuickEndAck& Right)
{
    this->FromPb(Right);
}

bool FPbDungeonKillAllQuickEndAck::operator==(const FPbDungeonKillAllQuickEndAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbDungeonKillAllQuickEndAck::operator!=(const FPbDungeonKillAllQuickEndAck& Right) const
{
    return !operator==(Right);
}

FPbNotifyDungeonKillAllChallengeOver::FPbNotifyDungeonKillAllChallengeOver()
{
    Reset();        
}

FPbNotifyDungeonKillAllChallengeOver::FPbNotifyDungeonKillAllChallengeOver(const idlepb::NotifyDungeonKillAllChallengeOver& Right)
{
    this->FromPb(Right);
}

void FPbNotifyDungeonKillAllChallengeOver::FromPb(const idlepb::NotifyDungeonKillAllChallengeOver& Right)
{
    uid = Right.uid();
    win = Right.win();
}

void FPbNotifyDungeonKillAllChallengeOver::ToPb(idlepb::NotifyDungeonKillAllChallengeOver* Out) const
{
    Out->set_uid(uid);
    Out->set_win(win);    
}

void FPbNotifyDungeonKillAllChallengeOver::Reset()
{
    uid = int32();
    win = bool();    
}

void FPbNotifyDungeonKillAllChallengeOver::operator=(const idlepb::NotifyDungeonKillAllChallengeOver& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyDungeonKillAllChallengeOver::operator==(const FPbNotifyDungeonKillAllChallengeOver& Right) const
{
    if (this->uid != Right.uid)
        return false;
    if (this->win != Right.win)
        return false;
    return true;
}

bool FPbNotifyDungeonKillAllChallengeOver::operator!=(const FPbNotifyDungeonKillAllChallengeOver& Right) const
{
    return !operator==(Right);
}

FPbNotifyDungeonKillAllChallengeCurWaveNum::FPbNotifyDungeonKillAllChallengeCurWaveNum()
{
    Reset();        
}

FPbNotifyDungeonKillAllChallengeCurWaveNum::FPbNotifyDungeonKillAllChallengeCurWaveNum(const idlepb::NotifyDungeonKillAllChallengeCurWaveNum& Right)
{
    this->FromPb(Right);
}

void FPbNotifyDungeonKillAllChallengeCurWaveNum::FromPb(const idlepb::NotifyDungeonKillAllChallengeCurWaveNum& Right)
{
    uid = Right.uid();
    curnum = Right.curnum();
    maxnum = Right.maxnum();
}

void FPbNotifyDungeonKillAllChallengeCurWaveNum::ToPb(idlepb::NotifyDungeonKillAllChallengeCurWaveNum* Out) const
{
    Out->set_uid(uid);
    Out->set_curnum(curnum);
    Out->set_maxnum(maxnum);    
}

void FPbNotifyDungeonKillAllChallengeCurWaveNum::Reset()
{
    uid = int32();
    curnum = int32();
    maxnum = int32();    
}

void FPbNotifyDungeonKillAllChallengeCurWaveNum::operator=(const idlepb::NotifyDungeonKillAllChallengeCurWaveNum& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyDungeonKillAllChallengeCurWaveNum::operator==(const FPbNotifyDungeonKillAllChallengeCurWaveNum& Right) const
{
    if (this->uid != Right.uid)
        return false;
    if (this->curnum != Right.curnum)
        return false;
    if (this->maxnum != Right.maxnum)
        return false;
    return true;
}

bool FPbNotifyDungeonKillAllChallengeCurWaveNum::operator!=(const FPbNotifyDungeonKillAllChallengeCurWaveNum& Right) const
{
    return !operator==(Right);
}

FPbDungeonKillAllDataReq::FPbDungeonKillAllDataReq()
{
    Reset();        
}

FPbDungeonKillAllDataReq::FPbDungeonKillAllDataReq(const idlepb::DungeonKillAllDataReq& Right)
{
    this->FromPb(Right);
}

void FPbDungeonKillAllDataReq::FromPb(const idlepb::DungeonKillAllDataReq& Right)
{
    ask_uid = Right.ask_uid();
}

void FPbDungeonKillAllDataReq::ToPb(idlepb::DungeonKillAllDataReq* Out) const
{
    Out->set_ask_uid(ask_uid);    
}

void FPbDungeonKillAllDataReq::Reset()
{
    ask_uid = int32();    
}

void FPbDungeonKillAllDataReq::operator=(const idlepb::DungeonKillAllDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbDungeonKillAllDataReq::operator==(const FPbDungeonKillAllDataReq& Right) const
{
    if (this->ask_uid != Right.ask_uid)
        return false;
    return true;
}

bool FPbDungeonKillAllDataReq::operator!=(const FPbDungeonKillAllDataReq& Right) const
{
    return !operator==(Right);
}

FPbDungeonKillAllDataAck::FPbDungeonKillAllDataAck()
{
    Reset();        
}

FPbDungeonKillAllDataAck::FPbDungeonKillAllDataAck(const idlepb::DungeonKillAllDataAck& Right)
{
    this->FromPb(Right);
}

void FPbDungeonKillAllDataAck::FromPb(const idlepb::DungeonKillAllDataAck& Right)
{
    ok = Right.ok();
}

void FPbDungeonKillAllDataAck::ToPb(idlepb::DungeonKillAllDataAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbDungeonKillAllDataAck::Reset()
{
    ok = bool();    
}

void FPbDungeonKillAllDataAck::operator=(const idlepb::DungeonKillAllDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbDungeonKillAllDataAck::operator==(const FPbDungeonKillAllDataAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbDungeonKillAllDataAck::operator!=(const FPbDungeonKillAllDataAck& Right) const
{
    return !operator==(Right);
}

FPbDungeonSurviveChallengeReq::FPbDungeonSurviveChallengeReq()
{
    Reset();        
}

FPbDungeonSurviveChallengeReq::FPbDungeonSurviveChallengeReq(const idlepb::DungeonSurviveChallengeReq& Right)
{
    this->FromPb(Right);
}

void FPbDungeonSurviveChallengeReq::FromPb(const idlepb::DungeonSurviveChallengeReq& Right)
{
    dungeon_uid = Right.dungeon_uid();
}

void FPbDungeonSurviveChallengeReq::ToPb(idlepb::DungeonSurviveChallengeReq* Out) const
{
    Out->set_dungeon_uid(dungeon_uid);    
}

void FPbDungeonSurviveChallengeReq::Reset()
{
    dungeon_uid = int32();    
}

void FPbDungeonSurviveChallengeReq::operator=(const idlepb::DungeonSurviveChallengeReq& Right)
{
    this->FromPb(Right);
}

bool FPbDungeonSurviveChallengeReq::operator==(const FPbDungeonSurviveChallengeReq& Right) const
{
    if (this->dungeon_uid != Right.dungeon_uid)
        return false;
    return true;
}

bool FPbDungeonSurviveChallengeReq::operator!=(const FPbDungeonSurviveChallengeReq& Right) const
{
    return !operator==(Right);
}

FPbDungeonSurviveChallengeAck::FPbDungeonSurviveChallengeAck()
{
    Reset();        
}

FPbDungeonSurviveChallengeAck::FPbDungeonSurviveChallengeAck(const idlepb::DungeonSurviveChallengeAck& Right)
{
    this->FromPb(Right);
}

void FPbDungeonSurviveChallengeAck::FromPb(const idlepb::DungeonSurviveChallengeAck& Right)
{
    ok = Right.ok();
}

void FPbDungeonSurviveChallengeAck::ToPb(idlepb::DungeonSurviveChallengeAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbDungeonSurviveChallengeAck::Reset()
{
    ok = bool();    
}

void FPbDungeonSurviveChallengeAck::operator=(const idlepb::DungeonSurviveChallengeAck& Right)
{
    this->FromPb(Right);
}

bool FPbDungeonSurviveChallengeAck::operator==(const FPbDungeonSurviveChallengeAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbDungeonSurviveChallengeAck::operator!=(const FPbDungeonSurviveChallengeAck& Right) const
{
    return !operator==(Right);
}

FPbDungeonSurviveQuickEndReq::FPbDungeonSurviveQuickEndReq()
{
    Reset();        
}

FPbDungeonSurviveQuickEndReq::FPbDungeonSurviveQuickEndReq(const idlepb::DungeonSurviveQuickEndReq& Right)
{
    this->FromPb(Right);
}

void FPbDungeonSurviveQuickEndReq::FromPb(const idlepb::DungeonSurviveQuickEndReq& Right)
{
    is_exit = Right.is_exit();
}

void FPbDungeonSurviveQuickEndReq::ToPb(idlepb::DungeonSurviveQuickEndReq* Out) const
{
    Out->set_is_exit(is_exit);    
}

void FPbDungeonSurviveQuickEndReq::Reset()
{
    is_exit = bool();    
}

void FPbDungeonSurviveQuickEndReq::operator=(const idlepb::DungeonSurviveQuickEndReq& Right)
{
    this->FromPb(Right);
}

bool FPbDungeonSurviveQuickEndReq::operator==(const FPbDungeonSurviveQuickEndReq& Right) const
{
    if (this->is_exit != Right.is_exit)
        return false;
    return true;
}

bool FPbDungeonSurviveQuickEndReq::operator!=(const FPbDungeonSurviveQuickEndReq& Right) const
{
    return !operator==(Right);
}

FPbDungeonSurviveQuickEndAck::FPbDungeonSurviveQuickEndAck()
{
    Reset();        
}

FPbDungeonSurviveQuickEndAck::FPbDungeonSurviveQuickEndAck(const idlepb::DungeonSurviveQuickEndAck& Right)
{
    this->FromPb(Right);
}

void FPbDungeonSurviveQuickEndAck::FromPb(const idlepb::DungeonSurviveQuickEndAck& Right)
{
    ok = Right.ok();
}

void FPbDungeonSurviveQuickEndAck::ToPb(idlepb::DungeonSurviveQuickEndAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbDungeonSurviveQuickEndAck::Reset()
{
    ok = bool();    
}

void FPbDungeonSurviveQuickEndAck::operator=(const idlepb::DungeonSurviveQuickEndAck& Right)
{
    this->FromPb(Right);
}

bool FPbDungeonSurviveQuickEndAck::operator==(const FPbDungeonSurviveQuickEndAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbDungeonSurviveQuickEndAck::operator!=(const FPbDungeonSurviveQuickEndAck& Right) const
{
    return !operator==(Right);
}

FPbNotifyDungeonSurviveChallengeOver::FPbNotifyDungeonSurviveChallengeOver()
{
    Reset();        
}

FPbNotifyDungeonSurviveChallengeOver::FPbNotifyDungeonSurviveChallengeOver(const idlepb::NotifyDungeonSurviveChallengeOver& Right)
{
    this->FromPb(Right);
}

void FPbNotifyDungeonSurviveChallengeOver::FromPb(const idlepb::NotifyDungeonSurviveChallengeOver& Right)
{
    uid = Right.uid();
    win = Right.win();
}

void FPbNotifyDungeonSurviveChallengeOver::ToPb(idlepb::NotifyDungeonSurviveChallengeOver* Out) const
{
    Out->set_uid(uid);
    Out->set_win(win);    
}

void FPbNotifyDungeonSurviveChallengeOver::Reset()
{
    uid = int32();
    win = bool();    
}

void FPbNotifyDungeonSurviveChallengeOver::operator=(const idlepb::NotifyDungeonSurviveChallengeOver& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyDungeonSurviveChallengeOver::operator==(const FPbNotifyDungeonSurviveChallengeOver& Right) const
{
    if (this->uid != Right.uid)
        return false;
    if (this->win != Right.win)
        return false;
    return true;
}

bool FPbNotifyDungeonSurviveChallengeOver::operator!=(const FPbNotifyDungeonSurviveChallengeOver& Right) const
{
    return !operator==(Right);
}

FPbNotifyDungeonSurviveChallengeCurWaveNum::FPbNotifyDungeonSurviveChallengeCurWaveNum()
{
    Reset();        
}

FPbNotifyDungeonSurviveChallengeCurWaveNum::FPbNotifyDungeonSurviveChallengeCurWaveNum(const idlepb::NotifyDungeonSurviveChallengeCurWaveNum& Right)
{
    this->FromPb(Right);
}

void FPbNotifyDungeonSurviveChallengeCurWaveNum::FromPb(const idlepb::NotifyDungeonSurviveChallengeCurWaveNum& Right)
{
    uid = Right.uid();
    curnum = Right.curnum();
    maxnum = Right.maxnum();
}

void FPbNotifyDungeonSurviveChallengeCurWaveNum::ToPb(idlepb::NotifyDungeonSurviveChallengeCurWaveNum* Out) const
{
    Out->set_uid(uid);
    Out->set_curnum(curnum);
    Out->set_maxnum(maxnum);    
}

void FPbNotifyDungeonSurviveChallengeCurWaveNum::Reset()
{
    uid = int32();
    curnum = int32();
    maxnum = int32();    
}

void FPbNotifyDungeonSurviveChallengeCurWaveNum::operator=(const idlepb::NotifyDungeonSurviveChallengeCurWaveNum& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyDungeonSurviveChallengeCurWaveNum::operator==(const FPbNotifyDungeonSurviveChallengeCurWaveNum& Right) const
{
    if (this->uid != Right.uid)
        return false;
    if (this->curnum != Right.curnum)
        return false;
    if (this->maxnum != Right.maxnum)
        return false;
    return true;
}

bool FPbNotifyDungeonSurviveChallengeCurWaveNum::operator!=(const FPbNotifyDungeonSurviveChallengeCurWaveNum& Right) const
{
    return !operator==(Right);
}

FPbDungeonSurviveDataReq::FPbDungeonSurviveDataReq()
{
    Reset();        
}

FPbDungeonSurviveDataReq::FPbDungeonSurviveDataReq(const idlepb::DungeonSurviveDataReq& Right)
{
    this->FromPb(Right);
}

void FPbDungeonSurviveDataReq::FromPb(const idlepb::DungeonSurviveDataReq& Right)
{
    ask_uid = Right.ask_uid();
}

void FPbDungeonSurviveDataReq::ToPb(idlepb::DungeonSurviveDataReq* Out) const
{
    Out->set_ask_uid(ask_uid);    
}

void FPbDungeonSurviveDataReq::Reset()
{
    ask_uid = int32();    
}

void FPbDungeonSurviveDataReq::operator=(const idlepb::DungeonSurviveDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbDungeonSurviveDataReq::operator==(const FPbDungeonSurviveDataReq& Right) const
{
    if (this->ask_uid != Right.ask_uid)
        return false;
    return true;
}

bool FPbDungeonSurviveDataReq::operator!=(const FPbDungeonSurviveDataReq& Right) const
{
    return !operator==(Right);
}

FPbDungeonSurviveDataAck::FPbDungeonSurviveDataAck()
{
    Reset();        
}

FPbDungeonSurviveDataAck::FPbDungeonSurviveDataAck(const idlepb::DungeonSurviveDataAck& Right)
{
    this->FromPb(Right);
}

void FPbDungeonSurviveDataAck::FromPb(const idlepb::DungeonSurviveDataAck& Right)
{
    ok = Right.ok();
}

void FPbDungeonSurviveDataAck::ToPb(idlepb::DungeonSurviveDataAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbDungeonSurviveDataAck::Reset()
{
    ok = bool();    
}

void FPbDungeonSurviveDataAck::operator=(const idlepb::DungeonSurviveDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbDungeonSurviveDataAck::operator==(const FPbDungeonSurviveDataAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbDungeonSurviveDataAck::operator!=(const FPbDungeonSurviveDataAck& Right) const
{
    return !operator==(Right);
}

FPbRequestEnterSeptDemonWorldReq::FPbRequestEnterSeptDemonWorldReq()
{
    Reset();        
}

FPbRequestEnterSeptDemonWorldReq::FPbRequestEnterSeptDemonWorldReq(const idlepb::RequestEnterSeptDemonWorldReq& Right)
{
    this->FromPb(Right);
}

void FPbRequestEnterSeptDemonWorldReq::FromPb(const idlepb::RequestEnterSeptDemonWorldReq& Right)
{
    sept_id = Right.sept_id();
}

void FPbRequestEnterSeptDemonWorldReq::ToPb(idlepb::RequestEnterSeptDemonWorldReq* Out) const
{
    Out->set_sept_id(sept_id);    
}

void FPbRequestEnterSeptDemonWorldReq::Reset()
{
    sept_id = int64();    
}

void FPbRequestEnterSeptDemonWorldReq::operator=(const idlepb::RequestEnterSeptDemonWorldReq& Right)
{
    this->FromPb(Right);
}

bool FPbRequestEnterSeptDemonWorldReq::operator==(const FPbRequestEnterSeptDemonWorldReq& Right) const
{
    if (this->sept_id != Right.sept_id)
        return false;
    return true;
}

bool FPbRequestEnterSeptDemonWorldReq::operator!=(const FPbRequestEnterSeptDemonWorldReq& Right) const
{
    return !operator==(Right);
}

FPbRequestEnterSeptDemonWorldAck::FPbRequestEnterSeptDemonWorldAck()
{
    Reset();        
}

FPbRequestEnterSeptDemonWorldAck::FPbRequestEnterSeptDemonWorldAck(const idlepb::RequestEnterSeptDemonWorldAck& Right)
{
    this->FromPb(Right);
}

void FPbRequestEnterSeptDemonWorldAck::FromPb(const idlepb::RequestEnterSeptDemonWorldAck& Right)
{
    ok = Right.ok();
}

void FPbRequestEnterSeptDemonWorldAck::ToPb(idlepb::RequestEnterSeptDemonWorldAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbRequestEnterSeptDemonWorldAck::Reset()
{
    ok = bool();    
}

void FPbRequestEnterSeptDemonWorldAck::operator=(const idlepb::RequestEnterSeptDemonWorldAck& Right)
{
    this->FromPb(Right);
}

bool FPbRequestEnterSeptDemonWorldAck::operator==(const FPbRequestEnterSeptDemonWorldAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbRequestEnterSeptDemonWorldAck::operator!=(const FPbRequestEnterSeptDemonWorldAck& Right) const
{
    return !operator==(Right);
}

FPbRequestLeaveSeptDemonWorldReq::FPbRequestLeaveSeptDemonWorldReq()
{
    Reset();        
}

FPbRequestLeaveSeptDemonWorldReq::FPbRequestLeaveSeptDemonWorldReq(const idlepb::RequestLeaveSeptDemonWorldReq& Right)
{
    this->FromPb(Right);
}

void FPbRequestLeaveSeptDemonWorldReq::FromPb(const idlepb::RequestLeaveSeptDemonWorldReq& Right)
{
    sept_id = Right.sept_id();
}

void FPbRequestLeaveSeptDemonWorldReq::ToPb(idlepb::RequestLeaveSeptDemonWorldReq* Out) const
{
    Out->set_sept_id(sept_id);    
}

void FPbRequestLeaveSeptDemonWorldReq::Reset()
{
    sept_id = int64();    
}

void FPbRequestLeaveSeptDemonWorldReq::operator=(const idlepb::RequestLeaveSeptDemonWorldReq& Right)
{
    this->FromPb(Right);
}

bool FPbRequestLeaveSeptDemonWorldReq::operator==(const FPbRequestLeaveSeptDemonWorldReq& Right) const
{
    if (this->sept_id != Right.sept_id)
        return false;
    return true;
}

bool FPbRequestLeaveSeptDemonWorldReq::operator!=(const FPbRequestLeaveSeptDemonWorldReq& Right) const
{
    return !operator==(Right);
}

FPbRequestLeaveSeptDemonWorldAck::FPbRequestLeaveSeptDemonWorldAck()
{
    Reset();        
}

FPbRequestLeaveSeptDemonWorldAck::FPbRequestLeaveSeptDemonWorldAck(const idlepb::RequestLeaveSeptDemonWorldAck& Right)
{
    this->FromPb(Right);
}

void FPbRequestLeaveSeptDemonWorldAck::FromPb(const idlepb::RequestLeaveSeptDemonWorldAck& Right)
{
    ok = Right.ok();
}

void FPbRequestLeaveSeptDemonWorldAck::ToPb(idlepb::RequestLeaveSeptDemonWorldAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbRequestLeaveSeptDemonWorldAck::Reset()
{
    ok = bool();    
}

void FPbRequestLeaveSeptDemonWorldAck::operator=(const idlepb::RequestLeaveSeptDemonWorldAck& Right)
{
    this->FromPb(Right);
}

bool FPbRequestLeaveSeptDemonWorldAck::operator==(const FPbRequestLeaveSeptDemonWorldAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbRequestLeaveSeptDemonWorldAck::operator!=(const FPbRequestLeaveSeptDemonWorldAck& Right) const
{
    return !operator==(Right);
}

FPbRequestSeptDemonWorldDataReq::FPbRequestSeptDemonWorldDataReq()
{
    Reset();        
}

FPbRequestSeptDemonWorldDataReq::FPbRequestSeptDemonWorldDataReq(const idlepb::RequestSeptDemonWorldDataReq& Right)
{
    this->FromPb(Right);
}

void FPbRequestSeptDemonWorldDataReq::FromPb(const idlepb::RequestSeptDemonWorldDataReq& Right)
{
    sept_id = Right.sept_id();
}

void FPbRequestSeptDemonWorldDataReq::ToPb(idlepb::RequestSeptDemonWorldDataReq* Out) const
{
    Out->set_sept_id(sept_id);    
}

void FPbRequestSeptDemonWorldDataReq::Reset()
{
    sept_id = int64();    
}

void FPbRequestSeptDemonWorldDataReq::operator=(const idlepb::RequestSeptDemonWorldDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbRequestSeptDemonWorldDataReq::operator==(const FPbRequestSeptDemonWorldDataReq& Right) const
{
    if (this->sept_id != Right.sept_id)
        return false;
    return true;
}

bool FPbRequestSeptDemonWorldDataReq::operator!=(const FPbRequestSeptDemonWorldDataReq& Right) const
{
    return !operator==(Right);
}

FPbRequestSeptDemonWorldDataAck::FPbRequestSeptDemonWorldDataAck()
{
    Reset();        
}

FPbRequestSeptDemonWorldDataAck::FPbRequestSeptDemonWorldDataAck(const idlepb::RequestSeptDemonWorldDataAck& Right)
{
    this->FromPb(Right);
}

void FPbRequestSeptDemonWorldDataAck::FromPb(const idlepb::RequestSeptDemonWorldDataAck& Right)
{
    data = Right.data();
}

void FPbRequestSeptDemonWorldDataAck::ToPb(idlepb::RequestSeptDemonWorldDataAck* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbRequestSeptDemonWorldDataAck::Reset()
{
    data = FPbSeptDemonWorldData();    
}

void FPbRequestSeptDemonWorldDataAck::operator=(const idlepb::RequestSeptDemonWorldDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbRequestSeptDemonWorldDataAck::operator==(const FPbRequestSeptDemonWorldDataAck& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbRequestSeptDemonWorldDataAck::operator!=(const FPbRequestSeptDemonWorldDataAck& Right) const
{
    return !operator==(Right);
}

FPbRequestInSeptDemonWorldEndTimeReq::FPbRequestInSeptDemonWorldEndTimeReq()
{
    Reset();        
}

FPbRequestInSeptDemonWorldEndTimeReq::FPbRequestInSeptDemonWorldEndTimeReq(const idlepb::RequestInSeptDemonWorldEndTimeReq& Right)
{
    this->FromPb(Right);
}

void FPbRequestInSeptDemonWorldEndTimeReq::FromPb(const idlepb::RequestInSeptDemonWorldEndTimeReq& Right)
{
}

void FPbRequestInSeptDemonWorldEndTimeReq::ToPb(idlepb::RequestInSeptDemonWorldEndTimeReq* Out) const
{    
}

void FPbRequestInSeptDemonWorldEndTimeReq::Reset()
{    
}

void FPbRequestInSeptDemonWorldEndTimeReq::operator=(const idlepb::RequestInSeptDemonWorldEndTimeReq& Right)
{
    this->FromPb(Right);
}

bool FPbRequestInSeptDemonWorldEndTimeReq::operator==(const FPbRequestInSeptDemonWorldEndTimeReq& Right) const
{
    return true;
}

bool FPbRequestInSeptDemonWorldEndTimeReq::operator!=(const FPbRequestInSeptDemonWorldEndTimeReq& Right) const
{
    return !operator==(Right);
}

FPbRequestInSeptDemonWorldEndTimeAck::FPbRequestInSeptDemonWorldEndTimeAck()
{
    Reset();        
}

FPbRequestInSeptDemonWorldEndTimeAck::FPbRequestInSeptDemonWorldEndTimeAck(const idlepb::RequestInSeptDemonWorldEndTimeAck& Right)
{
    this->FromPb(Right);
}

void FPbRequestInSeptDemonWorldEndTimeAck::FromPb(const idlepb::RequestInSeptDemonWorldEndTimeAck& Right)
{
    end_time = Right.end_time();
}

void FPbRequestInSeptDemonWorldEndTimeAck::ToPb(idlepb::RequestInSeptDemonWorldEndTimeAck* Out) const
{
    Out->set_end_time(end_time);    
}

void FPbRequestInSeptDemonWorldEndTimeAck::Reset()
{
    end_time = int64();    
}

void FPbRequestInSeptDemonWorldEndTimeAck::operator=(const idlepb::RequestInSeptDemonWorldEndTimeAck& Right)
{
    this->FromPb(Right);
}

bool FPbRequestInSeptDemonWorldEndTimeAck::operator==(const FPbRequestInSeptDemonWorldEndTimeAck& Right) const
{
    if (this->end_time != Right.end_time)
        return false;
    return true;
}

bool FPbRequestInSeptDemonWorldEndTimeAck::operator!=(const FPbRequestInSeptDemonWorldEndTimeAck& Right) const
{
    return !operator==(Right);
}

FPbGetFarmlandDataReq::FPbGetFarmlandDataReq()
{
    Reset();        
}

FPbGetFarmlandDataReq::FPbGetFarmlandDataReq(const idlepb::GetFarmlandDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetFarmlandDataReq::FromPb(const idlepb::GetFarmlandDataReq& Right)
{
}

void FPbGetFarmlandDataReq::ToPb(idlepb::GetFarmlandDataReq* Out) const
{    
}

void FPbGetFarmlandDataReq::Reset()
{    
}

void FPbGetFarmlandDataReq::operator=(const idlepb::GetFarmlandDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetFarmlandDataReq::operator==(const FPbGetFarmlandDataReq& Right) const
{
    return true;
}

bool FPbGetFarmlandDataReq::operator!=(const FPbGetFarmlandDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetFarmlandDataAck::FPbGetFarmlandDataAck()
{
    Reset();        
}

FPbGetFarmlandDataAck::FPbGetFarmlandDataAck(const idlepb::GetFarmlandDataAck& Right)
{
    this->FromPb(Right);
}

void FPbGetFarmlandDataAck::FromPb(const idlepb::GetFarmlandDataAck& Right)
{
    data = Right.data();
}

void FPbGetFarmlandDataAck::ToPb(idlepb::GetFarmlandDataAck* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbGetFarmlandDataAck::Reset()
{
    data = FPbRoleFarmlandData();    
}

void FPbGetFarmlandDataAck::operator=(const idlepb::GetFarmlandDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetFarmlandDataAck::operator==(const FPbGetFarmlandDataAck& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbGetFarmlandDataAck::operator!=(const FPbGetFarmlandDataAck& Right) const
{
    return !operator==(Right);
}

FPbFarmlandUnlockBlockReq::FPbFarmlandUnlockBlockReq()
{
    Reset();        
}

FPbFarmlandUnlockBlockReq::FPbFarmlandUnlockBlockReq(const idlepb::FarmlandUnlockBlockReq& Right)
{
    this->FromPb(Right);
}

void FPbFarmlandUnlockBlockReq::FromPb(const idlepb::FarmlandUnlockBlockReq& Right)
{
    x = Right.x();
    y = Right.y();
}

void FPbFarmlandUnlockBlockReq::ToPb(idlepb::FarmlandUnlockBlockReq* Out) const
{
    Out->set_x(x);
    Out->set_y(y);    
}

void FPbFarmlandUnlockBlockReq::Reset()
{
    x = int32();
    y = int32();    
}

void FPbFarmlandUnlockBlockReq::operator=(const idlepb::FarmlandUnlockBlockReq& Right)
{
    this->FromPb(Right);
}

bool FPbFarmlandUnlockBlockReq::operator==(const FPbFarmlandUnlockBlockReq& Right) const
{
    if (this->x != Right.x)
        return false;
    if (this->y != Right.y)
        return false;
    return true;
}

bool FPbFarmlandUnlockBlockReq::operator!=(const FPbFarmlandUnlockBlockReq& Right) const
{
    return !operator==(Right);
}

FPbFarmlandUnlockBlockAck::FPbFarmlandUnlockBlockAck()
{
    Reset();        
}

FPbFarmlandUnlockBlockAck::FPbFarmlandUnlockBlockAck(const idlepb::FarmlandUnlockBlockAck& Right)
{
    this->FromPb(Right);
}

void FPbFarmlandUnlockBlockAck::FromPb(const idlepb::FarmlandUnlockBlockAck& Right)
{
    ok = Right.ok();
}

void FPbFarmlandUnlockBlockAck::ToPb(idlepb::FarmlandUnlockBlockAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbFarmlandUnlockBlockAck::Reset()
{
    ok = bool();    
}

void FPbFarmlandUnlockBlockAck::operator=(const idlepb::FarmlandUnlockBlockAck& Right)
{
    this->FromPb(Right);
}

bool FPbFarmlandUnlockBlockAck::operator==(const FPbFarmlandUnlockBlockAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbFarmlandUnlockBlockAck::operator!=(const FPbFarmlandUnlockBlockAck& Right) const
{
    return !operator==(Right);
}

FPbFarmlandPlantSeedReq::FPbFarmlandPlantSeedReq()
{
    Reset();        
}

FPbFarmlandPlantSeedReq::FPbFarmlandPlantSeedReq(const idlepb::FarmlandPlantSeedReq& Right)
{
    this->FromPb(Right);
}

void FPbFarmlandPlantSeedReq::FromPb(const idlepb::FarmlandPlantSeedReq& Right)
{
    item_id = Right.item_id();
    x = Right.x();
    y = Right.y();
    rotation = Right.rotation();
    is_delete = Right.is_delete();
}

void FPbFarmlandPlantSeedReq::ToPb(idlepb::FarmlandPlantSeedReq* Out) const
{
    Out->set_item_id(item_id);
    Out->set_x(x);
    Out->set_y(y);
    Out->set_rotation(rotation);
    Out->set_is_delete(is_delete);    
}

void FPbFarmlandPlantSeedReq::Reset()
{
    item_id = int32();
    x = int32();
    y = int32();
    rotation = int32();
    is_delete = bool();    
}

void FPbFarmlandPlantSeedReq::operator=(const idlepb::FarmlandPlantSeedReq& Right)
{
    this->FromPb(Right);
}

bool FPbFarmlandPlantSeedReq::operator==(const FPbFarmlandPlantSeedReq& Right) const
{
    if (this->item_id != Right.item_id)
        return false;
    if (this->x != Right.x)
        return false;
    if (this->y != Right.y)
        return false;
    if (this->rotation != Right.rotation)
        return false;
    if (this->is_delete != Right.is_delete)
        return false;
    return true;
}

bool FPbFarmlandPlantSeedReq::operator!=(const FPbFarmlandPlantSeedReq& Right) const
{
    return !operator==(Right);
}

FPbFarmlandPlantSeedAck::FPbFarmlandPlantSeedAck()
{
    Reset();        
}

FPbFarmlandPlantSeedAck::FPbFarmlandPlantSeedAck(const idlepb::FarmlandPlantSeedAck& Right)
{
    this->FromPb(Right);
}

void FPbFarmlandPlantSeedAck::FromPb(const idlepb::FarmlandPlantSeedAck& Right)
{
    plant_data = Right.plant_data();
}

void FPbFarmlandPlantSeedAck::ToPb(idlepb::FarmlandPlantSeedAck* Out) const
{
    plant_data.ToPb(Out->mutable_plant_data());    
}

void FPbFarmlandPlantSeedAck::Reset()
{
    plant_data = FPbFarmlandPlantData();    
}

void FPbFarmlandPlantSeedAck::operator=(const idlepb::FarmlandPlantSeedAck& Right)
{
    this->FromPb(Right);
}

bool FPbFarmlandPlantSeedAck::operator==(const FPbFarmlandPlantSeedAck& Right) const
{
    if (this->plant_data != Right.plant_data)
        return false;
    return true;
}

bool FPbFarmlandPlantSeedAck::operator!=(const FPbFarmlandPlantSeedAck& Right) const
{
    return !operator==(Right);
}

FPbFarmlandWateringReq::FPbFarmlandWateringReq()
{
    Reset();        
}

FPbFarmlandWateringReq::FPbFarmlandWateringReq(const idlepb::FarmlandWateringReq& Right)
{
    this->FromPb(Right);
}

void FPbFarmlandWateringReq::FromPb(const idlepb::FarmlandWateringReq& Right)
{
    num = Right.num();
}

void FPbFarmlandWateringReq::ToPb(idlepb::FarmlandWateringReq* Out) const
{
    Out->set_num(num);    
}

void FPbFarmlandWateringReq::Reset()
{
    num = int32();    
}

void FPbFarmlandWateringReq::operator=(const idlepb::FarmlandWateringReq& Right)
{
    this->FromPb(Right);
}

bool FPbFarmlandWateringReq::operator==(const FPbFarmlandWateringReq& Right) const
{
    if (this->num != Right.num)
        return false;
    return true;
}

bool FPbFarmlandWateringReq::operator!=(const FPbFarmlandWateringReq& Right) const
{
    return !operator==(Right);
}

FPbFarmlandWateringAck::FPbFarmlandWateringAck()
{
    Reset();        
}

FPbFarmlandWateringAck::FPbFarmlandWateringAck(const idlepb::FarmlandWateringAck& Right)
{
    this->FromPb(Right);
}

void FPbFarmlandWateringAck::FromPb(const idlepb::FarmlandWateringAck& Right)
{
    totaol_speed_up = Right.totaol_speed_up();
}

void FPbFarmlandWateringAck::ToPb(idlepb::FarmlandWateringAck* Out) const
{
    Out->set_totaol_speed_up(totaol_speed_up);    
}

void FPbFarmlandWateringAck::Reset()
{
    totaol_speed_up = int32();    
}

void FPbFarmlandWateringAck::operator=(const idlepb::FarmlandWateringAck& Right)
{
    this->FromPb(Right);
}

bool FPbFarmlandWateringAck::operator==(const FPbFarmlandWateringAck& Right) const
{
    if (this->totaol_speed_up != Right.totaol_speed_up)
        return false;
    return true;
}

bool FPbFarmlandWateringAck::operator!=(const FPbFarmlandWateringAck& Right) const
{
    return !operator==(Right);
}

FPbFarmlandRipeningReq::FPbFarmlandRipeningReq()
{
    Reset();        
}

FPbFarmlandRipeningReq::FPbFarmlandRipeningReq(const idlepb::FarmlandRipeningReq& Right)
{
    this->FromPb(Right);
}

void FPbFarmlandRipeningReq::FromPb(const idlepb::FarmlandRipeningReq& Right)
{
    plant_uid = Right.plant_uid();
    item_id = Right.item_id();
    num = Right.num();
    one_click = Right.one_click();
    one_click_plants.Empty();
    for (const auto& Elem : Right.one_click_plants())
    {
        one_click_plants.Emplace(Elem);
    }
}

void FPbFarmlandRipeningReq::ToPb(idlepb::FarmlandRipeningReq* Out) const
{
    Out->set_plant_uid(plant_uid);
    Out->set_item_id(item_id);
    Out->set_num(num);
    Out->set_one_click(one_click);
    for (const auto& Elem : one_click_plants)
    {
        Elem.ToPb(Out->add_one_click_plants());    
    }    
}

void FPbFarmlandRipeningReq::Reset()
{
    plant_uid = int32();
    item_id = int32();
    num = int32();
    one_click = int32();
    one_click_plants = TArray<FPbFarmlandManagementInfo>();    
}

void FPbFarmlandRipeningReq::operator=(const idlepb::FarmlandRipeningReq& Right)
{
    this->FromPb(Right);
}

bool FPbFarmlandRipeningReq::operator==(const FPbFarmlandRipeningReq& Right) const
{
    if (this->plant_uid != Right.plant_uid)
        return false;
    if (this->item_id != Right.item_id)
        return false;
    if (this->num != Right.num)
        return false;
    if (this->one_click != Right.one_click)
        return false;
    if (this->one_click_plants != Right.one_click_plants)
        return false;
    return true;
}

bool FPbFarmlandRipeningReq::operator!=(const FPbFarmlandRipeningReq& Right) const
{
    return !operator==(Right);
}

FPbFarmlandRipeningAck::FPbFarmlandRipeningAck()
{
    Reset();        
}

FPbFarmlandRipeningAck::FPbFarmlandRipeningAck(const idlepb::FarmlandRipeningAck& Right)
{
    this->FromPb(Right);
}

void FPbFarmlandRipeningAck::FromPb(const idlepb::FarmlandRipeningAck& Right)
{
    ok = Right.ok();
    result.Empty();
    for (const auto& Elem : Right.result())
    {
        result.Emplace(Elem);
    }
    used_ripe_items.Empty();
    for (const auto& Elem : Right.used_ripe_items())
    {
        used_ripe_items.Emplace(Elem);
    }
}

void FPbFarmlandRipeningAck::ToPb(idlepb::FarmlandRipeningAck* Out) const
{
    Out->set_ok(ok);
    for (const auto& Elem : result)
    {
        Elem.ToPb(Out->add_result());    
    }
    for (const auto& Elem : used_ripe_items)
    {
        Elem.ToPb(Out->add_used_ripe_items());    
    }    
}

void FPbFarmlandRipeningAck::Reset()
{
    ok = bool();
    result = TArray<FPbFarmlandPlantData>();
    used_ripe_items = TArray<FPbSimpleItemData>();    
}

void FPbFarmlandRipeningAck::operator=(const idlepb::FarmlandRipeningAck& Right)
{
    this->FromPb(Right);
}

bool FPbFarmlandRipeningAck::operator==(const FPbFarmlandRipeningAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    if (this->result != Right.result)
        return false;
    if (this->used_ripe_items != Right.used_ripe_items)
        return false;
    return true;
}

bool FPbFarmlandRipeningAck::operator!=(const FPbFarmlandRipeningAck& Right) const
{
    return !operator==(Right);
}

FPbFarmlandHarvestReq::FPbFarmlandHarvestReq()
{
    Reset();        
}

FPbFarmlandHarvestReq::FPbFarmlandHarvestReq(const idlepb::FarmlandHarvestReq& Right)
{
    this->FromPb(Right);
}

void FPbFarmlandHarvestReq::FromPb(const idlepb::FarmlandHarvestReq& Right)
{
    plant_ids.Empty();
    for (const auto& Elem : Right.plant_ids())
    {
        plant_ids.Emplace(Elem);
    }
    auto_harvest_same_class = Right.auto_harvest_same_class();
}

void FPbFarmlandHarvestReq::ToPb(idlepb::FarmlandHarvestReq* Out) const
{
    for (const auto& Elem : plant_ids)
    {
        Out->add_plant_ids(Elem);    
    }
    Out->set_auto_harvest_same_class(auto_harvest_same_class);    
}

void FPbFarmlandHarvestReq::Reset()
{
    plant_ids = TArray<int32>();
    auto_harvest_same_class = bool();    
}

void FPbFarmlandHarvestReq::operator=(const idlepb::FarmlandHarvestReq& Right)
{
    this->FromPb(Right);
}

bool FPbFarmlandHarvestReq::operator==(const FPbFarmlandHarvestReq& Right) const
{
    if (this->plant_ids != Right.plant_ids)
        return false;
    if (this->auto_harvest_same_class != Right.auto_harvest_same_class)
        return false;
    return true;
}

bool FPbFarmlandHarvestReq::operator!=(const FPbFarmlandHarvestReq& Right) const
{
    return !operator==(Right);
}

FPbFarmlandHarvestAck::FPbFarmlandHarvestAck()
{
    Reset();        
}

FPbFarmlandHarvestAck::FPbFarmlandHarvestAck(const idlepb::FarmlandHarvestAck& Right)
{
    this->FromPb(Right);
}

void FPbFarmlandHarvestAck::FromPb(const idlepb::FarmlandHarvestAck& Right)
{
    ok = Right.ok();
    items.Empty();
    for (const auto& Elem : Right.items())
    {
        items.Emplace(Elem);
    }
    op_success_plant_id.Empty();
    for (const auto& Elem : Right.op_success_plant_id())
    {
        op_success_plant_id.Emplace(Elem);
    }
    continue_seeds.Empty();
    for (const auto& Elem : Right.continue_seeds())
    {
        continue_seeds.Emplace(Elem);
    }
}

void FPbFarmlandHarvestAck::ToPb(idlepb::FarmlandHarvestAck* Out) const
{
    Out->set_ok(ok);
    for (const auto& Elem : items)
    {
        Elem.ToPb(Out->add_items());    
    }
    for (const auto& Elem : op_success_plant_id)
    {
        Out->add_op_success_plant_id(Elem);    
    }
    for (const auto& Elem : continue_seeds)
    {
        Elem.ToPb(Out->add_continue_seeds());    
    }    
}

void FPbFarmlandHarvestAck::Reset()
{
    ok = bool();
    items = TArray<FPbSimpleItemData>();
    op_success_plant_id = TArray<int32>();
    continue_seeds = TArray<FPbFarmlandPlantData>();    
}

void FPbFarmlandHarvestAck::operator=(const idlepb::FarmlandHarvestAck& Right)
{
    this->FromPb(Right);
}

bool FPbFarmlandHarvestAck::operator==(const FPbFarmlandHarvestAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    if (this->items != Right.items)
        return false;
    if (this->op_success_plant_id != Right.op_success_plant_id)
        return false;
    if (this->continue_seeds != Right.continue_seeds)
        return false;
    return true;
}

bool FPbFarmlandHarvestAck::operator!=(const FPbFarmlandHarvestAck& Right) const
{
    return !operator==(Right);
}

FPbFarmerRankUpReq::FPbFarmerRankUpReq()
{
    Reset();        
}

FPbFarmerRankUpReq::FPbFarmerRankUpReq(const idlepb::FarmerRankUpReq& Right)
{
    this->FromPb(Right);
}

void FPbFarmerRankUpReq::FromPb(const idlepb::FarmerRankUpReq& Right)
{
}

void FPbFarmerRankUpReq::ToPb(idlepb::FarmerRankUpReq* Out) const
{    
}

void FPbFarmerRankUpReq::Reset()
{    
}

void FPbFarmerRankUpReq::operator=(const idlepb::FarmerRankUpReq& Right)
{
    this->FromPb(Right);
}

bool FPbFarmerRankUpReq::operator==(const FPbFarmerRankUpReq& Right) const
{
    return true;
}

bool FPbFarmerRankUpReq::operator!=(const FPbFarmerRankUpReq& Right) const
{
    return !operator==(Right);
}

FPbFarmerRankUpAck::FPbFarmerRankUpAck()
{
    Reset();        
}

FPbFarmerRankUpAck::FPbFarmerRankUpAck(const idlepb::FarmerRankUpAck& Right)
{
    this->FromPb(Right);
}

void FPbFarmerRankUpAck::FromPb(const idlepb::FarmerRankUpAck& Right)
{
    ok = Right.ok();
}

void FPbFarmerRankUpAck::ToPb(idlepb::FarmerRankUpAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbFarmerRankUpAck::Reset()
{
    ok = bool();    
}

void FPbFarmerRankUpAck::operator=(const idlepb::FarmerRankUpAck& Right)
{
    this->FromPb(Right);
}

bool FPbFarmerRankUpAck::operator==(const FPbFarmerRankUpAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbFarmerRankUpAck::operator!=(const FPbFarmerRankUpAck& Right) const
{
    return !operator==(Right);
}

FPbFarmlandSetManagementReq::FPbFarmlandSetManagementReq()
{
    Reset();        
}

FPbFarmlandSetManagementReq::FPbFarmlandSetManagementReq(const idlepb::FarmlandSetManagementReq& Right)
{
    this->FromPb(Right);
}

void FPbFarmlandSetManagementReq::FromPb(const idlepb::FarmlandSetManagementReq& Right)
{
    plans.Empty();
    for (const auto& Elem : Right.plans())
    {
        plans.Emplace(Elem);
    }
}

void FPbFarmlandSetManagementReq::ToPb(idlepb::FarmlandSetManagementReq* Out) const
{
    for (const auto& Elem : plans)
    {
        Elem.ToPb(Out->add_plans());    
    }    
}

void FPbFarmlandSetManagementReq::Reset()
{
    plans = TArray<FPbFarmlandManagementInfo>();    
}

void FPbFarmlandSetManagementReq::operator=(const idlepb::FarmlandSetManagementReq& Right)
{
    this->FromPb(Right);
}

bool FPbFarmlandSetManagementReq::operator==(const FPbFarmlandSetManagementReq& Right) const
{
    if (this->plans != Right.plans)
        return false;
    return true;
}

bool FPbFarmlandSetManagementReq::operator!=(const FPbFarmlandSetManagementReq& Right) const
{
    return !operator==(Right);
}

FPbFarmlandSetManagementAck::FPbFarmlandSetManagementAck()
{
    Reset();        
}

FPbFarmlandSetManagementAck::FPbFarmlandSetManagementAck(const idlepb::FarmlandSetManagementAck& Right)
{
    this->FromPb(Right);
}

void FPbFarmlandSetManagementAck::FromPb(const idlepb::FarmlandSetManagementAck& Right)
{
    ok = Right.ok();
}

void FPbFarmlandSetManagementAck::ToPb(idlepb::FarmlandSetManagementAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbFarmlandSetManagementAck::Reset()
{
    ok = bool();    
}

void FPbFarmlandSetManagementAck::operator=(const idlepb::FarmlandSetManagementAck& Right)
{
    this->FromPb(Right);
}

bool FPbFarmlandSetManagementAck::operator==(const FPbFarmlandSetManagementAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbFarmlandSetManagementAck::operator!=(const FPbFarmlandSetManagementAck& Right) const
{
    return !operator==(Right);
}

FPbUpdateFarmlandStateReq::FPbUpdateFarmlandStateReq()
{
    Reset();        
}

FPbUpdateFarmlandStateReq::FPbUpdateFarmlandStateReq(const idlepb::UpdateFarmlandStateReq& Right)
{
    this->FromPb(Right);
}

void FPbUpdateFarmlandStateReq::FromPb(const idlepb::UpdateFarmlandStateReq& Right)
{
}

void FPbUpdateFarmlandStateReq::ToPb(idlepb::UpdateFarmlandStateReq* Out) const
{    
}

void FPbUpdateFarmlandStateReq::Reset()
{    
}

void FPbUpdateFarmlandStateReq::operator=(const idlepb::UpdateFarmlandStateReq& Right)
{
    this->FromPb(Right);
}

bool FPbUpdateFarmlandStateReq::operator==(const FPbUpdateFarmlandStateReq& Right) const
{
    return true;
}

bool FPbUpdateFarmlandStateReq::operator!=(const FPbUpdateFarmlandStateReq& Right) const
{
    return !operator==(Right);
}

FPbUpdateFarmlandStateAck::FPbUpdateFarmlandStateAck()
{
    Reset();        
}

FPbUpdateFarmlandStateAck::FPbUpdateFarmlandStateAck(const idlepb::UpdateFarmlandStateAck& Right)
{
    this->FromPb(Right);
}

void FPbUpdateFarmlandStateAck::FromPb(const idlepb::UpdateFarmlandStateAck& Right)
{
    farmer_friendship_exp = Right.farmer_friendship_exp();
    auto_harvest_plants.Empty();
    for (const auto& Elem : Right.auto_harvest_plants())
    {
        auto_harvest_plants.Emplace(Elem);
    }
    continue_seeds.Empty();
    for (const auto& Elem : Right.continue_seeds())
    {
        continue_seeds.Emplace(Elem);
    }
    harvest_items.Empty();
    for (const auto& Elem : Right.harvest_items())
    {
        harvest_items.Emplace(Elem);
    }
}

void FPbUpdateFarmlandStateAck::ToPb(idlepb::UpdateFarmlandStateAck* Out) const
{
    Out->set_farmer_friendship_exp(farmer_friendship_exp);
    for (const auto& Elem : auto_harvest_plants)
    {
        Out->add_auto_harvest_plants(Elem);    
    }
    for (const auto& Elem : continue_seeds)
    {
        Elem.ToPb(Out->add_continue_seeds());    
    }
    for (const auto& Elem : harvest_items)
    {
        Elem.ToPb(Out->add_harvest_items());    
    }    
}

void FPbUpdateFarmlandStateAck::Reset()
{
    farmer_friendship_exp = int32();
    auto_harvest_plants = TArray<int32>();
    continue_seeds = TArray<FPbFarmlandPlantData>();
    harvest_items = TArray<FPbSimpleItemData>();    
}

void FPbUpdateFarmlandStateAck::operator=(const idlepb::UpdateFarmlandStateAck& Right)
{
    this->FromPb(Right);
}

bool FPbUpdateFarmlandStateAck::operator==(const FPbUpdateFarmlandStateAck& Right) const
{
    if (this->farmer_friendship_exp != Right.farmer_friendship_exp)
        return false;
    if (this->auto_harvest_plants != Right.auto_harvest_plants)
        return false;
    if (this->continue_seeds != Right.continue_seeds)
        return false;
    if (this->harvest_items != Right.harvest_items)
        return false;
    return true;
}

bool FPbUpdateFarmlandStateAck::operator!=(const FPbUpdateFarmlandStateAck& Right) const
{
    return !operator==(Right);
}

FPbGetRoleInfoReq::FPbGetRoleInfoReq()
{
    Reset();        
}

FPbGetRoleInfoReq::FPbGetRoleInfoReq(const idlepb::GetRoleInfoReq& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleInfoReq::FromPb(const idlepb::GetRoleInfoReq& Right)
{
    role_id = Right.role_id();
}

void FPbGetRoleInfoReq::ToPb(idlepb::GetRoleInfoReq* Out) const
{
    Out->set_role_id(role_id);    
}

void FPbGetRoleInfoReq::Reset()
{
    role_id = int64();    
}

void FPbGetRoleInfoReq::operator=(const idlepb::GetRoleInfoReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleInfoReq::operator==(const FPbGetRoleInfoReq& Right) const
{
    if (this->role_id != Right.role_id)
        return false;
    return true;
}

bool FPbGetRoleInfoReq::operator!=(const FPbGetRoleInfoReq& Right) const
{
    return !operator==(Right);
}

FPbGetRoleInfoAck::FPbGetRoleInfoAck()
{
    Reset();        
}

FPbGetRoleInfoAck::FPbGetRoleInfoAck(const idlepb::GetRoleInfoAck& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleInfoAck::FromPb(const idlepb::GetRoleInfoAck& Right)
{
    role_info = Right.role_info();
    ok = Right.ok();
}

void FPbGetRoleInfoAck::ToPb(idlepb::GetRoleInfoAck* Out) const
{
    role_info.ToPb(Out->mutable_role_info());
    Out->set_ok(ok);    
}

void FPbGetRoleInfoAck::Reset()
{
    role_info = FPbRoleInfo();
    ok = bool();    
}

void FPbGetRoleInfoAck::operator=(const idlepb::GetRoleInfoAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleInfoAck::operator==(const FPbGetRoleInfoAck& Right) const
{
    if (this->role_info != Right.role_info)
        return false;
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbGetRoleInfoAck::operator!=(const FPbGetRoleInfoAck& Right) const
{
    return !operator==(Right);
}

FPbGetRoleFriendDataReq::FPbGetRoleFriendDataReq()
{
    Reset();        
}

FPbGetRoleFriendDataReq::FPbGetRoleFriendDataReq(const idlepb::GetRoleFriendDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleFriendDataReq::FromPb(const idlepb::GetRoleFriendDataReq& Right)
{
}

void FPbGetRoleFriendDataReq::ToPb(idlepb::GetRoleFriendDataReq* Out) const
{    
}

void FPbGetRoleFriendDataReq::Reset()
{    
}

void FPbGetRoleFriendDataReq::operator=(const idlepb::GetRoleFriendDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleFriendDataReq::operator==(const FPbGetRoleFriendDataReq& Right) const
{
    return true;
}

bool FPbGetRoleFriendDataReq::operator!=(const FPbGetRoleFriendDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetRoleFriendDataAck::FPbGetRoleFriendDataAck()
{
    Reset();        
}

FPbGetRoleFriendDataAck::FPbGetRoleFriendDataAck(const idlepb::GetRoleFriendDataAck& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleFriendDataAck::FromPb(const idlepb::GetRoleFriendDataAck& Right)
{
    data = Right.data();
    role_infos.Empty();
    for (const auto& Elem : Right.role_infos())
    {
        role_infos.Emplace(Elem);
    }
}

void FPbGetRoleFriendDataAck::ToPb(idlepb::GetRoleFriendDataAck* Out) const
{
    data.ToPb(Out->mutable_data());
    for (const auto& Elem : role_infos)
    {
        Elem.ToPb(Out->add_role_infos());    
    }    
}

void FPbGetRoleFriendDataAck::Reset()
{
    data = FPbRoleFriendData();
    role_infos = TArray<FPbSimpleRoleInfo>();    
}

void FPbGetRoleFriendDataAck::operator=(const idlepb::GetRoleFriendDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleFriendDataAck::operator==(const FPbGetRoleFriendDataAck& Right) const
{
    if (this->data != Right.data)
        return false;
    if (this->role_infos != Right.role_infos)
        return false;
    return true;
}

bool FPbGetRoleFriendDataAck::operator!=(const FPbGetRoleFriendDataAck& Right) const
{
    return !operator==(Right);
}

FPbFriendOpReq::FPbFriendOpReq()
{
    Reset();        
}

FPbFriendOpReq::FPbFriendOpReq(const idlepb::FriendOpReq& Right)
{
    this->FromPb(Right);
}

void FPbFriendOpReq::FromPb(const idlepb::FriendOpReq& Right)
{
    role_id = Right.role_id();
    op_type = static_cast<EPbFriendRelationshipType>(Right.op_type());
    reverse_op = Right.reverse_op();
}

void FPbFriendOpReq::ToPb(idlepb::FriendOpReq* Out) const
{
    Out->set_role_id(role_id);
    Out->set_op_type(static_cast<idlepb::FriendRelationshipType>(op_type));
    Out->set_reverse_op(reverse_op);    
}

void FPbFriendOpReq::Reset()
{
    role_id = int64();
    op_type = EPbFriendRelationshipType();
    reverse_op = bool();    
}

void FPbFriendOpReq::operator=(const idlepb::FriendOpReq& Right)
{
    this->FromPb(Right);
}

bool FPbFriendOpReq::operator==(const FPbFriendOpReq& Right) const
{
    if (this->role_id != Right.role_id)
        return false;
    if (this->op_type != Right.op_type)
        return false;
    if (this->reverse_op != Right.reverse_op)
        return false;
    return true;
}

bool FPbFriendOpReq::operator!=(const FPbFriendOpReq& Right) const
{
    return !operator==(Right);
}

FPbFriendOpAck::FPbFriendOpAck()
{
    Reset();        
}

FPbFriendOpAck::FPbFriendOpAck(const idlepb::FriendOpAck& Right)
{
    this->FromPb(Right);
}

void FPbFriendOpAck::FromPb(const idlepb::FriendOpAck& Right)
{
    ok = Right.ok();
    relationship_ab = static_cast<EPbFriendRelationshipType>(Right.relationship_ab());
    relationship_ba = static_cast<EPbFriendRelationshipType>(Right.relationship_ba());
}

void FPbFriendOpAck::ToPb(idlepb::FriendOpAck* Out) const
{
    Out->set_ok(ok);
    Out->set_relationship_ab(static_cast<idlepb::FriendRelationshipType>(relationship_ab));
    Out->set_relationship_ba(static_cast<idlepb::FriendRelationshipType>(relationship_ba));    
}

void FPbFriendOpAck::Reset()
{
    ok = bool();
    relationship_ab = EPbFriendRelationshipType();
    relationship_ba = EPbFriendRelationshipType();    
}

void FPbFriendOpAck::operator=(const idlepb::FriendOpAck& Right)
{
    this->FromPb(Right);
}

bool FPbFriendOpAck::operator==(const FPbFriendOpAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    if (this->relationship_ab != Right.relationship_ab)
        return false;
    if (this->relationship_ba != Right.relationship_ba)
        return false;
    return true;
}

bool FPbFriendOpAck::operator!=(const FPbFriendOpAck& Right) const
{
    return !operator==(Right);
}

FPbReplyFriendRequestReq::FPbReplyFriendRequestReq()
{
    Reset();        
}

FPbReplyFriendRequestReq::FPbReplyFriendRequestReq(const idlepb::ReplyFriendRequestReq& Right)
{
    this->FromPb(Right);
}

void FPbReplyFriendRequestReq::FromPb(const idlepb::ReplyFriendRequestReq& Right)
{
    role_id = Right.role_id();
    agree = Right.agree();
    one_click = Right.one_click();
}

void FPbReplyFriendRequestReq::ToPb(idlepb::ReplyFriendRequestReq* Out) const
{
    Out->set_role_id(role_id);
    Out->set_agree(agree);
    Out->set_one_click(one_click);    
}

void FPbReplyFriendRequestReq::Reset()
{
    role_id = int64();
    agree = bool();
    one_click = bool();    
}

void FPbReplyFriendRequestReq::operator=(const idlepb::ReplyFriendRequestReq& Right)
{
    this->FromPb(Right);
}

bool FPbReplyFriendRequestReq::operator==(const FPbReplyFriendRequestReq& Right) const
{
    if (this->role_id != Right.role_id)
        return false;
    if (this->agree != Right.agree)
        return false;
    if (this->one_click != Right.one_click)
        return false;
    return true;
}

bool FPbReplyFriendRequestReq::operator!=(const FPbReplyFriendRequestReq& Right) const
{
    return !operator==(Right);
}

FPbReplyFriendRequestAck::FPbReplyFriendRequestAck()
{
    Reset();        
}

FPbReplyFriendRequestAck::FPbReplyFriendRequestAck(const idlepb::ReplyFriendRequestAck& Right)
{
    this->FromPb(Right);
}

void FPbReplyFriendRequestAck::FromPb(const idlepb::ReplyFriendRequestAck& Right)
{
    ok = Right.ok();
    relationship_ba.Empty();
    for (const auto& Elem : Right.relationship_ba())
    {
        relationship_ba.Emplace(Elem);
    }
    failed_ids.Empty();
    for (const auto& Elem : Right.failed_ids())
    {
        failed_ids.Emplace(Elem);
    }
}

void FPbReplyFriendRequestAck::ToPb(idlepb::ReplyFriendRequestAck* Out) const
{
    Out->set_ok(ok);
    for (const auto& Elem : relationship_ba)
    {
        Out->add_relationship_ba(Elem);    
    }
    for (const auto& Elem : failed_ids)
    {
        Out->add_failed_ids(Elem);    
    }    
}

void FPbReplyFriendRequestAck::Reset()
{
    ok = bool();
    relationship_ba = TArray<int32>();
    failed_ids = TArray<int64>();    
}

void FPbReplyFriendRequestAck::operator=(const idlepb::ReplyFriendRequestAck& Right)
{
    this->FromPb(Right);
}

bool FPbReplyFriendRequestAck::operator==(const FPbReplyFriendRequestAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    if (this->relationship_ba != Right.relationship_ba)
        return false;
    if (this->failed_ids != Right.failed_ids)
        return false;
    return true;
}

bool FPbReplyFriendRequestAck::operator!=(const FPbReplyFriendRequestAck& Right) const
{
    return !operator==(Right);
}

FPbFriendSearchRoleInfoReq::FPbFriendSearchRoleInfoReq()
{
    Reset();        
}

FPbFriendSearchRoleInfoReq::FPbFriendSearchRoleInfoReq(const idlepb::FriendSearchRoleInfoReq& Right)
{
    this->FromPb(Right);
}

void FPbFriendSearchRoleInfoReq::FromPb(const idlepb::FriendSearchRoleInfoReq& Right)
{
    role_name = UTF8_TO_TCHAR(Right.role_name().c_str());
}

void FPbFriendSearchRoleInfoReq::ToPb(idlepb::FriendSearchRoleInfoReq* Out) const
{
    Out->set_role_name(TCHAR_TO_UTF8(*role_name));    
}

void FPbFriendSearchRoleInfoReq::Reset()
{
    role_name = FString();    
}

void FPbFriendSearchRoleInfoReq::operator=(const idlepb::FriendSearchRoleInfoReq& Right)
{
    this->FromPb(Right);
}

bool FPbFriendSearchRoleInfoReq::operator==(const FPbFriendSearchRoleInfoReq& Right) const
{
    if (this->role_name != Right.role_name)
        return false;
    return true;
}

bool FPbFriendSearchRoleInfoReq::operator!=(const FPbFriendSearchRoleInfoReq& Right) const
{
    return !operator==(Right);
}

FPbFriendSearchRoleInfoAck::FPbFriendSearchRoleInfoAck()
{
    Reset();        
}

FPbFriendSearchRoleInfoAck::FPbFriendSearchRoleInfoAck(const idlepb::FriendSearchRoleInfoAck& Right)
{
    this->FromPb(Right);
}

void FPbFriendSearchRoleInfoAck::FromPb(const idlepb::FriendSearchRoleInfoAck& Right)
{
    role_infos.Empty();
    for (const auto& Elem : Right.role_infos())
    {
        role_infos.Emplace(Elem);
    }
}

void FPbFriendSearchRoleInfoAck::ToPb(idlepb::FriendSearchRoleInfoAck* Out) const
{
    for (const auto& Elem : role_infos)
    {
        Elem.ToPb(Out->add_role_infos());    
    }    
}

void FPbFriendSearchRoleInfoAck::Reset()
{
    role_infos = TArray<FPbSimpleRoleInfo>();    
}

void FPbFriendSearchRoleInfoAck::operator=(const idlepb::FriendSearchRoleInfoAck& Right)
{
    this->FromPb(Right);
}

bool FPbFriendSearchRoleInfoAck::operator==(const FPbFriendSearchRoleInfoAck& Right) const
{
    if (this->role_infos != Right.role_infos)
        return false;
    return true;
}

bool FPbFriendSearchRoleInfoAck::operator!=(const FPbFriendSearchRoleInfoAck& Right) const
{
    return !operator==(Right);
}

FPbNotifyFriendMessage::FPbNotifyFriendMessage()
{
    Reset();        
}

FPbNotifyFriendMessage::FPbNotifyFriendMessage(const idlepb::NotifyFriendMessage& Right)
{
    this->FromPb(Right);
}

void FPbNotifyFriendMessage::FromPb(const idlepb::NotifyFriendMessage& Right)
{
    role_info = Right.role_info();
    is_update_state = Right.is_update_state();
    b_refused = Right.b_refused();
    friend_event = Right.friend_event();
    online = Right.online();
}

void FPbNotifyFriendMessage::ToPb(idlepb::NotifyFriendMessage* Out) const
{
    role_info.ToPb(Out->mutable_role_info());
    Out->set_is_update_state(is_update_state);
    Out->set_b_refused(b_refused);
    friend_event.ToPb(Out->mutable_friend_event());
    Out->set_online(online);    
}

void FPbNotifyFriendMessage::Reset()
{
    role_info = FPbSimpleRoleInfo();
    is_update_state = bool();
    b_refused = bool();
    friend_event = FPbFriendListItem();
    online = bool();    
}

void FPbNotifyFriendMessage::operator=(const idlepb::NotifyFriendMessage& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyFriendMessage::operator==(const FPbNotifyFriendMessage& Right) const
{
    if (this->role_info != Right.role_info)
        return false;
    if (this->is_update_state != Right.is_update_state)
        return false;
    if (this->b_refused != Right.b_refused)
        return false;
    if (this->friend_event != Right.friend_event)
        return false;
    if (this->online != Right.online)
        return false;
    return true;
}

bool FPbNotifyFriendMessage::operator!=(const FPbNotifyFriendMessage& Right) const
{
    return !operator==(Right);
}

FPbGetRoleAvatarDataReq::FPbGetRoleAvatarDataReq()
{
    Reset();        
}

FPbGetRoleAvatarDataReq::FPbGetRoleAvatarDataReq(const idlepb::GetRoleAvatarDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleAvatarDataReq::FromPb(const idlepb::GetRoleAvatarDataReq& Right)
{
    draw_this_time = Right.draw_this_time();
}

void FPbGetRoleAvatarDataReq::ToPb(idlepb::GetRoleAvatarDataReq* Out) const
{
    Out->set_draw_this_time(draw_this_time);    
}

void FPbGetRoleAvatarDataReq::Reset()
{
    draw_this_time = bool();    
}

void FPbGetRoleAvatarDataReq::operator=(const idlepb::GetRoleAvatarDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleAvatarDataReq::operator==(const FPbGetRoleAvatarDataReq& Right) const
{
    if (this->draw_this_time != Right.draw_this_time)
        return false;
    return true;
}

bool FPbGetRoleAvatarDataReq::operator!=(const FPbGetRoleAvatarDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetRoleAvatarDataAck::FPbGetRoleAvatarDataAck()
{
    Reset();        
}

FPbGetRoleAvatarDataAck::FPbGetRoleAvatarDataAck(const idlepb::GetRoleAvatarDataAck& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleAvatarDataAck::FromPb(const idlepb::GetRoleAvatarDataAck& Right)
{
    data = Right.data();
}

void FPbGetRoleAvatarDataAck::ToPb(idlepb::GetRoleAvatarDataAck* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbGetRoleAvatarDataAck::Reset()
{
    data = FPbRoleAvatarData();    
}

void FPbGetRoleAvatarDataAck::operator=(const idlepb::GetRoleAvatarDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleAvatarDataAck::operator==(const FPbGetRoleAvatarDataAck& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbGetRoleAvatarDataAck::operator!=(const FPbGetRoleAvatarDataAck& Right) const
{
    return !operator==(Right);
}

FPbDispatchAvatarReq::FPbDispatchAvatarReq()
{
    Reset();        
}

FPbDispatchAvatarReq::FPbDispatchAvatarReq(const idlepb::DispatchAvatarReq& Right)
{
    this->FromPb(Right);
}

void FPbDispatchAvatarReq::FromPb(const idlepb::DispatchAvatarReq& Right)
{
    world_index = Right.world_index();
}

void FPbDispatchAvatarReq::ToPb(idlepb::DispatchAvatarReq* Out) const
{
    Out->set_world_index(world_index);    
}

void FPbDispatchAvatarReq::Reset()
{
    world_index = int32();    
}

void FPbDispatchAvatarReq::operator=(const idlepb::DispatchAvatarReq& Right)
{
    this->FromPb(Right);
}

bool FPbDispatchAvatarReq::operator==(const FPbDispatchAvatarReq& Right) const
{
    if (this->world_index != Right.world_index)
        return false;
    return true;
}

bool FPbDispatchAvatarReq::operator!=(const FPbDispatchAvatarReq& Right) const
{
    return !operator==(Right);
}

FPbDispatchAvatarAck::FPbDispatchAvatarAck()
{
    Reset();        
}

FPbDispatchAvatarAck::FPbDispatchAvatarAck(const idlepb::DispatchAvatarAck& Right)
{
    this->FromPb(Right);
}

void FPbDispatchAvatarAck::FromPb(const idlepb::DispatchAvatarAck& Right)
{
    data = Right.data();
}

void FPbDispatchAvatarAck::ToPb(idlepb::DispatchAvatarAck* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbDispatchAvatarAck::Reset()
{
    data = FPbRoleAvatarData();    
}

void FPbDispatchAvatarAck::operator=(const idlepb::DispatchAvatarAck& Right)
{
    this->FromPb(Right);
}

bool FPbDispatchAvatarAck::operator==(const FPbDispatchAvatarAck& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbDispatchAvatarAck::operator!=(const FPbDispatchAvatarAck& Right) const
{
    return !operator==(Right);
}

FPbAvatarRankUpReq::FPbAvatarRankUpReq()
{
    Reset();        
}

FPbAvatarRankUpReq::FPbAvatarRankUpReq(const idlepb::AvatarRankUpReq& Right)
{
    this->FromPb(Right);
}

void FPbAvatarRankUpReq::FromPb(const idlepb::AvatarRankUpReq& Right)
{
}

void FPbAvatarRankUpReq::ToPb(idlepb::AvatarRankUpReq* Out) const
{    
}

void FPbAvatarRankUpReq::Reset()
{    
}

void FPbAvatarRankUpReq::operator=(const idlepb::AvatarRankUpReq& Right)
{
    this->FromPb(Right);
}

bool FPbAvatarRankUpReq::operator==(const FPbAvatarRankUpReq& Right) const
{
    return true;
}

bool FPbAvatarRankUpReq::operator!=(const FPbAvatarRankUpReq& Right) const
{
    return !operator==(Right);
}

FPbAvatarRankUpAck::FPbAvatarRankUpAck()
{
    Reset();        
}

FPbAvatarRankUpAck::FPbAvatarRankUpAck(const idlepb::AvatarRankUpAck& Right)
{
    this->FromPb(Right);
}

void FPbAvatarRankUpAck::FromPb(const idlepb::AvatarRankUpAck& Right)
{
    data = Right.data();
}

void FPbAvatarRankUpAck::ToPb(idlepb::AvatarRankUpAck* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbAvatarRankUpAck::Reset()
{
    data = FPbRoleAvatarData();    
}

void FPbAvatarRankUpAck::operator=(const idlepb::AvatarRankUpAck& Right)
{
    this->FromPb(Right);
}

bool FPbAvatarRankUpAck::operator==(const FPbAvatarRankUpAck& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbAvatarRankUpAck::operator!=(const FPbAvatarRankUpAck& Right) const
{
    return !operator==(Right);
}

FPbReceiveAvatarTempPackageReq::FPbReceiveAvatarTempPackageReq()
{
    Reset();        
}

FPbReceiveAvatarTempPackageReq::FPbReceiveAvatarTempPackageReq(const idlepb::ReceiveAvatarTempPackageReq& Right)
{
    this->FromPb(Right);
}

void FPbReceiveAvatarTempPackageReq::FromPb(const idlepb::ReceiveAvatarTempPackageReq& Right)
{
}

void FPbReceiveAvatarTempPackageReq::ToPb(idlepb::ReceiveAvatarTempPackageReq* Out) const
{    
}

void FPbReceiveAvatarTempPackageReq::Reset()
{    
}

void FPbReceiveAvatarTempPackageReq::operator=(const idlepb::ReceiveAvatarTempPackageReq& Right)
{
    this->FromPb(Right);
}

bool FPbReceiveAvatarTempPackageReq::operator==(const FPbReceiveAvatarTempPackageReq& Right) const
{
    return true;
}

bool FPbReceiveAvatarTempPackageReq::operator!=(const FPbReceiveAvatarTempPackageReq& Right) const
{
    return !operator==(Right);
}

FPbReceiveAvatarTempPackageAck::FPbReceiveAvatarTempPackageAck()
{
    Reset();        
}

FPbReceiveAvatarTempPackageAck::FPbReceiveAvatarTempPackageAck(const idlepb::ReceiveAvatarTempPackageAck& Right)
{
    this->FromPb(Right);
}

void FPbReceiveAvatarTempPackageAck::FromPb(const idlepb::ReceiveAvatarTempPackageAck& Right)
{
    data = Right.data();
}

void FPbReceiveAvatarTempPackageAck::ToPb(idlepb::ReceiveAvatarTempPackageAck* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbReceiveAvatarTempPackageAck::Reset()
{
    data = FPbRoleAvatarData();    
}

void FPbReceiveAvatarTempPackageAck::operator=(const idlepb::ReceiveAvatarTempPackageAck& Right)
{
    this->FromPb(Right);
}

bool FPbReceiveAvatarTempPackageAck::operator==(const FPbReceiveAvatarTempPackageAck& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbReceiveAvatarTempPackageAck::operator!=(const FPbReceiveAvatarTempPackageAck& Right) const
{
    return !operator==(Right);
}

FPbGetRoleBiographyDataReq::FPbGetRoleBiographyDataReq()
{
    Reset();        
}

FPbGetRoleBiographyDataReq::FPbGetRoleBiographyDataReq(const idlepb::GetRoleBiographyDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleBiographyDataReq::FromPb(const idlepb::GetRoleBiographyDataReq& Right)
{
}

void FPbGetRoleBiographyDataReq::ToPb(idlepb::GetRoleBiographyDataReq* Out) const
{    
}

void FPbGetRoleBiographyDataReq::Reset()
{    
}

void FPbGetRoleBiographyDataReq::operator=(const idlepb::GetRoleBiographyDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleBiographyDataReq::operator==(const FPbGetRoleBiographyDataReq& Right) const
{
    return true;
}

bool FPbGetRoleBiographyDataReq::operator!=(const FPbGetRoleBiographyDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetRoleBiographyDataAck::FPbGetRoleBiographyDataAck()
{
    Reset();        
}

FPbGetRoleBiographyDataAck::FPbGetRoleBiographyDataAck(const idlepb::GetRoleBiographyDataAck& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleBiographyDataAck::FromPb(const idlepb::GetRoleBiographyDataAck& Right)
{
    data = Right.data();
}

void FPbGetRoleBiographyDataAck::ToPb(idlepb::GetRoleBiographyDataAck* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbGetRoleBiographyDataAck::Reset()
{
    data = FPbRoleBiographyData();    
}

void FPbGetRoleBiographyDataAck::operator=(const idlepb::GetRoleBiographyDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleBiographyDataAck::operator==(const FPbGetRoleBiographyDataAck& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbGetRoleBiographyDataAck::operator!=(const FPbGetRoleBiographyDataAck& Right) const
{
    return !operator==(Right);
}

FPbReceiveBiographyItemReq::FPbReceiveBiographyItemReq()
{
    Reset();        
}

FPbReceiveBiographyItemReq::FPbReceiveBiographyItemReq(const idlepb::ReceiveBiographyItemReq& Right)
{
    this->FromPb(Right);
}

void FPbReceiveBiographyItemReq::FromPb(const idlepb::ReceiveBiographyItemReq& Right)
{
    cfg_ids.Empty();
    for (const auto& Elem : Right.cfg_ids())
    {
        cfg_ids.Emplace(Elem);
    }
}

void FPbReceiveBiographyItemReq::ToPb(idlepb::ReceiveBiographyItemReq* Out) const
{
    for (const auto& Elem : cfg_ids)
    {
        Out->add_cfg_ids(Elem);    
    }    
}

void FPbReceiveBiographyItemReq::Reset()
{
    cfg_ids = TArray<int32>();    
}

void FPbReceiveBiographyItemReq::operator=(const idlepb::ReceiveBiographyItemReq& Right)
{
    this->FromPb(Right);
}

bool FPbReceiveBiographyItemReq::operator==(const FPbReceiveBiographyItemReq& Right) const
{
    if (this->cfg_ids != Right.cfg_ids)
        return false;
    return true;
}

bool FPbReceiveBiographyItemReq::operator!=(const FPbReceiveBiographyItemReq& Right) const
{
    return !operator==(Right);
}

FPbReceiveBiographyItemAck::FPbReceiveBiographyItemAck()
{
    Reset();        
}

FPbReceiveBiographyItemAck::FPbReceiveBiographyItemAck(const idlepb::ReceiveBiographyItemAck& Right)
{
    this->FromPb(Right);
}

void FPbReceiveBiographyItemAck::FromPb(const idlepb::ReceiveBiographyItemAck& Right)
{
    ok = Right.ok();
}

void FPbReceiveBiographyItemAck::ToPb(idlepb::ReceiveBiographyItemAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbReceiveBiographyItemAck::Reset()
{
    ok = bool();    
}

void FPbReceiveBiographyItemAck::operator=(const idlepb::ReceiveBiographyItemAck& Right)
{
    this->FromPb(Right);
}

bool FPbReceiveBiographyItemAck::operator==(const FPbReceiveBiographyItemAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbReceiveBiographyItemAck::operator!=(const FPbReceiveBiographyItemAck& Right) const
{
    return !operator==(Right);
}

FPbNotifyBiographyMessage::FPbNotifyBiographyMessage()
{
    Reset();        
}

FPbNotifyBiographyMessage::FPbNotifyBiographyMessage(const idlepb::NotifyBiographyMessage& Right)
{
    this->FromPb(Right);
}

void FPbNotifyBiographyMessage::FromPb(const idlepb::NotifyBiographyMessage& Right)
{
}

void FPbNotifyBiographyMessage::ToPb(idlepb::NotifyBiographyMessage* Out) const
{    
}

void FPbNotifyBiographyMessage::Reset()
{    
}

void FPbNotifyBiographyMessage::operator=(const idlepb::NotifyBiographyMessage& Right)
{
    this->FromPb(Right);
}

bool FPbNotifyBiographyMessage::operator==(const FPbNotifyBiographyMessage& Right) const
{
    return true;
}

bool FPbNotifyBiographyMessage::operator!=(const FPbNotifyBiographyMessage& Right) const
{
    return !operator==(Right);
}

FPbGetBiographyEventDataReq::FPbGetBiographyEventDataReq()
{
    Reset();        
}

FPbGetBiographyEventDataReq::FPbGetBiographyEventDataReq(const idlepb::GetBiographyEventDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetBiographyEventDataReq::FromPb(const idlepb::GetBiographyEventDataReq& Right)
{
}

void FPbGetBiographyEventDataReq::ToPb(idlepb::GetBiographyEventDataReq* Out) const
{    
}

void FPbGetBiographyEventDataReq::Reset()
{    
}

void FPbGetBiographyEventDataReq::operator=(const idlepb::GetBiographyEventDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetBiographyEventDataReq::operator==(const FPbGetBiographyEventDataReq& Right) const
{
    return true;
}

bool FPbGetBiographyEventDataReq::operator!=(const FPbGetBiographyEventDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetBiographyEventDataAck::FPbGetBiographyEventDataAck()
{
    Reset();        
}

FPbGetBiographyEventDataAck::FPbGetBiographyEventDataAck(const idlepb::GetBiographyEventDataAck& Right)
{
    this->FromPb(Right);
}

void FPbGetBiographyEventDataAck::FromPb(const idlepb::GetBiographyEventDataAck& Right)
{
    biography_lists.Empty();
    for (const auto& Elem : Right.biography_lists())
    {
        biography_lists.Emplace(Elem);
    }
    server_counter_data = Right.server_counter_data();
}

void FPbGetBiographyEventDataAck::ToPb(idlepb::GetBiographyEventDataAck* Out) const
{
    for (const auto& Elem : biography_lists)
    {
        Elem.ToPb(Out->add_biography_lists());    
    }
    server_counter_data.ToPb(Out->mutable_server_counter_data());    
}

void FPbGetBiographyEventDataAck::Reset()
{
    biography_lists = TArray<FPbBiographyEventLeaderboardList>();
    server_counter_data = FPbServerCounterData();    
}

void FPbGetBiographyEventDataAck::operator=(const idlepb::GetBiographyEventDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetBiographyEventDataAck::operator==(const FPbGetBiographyEventDataAck& Right) const
{
    if (this->biography_lists != Right.biography_lists)
        return false;
    if (this->server_counter_data != Right.server_counter_data)
        return false;
    return true;
}

bool FPbGetBiographyEventDataAck::operator!=(const FPbGetBiographyEventDataAck& Right) const
{
    return !operator==(Right);
}

FPbReceiveBiographyEventItemReq::FPbReceiveBiographyEventItemReq()
{
    Reset();        
}

FPbReceiveBiographyEventItemReq::FPbReceiveBiographyEventItemReq(const idlepb::ReceiveBiographyEventItemReq& Right)
{
    this->FromPb(Right);
}

void FPbReceiveBiographyEventItemReq::FromPb(const idlepb::ReceiveBiographyEventItemReq& Right)
{
    cfg_id = Right.cfg_id();
}

void FPbReceiveBiographyEventItemReq::ToPb(idlepb::ReceiveBiographyEventItemReq* Out) const
{
    Out->set_cfg_id(cfg_id);    
}

void FPbReceiveBiographyEventItemReq::Reset()
{
    cfg_id = int32();    
}

void FPbReceiveBiographyEventItemReq::operator=(const idlepb::ReceiveBiographyEventItemReq& Right)
{
    this->FromPb(Right);
}

bool FPbReceiveBiographyEventItemReq::operator==(const FPbReceiveBiographyEventItemReq& Right) const
{
    if (this->cfg_id != Right.cfg_id)
        return false;
    return true;
}

bool FPbReceiveBiographyEventItemReq::operator!=(const FPbReceiveBiographyEventItemReq& Right) const
{
    return !operator==(Right);
}

FPbReceiveBiographyEventItemAck::FPbReceiveBiographyEventItemAck()
{
    Reset();        
}

FPbReceiveBiographyEventItemAck::FPbReceiveBiographyEventItemAck(const idlepb::ReceiveBiographyEventItemAck& Right)
{
    this->FromPb(Right);
}

void FPbReceiveBiographyEventItemAck::FromPb(const idlepb::ReceiveBiographyEventItemAck& Right)
{
    items.Empty();
    for (const auto& Elem : Right.items())
    {
        items.Emplace(Elem);
    }
}

void FPbReceiveBiographyEventItemAck::ToPb(idlepb::ReceiveBiographyEventItemAck* Out) const
{
    for (const auto& Elem : items)
    {
        Elem.ToPb(Out->add_items());    
    }    
}

void FPbReceiveBiographyEventItemAck::Reset()
{
    items = TArray<FPbSimpleItemData>();    
}

void FPbReceiveBiographyEventItemAck::operator=(const idlepb::ReceiveBiographyEventItemAck& Right)
{
    this->FromPb(Right);
}

bool FPbReceiveBiographyEventItemAck::operator==(const FPbReceiveBiographyEventItemAck& Right) const
{
    if (this->items != Right.items)
        return false;
    return true;
}

bool FPbReceiveBiographyEventItemAck::operator!=(const FPbReceiveBiographyEventItemAck& Right) const
{
    return !operator==(Right);
}

FPbAddBiographyRoleLogReq::FPbAddBiographyRoleLogReq()
{
    Reset();        
}

FPbAddBiographyRoleLogReq::FPbAddBiographyRoleLogReq(const idlepb::AddBiographyRoleLogReq& Right)
{
    this->FromPb(Right);
}

void FPbAddBiographyRoleLogReq::FromPb(const idlepb::AddBiographyRoleLogReq& Right)
{
    log = Right.log();
}

void FPbAddBiographyRoleLogReq::ToPb(idlepb::AddBiographyRoleLogReq* Out) const
{
    log.ToPb(Out->mutable_log());    
}

void FPbAddBiographyRoleLogReq::Reset()
{
    log = FPbBiographyRoleLog();    
}

void FPbAddBiographyRoleLogReq::operator=(const idlepb::AddBiographyRoleLogReq& Right)
{
    this->FromPb(Right);
}

bool FPbAddBiographyRoleLogReq::operator==(const FPbAddBiographyRoleLogReq& Right) const
{
    if (this->log != Right.log)
        return false;
    return true;
}

bool FPbAddBiographyRoleLogReq::operator!=(const FPbAddBiographyRoleLogReq& Right) const
{
    return !operator==(Right);
}

FPbAddBiographyRoleLogAck::FPbAddBiographyRoleLogAck()
{
    Reset();        
}

FPbAddBiographyRoleLogAck::FPbAddBiographyRoleLogAck(const idlepb::AddBiographyRoleLogAck& Right)
{
    this->FromPb(Right);
}

void FPbAddBiographyRoleLogAck::FromPb(const idlepb::AddBiographyRoleLogAck& Right)
{
    log = Right.log();
}

void FPbAddBiographyRoleLogAck::ToPb(idlepb::AddBiographyRoleLogAck* Out) const
{
    log.ToPb(Out->mutable_log());    
}

void FPbAddBiographyRoleLogAck::Reset()
{
    log = FPbBiographyRoleLog();    
}

void FPbAddBiographyRoleLogAck::operator=(const idlepb::AddBiographyRoleLogAck& Right)
{
    this->FromPb(Right);
}

bool FPbAddBiographyRoleLogAck::operator==(const FPbAddBiographyRoleLogAck& Right) const
{
    if (this->log != Right.log)
        return false;
    return true;
}

bool FPbAddBiographyRoleLogAck::operator!=(const FPbAddBiographyRoleLogAck& Right) const
{
    return !operator==(Right);
}

FPbGetRoleVipShopDataReq::FPbGetRoleVipShopDataReq()
{
    Reset();        
}

FPbGetRoleVipShopDataReq::FPbGetRoleVipShopDataReq(const idlepb::GetRoleVipShopDataReq& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleVipShopDataReq::FromPb(const idlepb::GetRoleVipShopDataReq& Right)
{
}

void FPbGetRoleVipShopDataReq::ToPb(idlepb::GetRoleVipShopDataReq* Out) const
{    
}

void FPbGetRoleVipShopDataReq::Reset()
{    
}

void FPbGetRoleVipShopDataReq::operator=(const idlepb::GetRoleVipShopDataReq& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleVipShopDataReq::operator==(const FPbGetRoleVipShopDataReq& Right) const
{
    return true;
}

bool FPbGetRoleVipShopDataReq::operator!=(const FPbGetRoleVipShopDataReq& Right) const
{
    return !operator==(Right);
}

FPbGetRoleVipShopDataAck::FPbGetRoleVipShopDataAck()
{
    Reset();        
}

FPbGetRoleVipShopDataAck::FPbGetRoleVipShopDataAck(const idlepb::GetRoleVipShopDataAck& Right)
{
    this->FromPb(Right);
}

void FPbGetRoleVipShopDataAck::FromPb(const idlepb::GetRoleVipShopDataAck& Right)
{
    data = Right.data();
}

void FPbGetRoleVipShopDataAck::ToPb(idlepb::GetRoleVipShopDataAck* Out) const
{
    data.ToPb(Out->mutable_data());    
}

void FPbGetRoleVipShopDataAck::Reset()
{
    data = FPbRoleVipShopData();    
}

void FPbGetRoleVipShopDataAck::operator=(const idlepb::GetRoleVipShopDataAck& Right)
{
    this->FromPb(Right);
}

bool FPbGetRoleVipShopDataAck::operator==(const FPbGetRoleVipShopDataAck& Right) const
{
    if (this->data != Right.data)
        return false;
    return true;
}

bool FPbGetRoleVipShopDataAck::operator!=(const FPbGetRoleVipShopDataAck& Right) const
{
    return !operator==(Right);
}

FPbVipShopBuyReq::FPbVipShopBuyReq()
{
    Reset();        
}

FPbVipShopBuyReq::FPbVipShopBuyReq(const idlepb::VipShopBuyReq& Right)
{
    this->FromPb(Right);
}

void FPbVipShopBuyReq::FromPb(const idlepb::VipShopBuyReq& Right)
{
    index = Right.index();
    num = Right.num();
}

void FPbVipShopBuyReq::ToPb(idlepb::VipShopBuyReq* Out) const
{
    Out->set_index(index);
    Out->set_num(num);    
}

void FPbVipShopBuyReq::Reset()
{
    index = int32();
    num = int32();    
}

void FPbVipShopBuyReq::operator=(const idlepb::VipShopBuyReq& Right)
{
    this->FromPb(Right);
}

bool FPbVipShopBuyReq::operator==(const FPbVipShopBuyReq& Right) const
{
    if (this->index != Right.index)
        return false;
    if (this->num != Right.num)
        return false;
    return true;
}

bool FPbVipShopBuyReq::operator!=(const FPbVipShopBuyReq& Right) const
{
    return !operator==(Right);
}

FPbVipShopBuyAck::FPbVipShopBuyAck()
{
    Reset();        
}

FPbVipShopBuyAck::FPbVipShopBuyAck(const idlepb::VipShopBuyAck& Right)
{
    this->FromPb(Right);
}

void FPbVipShopBuyAck::FromPb(const idlepb::VipShopBuyAck& Right)
{
    ok = Right.ok();
}

void FPbVipShopBuyAck::ToPb(idlepb::VipShopBuyAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbVipShopBuyAck::Reset()
{
    ok = bool();    
}

void FPbVipShopBuyAck::operator=(const idlepb::VipShopBuyAck& Right)
{
    this->FromPb(Right);
}

bool FPbVipShopBuyAck::operator==(const FPbVipShopBuyAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbVipShopBuyAck::operator!=(const FPbVipShopBuyAck& Right) const
{
    return !operator==(Right);
}