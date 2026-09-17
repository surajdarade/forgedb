#include "forgedb/storage/write_queue.h"
#include <stdexcept>

namespace forgedb {
WriteQueue::WriteQueue(std::size_t workerCount) {
    if(workerCount==0) throw std::invalid_argument("WriteQueue: worker count must be positive");
    for(std::size_t i=0;i<workerCount;++i) workers_.emplace_back(&WriteQueue::workerLoop,this);
}
WriteQueue::~WriteQueue(){ shutdown(); }
void WriteQueue::enqueue(std::function<void()> operation){ if(!operation) return; { std::lock_guard lock(mutex_); if(stopping_) throw std::runtime_error("WriteQueue: queue is stopped"); queue_.push(std::move(operation)); } condition_.notify_one(); }
void WriteQueue::drain(){ std::unique_lock lock(mutex_); drained_.wait(lock,[&]{return queue_.empty()&&active_==0;}); }
void WriteQueue::shutdown(){ { std::lock_guard lock(mutex_); if(stopping_) return; stopping_=true; } condition_.notify_all(); for(auto& t:workers_) if(t.joinable()) t.join(); workers_.clear(); }
void WriteQueue::workerLoop(){ for(;;){ std::function<void()> op; { std::unique_lock lock(mutex_); condition_.wait(lock,[&]{return stopping_||!queue_.empty();}); if(queue_.empty()&&stopping_) return; op=std::move(queue_.front()); queue_.pop(); ++active_; } try{op();}catch(...){ } { std::lock_guard lock(mutex_); --active_; if(queue_.empty()&&active_==0) drained_.notify_all(); } } }
} // namespace forgedb
