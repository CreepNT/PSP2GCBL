#include <taihen.h>
#include <psp2/ctrl.h>  //sceCtrlPeekBufferPositive
#include <psp2/appmgr.h> //sceAppMgrAppParamGetString
#include <psp2common/types.h> //SCE_UID_INVALID_UID
#include <psp2/kernel/clib.h> //sceClibPrintf, sceClibStrncmp
#include <psp2/kernel/modulemgr.h> //SCE_KERNEL_START_xxx

#define MODNAME "BootVita"
#define MODFPRT 0x7A1D621C

#define ARRSZ(x) (sizeof((x)) / sizeof((x)[0]))

/** TAI_NEXT macro by Marburg */
#define TAI_NEXT(this_func, hook, ...) ({ \
  (((struct _tai_hook_user *)hook)->next) ? \
    ((__typeof__(&this_func))((struct _tai_hook_user *)((struct _tai_hook_user *)hook)->next)->func)(__VA_ARGS__) \
  : \
    ((__typeof__(&this_func))((struct _tai_hook_user *)hook)->old)(__VA_ARGS__) \
  ; \
})

struct patchlist {
    const void *patch;
    size_t patch_size;
    const uint32_t *offsets;
    size_t num_injects;
    SceUID *inject_ids;
};

#define DEFINE_PATCHLIST(name, patch_data, ...)                                     \
    static const uint32_t PATCHLIST_OFFSETS_ ##name [] = { __VA_ARGS__ };        \
    static SceUID PATCHLIST_IDS_ ##name[ARRSZ( PATCHLIST_OFFSETS_ ##name )];        \
    static const struct patchlist name = {                                          \
        .patch = patch_data, .patch_size = ARRSZ(patch_data),                       \
        .offsets = PATCHLIST_OFFSETS_ ##name,                                       \
        .inject_ids = PATCHLIST_IDS_ ##name,                                        \
        .num_injects = ARRSZ( PATCHLIST_OFFSETS_ ##name )                           \
    }

int apply_patchlist(SceUID modid, uint32_t seg, const struct patchlist *pl) {
    for (size_t i = 0; i < pl->num_injects; i++) {
        SceUID res = taiInjectData(modid, seg, pl->offsets[i], pl->patch, pl->patch_size);
        if (res < 0) {
            sceClibPrintf("Failed to apply patchlist %p[%zu]: 0x%08X\n", pl, i, res);
            return res;
        }

        pl->inject_ids[i] = res;
    }
    return SCE_OK;
}

void free_patchlist(const struct patchlist *pl)
{
    for (size_t i = 0; i < pl->num_injects; i++) {
        if (pl->inject_ids[i] > 0) {
            taiInjectRelease(pl->inject_ids[i]);
        }
    }
}

/* nop (Thumb, wide/32-bit form) */
const uint8_t T2_NOP_W[] = { 0xAF, 0xF3, 0x00, 0x80 };

/* movw lr, #5 (Thumb, wide) */
const uint8_t T2_MOVW_LR_5[] = { 0x40, 0xF2, 0x05, 0x0E };

/**
 * When a charge ends, a timer that inhibits player actions if
 * non-zero is set to 30 on patched game versions.
 * On Black Label, charge ending does not set this timer.
 * Simulate this by NOP'ing the `strh` that writes 30 to memory.
 */
DEFINE_PATCHLIST(chargeboots_fix, T2_NOP_W,
    0x0DC62C, /* Aranos 1 */
    0x0F4138, /* Oozla */
    0x47CE80, /* Maktar Nebula */
    0x972A48, /* Jamming Array */
    0x5DA0E6, /* Endako */
    0x6767BA, /* Barlow */
    0x78B93C, /* Notak */
/*  0x972A48,    Ship Shack - same as Jamming Array */
    0x7E4370, /* Siberius */
    0x85D04E, /* Tabora */
    0x8953C8, /* Dobbo */
    0x1C86FA, /* Joba */
    0x245C18, /* Todano */
    0x28473C, /* Boldan */
    0x304760, /* Aranos 2 */
    0x35DA7A, /* Snivelak */
    0x3BB3D8, /* Smolg */
    0x3F8084, /* Damosel */
    0x445942, /* Grelbin */
    0x4DA510  /* Yeedil */
);

/**
 * Ship shack prices are loaded from a LUT.
 * Replace the `ldr lr, [r4, #0x144]` with a `movw lr, #5`
 * to emulate the 5 Raritanium items trick of PS2 BL.
 */
DEFINE_PATCHLIST(ship_shack_fix, T2_MOVW_LR_5, 0xA53EC4);

/**
 * QoL/identifying feature: disable the annoying epilepsy
 * warning and SCEE presents screens.
 */
DEFINE_PATCHLIST(no_nagscreens, T2_NOP_W,
    0x0009A2,
    0x000A44,
    0x000AE6,
    0x000B8A,
    0x000C5A,
    0x000D04,
    0x000DAE,
    0x000E58,
    0x000F00,
    0x000FA6,
    0x001064,
    0x0010DC
);

void _start() __attribute__((weak, alias ("module_start")));
int module_start(SceSize argc, const void* args) {
    static tai_module_info_t module_info;
    int res;

    sceClibPrintf("PSP2GCBL by CreepNT\n");
    sceClibPrintf("Original BLMod by isaak & robo\n");

    module_info.size = sizeof(module_info);
    res = taiGetModuleInfo(MODNAME, &module_info);
    if (res < 0) {
        sceClibPrintf("Can't find shit in Detroit... (0x%08X)\n", res);
        goto fail;
    }

    if (module_info.module_nid != MODFPRT) {
        sceClibPrintf("Unsupported game version (dbg_fprt = 0x%08X)\n", module_info.module_nid);
        goto fail;
    }

    res = apply_patchlist(module_info.modid, 0, &chargeboots_fix);
    if (res < 0) {
        sceClibPrintf("Failed to apply CBFix patch (0x%08X)\n", res);
        goto fail;
    }

    res = apply_patchlist(module_info.modid, 0, &ship_shack_fix);
    if (res < 0) {
        sceClibPrintf("Failed to apply SSFix patch (0x%08X)\n", res);
        goto fail;
    }

    res = apply_patchlist(module_info.modid, 0, &no_nagscreens);
    if (res < 0) {
        sceClibPrintf("Failed to apply NNS patch (0x%08X)\n", res);
    }

    sceClibPrintf("All patches applied - enjoy the Black Label experience!");
    return SCE_KERNEL_START_SUCCESS;

fail:
    return SCE_KERNEL_START_NO_RESIDENT;
}

int module_stop(SceSize argc, const void *args) {
    free_patchlist(&chargeboots_fix);
    free_patchlist(&ship_shack_fix);
    free_patchlist(&no_nagscreens);

    return SCE_KERNEL_STOP_SUCCESS;
}