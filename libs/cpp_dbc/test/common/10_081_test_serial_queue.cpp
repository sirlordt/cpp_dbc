/*

 * Copyright 2025 Tomas R Moreno P <tomasr.morenop@gmail.com>. All Rights Reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.

 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.

 @file 10_081_test_serial_queue.cpp
 @brief Tests for SerialQueue — single-threaded async task queue

*/

#include <atomic>
#include <chrono>
#include <functional>
#include <latch>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>

using namespace std::chrono_literals;

// ============================================================================
// Helper: wait for an atomic flag with timeout
// ============================================================================
static bool waitForFlag(const std::atomic<bool> &flag, std::chrono::milliseconds timeout = 2000ms)
{
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!flag.load(std::memory_order_acquire))
    {
        if (std::chrono::steady_clock::now() >= deadline)
        {
            return false;
        }
        std::this_thread::sleep_for(1ms);
    }
    return true;
}

static bool waitForValue(const std::atomic<int> &val, int expected, std::chrono::milliseconds timeout = 2000ms)
{
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (val.load(std::memory_order_acquire) != expected)
    {
        if (std::chrono::steady_clock::now() >= deadline)
        {
            return false;
        }
        std::this_thread::sleep_for(1ms);
    }
    return true;
}

// ============================================================================
// 1. Basic construction and naming
// ============================================================================
TEST_CASE("SerialQueue basic construction", "[10_081_01_serial_queue]")
{
    SECTION("Default name is 'default'")
    {
        cpp_dbc::SerialQueue q;
        REQUIRE(q.name() == "default");
    }

    SECTION("Custom name is stored")
    {
        cpp_dbc::SerialQueue q("my-queue");
        REQUIRE(q.name() == "my-queue");
    }

    SECTION("Name is normalized to lowercase")
    {
        cpp_dbc::SerialQueue q("MY_QUEUE");
        REQUIRE(q.name() == "my_queue");
    }

    SECTION("Mixed case name is normalized")
    {
        cpp_dbc::SerialQueue q("Cleanup-Workers");
        REQUIRE(q.name() == "cleanup-workers");
    }

    SECTION("Empty queue has zero pending")
    {
        cpp_dbc::SerialQueue q("test");
        REQUIRE(q.pending() == 0);
    }
}

// ============================================================================
// 2. Task execution
// ============================================================================
TEST_CASE("SerialQueue task execution", "[10_081_02_serial_queue]")
{
    SECTION("Single task executes")
    {
        cpp_dbc::SerialQueue q("exec-single");
        std::atomic<bool> executed{false};

        q.post([&executed]()
               { executed.store(true, std::memory_order_release); });

        REQUIRE(waitForFlag(executed));
    }

    SECTION("Multiple tasks execute in FIFO order")
    {
        cpp_dbc::SerialQueue q("exec-fifo");
        std::vector<int> order;
        std::mutex orderMutex;
        std::atomic<int> count{0};

        for (int i = 0; i < 10; ++i)
        {
            q.post([&order, &orderMutex, &count, i]()
                   {
                std::scoped_lock lock(orderMutex);
                order.push_back(i);
                count.fetch_add(1, std::memory_order_release); });
        }

        REQUIRE(waitForValue(count, 10));

        std::scoped_lock lock(orderMutex);
        REQUIRE(order.size() == 10);
        for (int i = 0; i < 10; ++i)
        {
            REQUIRE(order[i] == i);
        }
    }

    SECTION("Tasks execute serially — no concurrent execution")
    {
        cpp_dbc::SerialQueue q("exec-serial");
        std::atomic<int> concurrency{0};
        std::atomic<int> maxConcurrency{0};
        std::atomic<int> completed{0};

        for (int i = 0; i < 20; ++i)
        {
            q.post([&concurrency, &maxConcurrency, &completed]()
                   {
                int current = concurrency.fetch_add(1, std::memory_order_acq_rel) + 1;

                // Track max concurrency observed
                int prevMax = maxConcurrency.load(std::memory_order_acquire);
                while (current > prevMax && !maxConcurrency.compare_exchange_weak(
                    prevMax, current, std::memory_order_acq_rel))
                {
                    // Intentionally empty — CAS retry loop
                }

                std::this_thread::sleep_for(1ms);
                concurrency.fetch_sub(1, std::memory_order_release);
                completed.fetch_add(1, std::memory_order_release); });
        }

        REQUIRE(waitForValue(completed, 20, 5000ms));
        REQUIRE(maxConcurrency.load(std::memory_order_acquire) == 1);
    }

    SECTION("Pending count reflects queued tasks")
    {
        cpp_dbc::SerialQueue q("exec-pending");
        std::latch gate(1);
        std::atomic<bool> firstStarted{false};

        // Post a blocking task so subsequent ones queue up
        q.post([&gate, &firstStarted]()
               {
            firstStarted.store(true, std::memory_order_release);
            gate.wait(); });

        // Wait until the first task is running
        REQUIRE(waitForFlag(firstStarted));

        // Post more tasks — they should queue up
        q.post([]() { /* intentionally empty — just queued */ });
        q.post([]() { /* intentionally empty — just queued */ });
        q.post([]() { /* intentionally empty — just queued */ });

        // Give a moment for the posts to be processed
        std::this_thread::sleep_for(10ms);
        REQUIRE(q.pending() >= 2);

        // Release the gate
        gate.count_down();
    }
}

