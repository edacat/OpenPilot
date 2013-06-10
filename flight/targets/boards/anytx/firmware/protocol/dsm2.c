/**
 ******************************************************************************
 * @addtogroup Radio Protocol hardware abstraction layer
 * @{
 * @addtogroup PIOS_CYRF6936 CYRF6936 Radio Protocol Functions
 * @brief CYRF6936 Radio Protocol functionality
 * @{
 *
 * @file       dsm2.c
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2012.
 * @brief      CYRF6936 Radio Protocol Functions
 *                 Full credits to DeviationTX Project, http://www.deviationtx.com/
 * @see        The GNU Public License (GPL) Version 3
 *
 *****************************************************************************/

/*
   This project is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Deviation is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Deviation.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <pios.h>
#include "interface.h"

#ifdef PROTO_HAS_CYRF6936
#define RANDOM_CHANNELS 0
#define BIND_CHANNEL    0x0d // This can be any odd channel
// #define USE_FIXED_MFGID
#define MODEL           0

#ifdef EMULATOR
#define USE_FIXED_MFGID
#define BIND_COUNT      2
#else
#define BIND_COUNT      600
#endif
enum {
    DSM2_BIND        = 0,
    DSM2_CHANSEL     = BIND_COUNT + 0,
    DSM2_CH1_WRITE_A = BIND_COUNT + 1,
    DSM2_CH1_CHECK_A = BIND_COUNT + 2,
    DSM2_CH2_WRITE_A = BIND_COUNT + 3,
    DSM2_CH2_CHECK_A = BIND_COUNT + 4,
    DSM2_CH2_READ_A  = BIND_COUNT + 5,
    DSM2_CH1_WRITE_B = BIND_COUNT + 6,
    DSM2_CH1_CHECK_B = BIND_COUNT + 7,
    DSM2_CH2_WRITE_B = BIND_COUNT + 8,
    DSM2_CH2_CHECK_B = BIND_COUNT + 9,
    DSM2_CH2_READ_B  = BIND_COUNT + 10,
};

static const u8 pncodes[5][9][8] = {
    /* Note these are in order transmitted (LSB 1st) */
    { /* Row 0 */
      /* Col 0 */   { 0x03, 0xBC, 0x6E, 0x8A, 0xEF, 0xBD, 0xFE, 0xF8 },
        /* Col 1 */ { 0x88, 0x17, 0x13, 0x3B, 0x2D, 0xBF, 0x06, 0xD6 },
        /* Col 2 */ { 0xF1, 0x94, 0x30, 0x21, 0xA1, 0x1C, 0x88, 0xA9 },
        /* Col 3 */ { 0xD0, 0xD2, 0x8E, 0xBC, 0x82, 0x2F, 0xE3, 0xB4 },
        /* Col 4 */ { 0x8C, 0xFA, 0x47, 0x9B, 0x83, 0xA5, 0x66, 0xD0 },
        /* Col 5 */ { 0x07, 0xBD, 0x9F, 0x26, 0xC8, 0x31, 0x0F, 0xB8 },
        /* Col 6 */ { 0xEF, 0x03, 0x95, 0x89, 0xB4, 0x71, 0x61, 0x9D },
        /* Col 7 */ { 0x40, 0xBA, 0x97, 0xD5, 0x86, 0x4F, 0xCC, 0xD1 },
        /* Col 8 */ { 0xD7, 0xA1, 0x54, 0xB1, 0x5E, 0x89, 0xAE, 0x86 }
    },
    { /* Row 1 */
      /* Col 0 */   { 0x83, 0xF7, 0xA8, 0x2D, 0x7A, 0x44, 0x64, 0xD3 },
        /* Col 1 */ { 0x3F, 0x2C, 0x4E, 0xAA, 0x71, 0x48, 0x7A, 0xC9 },
        /* Col 2 */ { 0x17, 0xFF, 0x9E, 0x21, 0x36, 0x90, 0xC7, 0x82 },
        /* Col 3 */ { 0xBC, 0x5D, 0x9A, 0x5B, 0xEE, 0x7F, 0x42, 0xEB },
        /* Col 4 */ { 0x24, 0xF5, 0xDD, 0xF8, 0x7A, 0x77, 0x74, 0xE7 },
        /* Col 5 */ { 0x3D, 0x70, 0x7C, 0x94, 0xDC, 0x84, 0xAD, 0x95 },
        /* Col 6 */ { 0x1E, 0x6A, 0xF0, 0x37, 0x52, 0x7B, 0x11, 0xD4 },
        /* Col 7 */ { 0x62, 0xF5, 0x2B, 0xAA, 0xFC, 0x33, 0xBF, 0xAF },
        /* Col 8 */ { 0x40, 0x56, 0x32, 0xD9, 0x0F, 0xD9, 0x5D, 0x97 }
    },
    { /* Row 2 */
      /* Col 0 */   { 0x40, 0x56, 0x32, 0xD9, 0x0F, 0xD9, 0x5D, 0x97 },
        /* Col 1 */ { 0x8E, 0x4A, 0xD0, 0xA9, 0xA7, 0xFF, 0x20, 0xCA },
        /* Col 2 */ { 0x4C, 0x97, 0x9D, 0xBF, 0xB8, 0x3D, 0xB5, 0xBE },
        /* Col 3 */ { 0x0C, 0x5D, 0x24, 0x30, 0x9F, 0xCA, 0x6D, 0xBD },
        /* Col 4 */ { 0x50, 0x14, 0x33, 0xDE, 0xF1, 0x78, 0x95, 0xAD },
        /* Col 5 */ { 0x0C, 0x3C, 0xFA, 0xF9, 0xF0, 0xF2, 0x10, 0xC9 },
        /* Col 6 */ { 0xF4, 0xDA, 0x06, 0xDB, 0xBF, 0x4E, 0x6F, 0xB3 },
        /* Col 7 */ { 0x9E, 0x08, 0xD1, 0xAE, 0x59, 0x5E, 0xE8, 0xF0 },
        /* Col 8 */ { 0xC0, 0x90, 0x8F, 0xBB, 0x7C, 0x8E, 0x2B, 0x8E }
    },
    { /* Row 3 */
      /* Col 0 */   { 0xC0, 0x90, 0x8F, 0xBB, 0x7C, 0x8E, 0x2B, 0x8E },
        /* Col 1 */ { 0x80, 0x69, 0x26, 0x80, 0x08, 0xF8, 0x49, 0xE7 },
        /* Col 2 */ { 0x7D, 0x2D, 0x49, 0x54, 0xD0, 0x80, 0x40, 0xC1 },
        /* Col 3 */ { 0xB6, 0xF2, 0xE6, 0x1B, 0x80, 0x5A, 0x36, 0xB4 },
        /* Col 4 */ { 0x42, 0xAE, 0x9C, 0x1C, 0xDA, 0x67, 0x05, 0xF6 },
        /* Col 5 */ { 0x9B, 0x75, 0xF7, 0xE0, 0x14, 0x8D, 0xB5, 0x80 },
        /* Col 6 */ { 0xBF, 0x54, 0x98, 0xB9, 0xB7, 0x30, 0x5A, 0x88 },
        /* Col 7 */ { 0x35, 0xD1, 0xFC, 0x97, 0x23, 0xD4, 0xC9, 0x88 },
        /* Col 8 */ { 0x88, 0xE1, 0xD6, 0x31, 0x26, 0x5F, 0xBD, 0x40 }
    },
    { /* Row 4 */
      /* Col 0 */   { 0xE1, 0xD6, 0x31, 0x26, 0x5F, 0xBD, 0x40, 0x93 },
        /* Col 1 */ { 0xDC, 0x68, 0x08, 0x99, 0x97, 0xAE, 0xAF, 0x8C },
        /* Col 2 */ { 0xC3, 0x0E, 0x01, 0x16, 0x0E, 0x32, 0x06, 0xBA },
        /* Col 3 */ { 0xE0, 0x83, 0x01, 0xFA, 0xAB, 0x3E, 0x8F, 0xAC },
        /* Col 4 */ { 0x5C, 0xD5, 0x9C, 0xB8, 0x46, 0x9C, 0x7D, 0x84 },
        /* Col 5 */ { 0xF1, 0xC6, 0xFE, 0x5C, 0x9D, 0xA5, 0x4F, 0xB7 },
        /* Col 6 */ { 0x58, 0xB5, 0xB3, 0xDD, 0x0E, 0x28, 0xF1, 0xB0 },
        /* Col 7 */ { 0x5F, 0x30, 0x3B, 0x56, 0x96, 0x45, 0xF4, 0xA1 },
        /* Col 8 */ { 0x03, 0xBC, 0x6E, 0x8A, 0xEF, 0xBD, 0xFE, 0xF8 }
    },
};

