
from fernconf.FCValue import *
from fernconf.FCTranslator import *
from fernconf.FCSchema import *
from fernconf.FCTooling import *

def fos_bound_int(minimum: int, maximum: int) -> FCSchema:
    """
    An integer that is within an inclusive range!
    """

    # NOTE: I used to check if minimum and maximum could both be represented as 64-bit integers,
    # but I realized this was redundant as FCValue's (when integers) are guaranteed to be
    # representable in 64-bits! 
    #
    # If `minimum` were below INT64_MIN it would be still be impossible for a valid FCValue 
    # to be less than INT64_MIN.

    if maximum < minimum:
        raise Exception(f"Integer maximum {maximum} is less than minimum {minimum}")

    return FCS_INT.with_extra_checks(
            lower_bound=lambda fcv: Ok(None) if minimum <= cast(int, fcv) else Err(f"value is below minimum: {fcv}"),
            upper_bound=lambda fcv: Ok(None) if cast(int, fcv) <= maximum else Err(f"value is above maximum: {fcv}") 
    )

FOS_UINT32 = fos_bound_int(0, 0xFFFF_FFFF)
FOS_UINT32_AND_1 = fos_bound_int(0, 0x1_0000_0000)

def fern_os_uint_align_4k(fcv: FCValue) -> Result[None, str]:
    dv = cast(int, fcv)

    if dv & (0x1000 - 1) != 0:
        return Err(f"unsigned int is not 4k aligned: {dv}")

    return Ok(None)

FOS_UINT32_4K = FOS_UINT32.with_extra_checks(align_4k=fern_os_uint_align_4k)
FOS_UINT32_AND_1_4K = FOS_UINT32_AND_1.with_extra_checks(align_4k=fern_os_uint_align_4k)

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

        # This is a little frustrating, "END" will be a derived field thus given the original
        # fields are valid, "END" must also be valid by its schema.
        # i.e. This check confirms that "END_INC" ends with 0xFFF, which guarantees "END"
        # will fit schema `FOS_UINT32_AND_1_4K`
        ("END_INC", FOS_UINT32.with_extra_checks(
            valid_range_end=lambda fcv: 
            Ok(None) if (cast(int, fcv) & 0xFFF) == 0xFFF else Err(f"Invalid incluive end value: 0x{cast(int, fcv):X}")
        )), # This is a UINT to allow for 0x1_0000_0000.
    ], 

    END=(FOS_UINT32_AND_1_4K, lambda v: cast(dict[str, int], v)["END_INC"] + 1)
).with_extra_checks(
    non_empty=fern_os_range32_not_empty
)

####################################################################################################

# FernOS Memory Layout

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
        lambda fcv: [0, cast(dict[str, dict[str, int]], fcv)["BODY"]["START"] - 1]
    ),

    EPILOGUE=(
        FOS_RANGE32.with_comment([
            "End of physical memory.",
            "It is very possible the bootloader puts things here!"
        ]),
        lambda fcv: [cast(dict[str, dict[str, int]], fcv)["BODY"]["END"], 0xFFFF_FFFF]
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
    pmem = cast(dict[str, dict[str, int]], dv["P"])
    body_start = pmem["BODY"]["START"]
    body_end = pmem["BODY"]["END"]

    vmem = cast(dict[str, dict[str, int]], dv["V"])
    for vr_name, vr in vmem.items():
        vr_start = vr["START"]
        vr_end = vr["END"]
        
        if not (body_start <= vr_start and vr_end <= body_end):
            return Err(f"V range outside FernOS body: {vr_name}")

    return Ok(None)

FOS_MEM_LAYOUT = FCSchemaStruct([
    ("P", FOS_PMEM_RANGES),
    ("V", FOS_VMEM_RANGES)
]).with_extra_checks(valid_virtual_ranges=fern_os_mem_v_valid)

####################################################################################################

FOS_SCHEMA = FCSchemaStruct([
    ("MEM", FOS_MEM_LAYOUT)
])

if __name__ == "__main__":
    run_fernconf(FOS_SCHEMA, gcc=FCT_GCC, make=FCT_MAKE, ld=FCT_LD32, gas=FCT_GAS)



