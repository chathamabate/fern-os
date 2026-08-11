
from fernconf.FCValue import *
from fernconf.FCTranslator import *
from fernconf.FCSchema import *
from fernconf.FCTooling import *

FOS_UINT = FCS_INT.with_extra_checks(
    non_neg=lambda fcv: Ok(None) if cast(int, fcv) >= 0 else Err("value must be non-negative")
)

FOS_UINT32 = FOS_UINT.with_extra_checks(
    u32=lambda fcv: Ok(None) if cast(int, fcv) < 0x1_0000_0000 else Err("value too large for 32-bits")
)

def fern_os_uint_align_4k(fcv: FCValue) -> Result[None, str]:
    dv = cast(int, fcv)

    if dv & (0x1000 - 1) != 0:
        return Err(f"unsigned int is not 4k aligned: {dv}")

    return Ok(None)

FOS_UINT_4K = FOS_UINT.with_extra_checks(align_4k=fern_os_uint_align_4k)
FOS_UINT32_4K = FOS_UINT32.with_extra_checks(align_4k=fern_os_uint_align_4k)

def fern_os_range32_not_empty(fcv: FCValue) -> Result[None, str]:
    dv = cast(dict[str, FCValue], fcv)

    start = cast(int, dv["START"])
    end_inc = cast(int, dv["END_INC"])

    if end_inc < start:
        return Err(f"empty range provided: [0x{start:X}, 0x{end_inc}:X]")

    return Ok(None)

FOS_RANGE32 = FCSchemaStruct(
    [
        ("START", FOS_UINT32_4K),
        ("END_INC", FOS_UINT32),
    ], 

    # Exclusive end as derived field!
    #
    # See that this is not a UINT32, as 0x1_0000_0000 is a valid value for "END".
    END=(FOS_UINT_4K, lambda v: cast(dict[str, int], v)["END_INC"] + 1)
).with_extra_checks(
    non_empty=fern_os_range32_not_empty
)

def fern_os_mem_ranges_no_overlap(fcv: FCValue) -> Result[None, str]:
    dv = cast(dict[str, dict[str, int]], fcv)

    ranges = [
        (range_name, range_val["START"], range_val["END_INC"])
        for range_name, range_val in dv.items()
    ]

    start_key: Callable[[tuple[str, int, int]], int] = lambda r: r[1]

    ranges.sort(
       key=start_key # sort by start address will make things easier.
    )

    # Should never be empty, but whatever.
    if len(ranges) == 0:
        return Ok(None)

    for i in range(len(ranges) - 1):
        r = ranges[i]
        next_r = ranges[i + 1]

        # Remember index 2 is inclusive end.
        if r[2] >= next_r[1]:
            return Err(f"ranges overlap: {r[0]} and {next_r[0]}")

    return Ok(None)

FOS_PMEM_RANGES = FCSchemaStruct([
    ("BODY", FOS_RANGE32.with_default_any(
        [0x0040_0000, 0xBFFF_FFFF]
    ).with_comment([
        "This is the area of physical memory which is abstractly owned by FernOS."
        "It is expected that when the system startups up, this area is just normal accessible memory." 
        "(i.e. there should live no MMIO stuff in this range)"
    ]))
], 
    PROLOGUE=(
        FOS_RANGE32.with_comment([
            "Beginning of physical memory.",
            "It is very possible the bootloader puts things here!"
        ]),
        lambda fcv: [0, cast(dict[str, dict[str, int]], fcv)["FERN"]["START"] - 1]
    ),

    EPILOGUE=(
        FOS_RANGE32.with_comment([
            "End of physical memory.",
            "It is very possible the bootloader puts things here!"
        ]),
        lambda fcv: [cast(dict[str, dict[str, int]], fcv)["FERN"]["END"], 0xFFFF_FFFF]
    )
).with_comment([
    "How physical memory is laid out in FernOS."
])

FOS_VMEM_RANGES = FCSchemaStruct([
    ("KERNEL", FOS_RANGE32.with_default_any(
        [0x0040_0000, 0x01FF_FFFF]    
    ).with_comment([
        "This is where the kernel elf is loaded into.",
        "",
        "NOTE: The linker script exposes the finer grained areas inside this range."
    ])),

    ("APP", FOS_RANGE32.with_default_any(
        [0x0200_0000, 0x02FF_FFFF]
    ).with_comment([
        "Where user application elf files are loaded."
    ])),

    ("APP_ARGS", FOS_RANGE32.with_default_any(
        [0x0300_0000, 0x0300_3FFF]
    ).with_comment([
        "Where user application arguments are loaded."
    ])),

    ("FREE", FOS_RANGE32.with_default_any(
        [0x1000_0000, 0x3FFF_FFFF]
    ).with_comment([
        "The dynamic area region for user processes.",
        "i.e. where you can request_mem from."
    ])),

    ("SHARED", FOS_RANGE32.with_default_any(
        [0x6000_0000, 0x8FFF_FFFF]
    ).with_comment([
        "Where shared pages area placed."
    ])),

    ("STACK", FOS_RANGE32.with_default_any(
        [0xA000_0000, 0xBFFF_FFFF]
    ).with_comment([
        "Where kernel and user thread stacks live."
    ]))
]).with_extra_checks(no_overlap=fern_os_mem_ranges_no_overlap).with_comment([
    "How virtual memory is laid out in FernOS.",
    "",
    "NOTE: Some of these ranges may be identity mapped. (for example the kernel area range)"
])

def fern_os_mem_v_valid(fcv: FCValue) -> Result[None, str]:
    """
    Confirm all virtual memory ranges are within the FernOS physical memory range.
    """
    dv = cast(dict[str, FCValue], fcv)

    fa_value = cast(dict[str, int], dv["FULL_AREA"])
    fa_start = fa_value["START"]
    fa_end_inc = fa_value["END_INC"]

    ranges_value = cast(dict[str, dict[str, int]], dv["RANGES"])
    for r_name, r_value in ranges_value.items():
        r_start = r_value["START"]
        r_end_inc = r_value["END_INC"]

        if not (fa_start <= r_start and r_end_inc <= fa_end_inc):
            return Err(f"range outside full area: {r_name}")

    return Ok(None)

# I feel like these areas should be derived??
# Yeah, I think so too tbh?
FOS_MEM_LAYOUT = FCSchemaStruct([
    ("P", FOS_PMEM_RANGES),
    ("V", FOS_VMEM_RANGES)
])

FOS_SCHEMA = FCSchemaStruct([
    ("MEM", FOS_MEM_LAYOUT)
])

if __name__ == "__main__":
    run_fernconf(FOS_SCHEMA, gcc=FCT_GCC, make=FCT_MAKE, ld=FCT_LD32, gas=FCT_GAS)



