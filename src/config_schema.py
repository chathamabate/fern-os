
from fernconf.FCValue import *
from fernconf.FCTranslator import *
from fernconf.FCSchema import *
from fernconf.FCTooling import *

FOS_UINT = FCS_INT.with_extra_checks(
    non_neg=lambda fcv: Ok(None) if  cast(int, fcv) >= 0 else Err("value must be non-negative")
)

FOS_UINT32 = FOS_UINT.with_extra_checks(
    u32=lambda fcv: Ok(None) if cast(int, fcv) < 0x1_0000_0000 else Err("value too large for 32-bits")
)

def fern_os_range32_not_empty(fcv: FCValue) -> Result[None, str]:
    dv = cast(dict[str, FCValue], fcv)

    start = cast(int, dv["START"])
    end_inc = cast(int, dv["END_INC"])

    if end_inc < start:
        return Err(f"empty range provided: [{start}, {end_inc}]")

    return Ok(None)

def fern_os_range32_4k_aligned(fcv: FCValue) -> Result[None, str]:
    dv = cast(dict[str, FCValue], fcv)

    start = cast(int, dv["START"])
    end_inc = cast(int, dv["END_INC"])

    if start & (0x1000 - 1) != 0:
        return Err(f"range start is not 4k aligned")

    if (end_inc + 1) & (0x1000 - 1) != 0:
        return Err(f"range exclusive end is not 4k aligned")

    return Ok(None)

def fern_os_range32_exclusive_end_translate(prefix: str, value: FCValue, translator: FCTranslator) -> list[str]:
    dv = cast(dict[str, FCValue], value)
    end = cast(int, dv["END_INC"]) + 1

    # It is possible the value of `end` is 0x1_0000_0000
    return translator.definition(prefix + "_END", end)

FOS_RANGE32 = FCSchemaStruct([
    ("START", FOS_UINT32),
    ("END_INC", FOS_UINT32),
]).with_extra_checks(
        non_empty=fern_os_range32_not_empty, align_4k=fern_os_range32_4k_aligned
).with_translates(fos_os_range32_exclusive_end_translate)

def fern_os_mem_ranges_no_overlap(fcv: FCValue) -> Result[None, str]:
    dv = cast(dict[str, FCValue], value)

    return Ok(None)

FOS_MEM_RANGES = FCSchemaStruct([
    ("KERNEL", FOS_RANGE32.with_comment([
        "This is where the kernel elf is loaded into.",
        "",
        "NOTE: The linker script exposes the finer grained areas inside this range."
    ])),
    ("APP", FOS_RANGE32.with_comment([
        "Where user application elf files are loaded."
    ])),
    ("APP_ARGS", FOS_RANGE32.with_comment([
        "Where user application arguments are loaded."
    ])) ,
    ("FREE", FOS_RANGE32.with_comment([
        "The dynamic area region for user processes.",
        "i.e. where you can request_mem from."
    ])),
    ("SHARED", FOS_RANGE32.with_comment([
        "Where shared pages area placed."
    ])),
    ("STACK", FOS_RANGE32.with_comment([
        "Where kernel and user thread stacks live."
    ]))
])



FOS_MEM_LAYOUT = FCSchemaStruct([
    ("FULL_AREA", FOS_RANGE32.with_comment([
        "Virtual AND Physical memory area owned entirely by FernOS.",
        "",
        "NOTE: Other areas may still be used depending on where registers live and how the",
        "bootloader sets things up."
    ])),
    ("RANGES", FOS_MEM_RANGES)
])


fern_os_config_schema = FCSchemaStruct([
    ("MEM", FCS_INT)
])

if __name__ == "__main__":
    run_fernconf(fern_os_config_schema, gcc=FCT_GCC, make=FCT_MAKE, ld=FCT_LD32, gas=FCT_GAS)