// ============================================================================
// 3. Exception handling
// ============================================================================
TEST_CASE("SerialQueue exception handling", "[10_081_03_serial_queue]")
{
    SECTION("Throwing task does not kill the queue")
    {
        cpp_dbc::SerialQueue q("exc-survive");
        std::atomic<bool> afterException{false};

        // Post a task that throws
        q.post([]()
               { throw std::runtime_error("test exception"); });

        // Post a task after the throwing one — it must still execute
        q.post([&afterException]()
               { afterException.store(true, std::memory_order_release); });

        REQUIRE(waitForFlag(afterException));
    }

    SECTION("Multiple throwing tasks do not accumulate damage")
    {
        cpp_dbc::SerialQueue q("exc-multi");
        std::atomic<int> successCount{0};

        for (int i = 0; i < 10; ++i)
        {
            // Alternate between throwing and normal tasks
            if (i % 2 == 0)
            {
                q.post([]()
                       { throw std::runtime_error("boom"); });
            }
            else
            {
                q.post([&successCount]()
                       { successCount.fetch_add(1, std::memory_order_release); });
            }
        }

        REQUIRE(waitForValue(successCount, 5));
    }
}

// ============================================================================
// 4. Queue drain on destruction
// ============================================================================
TEST_CASE("SerialQueue drains on destruction", "[10_081_04_serial_queue]")
{
    SECTION("All queued tasks complete before destructor returns")
    {
        std::atomic<int> completed{0};

        {
            cpp_dbc::SerialQueue q("drain");

            for (int i = 0; i < 50; ++i)
            {
                q.post([&completed]()
                       { completed.fetch_add(1, std::memory_order_release); });
            }
            // Destructor called here — should drain all 50 tasks
        }

        REQUIRE(completed.load(std::memory_order_acquire) == 50);
    }

    SECTION("Drain works even with slow tasks")
    {
        std::atomic<int> completed{0};

        {
            cpp_dbc::SerialQueue q("drain-slow");

            for (int i = 0; i < 5; ++i)
            {
                q.post([&completed]()
                       {
                    std::this_thread::sleep_for(5ms);
                    completed.fetch_add(1, std::memory_order_release); });
            }
        }

        REQUIRE(completed.load(std::memory_order_acquire) == 5);
    }
}

