#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bitmap.h"
#include "hash.h"
#include "list.h"

#define MAX_DS 10
#define NAME_LEN 32
#define LINE_LEN 256
#define TOKEN_MAX 8

#define hash_entry(HASH_ELEM, STRUCT, MEMBER) \
  list_entry (&(HASH_ELEM)->list_elem, STRUCT, MEMBER.list_elem)

#define DEFINE_FIND_USED_FUNC(FUNC_NAME, ARRAY_NAME, TYPE)                  \
  static TYPE *                                                             \
  FUNC_NAME (const char *name)                                              \
  {                                                                         \
    int i;                                                                  \
    for (i = 0; i < MAX_DS; i++)                                            \
      if (ARRAY_NAME[i].used && !strcmp (ARRAY_NAME[i].name, name))         \
        return &ARRAY_NAME[i];                                              \
    return NULL;                                                            \
  }

#define DEFINE_FIND_EMPTY_FUNC(FUNC_NAME, ARRAY_NAME)                       \
  static int                                                                \
  FUNC_NAME (void)                                                          \
  {                                                                         \
    int i;                                                                  \
    for (i = 0; i < MAX_DS; i++)                                            \
      if (!ARRAY_NAME[i].used)                                              \
        return i;                                                           \
    return -1;                                                              \
  }

struct list_item
{
  int value;
  struct list_elem elem;
};

struct hash_item
{
  int value;
  struct hash_elem elem;
};

struct list_obj
{
  char name[NAME_LEN];
  struct list list;
  bool used;
};

struct hash_obj
{
  char name[NAME_LEN];
  struct hash hash;
  bool used;
};

struct bitmap_obj
{
  char name[NAME_LEN];
  struct bitmap *bitmap;
  bool used;
};

enum ds_kind
{
  DS_NONE,
  DS_LIST,
  DS_HASH,
  DS_BITMAP
};

struct ds_ref
{
  enum ds_kind kind;
  void *obj;
};

static struct list_obj lists[MAX_DS];
static struct hash_obj hashes[MAX_DS];
static struct bitmap_obj bitmaps[MAX_DS];

static bool
parse_bool (const char *s)
{
  return s != NULL && !strcmp (s, "true");
}

static void
print_bool (bool value)
{
  printf ("%s\n", value ? "true" : "false");
}

static void
copy_name (char dst[NAME_LEN], const char *src)
{
  snprintf (dst, NAME_LEN, "%s", src);
}

DEFINE_FIND_USED_FUNC (find_list, lists, struct list_obj)
DEFINE_FIND_USED_FUNC (find_hash, hashes, struct hash_obj)
DEFINE_FIND_USED_FUNC (find_bitmap, bitmaps, struct bitmap_obj)

DEFINE_FIND_EMPTY_FUNC (find_empty_list_slot, lists)
DEFINE_FIND_EMPTY_FUNC (find_empty_hash_slot, hashes)
DEFINE_FIND_EMPTY_FUNC (find_empty_bitmap_slot, bitmaps)

static struct ds_ref
find_object (const char *name)
{
  struct list_obj *lobj = find_list (name);
  if (lobj != NULL) return (struct ds_ref) { DS_LIST, lobj };
  {
    struct hash_obj *hobj = find_hash (name);
    if (hobj != NULL) return (struct ds_ref) { DS_HASH, hobj };
  }
  {
    struct bitmap_obj *bobj = find_bitmap (name);
    if (bobj != NULL) return (struct ds_ref) { DS_BITMAP, bobj };
  }
  return (struct ds_ref) { DS_NONE, NULL };
}

static struct list_elem *
list_elem_at (struct list *list, int idx)
{
  struct list_elem *e = list_begin (list);
  int i;

  if (idx < 0 || (size_t) idx >= list_size (list)) return NULL;

  for (i = 0; i < idx; i++) e = list_next (e);
  return e;
}

static struct list_elem *
list_pos_at (struct list *list, int idx)
{
  if (idx < 0 || (size_t) idx > list_size (list)) return NULL;
  if ((size_t) idx == list_size (list)) return list_end (list);
  return list_elem_at (list, idx);
}

static bool
list_item_less (const struct list_elem *a,
                const struct list_elem *b,
                void *aux)
{
  const struct list_item *ia = list_entry (a, struct list_item, elem);
  const struct list_item *ib = list_entry (b, struct list_item, elem);
  (void) aux;
  return ia->value < ib->value;
}

static unsigned
hash_func (const struct hash_elem *e, void *aux)
{
  const struct hash_item *item = hash_entry (e, struct hash_item, elem);
  (void) aux;
  return hash_int (item->value);
}

