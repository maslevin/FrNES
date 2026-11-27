#ifndef __MOVIES_H
#define __MOVIES_H

/*
 * Movies from the browser (movies.c): a FrNES .frm or a BizHawk .bk2 played over its game, the game found by the
 * movie's ROM CRC32 (.frm) or SHA1 (.bk2): the path a .frm was recorded with (its ROM info block), a note left by an
 * earlier play or recording, else the movie's own folder;
 * when neither has it, the player picks the game in the browser (movies_pending) and movies_pick plays it.
 */
#define MOVIES_DIRECTORY "/sd/FrNES/movies"

// a movie chosen in the browser: 1 when it played (or a dialog says why it couldn't), 0 when its game wasn't found
// and the browser should ask for it
int movies_choose(const char* movie_path);
// the movie waiting for its game (movies_choose returned 0), NULL when none, and the game picked for it (1 when it
// played or a dialog said why not; the movie stays waiting when the game wasn't its game)
const char* movies_pending(void);
int movies_pick(const char* rom_path);
void movies_cancel(void);

// the directory a game is recorded into (START on it in the browser: the SD card's FrNES/movies, made if missing), NULL
// without an SD card; and after it, the note that finds this game for its recordings
const char* movies_record_directory(void);
void movies_recorded(const char* rom_path);

#endif
