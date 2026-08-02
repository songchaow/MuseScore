#ifndef DETO_ASYNC_ABSTRACTINVOKER_H
#define DETO_ASYNC_ABSTRACTINVOKER_H

#include <memory>
#include <vector>
#include <iostream>
#include <map>
#include <mutex>
#include <thread>
#include <functional>

#include "../asyncable.h"

namespace deto {
namespace async {
class NotifyData
{
public:
    NotifyData() {}

    template<typename ... T>
    void setArg(int i, const T&... val)
    {
        IArg* p = new Arg<T...>(val ...);
        m_args.insert(m_args.begin() + i, std::shared_ptr<IArg>(p));
    }

    template<typename T>
    T arg(int i = 0) const
    {
        IArg* p = m_args.at(i).get();
        if (!p) {
            return {};
        }
        Arg<T>* d = reinterpret_cast<Arg<T>*>(p);
        return std::get<0>(d->val);
    }

    template<typename ... T>
    std::tuple<T...> args(int i = 0) const
    {
        IArg* p = m_args.at(i).get();
        if (!p) {
            return {};
        }
        Arg<T...>* d = reinterpret_cast<Arg<T...>*>(p);
        return d->val;
    }

    struct IArg {
        virtual ~IArg() = default;
    };

    template<typename ... T>
    struct Arg : public IArg {
        std::tuple<T...> val;
        Arg(const T&... v)
            : IArg(), val(v ...) {}
    };

private:
    std::vector<std::shared_ptr<IArg> > m_args;
};

class QueuedInvoker;
class AbstractInvoker : public Asyncable::IConnectable,
    public std::enable_shared_from_this<AbstractInvoker>
{
public:
    void disconnectAsync(Asyncable* receiver);

    void invoke(int type);
    void invoke(int type, const NotifyData& data);

    bool isConnected() const;

    static void processEvents();
    static void onMainThreadInvoke(const std::function<void(const std::function<void()>&, bool)>& f);

protected:
    explicit AbstractInvoker();
    ~AbstractInvoker();

    virtual void doInvoke(int type, const std::shared_ptr<void>& call, const NotifyData& data) = 0;

    struct CallBack {
        std::thread::id threadID;
        int type = 0;
        Asyncable* receiver = nullptr;
        std::shared_ptr<void> call;
        CallBack() {}
        CallBack(std::thread::id threadID, int t, Asyncable* cr, std::shared_ptr<void> c)
            : threadID(threadID), type(t), receiver(cr), call(c) {}
    };

    class CallBacks : public std::vector<CallBack>
    {
    public:
        int receiverIndexOf(Asyncable* receiver) const;
        bool containsReceiver(Asyncable* receiver) const;
        bool containsCallBack(const CallBack& callback) const;
    };

    struct QInvoker
    {
        std::weak_ptr<AbstractInvoker> invoker;
        int type = -1;
        CallBack call;
        NotifyData data;

        QInvoker(std::weak_ptr<AbstractInvoker> i, int t, CallBack c, NotifyData d)
            : invoker(i), type(t), call(c), data(d) {}

        void invoke()
        {
            std::shared_ptr<AbstractInvoker> inv = invoker.lock();
            if (inv) {
                inv->invokeCallback(type, call, data);
            }
        }
    };

    void invokeCallback(int type, const CallBack& c, const NotifyData& data);

    void addCallBack(int type, Asyncable* receiver, std::shared_ptr<void> call,
                     Asyncable::AsyncMode mode = Asyncable::AsyncMode::AsyncSetRepeat);
    void removeCallBack(int type, Asyncable* receiver);
    void removeAllCallBacks();

    bool containsCallBack(int type, const CallBack& callback) const;

    mutable std::mutex m_callbacksMutex;
    std::map<int /*type*/, CallBacks > m_callbacks;
};

inline void processEvents()
{
    AbstractInvoker::processEvents();
}

inline void onMainThreadInvoke(const std::function<void(const std::function<void()>&, bool)>& f)
{
    AbstractInvoker::onMainThreadInvoke(f);
}
}
}

#endif // DETO_ASYNC_ABSTRACTINVOKER_H
