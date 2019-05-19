﻿/*
 * Copyright (c) 1997 - 2001 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */
#include <string>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../macros.h"
#include "../simdebug.h"
#include "../simsys.h"
#include "../simtypes.h"
#include "../display/simgraph.h" // for unicode stuff
#include "translator.h"
#include "loadsave.h"
#include "environment.h"
#include "../simmem.h"
#include "../utils/cbuffer_t.h"
#include "../utils/searchfolder.h"
#include "../utils/simstring.h"
#include "../utils/simrandom.h"
#include "../unicode.h"
#include "../tpl/vector_tpl.h"

using std::string;

// allow all kinds of line feeds
static char *fgets_line(char *buffer, int max_len, FILE *file)
{
	char *result = fgets(buffer, max_len, file);
	size_t len = strlen(buffer);
	// remove all trailing junk
	while(  len>1  &&  (buffer[len-1]==13  ||  buffer[len-1]==10)  ) {
		buffer[len-1] = 0;
		len--;
	}
	return result;
}


const char *translator::lang_info::translate(const char *text) const
{
	if(  text    == NULL  ) {
		return "(null)";
	}
	if(  text[0] == '\0'  ) {
		return text;
	}
	const char *trans = texts.get(text);
	return trans != NULL ? trans : text;
}


/* Made to be dynamic, allowing any number of languages to be loaded */
static translator::lang_info langs[40];
static translator::lang_info *current_langinfo = langs;
static stringhashtable_tpl<const char*> compatibility;


translator translator::single_instance;


const translator::lang_info* translator::get_lang()
{
	return current_langinfo;
}


const translator::lang_info* translator::get_langs()
{
	return langs;
}


#ifdef need_dump_hashtable
// diagnosis
static void dump_hashtable(stringhashtable_tpl<const char*>* tbl)
{
	printf("keys\n====\n");
	tbl->dump_stats();
	printf("entries\n=======\n");
	FOR(stringhashtable_tpl<char const*>, const& i, *tbl) {
		printf("%s\n", i.object);
	}
	fflush(NULL);
}
#endif

/* first two file functions needed in connection with utf */

/* checks, if we need a unicode translation (during load only done for identifying strings like "Aufl�sen")
 * @date 2.1.2005
 * @author prissi
 */
static bool is_unicode_file(FILE* f)
{
	unsigned char	str[2];
	int	pos = ftell(f);
//	DBG_DEBUG("is_unicode_file()", "checking for unicode");
//	fflush(NULL);
	fread( str, 1, 2,  f );
//	DBG_DEBUG("is_unicode_file()", "file starts with %x%x",str[0],str[1]);
//	fflush(NULL);
	if (str[0] == 0xC2 && str[1] == 0xA7) {
		// the first line must contain an UTF8 coded paragraph (Latin A7, UTF8 C2 A7), then it is unicode
		DBG_DEBUG("is_unicode_file()", "file is UTF-8");
		return true;
	}
	if(  str[0]==0xEF  &&  str[1]==0xBB   &&  fgetc(f)==0xBF  ) {
		// the first letter is the byte order mark => may need to skip a paragraph (Latin A7, UTF8 C2 A7)
		pos = ftell(f);
		fread( str, 1, 2,  f );
		if(  str[0] != 0xC2  ||  str[1] == 0xA7  ) {
			fseek(f, pos, SEEK_SET);
			dbg->error( "is_unicode_file()", "file is UTF-8 but has no paragraph" );
		}
		DBG_DEBUG("is_unicode_file()", "file is UTF-8");
		return true;
	}
	fseek(f, pos, SEEK_SET);
	return false;
}



// the bytes in an UTF sequence have always the format 10xxxxxx
static inline int is_cont_char(utf8 c) { return (c & 0xC0) == 0x80; }


