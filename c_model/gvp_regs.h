//----------------------------------------------------------------------------
// File        : gvp_regs.h
// Description : Auto-generated register map for the GVP DUT.
//               DO NOT EDIT. Regenerate via scripts/gen_regs.py
//               from specs/registers.yaml.
// Author      : Gyusup LEE <gyu2910@waric.co.kr>
// Created     : 2026-08-20
// Copyright   : (c) 2026 Gyusup LEE. All rights reserved.
//----------------------------------------------------------------------------

#ifndef GVP_REGS_H_
#define GVP_REGS_H_

#include <stdint.h>

#define GVP_ADDR_BITS  12
#define GVP_DATA_BITS  32

// -------------------------------------------------------------------
// Register offsets
// -------------------------------------------------------------------
#define GVP_REG_CTRL                             0x000u
#define GVP_REG_GIE                              0x004u
#define GVP_REG_IER                              0x008u
#define GVP_REG_ISR                              0x00Cu
#define GVP_REG_RD_ADDR_LO                       0x010u
#define GVP_REG_RD_ADDR_HI                       0x014u
#define GVP_REG_WR_ADDR_LO                       0x018u
#define GVP_REG_WR_ADDR_HI                       0x01Cu
#define GVP_REG_NUM_FFTS                         0x020u
#define GVP_REG_MODE                             0x024u
#define GVP_REG_FFT_CFG                          0x028u
#define GVP_REG_STATUS                           0x02Cu
#define GVP_REG_CYCLE_CNT                        0x030u
#define GVP_REG_RD_BEAT_CNT                      0x034u
#define GVP_REG_WR_BEAT_CNT                      0x038u
#define GVP_REG_RD_LAT_ACC                       0x03Cu
#define GVP_REG_WR_LAT_ACC                       0x040u
#define GVP_REG_MO_MAX                           0x044u
#define GVP_REG_RD_TXN_CNT                       0x048u
#define GVP_REG_WR_TXN_CNT                       0x04Cu
#define GVP_REG_MAPPER_CTRL                      0x050u

// -------------------------------------------------------------------
// Field masks and shifts
// -------------------------------------------------------------------
// CTRL (0x000) : Control and handshake register (ap_ctrl_hs)
#define GVP_CTRL_AP_START_LSB                    0u
#define GVP_CTRL_AP_START_WIDTH                  1u
#define GVP_CTRL_AP_START_MASK                   0x00000001u
#define GVP_CTRL_AP_DONE_LSB                     1u
#define GVP_CTRL_AP_DONE_WIDTH                   1u
#define GVP_CTRL_AP_DONE_MASK                    0x00000002u
#define GVP_CTRL_AP_IDLE_LSB                     2u
#define GVP_CTRL_AP_IDLE_WIDTH                   1u
#define GVP_CTRL_AP_IDLE_MASK                    0x00000004u
#define GVP_CTRL_AP_READY_LSB                    3u
#define GVP_CTRL_AP_READY_WIDTH                  1u
#define GVP_CTRL_AP_READY_MASK                   0x00000008u
#define GVP_CTRL_AUTO_RESTART_LSB                7u
#define GVP_CTRL_AUTO_RESTART_WIDTH              1u
#define GVP_CTRL_AUTO_RESTART_MASK               0x00000080u

// GIE (0x004) : Global interrupt enable
#define GVP_GIE_GIE_LSB                          0u
#define GVP_GIE_GIE_WIDTH                        1u
#define GVP_GIE_GIE_MASK                         0x00000001u

