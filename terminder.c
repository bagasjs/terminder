#include <assert.h>
#define SHTABLE_IMPLEMENTATION
#include "shtable.h"

#define ARENA_IMPLEMENTATION
#include "arena.h"

#define FSUTIL_IMPLEMENTATION
#include "fsutil.h"

#define DATETIME_IMPLEMENTATION
#include "datetime.h"

///
/// Utilties
///

#define MULTILINE_STR(...) #__VA_ARGS__
#ifndef NDEBUG
#define ERRORF(...) do { \
    fprintf(stderr, "%s:%d: error: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); \
    } while(0)
#else
#define ERRORF(...) do { \
    fprintf(stderr, "error: "); \
    fprintf(stderr, __VA_ARGS__); \
    } while(0)
#endif

typedef struct Args {
    int count;
    const char **items;
} Args;

const char *shift_args(Args *args, const char *on_empty_error_message)
{
    if(args->count == 0) {
        ERRORF("%s\n", on_empty_error_message);
        exit(EXIT_FAILURE);
    }
    const char *result = args->items[0];
    args->items += 1;
    args->count -= 1;
    return result;
}

char *read_file_text(const char *filepath, size_t *size_ptr)
{
    FILE *f = fopen(filepath, "r");
    if (!f) return NULL;

    // Seek to the end to get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 0) {
        ERRORF("Failed to get file size\n");
        fclose(f);
        return NULL;
    }

    // Allocate memory for content (+1 for null terminator)
    char *data = malloc(size + 1);
    if (!data) {
        ERRORF("Failed to allocate memory for read file\n");
        fclose(f);
        return NULL;
    }

    // Read file into buffer
    size_t read = fread(data, 1, size, f);
    fclose(f);

    data[read] = '\0';  // Null-terminate
    if(size_ptr) *size_ptr = read;
    return data;
}

bool nob_write_entire_file(const char *path, const void *data, size_t size)
{
    const char *buf = NULL;
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        ERRORF("Could not open file %s for writing\n", path);
        if (f) fclose(f);
        return false;
    }
    buf = (const char*)data;
    while (size > 0) {
        size_t n = fwrite(buf, 1, size, f);
        if (ferror(f)) {
            ERRORF("Could not write into file %s\n", path);
            if (f) fclose(f);
            return false;
        }
        size -= n;
        buf  += n;
    }
    if (f) fclose(f);
    return true;
}

///
/// Core Application Logic
///

typedef struct Task Task;
struct Task {
    Task *next;
    const char *id;
    const char *title;
    const char *status;
    int priority;
    const char *deadline;
    const char *body;
};

typedef struct Terminder {
    const char *tasks_dir;
    shtable_t lookup;
    Arena arena;
    Task *begin;
    Task *end;
} Terminder;


#include <stdio.h>
typedef struct Parser {
    const char *file_path;
    const char *source;
    size_t length;
    size_t it;
    size_t row;
    size_t row_start;
    size_t row_end;
    size_t col;
    const char *string;
    int64_t int_number;
    struct {
        char *items;
        size_t count;
        size_t capacity;
    } string_storage;
} Parser;

static inline char *escape_char(char ch)
{
    static char buf[4] = {0};
    buf[0] = '`';
    buf[1] = ch;
    buf[2] = '`';
    switch(ch) {
        case '\n': return "newline";
        case ' ':  return "space";
        case '\t': return "tab";
        case '\0': return "null terminator";
        default:   return  buf;
    }
}

void parser_storage_append(Parser *p, char ch)
{
    if(p->string_storage.count + 1 > p->string_storage.capacity) {
        if(p->string_storage.capacity == 0) {
            p->string_storage.capacity  = 32;
        } else {
            p->string_storage.capacity *= 2;
        }

        void *new_items = malloc(p->string_storage.capacity * sizeof(*p->string_storage.items));
        assert(new_items != NULL && "Buy more RAM LOL!");
        memcpy(new_items, p->string_storage.items, p->string_storage.count * sizeof(*p->string_storage.items));
        free(p->string_storage.items);
        p->string_storage.items = new_items;
    }
    p->string_storage.items[p->string_storage.count++] = ch;
}

static void parser_init(Parser *parser, const char *file_path, const char *source, size_t source_length)
{
    parser->file_path = file_path;
    parser->source = source;
    parser->length = source_length;
    parser->it = 0;
    parser->string_storage.items    = 0;
    parser->string_storage.capacity = 0;
    parser->string_storage.count    = 0;
    parser->row = 0;
    parser->col = 0;
    parser->row_start = parser->it;
    while(parser->it < parser->length && parser->source[parser->it] != '\n') {
        parser->it += 1;
    }
    parser->row_end = parser->it;
    parser->it = parser->row_start;
}