// recodes string to put them into the tables
static char *recode(const char *src, bool translate_from_utf, bool translate_to_utf, bool is_latin2 )
{
	char *base;
	if(  translate_to_utf != translate_from_utf  ) {
		// worst case
		base = MALLOCN(char, strlen(src) * 2 + 2);
	}
	else {
		base = MALLOCN(char, strlen(src) + 2);
	}
	char *dst = base;
	uint8 c = 0;

	do {
		if (*src =='\\') {
			src +=2;
			*dst++ = c = '\n';
		}
		else {
			c = *src;
			if(c>127) {
				if(  translate_from_utf == translate_to_utf  ) {
					// but copy full letters! (or, if ASCII, copy more than one letter, does not matter
					do {
						*dst++ = *src++;
					} while (is_cont_char(*src));
					c = *src;
				}
				else if(  translate_to_utf  ) {
					if(  !is_latin2  ) {
						// make UTF8 from latin1
						dst += c = utf16_to_utf8((unsigned char)*src++, (utf8*)dst);
					}
					else {
						dst += c = utf16_to_utf8( latin2_to_unicode( (uint8)*src++ ), (utf8*)dst );
					}
				}
				else if(  translate_from_utf  ) {
					// make latin from UTF8 (ignore overflows!)
					size_t len = 0;
					if(  !is_latin2  ) {
						*dst++ = c = (uint8)utf8_to_utf16( (const utf8*)src, &len );
					}
					else {
						*dst++ = c = unicode_to_latin2( utf8_to_utf16( (const utf8*)src, &len ) );
					}
					src += len;
				}
			}
			else if(c>=13) {
				// just copy
				src ++;
				*dst++ = c;
			}
			else {
				// ignore this character
				src ++;
			}
		}
	} while (c != '\0');
	*dst = 0;
	return base;
}



/* needed for loading city names */
static char pakset_path[256];

// List of custom city and streetnames
vector_tpl<char *> translator::city_name_list;
vector_tpl<char *> translator::street_name_list;


// fills a list from a file with the given prefix followed by a language code
void translator::load_custom_list( int lang, vector_tpl<char *>&name_list, const char *fileprefix )
{
	FILE *file;

	// Clean up all names
	FOR(vector_tpl<char*>, const i, name_list) {
		free(i);
	}
	name_list.clear();

	// @author prissi: first try in pakset
	{
		string local_file_name(env_t::user_dir);
		local_file_name = local_file_name + "addons/" + pakset_path + "text/" + fileprefix + langs[lang].iso_base + ".txt";
		DBG_DEBUG("translator::load_custom_list()", "try to read city name list from '%s'", local_file_name.c_str());
		file = fopen(local_file_name.c_str(), "rb");
	}
	// not found => try user location
	if(  file==NULL  ) {
		string local_file_name(env_t::user_dir);
		local_file_name = local_file_name + fileprefix + langs[lang].iso_base + ".txt";
		file = fopen(local_file_name.c_str(), "rb");
		DBG_DEBUG("translator::load_custom_list()", "try to read city name list from '%s'", local_file_name.c_str());
	}
	// not found => try pak location
	if(  file==NULL  ) {
		string local_file_name(env_t::program_dir);
		local_file_name = local_file_name + pakset_path + "text/" + fileprefix + langs[lang].iso_base + ".txt";
		DBG_DEBUG("translator::load_custom_list()", "try to read city name list from '%s'", local_file_name.c_str());
		file = fopen(local_file_name.c_str(), "rb");
	}
	// not found => try global translations
	if(  file==NULL  ) {
		string local_file_name(env_t::program_dir);
		local_file_name = local_file_name + "text/" + fileprefix + langs[lang].iso_base + ".txt";
		DBG_DEBUG("translator::load_custom_list()", "try to read city name list from '%s'", local_file_name.c_str());
		file = fopen(local_file_name.c_str(), "rb");
	}
	fflush(NULL);

	if (file != NULL) {
		// ok, could open file
		char buf[256];
		bool file_is_utf = is_unicode_file(file);
		while(  !feof(file)  ) {
			if (fgets_line(buf, sizeof(buf), file)) {
				rtrim(buf);
				char *c = recode(buf, file_is_utf, true, langs[lang].is_latin2_based );
				if(  *c!=0  &&  *c!='#'  ) {
					name_list.append(c);
				}
			}
		}
		fclose(file);
		DBG_DEBUG("translator::load_custom_list()","Loaded list %s_%s.txt.", fileprefix, langs[lang].iso_base );
	}
	else {
		DBG_DEBUG("translator::load_custom_list()","No list %s_%s.txt found, using defaults.", fileprefix, langs[lang].iso_base );
	}
}


/* the city list is now reloaded after the language is changed
 * new cities will get their appropriate names
 * @author hajo, prissi
 */
