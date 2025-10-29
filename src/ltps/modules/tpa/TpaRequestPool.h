#pragma once
#include "TpaRequest.h"
#include "ltps/Global.h"
#include "mc/platform/UUID.h"
#include <memory>
#include <vector>


namespace ltps::tpa {

class TpaRequestPool {
    struct Impl;
    std::unique_ptr<Impl> mImpl;

public:
    TPS_DISALLOW_COPY_AND_MOVE(TpaRequestPool)

    TPSAPI explicit TpaRequestPool();
    TPSAPI virtual ~TpaRequestPool();

public:
    TPSNDAPI std::shared_ptr<TpaRequest> createRequest(Player& sender, Player& receiver, TpaRequest::Type type);

    TPSNDAPI bool hasRequest(mce::UUID const& sender, mce::UUID const& receiver);
    TPSNDAPI bool hasRequest(Player& sender, Player& receiver);

    TPSAPI bool addRequest(std::shared_ptr<TpaRequest> const& request);

    TPSNDAPI std::shared_ptr<TpaRequest> getRequest(mce::UUID const& sender, mce::UUID const& receiver);

    TPSNDAPI std::vector<mce::UUID> getSenders(mce::UUID const& receiver);

    TPSNDAPI std::vector<std::shared_ptr<TpaRequest>> getInitiatedRequest(mce::UUID const& sender);
    TPSNDAPI std::vector<std::shared_ptr<TpaRequest>> getInitiatedRequest(Player& sender);
};


} // namespace ltps::tpa