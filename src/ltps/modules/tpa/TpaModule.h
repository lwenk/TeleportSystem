#pragma once
#include "TpaRequestPool.h"
#include "ll/api/event/ListenerBase.h"
#include "ltps/Global.h"
#include "ltps/common/Cooldown.h"
#include "ltps/modules/IModule.h"
#include <vector>


namespace ltps::tpa {


class TpaModule final : public IModule {
    Cooldown mCooldown;

    std::unique_ptr<TpaRequestPool> mTpaRequestPool;

    std::vector<ll::event::ListenerPtr> mListeners;

public:
    TPS_DISALLOW_COPY(TpaModule);

    TPSAPI explicit TpaModule();

    inline static std::string name = "TpaModule";
    TPSNDAPI std::string getModuleName() const override { return name; }

    TPSNDAPI std::vector<std::string> getDependencies() const override;

    TPSAPI bool isLoadable() const override;

    TPSNDAPI bool init() override;

    TPSNDAPI bool enable() override;

    TPSNDAPI bool disable() override;

    TPSNDAPI Cooldown& getCooldown();

    TPSNDAPI TpaRequestPool&       getRequestPool();
    TPSNDAPI TpaRequestPool const& getRequestPool() const;

private:
    void handlePlayerExecuteTpaCommand(class PlayerExecuteTpaCommandEvent& ev);
    void handleAcceptOrDenyTpaRequest(Player& receiver, bool accept);
    void handleCancelTpaRequest(Player& sender);
};


} // namespace ltps::tpa