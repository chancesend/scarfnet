#include <unity.h>
#include <TaskScheduler.h>

// ---------------------------------------------------------------------------
// Regression test: execute() must not follow a dangling nextTask pointer when
// a callback deletes the next task in the linked list.
//
// Root cause of the painlessMesh crash: BufferedConnection owns sentBufferTask
// and readBufferTask as value members.  When the connection is destroyed from
// inside a scheduler callback, Task::~Task() calls deleteTask(), which unlinks
// the task and frees its memory while execute() still holds a pre-captured
// nextTask pointer to it.
//
// Fix (lib/TaskScheduler): re-read nextTask = iCurrent->iNext after every
// callback so deleted tasks are skipped rather than dereferenced.
//
// Test setup:
//   Scheduler list: A → B → C  (construction order)
//   A's callback deletes B (via `delete`, which triggers Task::~Task() →
//   deleteTask() → relinks A.iNext to C before freeing B's memory).
//
// Expected:  A ran, B skipped (deleted before it was reached), C ran.
// Without fix: nextTask still points to freed B; C is unreachable, so
//   c_count == 0 and the assertion fails.
// ---------------------------------------------------------------------------

static Task* s_taskB;
static int   s_a_count;
static int   s_b_count;
static int   s_c_count;

static void cbA() {
    s_a_count++;
    // Mirrors BufferedConnection teardown: deleting the task calls
    // Task::~Task() → deleteTask(), which relinks A.iNext = C before
    // the memory is freed.
    delete s_taskB;
    s_taskB = nullptr;
}
static void cbB() { s_b_count++; }
static void cbC() { s_c_count++; }

void test_scheduler_execute_task_c_runs_after_b_deleted_by_a()
{
    Scheduler sched;
    s_a_count = s_b_count = s_c_count = 0;
    s_taskB = nullptr;

    // Construct in A → B → C order so that is their order in the linked list.
    Task taskA(TASK_IMMEDIATE, TASK_ONCE, cbA, &sched, true);
    s_taskB = new Task(TASK_IMMEDIATE, TASK_ONCE, cbB, &sched, true);
    Task taskC(TASK_IMMEDIATE, TASK_ONCE, cbC, &sched, true);

    sched.execute();

    TEST_ASSERT_EQUAL_INT(1, s_a_count); // A ran
    TEST_ASSERT_EQUAL_INT(0, s_b_count); // B was deleted before it could run
    TEST_ASSERT_EQUAL_INT(1, s_c_count); // C ran — UAF fix keeps nextTask valid
}

void scheduler_tests()
{
    RUN_TEST(test_scheduler_execute_task_c_runs_after_b_deleted_by_a);
}
