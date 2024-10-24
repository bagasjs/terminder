#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "sqlite3/sqlite3.h"

typedef struct Args {
    const char **items;
    size_t count;
} Args;

const char *shift_args(Args *args, const char *on_empty_error_message)
{
    if(args->count == 0) {
        fprintf(stderr, "ERROR: %s\n", on_empty_error_message);
        exit(EXIT_FAILURE);
    }
    const char *result = args->items[0];
    args->items += 1;
    args->count -= 1;
    return result;
}

typedef struct Terminder {
    sqlite3 *db;
    sqlite3_stmt *stmt;
} Terminder;

int terminder_init(Terminder *tm)
{
    if(!tm) return -1;

    if(sqlite3_open("terminder.sqlite3", &tm->db) != 0) {
        fprintf(stderr, "ERROR: Failed to open database: %s\n", sqlite3_errmsg(tm->db));
        sqlite3_close(tm->db);
        return -1;
    }

    const char *check_table_sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='tasks';";
    sqlite3_stmt *stmt;
    if(sqlite3_prepare_v2(tm->db, check_table_sql, -1, &stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "ERROR: Failed to prepare statement: %s\n", sqlite3_errmsg(tm->db));
        sqlite3_close(tm->db);
        return -1;
    }

    int table_exists = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    if(!table_exists) {
        const char *schema = 
            "CREATE TABLE tasks (\n"
            "   id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,\n"
            "   title VARCHAR(255) NOT NULL,\n"
            "   deadline DATETIME NOT NULL,\n"
            "   completed INTEGER NOT NULL DEFAULT 0\n"
            ")\n";

        if(sqlite3_exec(tm->db, schema, 0, 0, 0) != SQLITE_OK) {
            fprintf(stderr, "ERROR: %s\n", sqlite3_errmsg(tm->db));
            sqlite3_close(tm->db);
            return -1;
        }
    }

    return 0;
}

void terminder_deinit(Terminder *terminder)
{
    if(terminder->stmt != NULL) {
        sqlite3_finalize(terminder->stmt);
    }
    sqlite3_close(terminder->db);
}

// TODO(bagasjs)
// Add option for 
// -n The amount of tasks to be shown default is 20
// -E Show all tasks including the completed one
void terminder_command_show(Terminder *tm, Args *args)
{
    const char *show_task_info_sql = "SELECT * FROM tasks WHERE completed = 0 AND deadline > DATETIME('NOW') ORDER BY deadline ASC;";
    if(sqlite3_prepare_v2(tm->db, show_task_info_sql, -1, &tm->stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "ERROR: Failed to prepare statement: %s\n", sqlite3_errmsg(tm->db));
        terminder_deinit(tm);
        exit(EXIT_FAILURE);
    }

    if(sqlite3_step(tm->stmt) == SQLITE_ROW) {
        printf("Upcoming tasks:\n");
        do {
            int id = sqlite3_column_int(tm->stmt, 0);
            const unsigned char *title = sqlite3_column_text(tm->stmt, 1);
            const unsigned char *deadline = sqlite3_column_text(tm->stmt, 2);
            printf("%d\t%s %s\n", id, deadline, title);
        } while(sqlite3_step(tm->stmt) == SQLITE_ROW);
    } else {
        printf("There's no upcoming task!");
    }
    sqlite3_finalize(tm->stmt);
    tm->stmt = NULL;
}

// TODO(bagasjs): Adding a simpler way to define deadline
// i.e. terminder add "Hello World" 
void terminder_command_add(Terminder *tm, Args *args)
{
    const char *title = shift_args(args, "Expecting the title of the task");
    const char *deadline = shift_args(args, "Expecting the deadline of the task");

    const char *insert_task_sql = "INSERT INTO tasks (title, deadline) VALUES (?, ?);";
    if(sqlite3_prepare_v2(tm->db, insert_task_sql, -1, &tm->stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "ERROR: Failed to prepare statement: %s\n", sqlite3_errmsg(tm->db));
        terminder_deinit(tm);
        exit(EXIT_FAILURE);
    }
    if(sqlite3_bind_text(tm->stmt, 1, title, strlen(title), 0) != SQLITE_OK) {
        fprintf(stderr, "ERROR: Failed to bind parameter: %s\n", sqlite3_errmsg(tm->db));
        terminder_deinit(tm);
        exit(EXIT_FAILURE);
    }
    if(sqlite3_bind_text(tm->stmt, 2, deadline, strlen(deadline), 0) != SQLITE_OK) {
        fprintf(stderr, "ERROR: Failed to bind parameter: %s\n", sqlite3_errmsg(tm->db));
        terminder_deinit(tm);
        exit(EXIT_FAILURE);
    }

    if(sqlite3_step(tm->stmt) == SQLITE_ERROR) {
        fprintf(stderr, "ERROR: Failed to execute statement: %s\n", sqlite3_errmsg(tm->db));
        terminder_deinit(tm);
        exit(EXIT_FAILURE);
    }
}

