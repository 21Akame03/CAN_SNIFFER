#include "Oracle.h"
#include "unity.h"

#include <string.h>

static twai_message_t make_msg(uint32_t id, bool ext, bool rtr, uint8_t dlc,
                               const uint8_t *data) {
  twai_message_t msg = {
      .identifier = id,
      .extd = ext,
      .rtr = rtr,
      .data_length_code = dlc,
  };
  if (data && dlc > 0) {
    const uint8_t capped = dlc > 8 ? 8 : dlc;
    memcpy(msg.data, data, capped);
  }
  return msg;
}

TEST_CASE("Oracle formats standard CAN frame to JSON", "[oracle][usb_jtag]") {
  const uint8_t data[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
  oracle_can_frame_t frame = {
      .message = make_msg(0x123, false, false, 8, data),
      .timestamp_us = 123456789ULL,
  };

  char buf[160] = {0};
  size_t written = Oracle_FormatCANFrame(&frame, buf, sizeof(buf));

  TEST_ASSERT_GREATER_THAN(0, written);
  TEST_ASSERT_LESS_THAN(sizeof(buf), written);
  TEST_ASSERT_EQUAL_STRING(
      "{\"type\":\"can\",\"ts_us\":123456789,\"id\":291,\"ext\":false,\"rtr\":"
      "false,\"dlc\":8,\"data\":\"1122334455667788\"}\n",
      buf);
}

TEST_CASE("Oracle formats extended RTR frame and short payload", "[oracle][usb_jtag]") {
  const uint8_t data[3] = {0xDE, 0xAD, 0xBE};
  oracle_can_frame_t frame = {
      .message = make_msg(0x1ABCDE, true, true, 3, data),
      .timestamp_us = 42,
  };

  char buf[128] = {0};
  size_t written = Oracle_FormatCANFrame(&frame, buf, sizeof(buf));

  TEST_ASSERT_GREATER_THAN(0, written);
  TEST_ASSERT_NOT_EQUAL(0, strstr(buf, "\"ext\":true"));
  TEST_ASSERT_NOT_EQUAL(0, strstr(buf, "\"rtr\":true"));
  TEST_ASSERT_NOT_EQUAL(0, strstr(buf, "\"dlc\":3"));
  TEST_ASSERT_NOT_EQUAL(0, strstr(buf, "\"data\":\"DEADBE\""));
  TEST_ASSERT_EQUAL('\n', buf[written - 1]);
}

TEST_CASE("Oracle truncates payload longer than 8 bytes", "[oracle][usb_jtag]") {
  const uint8_t data[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  oracle_can_frame_t frame = {
      .message = make_msg(0x7FF, false, false, 10, data),
      .timestamp_us = 7,
  };

  char buf[160] = {0};
  size_t written = Oracle_FormatCANFrame(&frame, buf, sizeof(buf));

  TEST_ASSERT_GREATER_THAN(0, written);
  TEST_ASSERT_NOT_EQUAL(0, strstr(buf, "\"dlc\":8"));
  TEST_ASSERT_NOT_EQUAL(0, strstr(buf, "\"data\":\"0001020304050607\""));
  TEST_ASSERT_EQUAL('\n', buf[written - 1]);
}

TEST_CASE("Oracle_FormatCANFrame handles null inputs safely", "[oracle][usb_jtag]") {
  char buf[16];
  oracle_can_frame_t frame = {.message = make_msg(0, false, false, 0, NULL), .timestamp_us = 0};

  TEST_ASSERT_EQUAL_UINT32(0, Oracle_FormatCANFrame(NULL, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_UINT32(0, Oracle_FormatCANFrame(&frame, NULL, sizeof(buf)));
  TEST_ASSERT_EQUAL_UINT32(0, Oracle_FormatCANFrame(&frame, buf, 0));
}
