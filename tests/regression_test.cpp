// Regression test: a request carrying a coordinator epoch higher than the
// current one is rejected and does not move the epoch, so the current epoch's
// authority holders keep their authority.
#include "execution_fabric/engine.hpp"
#include "test_util.hpp"
#include <cstdio>

using namespace execution_fabric;

int main() {
    const CoordinatorEpoch EPOCH(1);
    ExecutionEngine eng(EPOCH);
    const ExecutionId ex = ExecutionId::random();
    CHECK_EQ(eng.create(ex, EPOCH).code, DecisionCode::ALLOW);
    CHECK_EQ(eng.activate(ex, EPOCH).code, DecisionCode::ALLOW);
    const WorkerId w = WorkerId::random();
    const WorkerBootId b = WorkerBootId::random();
    DispatchTicket d;
    CHECK_EQ(eng.dispatch(ex, w, b, EPOCH, d).code, DecisionCode::ALLOW);

    // Requests carrying a future epoch are refused and leave the epoch alone.
    const ExecutionId other = ExecutionId::random();
    CHECK_EQ(eng.create(other, CoordinatorEpoch(1000)).code, DecisionCode::REJECT_STALE_EPOCH);
    CHECK(eng.current_epoch() == EPOCH);
    CHECK(eng.find(other) == nullptr);
    const ExecutionId third = ExecutionId::random();
    CHECK_EQ(eng.create(third, EPOCH).code, DecisionCode::ALLOW);
    CHECK(eng.find(third) != nullptr);
    if (eng.find(third) != nullptr) {
        CHECK_EQ(eng.activate(third, CoordinatorEpoch(5)).code, DecisionCode::REJECT_STALE_EPOCH);
        CHECK_EQ(eng.activate(third, EPOCH).code, DecisionCode::ALLOW);
        DispatchTicket d3;
        CHECK_EQ(eng.dispatch(third, w, b, CoordinatorEpoch(5), d3).code, DecisionCode::REJECT_STALE_EPOCH);
    }
    CHECK(eng.current_epoch() == EPOCH);

    // The worker holding the current epoch's ticket still starts and completes.
    StartTicket s;
    s.execution_id = d.execution_id; s.execution_generation = d.execution_generation;
    s.attempt_id = d.attempt_id; s.attempt_generation = d.attempt_generation;
    s.worker_id = d.worker_id; s.worker_boot_id = d.worker_boot_id; s.epoch = d.epoch;
    s.dispatch_id = d.dispatch_id; s.dispatch_generation = d.dispatch_generation;
    s.ownership_generation = d.ownership_generation; s.fence_generation = d.fence_generation;
    CHECK_EQ(eng.mark_running(s).code, DecisionCode::ALLOW);
    CompletionTicket c;
    c.execution_id = d.execution_id; c.execution_generation = d.execution_generation;
    c.attempt_id = d.attempt_id; c.attempt_generation = d.attempt_generation;
    c.worker_id = d.worker_id; c.worker_boot_id = d.worker_boot_id; c.epoch = d.epoch;
    c.dispatch_id = d.dispatch_id; c.dispatch_generation = d.dispatch_generation;
    c.ownership_generation = d.ownership_generation; c.fence_generation = d.fence_generation;
    c.completion_generation = CompletionGeneration(1);
    const char payload[] = "result";
    c.has_result = true;
    c.result_digest = ResultDigest::of(payload, sizeof(payload));
    CHECK_EQ(eng.complete(c).code, DecisionCode::ALLOW);
    CHECK_EQ(eng.find(ex)->state, ExecutionState::COMMITTED);

    // The coordinator's own epoch advance still works.
    eng.raise_epoch(CoordinatorEpoch(2));
    CHECK(eng.current_epoch() == CoordinatorEpoch(2));
    CHECK_EQ(eng.create(other, CoordinatorEpoch(2)).code, DecisionCode::ALLOW);
    return TEST_MAIN_RETURN();
}