void terminder_command_check(Terminder *tm, Args *args)
{
    const char *id= shift_args(args, "Expecting the id of the task");

    const char *insert_task_sql = "UPDATE tasks SET completed=1 WHERE id=?;";
    if(sqlite3_prepare_v2(tm->db, insert_task_sql, -1, &tm->stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "ERROR: Failed to prepare statement: %s\n", sqlite3_errmsg(tm->db));
        terminder_deinit(tm);
        exit(EXIT_FAILURE);
    }
    if(sqlite3_bind_text(tm->stmt, 1, id, strlen(id), 0) != SQLITE_OK) {
        fprintf(stderr, "ERROR: Failed to bind parameter: %s\n", sqlite3_errmsg(tm->db));
        terminder_deinit(tm);
        exit(EXIT_FAILURE);
    }

    if(sqlite3_step(tm->stmt) == SQLITE_ERROR) {
        fprintf(stderr, "ERROR: Failed to execute statement: %s\n", sqlite3_errmsg(tm->db));
        terminder_deinit(tm);
        exit(EXIT_FAILURE);
    }
}

void terminder_command_remove(Terminder *tm, Args *args)
{
    const char *id= shift_args(args, "Expecting the id of the task");

    const char *insert_task_sql = "DELETE FROM tasks WHERE id=?;";
    if(sqlite3_prepare_v2(tm->db, insert_task_sql, -1, &tm->stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "ERROR: Failed to prepare statement: %s\n", sqlite3_errmsg(tm->db));
        terminder_deinit(tm);
        exit(EXIT_FAILURE);
    }
    if(sqlite3_bind_text(tm->stmt, 1, id, strlen(id), 0) != SQLITE_OK) {
        fprintf(stderr, "ERROR: Failed to bind parameter: %s\n", sqlite3_errmsg(tm->db));
        terminder_deinit(tm);
        exit(EXIT_FAILURE);
    }

    if(sqlite3_step(tm->stmt) == SQLITE_ERROR) {
        fprintf(stderr, "ERROR: Failed to execute statement: %s\n", sqlite3_errmsg(tm->db));
        terminder_deinit(tm);
        exit(EXIT_FAILURE);
    }
}

void terminder_command_reset(Terminder *tm, Args *args)
{
    assert(0 && "Unimplemented terminder_command_reset()");
}

void terminder_command_help(void)
{
    printf("USAGE: terminder SUBCOMMAND <ARGS> [OPTIONS]\n");
    printf("Available Subcommands\n");
    printf("show                        -> Show list of tasks\n");
    printf("add     <title> <deadline>  -> Add a new task. Date format is YYYY-MM-DD hh:mm:ss\n");
    printf("check   <id>                -> Set a task as completed\n");
    printf("remove  <id>                -> Remove tasks\n");
}


int main(int argc, const char **argv)
{
    Terminder terminder;
    Args args;
    args.count = argc;
    args.items = argv;

    shift_args(&args, "Unreachable (WTF is this platform?)");

    if(terminder_init(&terminder) != 0) return -1;

    if(args.count == 0) {
        terminder_command_show(&terminder, &args);
        return 0;
    }

    const char *subcommand = shift_args(&args, "Unreachable when getting subcommand");
    if(strcmp(subcommand, "show") == 0) terminder_command_show(&terminder, &args);
    else if(strcmp(subcommand, "add") == 0) terminder_command_add(&terminder, &args);
    else if(strcmp(subcommand, "check") == 0) terminder_command_check(&terminder, &args);
    else if(strcmp(subcommand, "remove") == 0) terminder_command_remove(&terminder, &args);
    else if(strcmp(subcommand, "reset") == 0) terminder_command_reset(&terminder, &args);
    else if(strcmp(subcommand, "help") == 0) terminder_command_help();
    else {
        fprintf(stderr, "ERROR: Unknown subcommand %s\n", subcommand);
        terminder_command_help();
    }

    terminder_deinit(&terminder);
}


/// TODOS
/// 1. Input validation
/// 2. Deadline syntax parsing
/// 3. Show command with some options for showing every tasks, pagination, etc
