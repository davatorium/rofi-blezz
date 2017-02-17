#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <dirent.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <gmodule.h>


#include <rofi/mode.h>
#include <rofi/helper.h>
#include <rofi/mode-private.h>
#include <rofi/settings.h>

G_MODULE_EXPORT Mode mode;

typedef enum {
    DIRECTORY,
    DIR_REF,
    ACT_REF,
    GO_UP
} NodeType;

typedef struct _Node {
    NodeType type;

    /** Hotkey */
    char *hotkey;
    /** name */
    char *name;
    /** Command */
    char *command;

    struct _Node *parent;
    struct _Node **children;
    size_t num_children;
}Node;

/**
 * The internal data structure holding the private data of the TEST Mode.
 */
typedef struct
{
    Node *current;
    GList *directories;
    char *current_name;
} BLEZZModePrivateData;

static void node_free ( Node *p )
{
    g_free ( p->hotkey );
    g_free ( p->name );
    g_free ( p->command );
    for ( size_t i = 0; i < p->num_children; i++ ) {
        node_free ( p->children[i]);
    }

    g_free ( p->children );
    g_free ( p );
}

static void node_set_current_name ( BLEZZModePrivateData *pd, Node *p )
{
    GList *list = NULL;
    Node *iter = p;
    int count = 0;
    size_t length = 0;
    do {
        list = g_list_prepend ( list, iter );
        length += strlen(iter->name);
        iter = iter->parent;
        count++;
    }while ( iter );
    g_free ( pd->current_name );
    if ( count == 0 || length == 0){
        pd->current_name = g_strdup("blezz");
        mode.display_name = pd->current_name;
        return;
    }
    pd->current_name = g_malloc0 ( (count+length+1)*sizeof(char));
    size_t index = 0;
    for ( GList *i = g_list_first (list); i != NULL; i = g_list_next(i))
    {
        Node *n = i->data;
        size_t l = strlen(n->name);
        g_strlcpy(&(pd->current_name[index]), n->name, l+1 );
        index+=l;
        pd->current_name[index] = '>';
        index++;
    }
    pd->current_name[index-1] = '\0';
    g_list_free(list);
    mode.display_name = pd->current_name;
}

static void node_child_add ( Node *p, Node *c )
{
    p->children = g_realloc (p->children, (p->num_children+1)*sizeof(Node*));
    p->children[p->num_children] = c;
    p->num_children++;
}

static char ** get_blezz (  Mode *sw )
{
    BLEZZModePrivateData *rmpd = (BLEZZModePrivateData *) mode_get_private_data ( sw );

    char *path = rofi_expand_path ( "~/.config/blezz/content");
    FILE *fp = fopen ( path, "r" );
    if ( fp != NULL ) {
        char *buffer = NULL;
        size_t buffer_length = 0;
        ssize_t rread = 1;
        Node * cur_dir = NULL;
        for ( rread = getline ( &buffer, &buffer_length, fp); rread > 0 ; rread = getline ( &buffer, &buffer_length, fp ) )
        {
            if ( buffer[rread-1] == '\n' ){
                buffer[rread-1] = '\0';
                rread--;
            }
            if ( buffer[rread-1] == ':') {
                buffer[rread-1] = '\0';

                Node *node = g_malloc0 ( sizeof ( Node ) );
                node->name = g_strdup(buffer);
                node->type = DIRECTORY;
                cur_dir = node;
                if( rmpd->current == NULL ){
                    rmpd->current = cur_dir;
                    node_set_current_name ( rmpd, rmpd->current );
                }
                rmpd->directories = g_list_append ( rmpd->directories, node );
            }
            else if ( strncmp ( buffer, "dir", 3) == 0 ){
                if ( cur_dir ){
                    char *start = g_strstr_len(buffer, rread, "(");
                    char *end = g_strrstr(buffer, ")");
                    if ( start && end ){
                        Node *node = g_malloc0 ( sizeof ( Node ) );
                        node->type = DIR_REF;
                        start++;
                        *end = '\0';
                        end = g_strstr_len (start, -1, ",");
                        *end = '\0';
                        node->hotkey = g_utf8_strdown(start,-1);
                        start = end+1;
                        node->name = g_strdup(start);
                        node_child_add ( cur_dir, node );
                    }
                }
            } else if ( strncmp ( buffer, "act", 3 ) == 0 ) {
                if ( cur_dir ){
                    char *start = g_strstr_len(buffer, rread, "(");
                    char *end = g_strrstr(buffer, ")");
                    if ( start && end ){
                        Node *node = g_malloc0 ( sizeof ( Node ) );
                        node->type = ACT_REF;
                        start++;
                        *end = '\0';
                        end = g_strstr_len (start, -1, ",");
                        *end = '\0';
                        node->hotkey = g_utf8_strdown(start,-1);
                        start = end+1;
                        end = g_strstr_len (start, -1, ",");
                        *end = '\0';
                        node->name = g_strdup(start);
                        start = end+1;
                        node->command = g_strdup(start);

                        node_child_add ( cur_dir, node );
                    }

                }
            }
        }
        if (buffer ) {
            free(buffer);
        }

        fclose ( fp );
    }

    for ( GList *iter = g_list_first ( rmpd->directories);
            iter != NULL; iter = g_list_next ( iter )){
        Node *n = g_malloc0 ( sizeof(Node));
        n->type = GO_UP;
        n->hotkey = g_strdup(".");
        node_child_add ( (Node *)(iter->data), n );
    }

    g_free(path);
}

