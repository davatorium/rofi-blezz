/**
 * rofi-blezz
 *
 * MIT/X11 License
 * Copyright (c) 2017 Qball Cow <qball@gmpclient.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <errno.h>
#include <gmodule.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rofi/helper.h>
#include <rofi/mode.h>
#include <rofi/mode-private.h>
#include <rofi/rofi-icon-fetcher.h>

G_MODULE_EXPORT Mode mode;

typedef enum { DIRECTORY, DIR_REF, ACT_REF, GO_UP } NodeType;

typedef enum {
  QUIT,
  RELOAD,
} ActExitMode;

typedef struct _Node {
  /** Type of the node */
  NodeType type;

  /** Hotkey */
  char *hotkey;
  /** name */
  char *name;
  /** Command */
  char *command;

  /** Icon to display */
  char *icon;

  /** Exit mode */
  ActExitMode exit_mode;

  /* UID for the icon to display */
  uint32_t icon_fetch_uid;

  /** Parent (when called) */
  struct _Node *parent;
  /** Children (only if type == DIRECTORY) */
  struct _Node **children;
  size_t num_children;
} Node;

/**
 * The internal data structure holding the private data of the TEST Mode.
 */
typedef struct {
  /** Current visible node */
  Node *current;
  /** List of all directories specified. */
  GList *directories;
  /** Name to show as prompt. */
  char *current_name;
} BLEZZModePrivateData;

static void node_free(Node *p) {
  g_free(p->hotkey);
  g_free(p->name);
  g_free(p->command);
  g_free(p->icon);
  for (size_t i = 0; i < p->num_children; i++) {
    node_free(p->children[i]);
  }

  g_free(p->children);
  g_free(p);
}

static void node_set_current_name(BLEZZModePrivateData *pd, Node *p) {
  GList *list = NULL;
  Node *iter = p;
  int count = 0;
  size_t length = 0;
  do {
    list = g_list_prepend(list, iter);
    length += strlen(iter->name);
    iter = iter->parent;
    count++;
  } while (iter);
  g_free(pd->current_name);
  if (count == 0 || length == 0) {
    pd->current_name = g_strdup("blezz");
    mode.display_name = pd->current_name;
    return;
  }
  pd->current_name = g_malloc0((count + length + 1) * sizeof(char));
  size_t index = 0;
  for (GList *i = g_list_first(list); i != NULL; i = g_list_next(i)) {
    Node *n = i->data;
    size_t l = strlen(n->name);
    g_strlcpy(&(pd->current_name[index]), n->name, l + 1);
    index += l;
    pd->current_name[index] = '>';
    index++;
  }
  pd->current_name[index - 1] = '\0';
  g_list_free(list);
  mode.display_name = pd->current_name;
}

static void node_child_add(Node *p, Node *c) {
  if (p == NULL || c == NULL)
    return;
  p->children = g_realloc(p->children, (p->num_children + 1) * sizeof(Node *));
  p->children[p->num_children] = c;
  p->num_children++;
}

static Node *blezz_parse_dir_node(char *start) {
  Node *node = NULL;
  char **strv = g_strsplit(start, ",", 3);
  if (strv && strv[0] && strv[1]) {
    node = g_malloc0(sizeof(Node));
    node->type = DIR_REF;
    node->hotkey = g_strstrip(g_utf8_strdown(strv[0], -1));
    node->name = g_strstrip(g_strdup(strv[1]));
    if (strv[2] != NULL) {
      node->icon = g_strstrip(g_strdup(strv[2]));
    }
  }
  g_strfreev(strv);
  return node;
}
static Node *blezz_parse_act_node(char *start, gboolean reload) {
  Node *node = NULL;
  char **strv = g_strsplit(start, ",", 4);
  if (strv && strv[0] && strv[1] && strv[2]) {
    node = g_malloc0(sizeof(Node));
    node->type = ACT_REF;
    node->hotkey = g_utf8_strdown(strv[0], -1);
    node->name = g_strdup(strv[1]);
    node->command = g_strdup(strv[2]);
    if (strv[3] != NULL) {
      node->icon = g_strstrip(g_strdup(strv[3]));
    }
    node->exit_mode = (reload) ? RELOAD : QUIT;
  }
  g_strfreev(strv);
  return node;
}

