#include "ltps/common/EconomySystem.h"
#include "ll/api/base/StdInt.h"
#include "ll/api/i18n/I18n.h"
#include "ll/api/service/Bedrock.h"
#include "ll/api/service/PlayerInfo.h"
#include "ltps/TeleportSystem.h"
#include "ltps/base/Config.h"
#include "ltps/utils/JsonUtls.h"
#include "mc/platform/UUID.h"
#include <mc/world/actor/player/Player.h>
#include <mc/world/level/Level.h>
#include <mc/world/scores/Objective.h>
#include <mc/world/scores/PlayerScoreSetFunction.h>
#include <mc/world/scores/ScoreInfo.h>
#include <mc/world/scores/Scoreboard.h>
#include <mc/world/scores/ScoreboardId.h>
#include <mc/world/scores/ScoreboardOperationResult.h>


#include <memory>
#include <stdexcept>
#include <string>


#ifdef _WIN32
#include <Windows.h>
#endif

namespace ltps {


std::shared_ptr<EconomySystem> EconomySystemManager::createEconomySystem() const {
    auto& cfg = getConfig();
    if (!cfg.enabled) {
        TeleportSystem::getInstance().getSelf().getLogger().debug("EconomySystem not enabled, using EmptyEconomySystem."
        );
        return std::make_shared<internals::EmptyEconomySystem>(); // 未启用经济系统，使用空实现
    }

    switch (cfg.kit) {
    case ltps::EconomySystem::Kit::LegacyMoney: {
        TeleportSystem::getInstance().getSelf().getLogger().debug("EconomySystem using LegacyMoney EconomySystem.");
        return std::make_shared<internals::LegacyMoneyEconomySystem>();
    }
    case ltps::EconomySystem::Kit::ScoreBoard: {
        // throw std::runtime_error("ScoreBoard Economy System not implemented yet.");
        TeleportSystem::getInstance().getSelf().getLogger().debug("EconomySystem using ScoreBoard EconomySystem.");
        return std::make_shared<internals::ScoreBoardEconomySystem>();
    }
    }

    throw std::runtime_error("Unknown EconomySystem Kit.");
}

EconomySystemManager& EconomySystemManager::getInstance() {
    static EconomySystemManager instance;
    return instance;
}

std::shared_ptr<EconomySystem> EconomySystemManager::getEconomySystem() const {
    std::lock_guard<std::mutex> lock(mInstanceMutex);
    if (!mEconomySystem) {
        throw std::runtime_error("EconomySystem not initialized.");
    }
    return mEconomySystem;
}

EconomySystem::Config& EconomySystemManager::getConfig() const { return config::getConfig().economySystem; }

void EconomySystemManager::initEconomySystem() {
    std::lock_guard<std::mutex> lock(mInstanceMutex);
    if (!mEconomySystem) {
        mEconomySystem = createEconomySystem();
        return;
    }
}
void EconomySystemManager::reloadEconomySystem() {
    std::lock_guard<std::mutex> lock(mInstanceMutex);
    mEconomySystem = createEconomySystem();
}


EconomySystemManager::EconomySystemManager() = default;
std::shared_ptr<EconomySystem> EconomySystemManager::operator->() const { return mEconomySystem; }
EconomySystem::EconomySystem() = default;

bool EconomySystem::has(Player& player, llong amount) const { return get(player) >= amount; }
bool EconomySystem::has(mce::UUID const& uuid, llong amount) const { return get(uuid) >= amount; }

std::string EconomySystem::getCostMessage(Player& player, long long amount, std::string const& localeCode) const {
    using ll::i18n_literals::operator""_trl;

    auto&                    config = EconomySystemManager::getInstance().getConfig();
    static const std::string prefix = "\n[Tip] ";

    if (config.enabled) {
        llong currentMoney = get(player);
        bool  isEnough     = currentMoney >= amount;

        return "\n[Tip] 本次操作需要: {0} {1} | 当前余额: {2} | 剩余余额: {3} | {4}"_trl(
            localeCode,
            amount,
            config.economyName,
            currentMoney,
            currentMoney - amount,
            isEnough ? "余额充足"_trl(localeCode) : "余额不足"_trl(localeCode)
        );
    }
    return "\n[Tip] 经济系统未启用，本次操作不消耗 {}"_trl(localeCode, config.economyName);
}

void EconomySystem::sendNotEnoughMoneyMessage(Player& player, long long amount, std::string const& localeCode) const {
    auto& config = EconomySystemManager::getInstance().getConfig();

    player.sendMessage("§c[EconomySystem] 操作失败，需要 {0} {1}，当前余额 {2}"_trl(
        localeCode,
        amount,
        config.economyName,
        get(player)
    ));
}

namespace internals {

#ifdef _WIN32
static const wchar_t* LEGACY_MONEY_MODULE_NAME = L"LegacyMoney.dll";
#define THROW_LEGACY_MONEY_NOT_LOADED() throw std::runtime_error("LegacyMoney not loaded.");

// c function pointer
using LLMoney_Get_Func    = llong (*)(std::string);
using LLMoney_Set_Func    = bool (*)(std::string, llong);
using LLMoney_Add_Func    = bool (*)(std::string, llong);
using LLMoney_Reduce_Func = bool (*)(std::string, llong);
using LLMoney_Trans_Func  = bool (*)(std::string from, std::string to, llong val, const std::string& note);

LegacyMoneyEconomySystem::LegacyMoneyEconomySystem() : EconomySystem() {}

std::optional<std::string> LegacyMoneyEconomySystem::getXuidFromPlayerInfo(mce::UUID const& uuid) const {
    auto info = ll::service::PlayerInfo::getInstance().fromUuid(uuid);
    if (!info) {
        return std::nullopt;
    }
    return info->xuid;
}

bool LegacyMoneyEconomySystem::isLegacyMoneyLoaded() const {
    return GetModuleHandle(LEGACY_MONEY_MODULE_NAME) != nullptr;
}

long long LegacyMoneyEconomySystem::get(Player& player) const {
    if (!isLegacyMoneyLoaded()) {
        THROW_LEGACY_MONEY_NOT_LOADED()
    }

    auto func = (LLMoney_Get_Func)GetProcAddress(GetModuleHandle(LEGACY_MONEY_MODULE_NAME), "LLMoney_Get");
    if (!func) {
        throw std::runtime_error("Dynamic call to LLMoney_Get failed.");
    }
    return func(player.getXuid());
}
long long LegacyMoneyEconomySystem::get(mce::UUID const& uuid) const {
    if (!isLegacyMoneyLoaded()) {
        THROW_LEGACY_MONEY_NOT_LOADED()
    }
    auto xuid = getXuidFromPlayerInfo(uuid);
    if (!xuid) {
        return 0;
    }
    auto func = (LLMoney_Get_Func)GetProcAddress(GetModuleHandle(LEGACY_MONEY_MODULE_NAME), "LLMoney_Get");
    if (!func) {
        throw std::runtime_error("Dynamic call to LLMoney_Get failed.");
    }
    return func(*xuid);
}

bool LegacyMoneyEconomySystem::set(Player& player, long long amount) const {
    if (!isLegacyMoneyLoaded()) {
        THROW_LEGACY_MONEY_NOT_LOADED()
    }
    auto func = (LLMoney_Set_Func)GetProcAddress(GetModuleHandle(LEGACY_MONEY_MODULE_NAME), "LLMoney_Set");
    if (!func) {
        throw std::runtime_error("Dynamic call to LLMoney_Set failed.");
    }
    return func(player.getXuid(), amount);
}
bool LegacyMoneyEconomySystem::set(mce::UUID const& uuid, long long amount) const {
    if (!isLegacyMoneyLoaded()) {
        THROW_LEGACY_MONEY_NOT_LOADED()
    }
    auto xuid = getXuidFromPlayerInfo(uuid);
    if (!xuid) {
        return false;
    }
    auto func = (LLMoney_Set_Func)GetProcAddress(GetModuleHandle(LEGACY_MONEY_MODULE_NAME), "LLMoney_Set");
    if (!func) {
        throw std::runtime_error("Dynamic call to LLMoney_Set failed.");
    }
    return func(*xuid, amount);
}

bool LegacyMoneyEconomySystem::add(Player& player, long long amount) const {
    if (!isLegacyMoneyLoaded()) {
        THROW_LEGACY_MONEY_NOT_LOADED()
    }
    auto func = (LLMoney_Add_Func)GetProcAddress(GetModuleHandle(LEGACY_MONEY_MODULE_NAME), "LLMoney_Add");
    if (!func) {
        throw std::runtime_error("Dynamic call to LLMoney_Add failed.");
    }
    return func(player.getXuid(), amount);
}
bool LegacyMoneyEconomySystem::add(mce::UUID const& uuid, long long amount) const {
    if (!isLegacyMoneyLoaded()) {
        THROW_LEGACY_MONEY_NOT_LOADED()
    }
    auto xuid = getXuidFromPlayerInfo(uuid);
    if (!xuid) {
        return false;
    }
    auto func = (LLMoney_Add_Func)GetProcAddress(GetModuleHandle(LEGACY_MONEY_MODULE_NAME), "LLMoney_Add");
    if (!func) {
        throw std::runtime_error("Dynamic call to LLMoney_Add failed.");
    }
    return func(*xuid, amount);
}

bool LegacyMoneyEconomySystem::reduce(Player& player, long long amount) const {
    if (!isLegacyMoneyLoaded()) {
        THROW_LEGACY_MONEY_NOT_LOADED()
    }
    auto func = (LLMoney_Reduce_Func)GetProcAddress(GetModuleHandle(LEGACY_MONEY_MODULE_NAME), "LLMoney_Reduce");
    if (!func) {
        throw std::runtime_error("Dynamic call to LLMoney_Reduce failed.");
    }
    return func(player.getXuid(), amount);
}
bool LegacyMoneyEconomySystem::reduce(mce::UUID const& uuid, long long amount) const {
    if (!isLegacyMoneyLoaded()) {
        THROW_LEGACY_MONEY_NOT_LOADED()
    }
    auto xuid = getXuidFromPlayerInfo(uuid);
    if (!xuid) {
        return false;
    }
    auto func = (LLMoney_Reduce_Func)GetProcAddress(GetModuleHandle(LEGACY_MONEY_MODULE_NAME), "LLMoney_Reduce");
    if (!func) {
        throw std::runtime_error("Dynamic call to LLMoney_Reduce failed.");
    }
    return func(*xuid, amount);
}

bool LegacyMoneyEconomySystem::transfer(Player& from, Player& to, long long amount) const {
    if (!isLegacyMoneyLoaded()) {
        THROW_LEGACY_MONEY_NOT_LOADED()
    }
    auto func = (LLMoney_Trans_Func)GetProcAddress(GetModuleHandle(LEGACY_MONEY_MODULE_NAME), "LLMoney_Trans");
    if (!func) {
        throw std::runtime_error("Dynamic call to LLMoney_Trans failed.");
    }
    return func(from.getXuid(), to.getXuid(), amount, "TeleportSystem Transfer");
}
bool LegacyMoneyEconomySystem::transfer(mce::UUID const& from, mce::UUID const& to, long long amount) const {
    if (!isLegacyMoneyLoaded()) {
        THROW_LEGACY_MONEY_NOT_LOADED()
    }
    auto fromXuid = getXuidFromPlayerInfo(from);
    if (!fromXuid) {
        return false;
    }
    auto toXuid = getXuidFromPlayerInfo(to);
    if (!toXuid) {
        return false;
    }
    auto func = (LLMoney_Trans_Func)GetProcAddress(GetModuleHandle(LEGACY_MONEY_MODULE_NAME), "LLMoney_Trans");
    if (!func) {
        throw std::runtime_error("Dynamic call to LLMoney_Trans failed.");
    }
    return func(*fromXuid, *toXuid, amount, "PLand internal transfer");
}
#endif


} // namespace internals

namespace internals {
ScoreBoardEconomySystem::ScoreBoardEconomySystem() : EconomySystem() {}

llong ScoreBoardEconomySystem::get(Player& player) const {
    auto& cfg = getConfig();

    Scoreboard& scoreboard = ll::service::getLevel()->getScoreboard();
    Objective*  obj        = scoreboard.getObjective(cfg.economySystem.scoreboardName);
    if (!obj) {
        return 0;
    }

    ScoreboardId const& id = scoreboard.getScoreboardId(player);
    if (id.mRawID == ScoreboardId::INVALID().mRawID) {
        scoreboard.createScoreboardId(player);
    }

    return obj->getPlayerScore(id).mValue;
}

llong ScoreBoardEconomySystem::get(mce::UUID const& uuid) const {
    auto player = ll::service::getLevel()->getPlayer(uuid);
    if (!player) {
        // land::PLand::getInstance().getSelf().getLogger().error(
        //     "[ScoreBoardInterface] Offline operations on the scoreboard are not supported"
        // );
        return 0;
    }
    return get(*player);
}

bool ScoreBoardEconomySystem::set(Player& player, llong amount) const {
    auto& cfg = getConfig();

    Scoreboard& scoreboard = ll::service::getLevel()->getScoreboard();
    Objective*  obj        = scoreboard.getObjective(cfg.economySystem.scoreboardName);
    if (!obj) {
        // land::PLand::getInstance().getSelf().getLogger().error(
        //     "[ScoreBoardInterface] Could not find scoreboard: {}",
        //     cfg.scoreboardName
        // );
        return false;
    }
    const ScoreboardId& id = scoreboard.getScoreboardId(player);
    if (id.mRawID == ScoreboardId::INVALID().mRawID) {
        scoreboard.createScoreboardId(player);
    }
    ScoreboardOperationResult result;
    scoreboard.modifyPlayerScore(result, id, *obj, static_cast<int>(amount), PlayerScoreSetFunction::Set);
    return result == ScoreboardOperationResult::Success;
}

bool ScoreBoardEconomySystem::set(mce::UUID const& uuid, llong amount) const {
    auto player = ll::service::getLevel()->getPlayer(uuid);
    if (!player) {
        // land::PLand::getInstance().getSelf().getLogger().error(
        //     "[ScoreBoardInterface] Offline operations on the scoreboard are not supported"
        // );
        return false;
    }
    return set(*player, amount);
}

bool ScoreBoardEconomySystem::add(Player& player, llong amount) const {
    if (amount < 0) return false;
    auto& cfg = getConfig();

    Scoreboard& scoreboard = ll::service::getLevel()->getScoreboard();
    Objective*  obj        = scoreboard.getObjective(cfg.economySystem.scoreboardName);
    if (!obj) {
        // land::PLand::getInstance().getSelf().getLogger().error(
        //     "[ScoreBoardInterface] Could not find scoreboard: {}",
        //     cfg.scoreboardName
        // );
        return false;
    }
    const ScoreboardId& id = scoreboard.getScoreboardId(player);
    if (id.mRawID == ScoreboardId::INVALID().mRawID) {
        scoreboard.createScoreboardId(player);
    }
    ScoreboardOperationResult result;
    scoreboard.modifyPlayerScore(result, id, *obj, static_cast<int>(amount), PlayerScoreSetFunction::Add);
    return result == ScoreboardOperationResult::Success;
}

bool ScoreBoardEconomySystem::add(mce::UUID const& uuid, llong amount) const {
    if (amount < 0) return false;
    auto player = ll::service::getLevel()->getPlayer(uuid);
    if (!player) {
        // land::PLand::getInstance().getSelf().getLogger().error(
        //     "[ScoreBoardInterface] Offline operations on the scoreboard are not supported"
        // );
        return false;
    }
    return add(*player, amount);
}

bool ScoreBoardEconomySystem::reduce(Player& player, llong amount) const {
    if (amount < 0) return false;
    if (!has(player, amount)) return false;
    auto& cfg = getConfig();

    Scoreboard& scoreboard = ll::service::getLevel()->getScoreboard();
    Objective*  obj        = scoreboard.getObjective(cfg.economySystem.scoreboardName);
    if (!obj) {
        // land::PLand::getInstance().getSelf().getLogger().error(
        //     "[ScoreBoardInterface] Could not find scoreboard: {}",
        //     cfg.scoreboardName
        // );
        return false;
    }
    const ScoreboardId& id = scoreboard.getScoreboardId(player);
    if (id.mRawID == ScoreboardId::INVALID().mRawID) {
        scoreboard.createScoreboardId(player);
    }
    ScoreboardOperationResult result;
    scoreboard.modifyPlayerScore(result, id, *obj, static_cast<int>(amount), PlayerScoreSetFunction::Subtract);
    return result == ScoreboardOperationResult::Success;
}

bool ScoreBoardEconomySystem::reduce(mce::UUID const& uuid, llong amount) const {
    if (amount < 0) return false;
    if (!has(uuid, amount)) return false;
    auto player = ll::service::getLevel()->getPlayer(uuid);
    if (!player) {
        // land::PLand::getInstance().getSelf().getLogger().error(
        //     "[ScoreBoardInterface] Offline operations on the scoreboard are not supported"
        // );
        return false;
    }
    return reduce(*player, amount);
}

bool ScoreBoardEconomySystem::transfer(Player& from, Player& to, llong amount) const {
    if (!reduce(from, amount)) {
        return false;
    }
    if (!add(to, amount)) {
        (void)add(from, amount); // rollback
        return false;
    }
    return true;
}

bool ScoreBoardEconomySystem::transfer(mce::UUID const& from, mce::UUID const& to, llong amount) const {
    auto fromPlayer = ll::service::getLevel()->getPlayer(from);
    auto toPlayer   = ll::service::getLevel()->getPlayer(to);

    if (!fromPlayer || !toPlayer) {
        // land::PLand::getInstance().getSelf().getLogger().error(
        //     "[ScoreBoardInterface] Offline operations on the scoreboard are not supported"
        // );
        return false;
    }
    return transfer(*fromPlayer, *toPlayer, amount);
}
} // namespace internals

namespace internals {
EmptyEconomySystem::EmptyEconomySystem() = default;
llong EmptyEconomySystem::get(Player&) const { return 0; }
llong EmptyEconomySystem::get(mce::UUID const&) const { return 0; }

bool EmptyEconomySystem::set(Player&, llong) const { return true; }
bool EmptyEconomySystem::set(mce::UUID const&, llong) const { return true; }

bool EmptyEconomySystem::add(Player&, llong) const { return true; }
bool EmptyEconomySystem::add(mce::UUID const&, llong) const { return true; }

bool EmptyEconomySystem::reduce(Player&, llong) const { return true; }
bool EmptyEconomySystem::reduce(mce::UUID const&, llong) const { return true; }

bool EmptyEconomySystem::transfer(Player&, Player&, llong) const { return true; }
bool EmptyEconomySystem::transfer(mce::UUID const&, mce::UUID const&, llong) const { return true; }
} // namespace internals


} // namespace ltps
