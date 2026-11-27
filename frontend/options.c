#include <stddef.h>
#include "options.h"
#include "../emulator/input.h"
#include "../emulator/mapper.h"
#include "../emulator/apu_synth.h"
#include "frontend.h"
#include "../emulator/vmu_icons.h"

#define OPTIONS_FILENAME "FrNES.opt"
// an older version's file, kept when it's moved to this one (VMU file names: 12 characters): FrNES.opt.5, .6
#define OPTIONS_OLD_FILENAME "FrNES.opt.%u"
#define OPTIONS_MAGIC 0x4f4e5246 // 'FRNO'
Options_t options;

typedef struct {
    uint32 magic;
    uint16 version;
    uint16 size;
} Options_File_Header_t;

void options_set_defaults() {
    memset(&options, 0, sizeof(options));
    options.GUI_BGColor = 0xFF0080FF;
    options.GUI_TextColor = 0xFF000000;
    options.GUI_SelectedTextColor = 0xFF00FF00;
    options.GUI_InsideWindowColor = 0xFF00803f;
    options.GUI_OutsideWindowColor = 0xFFFFFFFF;
    frnes_default_settings(&options.emulator);
    // the menus' defaults where they differ from the player's: 0.7.1 stretched with the filter on and hid the top and
    // bottom 8 lines, and showed no frame display
    options.emulator.bilinear = 1;
    options.emulator.crop_top = 8;
    options.emulator.crop_bottom = 8;
    options.emulator.hud_mode = 0;
    options.browser_index = 0;
    for (int port = 0; port < OPTIONS_PORTS; port++) {
        options_default_port(port);
    }
    // (the frame display on the right trigger held, as the emulator has it)
    options.hotkeys[HOTKEY_FRAME_DISPLAY] = 1;
    // (save and load state off: the game menu has them, and RT + a button fired them in play: Life Force's RT for the
    // frame display with X to shoot loaded a state each shot)
    options.hotkeys[HOTKEY_SAVE_STATE] = 0;
    options.hotkeys[HOTKEY_LOAD_STATE] = 0;
    // (the game menu on both triggers)
    options.hotkeys[HOTKEY_GAME_MENU] = 6;
}

void options_default_port(int port) {
    frnes_settings_t defaults;
    frnes_default_settings(&defaults);
    const frnes_pad_mapping_t* pad = &defaults.pads[0];
    Port_Options* options_port = &options.ports[port];
    memset(options_port, 0, sizeof(*options_port));
    options_port -> a = pad -> a_buttons;
    options_port -> b = pad -> b_buttons;
    options_port -> select = pad -> select_buttons;
    options_port -> start = CONT_START;
    options_port -> select_on_left_trigger = pad -> select_on_left_trigger;
    options_port -> analog_dpad = pad -> analog_dpad;
    options_sync_pads();
}

// a turbo button's mask as the emulator numbers it (frnes.h's FRNES_TURBO_*: one button each)
static uint8 turbo_button(uint32 mask) {
    static const uint32 masks[] = { 0, CONT_A, CONT_B, CONT_X, CONT_Y, CONT_C, CONT_Z };
    for (uint8 i = 1; i < sizeof(masks) / sizeof(masks[0]); i++) {
        if (masks[i] == mask) {
            return i;
        }
    }
    return FRNES_TURBO_NONE;
}

// NES Start's button as the emulator numbers it (frnes.h's FRNES_START_*; the Controls page's choices)
static uint8 start_button(uint32 mask) {
    static const uint32 masks[] = { CONT_START, CONT_A, CONT_B, CONT_X, CONT_Y, CONT_C, CONT_Z, 0, CONT_X | CONT_B, CONT_A | CONT_C };
    for (uint8 i = 0; i < sizeof(masks) / sizeof(masks[0]); i++) {
        if (masks[i] == mask) {
            return i;
        }
    }
    return FRNES_START_START;
}

int options_port_nes(int port) {
    int code = options.ports[port].plug & 7;
    return (code == 0) ? port : (((code >= 1) && (code <= 4)) ? code - 1 : OPTIONS_NES_NONE);
}

int options_port_device(int port) {
    int device = (options.ports[port].plug >> 3) & 3;
    return device;
}

static void port_plug(int port, int nes, int device) {
    options.ports[port].plug = (uint8)((nes == OPTIONS_NES_NONE ? 5 : nes + 1) | (device << 3));
}

void options_set_port_nes(int port, int nes) {
    int was = options_port_nes(port);
    for (int other = 0; (other < OPTIONS_PORTS) && (nes != OPTIONS_NES_NONE); other++) {
        if ((other != port) && (options_port_nes(other) == nes)) {
            // (the other port takes this one's old place)
            port_plug(other, was, options_port_device(other));
        }
    }
    port_plug(port, nes, options_port_device(port));
}

