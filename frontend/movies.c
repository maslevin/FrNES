/*===================================================================*/
/*                                                                   */
/*  movies.c : movies from the browser, over their games             */
/*                                                                   */
/*===================================================================*/

#include "movies.h"
#include "frontend.h"
#include "dialog.h"
#include "options.h"
#include "../emulator/movie.h"
#include "../emulator/input_recording.h"
#include "../emulator/cartridge.h"
#include "../emulator/sd_card.h"

#include <kos.h>
#include <strings.h>

// a .bk2 is made into a .frm for the emulator here (KOS's RAM disk)
#define MOVIE_FRM_TEMP "/ram/movie.frm"
// the movie's folder: how many ROMs are looked through for its game
#define MAX_SCAN 64
#define MAX_ROM_SIZE (4 * 1024 * 1024)

typedef struct {
	char path[256];
	int bk2;
	// .frm: the ROM's CRC32 (PRG + CHR without the iNES header, cartridge.c's currentCRC32); .bk2: its SHA1 (the same
	// bytes), as the movie's Header.txt has it
	uint32_t crc32;
	char sha1[48];
} Movie_Game;

static Movie_Game pending;
static int pending_active = 0;

/*-------------------------------------------------------------------*/
/*  SHA1 (FIPS 180-1), for a .bk2's game                             */
/*-------------------------------------------------------------------*/
typedef struct {
	uint32_t h[5];
	uint64_t length;
	uint8_t block[64];
	uint32_t used;
} Sha1;

#define ROL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static void sha1_block(Sha1* s, const uint8_t* p) {
	uint32_t w[80];
	for (int i = 0; i < 16; i++) {
		w[i] = ((uint32_t)p[4 * i] << 24) | ((uint32_t)p[4 * i + 1] << 16) | ((uint32_t)p[4 * i + 2] << 8) | p[4 * i + 3];
	}
	for (int i = 16; i < 80; i++) {
		w[i] = ROL(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
	}
	uint32_t a = s -> h[0], b = s -> h[1], c = s -> h[2], d = s -> h[3], e = s -> h[4];
	for (int i = 0; i < 80; i++) {
		uint32_t f, k;
		if (i < 20) {
			f = (b & c) | (~b & d);
			k = 0x5a827999;
		} else if (i < 40) {
			f = b ^ c ^ d;
			k = 0x6ed9eba1;
		} else if (i < 60) {
			f = (b & c) | (b & d) | (c & d);
			k = 0x8f1bbcdc;
		} else {
			f = b ^ c ^ d;
			k = 0xca62c1d6;
		}
		uint32_t t = ROL(a, 5) + f + e + k + w[i];
		e = d;
		d = c;
		c = ROL(b, 30);
		b = a;
		a = t;
	}
	s -> h[0] += a;
	s -> h[1] += b;
	s -> h[2] += c;
	s -> h[3] += d;
	s -> h[4] += e;
}

static void sha1_hex(const uint8_t* data, uint32_t size, char* hex) {
	Sha1 s = { { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0 }, 0, { 0 }, 0 };
	uint32_t at = 0;
	for (; at + 64 <= size; at += 64) {
		sha1_block(&s, data + at);
	}
	uint8_t last[128];
	uint32_t rest = size - at;
	memset(last, 0, sizeof(last));
	memcpy(last, data + at, rest);
	last[rest] = 0x80;
	uint32_t blocks = (rest + 9 <= 64) ? 1 : 2;
	uint64_t bits = (uint64_t)size * 8;
	for (int i = 0; i < 8; i++) {
		last[blocks * 64 - 1 - i] = (uint8_t)(bits >> (8 * i));
	}
	for (uint32_t i = 0; i < blocks; i++) {
		sha1_block(&s, last + 64 * i);
	}
	for (int i = 0; i < 5; i++) {
		sprintf(hex + 8 * i, "%08lX", (unsigned long)s.h[i]);
	}
}

/*-------------------------------------------------------------------*/
/*  Games and movies                                                 */
/*-------------------------------------------------------------------*/
static int has_extension(const char* path, const char* extension) {
	const char* dot = strrchr(path, '.');
	return (dot != NULL) && (strcasecmp(dot, extension) == 0);
}

