#include "popup.h"
#include "ui/ui.h"
#include "data/colors.h"
#include "renderer/renderer.h"
#include "core/utils.h"
#include <easymemory.h>
#include <easylogger.h>

#include <stdio.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdint.h>

#define print(...) {printf(__VA_ARGS__);printf("\n");}
#define crash(...) {printf("\033[31m[ERROR]\033[0m "); print(__VA_ARGS__);exit(1);}
#define warn(...) {printf("\033[33m[WARNING]\033[0m "); print(__VA_ARGS__);}
#define PATHLEN 4096

typedef void (*FileHandler)(const char*);

#ifdef __linux__
	#include <unistd.h>
	#include <dirent.h>
	#include <sys/time.h>
    #include <pthread.h>

	#define cwd(buffer) getcwd(buffer, sizeof(buffer))
	#define makedir(dir) (!mkdir(dir, 0755))

	int dexists(const char* dir) {
		struct stat statbuf;
		if (stat(dir, &statbuf) != 0) {
			return 0;
		}
		return S_ISDIR(statbuf.st_mode);
	}

	int fexists(const char* file) {
		struct stat statbuf;
		if (stat(file, &statbuf) != 0) {
			return 0;
		}
		return !S_ISDIR(statbuf.st_mode);
	}

	void walkdir(const char* path, FileHandler func) {
		DIR *dir = opendir(path);
		if (!dir) {
            crash("Unable to open directory \"%s\"", path);
        }
		struct dirent *entry;
		while ((entry = readdir(dir)) != NULL) {
			if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
			char full_path[PATH_MAX];
			snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
	        struct stat statbuf;
			if (stat(full_path, &statbuf) != 0) {
                crash("Stat call failed");
            }
			if (S_ISDIR(statbuf.st_mode)) {
				func(full_path);
				walkdir(full_path, func);
			}
		}
		closedir(dir);
	}

	void walkfiles(const char* path, FileHandler func) {
		DIR *dir = opendir(path);
		if (!dir) {
            crash("Unable to open directory \"%s\"", path);
        }
		struct dirent *entry;
		while ((entry = readdir(dir)) != NULL) {
			if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
			char full_path[PATH_MAX];
			snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
	        struct stat statbuf;
			if (stat(full_path, &statbuf) != 0) {
                crash("Stat call failed");
            }
			if (!S_ISDIR(statbuf.st_mode)) {
				func(full_path);
			} else {
				walkfiles(full_path, func);
			}
		}
		closedir(dir);
	}