static const u8 pn_bind[] = { 0xc6, 0x94, 0x22, 0xfe, 0x48, 0xe6, 0x57, 0x4e };

static const u8 ch_map4[] = { 0, 1, 2, 3, 0xff, 0xff, 0xff }; // Guess
static const u8 ch_map5[] = { 0, 1, 2, 3, 4, 0xff, 0xff }; // Guess
static const u8 ch_map6[] = { 1, 5, 2, 3, 0, 4, 0xff }; // HP6DSM
static const u8 ch_map7[] = { 1, 5, 2, 4, 3, 6, 0 }; // DX6i
static const u8 ch_map8[] = { 1, 5, 2, 3, 6, 0xff, 0xff, 4, 0, 7, 0xff, 0xff, 0xff, 0xff }; // DX8
static const u8 ch_map9[] = { 3, 2, 1, 5, 0, 4, 6, 7, 8, 0xff, 0xff, 0xff, 0xff, 0xff }; // DM9
static const u8 ch_map10[] = { 3, 2, 1, 5, 0, 4, 6, 7, 8, 9, 0xff, 0xff, 0xff, 0xff };
static const u8 ch_map11[] = { 3, 2, 1, 5, 0, 4, 6, 7, 8, 9, 10, 0xff, 0xff, 0xff };
static const u8 ch_map12[] = { 3, 2, 1, 5, 0, 4, 6, 7, 8, 9, 10, 11, 0xff, 0xff };
static const u8 *const ch_map[] = { ch_map4, ch_map5, ch_map6, ch_map7, ch_map8, ch_map9, ch_map10, ch_map11, ch_map12 };

