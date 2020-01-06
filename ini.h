/*
 * ini.h
 *
 * Copyright 2020 Will Eccles
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INI_H
#define INI_H

/*
 * Options for INI files. By default, options are assumed off.
 */
enum INI_OPT {
    INIO_NONE = 0, // no options specified
    INIO_SPACE_AROUND_DELIM = 1 << 0, // allow spaces around delimiters, i.e. name = val rather than name=val
    INIO_ALLOW_EMPTY = 1 << 1, // allow empty values for keys
    INIO_ALL = 0xFFFFFFFF, // allow all options
};

/*
 * Key-value pair in an INI file.
 * Values MUST be set to dynamically-allocated strings!
 * You should not write, only read. If you wish to set the
 * value, use pair_setval() or one of the other value-setting functions.
 */
struct inipair {
    struct inipair* next;
    char* key;
    char* val;
};

/*
 * Section in an INI file.
 */
struct inisection {
    char* name;
    struct inipair* head;
    struct inisection* next;
};

/*
 * Structure representing an INI file.
 * This is the only structure in this file you
 * should care about, except for manually iterating
 * through the INI file.
 *
 * You should initialize this with either makeini() or
 * newinifromfile().
 */
struct inifile {
    struct inisection* head; // head of the list of sections, kept in alphabetical
    struct inisection* default_section; // default section (options found before the first section)
    int flags; // flags determining parsing behavior (see enum INI_OPT)
};

/*
 * Callback function used when you call parseini().
 * The arguments are the INI section in which the pair was found
 * and the pair itself. Note that the 'name' field of the default
 * INI section (for options which appear before the first occurrence
 * of a [section] in the file) is NULL.
 */
typedef void(*ini_pair_op)(struct inisection*, struct inipair*);

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Make a new INI file structure.
 * Returns NULL on error.
 */
extern struct inifile* makeini(int flags);

/*
 * Parse an INI file into a new inifile structure.
 * Filename must not be NULL. Flags should be set from
 * enum INI_OPT.
 * Returns NULL on error, else returns a pointer to the
 * parsed INI file structure.
 */
extern struct inifile* newinifromfile(char* filename, int flags);

/*
 * Load data into an existing inifile structure,
 * presumably created by makeini() or newinifromfile().
 * If inif contains values already, they will be kept.
 * Duplicate values will be overwritten.
 * Returns 0 on success, else 1.
 */
extern int loadinifromfile(struct inifile* inif, char* filename);

/*
 * Writes an INI file structures contents to the disk.
 * Returns 0 on success, 1 on failure.
 * If the file contains comments, they will be deleted, unless I update
 * this utility.
 */
extern int writeinitofile(struct inifile* ini, char* filename);

/*
 * Parse an INI file. Every key-value pair is passed to the callback function,
 * along with its section. This callback function will be called repeatedly
 * until all pairs in the file have been parsed. This is effectively
 * a wrapper around ini_foreach with a less general name.
 */
extern void parseini(struct inifile* ini, ini_pair_op cb);

/*
 * Loops through all sections and pairs in an INI file and performs
 * the given operation on them. Note that the default section's
 * 'name' field is NULL.
 */
extern void ini_foreach(struct inifile* ini, ini_pair_op cb);

/*
 * Returns the section in an INI file with the given name.
 * If name is NULL, the default section is returned. If the
 * name is not found, NULL is returned.
 */
extern struct inisection* ini_getsection(struct inifile* ini, char* name);

/*
 * Returns the key-value pair in a given section with a given key.
 * If the key is not found, NULL is returned.
 * If the section provided is NULL, it's assumed that the key is in
 * the default section of the file.
 * Any other error results in NULL being returned.
 */
extern struct inipair* inisection_getpair(struct inisection* section, char* key);

/*
 * Attempts to find a key-value pair in an INI file given a section name and
 * key. If the section name is NULL, the default section is searched.
 * This is a wrapper around ini_getsection() and inisection_getpair().
 */
extern struct inipair* ini_getpair(struct inifile* ini, char* section, char* key);

/*
 * Sets the value of a key-value pair. This is the only recommended way
 * to set the value of a pair, as it deals with string duplication for you.
 * Returns a pointer to the new value, or NULL if there was an error. NULL
 * is also returned if the new value is NULL.
 */
extern char* pair_setval(struct inipair* pair, char* val);

/*
 * Sets the value of a given key in a given section. If the section and/or
 * key is not found, they will be created. NULL section implies default section.
 * Returns a pointer to the pair which contains the key, or NULL on error.
 */
extern struct inipair* ini_put(struct inifile* ini, char* section, char* key, char* val);

/*
 * Much like ini_put, except that if a section or key is not found, it will
 * not be created for you.
 */
extern struct inipair* ini_set(struct inifile* ini, char* section, char* key, char* val);

/*
 * Frees an entire INI file structure.
 */
extern void freeini(struct inifile* ini);

/*
 * Make a new section. Returns NULL on error.
 * Name is required.
 * The name parameter is duplicated, so overwriting the original pointer
 * will have no effect on the section's name.
 */
extern struct inisection* makesection(char* name);

/*
 * Frees a section and returns the next section, or NULL if there isn't one.
 * This also frees all pairs in the section to avoid leaking memory.
 */
extern struct inisection* freesection(struct inisection* sec);

/*
 * Make a new pair. Returns NULL on error.
 * Only key must be provided. If key is NULL, there will be an error.
 * Both key and val are duplicated, so they may be reusable pointers.
 */
extern struct inipair* makepair(char* key, char* val);

/*
 * Frees a pair and returns a pointer to the next pair in the list,
 * or returns NULL if there isn't one.
 */
extern struct inipair* freepair(struct inipair* pair);

/*
 * Insert a section into an INI file structure.
 * If returned value is NULL, there was an error.
 * If the returned value != sec, then sec should be freed and ignored, as the
 * returned value is a pre-existing section with the same name.
 * If successful, the return value == sec.
 */
extern struct inisection* section_insert(struct inifile* file, struct inisection* sec);

/*
 * Insert a pair into a section in an INI file.
 * If the return value is NULL, there was an error.
 * If the return value is non-NULL, it will be equal to pair.
 * If the supplied value is alreaady found in the list, the existing one
 * will be freed and the new one will replace it.
 */
extern struct inipair* pair_insert(struct inisection* sec, struct inipair* pair);

#ifdef __cplusplus
}
#endif

#endif
