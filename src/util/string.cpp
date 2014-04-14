/*
util/string.cpp
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
*/

/*
This file is part of Freeminer.

Freeminer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Freeminer  is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Freeminer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "string.h"
#include "pointer.h"
#include "numeric.h"

#include <sstream>
#include <iomanip>
#include <cctype>

#include "../sha1.h"
#include "../base64.h"
#include "../hex.h"
#include "../porting.h"

#ifndef _WIN32
#include <iconv.h>
#else
#include <Windows.h>
#endif

#ifndef _WIN32
size_t convert(const char *to, const char *from, char *outbuf, size_t outbuf_size, char *inbuf, size_t inbuf_size) {
	iconv_t cd = iconv_open(to, from);

	char *inbuf_ptr = inbuf;
	char *outbuf_ptr = outbuf;

	size_t *inbuf_left_ptr = &inbuf_size;
	size_t *outbuf_left_ptr = &outbuf_size;

	while (inbuf_size > 0)
		iconv(cd, sloppy<char**>(&inbuf_ptr), inbuf_left_ptr, &outbuf_ptr, outbuf_left_ptr);

	iconv_close(cd);
	return 0;
}

std::wstring utf8_to_wide(const std::string &input) {
	size_t inbuf_size = input.length() + 1;
	// maximum possible size, every character is sizeof(wchar_t) bytes
	size_t outbuf_size = (input.length() + 1) * sizeof(wchar_t);

	char *inbuf = new char[inbuf_size];
	memcpy(inbuf, input.c_str(), inbuf_size);
	char *outbuf = new char[outbuf_size];
	memset(outbuf, 0, outbuf_size);

	convert("WCHAR_T", "UTF-8", outbuf, outbuf_size, inbuf, inbuf_size);
	std::wstring out((wchar_t*)outbuf);

	delete[] inbuf;
	delete[] outbuf;

	return out;
}

std::string wide_to_utf8(const std::wstring &input) {
	size_t inbuf_size = (input.length() + 1) * sizeof(wchar_t);
	// maximum possible size: utf-8 encodes codepoints using 1 up to 6 bytes
	size_t outbuf_size = (input.length() + 1) * 6;

	char *inbuf = new char[inbuf_size];
	memcpy(inbuf, input.c_str(), inbuf_size);
	char *outbuf = new char[outbuf_size];
	memset(outbuf, 0, outbuf_size);

	convert("UTF-8", "WCHAR_T", outbuf, outbuf_size, inbuf, inbuf_size);
	std::string out(outbuf);

	delete[] inbuf;
	delete[] outbuf;

	return out;
}
#else
std::wstring utf8_to_wide(const std::string &input) {
	size_t outbuf_size = input.size() + 1;
	wchar_t *outbuf = new wchar_t[outbuf_size];
	memset(outbuf, 0, outbuf_size * sizeof(wchar_t));
	MultiByteToWideChar(CP_UTF8, 0, input.c_str(), input.size(), outbuf, outbuf_size);
	std::wstring out(outbuf);
	delete[] outbuf;
	return out;
}

std::string wide_to_utf8(const std::wstring &input) {
	size_t outbuf_size = (input.size() + 1) * 6;
	char *outbuf = new char[outbuf_size];
	memset(outbuf, 0, outbuf_size);
	WideCharToMultiByte(CP_UTF8, 0, input.c_str(), input.size(), outbuf, outbuf_size, NULL, NULL);
	std::string out(outbuf);
	delete[] outbuf;
	return out;
}
#endif

std::wstring narrow_to_wide(const std::string& mbs)
{
	size_t wcl = mbs.size();
	Buffer<wchar_t> wcs(wcl+1);
	size_t l = mbstowcs(*wcs, mbs.c_str(), wcl);
	if(l == (size_t)(-1))
		return L"<invalid multibyte string>";
	wcs[l] = 0;
	return *wcs;
}

std::string wide_to_narrow(const std::wstring& wcs)
{
	size_t mbl = wcs.size()*4;
	SharedBuffer<char> mbs(mbl+1);
	size_t l = wcstombs(*mbs, wcs.c_str(), mbl);
	if(l == (size_t)(-1)) {
		return "Character conversion failed!";
	}
	else
		mbs[l] = 0;
	return *mbs;
}

// Get an sha-1 hash of the player's name combined with
// the password entered. That's what the server uses as
// their password. (Exception : if the password field is
// blank, we send a blank password - this is for backwards
// compatibility with password-less players).
std::string translatePassword(std::string playername, std::string password)
{
	if(password.length() == 0)
		return "";

	std::string slt = playername + password;
	SHA1 sha1;
	sha1.addBytes(slt.c_str(), slt.length());
	unsigned char *digest = sha1.getDigest();
	std::string pwd = base64_encode(digest, 20);
	free(digest);
	return pwd;
}

std::string urlencode(std::string str)
{
	// Encodes non-unreserved URI characters by a percent sign
	// followed by two hex digits. See RFC 3986, section 2.3.
	static const char url_hex_chars[] = "0123456789ABCDEF";
	std::ostringstream oss(std::ios::binary);
	for (u32 i = 0; i < str.size(); i++) {
		unsigned char c = str[i];
		if (isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~')
			oss << c;
		else
			oss << "%"
				<< url_hex_chars[(c & 0xf0) >> 4]
				<< url_hex_chars[c & 0x0f];
	}
	return oss.str();
}

std::string urldecode(std::string str)
{
	// Inverse of urlencode
	std::ostringstream oss(std::ios::binary);
	for (u32 i = 0; i < str.size(); i++) {
		unsigned char highvalue, lowvalue;
		if (str[i] == '%' &&
				hex_digit_decode(str[i+1], highvalue) &&
				hex_digit_decode(str[i+2], lowvalue)) {
			oss << (char) ((highvalue << 4) | lowvalue);
			i += 2;
		}
		else
			oss << str[i];
	}
	return oss.str();
}

u32 readFlagString(std::string str, FlagDesc *flagdesc, u32 *flagmask)
{
	if (str.length() == 0)
		return 0;
	u32 result = 0, mask = 0;
	char *s = &str[0];
	char *flagstr, *strpos = NULL;

	while ((flagstr = strtok_r(s, ",", &strpos))) {
		s = NULL;

		while (*flagstr == ' ' || *flagstr == '\t')
			flagstr++;

		bool flagset = true;
		if (!strncasecmp(flagstr, "no", 2)) {
			flagset = false;
			flagstr += 2;
		}

		for (int i = 0; flagdesc[i].name; i++) {
			if (!strcasecmp(flagstr, flagdesc[i].name)) {
				mask |= flagdesc[i].flag;
				if (flagset)
					result |= flagdesc[i].flag;
				break;
			}
		}
	}

	if (flagmask)
		*flagmask = mask;

	return result;
}

std::string writeFlagString(u32 flags, FlagDesc *flagdesc, u32 flagmask)
{
	std::string result;

	for (int i = 0; flagdesc[i].name; i++) {
		if (flagmask & flagdesc[i].flag) {
			if (!(flags & flagdesc[i].flag))
				result += "no";

			result += flagdesc[i].name;
			result += ", ";
		}
	}

	size_t len = result.length();
	if (len >= 2)
		result.erase(len - 2, 2);

	return result;
}

size_t mystrlcpy(char *dst, const char *src, size_t size)
{
	size_t srclen  = strlen(src) + 1;
	size_t copylen = MYMIN(srclen, size);

	if (copylen > 0) {
		memcpy(dst, src, copylen);
		dst[copylen - 1] = '\0';
	}

	return srclen;
}

char *mystrtok_r(char *s, const char *sep, char **lasts)
{
	char *t;

	if (!s)
		s = *lasts;

	while (*s && strchr(sep, *s))
		s++;

	if (!*s)
		return NULL;

	t = s;
	while (*t) {
		if (strchr(sep, *t)) {
			*t++ = '\0';
			break;
		}
		t++;
	}
	
	*lasts = t;
	return s;
}

u64 read_seed(const char *str)
{
	char *endptr;
	u64 num;
	
	if (str[0] == '0' && str[1] == 'x')
		num = strtoull(str, &endptr, 16);
	else
		num = strtoull(str, &endptr, 10);
		
	if (*endptr)
		num = murmur_hash_64_ua(str, (int)strlen(str), 0x1337);
		
	return num;
}

bool parseColor(const std::string &value, video::SColor &color)
{
	const char *hexpattern = NULL;
	if (value[0] == '#') {
		if (value.size() == 9)
			hexpattern = "#RRGGBBAA";
		else if (value.size() == 7)
			hexpattern = "#RRGGBB";
		else if (value.size() == 5)
			hexpattern = "#RGBA";
		else if (value.size() == 4)
			hexpattern = "#RGB";
	}

	if (hexpattern) {
		assert(strlen(hexpattern) == value.size());
		video::SColor outcolor(255, 255, 255, 255);
		for (size_t pos = 0; pos < value.size(); ++pos) {
			// '#' in the pattern means skip that character
			if (hexpattern[pos] == '#')
				continue;

			// Else assume hexpattern[pos] is one of 'R' 'G' 'B' 'A'
			// Read one or two digits, depending on hexpattern
			unsigned char c1, c2;
			if (hexpattern[pos+1] == hexpattern[pos]) {
				// Two digits, e.g. hexpattern == "#RRGGBB"
				if (!hex_digit_decode(value[pos], c1) ||
				    !hex_digit_decode(value[pos+1], c2))
					return false;
				++pos;
			}
			else {
				// One digit, e.g. hexpattern == "#RGB"
				if (!hex_digit_decode(value[pos], c1))
					return false;
				c2 = c1;
			}
			u32 colorpart = ((c1 & 0x0f) << 4) | (c2 & 0x0f);

			// Update outcolor with newly read color part
			if (hexpattern[pos] == 'R')
				outcolor.setRed(colorpart);
			else if (hexpattern[pos] == 'G')
				outcolor.setGreen(colorpart);
			else if (hexpattern[pos] == 'B')
				outcolor.setBlue(colorpart);
			else if (hexpattern[pos] == 'A')
				outcolor.setAlpha(colorpart);
		}

		color = outcolor;
		return true;
	}

	return false;
}


std::wstring colorizeText(const std::wstring &s, std::vector<video::SColor> &colors, const video::SColor &initial_color) {
	std::wstring output;
	colors.clear();
	size_t i = 0;
	video::SColor color = initial_color;
	while (i < s.length()) {
		if (s[i] == L'\v' && i + 6 < s.length()) {
			parseColor("#" + wide_to_utf8(s.substr(i + 1, 6)), color);
			i += 7;
			continue;
		}
		output += s[i];
		colors.push_back(color);

		++i;
	}

	return output;
}

// removes escape sequences
std::wstring sanitizeChatString(const std::wstring &s) {
	std::wstring output;
	size_t i = 0;
	while (i < s.length()) {
		if (s[i] == L'\v') {
			i += 7;
			continue;
		}
		output += s[i];
		++i;
	}
	return output;
}

bool char_icompare(char c1, char c2)
{
	return (std::tolower(static_cast<unsigned char>(c1)) <std::tolower(static_cast<unsigned char>(c2)));
}

bool string_icompare(const std::string& a, const std::string& b)
{
	return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), char_icompare);
}
