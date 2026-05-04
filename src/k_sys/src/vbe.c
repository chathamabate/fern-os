
#include "k_sys/vbe.h"
#include "s_util/str.h"

void dump_vbe_info_block(const vbe_info_block_t *vib, void (*log)(const char *, ...)) {
    char sig_buf[sizeof(vib->sig) + 1] = {0};
    mem_cpy(sig_buf, vib->sig, sizeof(vib->sig));

    log("Sig: %s\n", sig_buf);
    log("Version: 0x%X\n", vib->version);

    log("OEM Str: %s\n", vbe_far_to_protected_32_ptr(vib->oem_str_ptr));
    log("Total Mem: 0x%X\n", vib->total_memory * (1024 * 64));
    log("OEM SW Rev: 0x%X\n", vib->oem_sw_revision);
    log("OEM Vendor Name: %s\n", vbe_far_to_protected_32_ptr(vib->oem_vendor_name_str));
    log("OEM Product Name: %s\n", vbe_far_to_protected_32_ptr(vib->oem_product_name_str));
    log("OEM Product Rev: %s\n", vbe_far_to_protected_32_ptr(vib->oem_product_rev_str));
}