static bool
hash_less (const struct hash_elem *a,
           const struct hash_elem *b,
           void *aux)
{
  const struct hash_item *ia = hash_entry (a, struct hash_item, elem);
  const struct hash_item *ib = hash_entry (b, struct hash_item, elem);
  (void) aux;
  return ia->value < ib->value;
}

static void
hash_destructor (struct hash_elem *e, void *aux)
{
  struct hash_item *item = hash_entry (e, struct hash_item, elem);
  (void) aux;
  free (item);
}

static void
hash_action_square (struct hash_elem *e, void *aux)
{
  struct hash_item *item = hash_entry (e, struct hash_item, elem);
  (void) aux;
  item->value *= item->value;
}

static void
hash_action_triple (struct hash_elem *e, void *aux)
{
  struct hash_item *item = hash_entry (e, struct hash_item, elem);
  (void) aux;
  item->value = item->value * item->value * item->value;
}

static void
destroy_list (struct list_obj *obj)
{
  while (!list_empty (&obj->list))
    {
      struct list_elem *e = list_pop_front (&obj->list);
      struct list_item *item = list_entry (e, struct list_item, elem);
      free (item);
    }
  obj->used = false;
}

static void
destroy_hash (struct hash_obj *obj)
{
  hash_destroy (&obj->hash, hash_destructor);
  obj->used = false;
}

static void
destroy_bitmap (struct bitmap_obj *obj)
{
  bitmap_destroy (obj->bitmap);
  obj->used = false;
}

static void
dump_list (struct list_obj *obj)
{
  struct list_elem *e;
  bool first = true;

  for (e = list_begin (&obj->list); e != list_end (&obj->list); e = list_next (e))
    {
      struct list_item *item = list_entry (e, struct list_item, elem);
      if (!first)
        printf (" ");
      printf ("%d", item->value);
      first = false;
    }
  printf ("\n");
}

static void
dump_hash (struct hash_obj *obj)
{
  struct hash_iterator it;
  struct hash_elem *e;
  bool first = true;

  hash_first (&it, &obj->hash);
  while ((e = hash_next (&it)) != NULL)
    {
      struct hash_item *item = hash_entry (e, struct hash_item, elem);
      if (!first) printf (" ");
      printf ("%d", item->value);
      first = false;
    }
  printf ("\n");
}

static void
dump_bitmap (struct bitmap_obj *obj)
{
  size_t i;
  size_t size = bitmap_size (obj->bitmap);

  for (i = 0; i < size; i++) printf ("%d", bitmap_test (obj->bitmap, i) ? 1 : 0);
  printf ("\n");
}

static void
create_list_object (const char *name)
{
  int idx = find_empty_list_slot ();
  if (idx < 0) return;

  lists[idx].used = true;
  copy_name (lists[idx].name, name);
  list_init (&lists[idx].list);
}

static void
create_hash_object (const char *name)
{
  int idx = find_empty_hash_slot ();
  if (idx < 0) return;

  hashes[idx].used = true;
  copy_name (hashes[idx].name, name);
  hash_init (&hashes[idx].hash, hash_func, hash_less, NULL);
}

static void
create_bitmap_object (const char *name, int bit_cnt)
{
  int idx = find_empty_bitmap_slot ();
  if (idx < 0) return;

  bitmaps[idx].bitmap = bitmap_create ((size_t) bit_cnt);
  if (bitmaps[idx].bitmap == NULL) return;

  bitmaps[idx].used = true;
  copy_name (bitmaps[idx].name, name);
}

static void
do_create (char **tokens, int count)
{
  if (count < 3) return;

  if (!strcmp (tokens[1], "list"))
    create_list_object (tokens[2]);
  else if (!strcmp (tokens[1], "hashtable"))
    create_hash_object (tokens[2]);
  else if (!strcmp (tokens[1], "bitmap") && count >= 4)
    create_bitmap_object (tokens[2], atoi (tokens[3]));
}

static void
do_delete (char *name)
{
  struct ds_ref ref = find_object (name);

  if (ref.kind == DS_LIST) destroy_list (ref.obj);
  else if (ref.kind == DS_HASH) destroy_hash (ref.obj);
  else if (ref.kind == DS_BITMAP) destroy_bitmap (ref.obj);
}

static void
do_dumpdata (char *name)
{
  struct ds_ref ref = find_object (name);

  if (ref.kind == DS_LIST) dump_list (ref.obj);
  else if (ref.kind == DS_HASH) dump_hash (ref.obj);
  else if (ref.kind == DS_BITMAP) dump_bitmap (ref.obj);
}