// ============================================================================
// 5. Global named instances
// ============================================================================
TEST_CASE("SerialQueue global named instances", "[10_081_05_serial_queue]")
{
    SECTION("global() returns the default instance")
    {
        auto &q = cpp_dbc::SerialQueue::global();
        REQUIRE(q.name() == "default");
    }

    SECTION("global('default') returns the same instance as global()")
    {
        auto &q1 = cpp_dbc::SerialQueue::global();
        auto &q2 = cpp_dbc::SerialQueue::global("default");
        REQUIRE(&q1 == &q2);
    }

    SECTION("Named instances are distinct from default")
    {
        auto &def = cpp_dbc::SerialQueue::global();
        auto &named = cpp_dbc::SerialQueue::global("test-queue-a");
        REQUIRE(&def != &named);
        REQUIRE(named.name() == "test-queue-a");
    }

    SECTION("Same name returns same instance")
    {
        auto &q1 = cpp_dbc::SerialQueue::global("test-queue-b");
        auto &q2 = cpp_dbc::SerialQueue::global("test-queue-b");
        REQUIRE(&q1 == &q2);
    }

    SECTION("Names are case-insensitive")
    {
        auto &q1 = cpp_dbc::SerialQueue::global("My-Queue");
        auto &q2 = cpp_dbc::SerialQueue::global("my-queue");
        auto &q3 = cpp_dbc::SerialQueue::global("MY-QUEUE");
        REQUIRE(&q1 == &q2);
        REQUIRE(&q2 == &q3);
        REQUIRE(q1.name() == "my-queue");
    }

    SECTION("Different names create different instances")
    {
        auto &q1 = cpp_dbc::SerialQueue::global("alpha");
        auto &q2 = cpp_dbc::SerialQueue::global("beta");
        REQUIRE(&q1 != &q2);
    }

    SECTION("Default global queue executes tasks")
    {
        std::atomic<bool> executed{false};
        cpp_dbc::SerialQueue::global().post([&executed]()
                                            { executed.store(true, std::memory_order_release); });
        REQUIRE(waitForFlag(executed));
    }

    SECTION("Named global queue executes tasks")
    {
        std::atomic<int> count{0};
        auto &q = cpp_dbc::SerialQueue::global("global-exec-test");

        for (int i = 0; i < 10; ++i)
        {
            q.post([&count]()
                   { count.fetch_add(1, std::memory_order_release); });
        }

        REQUIRE(waitForValue(count, 10));
    }

    SECTION("Three named global queues process tasks concurrently and independently")
    {
        auto &qA = cpp_dbc::SerialQueue::global("tri-queue-a");
        auto &qB = cpp_dbc::SerialQueue::global("tri-queue-b");
        auto &qC = cpp_dbc::SerialQueue::global("tri-queue-c");

        std::atomic<int> countA{0};
        std::atomic<int> countB{0};
        std::atomic<int> countC{0};
        std::latch gate(1);
        std::atomic<int> blocked{0};

        // Block all three queues behind a gate to prove they run concurrently
        qA.post([&gate, &blocked]()
                { blocked.fetch_add(1, std::memory_order_release); gate.wait(); });
        qB.post([&gate, &blocked]()
                { blocked.fetch_add(1, std::memory_order_release); gate.wait(); });
        qC.post([&gate, &blocked]()
                { blocked.fetch_add(1, std::memory_order_release); gate.wait(); });

        // All three should be blocked — proves they have separate worker threads
        REQUIRE(waitForValue(blocked, 3));

        // Queue work behind the gate
        for (int i = 0; i < 15; ++i)
        {
            qA.post([&countA]()
                    { countA.fetch_add(1, std::memory_order_release); });
            qB.post([&countB]()
                    { countB.fetch_add(1, std::memory_order_release); });
            qC.post([&countC]()
                    { countC.fetch_add(1, std::memory_order_release); });
        }

        // Release the gate — all three queues drain their tasks
        gate.count_down();

        REQUIRE(waitForValue(countA, 15));
        REQUIRE(waitForValue(countB, 15));
        REQUIRE(waitForValue(countC, 15));
    }

    SECTION("Global queue survives exceptions and keeps processing")
    {
        auto &q = cpp_dbc::SerialQueue::global("global-exc-survive");
        std::atomic<int> successCount{0};

        q.post([]()
               { throw std::runtime_error("global boom"); });

        for (int i = 0; i < 5; ++i)
        {
            q.post([&successCount]()
                   { successCount.fetch_add(1, std::memory_order_release); });
        }

        REQUIRE(waitForValue(successCount, 5));
    }

    SECTION("Reentrant posting on global queue — task posts to its own global queue")
    {
        auto &q = cpp_dbc::SerialQueue::global("global-reentrant");
        std::atomic<int> executed{0};

        q.post([&executed]()
               {
            executed.fetch_add(1, std::memory_order_release);
            cpp_dbc::SerialQueue::global("global-reentrant").post([&executed]()
                { executed.fetch_add(1, std::memory_order_release); }); });

        REQUIRE(waitForValue(executed, 2));
    }

    SECTION("Multiple threads posting to the same global queue")
    {
        auto &q = cpp_dbc::SerialQueue::global("global-mt-post");
        std::atomic<int> total{0};
        constexpr int threadsCount = 4;
        constexpr int tasksPerThread = 25;

        std::vector<std::jthread> posters;
        posters.reserve(threadsCount);

        for (int t = 0; t < threadsCount; ++t)
        {
            posters.emplace_back([&q, &total]()
                                 {
                for (int i = 0; i < tasksPerThread; ++i)
                {
                    q.post([&total]()
                           { total.fetch_add(1, std::memory_order_release); });
                } });
        }

        posters.clear();
        REQUIRE(waitForValue(total, threadsCount * tasksPerThread, 5000ms));
    }
}

