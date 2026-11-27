/*-------------------------------------------------------------------*/
/*  What a ROM file's header says, for the browser's status pane     */
/*-------------------------------------------------------------------*/
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "rom_info.h"
#include "../emulator/input_recording.h"
#include "../emulator/frnes.h"

// names for common boards, so the pane can name them (whether the emulator plays one is frnes_rom_support's answer)
static const struct {
	uint16_t mapper;
	const char* name;
} mappers[] = {
	{ 0, "NROM" },
	{ 1, "MMC1" },
	{ 2, "UxROM" },
	{ 3, "CNROM" },
	{ 4, "MMC3 / MMC6" },
	{ 5, "MMC5" },
	{ 7, "AxROM" },
	{ 9, "MMC2" },
	{ 10, "MMC4" },
	{ 11, "Color Dreams" },
	{ 13, "CPROM" },
	{ 15, "100-in-1 Contra Function 16" },
	{ 16, "Bandai FCG" },
	{ 18, "Jaleco SS88006" },
	{ 19, "Namco 163" },
	{ 21, "Konami VRC4" },
	{ 22, "Konami VRC2" },
	{ 23, "Konami VRC2 / VRC4" },
	{ 24, "Konami VRC6" },
	{ 25, "Konami VRC4 / VRC2" },
	{ 26, "Konami VRC6" },
	{ 28, "Action 53" },
	{ 30, "UNROM 512" },
	{ 32, "Irem G-101" },
	{ 33, "Taito TC0190" },
	{ 34, "BNROM / NINA-001" },
	{ 48, "Taito TC0690" },
	{ 64, "Tengen RAMBO-1" },
	{ 65, "Irem H3001" },
	{ 66, "GxROM" },
	{ 67, "Sunsoft-3" },
	{ 68, "Sunsoft-4" },
	{ 69, "Sunsoft FME-7" },
	{ 71, "Camerica BF909x" },
	{ 73, "Konami VRC3" },
	{ 75, "Konami VRC1" },
	{ 76, "Namco 3446" },
	{ 77, "Irem (Napoleon Senki)" },
	{ 78, "Irem / Jaleco 74HC161" },
	{ 79, "AVE NINA-03 / NINA-06" },
	{ 80, "Taito X1-005" },
	{ 82, "Taito X1-017" },
	{ 85, "Konami VRC7" },
	{ 86, "Jaleco JF-13" },
	{ 87, "Jaleco J87" },
	{ 88, "Namco 3443" },
	{ 89, "Sunsoft-2 (Tenka no Goikenban)" },
	{ 93, "Sunsoft-2" },
	{ 95, "Namco 3425" },
	{ 105, "NES-EVENT" },
	{ 111, "GTROM" },
	{ 118, "TxSROM (MMC3)" },
	{ 119, "TQROM (MMC3)" },
	{ 153, "Bandai LZ93D50 + SRAM" },
	{ 154, "Namco 3453" },
	{ 159, "Bandai LZ93D50 + 24C01" },
	{ 180, "UNROM (Crazy Climber)" },
	{ 184, "Sunsoft-1" },
	{ 206, "Namco 108" },
	{ 207, "Taito X1-005 (one-screen)" },
	{ 210, "Namco 175 / 340" },
	{ 228, "Action 52" },
	{ 232, "Camerica Quattro" },
};
#define NUM_MAPPERS (sizeof(mappers) / sizeof(mappers[0]))

const char* rom_info_mapper_name(int mapper) {
	for (unsigned i = 0; i < NUM_MAPPERS; i++) {
		if (mappers[i].mapper == mapper) {
			return mappers[i].name;
		}
	}
	return NULL;
}

void rom_info_support(const uint8_t* header, uint32_t size, char* text, size_t length) {
	const char* why = NULL;
	// (no size known: the truncation check can't apply)
	int support = frnes_rom_support(header, size ? size : 0xffffffffu, 0, &why);
	if (support == FRNES_ROM_SUPPORTED) {
		snprintf(text, length, "Supported");
	} else if (support == FRNES_ROM_NEEDS_DEVICE) {
		snprintf(text, length, "Runs, but: %s", why ? why : "needs a controller FrNES lacks");
	} else {
		// ("Mapper 188 not supported" reads "Not supported: Mapper 188")
		static const char tail[] = " not supported";
		size_t why_length = why ? strlen(why) : 0;
		if ((why_length > sizeof(tail) - 1) && (strcmp(why + why_length - (sizeof(tail) - 1), tail) == 0)) {
			why_length -= sizeof(tail) - 1;
		}
		snprintf(text, length, why_length ? "Not supported: %.*s" : "Not supported", (int)why_length, why);
	}
}