static struct list_item *
make_list_item (int value)
{
  struct list_item *item = malloc (sizeof *item);
  if (item != NULL) item->value = value;
  return item;
}

static struct hash_item *
make_hash_item (int value)
{
  struct hash_item *item = malloc (sizeof *item);
  if (item != NULL) item->value = value;
  return item;
}

static void
do_list_command (char **tokens, int count)
{
  struct list_obj *obj;

  if (count < 2) return;

  obj = find_list (tokens[1]);
  if (obj == NULL) return;

  if (!strcmp (tokens[0], "list_push_front") && count >= 3)
    {
      int value = atoi (tokens[2]);
      struct list_item *item = make_list_item (value);
      if (item != NULL) list_push_front (&obj->list, &item->elem);
    }
  else if (!strcmp (tokens[0], "list_push_back") && count >= 3)
    {
      int value = atoi (tokens[2]);
      struct list_item *item = make_list_item (value);
      if (item != NULL) list_push_back (&obj->list, &item->elem);
    }
  else if (!strcmp (tokens[0], "list_pop_front"))
    {
      if (!list_empty (&obj->list))
        {
          struct list_elem *e = list_pop_front (&obj->list);
          free (list_entry (e, struct list_item, elem));
        }
    }
  else if (!strcmp (tokens[0], "list_pop_back"))
    {
      if (!list_empty (&obj->list))
        {
          struct list_elem *e = list_pop_back (&obj->list);
          free (list_entry (e, struct list_item, elem));
        }
    }
  else if (!strcmp (tokens[0], "list_front"))
    {
      if (!list_empty (&obj->list))
        {
          struct list_item *item = list_entry (list_front (&obj->list), struct list_item, elem);
          printf ("%d\n", item->value);
        }
    }
  else if (!strcmp (tokens[0], "list_back"))
    {
      if (!list_empty (&obj->list))
        {
          struct list_item *item = list_entry (list_back (&obj->list), struct list_item, elem);
          printf ("%d\n", item->value);
        }
    }
  else if (!strcmp (tokens[0], "list_empty")) print_bool (list_empty (&obj->list));
  else if (!strcmp (tokens[0], "list_size")) printf ("%zu\n", list_size (&obj->list));
  else if (!strcmp (tokens[0], "list_insert") && count >= 4)
    {
      int idx = atoi (tokens[2]);
      int value = atoi (tokens[3]);
      struct list_elem *pos = list_pos_at (&obj->list, idx);
      struct list_item *item = make_list_item (value);
      if (item != NULL && pos != NULL) list_insert (pos, &item->elem);
      else free (item);
    }
  else if (!strcmp (tokens[0], "list_insert_ordered") && count >= 3)
    {
      int value = atoi (tokens[2]);
      struct list_item *item = make_list_item (value);
      if (item != NULL) list_insert_ordered (&obj->list, &item->elem, list_item_less, NULL);
    }
  else if (!strcmp (tokens[0], "list_remove") && count >= 3)
    {
      struct list_elem *e = list_elem_at (&obj->list, atoi (tokens[2]));
      if (e != NULL)
        {
          list_remove (e);
          free (list_entry (e, struct list_item, elem));
        }
    }
  else if (!strcmp (tokens[0], "list_reverse")) list_reverse (&obj->list);
  else if (!strcmp (tokens[0], "list_sort")) list_sort (&obj->list, list_item_less, NULL);
  else if (!strcmp (tokens[0], "list_splice") && count >= 6)
    {
      struct list_obj *src = find_list (tokens[3]);
      if (src != NULL)
        {
          struct list_elem *before = list_pos_at (&obj->list, atoi (tokens[2]));
          struct list_elem *first = list_elem_at (&src->list, atoi (tokens[4]));
          struct list_elem *last = list_pos_at (&src->list, atoi (tokens[5]));
          if (before != NULL && first != NULL && last != NULL) list_splice (before, first, last);
        }
    }
  else if (!strcmp (tokens[0], "list_unique"))
    {
      struct list *duplicates = NULL;
      if (count >= 3)
        {
          struct list_obj *dup_obj = find_list (tokens[2]);
          if (dup_obj != NULL)
            duplicates = &dup_obj->list;
        }
      list_unique (&obj->list, duplicates, list_item_less, NULL);
    }
  else if (!strcmp (tokens[0], "list_max"))
    {
      if (!list_empty (&obj->list))
        {
          struct list_item *item = list_entry (list_max (&obj->list, list_item_less, NULL), struct list_item, elem);
          printf ("%d\n", item->value);
        }
    }
  else if (!strcmp (tokens[0], "list_min"))
    {
      if (!list_empty (&obj->list))
        {
          struct list_item *item = list_entry (list_min (&obj->list, list_item_less, NULL), struct list_item, elem);
          printf ("%d\n", item->value);
        }
    }
  else if (!strcmp (tokens[0], "list_swap") && count >= 4)
    {
      struct list_elem *a = list_elem_at (&obj->list, atoi (tokens[2]));
      struct list_elem *b = list_elem_at (&obj->list, atoi (tokens[3]));
      if (a != NULL && b != NULL) list_swap (a, b);
    }
  else if (!strcmp (tokens[0], "list_shuffle")) list_shuffle (&obj->list);
}