void options_set_port_device(int port, int device) {
    port_plug(port, options_port_nes(port), device);
}

// a controller in Dreamcast port 'port' now (its settings count only then: one taken out leaves nothing behind, a
// Zapper set on an empty port included)
static int port_plugged(int port) {
    maple_device_t* device = maple_enum_dev(port, 0);
    return (device != NULL) && (device -> info.functions & (MAPLE_FUNC_CONTROLLER | MAPLE_FUNC_MOUSE));
}

static int port_mouse(int port) {
    maple_device_t* device = maple_enum_dev(port, 0);
    return (device != NULL) && (device -> info.functions & MAPLE_FUNC_MOUSE);
}

void options_input_to_core() {
    // (as at first, each port on its own NES port: the core's automatic choice, the nth controller found)
    int moved = 0, plugged[OPTIONS_PORTS];
    for (int port = 0; port < OPTIONS_PORTS; port++) {
        plugged[port] = port_plugged(port);
        moved |= plugged[port] && !port_mouse(port) && (options_port_nes(port) != port);
    }
    for (int nes = 0; nes < 4; nes++) {
        uint8 source = moved ? INPUT_SOURCE_NONE : INPUT_SOURCE_AUTO;
        uint8 device = INPUT_DEVICE_AUTO;
        for (int port = 0; port < OPTIONS_PORTS; port++) {
            if (plugged[port] && !port_mouse(port) && (options_port_nes(port) == nes)) {
                device = options_port_device(port);
                // (a mouse is never a pad: Matt)
                if ((device == OPTIONS_DEVICE_PAD) && port_mouse(port)) {
                    device = OPTIONS_DEVICE_AUTO;
                }
                // (a Zapper is aimed with its own controller only when that's said: input.h's default is player 1's)
                source = (moved || (device == OPTIONS_DEVICE_ZAPPER) || (device == OPTIONS_DEVICE_PADDLE)) ? port : INPUT_SOURCE_AUTO;
            }
        }
        input_set_port_source(nes, source);
        if (nes < 2) {
            input_set_port_device(nes, device);
        }
    }
    // (a port that's the paddle is NES port 2's, whichever NES port it plugs into: the Vaus is port 2's, or the
    // Famicom's expansion port; a mouse set to Zapper is NES port 2's Zapper. A mouse plugs into no NES port of its own:
    // it's never a pad)
    for (int port = 0; port < OPTIONS_PORTS; port++) {
        int device = options_port_device(port);
        if (plugged[port] && ((device == OPTIONS_DEVICE_PADDLE) || (port_mouse(port) && (device == OPTIONS_DEVICE_ZAPPER)))) {
            input_set_port_source(1, port);
            input_set_port_device(1, device);
            break;
        }
    }
    // (a port set to plug into nothing isn't used at all: a light gun or mouse there isn't taken on auto either, Matt)
    uint8 disabled = 0;
    for (int port = 0; port < OPTIONS_PORTS; port++) {
        // (a mouse has no NES port of its own: its setting, from before it lost Plugs into, doesn't count)
        disabled |= ((options_port_nes(port) == OPTIONS_NES_NONE) && !port_mouse(port)) ? (1 << port) : 0;
    }
    input_set_ports_disabled(disabled);
    input_set_four_score(options.four_score);
    // (the light gun's calibration)
    int16 x_scale, x_offset, y_scale, y_offset;
    options_gun_calibration(&x_scale, &x_offset, &y_scale, &y_offset);
    zapper_gun_set_calibration(x_scale, x_offset, y_scale, y_offset, options.gun_hide_reticle);
    // (the sound's filter and quality: a value past the last, from a file of a later version, is off / standard)
    apu_synth_set_sound_filter((options.sound_filter <= APU_SYNTH_FILTER_FAMICOM) ? options.sound_filter : APU_SYNTH_FILTER_OFF);
    apu_synth_set_sound_enabled(!options.sound_off);
    apu_synth_set_sound_quality((options.sound_quality <= APU_SYNTH_QUALITY_HIGH) ? options.sound_quality : APU_SYNTH_QUALITY_STANDARD);
}