u8 packet[16];
u8 channels[23];
u8 chidx;
u8 sop_col;
u8 data_col;
u16 state;
u8 crcidx;
#ifdef USE_FIXED_MFGID
// static const u8 cyrfmfg_id[6] = {0x5e, 0x28, 0xa3, 0x1b, 0x00, 0x00}; //dx8
static const u8 cyrfmfg_id[6] = { 0xd4, 0x62, 0xd6, 0xad, 0xd3, 0xff }; // dx6i
#else
static u8 cyrfmfg_id[6];
#endif
u8 num_channels;
u16 crc;
u8 model;

static void build_bind_packet()
{
    u8 i;
    u16 sum = 384 - 0x10;

    packet[0] = crc >> 8;
    packet[1] = crc & 0xff;
    packet[2] = 0xff ^ cyrfmfg_id[2];
    packet[3] = (0xff ^ cyrfmfg_id[3]) + model;
    packet[4] = packet[0];
    packet[5] = packet[1];
    packet[6] = packet[2];
    packet[7] = packet[3];
    for (i = 0; i < 8; i++) {
        sum += packet[i];
    }
    packet[8]  = sum >> 8;
    packet[9]  = sum & 0xff;
    packet[10] = 0x01; // ???
    packet[11] = num_channels;
    if (Model.protocol == PROTOCOL_DSMX) {
        packet[12] = num_channels < 8 ? 0xb2 : 0xb2;
    } else {
        packet[12] = num_channels < 8 ? 0x01 : 0x02;
    }
    packet[13] = 0x00; // ???
    for (i = 8; i < 14; i++) {
        sum += packet[i];
    }
    packet[14] = sum >> 8;
    packet[15] = sum & 0xff;
}

static void build_data_packet(u8 upper)
{
    u8 i;
    const u8 *chmap = ch_map[num_channels - 4];

    if (Model.protocol == PROTOCOL_DSMX) {
        packet[0] = cyrfmfg_id[2];
        packet[1] = cyrfmfg_id[3] + model;
    } else {
        packet[0] = (0xff ^ cyrfmfg_id[2]);
        packet[1] = (0xff ^ cyrfmfg_id[3]) + model;
    }
    u8 bits     = Model.protocol == PROTOCOL_DSMX ? 11 : 10;
    u16 max     = 1 << bits;
    u16 pct_100 = (u32)max * 100 / 150;
    for (i = 0; i < 7; i++) {
        s32 value;
        if (chmap[upper * 7 + i] == 0xff) {
            value = 0xffff;
        } else {
            value = (s32)Channels[chmap[upper * 7 + i]] * (pct_100 / 2) / CHAN_MAX_VALUE + (max / 2);
            if (value >= max) {
                value = max - 1;
            } else if (value < 0) {
                value = 0;
            }
            value = (upper && i == 0 ? 0x8000 : 0) | (chmap[upper * 7 + i] << bits) | value;
        }
        packet[i * 2 + 2] = (value >> 8) & 0xff;
        packet[i * 2 + 3] = (value >> 0) & 0xff;
    }
}

static u8 get_pn_row(u8 channel)
{
    return Model.protocol == PROTOCOL_DSMX
           ? (channel - 2) % 5
           : channel % 5;
}

static const u8 init_vals[][2] = {
    { CYRF_1D_MODE_OVERRIDE,  0x01     },
    { CYRF_28_CLK_EN,         0x02     },
    { CYRF_32_AUTO_CAL_TIME,  0x3c     },
    { CYRF_35_AUTOCAL_OFFSET, 0x14     },
    { CYRF_0D_IO_CFG,         0x04     }, // From Devo - Enable PACTL as GPIO
    { CYRF_0E_GPIO_CTRL,      0x20     }, // From Devo
    { CYRF_06_RX_CFG,         0x48     },
    { CYRF_1B_TX_OFFSET_LSB,  0x55     },
    { CYRF_1C_TX_OFFSET_MSB,  0x05     },
    { CYRF_0F_XACT_CFG,       0x24     },
    { CYRF_03_TX_CFG,         0x38 | 7 },
    { CYRF_12_DATA64_THOLD,   0x0a     },
    { CYRF_0C_XTAL_CTRL,      0xC0     }, // From Devo - Enable XOUT as GPIO
    { CYRF_0F_XACT_CFG,       0x04     },
    { CYRF_39_ANALOG_CTRL,    0x01     },
    { CYRF_0F_XACT_CFG,       0x24     }, // Force IDLE
    { CYRF_29_RX_ABORT,       0x00     }, // Clear RX abort
    { CYRF_12_DATA64_THOLD,   0x0a     }, // set pn correlation threshold
    { CYRF_10_FRAMING_CFG,    0x4a     }, // set sop len and threshold
    { CYRF_29_RX_ABORT,       0x0f     }, // Clear RX abort?
    { CYRF_03_TX_CFG,         0x38 | 7 }, // Set 64chip, SDE mode, max-power
    { CYRF_10_FRAMING_CFG,    0x4a     }, // set sop len and threshold
    { CYRF_1F_TX_OVERRIDE,    0x04     }, // disable tx CRC
    { CYRF_1E_RX_OVERRIDE,    0x14     }, // disable rx crc
    { CYRF_14_EOP_CTRL,       0x02     }, // set EOP sync == 2
    { CYRF_01_TX_LENGTH,      0x10     }, // 16byte packet
};

