#define HASH_ENTRY(type, ptr, member) \
  ((type*)(((char*)ptr) - (uintptr_t)&((type*)0)->member))

#define HASH_FUNCTION(key,keylen,num_bkts,hashv,bkt)  \
  hashv = value_hash(*((const Value**)key));              \
  bkt = (hashv) & (num_bkts-1U)

#define HASH_KEYCOMPARE(a, b, len) \
  value_compare(*((const Value**)a), *((const Value**)b))

#include "uthash.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

typedef enum {
  INT,
  FLOAT,
  STRING,
  MAP,
  SET,
} ValueType;

#define VALUE_FIELDS \
  ValueType type;

typedef struct {
  VALUE_FIELDS
} Value;

typedef struct {
  const Value* key;
  const Value* value;
  UT_hash_handle hh;
} MapEntry;

typedef struct {
  const Value* value;
  UT_hash_handle hh;
} SetEntry;

typedef struct {
  VALUE_FIELDS
  int i;
} Int;

typedef struct {
  VALUE_FIELDS
  float f;
} Float;

typedef struct {
  VALUE_FIELDS
  char* s;
  unsigned len;
} String;

typedef struct {
  VALUE_FIELDS
  MapEntry* entries;
  unsigned hashv;
  int dirty;
} Map;

typedef struct {
  VALUE_FIELDS
  SetEntry* entries;
  unsigned hashv;
  int dirty;
} Set;

unsigned hash_jen(const void* key, unsigned keylen) {
  unsigned _hj_i,_hj_j,_hj_k;
  unsigned const char *_hj_key=(unsigned const char*)(key);
  unsigned hashv = 0xfeedbeefu;
  _hj_i = _hj_j = 0x9e3779b9u;
  _hj_k = (unsigned)(keylen);
  while (_hj_k >= 12U) {
    _hj_i +=    (_hj_key[0] + ( (unsigned)_hj_key[1] << 8 )
        + ( (unsigned)_hj_key[2] << 16 )
        + ( (unsigned)_hj_key[3] << 24 ) );
    _hj_j +=    (_hj_key[4] + ( (unsigned)_hj_key[5] << 8 )
        + ( (unsigned)_hj_key[6] << 16 )
        + ( (unsigned)_hj_key[7] << 24 ) );
    hashv += (_hj_key[8] + ( (unsigned)_hj_key[9] << 8 )
        + ( (unsigned)_hj_key[10] << 16 )
        + ( (unsigned)_hj_key[11] << 24 ) );

     HASH_JEN_MIX(_hj_i, _hj_j, hashv);

     _hj_key += 12;
     _hj_k -= 12U;
  }
  hashv += (unsigned)(keylen);
  switch ( _hj_k ) {
     case 11: hashv += ( (unsigned)_hj_key[10] << 24 ); /* FALLTHROUGH */
     case 10: hashv += ( (unsigned)_hj_key[9] << 16 );  /* FALLTHROUGH */
     case 9:  hashv += ( (unsigned)_hj_key[8] << 8 );   /* FALLTHROUGH */
     case 8:  _hj_j += ( (unsigned)_hj_key[7] << 24 );  /* FALLTHROUGH */
     case 7:  _hj_j += ( (unsigned)_hj_key[6] << 16 );  /* FALLTHROUGH */
     case 6:  _hj_j += ( (unsigned)_hj_key[5] << 8 );   /* FALLTHROUGH */
     case 5:  _hj_j += _hj_key[4];                      /* FALLTHROUGH */
     case 4:  _hj_i += ( (unsigned)_hj_key[3] << 24 );  /* FALLTHROUGH */
     case 3:  _hj_i += ( (unsigned)_hj_key[2] << 16 );  /* FALLTHROUGH */
     case 2:  _hj_i += ( (unsigned)_hj_key[1] << 8 );   /* FALLTHROUGH */
     case 1:  _hj_i += _hj_key[0];
  }
  HASH_JEN_MIX(_hj_i, _hj_j, hashv);
  return hashv;
}

unsigned value_hash(const Value* v) {
  unsigned hashv = 0;
  switch (v->type) {
    case INT:
      hashv =  (unsigned)((Int*)v)->i;
      break;
    case FLOAT:
      /* Need to determine how C* deals with NaNs, Inf, denormals and close values */
      hashv = (unsigned)((Float*)v)->f;
      break;
    case STRING:
      hashv = hash_jen(((String*)v)->s, ((String*)v)->len);
      break;
    case MAP: {
      MapEntry* curr, * temp;
      if (!((Map*)v)->dirty) return ((Map*)v)->hashv;
      HASH_ITER(hh, ((Map*)v)->entries, curr, temp) {
        hashv ^= value_hash(curr->key) + 0x9e3779b9 + (hashv << 6) + (hashv >> 2);
        hashv ^= value_hash(curr->value) + 0x9e3779b9 + (hashv << 6) + (hashv >> 2);
      }
      ((Map*)v)->hashv = hashv;
      ((Map*)v)->dirty = 0;
    } break;
    case SET: {
      SetEntry* curr, * temp;
      if (!((Set*)v)->dirty) return ((Set*)v)->hashv;
      HASH_ITER(hh, ((Set*)v)->entries, curr, temp) {
        hashv ^= value_hash(curr->value) + 0x9e3779b9 + (hashv << 6) + (hashv >> 2);
      }
      ((Set*)v)->hashv = hashv;
      ((Set*)v)->dirty = 0;
    } break;
    default:
      break;
  }
  return hashv;
}

#define COMPARE(a, b) ((a) < (b) ? -1 : (a) > (b))

