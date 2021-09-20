/*
* A wrapper utility for using regular expressions in cpp.
* This was adapted from the tvhregex wrapper libary of the Tvheadend project:
*   https://github.com/tvheadend/tvheadend/blob/master/src/tvhregex.h
*
*/

#include "regexp_utils.h"

#define OPENTV_REGEX_MAX_MATCHES    10

/* Compile flags */
#define OPENTV_REGEX_POSIX          1       /* Use POSIX regex engine */
#define OPENTV_REGEX_CASELESS       2       /* Use case-insensitive matching */

int regex_compile(opentv_regex_t *regex, const char *re_str, int flags)
{
#if ENABLE_PCRE || ENABLE_PCRE2
	regex->is_posix = 0;
	if (flags & OPENTV_REGEX_POSIX) {
		regex->is_posix = 1;
#endif
		int options = REG_EXTENDED;
		if (flags & OPENTV_REGEX_CASELESS)
			options |= REG_ICASE;
		if (!regcomp(&regex->re_posix_code, re_str, options))
			return 0;
		printf("Unable to compile regex '%s'", re_str);
		return -1;
#if ENABLE_PCRE || ENABLE_PCRE2
	} else {
#if ENABLE_PCRE
		const char *estr;
		int eoff;
		int options = PCRE_UTF8;
		if (flags & OPENTV_REGEX_CASELESS)
			options |= PCRE_CASELESS;
#if PCRE_STUDY_JIT_COMPILE
		regex->re_jit_stack = NULL;
#endif
		regex->re_extra = NULL;
		regex->re_code = pcre_compile(re_str, options, &estr, &eoff, NULL);
		if (regex->re_code == NULL) {
			printf("Unable to compile PCRE '%s': %s", re_str, estr);
		} else {
			regex->re_extra = pcre_study(regex->re_code,
			PCRE_STUDY_JIT_COMPILE, &estr);
			if (regex->re_extra == NULL && estr)
				printf("Unable to study PCRE '%s': %s", re_str, estr);
			else {
#if PCRE_STUDY_JIT_COMPILE
				regex->re_jit_stack = pcre_jit_stack_alloc(32*1024, 512*1024);
				if (regex->re_jit_stack)
					pcre_assign_jit_stack(regex->re_extra, NULL, regex->re_jit_stack);
#endif
				return 0;
			}
		}
		return -1;
#elif ENABLE_PCRE2
		PCRE2_UCHAR8 ebuf[128];
		int ecode;
		PCRE2_SIZE eoff;
		size_t jsz;
		uint32_t options;
		assert(regex->re_jit_stack == NULL);
		regex->re_jit_stack = NULL;
		regex->re_match = NULL;
		regex->re_mcontext = pcre2_match_context_create(NULL);
		options = PCRE2_UTF;
		if (flags & OPENTV_REGEX_CASELESS)
			options |= PCRE2_CASELESS;
		regex->re_code = pcre2_compile((PCRE2_SPTR8)re_str, -1, options,
					&ecode, &eoff, NULL);
		if (regex->re_code == NULL) {
			(void)pcre2_get_error_message(ecode, ebuf, 120);
			printf("Unable to compile PCRE2 '%s': %s", re_str, ebuf);
		} else {
			regex->re_match = pcre2_match_data_create(OPENTV_REGEX_MAX_MATCHES, NULL);
			if (re_str[0] && pcre2_jit_compile(regex->re_code, PCRE2_JIT_COMPLETE) >= 0) {
				jsz = 0;
				if (pcre2_pattern_info(regex->re_code, PCRE2_INFO_JITSIZE, &jsz) >= 0 && jsz > 0) {
					regex->re_jit_stack = pcre2_jit_stack_create(32 * 1024, 512 * 1024, NULL);
					if (regex->re_jit_stack)
						pcre2_jit_stack_assign(regex->re_mcontext, NULL, regex->re_jit_stack);
				}
			}
			return 0;
		}
		return -1;
#endif
	}
#endif
}

int regex_match(opentv_regex_t *regex, const char *str)
{
#if ENABLE_PCRE || ENABLE_PCRE2
	if (regex->is_posix) {
#endif
		regex->re_posix_text = str;
		return regexec(&regex->re_posix_code, str, OPENTV_REGEX_MAX_MATCHES, regex->re_posix_match, 0);
#if ENABLE_PCRE || ENABLE_PCRE2
	} else {
#if ENABLE_PCRE
		regex->re_text = str;
		regex->re_matches =
			pcre_exec(regex->re_code, regex->re_extra,
				str, strlen(str), 0, 0, regex->re_match, OPENTV_REGEX_MAX_MATCHES * 3);
		return regex->re_matches < 0;
#elif ENABLE_PCRE2
		return pcre2_match(regex->re_code, (PCRE2_SPTR8)str, -1, 0, 0,
			regex->re_match, regex->re_mcontext) <= 0;
#endif
	}
#endif
}

int regex_match_substring(opentv_regex_t *regex, unsigned number, char *buf, size_t size_buf)
{
	assert(buf);
	if (number >= OPENTV_REGEX_MAX_MATCHES)
		return -2;
#if ENABLE_PCRE || ENABLE_PCRE2
	if (regex->is_posix) {
#endif
		if (regex->re_posix_match[number].rm_so == -1)
			return -1;
		ssize_t size = regex->re_posix_match[number].rm_eo - regex->re_posix_match[number].rm_so;
		if (size < 0 || size > (size_buf - 1))
			return -1;
		strlcpy(buf, regex->re_posix_text + regex->re_posix_match[number].rm_so, size + 1);
		return 0;
#if ENABLE_PCRE || ENABLE_PCRE2
	} else {
#if ENABLE_PCRE
		return pcre_copy_substring(regex->re_text, regex->re_match,
				(regex->re_matches == 0)
				? OPENTV_REGEX_MAX_MATCHES
				: regex->re_matches,
				number, buf, size_buf) < 0;
#elif ENABLE_PCRE2
		PCRE2_SIZE psiz = size_buf;
		return pcre2_substring_copy_bynumber(regex->re_match, number, (PCRE2_UCHAR8*)buf, &psiz);
#endif
	}
#endif
}

static void rtrim(char *buf)
{
	size_t len = strlen(buf);
	while (len > 0 && isspace(buf[len - 1]))
		--len;
	buf[len] = '\0';
}

int regex_pattern_apply(char *buf, size_t size_buf, const char *text, const char *re_str)
{
	char matchbuf[2048];
	int matchno;
	opentv_regex_t r;

	assert(buf);
	assert(text);

	if (regex_compile(&r, re_str, 0)) {
		printf("error compiling pattern \"%s\"", re_str);
		return 0;
	}

	if (!regex_match(&r, text) && !regex_match_substring(&r, 1, buf, size_buf)) {
		for (matchno = 2; ; ++matchno) {
			if (regex_match_substring(&r, matchno, matchbuf, sizeof(matchbuf)))
				break;
			size_t len = strlen(buf);
			strlcat(buf, matchbuf, size_buf - len);
		}
		rtrim(buf);
		return 1;
	}
	return 0;
}
