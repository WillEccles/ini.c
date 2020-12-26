/*
 *  __         __
 * |__|.-----.|__|  .----.
 * |  ||     ||  |__|  __|
 * |__||__|__||__|__|____|
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

#include "ini.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* fmt_spaces = " %256[^=; ] = %256[^\n] ";
static const char* fmt_no_spaces = " %256[^=; ]=%256[^ \n] ";

static void freepair_r(struct inipair* root);
static void freesec_r(struct inisection* sec);

struct inisection* makesection(char* name) {
  if (name == NULL) {
    return NULL;
  }
  struct inisection* s = calloc(1, sizeof(struct inisection));
  if (s == NULL) {
    perror("makesection: calloc");
    return NULL;
  }
  s->name = strdup(name);
  s->head = NULL;
  s->next = NULL;
  return s;
}

struct inifile* makeini(int flags) {
  struct inifile* f = calloc(1, sizeof(struct inifile));
  if (f == NULL) {
    perror("makeini: calloc");
    return NULL;
  }
  f->head = NULL;
  f->default_section = calloc(1, sizeof(struct inisection));
  if (f->default_section == NULL) {
    perror("makeini: calloc");
    free(f);
    return NULL;
  }
  f->default_section->name = NULL;
  f->default_section->head = NULL;
  f->default_section->next = NULL;
  f->flags = flags;
  return f;
}

struct inisection* freesection(struct inisection* sec) {
  if (sec != NULL) {
    freepair_r(sec->head);
    struct inisection* next = sec->next;
    // names are created with strdup
    free(sec->name);
    free(sec);
    return next;
  }
  return NULL;
}

struct inipair* makepair(char* key, char* val) {
  if (key == NULL) {
    return NULL;
  }

  struct inipair* p = calloc(1, sizeof(struct inipair));
  if (p == NULL) {
    perror("makepair: calloc");
    return NULL;
  }
  p->key = strdup(key);
  p->val = val == NULL ? NULL : strdup(val);
  return p;
}

struct inipair* freepair(struct inipair* pair) {
  if (pair != NULL) {
    struct inipair* next = pair->next;
    // keys/vals are created with strdup
    free(pair->key);
    free(pair->val);
    free(pair);
    return next;
  }
  return NULL;
}

void freepair_r(struct inipair* root) {
  if (root == NULL) {
    return;
  }

  while ((root = freepair(root)));
}

void freesec_r(struct inisection* sec) {
  if (sec == NULL) {
    return;
  }

  while ((sec = freesection(sec)));
}

void freeini(struct inifile* ini) {
  if (ini == NULL) {
    return;
  }

  freesec_r(ini->default_section);
  freesec_r(ini->head);
  free(ini);
}

struct inisection* section_insert(struct inifile* file, struct inisection* sec) {
  if (file == NULL || sec == NULL) {
    return NULL;
  }

  if (file->head == NULL) {
    file->head = sec;
    sec->next = NULL;
    return sec;
  } else {
    struct inisection* prev = NULL;
    struct inisection* curr = file->head;
    int s;
    while (curr->next != NULL) {
      s = strcmp(sec->name, curr->name);
      if (s < 0) { // less than
        if (prev == NULL) {
          sec->next = file->head;
          file->head = sec;
          return sec;
        } else {
          prev->next = sec;
          sec->next = curr;
          return sec;
        }
      } else if (s == 0) { // equal to
        return curr;
      }

      prev = curr;
      curr = curr->next;
    }

    curr->next = sec;
    sec->next = NULL;
    return sec;
  }
}

struct inipair* pair_insert(struct inisection* sec, struct inipair* pair) {
  if (sec == NULL || pair == NULL) {
    return NULL;
  }

  if (sec->head == NULL) {
    sec->head = pair;
    sec->head->next = NULL;
    return sec->head;
  } else {
    struct inipair* prev = NULL;
    struct inipair* curr = sec->head;
    int s;
    while (curr->next != NULL) {
      s = strcmp(pair->key, curr->key);
      if (s < 0) { // less than (insert before)
        if (prev == NULL) {
          sec->head = pair;
          pair->next = curr;
          return sec->head;
        } else {
          prev->next = pair;
          pair->next = curr;
          return pair;
        }
      } else if (s == 0) { // equal to (overwrite)
        if (prev == NULL) {
          sec->head = pair;
          pair->next = curr->next;
        } else {
          prev->next = pair;
          pair->next = curr->next;
        }
        freepair(curr);
        return pair;
      }

      prev = curr;
      curr = curr->next;
    }

    curr->next = pair;
    pair->next = NULL;
    return pair;
  }
}

int loadinifromfile(struct inifile* inif, char* filename) {
  if (inif == NULL || filename == NULL || inif->default_section == NULL) {
    return 1;
  }

  FILE* infile = fopen(filename, "r");
  if (NULL == infile) {
    perror("loadinifromfile: fopen");
    return 1;
  }

  const char* keyvalfmt;
  if (inif->flags & INIO_SPACE_AROUND_DELIM) {
    keyvalfmt = fmt_spaces;
  } else {
    keyvalfmt = fmt_no_spaces;
  }

  char tmpline[512] = {0};
  char tmpkey[256] = {0};
  char tmpval[256] = {0};
  char tmpsection[256] = {0};

  // default to inserting to the default section
  struct inisection* tmpsec = inif->default_section;

  int rval = 0;
  while (!feof(infile) && !ferror(infile)) {
    if (tmpline != fgets(tmpline, 512, infile)) {
      continue;
    }

    if (1 == (rval = sscanf(tmpline, " [%256[^]]]", tmpsection))) {
      // set the current section
      tmpsec = makesection(tmpsection);
      struct inisection* tmpsec2 = section_insert(inif, tmpsec);
      if (tmpsec2 != tmpsec) {
        free(tmpsec);
        tmpsec = tmpsec2;
      }
      continue;
    }

    // check for both a key and a value
    if (2 == (rval = sscanf(tmpline, keyvalfmt, tmpkey, tmpval))) {
      // insert the new key/value pair into the current section
      pair_insert(tmpsec, makepair(tmpkey, tmpval));
      continue;
    }

    // check for a key with no value
    if (inif->flags & INIO_ALLOW_EMPTY) {
      if (1 == (rval = sscanf(tmpline, " %256[^=; ] \n", tmpkey))) {
        pair_insert(tmpsec, makepair(tmpkey, NULL));
        continue;
      }
    }
  }

  fclose(infile);

  return 0;
}

struct inifile* newinifromfile(char* filename, int flags) {
  if (filename == NULL) {
    return NULL;
  }

  struct inifile* inif = makeini(flags);
  if (inif == NULL) {
    return NULL;
  }

  if (1 == loadinifromfile(inif, filename)) {
    freeini(inif);
    return NULL;
  }

  return inif;
}

void parseini(struct inifile* ini, ini_pair_op cb) {
  ini_foreach(ini, cb);
}

void ini_foreach(struct inifile* ini, ini_pair_op cb) {
  for (struct inipair* p = ini->default_section->head; p; p = p->next) {
    cb(ini->default_section, p);
  }

  for (struct inisection* s = ini->head; s; s = s->next) {
    for (struct inipair* p = s->head; p; p = p->next) {
      cb(s, p);
    }
  }
}

struct inisection* ini_getsection(struct inifile* ini, char* name) {
  if (ini == NULL) {
    return NULL;
  }

  if (name == NULL) {
    return ini->default_section;
  }

  for (struct inisection* s = ini->head; s; s = s->next) {
    if (0 == strcmp(name, s->name)) {
      return s;
    }
  }

  return NULL;
}

struct inipair* inisection_getpair(struct inisection* section, char* key) {
  if (section == NULL || key == NULL) {
    return NULL;
  }

  for (struct inipair* p = section->head; p; p = p->next) {
    if (0 == strcmp(key, p->key)) {
      return p;
    }
  }

  return NULL;
}

struct inipair* ini_getpair(struct inifile* ini, char* section, char* key) {
  struct inisection* s = ini_getsection(ini, section);
  if (s == NULL) {
    return NULL;
  }

  return inisection_getpair(s, key);
}

int writeinitofile(struct inifile* ini, char* filename) {
  if (ini == NULL || filename == NULL) {
    return 1;
  }

  FILE* outfile = fopen(filename, "w");
  if (outfile == NULL) {
    perror("writeinitofile: fopen");
    return 1;
  }

  for (struct inipair* p = ini->default_section->head; p; p = p->next) {
    if (p->val != NULL) {
      fprintf(outfile, "%s=%s\n", p->key, p->val);
    } else {
      if (ini->flags & INIO_ALLOW_EMPTY) {
        fprintf(outfile, "%s=\n", p->key);
      }
    }
  }

  fprintf(outfile, "\n");

  for (struct inisection* s = ini->head; s; s = s->next) {
    if (s->head == NULL) {
      continue;
    } else {
      fprintf(outfile, "[%s]\n", s->name);
    }

    for (struct inipair* p = s->head; p; p = p->next) {
      if (p->val != NULL) {
        fprintf(outfile, "%s=%s\n", p->key, p->val);
      } else {
        if (ini->flags & INIO_ALLOW_EMPTY) {
          fprintf(outfile, "%s=\n", p->key);
        }
      }
    }

    fprintf(outfile, "\n");
  }

  fclose(outfile);

  return 0;
}

char* pair_setval(struct inipair* pair, char* val) {
  if (pair == NULL) {
    return NULL;
  }

  if (pair->val != NULL) {
    free(pair->val);
    pair->val = NULL;
  }

  if (val != NULL) {
    pair->val = strdup(val);
  }

  return pair->val;
}

struct inipair* ini_put(struct inifile* ini, char* section, char* key,
                        char* val) {
  if (ini == NULL || key == NULL) {
    return NULL;
  }

  struct inisection* s = ini_getsection(ini, section);
  if (s == NULL) {
    s = section_insert(ini, makesection(section));
    if (s == NULL) {
      return NULL;
    }
  }

  struct inipair* p = inisection_getpair(s, key);
  if (p == NULL) {
    p = pair_insert(s, makepair(key, val));
    if (p == NULL) {
      return NULL;
    }
  }

  return p;
}

struct inipair* ini_set(struct inifile* ini, char* section, char* key,
                        char* val) {
  if (ini == NULL || key == NULL) {
    return NULL;
  }

  struct inisection* s = ini_getsection(ini, section);
  if (s == NULL) {
    return NULL;
  }

  struct inipair* p = inisection_getpair(s, key);
  if (p == NULL) {
    return NULL;
  }

  if (NULL == pair_setval(p, val)) {
    return NULL;
  }

  return p;
}