static void cyrf_config()
{
    for (u32 i = 0; i < sizeof(init_vals) / 2; i++) {
        CYRF_WriteRegister(init_vals[i][0], init_vals[i][1]);
    }
    CYRF_WritePreamble(0x333304);
    CYRF_ConfigRFChannel(0x61);
}

void initialize_bind_state()
{
    u8 data_code[32];

    CYRF_ConfigRFChannel(BIND_CHANNEL); // This seems to be random?
    u8 pn_row = get_pn_row(BIND_CHANNEL);
    // printf("Ch: %d Row: %d SOP: %d Data: %d\n", BIND_CHANNEL, pn_row, sop_col, data_col);
    CYRF_ConfigCRCSeed(crc);
    CYRF_ConfigSOPCode(pncodes[pn_row][sop_col]);
    memcpy(data_code, pncodes[pn_row][data_col], 16);
    memcpy(data_code + 16, pncodes[0][8], 8);
    memcpy(data_code + 24, pn_bind, 8);
    CYRF_ConfigDataCode(data_code, 32);
    build_bind_packet();
}

static const u8 data_vals[][2] = {
    { CYRF_05_RX_CTRL,      0x83     }, // Initialize for reading RSSI
    { CYRF_29_RX_ABORT,     0x20     },
    { CYRF_0F_XACT_CFG,     0x24     },
    { CYRF_29_RX_ABORT,     0x00     },
    { CYRF_03_TX_CFG,       0x08 | 7 },
    { CYRF_10_FRAMING_CFG,  0xea     },
    { CYRF_1F_TX_OVERRIDE,  0x00     },
    { CYRF_1E_RX_OVERRIDE,  0x00     },
    { CYRF_03_TX_CFG,       0x28 | 7 },
    { CYRF_12_DATA64_THOLD, 0x3f     },
    { CYRF_10_FRAMING_CFG,  0xff     },
    { CYRF_0F_XACT_CFG,     0x24     }, // Switch from reading RSSI to Writing
    { CYRF_29_RX_ABORT,     0x00     },
    { CYRF_12_DATA64_THOLD, 0x0a     },
    { CYRF_10_FRAMING_CFG,  0xea     },
};

static void cyrf_configdata()
{
    for (u32 i = 0; i < sizeof(data_vals) / 2; i++) {
        CYRF_WriteRegister(data_vals[i][0], data_vals[i][1]);
    }
}

static void set_sop_data_crc()
{
    u8 pn_row = get_pn_row(channels[chidx]);

    // printf("Ch: %d Row: %d SOP: %d Data: %d\n", ch[chidx], pn_row, sop_col, data_col);
    CYRF_ConfigRFChannel(channels[chidx]);
    CYRF_ConfigCRCSeed(crcidx ? ~crc : crc);
    CYRF_ConfigSOPCode(pncodes[pn_row][sop_col]);
    CYRF_ConfigDataCode(pncodes[pn_row][data_col], 16);
    /* setup for next iteration */
    if (Model.protocol == PROTOCOL_DSMX) {
        chidx = (chidx + 1) % 23;
    } else {
        chidx = (chidx + 1) % 2;
    }
    crcidx = !crcidx;
}

static void calc_dsmx_channel()
{
    int idx    = 0;
    u32 id     = ~((cyrfmfg_id[0] << 24) | (cyrfmfg_id[1] << 16) | (cyrfmfg_id[2] << 8) | (cyrfmfg_id[3] << 0));
    u32 id_tmp = id;

    while (idx < 23) {
        int i;
        int count_3_27 = 0, count_28_51 = 0, count_52_76 = 0;
        id_tmp = id_tmp * 0x0019660D + 0x3C6EF35F; // Randomization
        u8 next_ch     = ((id_tmp >> 8) % 0x49) + 3;       // Use least-significant byte and must be larger than 3
        if (((next_ch ^ id) & 0x01) == 0) {
            continue;
        }
        for (i = 0; i < idx; i++) {
            if (channels[i] == next_ch) {
                break;
            }
            if (channels[i] <= 27) {
                count_3_27++;
            } else if (channels[i] <= 51) {
                count_28_51++;
            } else {
                count_52_76++;
            }
        }
        if (i != idx) {
            continue;
        }
        if ((next_ch < 28 && count_3_27 < 8)
            || (next_ch >= 28 && next_ch < 52 && count_28_51 < 7)
            || (next_ch >= 52 && count_52_76 < 8)) {
            channels[idx++] = next_ch;
        }
    }
}

