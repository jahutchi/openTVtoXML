#include <assert.h>
#include <ctype.h>
#include <regex.h>
#include <stdio.h>
#include <string.h>

#define REGEX_MAX_MATCHES    10

typedef struct {
#if ENABLE_PCRE
	pcre *re_code;
	pcre_extra *re_extra;
	int re_match[REGEX_MAX_MATCHES * 3];
	const char *re_text;
	int re_matches;
#if PCRE_STUDY_JIT_COMPILE
	pcre_jit_stack *re_jit_stack;
#endif
#elif ENABLE_PCRE2
	pcre2_code *re_code;
	pcre2_match_data *re_match;
	pcre2_match_context *re_mcontext;
	pcre2_jit_stack *re_jit_stack;
#endif
	regex_t re_posix_code;
	regmatch_t re_posix_match[REGEX_MAX_MATCHES];
	const char *re_posix_text;
#if ENABLE_PCRE || ENABLE_PCRE2
	int is_posix;
#endif
} opentv_regex_t;

static inline size_t strlcpy(char *dst, const char *src, size_t size)
{
	size_t ret = strlen(src);
	if (size) {
		size_t len = ret >= size ? size - 1 : ret;
		memcpy(dst, src, len);
		dst[len] = '\0';
	}
	return ret;
}

static inline size_t strlcat(char *dst, const char *src, size_t count)
{
	size_t dlen = strlen(dst);
	size_t len = strlen(src);
	size_t res = dlen + len;

	dst += dlen;
	count -= dlen;
	if (len >= count)
		len = count - 1;
	memcpy(dst, src, len);
	dst[len] = '\0';
	return res;
}

int regex_compile(opentv_regex_t *regex, const char *re_str, int flags);
int regex_match(opentv_regex_t *regex, const char *str);
int regex_match_substring(opentv_regex_t *regex, unsigned number, char *buf, size_t size_buf);
int regex_pattern_apply(char *buf, size_t size_buf, const char *text, const char *re_str);
