/*===================================================================*/
/*                                                                   */
/*  game_name.c : A game's name as a key, for matching lists to ROMs */
/*                                                                   */
/*===================================================================*/

#include "game_name.h"

#include <ctype.h>
#include <string.h>

/*
 * A game's name for matching a ROM's with the list's: its words in lower case run together, without what's in
 * brackets, a "The" at the start (or after a comma: "Legend of Zelda, The"), "Disney's" or "Part", apostrophes, with
 * II-VI as digits and & as "and"; 'title' stops at a subtitle (':', "--" or " - "), so "Double Dragon II: The Revenge"
 * and "Double Dragon 2 (U)" both have "doubledragon2"
 */
void name_key(const char* name, char* key, size_t size, int title) {
	char words[128];
	size_t used = 0;
	int depth = 0;
	for (const char* c = name; *c && (used < sizeof(words) - 1); c++) {
		if ((*c == '(') || (*c == '[')) {
			depth++;
		} else if (((*c == ')') || (*c == ']')) && (depth > 0)) {
			depth--;
		} else if ((depth == 0) && (*c != '\'')) {
			words[used++] = tolower((unsigned char)*c);
		}
	}
	words[used] = '\0';
	if (title) {
		static const char* const marks[] = { ":", "--", " - " };
		for (int i = 0; i < 3; i++) {
			char* mark = strstr(words, marks[i]);
			if ((mark != NULL) && (mark > words)) {
				*mark = '\0';
			}
		}
	}
	char* the = strstr(words, ", the");
	if (the != NULL) {
		memset(the, ' ', 5);
	}
	static const char* const roman[][2] = { { "ii", "2" }, { "iii", "3" }, { "iv", "4" }, { "v", "5" }, { "vi", "6" } };
	size_t k = 0;
	int first = 1;
	for (char* word = strtok(words, " ,.!?;:-/"); word != NULL; word = strtok(NULL, " ,.!?;:-/")) {
		if ((first && (strcmp(word, "the") == 0)) || (strcmp(word, "disneys") == 0) || (strcmp(word, "part") == 0)) {
			continue;
		}
		first = 0;
		const char* as = word;
		for (int i = 0; i < 5; i++) {
			if (strcmp(word, roman[i][0]) == 0) {
				as = roman[i][1];
			}
		}
		if (strcmp(word, "&") == 0) {
			as = "and";
		}
		for (const char* c = as; *c && (k < size - 1); c++) {
			if (isalnum((unsigned char)*c)) {
				key[k++] = *c;
			} else if ((*c == '&') && (k + 3 < size - 1)) {
				memcpy(key + k, "and", 3);
				k += 3;
			}
		}
	}
	key[k] = '\0';
}