static void parse_telemetry_packet()
{
#if 0
    u32 time_ms = CLOCK_getms();
    switch (packet[0]) {
    case 0x7f: // TM1000 Flight log
    case 0xff: // TM1100 Flight log
        // Telemetry.fadesA = ((s32)packet[2] << 8) | packet[3];
        // Telemetry.fadesB = ((s32)packet[4] << 8) | packet[5];
        // Telemetry.fadesL = ((s32)packet[6] << 8) | packet[7];
        // Telemetry.fadesR = ((s32)packet[8] << 8) | packet[9];
        // Telemetry.frameloss = ((s32)packet[10] << 8) | packet[11];
        // Telemetry.holds = ((s32)packet[12] << 8) | packet[13];
        Telemetry.volt[1] = ((((s32)packet[14] << 8) | packet[15]) + 5) / 10; // In 1/10 of Volts
        Telemetry.time[0] = time_ms;
        Telemetry.time[1] = Telemetry.time[0];
        break;
    case 0x7e: // TM1000
    case 0xfe: // TM1100
        Telemetry.rpm[0] = (packet[2] << 8) | packet[3];
        if ((Telemetry.rpm[0] == 0xffff) || (Telemetry.rpm[0] < 200)) {
            Telemetry.rpm[0] = 0;
        } else {
            Telemetry.rpm[0] = 120000000 / 2 / Telemetry.rpm[0]; // In RPM (2 = number of poles)
        }
        // Telemetry.rpm[0] = 120000000 / number_of_poles(2, 4, ... 32) / gear_ratio(0.01 - 30.99) / Telemetry.rpm[0];
        // by default number_of_poles = 2, gear_ratio = 1.00
        Telemetry.volt[0] = ((((s32)packet[4] << 8) | packet[5]) + 5) / 10; // In 1/10 of Volts
        Telemetry.temp[0] = (((s32)(packet[6] << 8) | packet[7]) - 32) * 5 / 9; // In degrees-C (16Bit signed integer !!!)
        if (Telemetry.temp[0] > 500 || Telemetry.temp[0] < -100) {
            Telemetry.temp[0] = 0;
        }
        Telemetry.time[0] = time_ms;
        Telemetry.time[1] = Telemetry.time[0];
        break;
    case 0x03: // High Current sensor
        // Telemetry.current = (((s32)packet[2] << 8) | packet[3]) * 1967 / 1000; //In 1/10 of Amps (16bit value, 1 unit is 0.1967A)
        // Telemetry.time[x1] = time_ms;
        break;
    case 0x0a: // Powerbox sensor
        // Telemetry.pwb.volt1 = (((s32)packet[2] << 8) | packet[3] + 5) /10; //In 1/10 of Volts
        // Telemetry.pwb.volt1 = (((s32)packet[4] << 8) | packet[5] + 5) /10; //In 1/10 of Volts
        // Telemetry.pwb.capacity1 = ((s32)packet[6] << 8) | packet[7]; //In mAh
        // Telemetry.pwb.capacity2 = ((s32)packet[8] << 8) | packet[9]; //In mAh
        // Telemetry.pwb.alarm_v1 = packet[15] & 0x01; //0 = disable, 1 = enable
        // Telemetry.pwb.alarm_v2 = (packet[15] >> 1) & 0x01; //0 = disable, 1 = enable
        // Telemetry.pwb.alarm_c1 = (packet[15] >> 2) & 0x01; //0 = disable, 1 = enable
        // Telemetry.pwb.alarm_c2 = (packet[15] >> 3) & 0x01; //0 = disable, 1 = enable
        // Telemetry.time[x2] = time_ms;
        break;
    case 0x11: // AirSpeed sensor
        // Telemetry.airspeed = ((s32)packet[2] << 8) | packet[3]; //In km/h (16Bit value, 1 unit is 1 km/h)
        // Telemetry.time[x3] = time_ms;
        break;
    case 0x12: // Altimeter sensor
        // Telemetry.altitude = ((s32)(packet[2] << 8) | packet[3]) /10; //In meters (16Bit signed integer, 1 unit is 0.1m)
        // Telemetry.time[x4] = time_ms;
        break;
    case 0x14: // G-Force sensor
        // Telemetry.gforce.x = (s32)(packet[2] << 8) | packet[3]; //In 0.01g (16Bit signed integers, unit is 0.01g)
        // Telemetry.gforce.y = (s32)(packet[4] << 8) | packet[5];
        // Telemetry.gforce.z = (s32)(packet[6] << 8) | packet[7];
        // Telemetry.gforce.xmax = (s32)(packet[8] << 8) | packet[9];
        // Telemetry.gforce.ymax = (s32)(packet[10] << 8) | packet[11];
        // Telemetry.gforce.zmax = (s32)(packet[12] << 8) | packet[13];
        // Telemetry.gforce.zmin = (s32)(packet[14] << 8) | packet[15];
        // Telemetry.time[x5] = time_ms;
        break;
    case 0x15: // JetCat sensor
        // Telemetry.jc.status = packet[2];
        // Possible messages for status:
        // 0x00:OFF
        // 0x01:WAIT FOR RPM
        // 0x02:IGNITE
        // 0x03;ACCELERATE
        // 0x04:STABILIZE
        // 0x05:LEARN HIGH
        // 0x06:LEARN LOW
        // 0x07:undef
        // 0x08:SLOW DOWN
        // 0x09:MANUAL
        // 0x0a,0x10:AUTO OFF
        // 0x0b,0x11:RUN
        // 0x0c,0x12:ACCELERATION DELAY
        // 0x0d,0x13:SPEED REG
        // 0x0e,0x14:TWO SHAFT REGULATE
        // 0x0f,0x15:PRE HEAT
        // 0x16:PRE HEAT 2
        // 0x17:MAIN F START
        // 0x18:not used
        // 0x19:KERO FULL ON
        // 0x1a:MAX STATE
        // Telemetry.jc.throttle = (packet[3] >> 4) * 10 + (packet[3] & 0x0f); //up to 159% (the upper nibble is 0-f, the lower nibble 0-9)
        // Telemetry.jc.pack_volt = (((packet[4] >> 4) * 10 + (packet[4] & 0x0f)) * 100
        // + (packet[5] >> 4) * 10 + (packet[5] & 0x0f) + 5) / 10; //In 1/10 of Volts
        // Telemetry.jc.pump_volt = (((packet[6] >> 6) * 10 + (packet[6] & 0x0f)) * 100
        // + (packet[7] >> 4) * 10 + (packet[7] & 0x0f) + 5) / 10; //In 1/10 of Volts
        // Telemetry.jc.rpm = ((packet[10] >> 4) * 10 + (packet[10] & 0x0f)) * 10000
        // + ((packet[9] >> 4) * 10 + (packet[9] & 0x0f)) * 100
        // + ((packet[8] >> 4) * 10 + (packet[8] & 0x0f)); //RPM up to 999999
        // Telemetry.jc.tempEGT = (packet[13] & 0x0f) * 100 + (packet[12] >> 4) * 10 + (packet[12] & 0x0f); //EGT temp up to 999�
        // Telemetry.jc.off_condition = packet[14];
        // Messages for Off_Condition:
        // 0x00:NA
        // 0x01:OFF BY RC
        // 0x02:OVER TEMPERATURE
        // 0x03:IGNITION TIMEOUT
        // 0x04:ACCELERATION TIMEOUT
        // 0x05:ACCELERATION TOO SLOW
        // 0x06:OVER RPM
        // 0x07:LOW RPM OFF
        // 0x08:LOW BATTERY
        // 0x09:AUTO OFF
        // 0x0a,0x10:LOW TEMP OFF
        // 0x0b,0x11:HIGH TEMP OFF
        // 0x0c,0x12:GLOW PLUG DEFECTIVE
        // 0x0d,0x13:WATCH DOG TIMER
        // 0x0e,0x14:FAIL SAFE OFF
        // 0x0f,0x15:MANUAL OFF
        // 0x16:POWER BATT FAIL
        // 0x17:TEMP SENSOR FAIL
        // 0x18:FUEL FAIL
        // 0x19:PROP FAIL
        // 0x1a:2nd ENGINE FAIL
        // 0x1b:2nd ENGINE DIFFERENTIAL TOO HIGH
        // 0x1c:2nd ENGINE NO COMMUNICATION
        // 0x1d:MAX OFF CONDITION
        // Telemetry.time[x6] = time_ms;
        break;
    case 0x16: // GPS sensor
        Telemetry.gps.altitude = (((packet[3] >> 4) * 10 + (packet[3] & 0x0f)) * 100 // (16Bit decimal, 1 unit is 0.1m)
                                  + ((packet[2] >> 4) * 10 + (packet[2] & 0x0f))) * 100; // In meters * 1000
        Telemetry.gps.latitude = ((packet[7] >> 4) * 10 + (packet[7] & 0x0f)) * 3600000
                                 + ((packet[6] >> 4) * 10 + (packet[6] & 0x0f)) * 60000
                                 + ((packet[5] >> 4) * 10 + (packet[5] & 0x0f)) * 600
                                 + ((packet[4] >> 4) * 10 + (packet[4] & 0x0f)) * 6;
        if ((packet[15] & 0x01) == 0) {
            Telemetry.gps.latitude *= -1;
        }
        Telemetry.gps.longitude = ((packet[11] >> 4) * 10 + (packet[11] & 0x0f)) * 3600000
                                  + ((packet[10] >> 4) * 10 + (packet[10] & 0x0f)) * 60000
                                  + ((packet[9] >> 4) * 10 + (packet[9] & 0x0f)) * 600
                                  + ((packet[8] >> 4) * 10 + (packet[8] & 0x0f)) * 6;
        if ((packet[15] & 0x04) == 4) {
            Telemetry.gps.longitude += 360000000;
        }
        if ((packet[15] & 0x02) == 0) {
            Telemetry.gps.longitude *= -1;
        }
        // Telemetry.gps.heading = ((packet[13] >> 4) * 10 + (packet[13] & 0x0f)) * 10     //(16Bit decimal, 1 unit is 0.1 degree)
        // + ((packet[12] >> 4) * 10 + (packet[12] & 0x0f)) / 10; //In degrees
        Telemetry.time[2] = time_ms;
        break;
    case 0x17: // GPS sensor
        Telemetry.gps.velocity = (((packet[3] >> 4) * 10 + (packet[3] & 0x0f)) * 100
                                  + ((packet[2] >> 4) * 10 + (packet[2] & 0x0f))) * 5556 / 108; // In m/s * 1000
        u8 hour  = (packet[7] >> 4) * 10 + (packet[7] & 0x0f);
        u8 min   = (packet[6] >> 4) * 10 + (packet[6] & 0x0f);
        u8 sec   = (packet[5] >> 4) * 10 + (packet[5] & 0x0f);
        // u8 ssec   = (packet[4] >> 4) * 10 + (packet[4] & 0x0f);
        u8 day   = 0;
        u8 month = 0;
        u8 year  = 0; // + 2000
        Telemetry.gps.time = ((year & 0x3F) << 26)
                             | ((month & 0x0F) << 22)
                             | ((day & 0x1F) << 17)
                             | ((hour & 0x1F) << 12)
                             | ((min & 0x3F) << 6)
                             | ((sec & 0x3F) << 0);
        // Telemetry.gps.sats = ((packet[8] >> 4) * 10 + (packet[8] & 0x0f));
        Telemetry.time[2] = time_ms;
        break;
    }
#endif /* if 0 */
}

