/*
Copyright 2019 @foostan
Copyright 2020 Drashna Jaelre <@drashna>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "action.h"
#include "action_layer.h"
#include "action_util.h"
#include "keycodes.h"
#include "keymap_us.h"
#include "modifiers.h"
#include "process_key_override.h"
#include "quantum.h"
#include "quantum_keycodes.h"
#include QMK_KEYBOARD_H

#include "keymap_spanish.h"
#include "custom_keys.h"

#define CTL_ESC CTL_T(KC_ESC)
#define CTL_ENT CTL_T(KC_ENT)

#define ES_MDOT S(ES_3) // ·


#define LAYER_STATE_8BIT

enum layers {
    BASE = 0,
    SYM,  // Symbols
    SSYM, // Shift Symbols
    LACC, // Left Accents
    RACC, // Right Accents
    OS,   // Operating System
    MS,   // Mouse
    FN,   // Function keys
};


const key_override_t single_double_quote_override = ko_make_basic(MOD_MASK_SHIFT, ES_QUOT, ES_DQUO);

// Allow normal keypresses after an accent is typed so that i don't have to unpress alt
bool accent_action(bool activated, void* context) {
    del_mods(MOD_MASK_ALT);
    return true;
}

#define ko_make(trigger_key, replacement_key) \
    ((const key_override_t){                                                                \
        .trigger_mods                           = MOD_MASK_ALT,                             \
        .layers                                 = ~0,                                       \
        .suppressed_mods                        = MOD_MASK_ALT,                             \
        .options                                = ko_options_default,                       \
        .negative_mod_mask                      = 0,                                        \
        .custom_action                          = &accent_action,                           \
        .context                                = NULL,                                     \
        .trigger                                = (trigger_key),                            \
        .replacement                            = (replacement_key),                        \
        .enabled                                = NULL                                      \
    })

const key_override_t grave_override =     ko_make(NA_GRV,  ES_GRV);
const key_override_t acute_override =     ko_make(NA_ACUT, ES_ACUT);
const key_override_t diaeresis_override = ko_make(NA_DIAE, ES_DIAE);
const key_override_t cedilla_override =   ko_make(NA_CCED, ES_CCED);
const key_override_t ntilde_override =    ko_make(NA_NTIL, ES_NTIL);

const key_override_t* key_overrides[] = {
    // &single_double_quote_override,
    // &grave_override,
    // &acute_override,
    // &diaeresis_override,
    // &cedilla_override,
    // &ntilde_override,
};


// void keyboard_post_init_user(void) {
//   // Customise these values to desired behaviour
//   // debug_enable=true;
//   // debug_matrix=true;
//   // debug_keyboard=true;
//   // debug_mouse=true;
// }

// bool process_record_user(uint16_t keycode, keyrecord_t *record) {
//     switch (keycode) {
//         case ND_GRV:
//             if (record->event.pressed) {
//                 tap_code16(ES_GRV);
//                 tap_code(KC_SPC);
//             }
//             return false;
//         case ND_CIRC:
//             if (record->event.pressed) {
//                 tap_code16(ES_CIRC);
//                 tap_code(KC_SPC);
//             }
//             return false;
//     }
//     return true;
// }

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [BASE] = LAYOUT_split_3x6_3(
  //,-----------------------------------------------------.                    ,-----------------------------------------------------.
       ES_1,    ES_2,    ES_3,    ES_4,    ES_5,    ES_6,                         ES_7,    ES_8,    ES_9,    ES_0,   KC_MINUS,  KC_EQUAL,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      KC_TAB,    ES_Q,    ES_W,    ES_E,    ES_R,    ES_T,                         ES_U,    ES_I,    ES_O,    ES_P, KC_LBRC, KC_RBRC,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      KC_CAPS,    ES_A,    ES_S,    ES_D,    ES_F,    ES_G,                         ES_J,    ES_K, ES_L,  KC_COLN, KC_QUOTE, KC_RETN,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                               KC_X, KC_C, KC_V, KC_N, KC_M, ES_COMM
                                      //`--------------------------'  `--------------------------'
    ),

};

const uint16_t trans[MATRIX_ROWS][MATRIX_COLS] = LAYOUT_split_3x6_3(
  //,-----------------------------------------------------.                    ,-----------------------------------------------------.
      _______, _______, _______, _______, _______, _______,                      _______, _______, _______, _______, _______, _______,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      _______, _______, _______, _______, _______, _______,                      _______, _______, _______, _______, _______, _______,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      _______, _______, _______, _______, _______, _______,                      _______, _______, _______, _______, _______, _______,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          _______, _______, _______,    _______, _______, _______
                                      //`--------------------------'  `--------------------------'
);

#ifdef ENCODER_MAP_ENABLE
// const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
//   [0] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(KC_MPRV, KC_MNXT), ENCODER_CCW_CW(RM_VALD, RM_VALU), ENCODER_CCW_CW(KC_RGHT, KC_LEFT), },
//   [1] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(KC_MPRV, KC_MNXT), ENCODER_CCW_CW(RM_VALD, RM_VALU), ENCODER_CCW_CW(KC_RGHT, KC_LEFT), },
//   [2] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(KC_MPRV, KC_MNXT), ENCODER_CCW_CW(RM_VALD, RM_VALU), ENCODER_CCW_CW(KC_RGHT, KC_LEFT), },
//   [3] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(KC_MPRV, KC_MNXT), ENCODER_CCW_CW(RM_VALD, RM_VALU), ENCODER_CCW_CW(KC_RGHT, KC_LEFT), },
// };
#endif
