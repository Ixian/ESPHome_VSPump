#include "components/centuryvspump/CenturyVSPumpProtocol.h"

#include <cassert>
#include <cstdint>
#include <vector>

using esphome::century_vs_pump::protocol::ResponseValidation;

int main() {
  using namespace esphome::century_vs_pump::protocol;

  const std::vector<uint8_t> empty;
  assert((make_request_pdu(0x41, empty) == std::vector<uint8_t>{0x41, 0x20}));

  const std::vector<uint8_t> demand{0x00, 0x38, 0x15};
  const std::vector<uint8_t> demand_pdu{0x44, 0x20, 0x00, 0x38, 0x15};
  assert(make_request_pdu(0x44, demand) == demand_pdu);
  assert(request_matches(0x44, demand, demand_pdu));

  // The PDU deliberately excludes the Modbus device address (0x15). The
  // ESPHome ModbusClientDevice prepends that address when it builds the frame.
  const std::vector<uint8_t> incorrectly_addressed{0x15, 0x44, 0x20,
                                                   0x00, 0x38, 0x15};
  assert(!request_matches(0x44, demand, incorrectly_addressed));

  assert(validate_response(0x43, std::vector<uint8_t>{0x43, 0x10, 0x0B}) ==
         ResponseValidation::OK);
  assert(validate_response(0x43, std::vector<uint8_t>{}) ==
         ResponseValidation::TOO_SHORT);
  assert(validate_response(0x43, std::vector<uint8_t>{0x43}) ==
         ResponseValidation::TOO_SHORT);
  assert(validate_response(0x43, std::vector<uint8_t>{0x45, 0x10, 0x0B}) ==
         ResponseValidation::FUNCTION_MISMATCH);
  assert(validate_response(0x43, std::vector<uint8_t>{0x43, 0x06}) ==
         ResponseValidation::NACK);

  // Zero is the disabled sentinel and must remain ready across the 32-bit
  // millis() half-range and wrap.
  assert(deadline_reached(0x00000000, 0));
  assert(deadline_reached(0x80000000, 0));
  assert(deadline_reached(0xFFFFFFFF, 0));
  assert(!deadline_reached(0xFFFFFFEF, 0xFFFFFFF0));
  assert(deadline_reached(0xFFFFFFF0, 0xFFFFFFF0));
  assert(deadline_reached(0x00000010, 0xFFFFFFF0));
  assert(!deadline_reached(0xFFFFFFF0, 0x00000010));
  assert(deadline_reached(0x00000010, 0x00000010));

  assert(superseded_by_stop(0x41, true));
  assert(superseded_by_stop(0x44, true));
  assert(!superseded_by_stop(0x42, true));
  assert(!superseded_by_stop(0x64, true));
  assert(!superseded_by_stop(0x41, false));

  bool stop_pending = true;
  stop_pending = stop_pending_after_terminal(0x42, stop_pending);
  assert(!stop_pending);
  // A GO submitted after the completed STOP must retain normal retry behavior.
  assert(!superseded_by_stop(0x41, stop_pending));
  assert(stop_pending_after_terminal(0x41, true));

  return 0;
}