#elif __WIN32
	#define WIN32_LEAN_AND_MEAN
	#define NOGDICAPMASKS     // CC_*, LC_*, PC_*, CP_*, TC_*, RC_
	#define NOVIRTUALKEYCODES // VK_*
	#define NOWINMESSAGES     // WM_*, EM_*, LB_*, CB_*
	#define NOWINSTYLES       // WS_*, CS_*, ES_*, LBS_*, SBS_*, CBS_*
	#define NOSYSMETRICS      // SM_*
	#define NOMENUS           // MF_*
	#define NOICONS           // IDI_*
	#define NOKEYSTATES       // MK_*
	#define NOSYSCOMMANDS     // SC_*
	#define NORASTEROPS       // Binary and Tertiary raster ops
	#define NOSHOWWINDOW      // SW_*
	#define OEMRESOURCE       // OEM Resource values
	#define NOATOM            // Atom Manager routines
	#define NOCLIPBOARD       // Clipboard routines
	#define NOCOLOR           // Screen colors
	#define NOCTLMGR          // Control and Dialog routines
	#define NODRAWTEXT        // DrawText() and DT_*
	#define NOGDI             // All GDI defines and routines
	#define NOKERNEL          // All KERNEL defines and routines
	#define NOUSER            // All USER defines and routines
	#define NOMB              // MB_* and MessageBox()
	#define NOMEMMGR          // GMEM_*, LMEM_*, GHND, LHND, associated routines
	#define NOMETAFILE        // typedef METAFILEPICT
	#define NOMSG             // typedef MSG and associated routines
	#define NOOPENFILE        // OpenFile(), OemToAnsi, AnsiToOem, and OF_*
	#define NOSCROLL          // SB_* and scrolling routines
	#define NOSERVICE         // All Service Controller routines, SERVICE_ equates, etc.
	#define NOSOUND           // Sound driver routines
	#define NOTEXTMETRIC      // typedef TEXTMETRIC and associated routines
	#define NOWH              // SetWindowsHook and WH_*
	#define NOWINOFFSETS      // GWL_*, GCL_*, associated routines
	#define NOCOMM            // COMM driver routines
	#define NOKANJI           // Kanji support stuff.
	#define NOHELP            // Help engine interface.
	#define NOPROFILER        // Profiler interface.
	#define NODEFERWINDOWPOS  // DeferWindowPos routines
	#define NOMCX             // Modem Configuration Extensions

	#include <direct.h>
	#include <windows.h>

	#define cwd(buffer) _getcwd(buffer, sizeof(buffer))
	#define makedir(dir) (!_mkdir(dir))

	int dexists(const char* dir) {
		struct _stat statbuf;
		if (_stat(dir, &statbuf) != 0) {
		    return 0;
		}
		return (statbuf.st_mode & _S_IFDIR) != 0;
	}

	int fexists(const char* file) {
		struct _stat statbuf;
		if (_stat(file, &statbuf) != 0) {
		    return 0;
		}
		return (statbuf.st_mode & _S_IFDIR) == 0;
	}

	void walkdir(const char* path, FileHandler func) {
		char search_path[MAX_PATH];
		snprintf(search_path, MAX_PATH, "%s/*", path);
		WIN32_FIND_DATAA find_data;
		HANDLE hFind = FindFirstFileA(search_path, &find_data);
		if (hFind == INVALID_HANDLE_VALUE) {
            crash("Unable to open directory \"%s\"", path);
        }
		do {
			const char *name = find_data.cFileName;
			if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
			char full_path[MAX_PATH];
			snprintf(full_path, MAX_PATH, "%s/%s", path, name);
			if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				func(full_path);
				walkdir(full_path, func);
			}
		} while (FindNextFileA(hFind, &find_data) != 0);
		FindClose(hFind);
	}

	void walkfiles(const char* path, FileHandler func) {
		char search_path[MAX_PATH];
		snprintf(search_path, MAX_PATH, "%s/*", path);
		WIN32_FIND_DATAA find_data;
		HANDLE hFind = FindFirstFileA(search_path, &find_data);
		if (hFind == INVALID_HANDLE_VALUE) {
            crash("Unable to open directory \"%s\"", path);
        }
		do {
			const char *name = find_data.cFileName;
			if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
			char full_path[MAX_PATH];
			snprintf(full_path, MAX_PATH, "%s/%s", path, name);
			if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				func(full_path);
			} else {
				walkfiles(full_path, func);
			}
		} while (FindNextFileA(hFind, &find_data) != 0);
		FindClose(hFind);
	}
#elif __APPLE__
	#include <unistd.h>
	#include <dirent.h>
	#include <sys/time.h>
	#include <limits.h>

	#define cwd(buffer) getcwd(buffer, sizeof(buffer))
	#define makedir(dir) (!mkdir(dir, 0755))

	int dexists(const char* dir) {
		struct stat statbuf;
		if (stat(dir, &statbuf) != 0) {
			return 0;
		}
		return S_ISDIR(statbuf.st_mode);
	}

	int fexists(const char* file) {
		struct stat statbuf;
		if (stat(file, &statbuf) != 0) {
			return 0;
		}
		return !S_ISDIR(statbuf.st_mode);
	}

	void walkdir(const char* path, FileHandler func) {
		DIR *dir = opendir(path);
		if (!dir) {
			crash("Unable to open directory \"%s\"", path);
		}
		struct dirent *entry;
		while ((entry = readdir(dir)) != NULL) {
			if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
			char full_path[PATH_MAX];
			snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
			struct stat statbuf;
			if (stat(full_path, &statbuf) != 0) {
				crash("Stat call failed");
			}
			if (S_ISDIR(statbuf.st_mode)) {
				func(full_path);
				walkdir(full_path, func);
			}
		}
		closedir(dir);
	}

	void walkfiles(const char* path, FileHandler func) {
		DIR *dir = opendir(path);
		if (!dir) {
			crash("Unable to open directory \"%s\"", path);
		}
		struct dirent *entry;
		while ((entry = readdir(dir)) != NULL) {
			if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
			char full_path[PATH_MAX];
			snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
			struct stat statbuf;
			if (stat(full_path, &statbuf) != 0) {
				crash("Stat call failed");
			}
			if (!S_ISDIR(statbuf.st_mode)) {
				func(full_path);
			} else {
				walkfiles(full_path, func);
			}
		}
		closedir(dir);
	}