void options_sync_pads() {
    // (each NES port's buttons are those of the Dreamcast port plugged into it, or its own when none is)
    for (int nes = 0; nes < FRNES_PADS; nes++) {
        int port = nes;
        for (int p = 0; p < OPTIONS_PORTS; p++) {
            if (options_port_nes(p) == nes) {
                port = p;
            }
        }
        frnes_pad_mapping_t* pad = &options.emulator.pads[nes];
        pad -> a_buttons = options.ports[port].a;
        pad -> b_buttons = options.ports[port].b;
        pad -> select_buttons = options.ports[port].select;
        pad -> select_on_left_trigger = options.ports[port].select_on_left_trigger;
        pad -> analog_dpad = options.ports[port].analog_dpad;
        pad -> turbo_buttons = turbo_button(options.ports[port].turbo_a) | (turbo_button(options.ports[port].turbo_b) << 4);
        pad -> turbo_rate = (options.ports[port].turbo_rate & (FRNES_TURBO_RATES - 1)) |
            (start_button(options.ports[port].start) << FRNES_START_SHIFT);
    }
    options.emulator.hotkeys[0] = options.hotkeys[HOTKEY_SAVE_STATE] | (options.hotkeys[HOTKEY_LOAD_STATE] << 4);
    // (the SD Card page's choice)
    // (states_on_sd / battery_on_sd: 0 a VMU, 1 the SD card, 2 off)
    options.emulator.state_device = (options.states_on_sd == 2) ? FRNES_STATE_OFF : (options.states_on_sd ? FRNES_STATE_SD : FRNES_STATE_VMU);
    options.emulator.hotkeys[1] = options.hotkeys[HOTKEY_FRAME_DISPLAY] | (options.hotkeys[HOTKEY_GAME_MENU] << 4);
    // (options saved before the Zapper speed have 0 there)
    if ((options.emulator.zapper_speed < FRNES_ZAPPER_SPEED_MIN) || (options.emulator.zapper_speed > FRNES_ZAPPER_SPEED_MAX)) {
        options.emulator.zapper_speed = FRNES_ZAPPER_SPEED_DEFAULT;
    }
    // (options saved with the NES port 2 setting: the Zapper is now port B's device)
    if (options.emulator.port2_device && (options.emulator.port2_device < FRNES_PORT2_MODES) && !(options.ports[1].plug >> 3)) {
        options_set_port_device(1, options.emulator.port2_device);
    }
    options.emulator.port2_device = 0;
}

bool delete_options_from_VMU() {
    bool deleted = false;
    for (uint8 vmuIndex = 0; (!deleted) && (vmuIndex < 8); vmuIndex++) {
        maple_device_t* vmu = maple_enum_type(vmuIndex, MAPLE_FUNC_MEMCARD);
        if (vmu == NULL) 
            break;

        if (vmufs_delete(vmu, OPTIONS_FILENAME) == 0) {
            printf("VMU: Deleted options file\n");
            deleted = true;
        }
    }
    return deleted;
}

_Static_assert(offsetof(Options_t, gun_x_scale) == OPTIONS_V6_SIZE, "version 6's options are a prefix of version 7's");

void options_gun_calibration(int16* x_scale, int16* x_offset, int16* y_scale, int16* y_offset) {
    int set = options.gun_x_scale != 0;
    *x_scale = set ? options.gun_x_scale : OPTIONS_GUN_X_SCALE;
    *x_offset = set ? options.gun_x_offset : OPTIONS_GUN_X_OFFSET;
    *y_scale = set ? options.gun_y_scale : OPTIONS_GUN_Y_SCALE;
    *y_offset = set ? options.gun_y_offset : OPTIONS_GUN_Y_OFFSET;
}

void options_set_gun_calibration(int16 x_scale, int16 x_offset, int16 y_scale, int16 y_offset) {
    options.gun_x_scale = x_scale;
    options.gun_x_offset = x_offset;
    options.gun_y_scale = y_scale;
    options.gun_y_offset = y_offset;
}

// version 6's options: version 7's without the light gun's calibration (the defaults). 0 when 'data' was one
static int options_from_v6(const uint8_t* data, uint32_t size) {
    if (size != OPTIONS_V6_SIZE) {
        return -1;
    }
    memcpy(&options, data, OPTIONS_V6_SIZE);
    memset((uint8_t*)&options + OPTIONS_V6_SIZE, 0, sizeof(Options_t) - OPTIONS_V6_SIZE);
    // (version 6's padding)
    options.gun_hide_reticle = 0;
    options.sound_off = 0;
    options.sound_filter = 0;
    options.sound_quality = 0;
    options.browser_directory[OPTIONS_PATH_LENGTH - 1] = '\0';
    return 0;
}