static int blezz_mode_init ( Mode *sw )
{
    if ( mode_get_private_data ( sw ) == NULL ) {
        BLEZZModePrivateData *pd = g_malloc0 ( sizeof ( *pd ) );
        mode_set_private_data ( sw, (void *) pd );
        get_blezz ( sw );
    }
    return TRUE;
}
static unsigned int blezz_mode_get_num_entries ( const Mode *sw )
{
    const BLEZZModePrivateData *rmpd = (const BLEZZModePrivateData *) mode_get_private_data ( sw );
    if ( rmpd->current == NULL ) {
        return 0;
    }
    return rmpd->current->num_children;
}

static ModeMode blezz_mode_result ( Mode *sw, int mretv, char **input, unsigned int selected_line )
{
    ModeMode           retv  = MODE_EXIT;
    BLEZZModePrivateData *rmpd = (BLEZZModePrivateData *) mode_get_private_data ( sw );
    if ( mretv & MENU_NEXT ) {
        retv = NEXT_DIALOG;
    } else if ( mretv & MENU_PREVIOUS ) {
        retv = PREVIOUS_DIALOG;
    } else if ( mretv & MENU_QUICK_SWITCH ) {
        retv = ( mretv & MENU_LOWER_MASK );
    } else if ( ( mretv & MENU_OK ) ) {
        Node *cur =rmpd->current->children[selected_line];
        switch( cur->type ) {
            case DIR_REF:
                {
                    for ( GList *iter = g_list_first ( rmpd->directories);
                            iter != NULL; iter = g_list_next ( iter )){
                        Node *d = iter->data;
                        if ( g_strcmp0(cur->name, d->name) ==  0){
                            d->parent = rmpd->current;
                            rmpd->current = d;
                            node_set_current_name ( rmpd, rmpd->current );
                            break;
                        }
                    }
                    retv = RESET_DIALOG;
                    break;
                }
            case ACT_REF:
                helper_execute_command ( NULL, cur->command, FALSE );
                break;
            case GO_UP:
                {
                    if ( rmpd->current->parent  ) {
                        Node *d = rmpd->current->parent;
                        rmpd->current = d;
                        node_set_current_name ( rmpd, rmpd->current );
                    }
                    retv = RESET_DIALOG;
                    break;
                }
            default:
                retv = RESET_DIALOG;
        }
    }
    return retv;
}

static void blezz_mode_destroy ( Mode *sw )
{
    BLEZZModePrivateData *rmpd = (BLEZZModePrivateData *) mode_get_private_data ( sw );
    if ( rmpd != NULL ) {
        for ( GList *i = g_list_first (rmpd->directories); i != NULL; i = g_list_next(i)){
            Node *n = (Node *)i->data;
            node_free ( n );
        }
        g_list_free ( rmpd->directories );

        g_free ( rmpd->current_name );
        g_free ( rmpd );
        mode_set_private_data ( sw, NULL );
    }
}

static char *node_get_display_string ( Node *node )
{
    switch ( node->type )
    {
        case DIR_REF:
            return g_strdup_printf("/ [%s] %s", node->hotkey, node->name);
        case ACT_REF:
            return g_strdup_printf("~ [%s] %s", node->hotkey, node->name);
        case GO_UP:
            return g_strdup ("< [.] Back");
        default:
            return g_strdup("Error");
    }
}

static char *_get_display_value ( const Mode *sw, unsigned int selected_line, G_GNUC_UNUSED int *state, int get_entry )
{
    BLEZZModePrivateData *rmpd = (BLEZZModePrivateData *) mode_get_private_data ( sw );
    return get_entry ? node_get_display_string ( rmpd->current->children[selected_line]) : NULL;
}

static int blezz_token_match ( const Mode *sw, GRegex **tokens, unsigned int index )
{
    BLEZZModePrivateData *rmpd = (BLEZZModePrivateData *) mode_get_private_data ( sw );
    return helper_token_match ( tokens, rmpd->current->children[index]->hotkey);
}

Mode mode =
{
    .abi_version        = ABI_VERSION,
    .name               = "blezz",
    .cfg_name_key       = "display-test",
    ._init              = blezz_mode_init,
    ._get_num_entries   = blezz_mode_get_num_entries,
    ._result            = blezz_mode_result,
    ._destroy           = blezz_mode_destroy,
    ._token_match       = blezz_token_match,
    ._get_display_value = _get_display_value,
    ._get_completion    = NULL,
    ._preprocess_input  = NULL, 
    .private_data       = NULL,
    .free               = NULL,
};
