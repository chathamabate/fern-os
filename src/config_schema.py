
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
            lower_bound=lambda fcv: Ok(None) if minimum <= cast(int, fcv) else Err(f"value {fcv} is below minimum {minimum}"),
            upper_bound=lambda fcv: Ok(None) if cast(int, fcv) <= maximum else Err(f"value {fcv} is above maximum: {maximum}") 
    )

FOS_UINT32 = fos_bound_int(0, 0xFFFF_FFFF)
FOS_UINT32_AND_1 = fos_bound_int(0, 0x1_0000_0000)

def fern_os_uint_align_4k(fcv: FCValue) -> Result[None, str]:
    dv = cast(int, fcv)

    if dv & (0x1000 - 1) != 0:
        return Err(f"unsigned int is not 4k aligned: 0x{dv:X}")

    return Ok(None)

FOS_UINT32_4K = FOS_UINT32.with_extra_checks(align_4k=fern_os_uint_align_4k)
FOS_UINT32_AND_1_4K = FOS_UINT32_AND_1.with_extra_checks(align_4k=fern_os_uint_align_4k)

FOS_RANGE32 = FCSchemaStruct(
    [
        ("START", FOS_UINT32_4K),
        ("END", FOS_UINT32_AND_1_4K),
    ], 

    SIZE=(
        # By having this be unsigned, a negative size will fail! 
        # (Which confirms START <= END... empty ranges being allowed)
        #
        # NOTE: The 4K check on size is a little redundant, but that's ok!
        FOS_UINT32_AND_1_4K,
        lambda v: cast(dict[str, int], v)["END"] - cast(dict[str, int], v)["START"]
    )
)

####################################################################################################

# FernOS Core Properties + Memory Layout

