
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
        return Err(f"empty range provided: [0x{start:X}, 0x{end_inc}:X]")

    return Ok(None)

def fern_os_range32_4k_aligned(fcv: FCValue) -> Result[None, str]:
    dv = cast(dict[str, FCValue], fcv)

    start = cast(int, dv["START"])
    end = cast(int, dv["END"])

    if start & (0x1000 - 1) != 0:
        return Err(f"range start is not 4k aligned")

    if end & (0x1000 - 1) != 0:
        return Err(f"range exclusive end is not 4k aligned")

    return Ok(None)

FOS_RANGE32 = FCSchemaStruct(
    [
        ("START", FOS_UINT32),
        ("END_INC", FOS_UINT32),
    ], 

    # Exclusive end as derived field.
    END=(FOS_UINT32, lambda v: cast(dict[str, int], v)["END_INC"] + 1)
).with_extra_checks(
    non_empty=fern_os_range32_not_empty, align_4k=fern_os_range32_4k_aligned
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

FOS_MEM_RANGES = FCSchemaStruct([
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
]).with_extra_checks(no_overlap=fern_os_mem_ranges_no_overlap)

def fern_os_mem_layout_all_valid(fcv: FCValue) -> Result[None, str]:
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

FOS_MEM_LAYOUT = FCSchemaStruct([
    ("FULL_AREA", FOS_RANGE32.with_default_any(
        [0x0040_0000, 0xBFFF_FFFF]
    ).with_comment([
        "Virtual AND Physical memory area owned entirely by FernOS.",
        "",
        "NOTE: Other areas may still be used depending on where registers live and how the",
        "bootloader sets things up."
    ])),
    ("RANGES", FOS_MEM_RANGES)
]).with_extra_checks(all_valid=fern_os_mem_layout_all_valid)

FOS_SCHEMA = FCSchemaStruct([
    ("MEM", FOS_MEM_LAYOUT)
])

if __name__ == "__main__":
    run_fernconf(FOS_SCHEMA, gcc=FCT_GCC, make=FCT_MAKE, ld=FCT_LD32, gas=FCT_GAS)



