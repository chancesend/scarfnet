#include <unity.h>
#include <rx_queue.h>

using Scarfnet::RxQueue;

template <size_t N>
static bool pushValue(RxQueue<int, N>& q, int v)
{
    return q.push([v](int& slot) { slot = v; });
}

void test_rx_queue_pops_in_order()
{
    RxQueue<int, 3> q;
    pushValue(q, 1);
    pushValue(q, 2);
    int v = 0;
    TEST_ASSERT_TRUE(q.pop(v)); TEST_ASSERT_EQUAL(1, v);
    TEST_ASSERT_TRUE(q.pop(v)); TEST_ASSERT_EQUAL(2, v);
    TEST_ASSERT_FALSE(q.pop(v));
}

void test_rx_queue_holds_full_capacity()
{
    RxQueue<int, 3> q;
    TEST_ASSERT_TRUE(pushValue(q, 1));
    TEST_ASSERT_TRUE(pushValue(q, 2));
    TEST_ASSERT_TRUE(pushValue(q, 3));
    TEST_ASSERT_FALSE(pushValue(q, 4));
    // The 4th was dropped, not written over the oldest.
    int v = 0;
    q.pop(v);
    TEST_ASSERT_EQUAL(1, v);
}

void test_rx_queue_counts_and_resets_drops()
{
    RxQueue<int, 1> q;
    q.push([](int& s) { s = 1; });
    q.push([](int& s) { s = 2; });
    q.push([](int& s) { s = 3; });
    TEST_ASSERT_EQUAL_UINT32(2, q.takeDropped());
    TEST_ASSERT_EQUAL_UINT32(0, q.takeDropped());
}

void test_rx_queue_full_push_does_not_call_fill()
{
    RxQueue<int, 1> q;
    pushValue(q, 1);
    bool called = false;
    q.push([&](int&) { called = true; });
    TEST_ASSERT_FALSE(called);
}

void test_rx_queue_wraps_around()
{
    RxQueue<int, 3> q;
    int v = 0;
    for (int i = 0; i < 10; ++i) {
        TEST_ASSERT_TRUE(pushValue(q, i));
        TEST_ASSERT_TRUE(q.pop(v));
        TEST_ASSERT_EQUAL(i, v);
    }
    TEST_ASSERT_EQUAL_UINT32(0, q.takeDropped());
}

struct CountingLock {
    int depth = 0, maxDepth = 0, locks = 0;
    void lock()   { ++locks; if (++depth > maxDepth) maxDepth = depth; }
    void unlock() { --depth; }
};

void test_rx_queue_every_operation_takes_the_lock()
{
    RxQueue<int, 2, CountingLock> q;
    q.push([](int& s) { s = 1; });
    int v;
    q.pop(v);
    q.takeDropped();
    TEST_ASSERT_EQUAL(3, q.lock().locks);
    TEST_ASSERT_EQUAL(0, q.lock().depth);
    TEST_ASSERT_EQUAL(1, q.lock().maxDepth);
}

void rx_queue_tests()
{
    RUN_TEST(test_rx_queue_pops_in_order);
    RUN_TEST(test_rx_queue_holds_full_capacity);
    RUN_TEST(test_rx_queue_counts_and_resets_drops);
    RUN_TEST(test_rx_queue_full_push_does_not_call_fill);
    RUN_TEST(test_rx_queue_wraps_around);
    RUN_TEST(test_rx_queue_every_operation_takes_the_lock);
}