// a ROM's CRC32 and SHA1s: without its iNES header (BizHawk's, and FrNES's CRC32) and of the whole file (older
// BizHawk, 2.3's, hashed the file); 0 when it was read
static int rom_ids(const char* path, uint32_t* crc32, char* sha1, char* sha1_file) {
	file_t handle = fs_open(path, O_RDONLY);
	if (handle == -1) {
		return -1;
	}
	uint32_t size = fs_total(handle);
	uint8_t* data = ((size > 16) && (size <= MAX_ROM_SIZE)) ? malloc(size) : NULL;
	int ok = (data != NULL) && (fs_read(handle, data, size) == (ssize_t)size) && (memcmp(data, "NES\x1a", 4) == 0);
	fs_close(handle);
	if (ok) {
		*crc32 = input_recording_crc32(data + 16, size - 16);
		if (sha1) {
			sha1_hex(data + 16, size - 16, sha1);
		}
		if (sha1_file) {
			sha1_hex(data, size, sha1_file);
		}
	}
	free(data);
	return ok ? 0 : -1;
}

// whether a ROM is the movie's game (an FDS image can't be told here: the emulator's own check, by the CRC32 of the
// disk it makes of it, says)
static int rom_matches(const Movie_Game* game, const char* rom_path) {
	if (has_extension(rom_path, ".fds")) {
		return !game -> bk2;
	}
	uint32_t crc32;
	char sha1[48], sha1_file[48];
	if (rom_ids(rom_path, &crc32, game -> bk2 ? sha1 : NULL, game -> bk2 ? sha1_file : NULL) != 0) {
		return 0;
	}
	return game -> bk2 ? ((strcasecmp(sha1, game -> sha1) == 0) || (strcasecmp(sha1_file, game -> sha1) == 0)) :
		(crc32 == game -> crc32);
}

// what the movie needs: 0 when it's a movie this can play
static int movie_game(const char* movie_path, Movie_Game* game) {
	memset(game, 0, sizeof(*game));
	snprintf(game -> path, sizeof(game -> path), "%s", movie_path);
	game -> bk2 = has_extension(movie_path, ".bk2");
	if (game -> bk2) {
		return movie_bk2_to_frm(movie_path, 0, NULL, game -> sha1, sizeof(game -> sha1));
	}
	input_recording_header_t header;
	file_t handle = fs_open(movie_path, O_RDONLY);
	if (handle == -1) {
		return -1;
	}
	int got = fs_read(handle, &header, sizeof(header));
	fs_close(handle);
	if ((got != (int)sizeof(header)) || (memcmp(header.magic, "FRNESINP", 8) != 0)) {
		return -1;
	}
	game -> crc32 = header.rom_crc32;
	return 0;
}

// the notes that find a movie's game: a .frm's by its CRC32 in its folder (recordings leave one in MOVIES_DIRECTORY),
// a .bk2's next to it
static void note_path(const Movie_Game* game, int in_movies_directory, char* path, size_t size) {
	if (game -> bk2) {
		snprintf(path, size, "%s.rom", game -> path);
		return;
	}
	char directory[256];
	snprintf(directory, sizeof(directory), "%s", in_movies_directory ? MOVIES_DIRECTORY : game -> path);
	if (!in_movies_directory) {
		char* slash = strrchr(directory, '/');
		if (slash) {
			*slash = '\0';
		}
	}
	snprintf(path, size, "%s/%08lX.rom", directory, (unsigned long)game -> crc32);
}

static int note_read(const char* note, char* rom, size_t size) {
	file_t handle = fs_open(note, O_RDONLY);
	if (handle == -1) {
		return -1;
	}
	memset(rom, 0, size);
	int got = fs_read(handle, rom, size - 1);
	fs_close(handle);
	if (got <= 0) {
		return -1;
	}
	rom[strcspn(rom, "\r\n")] = '\0';
	return 0;
}

static void note_write(const char* note, const char* rom) {
	// (O_CREAT: the SD card's file system wants it for a new file)
	file_t handle = fs_open(note, O_WRONLY | O_TRUNC | O_CREAT);
	if (handle == -1) {
		return;
	}
	fs_write(handle, rom, strlen(rom));
	fs_write(handle, "\n", 1);
	fs_close(handle);
	sd_card_sync();
}

