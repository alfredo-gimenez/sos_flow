/*
 *   sosa.c   Library functions for writing SOS analytics modules.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

#include <mpi.h>

#include "sos.h"
#include "sosd.h"
#include "sosa.h"
#include "sos_types.h"
#include "sos_debug.h"

SOSA_runtime SOSA;


void SOSA_exec_query(char *query, SOSA_results *results) {
    SOS_SET_CONTEXT(SOSA.sos_context, "SOSA_exec_query");

    if (results == NULL) {
        dlog(0, "ERROR: Attempted to exec a query w/NULL results object!\n");
        exit(EXIT_FAILURE);
    }

    return;
}



void SOSA_results_to_buffer(SOS_buffer *buffer, SOSA_results *results) {
    SOS_SET_CONTEXT(SOSA.sos_context, "SOSA_results_to_buffer");

    return;
}



void SOSA_results_from_buffer(SOSA_results *results, SOS_buffer *buffer) {
    SOS_SET_CONTEXT(SOSA.sos_context, "SOSA_results_from_buffer");

    return;
}




void SOSA_results_output_to(FILE *fptr, SOSA_results *results, char *title, int options) {
    SOS_SET_CONTEXT(SOSA.sos_context, "SOSA_results_output_to");

    if (fptr == NULL) {
        dlog(0, "ERROR: (FILE *) fptr == NULL!\n");
        exit(EXIT_FAILURE);
    }
    if (results == NULL) {
        dlog(0, "ERROR: (SOSA_results *) results == NULL!\n");
        exit(EXIT_FAILURE);
    }
    if (title == NULL) {
        dlog(0, "ERROR: (char *) title == NULL!\n");
        exit(EXIT_FAILURE);
    }

    int output_mode;
    if (options & SOSA_OUTPUT_JSON) {
        output_mode = SOSA_OUTPUT_JSON;
    } else {
        output_mode = SOSA_OUTPUT_DEFAULT;  // CSV
    }

    int    row = 0;
    int    col = 0;
    double time_now = 0.0;
    SOS_TIME(time_now);

    switch(output_mode) {
    case SOSA_OUTPUT_JSON:
        
        fprintf(fptr, "{\"title\"      : \"%s\",\n",  title);
        fprintf(fptr, " \"time_stamp\" : \"%lf\",\n", time_now);
        fprintf(fptr, " \"col_count\"  : \"%d\",\n",  results->col_count);
        fprintf(fptr, " \"row_count\"  : \"%d\",\n",  results->row_count);
        fprintf(fptr, " \"data\"       : [\n");

        for (row = 0; row < results->row_count; row++) {
            fprintf(fptr, "\t{\n"); //row:begin
            fprintf(fptr, "\t\"row\": \"%d\"\n", row);

            for (col = 0; col < results->col_count; col++) {
                fprintf(fptr, "\t\t\"%s\": \"%s\"", results->col_names[col], results->data[row][col]);
                if (col < (results->col_count - 1)) {
                    fprintf(fptr, ",\n");
                } else {
                    fprintf(fptr, "\n");
                }//if

            }//for:col
            fprintf(fptr, "\t}"); //row:end
            if (row < (results->row_count - 1)) {
                fprintf(fptr, ",\n");
            } else {
                fprintf(fptr, "\n");
            }//if
        }//for:row
        fprintf(fptr, " ]\n");  //data
        fprintf(fptr, "}\n");   //json

        fflush(fptr);

        break;



    default://OUTPUT_CSV:
        // Display header (optional)
        if (options & SOSA_OUTPUT_W_HEADER) {
            for (col = 0; col < results->col_count; col++) {
                fprintf(fptr, "\"%s\"", results->col_names[col]);
                if (col == (results->col_count - 1)) { fprintf(fptr, "\n"); }
                else { fprintf(fptr, ","); }
            }//for:col
        }//if:header

        // Display data
        for (row = 0; row < results->row_count; row++) {
            for (col = 0; col < results->col_count; col++) {
                fprintf(fptr, "\"%s\"", results->data[row][col]);
                if (col == (results->col_count - 1)) { fprintf(fptr, "\n"); }
                else { fprintf(fptr, ","); }
            }//for:col
        }//for:row
        break;

    }//select

    return;
}







// NOTE: Only call this when you already hold the uid->lock
void SOSA_guid_request(SOS_uid *uid) {
    SOS_SET_CONTEXT(SOSA.sos_context, "SOSA_guid_request");

    dlog(7, "Obtaining new guid range...\n");

    SOS_buffer *msg;
    SOS_buffer *reply;
    SOS_buffer_init_sized_locking(SOS, &msg,   256, false);
    SOS_buffer_init_sized_locking(SOS, &reply, 256, false);

    SOS_msg_header header;
    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_GUID_BLOCK;
    header.msg_from = SOSA.world_rank;
    header.pub_guid = 0;

    int offset = 0;
    SOS_buffer_pack(msg, &offset, "iigg",
                    header.msg_size,
                    header.msg_type,
                    header.msg_from,
                    header.pub_guid);

    header.msg_size = offset;
    offset = 0;
    SOS_buffer_pack(msg, &offset, "i",
                    header.msg_size);

    SOSA_send_to_target_db(msg, reply);

    if (reply->len < (2 * sizeof(double))) {
        dlog(0, "WARNING: Malformed UID reply from sosd (db) ...\n");
        uid->next = -1;
        uid->last = 0;
    } else {
        offset = 0;
        SOS_buffer_unpack(reply, &offset, "gg",
                          &uid->next,
                          &uid->last);
    }

    dlog(7, "    ... %" SOS_GUID_FMT " -> %" SOS_GUID_FMT " assigned.  Done.\n", uid->next, uid->last);

    SOS_buffer_destroy(msg);
    SOS_buffer_destroy(reply);

    return;
}





void SOSA_results_init(SOS_runtime *sos_context, SOSA_results **results_object_ptraddr) {
    SOS_SET_CONTEXT(SOSA.sos_context, "SOSA_results_init");
    int col = 0;
    int row = 0;

    dlog(7, "Allocating space for a new results set...\n");

    SOSA_results *results = *results_object_ptraddr = (SOSA_results *) calloc(1, sizeof(SOSA_results));

    results->sos_context = SOS;

    results->col_count   = 0;
    results->row_count   = 0;
    results->col_max     = SOSA_DEFAULT_RESULT_COL_MAX;
    results->row_max     = SOSA_DEFAULT_RESULT_ROW_MAX;

    results->data = (char ***) calloc(results->row_max, sizeof(char **));
    for (row = 0; row < results->row_max; row++) {
        results->data[row] = (char **) calloc(results->col_max, sizeof(char *));
        for (col = 0; col < results->col_max; col++) {
            results->data[row][col] = NULL;
        }
    }

    results->col_names = (char **) calloc(results->col_max, sizeof(char *));
    for (col = 0; col < results->col_max; col++) {
        results->col_names[col] = NULL;
    }

    dlog(7, "    ... results->col_max = %d\n", results->col_max);
    dlog(7, "    ... results->row_max = %d\n", results->row_max);
    dlog(7, "    ... done.\n");

    return;
}


void SOSA_results_grow_to(SOSA_results *results, int new_col_max, int new_row_max) {
    SOS_SET_CONTEXT(SOSA.sos_context, "SOSA_results_grow_to");
    int row;
    int col;

    if ((new_col_max <= results->col_max) && (new_row_max <= results->row_max)) {
        dlog(7, "NOTE: results->data[%d][%d] can already handle requested size[%d][%d].\n",
             results->row_max, results->col_max,
             new_row_max, new_col_max );
        dlog(7, "NOTE: Nothing to do, returning.\n");
        return;
    } else {
        dlog(7, "Growing results->data[row_max:%d][col_max:%d] to handle size[row_max:%d][col_max:%d] ...\n",
             results->row_max, results->col_max,
             new_row_max, new_col_max );
    }

    if (new_col_max > results->col_max) {
        // Add column space to column names...
        results->col_names = (char **) realloc(results->col_names, (new_col_max * sizeof(char *)));
        // Initialize it.
        for (col = results->col_max; col < new_col_max; col++) {
            results->col_names[col] = NULL;
        }

        // Add column space to existing rows...
        for (row = 0; row < results->row_max; row++) {
            results->data[row] = (char **) realloc(results->data[row], (new_col_max * sizeof(char *)));
            // Initialize it.
            for (col = results->col_max; col < new_col_max; col++) {
                results->data[row][col] = NULL;
            }
        }
        results->col_max = new_col_max;
    }
    

    if (new_row_max > results->row_max) {
        // Add additional rows space
        results->data = (char ***) realloc(results->data, (new_row_max * sizeof(char **)));
        // For each new row...
        for (row = results->row_max; row < new_row_max; row++) {
            // ...add space for columns
            results->data[row] = (char **) calloc(results->col_max, sizeof(char **));
            for (col = 0; col < results->col_max; col++) {
                // ...and initialize each one.
                results->data[row][col] = NULL;
            }
        }
        results->row_max = new_row_max;
    }

    dlog(7, "    ... done.\n");
    return;
}


void SOSA_results_wipe(SOSA_results *results) {
    SOS_SET_CONTEXT(SOSA.sos_context, "SOSA_results_wipe");

    int row = 0;
    int col = 0;

    dlog(7, "Wiping results set...\n");
    dlog(7, "    ... results->col_max = %d\n", results->col_max);
    dlog(7, "    ... results->row_max = %d\n", results->row_max);

    for (row = 0; row < results->row_max; row++) {
        for (col = 0; col < results->col_max; col++) {
            if (results->data[row][col] != NULL) {
                free(results->data[row][col]);
                results->data[row][col] = NULL;
            }
        }
    }

    for (col = 0; col < results->col_max; col++) {
        if (results->col_names[col] != NULL) {
            free(results->col_names[col]);
            results->col_names[col] = NULL;
        }
    }

    results->col_count = 0;
    results->row_count = 0;

    dlog(7, "    ... done.\n");

    return;
}


// NOTE: Better to wipe and re-use a small results table if possible rather
//       than malloc/free a lot.  But... whatever works best.
void SOSA_results_destroy(SOSA_results *results) {
    SOS_SET_CONTEXT(SOSA.sos_context, "SOSA_results_destroy");

    dlog(7, "Destroying results set...\n");
    dlog(7, "    ... results->col_count == %d of %d\n", results->col_count, results->col_max);
    dlog(7, "    ... results->row_count == %d of %d\n", results->row_count, results->row_max);

    int row = 0;
    int col = 0;
    for (row = 0; row < results->row_count; row++) {
        for (col = 0; col < results->col_count; col++) {
            free(results->data[row][col]);
        }
    }

    for (row = 0; row < results->row_max; row++) {
        free(results->data[row]);
    }

    free(results->data);

    for (col = 0; col < results->col_max; col++) {
        if (results->col_names[col] != NULL) {
            free(results->col_names[col]);
        }
    }
    free(results->col_names);
    free(results);

    dlog(7, "    ... done.\n");

    return;
}





void SOSA_finalize(void) {
    SOS_SET_CONTEXT(SOSA.sos_context, "SOSA_finalize");    

    // TODO: Clean up any SOS.sos_context stuff?

    free(SOSA.analytics_locales);
    free(SOSA.world_roles);
    free(SOSA.world_hosts);

    return;
}



void SOSA_send_to_target_db(SOS_buffer *msg, SOS_buffer *reply) {
    SOS_SET_CONTEXT(SOSA.sos_context, "SOSA_send_to_target_db");

    if ((msg == NULL) || (reply == NULL)) {
        dlog(0, "ERROR: Buffer pointer supplied with NULL value!\n");
        exit(EXIT_FAILURE);
    }

    SOS_buffer *wrapper;
    SOS_buffer_init_sized_locking(SOS, &wrapper, (1 + msg->len + sizeof(int)), false);

    // All messages to DB ranks come with a wrapper that supports multiple-message
    // packing. We treat this function as a 1:1::call:message packager, so just
    // pack a 1 in it and call it good.
    int offset = 0;
    SOS_buffer_pack(wrapper, &offset, "i", 1);

    // Copy the message memory directly into the wrapper's data area:
    memcpy((wrapper->data + offset), msg->data, msg->len);
    wrapper->len += msg->len;

    dlog(7, "Sending message of %d bytes...\n", wrapper->len);
    MPI_Ssend((void *) wrapper->data, wrapper->len, MPI_CHAR, SOSA.db_target_rank, 0, MPI_COMM_WORLD);


    dlog(7, "Waiting for a reply...\n");
    MPI_Status status;
    int msg_waiting = 0;
    do {
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &msg_waiting, &status);
        usleep(1000);
    } while (msg_waiting == 0);

    int mpi_reply_len = -1;
    MPI_Get_count(&status, MPI_CHAR, &mpi_reply_len);

    while(reply->max < mpi_reply_len) {
        SOS_buffer_grow(reply, (1 + (mpi_reply_len - reply->max)), SOS_WHOAMI);
    }

    MPI_Recv((void *) reply->data, mpi_reply_len, MPI_CHAR, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);
    reply->len = mpi_reply_len;
    dlog(7, "  ... reply of %d bytes received from rank %d!\n", mpi_reply_len, status.MPI_SOURCE);
    dlog(7, "Done.\n");

    return;
}


SOS_runtime* SOSA_init(int *argc, char ***argv, int unique_color) {
    SOSA.sos_context = (SOS_runtime *) malloc(sizeof(SOS_runtime));

    SOSA.sos_context->role = SOS_ROLE_ANALYTICS;
    SOSA.sos_context->status = SOS_STATUS_RUNNING;
    SOSA.sos_context->config.argc = *argc;
    SOSA.sos_context->config.argv = *argv;

    MPI_Init_thread(argc, argv, MPI_THREAD_MULTIPLE, &SOSA.sos_context->config.comm_support);

    int world_size = -1;
    int world_rank = -1;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    // Information about this rank.
    int   my_role;
    char *my_host;
    int   my_host_name_len;

    // Target arrays for MPI_Allgather of roles and host names.
    int  *world_roles;
    char *world_hosts;

    // WORLD DISCOVER: ----------
    //   (includes ANALYTICS ranks)
    my_host     = (char *) calloc(MPI_MAX_PROCESSOR_NAME, sizeof(char));
    world_hosts = (char *) calloc(world_size * (MPI_MAX_PROCESSOR_NAME), sizeof(char));
    world_roles =  (int *) calloc(world_size, sizeof(int));
    my_role = SOSA.sos_context->role;
    MPI_Get_processor_name(my_host, &my_host_name_len);
    MPI_Allgather((void *) &my_role, 1, MPI_INT, world_roles, 1, MPI_INT, MPI_COMM_WORLD);
    MPI_Allgather((void *) my_host, MPI_MAX_PROCESSOR_NAME, MPI_CHAR,
                  (void *) world_hosts, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, MPI_COMM_WORLD);

    SOSA.world_rank  = world_rank;
    SOSA.world_size  = world_size;
    SOSA.world_roles = world_roles;
    SOSA.world_hosts = world_hosts;

    // SPLIT: -------------------
    //   (ANALYTICS ranks peel off into their own communicator)

    SOSA.analytics_color = unique_color;
    MPI_Comm_split(MPI_COMM_WORLD, SOSA.analytics_color, world_rank, &SOSA.comm);
    MPI_Comm_size(SOSA.comm, &SOSA.sos_context->config.comm_size);
    MPI_Comm_rank(SOSA.comm, &SOSA.sos_context->config.comm_rank);

    SOS_SET_CONTEXT(SOSA.sos_context, "SOSA_init");

    dlog(0, "Bringing analytics module online...\n");
    dlog(0, "    ... SOSA.analytics_color     == %d\n", SOSA.analytics_color);
    int i;

    // Count the number of database roles  (The 'i' index == MPI rank)
    SOSA.db_role_count = 0;
    for (i = 0; i < world_size; i++) {
        if (world_roles[i] == SOS_ROLE_DB) {
            SOSA.db_role_count++;
        }
    }

    dlog(0, "    ... SOSA.db_role_count       == %d\n", SOSA.db_role_count);

    // Construct a list of sosd database role MPI ranks:
    if (SOSA.db_role_count < 1) {
        fprintf(stderr, "SOSA ERROR: No database roles were discovered!\n");
        exit(EXIT_FAILURE);
    } else {
        int found_db_index = 0;
        SOSA.db_role_ranks = (int *) calloc(SOSA.db_role_count, sizeof(int));
        for (i = 0; i < world_size; i++) {
            if (world_roles[i] == SOS_ROLE_DB) {
                SOSA.db_role_ranks[found_db_index++] = i;
                dlog(0, "    ... SOSA.db_role_ranks[%3d]  == %d\n", (found_db_index - 1), i);
            }
        }
    }

    // See if we're aligned with a database rank:
    SOSA.db_target_rank             = -1;
    SOS->config.locale = -1;
    for (i = 0; i < world_size; i++) {
        if (world_roles[i] == SOS_ROLE_DB) {
            if (strncmp(my_host, (world_hosts + (i * MPI_MAX_PROCESSOR_NAME)), MPI_MAX_PROCESSOR_NAME) == 0) {
                // We're on the same node as this database...
                SOSA.db_target_rank = i;
                break;
            }
        }
    }

    dlog(0, "    ... SOSA.db_target_rank      == %d (MPI_COMM_WORLD)\n", SOSA.db_target_rank);

    if (SOSA.db_target_rank == -1) {
        SOS->config.locale = SOS_LOCALE_INDEPENDENT;
        // Give this rank a database to talk to for GUID-request purposes...
        SOSA.db_target_rank = (SOS->config.comm_rank % SOSA.db_role_count);
        dlog(0, "    ... SOSA.sos_context->config.locale == SOS_LOCALE_INDEPENDENT\n");
    } else {
        SOS->config.locale = SOS_LOCALE_DAEMON_DBMS;
        dlog(0, "    ... SOSA.sos_context->config.locale == SOS_LOCALE_DAEMON_DBMS\n");
    }

    // ANALYTICS discover: ------
    SOSA.analytics_locales = (int *) calloc(SOS->config.comm_size, sizeof(int));
    MPI_Allgather((void *) &SOS->config.locale, 1, MPI_INT,
                  (void *) SOSA.analytics_locales, 1, MPI_INT, SOSA.comm);



    // Pick up some GUIDs from our target db:
    SOS_uid_init(SOS, &SOS->uid.my_guid_pool, -1, -1);
    SOS_uid_init(SOS, &SOS->uid.local_serial, 0, SOS_DEFAULT_UID_MAX);
    SOSA_guid_request(SOS->uid.my_guid_pool);

    dlog(0, "   ... SOSA.sos_context->uid.my_guid_pool == %" SOS_GUID_FMT " -> %" SOS_GUID_FMT "\n",
         SOS->uid.my_guid_pool->next,
         SOS->uid.my_guid_pool->last);

    dlog(0, "   ... done.\n");
    return SOSA.sos_context;
}




