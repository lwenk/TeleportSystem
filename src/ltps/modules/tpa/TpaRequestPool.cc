#include "ltps/modules/tpa/TpaRequestPool.h"
#include "ltps/common/TimeScheduler.h"
#include "ltps/modules/tpa/TpaRequest.h"
#include "ltps/modules/tpa/event/TpaEvents.h"
#include "ltps/utils/McUtils.h"

#include "ll/api/coro/CoroTask.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/ListenerBase.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "ll/api/thread/ServerThreadExecutor.h"


#include "mc/platform/UUID.h"
#include "mc/world/actor/player/Player.h"

#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <utility>


namespace ltps::tpa {


struct Compare {
    bool operator()(std::shared_ptr<TpaRequest> const& lhs, std::shared_ptr<TpaRequest> const& rhs) const {
        return lhs->getExpireTime() > rhs->getExpireTime();
    }
};

struct TpaRequestPool::Impl {
    TimeScheduler<TpaRequest, Compare> mRequestScheduler;

    using RequestQueryMap = std::unordered_map<mce::UUID, std::unordered_map<mce::UUID, std::shared_ptr<TpaRequest>>>;
    RequestQueryMap mForwardMap; // Receiver -> [Sender] -> Request
    RequestQueryMap mReverseMap; // Sender -> [Receiver] -> Request

    ll::event::ListenerPtr mPLayerDisconnectListener;
    ll::event::ListenerPtr mRequestAcceptedListener;
    ll::event::ListenerPtr mRequestDeniedListener;
    ll::event::ListenerPtr mRequestCancelledListener;
    ll::event::ListenerPtr mRequestExpiredListener;

    mutable std::shared_mutex mMutex;

    void addRequestImpl(std::shared_ptr<TpaRequest> const& request) {
        auto& sender   = request->getSenderUUID();
        auto& receiver = request->getReceiverUUID();
        {
            std::unique_lock lock{mMutex};
            mRequestScheduler.add(request);
            mForwardMap[receiver][sender] = request;
            mReverseMap[sender][receiver] = request;
        }
    }

    bool hasRequestImpl(mce::UUID const& sender, mce::UUID const& receiver) {
        std::shared_lock lock{mMutex};
        auto             iter = mForwardMap.find(receiver);
        if (iter == mForwardMap.end()) {
            return false;
        }
        if (!iter->second.contains(sender)) {
            return false;
        }
        return true;
    }

    void removeRequestImpl(mce::UUID const& sender, mce::UUID const& receiver) {
        std::unique_lock lock{mMutex};
        if (auto iter = mForwardMap.find(receiver); iter != mForwardMap.end()) {
            iter->second.erase(sender); // delete mForwardMap[receiver][sender]
            if (iter->second.empty()) {
                mForwardMap.erase(iter); // delete mForwardMap[receiver]
            }
        }
        if (auto iter = mReverseMap.find(sender); iter != mReverseMap.end()) {
            iter->second.erase(receiver); // delete mReverseMap[sender][receiver]
            if (iter->second.empty()) {
                mReverseMap.erase(iter); // delete mReverseMap[sender]
            }
        }
    }

    void removeRequestImpl(std::shared_ptr<TpaRequest> const& request) {
        removeRequestImpl(request->getSenderUUID(), request->getReceiverUUID());
    }

    std::shared_ptr<TpaRequest> getRequestImpl(mce::UUID const& sender, mce::UUID const& receiver) {
        std::shared_lock lock{mMutex};
        if (auto iter = mForwardMap.find(receiver); iter != mForwardMap.end()) {
            if (auto iter2 = iter->second.find(sender); iter2 != iter->second.end()) {
                return iter2->second;
            }
        }
        return nullptr;
    }


    void
    markRequestAndRemove(Player& player, std::function<void(std::shared_ptr<TpaRequest> const& req)> const& callback) {
        std::unique_lock lock{mMutex};

        auto uuid = player.getUuid();

        // 标记所有发送给该玩家的请求为离线 (Receiver)
        if (auto iter = mForwardMap.find(uuid); iter != mForwardMap.end()) {
            for (auto& [sender, request] : iter->second) {
                callback(request);
            }
            mForwardMap.erase(iter);
        }

        // 标记所有该玩家发送的请求为离线 (Sender)
        if (auto iter = mReverseMap.find(uuid); iter != mReverseMap.end()) {
            for (auto& [receiver, request] : iter->second) {
                callback(request);
            }
            mReverseMap.erase(iter);
        }
    }

    void markRequestOffline(Player& player) {
        markRequestAndRemove(player, [uuid = player.getUuid()](std::shared_ptr<TpaRequest> const& req) {
            if (uuid == req->getSenderUUID()) {
                req->tryUpdateState(TpaRequest::State::SenderOffline);
                if (req->isReceiverOnline()) {
                    req->notifySenderOffline();
                }
            } else {
                req->tryUpdateState(TpaRequest::State::ReceiverOffline);
                if (req->isSenderOnline()) {
                    req->notifyReceiverOffline();
                }
            }
        });
    }