void translator::init_custom_names(int lang)
{
	// Hajo: init names. There are two options:
	//
	// 1.) read list from file
	// 2.) create random names (only for cities)

	// try to read list
	load_custom_list( lang, city_name_list, "citylist_" );
	load_custom_list( lang, street_name_list, "streetlist_" );

	if (city_name_list.empty()) {
		DBG_MESSAGE("translator::init_city_names", "reading failed, creating random names.");
		// Hajo: try to read list failed, create random names
		for(  uint i = 0;  i < 2048;  i++  ) {
			char name[64];
			sprintf( name, "%%%X_CITY_SYLL", i );
			const char *s1 = translator::translate(name,lang);
			if(s1==name) {
				// name not available ...
				continue;
			}
			// now add all second name extensions ...
			const size_t l1 = strlen(s1);
			for(  uint j = 0;  j < 1024;  j++  ) {

				sprintf( name, "&%X_CITY_SYLL", j );
				const char *s2 = translator::translate(name,lang);
				if(s2==name) {
					// name not available ...
					continue;
				}
				const size_t l2 = strlen(s2);
				char *const c = MALLOCN(char, l1 + l2 + 1);
				sprintf(c, "%s%s", s1, s2);
				city_name_list.append(c);
				
				// Now prefixes and suffixes. 
				// Only add a random fraction of these, as
				// it would be too repetative to have every
				// prefix and suffix with every name.

				// TODO: Add geographically specific 
				// prefixes and suffixes (e.g. "-on-sea"
				// appearing only where the town is
				// actually by the sea). 

				const uint32 random_percent = sim_async_rand(100);
				
				// TODO: Have these set from simuconf.tab
				const uint32 prefix_probability = 5;
				const uint32 suffix_probability = 5;

				const uint32 max_prefixes_per_name = 5;
				const uint32 max_suffixes_per_name = 5;

				const bool allow_prefixes_and_suffixes_together = false;

				uint32 prefixes_this_name = 0;
				uint32 suffixes_this_name = 0;

				if (random_percent <= prefix_probability)
				{
					// Prefixes
					for (uint32 p = 0; p < 256; p++)
					{
						sprintf(name, "&%X_CITY_PREFIX", p);
						const char *s3 = translator::translate(name, lang);
						const uint32 random_percent_prefix = sim_async_rand(100);
						
						if (s3 == name || random_percent_prefix > prefix_probability)
						{
							// name not available ...
							continue;
						}
						const size_t l3 = strlen(s3);
						char *const c2 = MALLOCN(char, l1 + l2 +l3 + 1);
						sprintf(c2, "%s%s%s", s3, s1, s2);
						city_name_list.append(c2);
						if (prefixes_this_name++ > max_prefixes_per_name)
						{
							break;
						}
					}
				}

				if (random_percent >= (100 - suffix_probability) && (random_percent >= prefix_probability || allow_prefixes_and_suffixes_together))
				{
					// Suffixes
					for (uint32 p = 0; p < 256; p++)
					{
						sprintf(name, "&%X_CITY_SUFFIX", p);
						const char *s3 = translator::translate(name, lang);
						const uint32 random_percent_suffix = sim_async_rand(100);

						if (s3 == name || random_percent_suffix > prefix_probability || strcmp(s3, s2) == 0)
						{
							// Name not available or the suffix is identical to the final syllable
							// (This should avoid names such as Tarwoodwood). 
							continue;
						}

						// Compare lower cases end parts without spaces with upper case end parts with spaces.
						// This should avoid town names such as "Bumblewick Wick"
						// First, remove the space
						std::string spaceless_name = s3;
						spaceless_name.erase(0, 1);
						// Now perform case insensitive comparison. Different for Linux and Windows.
#ifdef _MSC_VER
						if (stricmp(spaceless_name.c_str(), s2) == 0 || stricmp(s2, s3) == 0)
#else
						if (strcasecmp(spaceless_name.c_str(), s2) == 0 || strcasecmp(s2, s3) == 0)
#endif
						{
							continue;
						}

						const size_t l3 = strlen(s3);
						char *const c2 = MALLOCN(char, l1 + l2 + l3 + 1);
						sprintf(c2, "%s%s%s", s1, s2, s3);
						city_name_list.append(c2);
						if (suffixes_this_name++ > max_prefixes_per_name)
						{
							break;
						}
					}
				}
			}
		}
	}
}


/* now on to the translate stuff */


