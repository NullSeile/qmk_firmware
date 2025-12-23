#include "quantum_keycodes.h"

enum custom_keycodes {
    // Non-dead keys that produce a character and then a space
    ND_GRV = SAFE_RANGE,
    ND_CIRC,

    // Non-Alt modfied
    NA_GRV,  // ` (dead)
    NA_ACUT, // ´ (dead)
    NA_DIAE, // ¨ (dead)
    NA_CCED, // ç
    NA_NTIL, // ñ

    OL_PET,  // Pet the cat
};
