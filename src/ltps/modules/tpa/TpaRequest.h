#pragma once
#include "ltps/Global.h"
#include "mc/platform/UUID.h"
#include <chrono>
#include <memory>


class Player;

namespace ltps::tpa {


class TpaRequest final : public std::enable_shared_from_this<TpaRequest> {
public:
    enum class Type {
        To   = 0, // Sender -> Receiver
        Here = 1  // Receiver -> Sender
    };

    enum class State {
        Available,       // 请求有效
        Accepted,        // 请求已接受
        Denied,          // 请求已拒绝
        SenderOffline,   // 发起者离线
        ReceiverOffline, // 接收者离线
        Expired,         // 请求已过期
        Cancelled,       // 请求已取消
    };

    using SystemTime = std::chrono::system_clock::time_point;
    using SteadyTime = std::chrono::steady_clock::time_point;

    TPS_DISALLOW_COPY_AND_MOVE(TpaRequest);

    TPSAPI explicit TpaRequest(Player& sender, Player& receiver, Type type);

    TPSAPI ~TpaRequest();

    TPSNDAPI Player* getSender() const;

    TPSNDAPI Player* getReceiver() const;

    TPSNDAPI mce::UUID const& getSenderUUID() const;
    TPSNDAPI mce::UUID const& getReceiverUUID() const;

    TPSNDAPI Type getType() const;

    TPSNDAPI State getState() const;

    // 获取请求的创建时间
    TPSNDAPI SystemTime const& getCreationTime() const;

    // 获取请求剩余有效时间（单位：秒）
    TPSNDAPI std::chrono::seconds getRemainingTime() const;

    // 获取请求失效时间 (yyyy-mm-dd hh:mm:ss)
    TPSNDAPI std::string getExpirationTime() const;

    TPSNDAPI SteadyTime const& getExpireTime() const;

    TPSAPI bool tryUpdateState(State state);

    TPSAPI bool isFinalState() const;

    TPSNDAPI bool isExpired() const;

    TPSNDAPI bool isAvailable() const; // 注意: 调用前请先调用 refreshAvailability() 刷新状态

    TPSNDAPI bool isSenderOnline() const;

    TPSNDAPI bool isReceiverOnline() const;

    TPSNDAPI bool isSenderAndReceiverOnline() const;

    TPSAPI void refreshAvailability();

    TPSAPI void accept();

    TPSAPI void deny();

    TPSAPI void cancel();

    TPSAPI void sendFormToReceiver();

    TPSAPI void notifyAccepted() const;

    TPSAPI void notifyDenied() const;

    TPSAPI void notifyCancelled() const;

    TPSAPI void notifyExpired() const;

    TPSAPI void notifySenderOffline() const;

    TPSAPI void notifyReceiverOffline() const;

    TPSAPI void _notifyState(Player* player) const;

    TPSNDAPI static std::string getStateDescription(State state, std::string const& localeCode = "zh_CN");
    TPSNDAPI static std::string getTypeString(Type type);

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};


} // namespace ltps::tpa