static void load_language_file_body(FILE* file, stringhashtable_tpl<const char*>* table, bool language_is_utf, bool file_is_utf, bool language_is_latin2 )
{
	char buffer1 [4096];
	char buffer2 [4096];

	bool convert_to_unicode = language_is_utf && !file_is_utf;

	do {
		fgets_line(buffer1, sizeof(buffer1), file);
		if(  buffer1[0] == '#'  ) {
			// ignore comments
			continue;
		}
		if(  !feof(file)  ) {
			fgets_line(buffer2, sizeof(buffer2), file);
			if(  strcmp(buffer1,buffer2)  ) {
				// only add line which are actually different
				const char *raw = recode(buffer1, file_is_utf, false, language_is_latin2 );
				const char *translated = recode(buffer2, false, convert_to_unicode,language_is_latin2);
				if(  cbuffer_t::check_format_strings(raw, translated)  ) {
					table->set( raw, translated );
				}
			}
		}
	} while (!feof(file));
}


void translator::load_language_file(FILE* file)
{
	char buffer1 [256];
	bool file_is_utf = is_unicode_file(file);

	// Read language name
	fgets_line(buffer1, sizeof(buffer1), file);

	langs[single_instance.lang_count].name = strdup(buffer1);

	if(  !file_is_utf  ) {
		// find out the font if not unicode (and skip it)
		while(  !feof(file)  ) {
			fgets_line( buffer1, sizeof(buffer1), file );
			if(  buffer1[0] == '#'  ) {
				continue;
			}
			if(  strcmp(buffer1,"PROP_FONT_FILE") == 0  ) {
				fgets_line( buffer1, sizeof(buffer1), file );
				// HACK: so we guess about latin2 from the font name!
				langs[single_instance.lang_count].is_latin2_based = strcmp( buffer1, "prop-latin2.fnt" ) == 0;
				// we must register now a unicode font
				langs[single_instance.lang_count].texts.set( "PROP_FONT_FILE", "cyr.bdf" );
				break;
			}
		}
	}
	else {
		// since it is anyway UTF8
		langs[single_instance.lang_count].is_latin2_based = false;
	}

	//load up translations, putting them into
	//language table of index 'lang'
	load_language_file_body(file, &langs[single_instance.lang_count].texts, true, file_is_utf, langs[single_instance.lang_count].is_latin2_based );
 }


static translator::lang_info* get_lang_by_iso(const char *iso)
{
	for(  translator::lang_info* i = langs;  i != langs + translator::get_language_count();  ++i  ) {
		if(  i->iso_base[0] == iso[0]  &&  i->iso_base[1] == iso[1]  ) {
			return i;
		}
	}
	return NULL;
}


void translator::load_files_from_folder(const char *folder_name, const char *what)
{
	searchfolder_t folder;
	int num_pak_lang_dat = folder.search(folder_name, "tab");
	DBG_MESSAGE("translator::load_files_from_folder()", "search folder \"%s\" and found %i files", folder_name, num_pak_lang_dat); (void)num_pak_lang_dat;
	//read now the basic language infos
	FOR(searchfolder_t, const& i, folder) {
		string const fileName(i);
		size_t pstart = fileName.rfind('/') + 1;
		const string iso = fileName.substr(pstart, fileName.size() - pstart - 4);

		lang_info* lang = get_lang_by_iso(iso.c_str());
		if (lang != NULL) {
			DBG_MESSAGE("translator::load_files_from_folder()", "loading %s translations from %s for language %s", what, fileName.c_str(), lang->iso_base);
			if (FILE* const file = fopen(fileName.c_str(), "rb")) {
				bool file_is_utf = is_unicode_file(file);
				load_language_file_body(file, &lang->texts, true, file_is_utf, lang->is_latin2_based );
				fclose(file);
			}
			else {
				dbg->warning("translator::load_files_from_folder()", "cannot open '%s'", fileName.c_str());
			}
		}
		else {
			dbg->warning("translator::load_files_from_folder()", "no %s texts for language '%s'", what, iso.c_str());
		}
	}
}


