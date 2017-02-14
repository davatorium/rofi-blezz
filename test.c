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

typedef enum {
    DIRECTORY,
    DIR_REF,
    ACT_REF

} NodeType;
typedef struct _Node {
    NodeType type;

    /** Hotkey */
    char *hostkey;
    /** name */
    char *name;

    /** Command */
    char *command;

    struct _Node *children;
    size_t num_children;
}Node;
/**
 * The internal data structure holding the private data of the TEST Mode.
 */
typedef struct
{
    Node *current;
    Node *root;


    GHashTable *directories;
    /** List if available test commands.*/
    char         **entry_list;
    /** Length of the #entry_list.*/
    unsigned int entry_list_length;
} TESTModePrivateData;

int list_mode = 0;
char *lista[] = {
    "Developer",
    "Options",
    "Applications",
    "Systems"
};
char *listb[] = {
    "Crapton",
    "Blaat",
};

static void exec_test ( const char *command )
{
    if ( !command || !command[0] ) {
        return;
    }
}

static void delete_test ( const char *command )
{
    if ( !command || !command[0] ) {
        return;
    }
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
                buffer[rread] = '\0';

                Node *node = g_malloc0 ( sizeof ( Node ) );
                node->name = g_strdup(buffer);
                node->type = DIRECTORY;
                cur_dir = node;
                if( rmpd->root == NULL ){
                    rmpd->root = cur_dir;
                }
            }
            else if ( strncmp ( buffer, "dir", 3) == 0 ){
                if ( cur_dir ){
                
                }
            } else if ( strncmp ( buffer, "act", 3 ) == 0 ) {
                if ( cur_dir ){
                
                }
            }

        }
        if (buffer ) {
            free(buffer);
        }
    
        fclose ( fp );
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
    else if ( ( mretv & MENU_OK ) && rmpd->entry_list[selected_line] != NULL ) {
//        exec_test ( rmpd->entry_list[selected_line] );
        retv = RELOAD_DIALOG;        
    }
    else if ( ( mretv & MENU_CUSTOM_INPUT ) && *input != NULL && *input[0] != '\0' ) {
        exec_test ( *input );
    }
    else if ( ( mretv & MENU_ENTRY_DELETE ) && rmpd->entry_list[selected_line] ) {
        delete_test ( rmpd->entry_list[selected_line] );
        g_strfreev ( rmpd->entry_list );
        rmpd->entry_list_length = 0;
        rmpd->entry_list        = NULL;
        // Stay
        retv = RELOAD_DIALOG;
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

static char *_get_display_value ( const Mode *sw, unsigned int selected_line, G_GNUC_UNUSED int *state, int get_entry )
{
    TESTModePrivateData *rmpd = (TESTModePrivateData *) mode_get_private_data ( sw );
    return get_entry ? g_strdup ( rmpd->entry_list[selected_line] ) : NULL;
}

static int test_token_match ( const Mode *sw, GRegex **tokens, unsigned int index )
{
    TESTModePrivateData *rmpd = (TESTModePrivateData *) mode_get_private_data ( sw );
    return token_match ( tokens, rmpd->entry_list[index] );
}

static char * test_process_input ( Mode *sw, const char *input )
{
    printf("%s\n", input);
    return g_strdup(input);
}
#include "mode-private.h"
G_MODULE_EXPORT Mode mode =
{
    .abi_version        = ABI_VERSION,
    .name               = "test",
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