#else
	#error "Unsupported operating system detected!"
#endif

uint32_t g_toh_w = 0;
uint32_t g_toh_l = 0;
uint32_t g_toh_h = 0;
char g_save_name_buffer[512] = "Untitled";
ARRLIST_DynamicString g_session_names = { 0 };
size_t g_session_index = 0;

size_t dropdown_select_session(void* data, size_t index) {
    if (index != (size_t)-1) g_session_index = index;
    return g_session_index;
}

void collect_sessions(const char* path) {
    if (strstr(path, ".ffsession")) {
        int offset = 0;
        if (path[0] == '.' && path[1] == '/') offset = 2;
        char* str = EZ_ALLOC(strlen(path + offset), sizeof(char));
        strcpy(str, path + offset);
        *(strstr(str, ".ffsession")) = '\0';
        if (str[0] == '\0') strcpy(str, "_default");
        ARRLIST_DynamicString_add(&(g_session_names), str);
    }
}

int add_object_popup_stage_0(size_t x, size_t y, size_t w, size_t h) {
    g_toh_w = 5;
    g_toh_l = 5;
    g_toh_h = 5;
    float width = 350;
    float height = 220;
    float xpos = x + ((w - width) / 2.0f);
    float ypos = y + ((h - height) / 2.0f);
    float button_width = 300;
    UISetPosition(0, 0);
    UISetCursor(0, ypos + 10);
    DrawRectangle(xpos, ypos, width, height, MappedColor(PANEL_BG_COLOR));
    UIMoveCursor(xpos + (width / 2) - (UITextWidth("Configure Scene") / 2), 0);
    UIDrawText("Configure Scene");
    UIMoveCursor(xpos + (width / 2) - (button_width / 2) - 10, 20);
    if (UIButton("New Scene", button_width)) return 0;
    UIMoveCursor(xpos + (width / 2) - (button_width / 2) - 10, 5);
    if (UIButton("Save Scene As", button_width)) {
        walkfiles(".", collect_sessions);
        if (g_session_names.size == 0) {
            SaveSimulation();
            walkfiles(".", collect_sessions);
        }
        g_session_index = 0;
        return 1;
    }
    UIMoveCursor(xpos + (width / 2) - (button_width / 2) - 10, 5);
    if (UIButton("Save Scene", button_width)) {
        walkfiles(".", collect_sessions);
        if (g_session_names.size == 0) {
            SaveSimulation();
            walkfiles(".", collect_sessions);
        }
        g_session_index = 0;
        return 2;
    }
    UIMoveCursor(xpos + (width / 2) - (button_width / 2) - 10, 5);
    if (UIButton("Load Scene", button_width)) {
        walkfiles(".", collect_sessions);
        if (g_session_names.size == 0) {
            SaveSimulation();
            walkfiles(".", collect_sessions);
        }
        g_session_index = 0;
        return 3;
    }
    UISetCursor(xpos + (width / 2) - (button_width / 2), ypos + height - 40);
    if (UIButton("Cancel", button_width)) return 3;
    return -1;
}

int save_toh_as(size_t x, size_t y, size_t w, size_t h) {
    float width = 555;
    float height = 300;
    float xpos = x + ((w - width) / 2.0f);
    float ypos = y + ((h - height) / 2.0f);
    float button_width = 200;
    UISetPosition(0, 0);
    UISetCursor(0, ypos + 10);
    DrawRectangle(xpos, ypos, width, height, MappedColor(PANEL_BG_COLOR));
    UIMoveCursor(xpos + (width / 2) - (UITextWidth("Save Configuration") / 2), 0);
    UIDrawText("Save Configuration");

    UIMoveCursor(xpos + 10, 15);
    UITextInput("Name", g_save_name_buffer, 512, width - UITextWidth("Name"));

    BOOL found = FALSE;
    for (size_t i = 0; i < g_session_names.size; i++) {
        if (strcmp(g_session_names.data[i], g_save_name_buffer) == 0) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        UISetCursor(xpos + (width / 2) - (UITextWidth("[WARNING]") / 2), ypos + height - 150);
        UIDrawWarning("[WARNING]");
        UISetCursor(xpos + (width / 2) - (UITextWidth("This session name already exists") / 2), ypos + height - 130);
        UIDrawWarning("This session name already exists");
        UISetCursor(xpos + (width / 2) - (UITextWidth("Use a different name or use the \"Save\" option instead of \"Save As\" to overwrite") / 2), ypos + height - 110);
        UIDrawWarning("Use a different name or use the \"Save\" option instead of \"Save As\" to overwrite");
        DisableUI();
    }
    UISetCursor(xpos + (width / 2) - (button_width / 2), ypos + height - 70);
    if (UIButton("Save", button_width)) {
        SaveSimulationToFile(g_save_name_buffer);
        return 0;
    }
    EnableUI();
    UISetCursor(xpos + (width / 2) - (button_width / 2), ypos + height - 40);
    if (UIButton("Cancel", button_width)) return 0;
    return -1;
}

