#include "ltps/modules/tpa/TpaRequestPool.h"
#include "ll/api/coro/CoroTask.h"
#include "ltps/modules/tpa/TpaRequest.h"
#include "ltps/utils/McUtils.h"
#include "mc/world/actor/player/Player.h"
#include <chrono>


namespace ltps::tpa {

TpaRequestPool::TpaRequestPool(ll::thread::ThreadPoolExecutor& executor) {
    mInterruptableSleep = std::make_shared<ll::coro::InterruptableSleep>();
    mPollingAbortFlag   = std::make_shared<std::atomic_bool>(false);

    ll::coro::keepThis([this, abortFlag = mPollingAbortFlag, sleep = mInterruptableSleep]() -> ll::coro::CoroTask<> {
        while (!abortFlag->load()) {
            co_await sleep->sleepFor(std::chrono::seconds(2));
            if (!abortFlag->load()) {
                cleanupExpiredRequests();
            }
        }
        co_return;
    }).launch(executor);
}
TpaRequestPool::~TpaRequestPool() {
    mPollingAbortFlag->store(true);
    mInterruptableSleep->interrupt(true);
}


std::shared_ptr<TpaRequest> TpaRequestPool::createRequest(Player& sender, Player& receiver, TpaRequest::Type type) {
    auto req = std::make_shared<TpaRequest>(sender, receiver, type);
    addRequest(req);
    return req;
}

bool TpaRequestPool::hasRequest(mce::UUID const& sender, mce::UUID const& receiver) {
    auto iter = mPool.find(receiver);
    if (iter != mPool.end()) {
        if (iter->second.contains(sender)) {
            return true;
        }
    }
    return false;
}
bool TpaRequestPool::hasRequest(Player& sender, Player& receiver) {
    return hasRequest(sender.getUuid(), receiver.getUuid());
}

bool TpaRequestPool::addRequest(std::shared_ptr<TpaRequest> const& request) {
    if (request) {
        auto sender   = request->getSender();
        auto receiver = request->getReceiver();
        if (sender && receiver) {
            mPool[receiver->getUuid()][sender->getUuid()] = request; // Receiver -> Sender -> Request
            return true;
        }
    }
    return false;
}

bool TpaRequestPool::removeRequest(mce::UUID const& sender, mce::UUID const& receiver) {
    auto iter = mPool.find(receiver);
    if (iter != mPool.end()) {
        if (iter->second.contains(sender)) {
            iter->second.erase(sender);
            return true;
        }
    }
    return false;
}

std::shared_ptr<TpaRequest> TpaRequestPool::getRequest(mce::UUID const& sender, mce::UUID const& receiver) {
    auto iter = mPool.find(receiver);
    if (iter != mPool.end()) {
        if (iter->second.contains(sender)) {
            return iter->second[sender];
        }
    }
    return nullptr;
}

std::vector<mce::UUID> TpaRequestPool::getSenders(mce::UUID const& receiver) {
    auto iter = mPool.find(receiver);
    if (iter != mPool.end()) {
        std::vector<mce::UUID> senders;
        senders.reserve(iter->second.size());
        for (auto& [sender, req] : iter->second) {
            senders.push_back(sender);
        }
        return senders;
    }
    return {};
}

void TpaRequestPool::cleanupExpiredRequests() {
    if (mPool.empty()) {
        return;
    }

    for (auto receiverIter = mPool.begin(); receiverIter != mPool.end();) {
        auto& [receiver, senderMap] = *receiverIter;
        if (senderMap.empty()) {
            receiverIter = mPool.erase(receiverIter);
            continue;
        }

        for (auto senderIter = senderMap.begin(); senderIter != senderMap.end();) {
            auto& [sender, request] = *senderIter;

            request->forceUpdateState();

            auto state          = request->getState();
            auto senderPlayer   = request->getSender();
            auto receiverPlayer = request->getReceiver();
            switch (state) {
            case TpaRequest::State::Available:
                break;
            case TpaRequest::State::SenderOffline: {
                if (receiverPlayer) {
                    mc_utils::sendText<mc_utils::Error>(*receiverPlayer, TpaRequest::getStateDescription(state));
                }
                senderIter = senderMap.erase(senderIter);
                break;
            }
            case TpaRequest::State::ReceiverOffline: {
                if (senderPlayer) {
                    mc_utils::sendText<mc_utils::Error>(*senderPlayer, TpaRequest::getStateDescription(state));
                }
                senderIter = senderMap.erase(senderIter);
                break;
            }
            case TpaRequest::State::Expired: {
                if (senderPlayer && receiverPlayer) {
                    mc_utils::sendText<mc_utils::Error>(*senderPlayer, TpaRequest::getStateDescription(state));
                    mc_utils::sendText<mc_utils::Error>(*receiverPlayer, TpaRequest::getStateDescription(state));
                }
                senderIter = senderMap.erase(senderIter);
                break;
            }
            default:
                senderIter = senderMap.erase(senderIter);
                break;
            }

            ++senderIter;
        }

        ++receiverIter;
    }
}


} // namespace ltps::tpa