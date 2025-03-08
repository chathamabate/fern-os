
#pragma once

#include <stdint.h>
#include "fstndutil/misc.h"

typedef uint64_t gate_desc_t;

// Gate Descriptor Format : 
//
// ...
// selector  [16 : 31]
// ... 
// type      [48 : 52]
// privilege [53 : 54]
// present   [55 : 55]
// ...

#define GD_SELECTOR_OFF (16)      
#define GD_SELECTOR_WID (16)  
#define GD_SELECTOR_WID_MASK TO_MASK64(SD_SELECTOR_WID)
#define GD_SELECTOR_MASK (SD_SELECTOR_WID_MASK << SD_SELECTOR_OFF)       

#define GD_TYPE_OFF (48)      
#define GD_TYPE_WID (5)  
#define GD_TYPE_WID_MASK TO_MASK64(SD_TYPE_WID)
#define GD_TYPE_MASK (SD_TYPE_WID_MASK << SD_TYPE_OFF)       

#define GD_PRIVILEGE_OFF (53)      
#define GD_PRIVILEGE_WID (2)  
#define GD_PRIVILEGE_WID_MASK TO_MASK64(SD_PRIVILEGE_WID)
#define GD_PRIVILEGE_MASK (SD_PRIVILEGE_WID_MASK << SD_PRIVILEGE_OFF)       

#define GD_PRESENT_OFF (55)      
#define GD_PRESENT_WID (1)  
#define GD_PRESENT_WID_MASK TO_MASK64(SD_PRIVILEGE_WID)
#define GD_PRESENT_MASK (SD_PRIVILEGE_WID_MASK << SD_PRIVILEGE_OFF)       

static inline gate_desc_t gate_desc(void) {
    return 0;
}


