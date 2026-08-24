#pragma once

#include "CenturyPumpCommand.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>

namespace esphome::century_vs_pump {

enum class ControlQueueResult : uint8_t {
  QUEUED,
  REFUSED,
  DUPLICATE_STOP,
};

class CenturyPumpCommandQueue {
public:
  static constexpr size_t MAX_CONTROL_QUEUE_SIZE = 24;

  bool enqueue_poll(const CenturyPumpCommand &command,
                    const CenturyPumpCommand *in_flight) {
    if (in_flight != nullptr &&
        in_flight->kind_ == CenturyPumpCommandKind::POLL &&
        same_request_(*in_flight, command))
      return false;
    for (auto &queued : this->polls_) {
      if (same_request_(*queued, command)) {
        queued->add_poll_consumer(command);
        return false;
      }
    }

    auto queued = std::make_unique<CenturyPumpCommand>(command);
    queued->kind_ = CenturyPumpCommandKind::POLL;
    if (command.poll_consumer_ != nullptr)
      queued->poll_consumers_.push_back(command.poll_consumer_);
    this->polls_.push_back(std::move(queued));
    return true;
  }

  ControlQueueResult enqueue_control(const CenturyPumpCommand &command,
                                     bool front = false) {
    // Reserve one slot for the continuation/retry of the in-flight command.
    const size_t admission_limit =
        front ? MAX_CONTROL_QUEUE_SIZE : MAX_CONTROL_QUEUE_SIZE - 1;
    if (this->controls_.size() >= admission_limit)
      return ControlQueueResult::REFUSED;

    auto queued = std::make_unique<CenturyPumpCommand>(command);
    queued->kind_ = CenturyPumpCommandKind::CONTROL;
    if (front)
      this->controls_.push_front(std::move(queued));
    else
      this->controls_.push_back(std::move(queued));
    return ControlQueueResult::QUEUED;
  }

  ControlQueueResult enqueue_stop(const CenturyPumpCommand &command) {
    this->controls_.erase(std::remove_if(this->controls_.begin(),
                                         this->controls_.end(),
                                         [](const auto &queued) {
                                           return queued->function_ == 0x41 ||
                                                  queued->function_ == 0x44;
                                         }),
                          this->controls_.end());
    if (this->stop_ != nullptr)
      return ControlQueueResult::DUPLICATE_STOP;
    this->stop_ = std::make_unique<CenturyPumpCommand>(command);
    this->stop_->kind_ = CenturyPumpCommandKind::CONTROL;
    return ControlQueueResult::QUEUED;
  }

  CenturyPumpCommand *next() {
    if (this->stop_ != nullptr)
      return this->stop_.get();
    if (!this->controls_.empty())
      return this->controls_.front().get();
    if (!this->polls_.empty())
      return this->polls_.front().get();
    return nullptr;
  }

  std::unique_ptr<CenturyPumpCommand> take_next() {
    if (this->stop_ != nullptr)
      return std::move(this->stop_);
    auto *queue = !this->controls_.empty() ? &this->controls_ : &this->polls_;
    if (queue->empty())
      return nullptr;
    auto command = std::move(queue->front());
    queue->pop_front();
    return command;
  }

  void restore_front(std::unique_ptr<CenturyPumpCommand> command) {
    if (command->kind_ == CenturyPumpCommandKind::CONTROL)
      this->controls_.push_front(std::move(command));
    else
      this->polls_.push_front(std::move(command));
  }

  size_t control_size() const { return this->controls_.size(); }
  size_t poll_size() const { return this->polls_.size(); }
  bool stop_pending() const { return this->stop_ != nullptr; }

private:
  static bool same_request_(const CenturyPumpCommand &left,
                            const CenturyPumpCommand &right) {
    return left.function_ == right.function_ && left.payload_ == right.payload_;
  }

  std::deque<std::unique_ptr<CenturyPumpCommand>> controls_;
  std::deque<std::unique_ptr<CenturyPumpCommand>> polls_;
  std::unique_ptr<CenturyPumpCommand> stop_;
};

} // namespace esphome::century_vs_pump
