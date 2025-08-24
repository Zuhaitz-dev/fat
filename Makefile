# ==============================================================================
# Configuration
# ==============================================================================
DEBUG ?= 0
VERSION := "$(shell cat .version/VERSION_FILE) - ($(shell cat .version/CODENAME_FILE))"
PREFIX ?= /usr/local
USE_HOMEBREW ?= 1

# ==============================================================================
# OS Detection
# ==============================================================================
ifeq ($(OS),Windows_NT)
    detected_OS := Windows
else
    detected_OS := $(shell uname)
endif

# ==============================================================================
# Variables
# ==============================================================================
CC ?= gcc
# Add this line to allow for extra flags from the build environment
EXTRA_CFLAGS ?=

ifeq ($(DEBUG), 1)
	BUILD_TYPE_STR := Debug
	BUILD_DIR_SUFFIX := debug
else
	BUILD_TYPE_STR := Release
	BUILD_DIR_SUFFIX := release
endif

# ==============================================================================
# Directories
# ==============================================================================
SRC_BASE_DIR := src
OBJ_DIR := obj/$(BUILD_DIR_SUFFIX)
BIN_DIR := bin
PLUGIN_DIR := plugins
LIB_DIR := lib
THEMES_DIR := themes
DEFAULTS_DIR := defaults
MAN_DIR := man

TARGET := $(BIN_DIR)/fat-$(BUILD_DIR_SUFFIX)

# ==============================================================================
# Platform-specific Settings
# ==============================================================================
ifeq ($(detected_OS),Darwin) # macOS
    SHARED_LIB_EXT := .dylib
    LDFLAGS_NCURSES := -lncurses
    LDFLAGS_PLATFORM := -ldl
    CLEAN_CMD := rm -rf
    TARGET_EXT :=
    STRIP_FLAG :=
else ifeq ($(detected_OS),Windows)
    SHARED_LIB_EXT := .dll
    MINGW_PREFIX ?= /mingw64
    LDFLAGS_NCURSES := -L$(MINGW_PREFIX)/lib -lpdcurses
    LDFLAGS_PLATFORM :=
    CLEAN_CMD := cmd /c rmdir /s /q
    TARGET_EXT := .exe
    STRIP_FLAG := -s
else
    SHARED_LIB_EXT := .so
    LDFLAGS_NCURSES := -lncursesw
    LDFLAGS_PLATFORM := -ldl
    CLEAN_CMD := rm -rf
    TARGET_EXT :=
    STRIP_FLAG := -s
endif

# ==============================================================================
# Compiler and Linker Flags
# ==============================================================================
ifeq ($(USE_HOMEBREW), 1)
    HOMEBREW_MAGIC_INC := -I/opt/homebrew/opt/libmagic/include
    HOMEBREW_TAR_INC := -I/opt/homebrew/opt/libtar/include
    HOMEBREW_ZIP_INC := -I/opt/homebrew/opt/libzip/include
    HOMEBREW_MAGIC_LFLAG := -L/opt/homebrew/opt/libmagic/lib
    HOMEBREW_TAR_LFLAG := -L/opt/homebrew/opt/libtar/lib
    HOMEBREW_ZIP_LFLAG := -L/opt/homebrew/opt/libzip/lib
else
    HOMEBREW_MAGIC_INC :=
    HOMEBREW_TAR_INC :=
    HOMEBREW_ZIP_INC :=
    HOMEBREW_MAGIC_LFLAG :=
    HOMEBREW_TAR_LFLAG :=
    HOMEBREW_ZIP_LFLAG :=
endif

COMMON_CFLAGS := -Wall -Wextra -Iinclude -fPIC $(HOMEBREW_MAGIC_INC) $(HOMEBREW_TAR_INC) $(HOMEBREW_ZIP_INC) -DFAT_VERSION=\"$(VERSION)\" -DINSTALL_PREFIX=\"$(PREFIX)\"

ifeq ($(DEBUG), 1)
	CFLAGS := $(COMMON_CFLAGS) -g3 -O0 $(EXTRA_CFLAGS)
