#pragma once

typedef struct {
	char *string;
	size_t count;
} StringView;

StringView sv(
	char *string
);

void print_sv_string(
	StringView *sv_string
);

void remove_from_left(
	StringView *sv,
	size_t amount
);

void remove_from_right(
	StringView *sv,
	size_t amount
);

void delim_from_left(
	StringView *sv,
	char delim
);

void delim_from_right(
	StringView *sv,
	char delim
);

void trim_by_delim(
	StringView *sv,
	char delim
);

StringView split_by_delim(	
	StringView *stv,
	char delim
);

#define SV_fmt "%.*s"
#define SV_arg(s) (int) (s)->count, (s)->string
#define SV_print(s) printf(SV_fmt"\n", SV_arg(s))
#define SV_to_memory(h, h_size, s) snprintf(h, h_size, SV_fmt, SV_arg(s))