static void parser_dump_current_line(Parser *p)
{
    fprintf(stderr, "%*zu | %.*s\n",  6, p->row, (int)(p->row_end - p->row_start), &p->source[p->row_start]); 
    fprintf(stderr, "%*s | %*s%c\n", 6, "", (int)p->col, "",  '^'); 
}

static void parser_advance(Parser *parser)
{
    if(parser->it < parser->length) {
        parser->it += 1;
        if(parser->source[parser->it - 1] == '\n') {
            parser->row += 1;
            parser->col  = 0;
            parser->row_start = parser->it;
            while(parser->it < parser->length && parser->source[parser->it] != '\n') {
                parser->it += 1;
            }
            parser->row_end = parser->it;
            parser->it = parser->row_start;
        } else {
            parser->col += 1;
        }
    }
}

static void parser_skip_whitespace(Parser *parser)
{
    bool skip = true;
    while(skip && parser->it < parser->length) {
        char ch = parser->source[parser->it];
        switch(ch) {
        case ' ':
        case '\t':
            parser_advance(parser);
            break;
        default:
            skip = false;
            break;
        }
    }
}

static bool parser_next_symbol(Parser *parser)
{
    char ch = parser->source[parser->it];
    parser->string = NULL;
    parser->string_storage.count = 0;
    if(isalpha(ch) || ch == '_') {
        while(isalnum(ch) || ch == '_') {
            parser_storage_append(parser, ch);
            parser_advance(parser);
            ch = parser->source[parser->it];
        }
        parser_storage_append(parser, 0);
        parser->string = parser->string_storage.items;
        return true;
    } else {
        ERRORF("at %s:%zu:%zu: Expecting a symbol here\n", parser->file_path, parser->row, parser->col);
        parser_dump_current_line(parser);
        return false;
    }
}

static bool parser_next_dqstring(Parser *parser)
{
    char curr = parser->source[parser->it];
    parser->string = NULL;
    parser->string_storage.count = 0;
    if(curr == '"') {
        parser_advance(parser);
        char prev = curr;
        curr = parser->source[parser->it];
        while(parser->it < parser->length) {
            if(curr == '\n') {
                ERRORF("at %s:%zu:%zu: Unexpected newline\n", parser->file_path, parser->row, parser->col);
                parser_dump_current_line(parser);
                return false;
            }
            if(curr == '"' && prev != '\\') {
                parser_advance(parser);
                break;
            }
            parser_storage_append(parser, curr);
            parser_advance(parser);
            curr = parser->source[parser->it];
        }
        parser_storage_append(parser, 0);
        parser->string = parser->string_storage.items;
        return true;
    } else {
        ERRORF("at %s:%zu:%zu: Expecting a %s here\n", parser->file_path, parser->row, parser->col, escape_char('"'));
        parser_dump_current_line(parser);
        return false;
    }
}

static bool parser_next_int(Parser *parser)
{
    char curr = parser->source[parser->it];
    if(isdigit(curr) != 0) {
        parser->int_number = 0;
        while(parser->it < parser->length) {
            if(!isdigit(curr)) break;
            parser->int_number *= 10;
            parser->int_number += (int64_t)curr - (int64_t)'0';
            parser_advance(parser);
            curr = parser->source[parser->it];
        }
        return true;
    } else {
        ERRORF("at %s:%zu:%zu: Expecting a numeric character\n", parser->file_path, parser->row, parser->col);
        parser_dump_current_line(parser);
        return false;
    }
}

static bool parser_expect_next_char(Parser *parser, char ch)
{
    if(parser->it >= parser->length) {
        ERRORF("at %s:%zu:%zu: Unexpected end of file expecting %s\n", parser->file_path, parser->row, parser->col,
                escape_char(ch));
        parser_dump_current_line(parser);
        return false;
    }
    if(parser->source[parser->it] != ch) {
        ERRORF("at %s:%zu:%zu: Expecting character here to be %s\n", parser->file_path, parser->row, parser->col, 
                escape_char(ch));
        parser_dump_current_line(parser);
        return false;
    }
    return true;
}

static bool parser_get_and_expect_next_char(Parser *parser, char ch)
{
    if(!parser_expect_next_char(parser, ch)) return false;
    parser_advance(parser);
    return true;
}