static void
do_hash_command (char **tokens, int count)
{
  struct hash_obj *obj;

  if (count < 2) return;

  obj = find_hash (tokens[1]);
  if (obj == NULL) return;

  if (!strcmp (tokens[0], "hash_insert") && count >= 3)
    {
      struct hash_item *item = make_hash_item (atoi (tokens[2]));
      if (item != NULL && hash_insert (&obj->hash, &item->elem) != NULL) free (item);
    }
  else if (!strcmp (tokens[0], "hash_replace") && count >= 3)
    {
      struct hash_item *item = make_hash_item (atoi (tokens[2]));
      if (item != NULL)
        {
          struct hash_elem *old = hash_replace (&obj->hash, &item->elem);
          if (old != NULL) free (hash_entry (old, struct hash_item, elem));
        }
    }
  else if (!strcmp (tokens[0], "hash_delete") && count >= 3)
    {
      struct hash_item temp;
      struct hash_elem *found;
      temp.value = atoi (tokens[2]);
      found = hash_delete (&obj->hash, &temp.elem);
      if (found != NULL) free (hash_entry (found, struct hash_item, elem));
    }
  else if (!strcmp (tokens[0], "hash_find") && count >= 3)
    {
      struct hash_item temp;
      struct hash_elem *found;
      temp.value = atoi (tokens[2]);
      found = hash_find (&obj->hash, &temp.elem);
      if (found != NULL) printf ("%d\n", hash_entry (found, struct hash_item, elem)->value);
    }
  else if (!strcmp (tokens[0], "hash_size")) printf ("%zu\n", hash_size (&obj->hash));
  else if (!strcmp (tokens[0], "hash_empty")) print_bool (hash_empty (&obj->hash));
  else if (!strcmp (tokens[0], "hash_clear")) hash_clear (&obj->hash, hash_destructor);
  else if (!strcmp (tokens[0], "hash_apply") && count >= 3)
    {
      if (!strcmp (tokens[2], "square")) hash_apply (&obj->hash, hash_action_square);
      else if (!strcmp (tokens[2], "triple")) hash_apply (&obj->hash, hash_action_triple);
    }
}

