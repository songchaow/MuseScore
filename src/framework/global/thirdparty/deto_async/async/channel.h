#ifndef DETO_ASYNC_CHANNEL_H
#define DETO_ASYNC_CHANNEL_H

#include <memory>
#include "internal/abstractinvoker.h"

namespace deto {
namespace async {
template<typename ... T>
class Channel
{
public:
    Channel() = default;
    Channel(const Channel& ch)
        : m_ptr(ch.ptr()) {}
    ~Channel() {}

    Channel& operator=(const Channel& ch)
    {
        if (m_ptr == ch.ptr()) {
            return *this;
        }

        m_ptr = ch.ptr();
        return *this;
    }

    void send(const T&... d)
    {
        NotifyData nd;
        nd.setArg<T...>(0, d ...);
        ptr()->invoke(Receive, nd);
    }

    template<typename Func>
    void onReceive(const Asyncable* receiver, Func f, Asyncable::AsyncMode mode = Asyncable::AsyncMode::AsyncSetOnce)
    {
        ptr()->addCallBack(Receive, const_cast<Asyncable*>(receiver),
                            std::make_shared<ReceiveCall<Func, T...> >(f), mode);
    }

    void resetOnReceive(const Asyncable* receiver)
    {
        ptr()->removeCallBack(Receive, const_cast<Asyncable*>(receiver));
    }

    void close()
    {
        ptr()->invoke(Close);
    }

    template<typename Func>
    void onClose(const Asyncable* receiver, Func f, Asyncable::AsyncMode mode = Asyncable::AsyncMode::AsyncSetOnce)
    {
        ptr()->addCallBack(Close, const_cast<Asyncable*>(receiver),
                            std::make_shared<CloseCall<Func> >(f), mode);
    }

    bool isConnected() const
    {
        return m_ptr && ptr()->isConnected();
    }

private:

    enum CallType {
        Undefined = 0,
        Receive,
        Close
    };

    struct IReceive {
        virtual ~IReceive() {}
        virtual void received(const NotifyData& d) = 0;
    };

    template<typename Call, typename ... Arg>
    struct ReceiveCall : public IReceive {
        Call f;
        ReceiveCall(Call _f)
            : f(_f) {}
        void received(const NotifyData& d) { std::apply(f, d.args<Arg...>()); }
    };

    struct IClose {
        virtual ~IClose() {}
        virtual void closed() = 0;
    };

    template<typename Call>
    struct CloseCall : public IClose {
        Call f;
        CloseCall(Call _f)
            : f(_f) {}
        void closed() { f(); }
    };

    struct ChannelInvoker : public AbstractInvoker
    {
        friend class Channel;

        ChannelInvoker() = default;
        ~ChannelInvoker()
        {
            removeAllCallBacks();
        }

        void doInvoke(int callKey, const std::shared_ptr<void>& call, const NotifyData& d) override
        {
            CallType type = static_cast<CallType>(callKey);
            switch (type) {
            case Undefined:  break;
            case Receive:
                static_cast<IReceive*>(call.get())->received(d);
                break;
            case Close:
                static_cast<IClose*>(call.get())->closed();
                break;
            }
        }
    };

    std::shared_ptr<ChannelInvoker> ptr() const
    {
        if (!m_ptr) {
            m_ptr = std::make_shared<ChannelInvoker>();
        }
        return m_ptr;
    }

    mutable std::shared_ptr<ChannelInvoker> m_ptr = nullptr;
};
}
}

#endif // DETO_ASYNC_CHANNEL_H