// ============================================================================
// 6. Multiple concurrent queues
// ============================================================================
TEST_CASE("SerialQueue multiple concurrent queues", "[10_081_06_serial_queue]")
{
    SECTION("Two queues execute tasks independently and concurrently")
    {
        cpp_dbc::SerialQueue q1("concurrent-a");
        cpp_dbc::SerialQueue q2("concurrent-b");

        std::atomic<bool> q1Started{false};
        std::atomic<bool> q2Started{false};
        std::atomic<bool> q1Finished{false};
        std::atomic<bool> q2Finished{false};
        std::latch gate(1);

        // Block both queues behind a gate
        q1.post([&q1Started, &q1Finished, &gate]()
                {
            q1Started.store(true, std::memory_order_release);
            gate.wait();
            q1Finished.store(true, std::memory_order_release); });

        q2.post([&q2Started, &q2Finished, &gate]()
                {
            q2Started.store(true, std::memory_order_release);
            gate.wait();
            q2Finished.store(true, std::memory_order_release); });

        // Both should start (they have separate worker threads)
        REQUIRE(waitForFlag(q1Started));
        REQUIRE(waitForFlag(q2Started));

        // Neither should have finished (blocked on gate)
        REQUIRE_FALSE(q1Finished.load(std::memory_order_acquire));
        REQUIRE_FALSE(q2Finished.load(std::memory_order_acquire));

        // Release the gate — both finish
        gate.count_down();
        REQUIRE(waitForFlag(q1Finished));
        REQUIRE(waitForFlag(q2Finished));
    }

    SECTION("Multiple global queues process tasks independently")
    {
        auto &qA = cpp_dbc::SerialQueue::global("multi-global-a");
        auto &qB = cpp_dbc::SerialQueue::global("multi-global-b");

        std::atomic<int> countA{0};
        std::atomic<int> countB{0};

        for (int i = 0; i < 20; ++i)
        {
            qA.post([&countA]()
                    { countA.fetch_add(1, std::memory_order_release); });
            qB.post([&countB]()
                    { countB.fetch_add(1, std::memory_order_release); });
        }

        REQUIRE(waitForValue(countA, 20));
        REQUIRE(waitForValue(countB, 20));
    }
}

// ============================================================================
// 7. Concurrent posting from multiple threads
// ============================================================================
TEST_CASE("SerialQueue concurrent posting", "[10_081_07_serial_queue]")
{
    SECTION("Multiple threads posting to the same queue")
    {
        cpp_dbc::SerialQueue q("concurrent-post");
        std::atomic<int> total{0};
        constexpr int threadsCount = 4;
        constexpr int tasksPerThread = 25;

        std::vector<std::jthread> posters;
        posters.reserve(threadsCount);

        for (int t = 0; t < threadsCount; ++t)
        {
            posters.emplace_back([&q, &total]()
                                 {
                for (int i = 0; i < tasksPerThread; ++i)
                {
                    q.post([&total]()
                           { total.fetch_add(1, std::memory_order_release); });
                } });
        }

        // Join poster threads
        posters.clear();

        // Wait for all tasks to complete
        REQUIRE(waitForValue(total, threadsCount * tasksPerThread, 5000ms));
    }
}

