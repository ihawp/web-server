#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "string_view.h"

StringView sv(
	char *string
) {
	return (StringView) {
		.string = string,
		.count = strlen(string) 
	};
}

void print_sv_string(
	StringView *sv_string
) {
	printf(SV_fmt, SV_arg(sv_string));
}

void remove_from_left(
	StringView *sv,
	size_t amount
) {
	if (sv->count == 0) return;
	if (amount > sv->count) amount = sv->count;
	sv->string += amount;
	sv->count -= amount;
}

void remove_from_right(
	StringView *sv,
	size_t amount
) {
	if (sv->count == 0) return;
	if (amount > sv->count) amount = sv->count;
	sv->count -= amount;
}

void delim_from_left(
	StringView *sv,
	char delim
) {
	while (sv->string[0] == delim) {
		sv->string += 1;
		sv->count -= 1;
	}	
}

void delim_from_right(
	StringView *sv,
	char delim
) {
	while (sv->string[sv->count - 1] == delim) {
		sv->count -= 1;
	}
}

void trim_by_delim(
	StringView *sv,
	char delim
) {
	delim_from_left(sv, delim);
	delim_from_right(sv, delim);
}

/*
	Returns a StringView from start of string -> first instance of delimeter
	Remaining string (after delimeter) is located in *stv
*/
StringView split_by_delim(	
	StringView *stv,
	char delim
) {
	size_t i = 0;
	while (i < stv->count && stv->string[i] != delim) {
		i += 1;
	}

	if (i < stv->count) {
		StringView item = {
			.string = stv->string,
			.count = i
		};
		remove_from_left(stv, i + 1);
		return item;
	}

	StringView item = *stv;
	remove_from_left(stv, stv->count);
	return item;
}