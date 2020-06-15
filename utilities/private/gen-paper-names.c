/* STLWRT - A modern implementation of GTK+ 2 capable of running GTK+ 3 applications
 * Copyright (C) 2006 Matthias Clasen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>

#include "paper_names.c"

static const gint n_infos = G_N_ELEMENTS (standard_names);
static const gint n_extra = G_N_ELEMENTS (extra_ppd_names);

typedef struct {
  const gchar *s;
  gint         len;
  gint         suffix;
  gint         offset;
} NameInfo;

static NameInfo *names = NULL;
static gint n_names = 0;

static void
add_name (const gchar *name)
{
  names[n_names].s = name;
  names[n_names].len = strlen (name);
  names[n_names].suffix = -1;
  names[n_names].offset = 0;

  n_names++;
}

static gint
find_name (const gchar *name)
{
  gint i;

  if (!name)
    return -1;

  for (i = 0; i < n_names; i++)
    {
      if (strcmp (names[i].s, name) == 0)
        return names[i].offset;
    }

  fprintf (stderr, "BOO! %s not found\n", name);

  return -2;
}

#define MM_PER_INCH 25.4
#define POINTS_PER_INCH 72

static gboolean
parse_media_size (const gchar *size,
                  gdouble     *width_mm, 
                  gdouble     *height_mm)
{
  const gchar *p;
  gchar *e;
  gdouble short_dim, long_dim;

  p = size;
  
  short_dim = g_ascii_strtod (p, &e);

  if (p == e || *e != 'x')
    return FALSE;

  p = e + 1; /* Skip x */

  long_dim = g_ascii_strtod (p, &e);

  if (p == e)
    return TRUE;

  p = e;

  if (strcmp (p, "in") == 0)
    {
      short_dim = short_dim * MM_PER_INCH;
      long_dim = long_dim * MM_PER_INCH;
    }
  else if (strcmp (p, "mm") != 0)
    return FALSE;

  if (width_mm)
    *width_mm = short_dim;
  if (height_mm)
    *height_mm = long_dim;
  
  return TRUE;  
}

int 
main (int argc, char *argv[])
{
  gint i, j, offset;
  gdouble width, height;

  names = (NameInfo *) malloc (sizeof (NameInfo) * (4 + n_infos + 2 * n_extra));
  n_names = 0;

  /* collect names */
  for (i = 0; i < n_infos; i++)
    {
      add_name (standard_names[i].name);
      add_name (standard_names[i].display_name);
      if (standard_names[i].ppd_name)
        add_name (standard_names[i].ppd_name);
    }

  for (i = 0; i < n_extra; i++)
    {
      add_name (extra_ppd_names[i].ppd_name);
      add_name (extra_ppd_names[i].standard_name);
    }

  /* find suffixes and dupes */
  for (i = 0; i < n_names; i++)
    for (j = 0; j < n_names; j++)
      {
        if (i == j) continue;

        if (names[i].len < names[j].len ||
            (names[i].len == names[j].len && j < i))
          {
            if (strcmp (names[i].s, names[j].s + names[j].len - names[i].len) == 0)
              {
                names[i].suffix = j;
                break;
              }
          }
      }

  /* calculate offsets for regular entries */
  offset = 0;
  for (i = 0; i < n_names; i++)
    {
      if (names[i].suffix == -1)
        {
          names[i].offset = offset;
          offset += names[i].len + 1;
        }
    }

  /* calculate offsets for suffixes */
  for (i = 0; i < n_names; i++)
    {
      if (names[i].suffix != -1)
        {
          j = i;
          do {
            j = names[j].suffix;
          } while (names[j].suffix != -1);
          names[i].offset = names[j].offset + names[j].len - names[i].len;
        }
    }

  printf ("/* Generated by gen-paper-names */\n\n");

  /* write N_ marked names */

  printf ("#if 0\n");
  for (i = 0; i < n_infos; i++)
    printf ("NC_(\"paper size\", \"%s\")\n", standard_names[i].display_name);
  printf ("#endif\n\n");

  /* write strings */
  printf ("static const char paper_names[] =");
  for (i = 0; i < n_names; i++)
    {
      if (names[i].suffix == -1)
        printf ("\n  \"%s\\0\"", names[i].s);
    }
  printf (";\n\n");

  /* dump PaperInfo array */
  printf ("typedef struct {\n"
          "  int name;\n"
          "  float width;\n"
          "  float height;\n"
          "  int display_name;\n"
          "  int ppd_name;\n"
          "} PaperInfo;\n\n"
          "static const PaperInfo standard_names_offsets[] = {\n");

  for (i = 0; i < n_infos; i++)
    {
      width = height = 0.0;
      if (!parse_media_size (standard_names[i].size, &width, &height))
        printf ("failed to parse size %s\n", standard_names[i].size);

      printf ("  { %4d, %g, %g, %4d, %4d },\n",
              find_name (standard_names[i].name),
              width, height,
              find_name (standard_names[i].display_name),
              find_name (standard_names[i].ppd_name));
    }

  printf ("};\n\n");


  /* dump extras */

  printf ("static const struct {\n"
          "  int ppd_name;\n"
          "  int standard_name;\n"
          "} extra_ppd_names_offsets[] = {\n");

  for (i = 0; i < n_extra; i++)
    {
      printf ("  { %4d, %4d },\n",
              find_name (extra_ppd_names[i].ppd_name),
              find_name (extra_ppd_names[i].standard_name));
    }

  printf ("};\n\n");

  return 0;
}
