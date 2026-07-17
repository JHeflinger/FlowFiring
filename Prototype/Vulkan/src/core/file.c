#include "file.h"
#include <easylogger.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <easymemory.h>

const char* FileExtension(const char* path) {
    const char* dot = strrchr(path, '.');
    const char* slash1 = strrchr(path, '/');
    const char* slash2 = strrchr(path, '\\');
    const char* slash = slash1 > slash2 ? slash1 : slash2;
    if (!dot || (slash && dot < slash)) return NULL;
    return dot + 1;
}

char* StripFilename(char* path) {
    for (int i = (int)strlen(path) - 1; i >= 0; i--) {
        if (path[i] == '/' || path[i] == '\\') {
            if (i == (int)strlen(path) - 1) return NULL;
            return path + i + 1;
        }
    }
    return NULL;
}

SimpleFile* ReadFile(const char* filename) {
	SimpleFile* sfile = EZ_ALLOC(1, sizeof(SimpleFile));
    sfile->type = GetFileType(filename);
	FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        EZ_ERROR("Unable to open file \"%s\"", filename);
        EZ_FREE(sfile);
        return NULL;
    }
	fseek(file, 0, SEEK_END);
	sfile->size = ftell(file);
	rewind(file);
	sfile->data = EZ_ALLOC(sfile->size, sizeof(char));
	size_t read = fread(sfile->data, 1, sfile->size, file);
    fclose(file);
    if (read != sfile->size) {
        EZ_ERROR("Unable to read file \"%s\"", filename);
        FreeFile(sfile);
        return NULL;
    }
	return sfile;
}

void FreeFile(SimpleFile* file) {
	EZ_FREE(file->data);
	EZ_FREE(file);
}

LineParser Parser(SimpleFile* file) {
    return (LineParser){ file, 0, 0 };
}

BOOL NextLine(LineParser* lp, char* buffer, size_t size) {
    if (lp->cursor >= lp->file->size) return FALSE;
    memset(buffer, 0, size);
    int last_ind = -1;
    for (size_t i = lp->cursor; i < lp->file->size; i++) {
        if (lp->file->data[i] == '\n') {
            last_ind = (int)i;
            break;
        }
    }
    lp->line++;
    if (last_ind < 0) {
        memcpy(buffer, lp->file->data + lp->cursor, lp->file->size - lp->cursor);
        lp->cursor = lp->file->size;
        return TRUE;
    }
    if (last_ind - lp->cursor > size) {
        EZ_WARN("line overflow detected when parsing for lines...");
        memcpy(buffer, lp->file->data + lp->cursor, size);
        lp->cursor += size;
        return TRUE;
    }
    memcpy(buffer, lp->file->data + lp->cursor, last_ind - lp->cursor);
    lp->cursor = last_ind + 1;
    return TRUE;
}