// IER (0x008) : IP interrupt enable
#define GVP_IER_AP_DONE_INT_LSB                  0u
#define GVP_IER_AP_DONE_INT_WIDTH                1u
#define GVP_IER_AP_DONE_INT_MASK                 0x00000001u
#define GVP_IER_AP_READY_INT_LSB                 1u
#define GVP_IER_AP_READY_INT_WIDTH               1u
#define GVP_IER_AP_READY_INT_MASK                0x00000002u
#define GVP_IER_RD_ERR_INT_LSB                   2u
#define GVP_IER_RD_ERR_INT_WIDTH                 1u
#define GVP_IER_RD_ERR_INT_MASK                  0x00000004u
#define GVP_IER_WR_ERR_INT_LSB                   3u
#define GVP_IER_WR_ERR_INT_WIDTH                 1u
#define GVP_IER_WR_ERR_INT_MASK                  0x00000008u

// ISR (0x00C) : IP interrupt status (write 1 to clear)
#define GVP_ISR_AP_DONE_INT_LSB                  0u
#define GVP_ISR_AP_DONE_INT_WIDTH                1u
#define GVP_ISR_AP_DONE_INT_MASK                 0x00000001u
#define GVP_ISR_AP_READY_INT_LSB                 1u
#define GVP_ISR_AP_READY_INT_WIDTH               1u
#define GVP_ISR_AP_READY_INT_MASK                0x00000002u
#define GVP_ISR_RD_ERR_INT_LSB                   2u
#define GVP_ISR_RD_ERR_INT_WIDTH                 1u
#define GVP_ISR_RD_ERR_INT_MASK                  0x00000004u
#define GVP_ISR_WR_ERR_INT_LSB                   3u
#define GVP_ISR_WR_ERR_INT_WIDTH                 1u
#define GVP_ISR_WR_ERR_INT_MASK                  0x00000008u

// MODE (0x024) : Operating mode select
#define GVP_MODE_MODE_LSB                        0u
#define GVP_MODE_MODE_WIDTH                      2u
#define GVP_MODE_MODE_MASK                       0x00000003u

// STATUS (0x02C) : Error / status flags (auto-clear on ap_start)
#define GVP_STATUS_RD_ERROR_LSB                  0u
#define GVP_STATUS_RD_ERROR_WIDTH                1u
#define GVP_STATUS_RD_ERROR_MASK                 0x00000001u
#define GVP_STATUS_WR_ERROR_LSB                  1u
#define GVP_STATUS_WR_ERROR_WIDTH                1u
#define GVP_STATUS_WR_ERROR_MASK                 0x00000002u
#define GVP_STATUS_OVERFLOW_LSB                  2u
#define GVP_STATUS_OVERFLOW_WIDTH                1u
#define GVP_STATUS_OVERFLOW_MASK                 0x00000004u

// MO_MAX (0x044) : Peak outstanding transaction counts (per master, saturating)
#define GVP_MO_MAX_RD_MO_MAX_LSB                 0u
#define GVP_MO_MAX_RD_MO_MAX_WIDTH               16u
#define GVP_MO_MAX_RD_MO_MAX_MASK                0x0000FFFFu
#define GVP_MO_MAX_WR_MO_MAX_LSB                 16u
#define GVP_MO_MAX_WR_MO_MAX_WIDTH               16u
#define GVP_MO_MAX_WR_MO_MAX_MASK                0xFFFF0000u

// MAPPER_CTRL (0x050) : ID mapper policy selection
#define GVP_MAPPER_CTRL_POLICY_LSB               0u
#define GVP_MAPPER_CTRL_POLICY_WIDTH             2u
#define GVP_MAPPER_CTRL_POLICY_MASK              0x00000003u

// -------------------------------------------------------------------
// Enumerated field values
// -------------------------------------------------------------------
// MODE.mode
#define GVP_MODE_MODE_FFT                        0u
#define GVP_MODE_MODE_READ_ONLY                  1u
#define GVP_MODE_MODE_WRITE_ONLY                 2u

// MAPPER_CTRL.policy
#define GVP_MAPPER_CTRL_POLICY_SEQUENTIAL        0u
#define GVP_MAPPER_CTRL_POLICY_ROUND_ROBIN       1u
#define GVP_MAPPER_CTRL_POLICY_RANDOM            2u

#endif  // GVP_REGS_H_
