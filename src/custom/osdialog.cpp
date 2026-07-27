/*
 * DISTRHO Cardinal Plugin
 * Copyright (C) 2021-2022 Filipe Coelho <falktx@falktx.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * For a full copy of the GNU General Public License see the LICENSE file.
 */

#include <osdialog.h>
#include <cstdlib>
#include <cstring>

#include "DistrhoUtils.hpp"

#if defined(__EMSCRIPTEN__)
# include <emscripten.h>
# include <emscripten/em_asm.h>
#endif

char* osdialog_file(osdialog_file_action action, const char* path, const char* filename, osdialog_filters* filters)
{
    d_stderr2("[Cardinal] osdialog_file called %d %s %s", action, path, filename);
#if defined(__EMSCRIPTEN__)
    // Under wasm we have no native file browser. For SAVE, prompt via
    // JS for a filename — the returned path is arbitrary (we don't
    // actually write to disk), but Cardinal's save flow needs a
    // non-null path so it proceeds through archiveDirectory, which our
    // libarchive-stub intercepts to trigger a browser download of
    // patch.json under the chosen name.
    //
    // For OPEN we return null (use the outer project's file picker).
    if (action != OSDIALOG_SAVE) return nullptr;
    const char* dflt = filename && filename[0] ? filename : "patch.vcv";
    // emscripten_run_script_string returns a static buffer we must strdup.
    char script[512];
    std::snprintf(script, sizeof(script),
        "(function(){var r=window.prompt('Save patch as:', %c%s%c);"
        "return r===null?'':r;})()",
        '"', dflt, '"');
    const char* answer = emscripten_run_script_string(script);
    if (!answer || !answer[0]) return nullptr;
    return strdup(answer);
#else
    return nullptr;
#endif
}

int osdialog_message(osdialog_message_level level, osdialog_message_buttons buttons, const char* message)
{
    d_stderr2("[Cardinal] osdialog_message called %d %d %s", level, buttons, message);
    return 0;
}

char* osdialog_prompt(osdialog_message_level level, const char* message, const char* text)
{
    d_stderr2("[Cardinal] osdialog_prompt called %d %s %s", level, message, text);
    return nullptr;
}