// version 5's options: the emulator's settings had 2 pads (FRNES_PADS 4 since 6), the rest as now: the settings up to
// the second pad as they were, the third and fourth pads the defaults, the rest moved along. 0 when 'data' was one
int options_from_v5(const uint8_t* data, uint32_t size) {
    const uint32_t missing = (FRNES_PADS - 2) * sizeof(frnes_pad_mapping_t);
    const uint32_t pads_end = offsetof(Options_t, emulator) + offsetof(frnes_settings_t, pads) + 2 * sizeof(frnes_pad_mapping_t);
    if (size != OPTIONS_V6_SIZE - missing) {
        return -1;
    }
    memcpy(&options, data, pads_end);
    memcpy((uint8_t*)&options + pads_end + missing, data + pads_end, size - pads_end);
    memset((uint8_t*)&options + OPTIONS_V6_SIZE, 0, sizeof(Options_t) - OPTIONS_V6_SIZE);
    // (version 5's padding)
    options.gun_hide_reticle = 0;
    options.sound_off = 0;
    options.sound_filter = 0;
    options.sound_quality = 0;
    frnes_settings_t defaults;
    frnes_default_settings(&defaults);
    memcpy(&options.emulator.pads[2], &defaults.pads[2], missing);
    // (version 5 had no plug: its byte was reserved, and isn't trusted; each port on its own NES port)
    for (int port = 0; port < OPTIONS_PORTS; port++) {
        options.ports[port].plug = 0;
    }
    options.browser_directory[OPTIONS_PATH_LENGTH - 1] = '\0';
    return 0;
}

bool load_options_from_VMU() {
    printf("VMU: Attempting to Load User Options from attached VMUs\n");
    bool loaded = false;
    // (FrNES.opt on any VMU; with none, the copy of an older version's file kept when it was moved, FrNES.opt.6 then
    // .5: Matt's FrNES.opt was deleted, its .6 copy still there)
    char names[3][16];
    snprintf(names[0], sizeof(names[0]), "%s", OPTIONS_FILENAME);
    snprintf(names[1], sizeof(names[1]), OPTIONS_OLD_FILENAME, 6);
    snprintf(names[2], sizeof(names[2]), OPTIONS_OLD_FILENAME, 5);
    int from_copy = 0;
    for (int n = 0; (!loaded) && (n < 3); n++) {
    const char* name = names[n];
    from_copy = n > 0;
    for (uint8 vmuIndex = 0; (!loaded) && (vmuIndex < 8); vmuIndex++) {
        maple_device_t* vmu = maple_enum_type(vmuIndex, MAPLE_FUNC_MEMCARD);
        if (vmu == NULL) 
            break;

        draw_VMU_icon(vmu, vmu_screen_loading);

        unsigned char* readBuffer;
        int readBufferSize;
        if (vmufs_read(vmu, name, (void**)&readBuffer, &readBufferSize) == 0) {
            vmu_pkg_t package;
            if (vmu_pkg_parse(readBuffer, readBufferSize, &package) == 0) {
                const Options_File_Header_t* header = (const Options_File_Header_t*)package.data;
                if ((package.data_len >= (int)sizeof(*header)) && (header -> magic == OPTIONS_MAGIC) &&
                    (header -> version == OPTIONS_VERSION) && (header -> size == sizeof(Options_t)) &&
                    (package.data_len >= (int)(sizeof(*header) + sizeof(Options_t)))) {
                    printf("VMU: Found FrNES Options File (version %u) on VMU [%i]\n", header -> version, vmuIndex);
                    memcpy(&options, package.data + sizeof(*header), sizeof(Options_t));
                    options.browser_directory[OPTIONS_PATH_LENGTH - 1] = '\0';
                    loaded = true;
                } else if ((package.data_len >= (int)sizeof(*header)) && (header -> magic == OPTIONS_MAGIC) &&
                    ((header -> version == 5) || (header -> version == 6)) &&
                    (package.data_len >= (int)(sizeof(*header) + header -> size)) &&
                    (((header -> version == 5) ? options_from_v5(package.data + sizeof(*header), header -> size) :
                    options_from_v6(package.data + sizeof(*header), header -> size)) == 0)) {
                    printf("VMU: Found FrNES Options File (version %u) as [%s] on VMU [%i], moved to version %d\n", header -> version, name, vmuIndex, OPTIONS_VERSION);
                    // (the original kept beside it before anything writes the new version over it, unless a copy is
                    // there already)
                    char old_name[16];
                    snprintf(old_name, sizeof(old_name), OPTIONS_OLD_FILENAME, header -> version);
                    uint8_t* existing = NULL;
                    int existing_size = 0;
                    if (from_copy || (vmufs_read(vmu, old_name, (void**)&existing, &existing_size) == 0)) {
                        free(existing);
                    } else if (vmufs_write(vmu, old_name, readBuffer, readBufferSize, VMUFS_OVERWRITE) == 0) {
                        printf("VMU: kept the version %u file as [%s]\n", header -> version, old_name);
                    } else {
                        printf("VMU: no room to keep the version %u file as [%s]\n", header -> version, old_name);
                    }
                    loaded = true;
                } else {
                    // (0.7.1's file has no header, and a later version's another layout)
                    printf("VMU: FrNES Options File on VMU [%i] is from another version, using the defaults\n", vmuIndex);
                }
                draw_VMU_icon(vmu, vmu_screen_normal);
            } else {
                printf("VMU: Package failed CRC check\n");
                draw_VMU_icon(vmu, vmu_screen_error);
            }

            free(readBuffer);
            break;
        } else {
            printf("VMU: Unable to load User Options [%s] from VMU [%i]\n", name, vmuIndex);
            // (what is on it, for telling a missing file from an unreadable one: the console log only)
            vmu_dir_t* dir = NULL;
            int count = 0;
            if ((n == 0) && (vmufs_readdir(vmu, &dir, &count) == 0)) {
                printf("VMU: [%c%d] has %d files, %d blocks free:", 'A' + vmu -> port, vmu -> unit, count, vmufs_free_blocks(vmu));
                for (int i = 0; i < count; i++) {
                    printf(" %.12s (%d)", dir[i].filename, dir[i].filesize);
                }
                printf("\n");
                free(dir);
            } else if (n == 0) {
                printf("VMU: [%c%d]'s directory can't be read either\n", 'A' + vmu -> port, vmu -> unit);
            }
            draw_VMU_icon(vmu, vmu_screen_normal);            
        }
    }
    }
    // (the emulator's part brought in line now, a moved file's included: the menus' change check then sees only what
    // the player changes, and nothing is written back until they do)
    if (loaded) {
        options_sync_pads();
        // (the frame display's old instruction cache mode, 2, is on: the profiling display shows both caches)
        if (options.emulator.hud_mode > 1) {
            options.emulator.hud_mode = 1;
        }
        // (from a kept copy: written back as FrNES.opt, so the next start finds it)
        if (from_copy) {
            save_options_to_VMU();
        }
    }
    return loaded;
}