// TODO: move parser_init outside this function so that we can just reuse parser in this function
static bool parse_task_file(Terminder *tmd, const char *file_path)
{
    Task *task = arena_alloc(&tmd->arena, sizeof(*task));;

    size_t length = 0;
    char *source = read_file_text(file_path, &length);

    Parser _parser, *parser;
    parser = &_parser;
    parser_init(parser, file_path, source, length);
    _parser.file_path = file_path;
    assert(_parser.source);
    for(int i = 0; i < 3; ++i) if(!parser_get_and_expect_next_char(parser, '-')) return false;
    if(!parser_get_and_expect_next_char(parser, '\n')) return false;
    while(parser->source[parser->it] != '-') {
        parser_skip_whitespace(parser);
        if(!parser_next_symbol(parser)) return false;
        parser_skip_whitespace(parser);
        if(!parser_get_and_expect_next_char(parser, ':')) return false;
        parser_skip_whitespace(parser);
        if(strcmp(parser->string, "id") == 0) {
            if(!parser_next_dqstring(parser)) return false;
            if(!parser_get_and_expect_next_char(parser, '\n')) return false;
            task->id = arena_strdup(&tmd->arena, parser->string);
        } else if(strcmp(parser->string, "title") == 0) {
            if(!parser_next_dqstring(parser)) return false;
            if(!parser_get_and_expect_next_char(parser, '\n')) return false;
            task->title = arena_strdup(&tmd->arena, parser->string);
        } else if(strcmp(parser->string, "status") == 0) {
            if(!parser_next_dqstring(parser)) return false;
            if(!parser_get_and_expect_next_char(parser, '\n')) return false;
            task->status = arena_strdup(&tmd->arena, parser->string);
        } else if(strcmp(parser->string, "deadline") == 0) {
            if(!parser_next_dqstring(parser)) return false;
            if(!parser_get_and_expect_next_char(parser, '\n')) return false;
            task->deadline = arena_strdup(&tmd->arena, parser->string);
        } else if(strcmp(parser->string, "priority") == 0) {
            if(!parser_next_int(parser)) return false;
            if(!parser_get_and_expect_next_char(parser, '\n')) return false;
            task->priority = parser->int_number;
        } else {
            ERRORF("at %s:%zu:%zu: Unknown task field `%s`\n", parser->file_path, parser->row, parser->col, 
                    parser->string);
            parser_dump_current_line(parser);
            return false;
        }
    }
    for(int i = 0; i < 3; ++i) if(!parser_get_and_expect_next_char(parser, '-')) return false;
    if(!parser_get_and_expect_next_char(parser, '\n')) return false;
    task->body = arena_strdup(&tmd->arena, &parser->source[parser->it]);
    task->next = NULL;
    free(source);
    free(parser->string_storage.items);

    shtable_set(&tmd->lookup, task->id, task);
    if(tmd->begin == NULL) {
        tmd->begin = task;
        tmd->end   = task;
    } else {
        tmd->end->next = task;
        tmd->end = task;
    }

    return true;
}

// Initialize terminder tasks directory if necessary
bool tmd_must_init(Terminder *tmd, const char *tasks_dir)
{
    memset(tmd, 0, sizeof(*tmd));
    tmd->tasks_dir = tasks_dir;

    FsFileType file_type = fs_get_file_type(tasks_dir);
    switch(file_type) {
    case FS_FILE_NOT_EXIST:
        {
            FsResult res = fs_mkdir(tasks_dir);
            if(res != FS_OK) {
                ERRORF("Failed to create directory: %s\n", fs_result_to_cstr(res));
                return false;
            }
        } break;
    case FS_FILE_DIRECTORY:
        {
            FsResult res = 0;
            FsDirEntry entry = {0};
            res = fs_dir_entry_open(tasks_dir, &entry);
            if(res != FS_OK) {
                ERRORF("Failed to open directory entries: %s\n", fs_result_to_cstr(res));
                return false;
            }
            FsTemp temp = {0};
            temp.capacity = 4 * 1024;
            void *buf = malloc(temp.capacity);
            temp.buffer  = buf;
            int count = 0;
            while(fs_dir_entry_next(&entry)) {
                if(count < 2) { // Skip . and ..
                    count += 1;
                    continue; // The first entry is this directory
                }
                if(entry.result != FS_OK) {
                    ERRORF("Failed to load next entry in directory: %s\n", fs_result_to_cstr(entry.result));
                    break;
                }
                const char *task_dir = NULL, *task_file = NULL;
                task_dir = fs_path_join(&temp, tasks_dir, entry.name);
                if(fs_get_file_type(task_dir) != FS_FILE_DIRECTORY) continue;
                task_file = fs_path_join(&temp, task_dir, "TASK.md");
                if(fs_get_file_type(task_file) != FS_FILE_REGULER) continue; 
                parse_task_file(tmd, task_file);

                fs_temp_reset(&temp);
                count++;
            }
            free(buf);
            fs_dir_entry_close(&entry);
        } break;
    default:
        ERRORF("`%s` already exists and not a directory\n", tasks_dir);
        return false;
    }
    return true;
}

///
/// Command Line Logic
///