int save_toh(size_t x, size_t y, size_t w, size_t h) {
    float width = 285;
    float height = 300;
    float xpos = x + ((w - width) / 2.0f);
    float ypos = y + ((h - height) / 2.0f);
    float button_width = 200;
    UISetPosition(0, 0);
    UISetCursor(0, ypos + 10);
    DrawRectangle(xpos, ypos, width, height, MappedColor(PANEL_BG_COLOR));
    UIMoveCursor(xpos + (width / 2) - (UITextWidth("Save Configuration") / 2), 0);
    UIDrawText("Save Configuration");

    UIMoveCursor(xpos + 10, 15);
    UIDrawText("Select Session:");
    UIMoveCursor(xpos + 10 + UITextWidth("Select Session:") + 10, -20);
    UIDropdownMenu(width - UITextWidth("Select Session:") - 40, g_session_names.size, g_session_names.data, dropdown_select_session, NULL);

    UISetCursor(xpos + (width / 2) - (button_width / 2), ypos + height - 70);
    if (UIButton("Save", button_width)) {
        SaveSimulationToFile(g_session_names.data[g_session_index]);
        return 0;
    }
    UISetCursor(xpos + (width / 2) - (button_width / 2), ypos + height - 40);
    if (UIButton("Cancel", button_width)) return 0;
    return -1;
}

int load_toh(size_t x, size_t y, size_t w, size_t h) {
    float width = 365;
    float height = 300;
    float xpos = x + ((w - width) / 2.0f);
    float ypos = y + ((h - height) / 2.0f);
    float button_width = 200;
    UISetPosition(0, 0);
    UISetCursor(0, ypos + 10);
    DrawRectangle(xpos, ypos, width, height, MappedColor(PANEL_BG_COLOR));
    UIMoveCursor(xpos + (width / 2) - (UITextWidth("Load Configuration") / 2), 0);
    UIDrawText("Load Configuration");

    UIMoveCursor(xpos + 10, 15);
    UIDrawText("Select Session:");
    UIMoveCursor(xpos + 10 + UITextWidth("Select Session:") + 10, -20);
    UIDropdownMenu(width - UITextWidth("Select Session:") - 40, g_session_names.size, g_session_names.data, dropdown_select_session, NULL);

    UISetCursor(xpos + (width / 2) - (UITextWidth("[WARNING]") / 2), ypos + height - 150);
    UIDrawWarning("[WARNING]");
    UISetCursor(xpos + (width / 2) - (UITextWidth("This will override your current session!!") / 2), ypos + height - 130);
    UIDrawWarning("This will override your current session!!");
    UISetCursor(xpos + (width / 2) - (UITextWidth("Make sure you have saved your current progress") / 2), ypos + height - 110);
    UIDrawWarning("Make sure you have saved your current progress!");
    UISetCursor(xpos + (width / 2) - (button_width / 2), ypos + height - 70);
    if (UIButton("Load", button_width)) {
        LoadSimulationFromFile(g_session_names.data[g_session_index]);
        return 0;
    }
    UISetCursor(xpos + (width / 2) - (button_width / 2), ypos + height - 40);
    if (UIButton("Cancel", button_width)) return 0;
    return -1;
}