    explicit Impl() {
        mRequestScheduler.setExpireCallback([](std::shared_ptr<TpaRequest> const& req) {
            if (req->isFinalState() && req->getState() != TpaRequest::State::Expired) {
                return; // 请求已经处理过，不再处理
            }
            req->tryUpdateState(TpaRequest::State::Expired);
            ll::coro::keepThis([req]() -> ll::coro::CoroTask<> {
                ll::event::EventBus::getInstance().publish(TpaRequestExpiredEvent{req});
                co_return;
            }).launch(ll::thread::ServerThreadExecutor::getDefault());
        });

        auto& bus = ll::event::EventBus::getInstance();
        mPLayerDisconnectListener =
            bus.emplaceListener<ll::event::PlayerDisconnectEvent>([this](ll::event::PlayerDisconnectEvent& ev) {
                auto& player = ev.self();
                markRequestOffline(player); // 仅标记离线和删除查询表，调度器懒清理
            });

        mRequestAcceptedListener  = bus.emplaceListener<TpaRequestAcceptedEvent>([this](TpaRequestAcceptedEvent& ev) {
            this->removeRequestImpl(ev.getRequest());
        });
        mRequestDeniedListener    = bus.emplaceListener<TpaRequestDeniedEvent>([this](TpaRequestDeniedEvent& ev) {
            this->removeRequestImpl(ev.getRequest());
        });
        mRequestCancelledListener = bus.emplaceListener<TpaRequestCancelledEvent>([this](TpaRequestCancelledEvent& ev) {
            this->removeRequestImpl(ev.getRequest());
        });
        mRequestExpiredListener   = bus.emplaceListener<TpaRequestExpiredEvent>([this](TpaRequestExpiredEvent& ev) {
            this->removeRequestImpl(ev.getRequest());
        });

        mRequestScheduler.start();
    }

    ~Impl() {
        auto& bus = ll::event::EventBus::getInstance();
        bus.removeListener(mPLayerDisconnectListener);
        bus.removeListener(mRequestAcceptedListener);
        bus.removeListener(mRequestDeniedListener);
        bus.removeListener(mRequestCancelledListener);
        bus.removeListener(mRequestExpiredListener);
    }
};


TpaRequestPool::TpaRequestPool() : mImpl(std::make_unique<Impl>()) {}
TpaRequestPool::~TpaRequestPool() = default;


std::shared_ptr<TpaRequest> TpaRequestPool::createRequest(Player& sender, Player& receiver, TpaRequest::Type type) {
    auto req = std::make_shared<TpaRequest>(sender, receiver, type);
    mImpl->addRequestImpl(req);
    return req;
}

bool TpaRequestPool::hasRequest(mce::UUID const& sender, mce::UUID const& receiver) {
    return mImpl->hasRequestImpl(sender, receiver);
}

bool TpaRequestPool::hasRequest(Player& sender, Player& receiver) {
    return hasRequest(sender.getUuid(), receiver.getUuid());
}

bool TpaRequestPool::addRequest(std::shared_ptr<TpaRequest> const& request) {
    mImpl->addRequestImpl(request);
    return true;
}

std::shared_ptr<TpaRequest> TpaRequestPool::getRequest(mce::UUID const& sender, mce::UUID const& receiver) {
    return mImpl->getRequestImpl(sender, receiver);
}

std::vector<mce::UUID> TpaRequestPool::getSenders(mce::UUID const& receiver) {
    std::shared_lock lock(mImpl->mMutex);

    auto iter = mImpl->mForwardMap.find(receiver);
    if (iter == mImpl->mForwardMap.end()) {
        return {};
    }

    std::vector<mce::UUID> senders;
    senders.reserve(iter->second.size());
    for (auto& [sender, req] : iter->second) {
        senders.push_back(sender);
    }
    return senders;
}

std::vector<std::shared_ptr<TpaRequest>> TpaRequestPool::getInitiatedRequest(mce::UUID const& sender) {
    std::shared_lock lock(mImpl->mMutex);

    auto iter = mImpl->mReverseMap.find(sender);
    if (iter == mImpl->mReverseMap.end()) {
        return {};
    }

    std::vector<std::shared_ptr<TpaRequest>> requests;
    requests.reserve(iter->second.size());
    for (auto& [receiver, req] : iter->second) {
        requests.push_back(req);
    }
    return requests;
}

std::vector<std::shared_ptr<TpaRequest>> TpaRequestPool::getInitiatedRequest(Player& sender) {
    return getInitiatedRequest(sender.getUuid());
}


} // namespace ltps::tpa