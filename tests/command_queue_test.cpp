#include "components/centuryvspump/CenturyPumpCommandQueue.h"

#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>

using namespace esphome::century_vs_pump;

static CenturyPumpCommand command(uint8_t function,
                                  std::vector<uint8_t> payload = {}) {
  CenturyPumpCommand result;
  result.function_ = function;
  result.payload_ = std::move(payload);
  return result;
}

int main() {
  {
    CenturyPumpCommandQueue queue;
    auto rpm = command(0x45, {0x00, 0x00});
    assert(queue.enqueue_poll(rpm, nullptr));
    assert(!queue.enqueue_poll(rpm, nullptr));
    assert(queue.poll_size() == 1);

    auto in_flight = queue.take_next();
    in_flight->kind_ = CenturyPumpCommandKind::POLL;
    assert(!queue.enqueue_poll(rpm, in_flight.get()));
    assert(queue.enqueue_poll(command(0x45, {0x00, 0x03}), in_flight.get()));
  }

  {
    CenturyPumpCommandQueue queue;
    int first_consumer = 0;
    int second_consumer = 0;

    auto first = command(0x45, {0x00, 0x03});
    first.poll_consumer_ = &first_consumer;
    first.on_data_funcs_.push_back(
        [&](CenturyVSPump *, const std::vector<uint8_t> &) {
          first_consumer++;
        });
    auto second = command(0x45, {0x00, 0x03});
    second.poll_consumer_ = &second_consumer;
    second.on_data_funcs_.push_back(
        [&](CenturyVSPump *, const std::vector<uint8_t> &) {
          second_consumer++;
        });

    assert(queue.enqueue_poll(first, nullptr));
    assert(!queue.enqueue_poll(second, nullptr));
    // A later update of the same consumer must not duplicate its callback.
    assert(!queue.enqueue_poll(first, nullptr));
    auto coalesced = queue.take_next();
    assert(coalesced->on_data_funcs_.size() == 2);
    for (const auto &callback : coalesced->on_data_funcs_)
      callback(nullptr, {});
    assert(first_consumer == 1);
    assert(second_consumer == 1);
  }

  {
    CenturyPumpCommandQueue queue;
    assert(queue.enqueue_poll(command(0x43), nullptr));
    assert(queue.enqueue_control(command(0x44, {0x00, 0x10, 0x00})) ==
           ControlQueueResult::QUEUED);
    assert(queue.enqueue_control(command(0x41)) == ControlQueueResult::QUEUED);
    assert(queue.take_next()->function_ == 0x44);
    assert(queue.take_next()->function_ == 0x41);
    assert(queue.take_next()->function_ == 0x43);
  }

  {
    CenturyPumpCommandQueue queue;
    assert(queue.enqueue_control(command(0x44, {0x00, 0x10, 0x00})) ==
           ControlQueueResult::QUEUED);
    assert(queue.enqueue_control(command(0x41)) == ControlQueueResult::QUEUED);
    assert(queue.enqueue_control(command(0x64, {0x01, 0x00, 0x00})) ==
           ControlQueueResult::QUEUED);
    assert(queue.enqueue_stop(command(0x42)) == ControlQueueResult::QUEUED);
    assert(queue.stop_pending());
    assert(queue.enqueue_stop(command(0x42)) ==
           ControlQueueResult::DUPLICATE_STOP);
    assert(queue.take_next()->function_ == 0x42);
    assert(queue.take_next()->function_ == 0x64);
    assert(queue.take_next() == nullptr);
  }

  {
    CenturyPumpCommandQueue queue;
    for (size_t i = 0; i < CenturyPumpCommandQueue::MAX_CONTROL_QUEUE_SIZE - 1;
         i++) {
      assert(queue.enqueue_control(command(0x64, {static_cast<uint8_t>(i)})) ==
             ControlQueueResult::QUEUED);
    }
    assert(queue.control_size() ==
           CenturyPumpCommandQueue::MAX_CONTROL_QUEUE_SIZE - 1);
    assert(queue.enqueue_control(command(0x41)) == ControlQueueResult::REFUSED);

    // The reserved slot is available to a transaction continuation.
    assert(queue.enqueue_control(command(0x65), true) ==
           ControlQueueResult::QUEUED);
    assert(queue.control_size() ==
           CenturyPumpCommandQueue::MAX_CONTROL_QUEUE_SIZE);
    assert(queue.take_next()->function_ == 0x65);
  }

  {
    CenturyPumpCommandQueue queue;
    for (size_t i = 0; i < CenturyPumpCommandQueue::MAX_CONTROL_QUEUE_SIZE - 1;
         i++) {
      assert(queue.enqueue_control(command(0x64, {static_cast<uint8_t>(i)})) ==
             ControlQueueResult::QUEUED);
    }
    auto in_flight = queue.take_next();
    assert(queue.enqueue_control(command(0x41)) == ControlQueueResult::QUEUED);
    queue.restore_front(std::move(in_flight));
    assert(queue.control_size() ==
           CenturyPumpCommandQueue::MAX_CONTROL_QUEUE_SIZE);
  }

  return 0;
}