// the lines being written, and a line to add things to, " | " between them
typedef struct {
	char (*lines)[ROM_INFO_LENGTH];
	int count;
	int max;
} Lines;

static void line_add(Lines* out, const char* format, ...) {
	if (out -> count >= out -> max) {
		return;
	}
	va_list args;
	va_start(args, format);
	vsnprintf(out -> lines[out -> count++], ROM_INFO_LENGTH, format, args);
	va_end(args);
}

static void line_append(char* line, const char* format, ...) {
	size_t used = strlen(line);
	if (used + 4 >= ROM_INFO_LENGTH) {
		return;
	}
	if (used > 0) {
		strcpy(line + used, " | ");
		used += 3;
	}
	va_list args;
	va_start(args, format);
	vsnprintf(line + used, ROM_INFO_LENGTH - used, format, args);
	va_end(args);
}

// a size in bytes as "512 bytes", "8 kB" or "1.5 MB"
static void format_size(char* text, size_t length, uint32_t bytes) {
	if (bytes < 1024) {
		snprintf(text, length, "%lu bytes", (unsigned long)bytes);
	} else if ((bytes < 1024 * 1024) || (bytes % (1024 * 1024) != 0 && bytes % (512 * 1024) != 0)) {
		snprintf(text, length, "%lu kB", (unsigned long)(bytes / 1024));
	} else if (bytes % (1024 * 1024) == 0) {
		snprintf(text, length, "%lu MB", (unsigned long)(bytes / (1024 * 1024)));
	} else {
		snprintf(text, length, "%lu.5 MB", (unsigned long)(bytes / (1024 * 1024)));
	}
}

// an NES 2.0 RAM size nibble: 0 none, else 64 << n bytes
static uint32_t nes2_ram(int shift) {
	return shift ? (64u << shift) : 0;
}

// an NES 2.0 ROM size: the low byte, with its nibble of byte 9; a nibble of $F is the exponent-multiplier form
static uint32_t nes2_rom(uint8_t low, int nibble, uint32_t unit) {
	if (nibble == 0x0f) {
		return (1u << (low >> 2)) * ((low & 3) * 2 + 1);
	}
	return ((uint32_t)nibble << 8 | low) * unit;
}

static void nes_lines(const uint8_t* header, int got, uint32_t size, Lines* out) {
	if ((got < 16) || (memcmp(header, "NES\x1a", 4) != 0)) {
		line_add(out, "Not an iNES file (no \"NES\" header)");
		return;
	}
	int nes2 = (header[7] & 0x0c) == 0x08;
	// (an archaic header, e.g. an old dump tool's name "DiskDude!" in bytes 7-15: the loader ignores bytes 7-15)
	int junk = ((header[7] & 0x0c) == 0x04) || ((header[7] & 0x0c) == 0x0c);
	int mapper = (header[6] >> 4) | (junk ? 0 : (header[7] & 0xf0)) | (nes2 ? ((header[8] & 0x0f) << 8) : 0);
	char line[ROM_INFO_LENGTH];

	// the board
	const char* name = rom_info_mapper_name(mapper);
	snprintf(line, sizeof(line), "Mapper %d%s%s", mapper, name ? ": " : "", name ? name : "");
	if (nes2 && (header[8] >> 4)) {
		size_t used = strlen(line);
		snprintf(line + used, sizeof(line) - used, ", submapper %d", header[8] >> 4);
	}
	// whether it plays, by the emulator loader's own rules
	char support[ROM_INFO_LENGTH];
	rom_info_support(header, size, support, sizeof(support));
	line_append(line, "%s", support);
	line_add(out, "%s", line);

	// the memory
	uint32_t prg = nes2 ? nes2_rom(header[4], header[9] & 0x0f, 16384) : header[4] * 16384u;
	uint32_t chr = nes2 ? nes2_rom(header[5], header[9] >> 4, 8192) : header[5] * 8192u;
	char text[24];
	line[0] = '\0';
	format_size(text, sizeof(text), prg);
	line_append(line, "PRG ROM %s", text);
	if (chr) {
		format_size(text, sizeof(text), chr);
		line_append(line, "CHR ROM %s", text);
	}
	if (nes2) {
		uint32_t chr_ram = nes2_ram(header[11] & 0x0f) + nes2_ram(header[11] >> 4);
		uint32_t prg_ram = nes2_ram(header[10] & 0x0f);
		uint32_t prg_nvram = nes2_ram(header[10] >> 4);
		if (chr_ram) {
			format_size(text, sizeof(text), chr_ram);
			line_append(line, "CHR RAM %s", text);
		}
		if (prg_ram) {
			format_size(text, sizeof(text), prg_ram);
			line_append(line, "PRG RAM %s", text);
		}
		if (prg_nvram) {
			format_size(text, sizeof(text), prg_nvram);
			line_append(line, "save RAM %s", text);
		}
	} else if (!chr) {
		line_append(line, "CHR RAM");
	}
	line_add(out, "%s", line);

	// the cartridge
	line[0] = '\0';
	if (header[6] & 0x08) {
		line_append(line, "Four-screen");
	} else {
		line_append(line, "%s mirroring", (header[6] & 0x01) ? "Vertical" : "Horizontal");
	}
	if (header[6] & 0x02) {
		line_append(line, "Battery save");
	}
	if (header[6] & 0x04) {
		line_append(line, "512-byte trainer");
	}
	static const char* const regions[] = { "NTSC", "PAL", "NTSC / PAL", "Dendy" };
	if (nes2) {
		line_append(line, "%s", regions[header[12] & 3]);
	} else if (!junk && (header[9] & 0x01)) {
		line_append(line, "PAL");
	}
	static const char* const consoles[] = { NULL, "VS. System", "PlayChoice-10", "Extended console" };
	if (!junk && consoles[header[7] & 3]) {
		line_append(line, "%s", consoles[header[7] & 3]);
	}
	line_add(out, "%s", line);

	// the file
	line[0] = '\0';
	line_append(line, "%s", nes2 ? "NES 2.0 header" : (junk ? "iNES header (junk in bytes 7-15)" : "iNES header"));
	if (size) {
		uint32_t expected = 16 + ((header[6] & 0x04) ? 512 : 0) + prg + chr;
		if (size == expected) {
			format_size(text, sizeof(text), size);
			line_append(line, "file %s", text);
		} else {
			line_append(line, "file %lu bytes, the header says %lu", (unsigned long)size, (unsigned long)expected);
		}
	}
	line_add(out, "%s", line);
}

