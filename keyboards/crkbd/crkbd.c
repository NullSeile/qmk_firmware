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

#ifdef OLED_ENABLE

#if defined(RIGHT_SIDE)
#include "oled/screensaver.c"
// #include "oled/bongo.c"
// #include "oled/scug_batfly.c"
// #include "oled/scug_full.c"
#else
// #include "oled/scug_sleep.c"
// #include "oled/scug_full_v1.c"
#include "oled/scug_full_v5.c"
// #include "oled/scug_full_v2.c"
// #include "oled/scug_full_debug.c"
#endif

#endif // OLED_ENABLE