else
	# Append the EXTRA_CFLAGS variable here
	CFLAGS := $(COMMON_CFLAGS) -O2 $(STRIP_FLAG) $(EXTRA_CFLAGS)
endif

LDFLAGS := $(LDFLAGS_NCURSES) -lmagic $(HOMEBREW_MAGIC_LFLAG) $(LDFLAGS_PLATFORM) -L$(LIB_DIR) -lfat_utils -lm -lzip $(HOMEBREW_ZIP_LFLAG)
ifeq ($(detected_OS),Windows)
    LDFLAGS += -lgnurx
endif

# ==============================================================================
# Source Files
# ==============================================================================

# Find all .c files in the project
SOURCES := $(wildcard $(SRC_BASE_DIR)/core/*.c $(SRC_BASE_DIR)/main/*.c $(SRC_BASE_DIR)/plugins/*.c $(SRC_BASE_DIR)/ui/*.c $(SRC_BASE_DIR)/utils/*.c)

# Define the source files that will be compiled into the shared library
SHARED_LIB_SRC := $(SRC_BASE_DIR)/utils/logger.c $(SRC_BASE_DIR)/core/string_list.c
# Automatically generate the corresponding object file names
SHARED_LIB_OBJ := $(patsubst $(SRC_BASE_DIR)/%.c,$(OBJ_DIR)/%.o,$(SHARED_LIB_SRC))

# The application sources are all sources EXCEPT those used in the shared library
APP_SOURCES := $(filter-out $(SHARED_LIB_SRC), $(SOURCES))
# Automatically generate the object files for the main application
OBJECTS := $(patsubst $(SRC_BASE_DIR)/%.c,$(OBJ_DIR)/%.o,$(APP_SOURCES))

PLUGINS_SRC := tar zip gz
PLUGINS_SO := $(foreach plugin,$(PLUGINS_SRC),$(PLUGIN_DIR)/$(plugin)_plugin$(SHARED_LIB_EXT))

SHARED_LIB := $(LIB_DIR)/libfat_utils$(SHARED_LIB_EXT)

# ==============================================================================
# Installation Paths
# ==============================================================================
DESTDIR ?=
INSTALL_BIN_DIR := $(DESTDIR)$(PREFIX)/bin
INSTALL_LIB_DIR := $(DESTDIR)$(PREFIX)/lib
INSTALL_SHARE_DIR := $(DESTDIR)$(PREFIX)/share/fat
INSTALL_PLUGINS_DIR := $(INSTALL_LIB_DIR)/fat/plugins
INSTALL_THEMES_DIR := $(INSTALL_SHARE_DIR)/themes
INSTALL_DEFAULTS_DIR := $(INSTALL_SHARE_DIR)/defaults
INSTALL_MAN_DIR := $(DESTDIR)$(PREFIX)/share/man/man1

# ==============================================================================
# Main Build Rules
# ==============================================================================
.PHONY: all release debug clean app plugins build_lib install uninstall

all: release

release: build_lib app plugins

debug:
	$(MAKE) all DEBUG=1

build_lib: $(SHARED_LIB)
app: $(TARGET)$(TARGET_EXT)
plugins: $(PLUGINS_SO)

$(SHARED_LIB): $(SHARED_LIB_OBJ)
	@mkdir -p $(LIB_DIR)
	$(CC) -shared $(SHARED_LIB_OBJ) -o $(SHARED_LIB)

$(TARGET)$(TARGET_EXT): $(OBJECTS) $(SHARED_LIB)
	@mkdir -p $(BIN_DIR)
	$(CC) $(OBJECTS) -o $@$(TARGET_EXT) $(LDFLAGS) -Wl,-rpath,'$$ORIGIN/../lib'

# Generic rule to compile any .c file from its new location into the obj directory
$(OBJ_DIR)/%.o: $(SRC_BASE_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(PLUGIN_DIR)/tar_plugin$(SHARED_LIB_EXT): $(PLUGIN_DIR)/tar_plugin.c | $(SHARED_LIB)
	$(CC) -shared $(CFLAGS) $< -o $@ -ltar $(HOMEBREW_TAR_LFLAG) -L$(LIB_DIR) -lfat_utils -Wl,-rpath,'$$ORIGIN/../../lib'

$(PLUGIN_DIR)/zip_plugin$(SHARED_LIB_EXT): $(PLUGIN_DIR)/zip_plugin.c | $(SHARED_LIB)
	$(CC) -shared $(CFLAGS) $< -o $@ -lzip $(HOMEBREW_ZIP_INC) $(HOMEBREW_ZIP_LFLAG) -L$(LIB_DIR) -lfat_utils -Wl,-rpath,'$$ORIGIN/../../lib'

$(PLUGIN_DIR)/gz_plugin$(SHARED_LIB_EXT): $(PLUGIN_DIR)/gz_plugin.c | $(SHARED_LIB)
	$(CC) -shared $(CFLAGS) $< -o $@ -lz -L$(LIB_DIR) -lfat_utils -Wl,-rpath,'$$ORIGIN/../../lib'

clean:
	@$(CLEAN_CMD) obj bin lib
	@$(CLEAN_CMD) $(PLUGIN_DIR)/*.so $(PLUGIN_DIR)/*.dll $(PLUGIN_DIR)/*.dylib
	@$(CLEAN_CMD) $(PLUGIN_DIR)/*.fp
	@echo "Project cleaned."

.PHONY: distclean
distclean: clean
	@echo "Cleaning distribution files..."
	@$(CLEAN_CMD) AppDir
	@$(CLEAN_CMD) fat-*.AppImage

# ==============================================================================
# Installation Rules
# ==============================================================================
install: all
	@echo "Installing FAT to $(PREFIX)..."
	@mkdir -p $(INSTALL_BIN_DIR)
	@mkdir -p $(INSTALL_PLUGINS_DIR)
	@mkdir -p $(INSTALL_THEMES_DIR)
	@mkdir -p $(INSTALL_DEFAULTS_DIR)
	@mkdir -p $(INSTALL_MAN_DIR)
	install -m 755 $(TARGET)$(TARGET_EXT) $(INSTALL_BIN_DIR)/fat$(TARGET_EXT)
	install -m 644 $(SHARED_LIB) $(INSTALL_LIB_DIR)
	install -m 644 $(PLUGIN_DIR)/*_plugin$(SHARED_LIB_EXT) $(INSTALL_PLUGINS_DIR)
	install -m 644 $(THEMES_DIR)/*.json $(INSTALL_THEMES_DIR)
	install -m 644 $(DEFAULTS_DIR)/*.json $(INSTALL_DEFAULTS_DIR)
	install -m 644 $(MAN_DIR)/fat.1 $(INSTALL_MAN_DIR)/fat.1
	@echo "Installation complete."
	@echo "Run 'sudo ldconfig' to update the library cache (Linux only)."

uninstall:
	@echo "Uninstalling FAT from $(PREFIX)..."
	rm -f $(INSTALL_BIN_DIR)/fat$(TARGET_EXT)
	rm -f $(INSTALL_LIB_DIR)/libfat_utils.so
	rm -rf $(INSTALL_LIB_DIR)/fat
	rm -rf $(INSTALL_SHARE_DIR)
	rm -f $(INSTALL_MAN_DIR)/fat.1
	@echo "Uninstallation complete."
	@echo "Run 'sudo ldconfig' to update the library cache (Linux only)."

# ==============================================================================
# AppImage Packaging
# ==============================================================================
.PHONY: appimage
appimage: release
	@echo "Creating AppImage..."
	@$(CLEAN_CMD) AppDir
	@$(MAKE) install DESTDIR=AppDir
	@mkdir -p AppDir/usr/share/applications/
	@cp fat.desktop AppDir/usr/share/applications/
	@mkdir -p AppDir/usr/share/icons/hicolor/256x256/apps/
	@cp fat.png AppDir/usr/share/icons/hicolor/256x256/apps/

	@if [ ! -f linuxdeploy-x86_64.AppImage ]; then \
		echo "Downloading linuxdeploy..."; \
		wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage; \
		chmod +x linuxdeploy-x86_64.AppImage; \
	fi
	@./linuxdeploy-x86_64.AppImage --appdir AppDir --executable AppDir/usr/local/bin/fat --output appimage

	@echo "AppImage created successfully!"