static void
do_bitmap_command (char **tokens, int count)
{
  struct bitmap_obj *obj;

  if (count < 2) return;

  obj = find_bitmap (tokens[1]);
  if (obj == NULL) return;

  if (!strcmp (tokens[0], "bitmap_mark") && count >= 3)
    {
      size_t idx = (size_t) atoi (tokens[2]);
      bitmap_mark (obj->bitmap, idx);
    }
  else if (!strcmp (tokens[0], "bitmap_reset") && count >= 3)
    {
      size_t idx = (size_t) atoi (tokens[2]);
      bitmap_reset (obj->bitmap, idx);
    }
  else if (!strcmp (tokens[0], "bitmap_flip") && count >= 3)
    {
      size_t idx = (size_t) atoi (tokens[2]);
      bitmap_flip (obj->bitmap, idx);
    }
  else if (!strcmp (tokens[0], "bitmap_set") && count >= 4)
    {
      size_t idx = (size_t) atoi (tokens[2]);
      bool value = parse_bool (tokens[3]);
      bitmap_set (obj->bitmap, idx, value);
    }
  else if (!strcmp (tokens[0], "bitmap_test") && count >= 3)
    {
      size_t idx = (size_t) atoi (tokens[2]);
      print_bool (bitmap_test (obj->bitmap, idx));
    }
  else if (!strcmp (tokens[0], "bitmap_set_all") && count >= 3)
    {
      bool value = parse_bool (tokens[2]);
      bitmap_set_all (obj->bitmap, value);
    }
  else if (!strcmp (tokens[0], "bitmap_set_multiple") && count >= 5)
    {
      size_t start = (size_t) atoi (tokens[2]);
      size_t cnt = (size_t) atoi (tokens[3]);
      bool value = parse_bool (tokens[4]);
      bitmap_set_multiple (obj->bitmap, start, cnt, value);
    }
  else if (!strcmp (tokens[0], "bitmap_count") && count >= 5)
    {
      size_t start = (size_t) atoi (tokens[2]);
      size_t cnt = (size_t) atoi (tokens[3]);
      bool value = parse_bool (tokens[4]);
      printf ("%zu\n", bitmap_count (obj->bitmap, start, cnt, value));
    }
  else if (!strcmp (tokens[0], "bitmap_contains") && count >= 5)
    {
      size_t start = (size_t) atoi (tokens[2]);
      size_t cnt = (size_t) atoi (tokens[3]);
      bool value = parse_bool (tokens[4]);
      print_bool (bitmap_contains (obj->bitmap, start, cnt, value));
    }
  else if (!strcmp (tokens[0], "bitmap_any") && count >= 4)
    {
      size_t start = (size_t) atoi (tokens[2]);
      size_t cnt = (size_t) atoi (tokens[3]);
      print_bool (bitmap_any (obj->bitmap, start, cnt));
    }
  else if (!strcmp (tokens[0], "bitmap_none") && count >= 4)
    {
      size_t start = (size_t) atoi (tokens[2]);
      size_t cnt = (size_t) atoi (tokens[3]);
      print_bool (bitmap_none (obj->bitmap, start, cnt));
    }
  else if (!strcmp (tokens[0], "bitmap_all") && count >= 4)
    {
      size_t start = (size_t) atoi (tokens[2]);
      size_t cnt = (size_t) atoi (tokens[3]);
      print_bool (bitmap_all (obj->bitmap, start, cnt));
    }
  else if (!strcmp (tokens[0], "bitmap_scan") && count >= 5)
    {
      size_t start = (size_t) atoi (tokens[2]);
      size_t cnt = (size_t) atoi (tokens[3]);
      bool value = parse_bool (tokens[4]);
      printf ("%zu\n", bitmap_scan (obj->bitmap, start, cnt, value));
    }
  else if (!strcmp (tokens[0], "bitmap_scan_and_flip") && count >= 5)
    {
      size_t start = (size_t) atoi (tokens[2]);
      size_t cnt = (size_t) atoi (tokens[3]);
      bool value = parse_bool (tokens[4]);
      printf ("%zu\n", bitmap_scan_and_flip (obj->bitmap, start, cnt, value));
    }
  else if (!strcmp (tokens[0], "bitmap_size")) printf ("%zu\n", bitmap_size (obj->bitmap));
  else if (!strcmp (tokens[0], "bitmap_dump")) bitmap_dump (obj->bitmap);
  else if (!strcmp (tokens[0], "bitmap_expand") && count >= 3)
    {
      struct bitmap *expanded = bitmap_expand (obj->bitmap, atoi (tokens[2]));
      if (expanded != NULL) obj->bitmap = expanded;
    }
}

static void
cleanup_all_objects (void)
{
  int i;
  for (i = 0; i < MAX_DS; i++)
    {
      if (lists[i].used) destroy_list (&lists[i]);
      if (hashes[i].used) destroy_hash (&hashes[i]);
      if (bitmaps[i].used) destroy_bitmap (&bitmaps[i]);
    }
}

int
main (void)
{
  char line[LINE_LEN];

  while (fgets (line, sizeof line, stdin) != NULL)
    {
      char *tokens[TOKEN_MAX];
      int count = 0;
      char *tok = strtok (line, " \n");

      while (tok != NULL && count < TOKEN_MAX)
        {
          tokens[count++] = tok;
          tok = strtok (NULL, " \n");
        }

      if (count == 0) continue;

      if (!strcmp (tokens[0], "quit")) break;
      else if (!strcmp (tokens[0], "create")) do_create (tokens, count);
      else if (!strcmp (tokens[0], "delete") && count >= 2) do_delete (tokens[1]);
      else if (!strcmp (tokens[0], "dumpdata") && count >= 2) do_dumpdata (tokens[1]);
      else if (!strncmp (tokens[0], "list_", 5)) do_list_command (tokens, count);
      else if (!strncmp (tokens[0], "hash_", 5)) do_hash_command (tokens, count);
      else if (!strncmp (tokens[0], "bitmap_", 7)) do_bitmap_command (tokens, count);
    }

  cleanup_all_objects ();

  return 0;
}
