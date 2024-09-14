#include "options.h"
#include "pNesX_System_DC.h"
#include "vmu_icons.h"

#define OPTIONS_FILENAME "FrNES.opt"
Options_t options;

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

bool load_options_from_VMU() {
    printf("VMU: Attempting to Load User Options from attached VMUs\n");
    bool loaded = false;
    for (uint8 vmuIndex = 0; (!loaded) && (vmuIndex < 8); vmuIndex++) {
        maple_device_t* vmu = maple_enum_type(vmuIndex, MAPLE_FUNC_MEMCARD);
        if (vmu == NULL) 
            break;

        draw_VMU_icon(vmu, vmu_screen_loading);

        unsigned char* readBuffer;
        int readBufferSize;
        if (vmufs_read(vmu, OPTIONS_FILENAME, (void**)&readBuffer, &readBufferSize) == 0) {
            vmu_pkg_t package;
            if (vmu_pkg_parse(readBuffer, &package) == 0) {
                printf("VMU: Found FrNES Options File from VMU [%i]\n", vmuIndex);
                printf("Copying %i Bytes into options struct\n", sizeof(Options_t));
                memcpy(&options, package.data, sizeof(Options_t));
                draw_VMU_icon(vmu, vmu_screen_normal);
                loaded = true;
            } else {
                printf("VMU: Package failed CRC check\n");
                draw_VMU_icon(vmu, vmu_screen_error);
            }

            free(readBuffer);
            break;
        } else {
            printf("VMU: Unable to load User Options from VMU [%i]\n", vmuIndex);
            draw_VMU_icon(vmu, vmu_screen_normal);            
        }
    }
    return loaded;
}

bool save_options_to_VMU() {
    bool saved = false;

    for (int i = 0; (!saved) && (i < 8); i++) {
        printf("VMU: Attempting to Save SRAM to VMU [%i]\n", i);
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
        package.data_len = sizeof(Options_t);
        package.icon_data = NULL;
        package.eyecatch_data = NULL;
        package.data = (uint8*)&options;

        printf("VMU: Compiling Package\n");
        unsigned char* packageBuffer;
        int packageBufferLength;
        if (vmu_pkg_build(&package, &packageBuffer, &packageBufferLength) == 0) {
            printf("VMU: Saving Package to VMU as [%s]\n", OPTIONS_FILENAME);
            if (vmufs_write(vmu, OPTIONS_FILENAME, packageBuffer, packageBufferLength, VMUFS_OVERWRITE) == 0) {
                printf("VMU: Saved SRAM Save File to VMU [%i]\n", i);
                saved = 1;
                draw_VMU_icon(vmu, vmu_screen_normal);
                break;
            } else {
                printf("VMU: Unable to save SRAM Save File to VMU [%i]\n", i);
                draw_VMU_icon(vmu, vmu_screen_error);				
            }

            free(packageBuffer);
        } else {
            printf("VMU: Error Compiling package\n");
        }
    }
	return saved;
}