u16 dsm2_cb()
{
#define CH1_CH2_DELAY 4010 // Time between write of channel 1 and channel 2
#define WRITE_DELAY   1550  // Time after write to verify write complete
#define READ_DELAY    400  // Time before write to check read state, and switch channels
    if (state < DSM2_CHANSEL) {
        // Binding
        state += 1;
        if (state & 1) {
            // Send packet on even states
            // Note state has already incremented,
            // so this is actually 'even' state
            CYRF_WriteDataPacket(packet);
            return 8500;
        } else {
            // Check status on odd states
            CYRF_ReadRegister(CYRF_04_TX_IRQ_STATUS);
            return 1500;
        }
    } else if (state < DSM2_CH1_WRITE_A) {
        // Select channels and configure for writing data
        // CYRF_FindBestChannels(ch, 2, 10, 1, 79);
        cyrf_configdata();
        CYRF_ConfigRxTx(1);
        chidx  = 0;
        crcidx = 0;
        state  = DSM2_CH1_WRITE_A;
        // PROTOCOL_SetBindState(0);  //Turn off Bind dialog
        set_sop_data_crc();
        return 10000;
    } else if (state == DSM2_CH1_WRITE_A || state == DSM2_CH1_WRITE_B
               || state == DSM2_CH2_WRITE_A || state == DSM2_CH2_WRITE_B) {
        if (state == DSM2_CH1_WRITE_A || state == DSM2_CH1_WRITE_B) {
            build_data_packet(state == DSM2_CH1_WRITE_B);
        }
        CYRF_WriteDataPacket(packet);
        state++;
        return WRITE_DELAY;
    } else if (state == DSM2_CH1_CHECK_A || state == DSM2_CH1_CHECK_B) {
        while (!(CYRF_ReadRegister(0x04) & 0x02)) {
            ;
        }
        set_sop_data_crc();
        state++;
        return CH1_CH2_DELAY - WRITE_DELAY;
    } else if (state == DSM2_CH2_CHECK_A || state == DSM2_CH2_CHECK_B) {
        while (!(CYRF_ReadRegister(0x04) & 0x02)) {
            ;
        }
        if (state == DSM2_CH2_CHECK_A) {
            // Keep transmit power in sync
            CYRF_WriteRegister(CYRF_03_TX_CFG, 0x28 | Model.tx_power);
        }
        /*if (Model.proto_opts[PROTOOPTS_TELEMETRY] == TELEM_ON) {
            state++;
            CYRF_ConfigRxTx(0); //Receive mode
            CYRF_WriteRegister(0x07, 0x80); //Prepare to receive
            CYRF_WriteRegister(CYRF_05_RX_CTRL, 0x87); //Prepare to receive
            return 11000 - CH1_CH2_DELAY - WRITE_DELAY - READ_DELAY;
           } else */{
            set_sop_data_crc();
            if (state == DSM2_CH2_CHECK_A) {
                if (num_channels < 8) {
                    state = DSM2_CH1_WRITE_A;
                    return 22000 - CH1_CH2_DELAY - WRITE_DELAY;
                }
                state = DSM2_CH1_WRITE_B;
            } else {
                state = DSM2_CH1_WRITE_A;
            }
            return 11000 - CH1_CH2_DELAY - WRITE_DELAY;
        }
    } else if (state == DSM2_CH2_READ_A || state == DSM2_CH2_READ_B) {
        // Read telemetry if needed
        if (CYRF_ReadRegister(0x07) & 0x02) {
            CYRF_ReadDataPacket(packet);
            parse_telemetry_packet();
        }
        if (state == DSM2_CH2_READ_A && num_channels < 8) {
            state = DSM2_CH2_READ_B;
            CYRF_WriteRegister(0x07, 0x80); // Prepare to receive
            CYRF_WriteRegister(CYRF_05_RX_CTRL, 0x87); // Prepare to receive
            return 11000;
        }
        if (state == DSM2_CH2_READ_A) {
            state = DSM2_CH1_WRITE_B;
        } else {
            state = DSM2_CH1_WRITE_A;
        }
        CYRF_ConfigRxTx(1); // Write mode
        set_sop_data_crc();
        return READ_DELAY;
    }
    return 0;
}