int add_toh(size_t x, size_t y, size_t w, size_t h) {
    float width = 385;
    float height = 300;
    float xpos = x + ((w - width) / 2.0f);
    float ypos = y + ((h - height) / 2.0f);
    float button_width = 200;
    UISetPosition(0, 0);
    UISetCursor(0, ypos + 10);
    DrawRectangle(xpos, ypos, width, height, MappedColor(PANEL_BG_COLOR));
    UIMoveCursor(xpos + (width / 2) - (UITextWidth("Tetrahedral-Octahedral Honeycomb Bounds") / 2), 0);
    UIDrawText("Tetrahedral-Octahedral Honeycomb Bounds");

    size_t current_ram = CurrentRAMUsage();
    size_t total_ram = SystemRAMTotal();
    size_t total_bytes = sizeof(float) * ceil((double)g_toh_h / 2) * ((g_toh_l + 1) * g_toh_w + g_toh_w * (g_toh_l + 1) + 4 * g_toh_w * g_toh_l);
    size_t total_mb = (size_t)(total_bytes / (1024 * 1024));
    char ram_text[128];
    snprintf(ram_text, sizeof(ram_text), "RAM Usage: %zu MB / %zu MB, Expected: %zu MB", current_ram, total_ram, (size_t)(total_mb * 1.1));
    UIMoveCursor(0, 20); 
    UIMoveCursor(xpos + (width / 2) - (UITextWidth(ram_text) / 2), 0);
    UIDrawText(ram_text);

    UIMoveCursor(0, 15);
    UIMoveCursor(xpos + (width / 2) - (UITextWidth("Dimensions") / 2) - 10, 0);
    UIDrawText("Dimensions");
    UIMoveCursor(xpos, 0);
    UIDrawText("w");
    UIMoveCursor(xpos + 15, -20);
    UIDragUInt(&g_toh_w, 0, 50, 1, 100);
    UIMoveCursor(xpos + 125, -20);
    UIDrawText("h");
    UIMoveCursor(xpos + 140, -20);
    UIDragUInt(&g_toh_h, 0, 50, 1, 100);
    UIMoveCursor(xpos + 250, -20);
    UIDrawText("l");
    UIMoveCursor(xpos + 265, -20);
    UIDragUInt(&g_toh_l, 0, 50, 1, 100);

    UISetCursor(xpos + (width / 2) - (UITextWidth("[WARNING]") / 2), ypos + height - 150);
    UIDrawWarning("[WARNING]");
    UISetCursor(xpos + (width / 2) - (UITextWidth("This will override your current session!!") / 2), ypos + height - 130);
    UIDrawWarning("This will override your current session!!");
    UISetCursor(xpos + (width / 2) - (UITextWidth("Make sure you have saved your current progress") / 2), ypos + height - 110);
    UIDrawWarning("Make sure you have saved your current progress!");
    UISetCursor(xpos + (width / 2) - (button_width / 2), ypos + height - 70);
    if (UIButton("Submit", button_width)) {
        ClearSimulation();
        SubmitTOH(g_toh_w, g_toh_l, g_toh_h);
        return 0;
    }
    UISetCursor(xpos + (width / 2) - (button_width / 2), ypos + height - 40);
    if (UIButton("Cancel", button_width)) return 0;
    return -1;
}

Popup* GenerateEmptyPopup() {
    return EZ_ALLOC(1, sizeof(Popup));
}

void CleanPopup(Popup* popup) {
    if (popup->options != 0)
        for (size_t i = 0; i < popup->options; i++)
            CleanPopup(((Popup**)popup->results)[i]);
    if (popup->options > 0) EZ_FREE(popup->results);
    EZ_FREE(popup);
    for (size_t i = 0; i < g_session_names.size; i++) EZ_FREE(g_session_names.data[i]);
    ARRLIST_DynamicString_clear(&g_session_names);
}

Popup* GenerateAddObjectPopup() {
    Popup* popup = GenerateEmptyPopup();
    popup->options = 4;
    popup->behavior = add_object_popup_stage_0;
    popup->results = EZ_ALLOC(popup->options, sizeof(Popup*));
    PopupFunction stage_1[] = {add_toh, save_toh_as, save_toh, load_toh};
    for (size_t i = 0; i < popup->options; i++) {
        Popup* next = GenerateEmptyPopup();
        next->options = 0;
        next->behavior = stage_1[i];
        ((Popup**)popup->results)[i] = next;
    }
    return popup;
}
