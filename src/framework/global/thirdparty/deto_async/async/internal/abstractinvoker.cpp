#include "abstractinvoker.h"

#include <cassert>

#include "queuedinvoker.h"

using namespace deto::async;

AbstractInvoker::AbstractInvoker()
{
}

AbstractInvoker::~AbstractInvoker()
{
}

void AbstractInvoker::invoke(int type)
{
    invoke(type, NotifyData());
}

void AbstractInvoker::invoke(int type, const NotifyData& data)
{
    CallBacks callbacks;
    {
        std::lock_guard<std::mutex> lock(m_callbacksMutex);
        auto it = m_callbacks.find(type);
        if (it == m_callbacks.end()) {
            return;
        }

        // Take a snapshot while the registry is locked. A callback may be
        // removed by its receiving thread while another thread sends.
        callbacks = it->second;
    }

    std::thread::id threadID = std::this_thread::get_id();

    for (const CallBack& c : callbacks) {
        if (c.threadID == threadID) {
            invokeCallback(type, c, data);
        } else {
            // A queued callback takes a temporary owner only when it runs.
            // If its invoker or receiver has gone away first, it becomes a
            // no-op instead of following a stale raw pointer.
            auto qi = std::make_shared<QInvoker>(shared_from_this(), type, c, data);
            QueuedInvoker::instance()->invoke(c.threadID, [qi]() {
                qi->invoke();
            });
        }
    }
}

void AbstractInvoker::invokeCallback(int type, const CallBack& c, const NotifyData& data)
{
    assert(c.threadID == std::this_thread::get_id());

    // A receiver can replace a callback before an older cross-thread
    // delivery reaches its target. Check the exact registration, rather
    // than only the receiver, so the old call object is never invoked.
    if (!containsCallBack(type, c)) {
        return;
    }

    if (c.receiver && !c.receiver->isConnectedAsync()) {
        return;
    }

    doInvoke(type, c.call, data);
}

void AbstractInvoker::processEvents()
{
    QueuedInvoker::instance()->processEvents();
}

void AbstractInvoker::onMainThreadInvoke(const std::function<void(const std::function<void()>&, bool)>& f)
{
    QueuedInvoker::instance()->onMainThreadInvoke(f);
}

bool AbstractInvoker::isConnected() const
{
    std::lock_guard<std::mutex> lock(m_callbacksMutex);
    for (auto it = m_callbacks.cbegin(); it != m_callbacks.cend(); ++it) {
        const CallBacks& cs = it->second;
        if (cs.size() > 0) {
            return true;
        }
    }
    return false;
}

int AbstractInvoker::CallBacks::receiverIndexOf(Asyncable* receiver) const
{
    for (size_t i = 0; i < size(); ++i) {
        if (at(i).receiver == receiver) {
            return int(i);
        }
    }
    return -1;
}

bool AbstractInvoker::CallBacks::containsReceiver(Asyncable* receiver) const
{
    return receiverIndexOf(receiver) > -1;
}

bool AbstractInvoker::CallBacks::containsCallBack(const CallBack& callback) const
{
    for (const CallBack& registered : *this) {
        if (registered.type == callback.type
            && registered.receiver == callback.receiver
            && registered.call == callback.call) {
            return true;
        }
    }
    return false;
}

void AbstractInvoker::removeCallBack(int type, Asyncable* receiver)
{
    CallBack c;
    {
        std::lock_guard<std::mutex> lock(m_callbacksMutex);
        auto it = m_callbacks.find(type);
        if (it == m_callbacks.end()) {
            return;
        }

        CallBacks& callbacks = it->second;
        int index = callbacks.receiverIndexOf(receiver);
        if (index < 0) {
            return;
        }

        c = callbacks.at(index);
        callbacks.erase(callbacks.begin() + index);
    }

    if (c.receiver) {
        c.receiver->disconnectAsync(this);
    }
}

void AbstractInvoker::removeAllCallBacks()
{
    std::vector<CallBack> callbacks;
    {
        std::lock_guard<std::mutex> lock(m_callbacksMutex);
        for (auto it = m_callbacks.begin(); it != m_callbacks.end(); ++it) {
            for (const CallBack& c : it->second) {
                callbacks.push_back(c);
            }
        }
        m_callbacks.clear();
    }

    for (const CallBack& c : callbacks) {
        if (c.receiver) {
            c.receiver->disconnectAsync(this);
        }
    }
}

void AbstractInvoker::addCallBack(int type, Asyncable* receiver, std::shared_ptr<void> call,
                                  Asyncable::AsyncMode mode)
{
    bool receiverAlreadyRegistered = false;
    {
        std::lock_guard<std::mutex> lock(m_callbacksMutex);
        auto it = m_callbacks.find(type);
        receiverAlreadyRegistered = it != m_callbacks.end()
            && it->second.containsReceiver(receiver);
    }

    if (receiverAlreadyRegistered) {
        switch (mode) {
        case Asyncable::AsyncMode::AsyncSetOnce:
            return;
        case Asyncable::AsyncMode::AsyncSetRepeat:
            removeCallBack(type, receiver);
            break;
        }
    }

    CallBack c(std::this_thread::get_id(), type, receiver, call);
    {
        std::lock_guard<std::mutex> lock(m_callbacksMutex);
        m_callbacks[type].push_back(c);
    }

    if (c.receiver) {
        c.receiver->connectAsync(this);
    }
}

void AbstractInvoker::disconnectAsync(Asyncable* receiver)
{
    std::vector<int> types;
    {
        std::lock_guard<std::mutex> lock(m_callbacksMutex);
        for (auto it = m_callbacks.begin(); it != m_callbacks.end(); ++it) {
            for (const CallBack& c : it->second) {
                if (c.receiver == receiver) {
                    types.push_back(c.type);
                }
            }
        }
    }

    for (int type : types) {
        removeCallBack(type, receiver);
    }
}

bool AbstractInvoker::containsCallBack(int type, const CallBack& callback) const
{
    std::lock_guard<std::mutex> lock(m_callbacksMutex);
    auto it = m_callbacks.find(type);
    return it != m_callbacks.end() && it->second.containsCallBack(callback);
}