def fern_os_mem_ranges_no_overlap(fcv: FCValue) -> Result[None, str]:
    dv = cast(dict[str, dict[str, int]], fcv)

    ranges = [
        (range_name, range_val["START"], range_val["END"])
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

        # Remember index 2 is exclusive end.
        if r[2] > next_r[1]:
            return Err(f"ranges overlap: {r[0]} and {next_r[0]}")

    return Ok(None)

FOS_PMEM_RANGES = FCSchemaStruct([
    ("BODY", FOS_RANGE32.with_default_any(
        [0x0040_0000, 0xC000_0000]
    ).with_comment([
        "This is the area of physical memory which is abstractly owned by FernOS."
        "It is expected that when the system startups up, this area is just normal accessible memory." 
        "(i.e. there should live no MMIO stuff in this range)"
    ]))
], 
    PROLOGUE=(
        FOS_RANGE32.with_comment([
            "Beginning of physical memory. (excluding first physical page)",
            "It is very possible and allowed for the bootloader to puts things here!"
        ]),
        lambda fcv: [0x1000, cast(dict[str, dict[str, int]], fcv)["BODY"]["START"]]
    ),

    EPILOGUE=(
        FOS_RANGE32.with_comment([
            "End of physical memory. (excluding last physical page)",
            "It is very possible and allowed for the bootloader to puts things here!"
        ]),
        lambda fcv: [cast(dict[str, dict[str, int]], fcv)["BODY"]["END"], 0x1_0000_0000 - 0x1000]
    )
).with_comment([
    "How physical memory is laid out in FernOS."
])

FOS_VMEM_RANGES = FCSchemaStruct([
    ("KERNEL", FOS_RANGE32.with_default_any(
        [0x0040_0000, 0x0200_0000]    
    ).with_comment([
        "This is where the kernel elf is loaded into.",
        "",
        "NOTE: The linker script exposes the finer grained areas inside this range."
    ])),

    ("APP", FOS_RANGE32.with_default_any(
        [0x0200_0000, 0x0300_0000]
    ).with_comment([
        "Where user application elf files are loaded."
    ])),

    ("APP_ARGS", FOS_RANGE32.with_default_any(
        [0x0300_0000, 0x0300_4000]
    ).with_comment([
        "Where user application arguments are loaded."
    ])),

    ("FREE", FOS_RANGE32.with_default_any(
        [0x1000_0000, 0x4000_0000]
    ).with_comment([
        "The dynamic area region for user processes.",
        "i.e. where you can request_mem from."
    ])),

    ("SHARED", FOS_RANGE32.with_default_any(
        [0x6000_0000, 0x9000_0000]
    ).with_comment([
        "Where shared pages area placed."
    ])),

    ("STACK", FOS_RANGE32.with_default_any(
        [0xA000_0000, 0xC000_0000]
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
    pmem = cast(dict[str, dict[str, int]], dv["PMEM"])
    body_start = pmem["BODY"]["START"]
    body_end = pmem["BODY"]["END"]

    vmem = cast(dict[str, dict[str, int]], dv["VMEM"])
    for vr_name, vr in vmem.items():
        vr_start = vr["START"]
        vr_end = vr["END"]
        
        if not (body_start <= vr_start and vr_end <= body_end):
            return Err(f"V range outside FernOS body: {vr_name}")

    return Ok(None)

def fern_os_stack_sizes_valid(fcv: FCValue) -> Result[None, str]:
    """
    Confirms the defined stack area in VMEM is large enough to fit the kernel stack and thread
    stacks!
    """
    dv = cast(dict[str, FCValue], fcv)

    kstack_size = cast(int, dv["KSTACK_SIZE"])
    tstack_size = cast(int, dv["TSTACK_SIZE"])
    threads_per_proc = cast(int, dv["MAX_THREADS_PER_PROC"])

    min_stack_area_size = kstack_size + (tstack_size * threads_per_proc)

    vmem = cast(dict[str, dict[str, int]], dv["VMEM"])
    stack_area_size = vmem["STACK"]["SIZE"]

    if min_stack_area_size > stack_area_size:
        return Err(f"Minimum stack area size 0x{min_stack_area_size:X} is greater than actual stack area size 0x{stack_area_size:X}")

    return Ok(None)

FOS_CORE = FCSchemaStruct(
    [
        # Memory layout
        ("PMEM", FOS_PMEM_RANGES),
        ("VMEM", FOS_VMEM_RANGES),

        # Core properties

        ("KSTACK_SIZE", FOS_UINT32_4K.with_default_any(256 * 1024).with_extra_checks(
            two_page_min=lambda v: Ok(None) if cast(int, v) >= 2 * 4 * 1024 else Err("Stacks must be at least 2 pages")
        ).with_comment([
            "Kernel stack size (including redzone page)",
            "Guaranteed to be at least 2 pages"
        ])),

        ("TSTACK_SIZE", FOS_UINT32_4K.with_default_any(4 * 1024 * 1024).with_extra_checks(
            two_page_min=lambda v: Ok(None) if cast(int, v) >= 2 * 4 * 1024 else Err("Stacks must be at least 2 pages")
        ).with_comment([
            "Size of a single user thread stack (including redzone page)",
            "Guaranteed to be at least 2 pages"
        ])),

        # 512 has no significance here, just an arbitrary multiple of 8 I picked.
        # Also, I decided early on that having max procs be a multiple of 8, makes logic pretty
        # simple in certain areas.
        ("MAX_PROCS", fos_bound_int(1, 512).with_extra_checks(
            mult_of_8=lambda v: Ok(None) if cast(int, v) % 8 == 0 else Err("Not multple of 8")
        ).with_default_any(256).with_comment([
            "Maximum number of processes (guaranteed to be a multiple of 8)"
        ])),

        # MUST be less than or equal to 32 to fit all thread bits into a single 32-bit vector!
        ("MAX_THREADS_PER_PROC", fos_bound_int(1, 32).with_default_any(16).with_comment([
            "Guaranteed to be <= 32"
        ])),

        # Must be able to fit a handle id into a single byte!
        ("MAX_HANDLES_PER_PROC", fos_bound_int(1, 256).with_default_any(32).with_comment([
            "Guarnateed to be <= 256"
        ])),

        # Similarly, must be able to fit a plugin id into a single byte!
        ("MAX_PLUGINS", fos_bound_int(1, 256).with_default_any(16).with_comment([
            "Guarnateed to be <= 256"
        ]))
    ]

    # I had planned to derive the start and end of the kernel stack here.
    # But I may just do that in a C header tbh, constructing the lambda will take a lot
    # of ugly casting. Plus I will be required to define the user thread stack helper macros in
    # a header file anyway.

).with_extra_checks(
    # Confirm all virtual ranges are within BODY.
    valid_virtual_ranges=fern_os_mem_v_valid,
    valid_stack_sizes=fern_os_stack_sizes_valid
)

####################################################################################################

FOS_SCHEMA = FCSchemaStruct([
    ("CORE", FOS_CORE)
])

if __name__ == "__main__":
    run_fernconf(FOS_SCHEMA, gcc=FCT_GCC, make=FCT_MAKE, ld=FCT_LD32, gas=FCT_GAS)



