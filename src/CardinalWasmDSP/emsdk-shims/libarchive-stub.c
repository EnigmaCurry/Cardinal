/*
 * Stub libarchive — compiled into libarchive.a for wasm builds where we
 * can't link the real thing (PawPaw's wasm target doesn't build it, and
 * source-building fails on arc4random_buf / errno_t under Emscripten).
 *
 * Loads return failure codes — Cardinal's Rack UI "Load Patch" menu
 * won't work, use the outer project's file picker instead (it does
 * tar.zst extraction in pure JS).
 *
 * Saves are HOOKED: archive_write_open_filename triggers a browser
 * download of the raw patch.json from MEMFS at the filename Cardinal
 * passed in. The rest of the write_* functions return OK so Cardinal
 * thinks the save succeeded (avoiding error dialogs). The resulting
 * file is a raw-JSON .vcv (legacy V1 format) which Cardinal's own
 * load path accepts.
 *
 * ARCHIVE_OK == 0, ARCHIVE_FATAL == -30 (libarchive constants).
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <emscripten.h>

typedef struct archive archive;
typedef struct archive_entry archive_entry;
typedef long la_int64_t;

/* --- error --------------------------------------------------------- */
const char* archive_error_string(archive* a)                                 { (void)a; return 0; }

/* --- archive_entry ------------------------------------------------- */
archive_entry* archive_entry_new(void)                                       { return 0; }
void archive_entry_free(archive_entry* e)                                    { (void)e; }
int  archive_entry_filetype(archive_entry* e)                                { (void)e; return 0; }
int  archive_entry_mode(archive_entry* e)                                    { (void)e; return 0; }
const char* archive_entry_pathname(archive_entry* e)                         { (void)e; return 0; }
const char* archive_entry_sourcepath(archive_entry* e)                       { (void)e; return 0; }
la_int64_t  archive_entry_size(archive_entry* e)                             { (void)e; return 0; }
void archive_entry_set_gid(archive_entry* e, la_int64_t v)                   { (void)e; (void)v; }
void archive_entry_set_gname(archive_entry* e, const char* n)                { (void)e; (void)n; }
void archive_entry_set_mode(archive_entry* e, int m)                         { (void)e; (void)m; }
void archive_entry_set_pathname(archive_entry* e, const char* p)             { (void)e; (void)p; }
void archive_entry_set_uid(archive_entry* e, la_int64_t v)                   { (void)e; (void)v; }
void archive_entry_set_uname(archive_entry* e, const char* n)                { (void)e; (void)n; }

/* --- archive_read -------------------------------------------------- */
archive* archive_read_new(void)                                              { return 0; }
int archive_read_free(archive* a)                                            { (void)a; return 0; }
int archive_read_close(archive* a)                                           { (void)a; return 0; }
int archive_read_support_format_tar(archive* a)                              { (void)a; return 0; }
int archive_read_support_filter_zstd(archive* a)                             { (void)a; return 0; }
int archive_read_open(archive* a, void* cd, void* o, void* r, void* c)       { (void)a; (void)cd; (void)o; (void)r; (void)c; return -30; }
int archive_read_open_filename(archive* a, const char* n, size_t bs)         { (void)a; (void)n; (void)bs; return -30; }
int archive_read_next_header(archive* a, archive_entry** e)                  { (void)a; (void)e; return -30; }
int archive_read_next_header2(archive* a, archive_entry* e)                  { (void)a; (void)e; return -30; }
int archive_read_data_block(archive* a, const void** b, size_t* s, la_int64_t* o) { (void)a; (void)b; (void)s; (void)o; return -30; }

/* --- archive_read_disk --------------------------------------------- */
archive* archive_read_disk_new(void)                                         { return 0; }
int archive_read_disk_open(archive* a, const char* p)                        { (void)a; (void)p; return -30; }
int archive_read_disk_descend(archive* a)                                    { (void)a; return 0; }

/* --- archive_write ------------------------------------------------- */
/*
 * When Cardinal's Save menu action calls archive_write_open_filename,
 * we call out to JS to (a) locate the newest /tmp/Cardinal.NNNN/patch.json
 * (already written by saveAutosave() before this call), (b) trigger a
 * browser download named after the filename Cardinal passed. Cardinal
 * thinks the save succeeded because all our stubs return OK.
 *
 * emscripten_run_script (not EM_JS) is required because CardinalMiniSep
 * compiles this .c into SIDE_MODULE wasms, and EM_JS is main-module-only.
 * The JS function `window.__cardinal_download_patch` is defined by
 * web/dev-mini-helper.js on page load.
 */
static void _cardinal_trigger_save_download(const char* filename) {
    if (!filename) {
        emscripten_run_script("console.log('[libarchive-stub] open_filename(NULL)');");
        return;
    }
    char buf[2048];
    char safe[1024];
    size_t j = 0;
    for (size_t i = 0; filename[i] && j < sizeof(safe) - 1; i++) {
        if (filename[i] == '\'' || filename[i] == '\\' || filename[i] == '\n') continue;
        safe[j++] = filename[i];
    }
    safe[j] = '\0';
    snprintf(buf, sizeof(buf),
        "console.log('[libarchive-stub] archive_write_open_filename(%s) called; "
        "download hook =', typeof window.__cardinal_download_patch); "
        "if(window.__cardinal_download_patch)window.__cardinal_download_patch('%s');",
        safe, safe);
    emscripten_run_script(buf);
}

archive* archive_write_new(void)                                             { return (archive*)1; }
int archive_write_free(archive* a)                                           { (void)a; return 0; }
int archive_write_close(archive* a)                                          { (void)a; return 0; }
int archive_write_open(archive* a, void* cd, void* o, void* w, void* c)      { (void)a; (void)cd; (void)o; (void)w; (void)c; return 0; }
int archive_write_open_filename(archive* a, const char* n)                   {
    (void)a;
    _cardinal_trigger_save_download(n);
    return 0;
}
int archive_write_header(archive* a, archive_entry* e)                       { (void)a; (void)e; return 0; }
long archive_write_data(archive* a, const void* b, size_t s)                 { (void)a; (void)b; return (long)s; }
long archive_write_data_block(archive* a, const void* b, size_t s, la_int64_t o) { (void)a; (void)b; (void)o; return (long)s; }
int archive_write_finish_entry(archive* a)                                   { (void)a; return 0; }
int archive_write_add_filter_zstd(archive* a)                                { (void)a; return 0; }
int archive_write_set_format_pax_restricted(archive* a)                      { (void)a; return 0; }
int archive_write_set_bytes_per_block(archive* a, int bpb)                   { (void)a; (void)bpb; return 0; }
int archive_write_set_filter_option(archive* a, const char* m, const char* o, const char* v) { (void)a; (void)m; (void)o; (void)v; return 0; }
archive* archive_write_disk_new(void)                                        { return (archive*)1; }
int archive_write_disk_set_options(archive* a, int flags)                    { (void)a; (void)flags; return 0; }
