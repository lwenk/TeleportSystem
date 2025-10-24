#pragma once
#include "ll/api/event/Cancellable.h"
#include "ll/api/event/Event.h"
#include "ltps/Global.h"
#include "ltps/modules/tpa/TpaRequest.h"
#include <functional>
#include <memory>


class Player;

namespace ltps::tpa {

using ll::event::Cancellable;
using ll::event::Event;

class ICreateTpaRequestEvent {
protected:
    Player&          mSender;
    Player&          mReceiver;
    TpaRequest::Type mType;

public:
    TPSAPI explicit ICreateTpaRequestEvent(Player& sender, Player& receiver, TpaRequest::Type type);

    TPSNDAPI Player& getSender() const;

    TPSNDAPI Player& getReceiver() const;

    TPSNDAPI TpaRequest::Type getType() const;
};


/**
 * @brief 创建 TPA 请求事件
 *  流程: CreateTpaRequestEvent -> CreatingTpaRequestEvent -> TpaRequestPool::createRequest() -> CreatedTpaRequestEvent
 */
class CreateTpaRequestEvent final : public ICreateTpaRequestEvent, public Event {
    using Callback = std::function<void(std::shared_ptr<TpaRequest> request)>;
    Callback mCallback;

public:
    TPSAPI explicit CreateTpaRequestEvent(
        Player&          sender,
        Player&          receiver,
        TpaRequest::Type type,
        Callback         callback = {}
    );

    TPSAPI void invokeCallback(std::shared_ptr<TpaRequest> request) const;
};


// 正在创建 TPA 请求事件
class CreatingTpaRequestEvent final : public ICreateTpaRequestEvent, public Cancellable<Event> {
public:
    TPSAPI explicit CreatingTpaRequestEvent(CreateTpaRequestEvent const& event);
    TPSAPI explicit CreatingTpaRequestEvent(Player& sender, Player& receiver, TpaRequest::Type type);
};


// TPA 请求创建完毕事件
class CreatedTpaRequestEvent final : public Event {
    std::shared_ptr<TpaRequest> mRequest;

public:
    TPSAPI explicit CreatedTpaRequestEvent(std::shared_ptr<TpaRequest> request);

    TPSNDAPI std::shared_ptr<TpaRequest> getRequest() const;
};


class IOperationTpaRequestEvent {
protected:
    std::shared_ptr<TpaRequest> mRequest;

public:
    TPSAPI explicit IOperationTpaRequestEvent(std::shared_ptr<TpaRequest> const& request);

    TPSNDAPI std::shared_ptr<TpaRequest> const& getRequest() const;
};


// Tpa 请求正在被接受
class TpaRequestAcceptingEvent final : public IOperationTpaRequestEvent, public Cancellable<Event> {
public:
    TPSAPI explicit TpaRequestAcceptingEvent(std::shared_ptr<TpaRequest> const& request);
};

// Tpa 请求已接受
class TpaRequestAcceptedEvent final : public IOperationTpaRequestEvent, public Event {
public:
    TPSAPI explicit TpaRequestAcceptedEvent(std::shared_ptr<TpaRequest> const& request);
};

// Tpa 请求正在被拒绝
class TpaRequestDenyingEvent final : public IOperationTpaRequestEvent, public Cancellable<Event> {
public:
    TPSAPI explicit TpaRequestDenyingEvent(std::shared_ptr<TpaRequest> const& request);
};

// Tpa 请求已拒绝
class TpaRequestDeniedEvent final : public IOperationTpaRequestEvent, public Event {
public:
    TPSAPI explicit TpaRequestDeniedEvent(std::shared_ptr<TpaRequest> const& request);
};

// Tpa 请求被取消
class TpaRequestCancelledEvent final : public IOperationTpaRequestEvent, public Event {
public:
    TPSAPI explicit TpaRequestCancelledEvent(std::shared_ptr<TpaRequest> const& request);
};

// Tpa 请求超时
class TpaRequestExpiredEvent final : public IOperationTpaRequestEvent, public Event {
public:
    TPSAPI explicit TpaRequestExpiredEvent(std::shared_ptr<TpaRequest> const& request);
};

/**
 * @brief 玩家执行 TPA 命令事件
 * 流程: PlayerExecuteTpaCommandEvent -> TpaRequest::accept/deny() ->
 * TpaRequestAcceptingEvent/TpaRequestDenyingEvent -> TpaRequestAcceptedEvent/TpaRequestDeniedEvent
 */
class PlayerExecuteTpaCommandEvent final : public Event {
public:
    enum class Action { Accept, Deny, Cancel };

private:
    Player& mPlayer;
    Action  mAction;

public:
    TPSAPI explicit PlayerExecuteTpaCommandEvent(Player& player, Action action);

    TPSNDAPI Player& getPlayer() const;

    TPSNDAPI Action getAction() const;
};

} // namespace ltps::tpa