bool translator::load(const string &path_to_pakset)
{
	chdir( env_t::program_dir );
	tstrncpy(pakset_path, path_to_pakset.c_str(), lengthof(pakset_path));

	//initialize these values to 0(ie. nothing loaded)
	single_instance.current_lang = -1;
	single_instance.lang_count = 0;

	DBG_MESSAGE("translator::load()", "Loading languages...");
	searchfolder_t folder;
	folder.search("text/", "tab");

	//read now the basic language infos
	for (searchfolder_t::const_iterator i = folder.begin(), end = folder.end(); i != end; ++i) {
		const string fileName(*i);
		size_t pstart = fileName.rfind('/') + 1;
		const string iso = fileName.substr(pstart, fileName.size() - pstart - 4);

		if (FILE* const file = fopen(fileName.c_str(), "rb")) {
			DBG_MESSAGE("translator::load()", "base file \"%s\" - iso: \"%s\"", fileName.c_str(), iso.c_str());
			load_language_iso(iso);
			load_language_file(file);
			fclose(file);
			single_instance.lang_count++;
			if (single_instance.lang_count == lengthof(langs)) {
				if (++i != end) {
					// some languages were not loaded, let the user know what happened
					dbg->warning("translator::load()", "some languages were not loaded, limit reached");
					for (; i != end; ++i) {
						dbg->warning("translator::load()", " %s not loaded", *i);
					}
				}
				break;
			}
		}
	}

	// now read the pakset specific text
	// there can be more than one file per language, provided it is name like iso_xyz.tab
	const string folderName(path_to_pakset + "text/");
	load_files_from_folder(folderName.c_str(), "pak");

	if(  env_t::default_settings.get_with_private_paks()  ) {
		chdir( env_t::user_dir );
		// now read the pakset specific text
		// there can be more than one file per language, provided it is name like iso_xyz.tab
		const string folderName("addons/" + path_to_pakset + "text/");
		load_files_from_folder(folderName.c_str(), "pak addons");
		chdir( env_t::program_dir );
	}

	//if NO languages were loaded then game cannot continue
	if (single_instance.lang_count < 1) {
		return false;
	}

	// now we try to read the compatibility stuff
	if (FILE* const file = fopen((path_to_pakset + "compat.tab").c_str(), "rb")) {
		load_language_file_body(file, &compatibility, false, false, false );
		DBG_MESSAGE("translator::load()", "pakset compatibility texts loaded.");
		fclose(file);
	}
	else {
		DBG_MESSAGE("translator::load()", "no pakset compatibility texts");
	}

	// also addon compatibility ...
	if(  env_t::default_settings.get_with_private_paks()  ) {
		chdir( env_t::user_dir );
		if (FILE* const file = fopen(string("addons/"+path_to_pakset + "compat.tab").c_str(), "rb")) {
			load_language_file_body(file, &compatibility, false, false, false );
			DBG_MESSAGE("translator::load()", "pakset addon compatibility texts loaded.");
			fclose(file);
		}
		chdir( env_t::program_dir );
	}

#if DEBUG>=4
#ifdef need_dump_hashtable
	dump_hashtable(&compatibility);
#endif
#endif

	// use english if available
	current_langinfo = get_lang_by_iso("en");

	// it's all ok
	return true;
}


void translator::load_language_iso(const string &iso)
{
	string base(iso);
	langs[single_instance.lang_count].iso = strdup(iso.c_str());
	int loc = iso.find('_');
	if (loc != -1) {
		base = iso.substr(0, loc);
	}
	langs[single_instance.lang_count].iso_base = strdup(base.c_str());
}


void translator::set_language(int lang)
{
	if(  0 <= lang  &&  lang < single_instance.lang_count  ) {
		single_instance.current_lang = lang;
		current_langinfo = langs+lang;
		env_t::language_iso = langs[lang].iso;
		env_t::default_settings.set_name_language_iso( langs[lang].iso );
		init_custom_names(lang);
		current_langinfo->eclipse_width = proportional_string_width( translate("...") );
		DBG_MESSAGE("translator::set_language()", "%s, unicode %d", langs[lang].name, true);
	}
	else {
		dbg->warning("translator::set_language()", "Out of bounds : %d", lang);
	}
}


// returns the id for this language or -1 if not there
int translator::get_language(const char *iso)
{
	for(  int i = 0;  i < single_instance.lang_count;  i++  ) {
		const char *iso_base = langs[i].iso_base;
		if(  iso_base[0] == iso[0]  &&  iso_base[1] == iso[1]  ) {
			return i;
		}
	}
	return -1;
}


void translator::set_language(const char *iso)
{
	for(  int i = 0;  i < single_instance.lang_count;  i++  ) {
		const char *iso_base = langs[i].iso_base;
		if(  iso_base[0] == iso[0]  &&  iso_base[1] == iso[1]  ) {
			set_language(i);
			return;
		}
	}
}


const char *translator::translate(const char *str)
{
	return get_lang()->translate(str);
}


