#include "miniraster/thread_pool.hpp"
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition,const std::string& message) { if (!condition) throw std::runtime_error(message); }
}
int main() {
    try {
        bool rejected=false; try { mr::ThreadPool invalid(0); } catch (const std::invalid_argument&) { rejected=true; }
        require(rejected,"zero worker pool accepted");
        mr::ThreadPool pool(4); require(pool.size()==4,"worker count mismatch"); pool.wait();
        rejected=false; try { pool.enqueue({}); } catch (const std::invalid_argument&) { rejected=true; }
        require(rejected,"empty function accepted");
        std::atomic<int> sum{0};
        for (int i=0;i<1000;++i) pool.enqueue([&sum] { sum.fetch_add(1,std::memory_order_relaxed); });
        pool.wait(); require(sum==1000,"tasks missing after wait");
        pool.enqueue([] { throw std::runtime_error("expected worker failure"); });
        for (int i=0;i<50;++i) pool.enqueue([&sum] { sum.fetch_add(1,std::memory_order_relaxed); });
        rejected=false; try { pool.wait(); } catch (const std::runtime_error& e) { rejected=std::string(e.what())=="expected worker failure"; }
        require(rejected&&sum==1050,"worker exception not propagated after draining work");
        pool.wait(); // Exception state is consumed; pool remains usable.
        pool.enqueue([&sum] { sum.fetch_add(1,std::memory_order_relaxed); }); pool.wait(); require(sum==1051,"pool unusable after task exception");
        std::atomic<int> drained{0};
        { mr::ThreadPool temporary(2); for (int i=0;i<100;++i) temporary.enqueue([&drained] { ++drained; }); }
        require(drained==100,"destructor did not drain queued work");
        std::cout << "thread pool tests passed (empty wait, completion, exception, reuse, destructor drain)\n"; return 0;
    } catch (const std::exception& error) { std::cerr << "thread pool test failure: " << error.what() << '\n'; return 1; }
}