int value_compare(const Value* a, const Value* b) {
  if (a == b) return 0;
  if (a->type != b->type)  return a->type < b->type ? -1 : 1;

  switch (a->type) {
    case INT:
      return COMPARE((unsigned)((Int*)a)->i, (unsigned)((Int*)b)->i);
    case FLOAT:
      return COMPARE((unsigned)((Float*)a)->f, (unsigned)((Float*)b)->f);
    case STRING:
      if (((String*)a)->len != ((String*)b)->len) {
          return ((String*)a)->len < ((String*)b)->len ? -1 : 1;
      }
      return strncmp(((String*)a)->s, ((String*)b)->s, ((String*)a)->len);
    case MAP: {
      if (HASH_COUNT(((Map*)a)->entries) != HASH_COUNT(((Map*)b)->entries)) {
        return HASH_COUNT(((Map*)a)->entries) < HASH_COUNT(((Map*)b)->entries) ? -1 : 1;
      }
      MapEntry* i = ((Map*)a)->entries;
      MapEntry* j = ((Map*)b)->entries;
      while (i && j) {
        int r;
        r = value_compare(i->key, j->key);
        if (r != 0) return r;
        r = value_compare(i->value, j->value);
        if (r != 0) return r;
        i = (MapEntry*)i->hh.next;
        j = (MapEntry*)j->hh.next;
      }
    } break;
    case SET: {
      if (HASH_COUNT(((Set*)a)->entries) != HASH_COUNT(((Set*)b)->entries)) {
        return HASH_COUNT(((Set*)a)->entries) < HASH_COUNT(((Set*)b)->entries) ? -1 : 1;
      }
      SetEntry* i = ((Set*)a)->entries;
      SetEntry* j = ((Set*)b)->entries;
      while (i && j) {
        int r = value_compare(i->value, j->value);
        if (r != 0) return r;
        i = (SetEntry*)i->hh.next;
        j = (SetEntry*)j->hh.next;
      }
    } break;
    default:
      break;
  }
  return 0;
}

#undef COMPARE

Int* int_new(int i) {
  Int* num = (Int*)malloc(sizeof(Int));
  num->type = INT;
  num->i = i;
  return num;
}

Float* float_new(float f) {
  Float* num = (Float*)malloc(sizeof(Float));
  num->type = FLOAT;
  num->f = f;
  return num;
}

String* string_new(const char* s) {
  String* str = (String*)malloc(sizeof(String));
  size_t len = strlen(s);
  str->type = STRING;
  str->s = (char*)malloc(len + 1);
  str->len = len;
  strcpy(str->s, s);
  return str;
}

Map* map_new() {
  Map* map = (Map*)malloc(sizeof(Map));
  map->type = MAP;
  map->entries = NULL;
  map->dirty = 1;
  return map;
}

void map_add(Map* map, const Value* key, const Value* value) {
  MapEntry* entry;
  map->dirty = 1;
  HASH_FIND_PTR(map->entries, &key, entry);
  if (entry == NULL) {
    entry = (MapEntry*)malloc(sizeof(MapEntry));
    entry->key = key;
    HASH_ADD_PTR(map->entries, key, entry);
  }
  entry->value = value;
}

const Value* map_find(Map* map, const Value* key) {
  MapEntry* entry;
  HASH_FIND_PTR(map->entries, &key, entry);
  if (entry == NULL) return NULL;
  return entry->value;
}

Set* set_new() {
  Set* set = (Set*)malloc(sizeof(Set));
  set->type = SET;
  set->entries = NULL;
  set->dirty = 1;
  return set;
}

void set_add(Set* set, const Value* value) {
  SetEntry* entry;
  HASH_FIND_PTR(set->entries, &value, entry);
  if (entry == NULL) {
    entry = (SetEntry*)malloc(sizeof(SetEntry));
    entry->value = value;
    HASH_ADD_PTR(set->entries, value, entry);
    set->dirty = 1;
  }
}

int set_contains(Set* set, const Value* value) {
  SetEntry* entry;
  HASH_FIND_PTR(set->entries, &value, entry);
  return entry != NULL;
}

int main() {
  const Value* value;

  Map* map1 = map_new();
  Map* map2 = map_new();

  map_add(map1, (const Value*)string_new("cat"), (const Value*)int_new(1));
  map_add(map1, (const Value*)string_new("dog"), (const Value*)int_new(2));

  value = map_find(map1, (const Value*)string_new("cat"));
  assert(value != NULL && value->type == INT && ((Int*)value)->i == 1);

  value = map_find(map1, (const Value*)string_new("dog"));
  assert(value != NULL && value->type == INT && ((Int*)value)->i == 2);

  map_add(map2, (const Value*)map1, (const Value*)int_new(99));
  value = map_find(map2, (const Value*)map1);

  assert(value != NULL && value->type == INT && ((Int*)value)->i == 99);

  Set* set1 = set_new();
  Set* set2 = set_new();

  set_add(set1, (const Value*)string_new("monkey"));
  set_add(set1, (const Value*)string_new("lizard"));
  set_add(set1, (const Value*)string_new("beetle"));
  set_add(set1, (const Value*)map1);

  assert(set_contains(set1, (const Value*)map1));

  set_add(set2, (const Value*)string_new("monkey"));
  set_add(set2, (const Value*)string_new("lizard"));
  set_add(set2, (const Value*)string_new("beetle"));
  set_add(set2, (const Value*)map1);

  assert(set_contains(set2, (const Value*)map1));

  map_add(map1, (const Value*)set1, (const Value*)int_new(42));

  value = map_find(map1, (const Value*)set2);
  assert(value != NULL && value->type == INT && ((Int*)value)->i == 42);
}
