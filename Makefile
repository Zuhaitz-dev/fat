# ==============================================================================
# Configuration
# ==============================================================================
DEBUG ?= 0
VERSION := "v0.1.0-beta - (Betelgeuse)"
PREFIX ?= /usr/local

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
SRC_DIR := src
OBJ_DIR := obj/$(BUILD_DIR_SUFFIX)
BIN_DIR := bin
PLUGIN_DIR := plugins
LIB_DIR := lib
THEMES_DIR := themes
MAN_DIR := man

TARGET := $(BIN_DIR)/fat-$(BUILD_DIR_SUFFIX)

# ==============================================================================
# Platform-specific Settings
# ==============================================================================
ifeq ($(detected_OS),Linux)
    SHARED_LIB_EXT := .so
    LDFLAGS_NCURSES := -lncursesw
    LDFLAGS_PLATFORM := -ldl
    CLEAN_CMD := rm -rf
    TARGET_EXT :=
endif
ifeq ($(detected_OS),Darwin) # macOS
    SHARED_LIB_EXT := .dylib
    LDFLAGS_NCURSES := -lncurses
    LDFLAGS_PLATFORM := -ldl
    CLEAN_CMD := rm -rf
    TARGET_EXT :=
endif
ifeq ($(detected_OS),Windows)
    SHARED_LIB_EXT := .dll
    MINGW_PREFIX ?= /mingw64
    LDFLAGS_NCURSES := -L$(MINGW_PREFIX)/lib -lpdcurses
    LDFLAGS_PLATFORM :=
    CLEAN_CMD := cmd /c rmdir /s /q
    TARGET_EXT := .exe
endif

# ==============================================================================
# Compiler and Linker Flags
# ==============================================================================
COMMON_CFLAGS := -Wall -Wextra -Iinclude -fPIC -I/opt/homebrew/opt/libmagic/include -I/opt/homebrew/opt/libtar/include -I/opt/homebrew/opt/libzip/include -DFAT_VERSION=\"$(VERSION)\" -DINSTALL_PREFIX=\"$(PREFIX)\"

ifeq ($(DEBUG), 1)
	CFLAGS := $(COMMON_CFLAGS) -g3 -O0
else
	CFLAGS := $(COMMON_CFLAGS) -O2 -s
endif

LDFLAGS := $(LDFLAGS_NCURSES) -lmagic -L/opt/homebrew/opt/libmagic/lib $(LDFLAGS_PLATFORM) -L$(LIB_DIR) -lfat_utils -lm -lzip
ifeq ($(detected_OS),Windows)
    LDFLAGS += -lgnurx
endif

