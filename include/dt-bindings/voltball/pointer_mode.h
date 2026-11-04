/* SPDX-License-Identifier: MIT */
#pragma once

/* &ptr commands. Speed commands act on the current mode (pointer or scroll). */
#define PTR_SPD_UP 0
#define PTR_SPD_DN 1
#define PTR_SPD_RST 2
#define PTR_SCRL_TOG 3
#define PTR_SCRL_MO 4

/* zip_pointer_mode stages, in listener order. */
#define PTR_STAGE_MODE 0
#define PTR_STAGE_SPEED 1
