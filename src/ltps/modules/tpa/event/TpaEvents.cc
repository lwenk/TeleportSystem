#include "ltps/modules/tpa/event/TpaEvents.h"
#include "ll/api/event/Emitter.h"
#include "ltps/Global.h"
#include <utility>


namespace ltps::tpa {


// ICreateTpaRequestEvent
ICreateTpaRequestEvent::ICreateTpaRequestEvent(Player& sender, Player& receiver, TpaRequest::Type type)
: mSender(sender),
  mReceiver(receiver),
  mType(type) {}

Player& ICreateTpaRequestEvent::getSender() const { return mSender; }

Player& ICreateTpaRequestEvent::getReceiver() const { return mReceiver; }

TpaRequest::Type ICreateTpaRequestEvent::getType() const { return mType; }


// CreateTpaRequestEvent
CreateTpaRequestEvent::CreateTpaRequestEvent(Player& sender, Player& receiver, TpaRequest::Type type, Callback callback)
: ICreateTpaRequestEvent(sender, receiver, type),
  mCallback(std::move(callback)) {}

void CreateTpaRequestEvent::invokeCallback(std::shared_ptr<TpaRequest> request) const {
    if (mCallback) {
        mCallback(std::move(request));
    }
}


// CreatingTpaRequestEvent
CreatingTpaRequestEvent::CreatingTpaRequestEvent(CreateTpaRequestEvent const& event)
: ICreateTpaRequestEvent(event.getSender(), event.getReceiver(), event.getType()) {}

CreatingTpaRequestEvent::CreatingTpaRequestEvent(Player& sender, Player& receiver, TpaRequest::Type type)
: ICreateTpaRequestEvent(sender, receiver, type) {}


// CreatedTpaRequestEvent
CreatedTpaRequestEvent::CreatedTpaRequestEvent(std::shared_ptr<TpaRequest> request) : mRequest(std::move(request)) {}

std::shared_ptr<TpaRequest> CreatedTpaRequestEvent::getRequest() const { return mRequest; }


// IAcceptOrDenyTpaRequestEvent
IOperationTpaRequestEvent::IOperationTpaRequestEvent(std::shared_ptr<TpaRequest> const& request) : mRequest(request) {}

std::shared_ptr<TpaRequest> const& IOperationTpaRequestEvent::getRequest() const { return mRequest; }

// TpaRequestAcceptingEvent
TpaRequestAcceptingEvent ::TpaRequestAcceptingEvent(std::shared_ptr<TpaRequest> const& request)
: IOperationTpaRequestEvent(request) {}

// TpaRequestAcceptedEvent
TpaRequestAcceptedEvent ::TpaRequestAcceptedEvent(std::shared_ptr<TpaRequest> const& request)
: IOperationTpaRequestEvent(request) {}

// TpaRequestDenyingEvent
TpaRequestDenyingEvent::TpaRequestDenyingEvent(std::shared_ptr<TpaRequest> const& request)
: IOperationTpaRequestEvent(request) {}

// TpaRequestDeniedEvent
TpaRequestDeniedEvent::TpaRequestDeniedEvent(std::shared_ptr<TpaRequest> const& request)
: IOperationTpaRequestEvent(request) {}

TpaRequestCancelledEvent::TpaRequestCancelledEvent(std::shared_ptr<TpaRequest> const& request)
: IOperationTpaRequestEvent(request) {}

TpaRequestExpiredEvent::TpaRequestExpiredEvent(std::shared_ptr<TpaRequest> const& request)
: IOperationTpaRequestEvent(request) {}

// PlayerExecuteTpaAcceptOrDenyCommandEvent
PlayerExecuteTpaCommandEvent::PlayerExecuteTpaCommandEvent(Player& player, Action action)
: mPlayer(player),
  mAction(action) {}

Player& PlayerExecuteTpaCommandEvent::getPlayer() const { return mPlayer; }

PlayerExecuteTpaCommandEvent::Action PlayerExecuteTpaCommandEvent::getAction() const { return mAction; }


IMPL_EVENT_EMITTER(CreateTpaRequestEvent);
IMPL_EVENT_EMITTER(CreatingTpaRequestEvent);
IMPL_EVENT_EMITTER(CreatedTpaRequestEvent);
IMPL_EVENT_EMITTER(TpaRequestAcceptingEvent);
IMPL_EVENT_EMITTER(TpaRequestAcceptedEvent);
IMPL_EVENT_EMITTER(TpaRequestDenyingEvent);
IMPL_EVENT_EMITTER(TpaRequestDeniedEvent);
IMPL_EVENT_EMITTER(TpaRequestCancelledEvent);
IMPL_EVENT_EMITTER(TpaRequestExpiredEvent);
IMPL_EVENT_EMITTER(PlayerExecuteTpaCommandEvent);

} // namespace ltps::tpa