# ==============================================================================
# Source Files
# ==============================================================================
SOURCES := $(filter-out $(SRC_DIR)/string_list.c, $(wildcard $(SRC_DIR)/*.c))
OBJECTS := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SOURCES))

# Define plugin sources and their final .fp packages
PLUGINS_SRC := tar zip
PLUGINS_SO := $(foreach plugin,$(PLUGINS_SRC),$(PLUGIN_DIR)/$(plugin)_plugin$(SHARED_LIB_EXT))
FP_PACKAGES := $(foreach plugin,$(PLUGINS_SRC),$(PLUGIN_DIR)/$(plugin).fp)

SHARED_LIB := $(LIB_DIR)/libfat_utils$(SHARED_LIB_EXT)
SHARED_LIB_OBJ := $(OBJ_DIR)/logger.o $(OBJ_DIR)/string_list.o

# ==============================================================================
# Installation Paths
# ==============================================================================
DESTDIR ?=
INSTALL_BIN_DIR := $(DESTDIR)$(PREFIX)/bin
INSTALL_LIB_DIR := $(DESTDIR)$(PREFIX)/lib
INSTALL_PLUGINS_DIR := $(INSTALL_LIB_DIR)/fat/plugins
INSTALL_THEMES_DIR := $(DESTDIR)$(PREFIX)/share/fat/themes
INSTALL_MAN_DIR := $(DESTDIR)$(PREFIX)/share/man/man1

# ==============================================================================
# Main Build Rules
# ==============================================================================
.PHONY: all release debug clean app plugins build_lib install uninstall package-plugins

all: release

# Release now depends on **package-plugins**
release: build_lib app package-plugins
debug:
	@$(MAKE) all DEBUG=1

build_lib: $(SHARED_LIB)
app: $(TARGET)$(TARGET_EXT)
plugins: $(PLUGINS_SO)

package-plugins: $(FP_PACKAGES)

$(PLUGIN_DIR)/%.fp: $(PLUGIN_DIR)/%_plugin$(SHARED_LIB_EXT) $(PLUGIN_DIR)/%_manifest.json
	@echo "Packaging $@..."
	@rm -f $@
	@(cd $(PLUGIN_DIR) && zip -q $(notdir $@) $(notdir $<) $(notdir $(word 2, $^)))


$(SHARED_LIB): $(SHARED_LIB_OBJ)
	@mkdir -p $(LIB_DIR)
	$(CC) -shared $(SHARED_LIB_OBJ) -o $(SHARED_LIB)

$(OBJ_DIR)/logger.o: $(SRC_DIR)/logger.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/string_list.o: $(SRC_DIR)/string_list.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET)$(TARGET_EXT): $(OBJECTS) $(SHARED_LIB)
	@mkdir -p $(BIN_DIR)
	$(CC) $(OBJECTS) -o $@$(TARGET_EXT) $(LDFLAGS) -Wl,-rpath,'$$ORIGIN/../lib'

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(PLUGIN_DIR)/tar_plugin$(SHARED_LIB_EXT): $(PLUGIN_DIR)/tar_plugin.c | $(SHARED_LIB)
	$(CC) -shared $(CFLAGS) $< -o $@ -ltar -L/opt/homebrew/opt/libtar/lib -L$(LIB_DIR) -lfat_utils -Wl,-rpath,'$$ORIGIN/../../lib'

$(PLUGIN_DIR)/zip_plugin$(SHARED_LIB_EXT): $(PLUGIN_DIR)/zip_plugin.c | $(SHARED_LIB)
	$(CC) -shared $(CFLAGS) $< -o $@ -lzip -I/opt/homebrew/opt/libzip/include -L/opt/homebrew/opt/libzip/lib -L$(LIB_DIR) -lfat_utils -Wl,-rpath,'$$ORIGIN/../../lib'


clean:
	@$(CLEAN_CMD) obj bin lib
	@$(CLEAN_CMD) $(PLUGIN_DIR)/*.so $(PLUGIN_DIR)/*.dll $(PLUGIN_DIR)/*.dylib
	@$(CLEAN_CMD) $(PLUGIN_DIR)/*.fp # Also clean the .fp files
	@echo "Project cleaned."
.PHONY: distclean

distclean: clean
	@echo "Cleaning distribution files..."
	@$(CLEAN_CMD) AppDir
	@$(CLEAN_CMD) fat-*.AppImage

# ==============================================================================
# Installation Rules
# ==============================================================================
install: release
	@echo "Installing FAT to $(PREFIX)..."
	@mkdir -p $(INSTALL_BIN_DIR)
	@mkdir -p $(INSTALL_PLUGINS_DIR)
	@mkdir -p $(INSTALL_THEMES_DIR)
	@mkdir -p $(INSTALL_MAN_DIR)
	install -m 755 $(TARGET)$(TARGET_EXT) $(INSTALL_BIN_DIR)/fat$(TARGET_EXT)
	install -m 644 $(SHARED_LIB) $(INSTALL_LIB_DIR)
	install -m 644 $(PLUGIN_DIR)/*.fp $(INSTALL_PLUGINS_DIR)
	install -m 644 $(THEMES_DIR)/*.json $(INSTALL_THEMES_DIR)
	install -m 644 $(MAN_DIR)/fat.1 $(INSTALL_MAN_DIR)/fat.1
	@echo "Installation complete."
	@echo "Run 'sudo ldconfig' to update the library cache (Linux only)."

uninstall:
	@echo "Uninstalling FAT from $(PREFIX)..."
	rm -f $(INSTALL_BIN_DIR)/fat$(TARGET_EXT)
	rm -f $(INSTALL_LIB_DIR)/libfat_utils.so
	rm -rf $(INSTALL_LIB_DIR)/fat
	rm -rf $(INSTALL_THEMES_DIR)
	rm -f $(INSTALL_MAN_DIR)/fat.1
	@echo "Uninstallation complete."
	@echo "Run 'sudo ldconfig' to update the library cache (Linux only)."

# ==============================================================================
# AppImage Packaging
# ==============================================================================
.PHONY: appimage

appimage: release
	@echo "Creating AppImage..."
	# 1. Clean up any previous AppDir
	@$(CLEAN_CMD) AppDir

	# 2. Use the existing 'install' rule to create the AppDir structure
	@$(MAKE) install DESTDIR=AppDir

	# 3. Copy the desktop file and icon into the AppDir
	@mkdir -p AppDir/usr/share/applications/
	@cp fat.desktop AppDir/usr/share/applications/
	@mkdir -p AppDir/usr/share/icons/hicolor/256x256/apps/
	@cp fat.png AppDir/usr/share/icons/hicolor/256x256/apps/

	# 4. Download linuxdeploy if it doesn't exist
	@if [ ! -f linuxdeploy-x86_64.AppImage ]; then \
		echo "Downloading linuxdeploy..."; \
		wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage; \
		chmod +x linuxdeploy-x86_64.AppImage; \
	fi

	# 5. Run linuxdeploy to bundle dependencies and create the final AppImage
	# The --executable flag ensures the main binary is found correctly.
	@./linuxdeploy-x86_64.AppImage --appdir AppDir --executable AppDir/usr/local/bin/fat --output appimage

	@echo "AppImage created successfully!"