static void get_blezz(Mode *sw) {
  BLEZZModePrivateData *rmpd =
      (BLEZZModePrivateData *)mode_get_private_data(sw);
  char *dpath = "~/.config/blezz/content";
  find_arg_str("-blezz-config", &dpath);
  char *path = rofi_expand_path(dpath);
  FILE *fp = fopen(path, "r");
  if (fp != NULL) {
    char *buffer = NULL;
    size_t buffer_length = 0;
    ssize_t rread = 1;
    Node *cur_dir = NULL;
    for (rread = getline(&buffer, &buffer_length, fp); rread > 0;
         rread = getline(&buffer, &buffer_length, fp)) {
      if (buffer[rread - 1] == '\n') {
        buffer[rread - 1] = '\0';
        rread--;
      }
      if (buffer[rread - 1] == ':') {
        buffer[rread - 1] = '\0';

        Node *node = g_malloc0(sizeof(Node));
        node->name = g_strstrip(g_strdup(buffer));
        node->type = DIRECTORY;
        cur_dir = node;
        if (rmpd->current == NULL) {
          rmpd->current = cur_dir;
          node_set_current_name(rmpd, rmpd->current);
        }
        rmpd->directories = g_list_append(rmpd->directories, node);
      } else if (strncmp(buffer, "dir", 3) == 0) {
        if (cur_dir) {
          char *start = g_strstr_len(buffer, rread, "(");
          char *end = g_strrstr(buffer, ")");
          if (start && end) {
            start++;
            *end = '\0';
            Node *node = blezz_parse_dir_node(start);
            node_child_add(cur_dir, node);
          }
        }
      } else if (strncmp(buffer, "actReload", 9) == 0) {
        if (cur_dir) {
          char *start = g_strstr_len(buffer, rread, "(");
          char *end = g_strrstr(buffer, ")");
          if (start && end) {
            start++;
            *end = '\0';
            Node *node = blezz_parse_act_node(start, TRUE);
            node_child_add(cur_dir, node);
          }
        }
      } else if (strncmp(buffer, "act", 3) == 0) {
        if (cur_dir) {
          char *start = g_strstr_len(buffer, rread, "(");
          char *end = g_strrstr(buffer, ")");
          if (start && end) {
            start++;
            *end = '\0';
            Node *node = blezz_parse_act_node(start, FALSE);
            node_child_add(cur_dir, node);
          }
        }
      }
    }
    if (buffer) {
      free(buffer);
    }

    fclose(fp);
  }

  for (GList *iter = g_list_first(rmpd->directories); iter != NULL;
       iter = g_list_next(iter)) {
    Node *n = g_malloc0(sizeof(Node));
    n->type = GO_UP;
    n->hotkey = g_strdup(".");
    n->icon = g_strdup("go-up");
    node_child_add((Node *)(iter->data), n);
  }

  g_free(path);
}
static void blezz_mode_set_directory(BLEZZModePrivateData *rmpd,
                                     char const *const name) {
  for (GList *iter = g_list_first(rmpd->directories); iter != NULL;
       iter = g_list_next(iter)) {
    Node *d = iter->data;
    if (g_strcmp0(name, d->name) == 0) {
      if (rmpd->current == d) {
        return;
      }
      d->parent = rmpd->current;
      rmpd->current = d;
      node_set_current_name(rmpd, rmpd->current);
      return;
    }
  }
}

static int blezz_mode_init(Mode *sw) {
  if (mode_get_private_data(sw) == NULL) {
    BLEZZModePrivateData *pd = g_malloc0(sizeof(*pd));
    mode_set_private_data(sw, (void *)pd);
    get_blezz(sw);
    char *dir_switch = NULL;
    if (find_arg_str("-blezz-directory", &dir_switch) >= 0) {
      if (dir_switch != NULL) {
        blezz_mode_set_directory(pd, dir_switch);
      }
    }
  }
  return TRUE;
}
static unsigned int blezz_mode_get_num_entries(const Mode *sw) {
  const BLEZZModePrivateData *rmpd =
      (const BLEZZModePrivateData *)mode_get_private_data(sw);
  if (rmpd->current == NULL) {
    return 0;
  }
  return rmpd->current->num_children;
}

