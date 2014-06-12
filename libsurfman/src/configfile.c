/*
 * Copyright (c) 2012 Citrix Systems, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "project.h"

/*
 * Very simple configuration file parser.
 * It should be enough for all our requirements.
 *
 * We shouldn't require any locking because we never free any
 * entries.
 */

struct config_entry
{
  LIST_ENTRY(struct config_entry) link;

  char *key;
  char *value;
};

static LIST_HEAD (, struct config_entry) config_entry_list;

static void
insert_entry (char *key, char *value)
{
  struct config_entry *e;

  e = xmalloc (sizeof (*e));
  e->key = key;
  e->value = value;

  LIST_INSERT_HEAD (&config_entry_list, e, link);
}

EXTERNAL const char *
config_get (const char *prefix, const char *key)
{
  const char *v = NULL;
  struct config_entry *e;
  char buff[128];

  /* Assume key is never larger than 128 bytes */

  if (prefix)
    {
      snprintf (buff, 128, "%s.%s", prefix, key);
      key = buff;
    }

  LIST_FOREACH (e, &config_entry_list, link)
    {
      if (!strcmp (key, e->key))
        {
          v = e->value;
          break;
        }
    }

  return v;
}

EXTERNAL const char *
config_dump (void)
{
  struct config_entry *e;

  surfman_info ("=== DUMP ===");

  LIST_FOREACH(e, &config_entry_list, link)
    {
      surfman_info ("key=%s\tvalue=%s", e->key, e->value);
    }

  surfman_info ("=== END OF DUMP ===");
}

EXTERNAL int
config_load_file (const char *filename)
{
  FILE *f;
  char *line = NULL;
  size_t bufsz;
  int lineno = 0;
  int len;
  int rc = 0;

  f = fopen (filename, "r");

  if (!f)
    {
      surfman_error ("Failed to open %s: %s", filename, strerror (errno));
      return -1;
    }

  while ((len = getline (&line, &bufsz, f)) != -1)
    {
      char *key;
      char *value;

      lineno++;

      /* Remove newline and stuff */
      while (len && isspace (line[len - 1]))
        {
          line[len - 1] = '\0';
          len--;
        }

      /* Skip empty lines */
      if (!len)
        continue;

      /* Skip full comment lines */
      if (line[0] == '#')
        continue;


      /* key=value, discard comments */
      if (sscanf (line, " %m[^= ] = \"%m[^\"]\"", &key, &value) == 2 ||
          sscanf (line, " %m[^= ] = '%m[^\']'", &key, &value) == 2 ||
          sscanf (line, " %m[^= ] = %m[^;#]", &key, &value) == 2)
        {
          /* Handle empty string case */
          if (!strcmp (value, "\"\"") || !strcmp (value, "''"))
            value[0] = '\0';

          insert_entry (key, value);
          rc++;
        }
      /* Handle empty value weird cases */
      else if (sscanf (line, " %m[^= ] = %m[;#]", &key, &value) == 2 ||
               sscanf (line, " %m[^= ] %m[=]", &key, &value) == 2)
        {
          value[0] = '\0';

          insert_entry (key, value);
          rc++;
        }
      else
        {
          surfman_error ("Parse error at line %d: \"%s\"", lineno, line);
        }

    }

  if (line)
    free (line);

  return rc;
}