// the movie's game: the path it was recorded with (a .frm's ROM info), its notes, then the ROMs in its folder; 0 when
// found
static int game_find(const Movie_Game* game, char* rom, size_t size) {
	input_recording_rom_info_t info;
	if (!game -> bk2 && (input_recording_rom_info(game -> path, &info) == 0) && info.path[0]) {
		snprintf(rom, size, "%s", info.path);
		if (rom_matches(game, rom)) {
			return 0;
		}
	}
	char note[300];
	for (int i = 0; i < (game -> bk2 ? 1 : 2); i++) {
		note_path(game, i, note, sizeof(note));
		if ((note_read(note, rom, size) == 0) && rom_matches(game, rom)) {
			return 0;
		}
	}
	char directory[256];
	snprintf(directory, sizeof(directory), "%s", game -> path);
	char* slash = strrchr(directory, '/');
	if (slash == NULL) {
		return -1;
	}
	*slash = '\0';
	file_t handle = fs_open(directory, O_RDONLY | O_DIR);
	if (handle == -1) {
		return -1;
	}
	int found = -1, looked = 0;
	const dirent_t* entry;
	while ((found != 0) && (looked < MAX_SCAN) && ((entry = fs_readdir(handle)) != NULL)) {
		if ((entry -> name[0] == '.') || !has_extension(entry -> name, ".nes")) {
			continue;
		}
		looked++;
		snprintf(rom, size, "%s/%s", directory, entry -> name);
		if (rom_matches(game, rom)) {
			found = 0;
		}
	}
	fs_close(handle);
	return found;
}

// plays the movie over the game: 1 (a dialog when it couldn't)
static int game_play(const Movie_Game* game, const char* rom) {
	const char* frm = game -> path;
	if (game -> bk2) {
		uint32_t crc32 = 0;
		char sha1[48];
		rom_ids(rom, &crc32, NULL, NULL);
		if (movie_bk2_to_frm(game -> path, crc32, MOVIE_FRM_TEMP, sha1, sizeof(sha1)) != 0) {
			const char* lines[2] = { strrchr(game -> path, '/') ? strrchr(game -> path, '/') + 1 : game -> path,
				"The BizHawk movie couldn't be read (NesHawk NTSC movies with standard controllers play)." };
			dialog_show("Unable to play that movie", lines, 2);
			return 1;
		}
		frm = MOVIE_FRM_TEMP;
	}
	int result = launchEmulatorMovie(rom, frm, NULL);
	if (game -> bk2) {
		fs_unlink(MOVIE_FRM_TEMP);
	}
	if (result != 0) {
		const char* name = strrchr(rom, '/');
		const char* lines[2] = { name ? name + 1 : rom, "The movie couldn't be played over this game." };
		dialog_show("Unable to play that movie", lines, 2);
		return 1;
	}
	// (found by its note next time)
	char note[300];
	note_path(game, 0, note, sizeof(note));
	note_write(note, rom);
	return 1;
}

int movies_choose(const char* movie_path) {
	pending_active = 0;
	Movie_Game game;
	if (movie_game(movie_path, &game) != 0) {
		const char* name = strrchr(movie_path, '/');
		const char* lines[2] = { name ? name + 1 : movie_path, game.bk2 ?
			"This BizHawk movie can't be played (NesHawk NTSC movies with standard controllers play)." :
			"It isn't a FrNES movie this can play." };
		dialog_show("Unable to play that movie", lines, 2);
		return 1;
	}
	char rom[256];
	if (game_find(&game, rom, sizeof(rom)) == 0) {
		return game_play(&game, rom);
	}
	pending = game;
	pending_active = 1;
	return 0;
}

const char* movies_pending(void) {
	return pending_active ? pending.path : NULL;
}

int movies_pick(const char* rom_path) {
	if (!pending_active) {
		return 0;
	}
	if (!rom_matches(&pending, rom_path)) {
		const char* name = strrchr(rom_path, '/');
		char wanted[DIALOG_LINE_LENGTH];
		if (pending.bk2) {
			snprintf(wanted, sizeof(wanted), "The movie was made with the ROM whose SHA1 is %s.", pending.sha1);
		} else {
			snprintf(wanted, sizeof(wanted), "The movie was made with the ROM whose CRC32 is %08lX.", (unsigned long)pending.crc32);
		}
		const char* lines[3] = { name ? name + 1 : rom_path, "isn't the movie's game.", wanted };
		dialog_show("Not the movie's game", lines, 3);
		return 1;
	}
	pending_active = 0;
	return game_play(&pending, rom_path);
}

void movies_cancel(void) {
	pending_active = 0;
}

const char* movies_record_directory(void) {
	if (!sd_card_mount()) {
		return NULL;
	}
	fs_mkdir("/sd/FrNES");
	fs_mkdir(MOVIES_DIRECTORY);
	return MOVIES_DIRECTORY;
}

void movies_recorded(const char* rom_path) {
	// (currentCRC32: the game just played, as the recording's header has it)
	char note[300];
	snprintf(note, sizeof(note), "%s/%08lX.rom", MOVIES_DIRECTORY, (unsigned long)currentCRC32);
	note_write(note, rom_path);
}
