#include "DeathGUI.h"

#include "ltps/TeleportSystem.h"
#include "ltps/modules/death/DeathStorage.h"
#include "ltps/modules/death/event/DeathEvents.h"
#include "ltps/utils/McUtils.h"

#include <ll/api/event/EventBus.h>
#include <mc/world/level/dimension/VanillaDimensions.h>

namespace ltps ::death {


void DeathGUI::sendMainMenu(Player& player, BackCB backCb) {
    auto localeCode = player.getLocaleCode();

    auto infos = TeleportSystem::getInstance().getStorageManager().getStorage<DeathStorage>()->getDeathInfos(
        player.getRealName()
    );

    if (!infos || infos->empty()) {
        mc_utils::sendText(player, "您还没有任何死亡信息"_trl(localeCode));
        return;
    }

    auto fm = BackSimpleForm{std::move(backCb)};
    fm.setTitle("Death - 死亡信息列表"_trl(localeCode));
    fm.setContent("您有 {0} 条死亡信息"_trl(localeCode, infos->size()));

    int index = 0;
    for (auto& info : *infos) {
        fm.appendButton("{}\n{}"_tr(info.time, info.toPosString()), [index](Player& self) {
            sendBackGUI(self, index, BackSimpleForm::makeCallback<sendMainMenu>(nullptr));
        });
        index++;
    }

    fm.sendTo(player);
}


void DeathGUI::sendBackGUI(Player& player, int index, BackCB backCb) {
    auto localeCode = player.getLocaleCode();

    auto info = TeleportSystem::getInstance().getStorageManager().getStorage<DeathStorage>()->getSpecificDeathInfo(
        player.getRealName(),
        index
    );
    if (!info) {
        mc_utils::sendText(player, "您还没有任何死亡信息"_trl(localeCode));
        return;
    }

    BackSimpleForm{std::move(backCb)}
        .setTitle("Death - 死亡信息"_trl(localeCode))
        .setContent("死亡时间: {0}\n死亡坐标: {1}"_trl(localeCode, info->time, info->toPosString()))
        .appendButton(
            "前往死亡点"_trl(localeCode),
            [index](Player& self) {
                ll::event::EventBus::getInstance().publish(PlayerRequestBackDeathPointEvent{self, index});
            }
        )
        .appendButton("取消"_trl(localeCode))
        .sendTo(player);
}


} // namespace ltps::death