static void fds_lines(const uint8_t* header, int got, uint32_t size, Lines* out) {
	char line[ROM_INFO_LENGTH] = "";
	// (a fwNES header, 16 bytes, gives the sides; without one, a side is 65500 bytes)
	int fwnes = (got >= 16) && (memcmp(header, "FDS\x1a", 4) == 0);
	int sides = fwnes ? header[4] : (int)(size / 65500);
	line_append(line, "Famicom Disk System");
	if (sides) {
		line_append(line, "%d disk side%s", sides, (sides == 1) ? "" : "s");
	}
	line_append(line, "%s", fwnes ? "fwNES header" : "no header");
	line_add(out, "%s", line);

	// side A's disk info block
	int info = fwnes ? 16 : 0;
	if ((got >= info + 0x22) && (memcmp(&header[info], "\x01*NINTENDO-HVC*", 15) == 0)) {
		const uint8_t* block = &header[info];
		line[0] = '\0';
		char code[4];
		for (int i = 0; i < 3; i++) {
			code[i] = ((block[0x10 + i] >= 0x20) && (block[0x10 + i] < 0x7f)) ? (char)block[0x10 + i] : '?';
		}
		code[3] = '\0';
		line_append(line, "Game code %s", code);
		line_append(line, "revision %d", block[0x14]);
		if (block[0x0f] == 0x01) {
			line_append(line, "Nintendo");
		} else {
			line_append(line, "maker $%02X", block[0x0f]);
		}
		// (the manufacturing date in BCD: the year in the Showa or Heisei era, or now and then the Gregorian one)
		int year = (block[0x1f] >> 4) * 10 + (block[0x1f] & 0x0f);
		int month = (block[0x20] >> 4) * 10 + (block[0x20] & 0x0f);
		int day = (block[0x21] >> 4) * 10 + (block[0x21] & 0x0f);
		if ((month >= 1) && (month <= 12) && (day >= 1) && (day <= 31)) {
			year += (year >= 80) ? 1900 : ((year >= 58) ? 1925 : 1988);
			line_append(line, "made %04d-%02d-%02d", year, month, day);
		}
		line_add(out, "%s", line);
	} else {
		line_add(out, "No disk info block where it should be");
	}

	line[0] = '\0';
	if (size) {
		char text[24];
		format_size(text, sizeof(text), size);
		line_append(line, "File %s", text);
	}
	line_append(line, "needs disksys.rom");
	line_add(out, "%s", line);
}