// ============================================================================
// 8. Null / empty task
// ============================================================================
TEST_CASE("SerialQueue null task handling", "[10_081_08_serial_queue]")
{
    SECTION("Null std::function does not crash — subsequent tasks still run")
    {
        cpp_dbc::SerialQueue q("null-task");
        std::atomic<bool> afterNull{false};

        // Post a default-constructed (null) std::function
        q.post(std::function<void()>{});

        // Post a normal task after it
        q.post([&afterNull]()
               { afterNull.store(true, std::memory_order_release); });

        REQUIRE(waitForFlag(afterNull));
    }

    SECTION("nullptr std::function does not crash")
    {
        cpp_dbc::SerialQueue q("nullptr-task");
        std::atomic<bool> afterNull{false};

        q.post(nullptr);

        q.post([&afterNull]()
               { afterNull.store(true, std::memory_order_release); });

        REQUIRE(waitForFlag(afterNull));
    }
}

// ============================================================================
// 9. Reentrant posting — task posts to its own queue
// ============================================================================
TEST_CASE("SerialQueue reentrant posting", "[10_081_09_serial_queue]")
{
    SECTION("Task can post another task to the same queue without deadlock")
    {
        cpp_dbc::SerialQueue q("reentrant");
        std::atomic<int> executed{0};

        q.post([&q, &executed]()
               {
            executed.fetch_add(1, std::memory_order_release);

            // Post from within a running task — same queue
            q.post([&executed]()
                   { executed.fetch_add(1, std::memory_order_release); }); });

        REQUIRE(waitForValue(executed, 2));
    }

    SECTION("Chain of reentrant posts — 5 levels deep")
    {
        cpp_dbc::SerialQueue q("reentrant-chain");
        std::atomic<int> depth{0};

        std::function<void()> postNext = [&q, &depth, &postNext]()
        {
            int current = depth.fetch_add(1, std::memory_order_acq_rel) + 1;
            if (current < 5)
            {
                q.post(postNext);
            }
        };

        q.post(postNext);
        REQUIRE(waitForValue(depth, 5));
    }
}

// ============================================================================
// 10. Non-std exception (catch(...) path)
// ============================================================================
TEST_CASE("SerialQueue non-std exception", "[10_081_10_serial_queue]")
{
    SECTION("Throwing an int does not crash the queue")
    {
        cpp_dbc::SerialQueue q("non-std-exc");
        std::atomic<bool> afterThrow{false};

        // Throw an int — not derived from std::exception
        q.post([]()
               { throw 42; }); // NOSONAR(cpp:S3257) — intentionally throwing non-std type for testing catch(...) path

        q.post([&afterThrow]()
               { afterThrow.store(true, std::memory_order_release); });

        REQUIRE(waitForFlag(afterThrow));
    }

    SECTION("Throwing a C string does not crash the queue")
    {
        cpp_dbc::SerialQueue q("non-std-cstr");
        std::atomic<bool> afterThrow{false};

        q.post([]()
               { throw "boom"; }); // NOSONAR(cpp:S3257) — intentionally throwing non-std type for testing catch(...) path

        q.post([&afterThrow]()
               { afterThrow.store(true, std::memory_order_release); });

        REQUIRE(waitForFlag(afterThrow));
    }
}