void DSM2_Initialize()
{
    // CLOCK_StopTimer();
    CYRF_Reset();
#ifndef USE_FIXED_MFGID
    CYRF_GetMfgData(cyrfmfg_id);
    if (Model.fixed_id) {
        cyrfmfg_id[0] ^= (Model.fixed_id >> 0) & 0xff;
        cyrfmfg_id[1] ^= (Model.fixed_id >> 8) & 0xff;
        cyrfmfg_id[2] ^= (Model.fixed_id >> 16) & 0xff;
        cyrfmfg_id[3] ^= (Model.fixed_id >> 24) & 0xff;
    }
#endif
    cyrf_config();

    if (Model.protocol == PROTOCOL_DSMX) {
        calc_dsmx_channel();
    } else {
        if (RANDOM_CHANNELS) {
            u8 tmpch[10];
            CYRF_FindBestChannels(tmpch, 10, 5, 3, 75);
            u8 idx = rand() % 10;
            channels[0] = tmpch[idx];
            while (1) {
                idx = rand() % 10;
                if (tmpch[idx] != channels[0]) {
                    break;
                }
            }
            channels[1] = tmpch[idx];
        } else {
            channels[0] = (cyrfmfg_id[0] + cyrfmfg_id[2] + cyrfmfg_id[4]
                           + ((Model.fixed_id >> 0) & 0xff) + ((Model.fixed_id >> 16) & 0xff)) % 39 + 1;
            channels[1] = (cyrfmfg_id[1] + cyrfmfg_id[3] + cyrfmfg_id[5]
                           + ((Model.fixed_id >> 8) & 0xff) + ((Model.fixed_id >> 8) & 0xff)) % 40 + 40;
        }
    }
    /*
       channels[0] = 0;
       channels[1] = 0;
       if (Model.fixed_id == 0)
        Model.fixed_id = 0x2b9d2952;
       cyrfmfg_id[0] = 0xff ^ ((Model.fixed_id >> 24) & 0xff);
       cyrfmfg_id[1] = 0xff ^ ((Model.fixed_id >> 16) & 0xff);
       cyrfmfg_id[2] = 0xff ^ ((Model.fixed_id >> 8) & 0xff);
       cyrfmfg_id[3] = 0xff ^ ((Model.fixed_id >> 0) & 0xff);
       printf("DSM2 Channels: %02x %02x\n", channels[0], channels[1]);
     */
    crc          = ~((cyrfmfg_id[0] << 8) + cyrfmfg_id[1]);
    crcidx       = 0;
    sop_col      = (cyrfmfg_id[0] + cyrfmfg_id[1] + cyrfmfg_id[2] + 2) & 0x07;
    data_col     = 7 - sop_col;
    model        = MODEL;
    num_channels = Model.num_channels;
    if (num_channels < 6) {
        num_channels = 6;
    } else if (num_channels > 12) {
        num_channels = 12;
    }
    num_channels = 7;

    CYRF_ConfigRxTx(1);
    if (1) {
        state = DSM2_BIND;
        // PROTOCOL_SetBindState((BIND_COUNT > 200 ? BIND_COUNT / 2 : 200) * 10); //msecs
        initialize_bind_state();
    } else {
        state = DSM2_CHANSEL;
    }
    // CLOCK_StartTimer(10000, dsm2_cb);
}
#endif /* ifdef PROTO_HAS_CYRF6936 */