bool save_options_to_VMU() {
    bool saved = false;

    for (int i = 0; (!saved) && (i < 8); i++) {
        printf("VMU: Attempting to Save Options to VMU [%i]\n", i);
        maple_device_t* vmu = maple_enum_type(i, MAPLE_FUNC_MEMCARD);
        if (vmu == NULL)
            break;

        draw_VMU_icon(vmu, vmu_screen_saving);

        printf("VMU: Generating VMU Package\n");
        vmu_pkg_t package;
        // Short Description
        char description[20];
        snprintf(description, 20, "%s %s OPTS", APP_STRING, APP_VERSION);
        strcpy(package.desc_short, description);

        // Long Description
        strcpy(package.desc_long, "User Configuration for FrNES");

        // App Id
        snprintf(package.app_id, 20, "%s %s", APP_STRING, APP_VERSION);

        // Final Package Fields
        package.icon_cnt = 0;
        package.icon_anim_speed = 0;
        package.eyecatch_type = VMUPKG_EC_NONE;
        memset(&package.icon_pal, 0, 16 * 2);
        uint8 data[sizeof(Options_File_Header_t) + sizeof(Options_t)];
        Options_File_Header_t header = { OPTIONS_MAGIC, OPTIONS_VERSION, sizeof(Options_t) };
        memcpy(data, &header, sizeof(header));
        memcpy(data + sizeof(header), &options, sizeof(Options_t));
        package.data_len = sizeof(data);
        package.icon_data = NULL;
        package.eyecatch_data = NULL;
        package.data = data;

        printf("VMU: Compiling Package\n");
        unsigned char* packageBuffer;
        int packageBufferLength;
        if (vmu_pkg_build(&package, &packageBuffer, &packageBufferLength) == 0) {
            printf("VMU: Saving Package to VMU as [%s]\n", OPTIONS_FILENAME);
            if (vmufs_write(vmu, OPTIONS_FILENAME, packageBuffer, packageBufferLength, VMUFS_OVERWRITE) == 0) {
                printf("VMU: Saved Options to VMU [%i]\n", i);
                saved = 1;
                draw_VMU_icon(vmu, vmu_screen_normal);
            } else {
                printf("VMU: Unable to save Options to VMU [%i]\n", i);
                draw_VMU_icon(vmu, vmu_screen_error);				
            }

            free(packageBuffer);
        } else {
            printf("VMU: Error Compiling package\n");
        }
    }
	return saved;
}