typedef struct Subcommand {
    const char *name;
    bool (*handler)(Terminder *tmd, Args *args);
    const char *help;
} Subcommand;

bool tmd_handle_new_task(Terminder *tmd, Args *args)
{
    (void)args;
    FsTemp temp = {0};
    temp.capacity = 4 * 1024;
    void *buf = malloc(temp.capacity);
    temp.buffer  = buf;

    Task *task = arena_alloc(&tmd->arena, sizeof(*task));
    Datetime dt = datetime_now();
    Datetime deadline = datetime_add(dt, (Datetime){ .hour = 1 });
    const char *id = arena_sprintf(&tmd->arena, "%04d%02d%02d-%02d%02d%02d", 
            dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
    const char *deadline_str = arena_sprintf(&tmd->arena, "%04d-%02d-%02d %02d:%02d:%02d", 
            deadline.year, deadline.month, deadline.day, deadline.hour + 1, deadline.minute, deadline.second);
    const char *task_dir = NULL, *task_file = NULL;
    task_dir = fs_path_join(&temp, tmd->tasks_dir, id);
    if(fs_get_file_type(task_dir) != FS_FILE_NOT_EXIST) {
        ERRORF("Directory %s already exists. Could not create a new task\n", task_dir);
        return false;
    }
    FsResult res = fs_mkdir(task_dir);
    if(res != FS_OK) {
        ERRORF("Failed to create directory for a task: %s\n", fs_result_to_cstr(res));
        return false;
    }
    task_file = fs_path_join(&temp, task_dir, "TASK.md");
    if(fs_get_file_type(task_file) != FS_FILE_NOT_EXIST) {
        ERRORF("File %s already exists. Could not create a new task\n", task_file);
        return false;
    }

    FILE *f = fopen(task_file, "w");
    if(!f) {
        ERRORF("Could not open file %s for writing\n", task_file);
        return false;
    }
    fprintf(f, "---\n");
    fprintf(f, "id: \"%s\"\n", id);
    fprintf(f, "title: \"\"\n");
    fprintf(f, "status: \"\"\n");
    fprintf(f, "priority: 0\n");
    fprintf(f, "deadline: \"%s\"\n", deadline_str);
    fprintf(f, "---\n");
    fprintf(f, "\nAdd the description of your task here. You can use **markdown** syntax\n");
    fclose(f);

    fprintf(stdout, "info: Created new task at %s\n", task_file);

    return true;
}

bool tmd_handle_ls(Terminder *tmd, Args *args)
{
    (void)args;
    int i = 1;
    printf("    No | ID              | Deadline            | Title\n");
    printf("------------------------------------------------------\n");
    for(Task *t = tmd->begin; t != NULL; t = t->next) {
        printf("%5d. | %s | %s | %s\n", i, t->id, t->deadline, t->title);
        i += 1;
    }
    return true;
}


bool tmd_handle_help(Terminder *tmd, Args *args);

static Subcommand subcommands[] = {
    { .name = "new-task",  .handler =  tmd_handle_new_task,  .help = "Create a new task" },
    { .name = "ls",        .handler =  tmd_handle_ls,        .help = "Show the list of all tasks" },
    { .name = "help",      .handler =  tmd_handle_help,      .help = "Get this message" },
};
static size_t subcommands_count = sizeof(subcommands)/sizeof(subcommands[0]);

bool tmd_handle_help(Terminder *tmd, Args *args)
{
    (void)tmd;
    (void)args;
    printf("USAGE: terminder SUBCOMMAND <ARGS> [OPTIONS]\n");
    printf("Available Subcommands: \n");
    for(size_t i = 0; i < subcommands_count; ++i) {
        printf("%*s %s\n", -10, subcommands[i].name, subcommands[i].help);
    }
    return true;
}

uint8_t buf[1024] = {0};
FsTemp tmp = { .buffer = buf, .capacity = sizeof(buf) };

int main(int argc, char *argv[])
{
    Terminder tmd;
    const char *root_dir  = "terminder_tasks";

    if(!tmd_must_init(&tmd, root_dir)) return false;

    Args args = { .count = argc, .items = (const char **)argv };
    shift_args(&args, "Unreachable");
    const char *subcommand = shift_args(&args, "Provide a subcommand. Look at `terminder help`");
    bool handled = false;
    int exit_code = 0;
    for(size_t i = 0; i < subcommands_count; ++i) {
        if(strcmp(subcommand, subcommands[i].name) == 0) {
            handled = true;
            if(!subcommands[i].handler(&tmd, &args)) {
                exit_code = -1;
            }
        }
    }

    if(!handled) {
        fprintf(stderr, "ERROR: Unknown subcommand %s\n", subcommand);
        tmd_handle_help(&tmd, &args);
    }

    return exit_code;
}