static void nsf_lines(const uint8_t* header, int got, Lines* out) {
	if ((got < 128) || (memcmp(header, "NESM\x1a", 5) != 0)) {
		line_add(out, "Not an NSF file (no \"NESM\" header)");
		return;
	}
	char line[ROM_INFO_LENGTH] = "";
	// (title, artist and copyright: 32 bytes each, not always terminated)
	line_add(out, "%.32s", (const char*)&header[0x0e]);
	line_append(line, "%.32s", (const char*)&header[0x2e]);
	if (header[0x4e]) {
		line_append(line, "%.32s", (const char*)&header[0x4e]);
	}
	line_add(out, "%s", line);

	line[0] = '\0';
	line_append(line, "%d song%s, from %d", header[6], (header[6] == 1) ? "" : "s", header[7]);
	static const char* const regions[] = { "NTSC", "PAL", "NTSC / PAL", "NTSC / PAL" };
	line_append(line, "%s", regions[header[0x7a] & 3]);
	static const char* const chips[] = { "VRC6", "VRC7", "FDS", "MMC5", "Namco 163", "Sunsoft 5B" };
	for (int chip = 0; chip < 6; chip++) {
		if (header[0x7b] & (1 << chip)) {
			line_append(line, "%s", chips[chip]);
		}
	}
	line_add(out, "%s", line);

	line[0] = '\0';
	line_append(line, "NSF version %d", header[5]);
	line_append(line, "load $%04X init $%04X play $%04X", header[8] | (header[9] << 8), header[10] | (header[11] << 8),
		header[12] | (header[13] << 8));
	int banked = 0;
	for (int i = 0x70; i < 0x78; i++) {
		banked |= header[i];
	}
	if (banked) {
		line_append(line, "bank switched");
	}
	line_add(out, "%s", line);
}

static void movie_lines(const uint8_t* header, int got, uint32_t size, Lines* out) {
	char line[ROM_INFO_LENGTH] = "";
	if ((got >= 40) && (memcmp(header, "FRNESINP", 8) == 0)) {
		// (input_recording.h's header: magic, version, header size, the ROM's CRC32, frames, runs, flags)
		uint32_t crc = header[16] | (header[17] << 8) | (header[18] << 16) | ((uint32_t)header[19] << 24);
		uint32_t frames = header[20] | (header[21] << 8) | (header[22] << 16) | ((uint32_t)header[23] << 24);
		uint32_t seconds = (uint32_t)(frames / 60.0988f);
		uint32_t flags = header[28] | (header[29] << 8) | (header[30] << 16) | ((uint32_t)header[31] << 24);
		line_add(out, "FrNES movie, version %d", header[8]);
		// the game it was recorded on (the ROM info block's name, right after the 40 byte header) and its ROM's CRC32
		if ((flags & INPUT_RECORDING_FLAG_ROM_INFO) && (got >= 40 + 64) && header[40]) {
			char name[64];
			memcpy(name, &header[40], sizeof(name));
			name[sizeof(name) - 1] = '\0';
			line_add(out, "Game: %s (%08lX)", name, (unsigned long)crc);
		} else {
			line_add(out, "Game: CRC32 %08lX", (unsigned long)crc);
		}
		line_append(line, "%lu frames (%lu:%02lu)", (unsigned long)frames, (unsigned long)(seconds / 60), (unsigned long)(seconds % 60));
		line_add(out, "%s", line);
	} else if ((got >= 7) && (memcmp(header, "version", 7) == 0)) {
		line_add(out, "FCEUX movie (.fm2): not supported");
	} else if ((got >= 4) && (memcmp(header, "PK\x03\x04", 4) == 0)) {
		line_add(out, "BizHawk movie (.bk2)");
	} else {
		line_add(out, "Not a movie this can read");
	}
	if (size) {
		char text[24];
		format_size(text, sizeof(text), size);
		line_add(out, "File %s | A plays it over its game", text);
	}
}

int rom_info_lines(Rom_Kind kind, const uint8_t* header, int got, uint32_t size, char lines[][ROM_INFO_LENGTH], int max_lines) {
	Lines out = { lines, 0, max_lines };
	switch (kind) {
		case ROM_KIND_NSF:
			nsf_lines(header, got, &out);
			break;
		case ROM_KIND_FDS:
			fds_lines(header, got, size, &out);
			break;
		case ROM_KIND_MOVIE:
			movie_lines(header, got, size, &out);
			break;
		default:
			nes_lines(header, got, size, &out);
			break;
	}
	return out.count;
}
