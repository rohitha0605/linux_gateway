#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

bool calc_handle_frame(const uint8_t *frame, size_t frame_len,
                       uint8_t *out, size_t out_cap, size_t *out_len);
