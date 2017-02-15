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


#include "helper.h"
#include "mode-private.h"
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

static inline int execsh ( const char *cmd )
{
    int  retv   = TRUE;
    char **args = NULL;
    int  argc   = 0;
    helper_parse_setup ( config.run_command, &args, &argc, "{cmd}", cmd, NULL );
    GError *error = NULL;
    g_spawn_async ( NULL, args, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error );
    if ( error != NULL ) {
        char *msg = g_strdup_printf ( "Failed to execute: '%s'\nError: '%s'", cmd, error->message );
        rofi_view_error_dialog ( msg, FALSE  );
        g_free ( msg );
        // print error.
        g_error_free ( error );
        retv = FALSE;
    }

    // Free the args list.
    g_strfreev ( args );
    return retv;
}
/**
 * The internal data structure holding the private data of the TEST Mode.
 */
typedef struct
{
    Node *current;
    Node *root;


    GList *directories;
    /** List if available test commands.*/
    char         **entry_list;
    /** Length of the #entry_list.*/
    unsigned int entry_list_length;

    char *current_name;
} TESTModePrivateData;


static void exec_test ( const char *command )
{
    if ( !command || !command[0] ) {
        return;
    }
}

static void node_set_current_name ( TESTModePrivateData *pd, Node *p )
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


static char ** get_test (  Mode *sw, unsigned int *length )
{
    TESTModePrivateData *rmpd = (TESTModePrivateData *) mode_get_private_data ( sw );
    char         **retv        = NULL;

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
                if( rmpd->root == NULL ){
                    rmpd->root = cur_dir;
                    rmpd->current = rmpd->root;
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
                        node->hotkey = g_strdup(start);
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
                        node->hotkey = g_strdup(start);
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
    return retv;
}


static int test_mode_init ( Mode *sw )
{
    if ( mode_get_private_data ( sw ) == NULL ) {
        TESTModePrivateData *pd = g_malloc0 ( sizeof ( *pd ) );
        mode_set_private_data ( sw, (void *) pd );
        pd->entry_list = get_test ( sw, &( pd->entry_list_length ) );
    }
    return TRUE;
}
static unsigned int test_mode_get_num_entries ( const Mode *sw )
{
    const TESTModePrivateData *rmpd = (const TESTModePrivateData *) mode_get_private_data ( sw );
    if ( rmpd->current == NULL ) {
        return 0;
    }
    return rmpd->current->num_children;
}

static ModeMode test_mode_result ( Mode *sw, int mretv, char **input, unsigned int selected_line )
{
    ModeMode           retv  = MODE_EXIT;
    TESTModePrivateData *rmpd = (TESTModePrivateData *) mode_get_private_data ( sw );
    if ( mretv & MENU_NEXT ) {
        retv = NEXT_DIALOG;
    }
    else if ( mretv & MENU_PREVIOUS ) {
        retv = PREVIOUS_DIALOG;
    }
    else if ( mretv & MENU_QUICK_SWITCH ) {
        retv = ( mretv & MENU_LOWER_MASK );
    }
    else if ( ( mretv & MENU_OK ) ) {
        Node *cur =rmpd->current->children[selected_line];
        switch( cur->type ) {
            case DIR_REF:
                {
                    for ( GList *iter = g_list_first ( rmpd->directories);
                            iter != NULL; iter = g_list_next ( iter )){
                        Node *d = iter->data;
                        if ( g_strcmp0(rmpd->current->children[selected_line]->name, d->name) ==  0){
                            d->parent = rmpd->current;
                            rmpd->current = d;
                            node_set_current_name ( rmpd, rmpd->current );
                            //rofi_view_update_prompt ();
                        }

                    }
                    retv = RESET_DIALOG;
                    break;
                }
            case ACT_REF:
                execsh ( rmpd->current->children[selected_line]->command);
                break;
            case GO_UP:
                {
                    if ( rmpd->current->parent  ) {
                        Node *d = rmpd->current->parent;
                        rmpd->current = d;
                        node_set_current_name ( rmpd, rmpd->current );
                        //rofi_view_clear_input ( rofi_view_get_active () );
                    }
                    retv = RESET_DIALOG;
                    break;
                }
            default:
                retv = RESET_DIALOG;


        }
    }
    else if ( ( mretv & MENU_CUSTOM_INPUT ) && *input != NULL && *input[0] != '\0' ) {
        exec_test ( *input );
    }
    else if ( ( mretv & MENU_ENTRY_DELETE ) ) {
        // Stay
        retv = RESET_DIALOG;
    }
    return retv;
}
static void test_mode_destroy ( Mode *sw )
{
    TESTModePrivateData *rmpd = (TESTModePrivateData *) mode_get_private_data ( sw );
    if ( rmpd != NULL ) {
        //        g_strfreev ( rmpd->entry_list );
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
    TESTModePrivateData *rmpd = (TESTModePrivateData *) mode_get_private_data ( sw );
    return get_entry ? node_get_display_string ( rmpd->current->children[selected_line]) : NULL;
}

static int test_token_match ( const Mode *sw, GRegex **tokens, unsigned int index )
{
    TESTModePrivateData *rmpd = (TESTModePrivateData *) mode_get_private_data ( sw );
    return token_match ( tokens, rmpd->current->children[index]->hotkey);
}

static char * test_process_input ( Mode *sw, const char *input )
{
    return g_strdup(input);
}

Mode mode =
{
    .abi_version        = ABI_VERSION,
    .name               = "blezz",
    .cfg_name_key       = "display-test",
    ._init              = test_mode_init,
    ._get_num_entries   = test_mode_get_num_entries,
    ._result            = test_mode_result,
    ._destroy           = test_mode_destroy,
    ._token_match       = test_token_match,
    ._get_display_value = _get_display_value,
    ._get_completion    = NULL,
    ._preprocess_input  = test_process_input,
    .private_data       = NULL,
    .free               = NULL
};