const char *translator::translate(const char *str, int lang)
{
	return langs[lang].translate(str);
}


const char *translator::get_month_name(uint16 month)
{
	static const char *const month_names[] = {
		"January",
		"February",
		"March",
		"April",
		"May",
		"June",
		"July",
		"August",
		"September",
		"Oktober",
		"November",
		"December"
	};
	return translate(month_names[month % lengthof(month_names)]);
}

const char *translator::get_date(uint16 year, uint16 month, uint16 day, char const* season)
{
	char const* const month_ = get_month_name(month);
	char const* const year_sym = strcmp("YEAR_SYMBOL", translate("YEAR_SYMBOL")) ? translate("YEAR_SYMBOL") : "";
	char const* const day_sym = strcmp("DAY_SYMBOL", translate("DAY_SYMBOL")) ? translate("DAY_SYMBOL") : "";
	static char date[256];
	switch (env_t::show_month) {
	case env_t::DATE_FMT_GERMAN:
		sprintf(date, "%s %d. %s %d%s", season, day, month_, year, year_sym);
		break;
	case env_t::DATE_FMT_GERMAN_NO_SEASON:
		sprintf(date, "%d. %s %d%s", day, month_, year, year_sym);
		break;
	case env_t::DATE_FMT_US:
		sprintf(date, "%s %s %d %d%s", season, month_, day, year, year_sym);
		break;
	case env_t::DATE_FMT_US_NO_SEASON:
		sprintf(date, "%s %d %d%s", month_, day, year, year_sym);
		break;
	case env_t::DATE_FMT_JAPANESE:
		sprintf(date, "%s %d%s %s %d%s", season, year, year_sym, month_, day, day_sym);
		break;
	case env_t::DATE_FMT_JAPANESE_NO_SEASON:
		sprintf(date, "%d%s %s %d%s", year, year_sym, month_, day, day_sym);
		break;
	case env_t::DATE_FMT_MONTH:
		sprintf(date, "%s, %s %d%s", month_, season, year, year_sym);
		break;
	case env_t::DATE_FMT_SEASON:
		sprintf(date, "%s %d%s", season, year, year_sym);
		break;
	case env_t::DATE_FMT_INTERNAL_MINUTE: // Extended unique
	case env_t::DATE_FMT_JAPANESE_INTERNAL_MINUTE: // Extended unique
		sprintf(date, "%s %d%s %s", season, year, year_sym, month_);
		break;
	}
	return date;
}

/* get a name for a non-matching object */
const char *translator::compatibility_name(const char *str)
{
	if(  str==NULL  ) {
		return "(null)";
	}
	if(  str[0]=='\0'  ) {
		return str;
	}
	const char *trans = compatibility.get(str);
	return trans != NULL ? trans : str;
}

const char *translator::get_year_month(uint16 year_month)
{
	uint16 year = year_month / 12;
	uint8 month = year_month % 12 + 1;
	char const* const month_ = get_month_name(year_month % 12);
	char const* mon_sym = strcmp("MON_SYMBOL", translate("MON_SYMBOL")) ? translate("MON_SYMBOL") : "";
	char const* year_sym = strcmp("YEAR_SYMBOL", translate("YEAR_SYMBOL")) ? translate("YEAR_SYMBOL") : "";
	static char format_year_month[64];

	switch (env_t::show_month) {
		case env_t::DATE_FMT_SEASON:
			sprintf(format_year_month, "%04d%s", year, year_sym);
			break;
		case env_t::DATE_FMT_JAPANESE:
		case env_t::DATE_FMT_JAPANESE_NO_SEASON:
		case env_t::DATE_FMT_JAPANESE_INTERNAL_MINUTE: // Extended unique
			if (year_sym == "") {
				year_sym = "/";
				mon_sym = "";
			}
			sprintf(format_year_month, "%04d%s%2d%s", year, year_sym, month, mon_sym);
			break;
		case env_t::DATE_FMT_MONTH:
		case env_t::DATE_FMT_GERMAN:
		case env_t::DATE_FMT_GERMAN_NO_SEASON:
		case env_t::DATE_FMT_US:
		case env_t::DATE_FMT_US_NO_SEASON:
		case env_t::DATE_FMT_INTERNAL_MINUTE: // Extended unique
			sprintf(format_year_month, "%s %04d%s", month_, year, year_sym);
			break;
	}

	return format_year_month;
}