// ============================================================================
// 11. Concurrent global() access from multiple threads
// ============================================================================
TEST_CASE("SerialQueue global registry thread safety", "[10_081_11_serial_queue]")
{
    SECTION("Multiple threads requesting the same global name get the same instance")
    {
        constexpr int threadCount = 8;
        std::vector<cpp_dbc::SerialQueue *> results(threadCount, nullptr);
        std::vector<std::jthread> threads;
        threads.reserve(threadCount);
        std::latch startGate(threadCount);

        for (int i = 0; i < threadCount; ++i)
        {
            threads.emplace_back([&results, &startGate, i]()
                                 {
                startGate.arrive_and_wait(); // All threads start simultaneously
                auto &q = cpp_dbc::SerialQueue::global("registry-race");
                results[i] = &q; });
        }

        threads.clear(); // join all

        // All threads must have gotten the same pointer
        for (int i = 1; i < threadCount; ++i)
        {
            REQUIRE(results[i] == results[0]);
        }
    }

    SECTION("Multiple threads requesting different global names get different instances")
    {
        constexpr int threadCount = 4;
        std::vector<cpp_dbc::SerialQueue *> results(threadCount, nullptr);
        std::vector<std::jthread> threads;
        threads.reserve(threadCount);
        std::latch startGate(threadCount);

        for (int i = 0; i < threadCount; ++i)
        {
            threads.emplace_back([&results, &startGate, i]()
                                 {
                startGate.arrive_and_wait();
                auto &q = cpp_dbc::SerialQueue::global("diff-name-" + std::to_string(i));
                results[i] = &q; });
        }

        threads.clear();

        // All pointers must be different
        for (int i = 0; i < threadCount; ++i)
        {
            for (int j = i + 1; j < threadCount; ++j)
            {
                REQUIRE(results[i] != results[j]);
            }
        }
    }
}

// ============================================================================
// 12. Stress test
// ============================================================================
TEST_CASE("SerialQueue stress test", "[10_081_12_serial_queue]")
{
    SECTION("1000 tasks execute without loss")
    {
        cpp_dbc::SerialQueue q("stress");
        std::atomic<int> total{0};
        constexpr int taskCount = 1000;

        for (int i = 0; i < taskCount; ++i)
        {
            q.post([&total]()
                   { total.fetch_add(1, std::memory_order_release); });
        }

        REQUIRE(waitForValue(total, taskCount, 10000ms));
    }

    SECTION("1000 tasks drain on destruction without loss")
    {
        std::atomic<int> total{0};
        constexpr int taskCount = 1000;

        {
            cpp_dbc::SerialQueue q("stress-drain");
            for (int i = 0; i < taskCount; ++i)
            {
                q.post([&total]()
                       { total.fetch_add(1, std::memory_order_release); });
            }
            // Destructor drains
        }

        REQUIRE(total.load(std::memory_order_acquire) == taskCount);
    }
}

// ============================================================================
// 13. Task lifecycle — shared_ptr released after execution
// ============================================================================
TEST_CASE("SerialQueue task lifecycle", "[10_081_13_serial_queue]")
{
    SECTION("Captured shared_ptr is released after task completes")
    {
        cpp_dbc::SerialQueue q("lifecycle");
        auto resource = std::make_shared<int>(42);
        std::weak_ptr<int> observer = resource;
        std::atomic<bool> taskDone{false};

        // Post a task that captures the shared_ptr — then release our copy
        q.post([captured = std::move(resource), &taskDone]()
               {
            (void)captured; // Use the capture
            taskDone.store(true, std::memory_order_release); });

        REQUIRE(waitForFlag(taskDone));

        // Post a barrier task — when it completes, the previous task's std::function
        // (which captured the shared_ptr) has already been destroyed by the worker.
        std::atomic<bool> barrier{false};
        q.post([&barrier]()
               { barrier.store(true, std::memory_order_release); });
        REQUIRE(waitForFlag(barrier));

        REQUIRE(observer.expired());
    }

    SECTION("Captured shared_ptr is released even if task throws")
    {
        cpp_dbc::SerialQueue q("lifecycle-throw");
        auto resource = std::make_shared<int>(99);
        std::weak_ptr<int> observer = resource;
        std::atomic<bool> afterDone{false};

        q.post([captured = std::move(resource)]()
               {
            (void)captured;
            throw std::runtime_error("intentional"); });

        // Post a follow-up to know when the throwing task has been processed
        q.post([&afterDone]()
               { afterDone.store(true, std::memory_order_release); });

        REQUIRE(waitForFlag(afterDone));

        // Post a barrier task — when it completes, the throwing task's std::function
        // (which captured the shared_ptr) has already been destroyed by the worker.
        std::atomic<bool> barrier{false};
        q.post([&barrier]()
               { barrier.store(true, std::memory_order_release); });
        REQUIRE(waitForFlag(barrier));

        REQUIRE(observer.expired());
    }
}