static ModeMode blezz_mode_result(Mode *sw, int mretv, char **input,
                                  unsigned int selected_line) {
  ModeMode retv = MODE_EXIT;
  BLEZZModePrivateData *rmpd =
      (BLEZZModePrivateData *)mode_get_private_data(sw);
  if (mretv & MENU_NEXT) {
    retv = NEXT_DIALOG;
  } else if (mretv & MENU_PREVIOUS) {
    retv = PREVIOUS_DIALOG;
  } else if (mretv & MENU_QUICK_SWITCH) {
    retv = (mretv & MENU_LOWER_MASK);
  } else if ((mretv & MENU_OK)) {
    Node *cur = rmpd->current->children[selected_line];
    switch (cur->type) {
    case DIR_REF: {
      blezz_mode_set_directory(rmpd, cur->name);
      retv = RESET_DIALOG;
      break;
    }
    case ACT_REF:
      helper_execute_command(NULL, cur->command, FALSE, NULL);
      if (cur->exit_mode == RELOAD)
        retv = RESET_DIALOG;
      break;
    case GO_UP: {
      if (rmpd->current->parent) {
        Node *d = rmpd->current->parent;
        rmpd->current = d;
        node_set_current_name(rmpd, rmpd->current);
        retv = RESET_DIALOG;
      }
      break;
    }
    default:
      retv = RESET_DIALOG;
    }
  }
  return retv;
}

static void blezz_mode_destroy(Mode *sw) {
  BLEZZModePrivateData *rmpd =
      (BLEZZModePrivateData *)mode_get_private_data(sw);
  if (rmpd != NULL) {
    g_list_foreach(rmpd->directories, (GFunc)node_free, NULL);
    g_list_free(rmpd->directories);
    g_free(rmpd->current_name);
    g_free(rmpd);
    mode_set_private_data(sw, NULL);
  }
}

static char *node_get_display_string(Node *node) {
  switch (node->type) {
  case DIR_REF:
    return g_strdup_printf("/ [%s] %s", node->hotkey, node->name);
  case ACT_REF:
    return g_strdup_printf("~ [%s] %s", node->hotkey, node->name);
  case GO_UP:
    return g_strdup("< [.] Back");
  default:
    return g_strdup("Error");
  }
}

static char *_get_display_value(const Mode *sw, unsigned int selected_line,
                                G_GNUC_UNUSED int *state, GList **l,
                                int get_entry) {
  BLEZZModePrivateData *rmpd =
      (BLEZZModePrivateData *)mode_get_private_data(sw);
  return get_entry
             ? node_get_display_string(rmpd->current->children[selected_line])
             : NULL;
}

static int blezz_token_match(const Mode *sw, rofi_int_matcher **tokens,
                             unsigned int index) {
  BLEZZModePrivateData *rmpd =
      (BLEZZModePrivateData *)mode_get_private_data(sw);
  return helper_token_match(tokens, rmpd->current->children[index]->hotkey);
}

static cairo_surface_t *blezz_get_icon(const Mode *sw,
                                       unsigned int selected_line, int height) {
  BLEZZModePrivateData *pd = (BLEZZModePrivateData *)mode_get_private_data(sw);
  if (pd->current == NULL) {
    return NULL;
  }
  Node *node = pd->current->children[selected_line];
  if (node->icon != NULL) {
    if (node->icon_fetch_uid > 0) {
      cairo_surface_t *icon = rofi_icon_fetcher_get(node->icon_fetch_uid);
      return icon;
    }
    node->icon_fetch_uid = rofi_icon_fetcher_query(node->icon, height);
    cairo_surface_t *icon = rofi_icon_fetcher_get(node->icon_fetch_uid);
    return icon;
  }
  return NULL;
}

Mode mode = {
    .abi_version = ABI_VERSION,
    .name = "blezz",
    .cfg_name_key = "display-test",
    ._init = blezz_mode_init,
    ._get_num_entries = blezz_mode_get_num_entries,
    ._result = blezz_mode_result,
    ._destroy = blezz_mode_destroy,
    ._token_match = blezz_token_match,
    ._get_icon = blezz_get_icon,
    ._get_display_value = _get_display_value,
    ._get_completion = NULL,
    ._preprocess_input = NULL,
    .private_data = NULL,
    .free = NULL,
};
