/* tools/simplay/data_ld.py: .data at its offset modulo 16 kB in simplay.map */
. = ALIGN(. - 0x40, 0x4000) + 0x40;
