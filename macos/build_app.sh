#!/bin/sh
# Builds a self-contained macOS application for one game.
#
#   build_app.sh "<App Name>" <game data folder> <output folder>
#
# The game data folder must hold dungeon.dat and graphics.dat. The hint files and the extras of
# the binary packages (sounds, fonts, lsb.bmp) are included when it has them. The app carries its
# own copy of SDL2 and keeps saved games and settings in
# ~/Library/Application Support/CSB/<App Name>.
set -eu

REQUIRED_GAME_FILES="dungeon.dat graphics.dat"
OPTIONAL_GAME_FILES="hcsb.dat hcsb.hct mini.dat sounds fonts lsb.bmp"
ENGINE_FILES="csb.cfg en.txt fr.txt locale"
EXECUTABLE_NAME=csb

fail() {
    echo "build_app.sh: $1" >&2
    exit 1
}

[ $# -eq 3 ] || fail "usage: build_app.sh \"<App Name>\" <game data folder> <output folder>"
APP_NAME=$1
GAME_DATA_FOLDER=$2
OUTPUT_FOLDER=$3

REPOSITORY=$(cd "$(dirname "$0")/.." && pwd)
SOURCE_FOLDER="$REPOSITORY/src"
APP="$OUTPUT_FOLDER/$APP_NAME.app"
BUNDLED_GAME_FOLDER="$APP/Contents/Resources/game"
FRAMEWORKS_FOLDER="$APP/Contents/Frameworks"
BUNDLED_EXECUTABLE="$APP/Contents/MacOS/$EXECUTABLE_NAME"

check_game_data() {
    for file in $REQUIRED_GAME_FILES; do
        [ -f "$GAME_DATA_FOLDER/$file" ] || fail "$GAME_DATA_FOLDER has no $file"
    done
}

build_release_executable() {
    make -C "$SOURCE_FOLDER" clean
    make -C "$SOURCE_FOLDER" RELEASE=1 -j"$(sysctl -n hw.ncpu)"
}

create_empty_bundle() {
    rm -rf "$APP"
    mkdir -p "$APP/Contents/MacOS" "$BUNDLED_GAME_FOLDER" "$FRAMEWORKS_FOLDER"
    cp "$SOURCE_FOLDER/$EXECUTABLE_NAME" "$BUNDLED_EXECUTABLE"
}

copy_game_files() {
    for file in $ENGINE_FILES; do
        cp -R "$REPOSITORY/$file" "$BUNDLED_GAME_FOLDER/"
    done
    for file in $REQUIRED_GAME_FILES; do
        cp "$GAME_DATA_FOLDER/$file" "$BUNDLED_GAME_FOLDER/"
    done
    for file in $OPTIONAL_GAME_FILES; do
        if [ -e "$GAME_DATA_FOLDER/$file" ]; then
            cp -R "$GAME_DATA_FOLDER/$file" "$BUNDLED_GAME_FOLDER/"
        fi
    done
}

bundle_sdl() {
    installed_sdl=$(otool -L "$BUNDLED_EXECUTABLE" | awk '/libSDL2/ { print $1 }')
    [ -n "$installed_sdl" ] || fail "$BUNDLED_EXECUTABLE is not linked against SDL2"
    bundled_sdl_name=$(basename "$installed_sdl")
    bundled_sdl_reference="@executable_path/../Frameworks/$bundled_sdl_name"
    cp "$installed_sdl" "$FRAMEWORKS_FOLDER/$bundled_sdl_name"
    chmod u+w "$FRAMEWORKS_FOLDER/$bundled_sdl_name"
    install_name_tool -id "$bundled_sdl_reference" "$FRAMEWORKS_FOLDER/$bundled_sdl_name"
    install_name_tool -change "$installed_sdl" "$bundled_sdl_reference" "$BUNDLED_EXECUTABLE"
}

engine_version() {
    sed -n 's/^#define APPVERSION *"\(.*\)".*/\1/p' "$SOURCE_FOLDER/CSBlinux.cpp"
}

bundle_identifier() {
    printf 'local.csb.%s' "$(printf '%s' "$APP_NAME" | tr '[:upper:]' '[:lower:]' | tr -cs 'a-z0-9' '-')"
}

write_info_plist() {
    cat > "$APP/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>
    <string>$APP_NAME</string>
    <key>CFBundleDisplayName</key>
    <string>$APP_NAME</string>
    <key>CFBundleIdentifier</key>
    <string>$(bundle_identifier)</string>
    <key>CFBundleExecutable</key>
    <string>$EXECUTABLE_NAME</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>$(engine_version)</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>LSApplicationCategoryType</key>
    <string>public.app-category.role-playing-games</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
EOF
}

# Changing the library paths invalidates the signatures, and Apple Silicon refuses unsigned code
sign_bundle() {
    codesign --force --sign - "$FRAMEWORKS_FOLDER"/*.dylib
    codesign --force --sign - "$APP"
}

check_game_data
build_release_executable
create_empty_bundle
copy_game_files
bundle_sdl
write_info_plist
sign_bundle
echo "Built $APP"
