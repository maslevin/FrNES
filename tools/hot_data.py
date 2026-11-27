#!/usr/bin/env python3
"""Writes the linker script fragment that places the emulator's hot global data (tools/shlelf_hot.xc INCLUDEs it).

  python3 tools/hot_data.py OUT.ld CC "CFLAGS" file.c...

Where the linker puts each global decides which operand cache lines it shares with the NES memory pages (mapped at
fixed addresses, see memory.h) and with each other. With LTO that placement follows whatever else is linked, so an
unrelated change elsewhere moved the emulator's data and changed AccuracyCoin's speed by 7-10%. The .hotdata section
puts these globals in one block at a fixed offset from a 16 KB boundary (the operand cache size), so their cache sets
stay the same from build to build.

The variables come from compiling the sources without LTO (-fdata-sections gives each its own .bss.NAME / .data.NAME
section; LTO keeps those names, adding .lto_priv.N to statics and renumbering static locals). Small state goes
first, then the tables in TABLE_ORDER; data that is only streamed through once per frame (the frame buffer, the
sound write log and ring) or that is cold stays outside the block.
"""
import os
import re
import shlex
import subprocess
import sys
import tempfile

# The globals of the hot files in the block: an allowlist, so a new global or static stays out of it (and out of
# the way of the ones in it) until it is classified here. HOT are in the block, KNOWN_COLD and the COLD families
# below deliberately not; anything else is placed with ordinary data and reported loudly, to be added to one or the
# other (a new mapper static that slid into the block moved every table after it). Generated from the block as the
# rules below made it at perf-s4 0922142, over the default, CPU_C_ONLY, FRNES_OCINDEX_SPLIT and stats
# (PPU_STATS / FRNES_PERF_SPLIT / PPU_PERF) builds, so the block didn't change.
HOT = {
    # 6502_readbyte.c
    "cpu_internal_bus_value", "cpu_io_bus_cycle", "cpu_io_bus_value", "ppu_data_read_cycle",
    # 6502_writebyte.c
    "strobe_cycle", "strobe_saved_bit1", "strobe_saved_bit2",
    # apu.c
    "apu_channel_enabled", "apu_cycles_to_event", "apu_delays_pending", "apu_dmc_address", "apu_dmc_bits_remaining",
    "apu_dmc_buffer_full", "apu_dmc_bytes_remaining", "apu_dmc_disable_delay", "apu_dmc_irq_enable",
    "apu_dmc_irq_flag", "apu_dmc_loop", "apu_dmc_rate", "apu_dmc_request_due", "apu_dmc_request_time",
    "apu_dmc_sample_address", "apu_dmc_sample_length", "apu_dmc_start_delay", "apu_dmc_timer",
    "apu_frame_block_time", "apu_frame_blocking", "apu_frame_counter_cycles", "apu_frame_counter_mode",
    "apu_frame_irq_flag", "apu_frame_irq_read_pending", "apu_frame_irq_read_until", "apu_frame_irq_visible",
    "apu_frame_step", "apu_frame_write_cycles", "apu_frame_write_value", "apu_length", "apu_length_halt",
    "apu_length_new_halt", "apu_length_pending", "apu_length_pending_time", "apu_length_previous",
    "apu_length_reload",
    # apu_synth.c
    "apu_synth_muted", "apu_synth_tap", "cycles_per_sample_fp", "dc_previous_in", "dc_previous_out", "dmc",
    "eg_tick", "expansion", "log_head", "log_tail", "mmc5", "noise", "noise_level", "pulse", "pulse_level",
    "ring_read", "ring_write", "sample_fraction", "sequencer_counter", "sequencer_five_step", "sequencer_step",
    "stream", "synth_cycle", "triangle",
    # cpu.c
    "cpu_batch_cut", "cpu_batch_device_event", "cpu_batch_end", "cpu_batch_mapper_only", "cpu_batch_resync",
    "cpu_cycle_now", "cpu_cycle_total", "cpu_devices_synced_cycle", "cpu_instruction_end", "cpu_irq_raise_cycle",
    "cpu_irq_raise_recent", "cpu_nmi_raise_cycle", "cpu_nmi_raise_dots", "cpu_poll_early", "perf_split_apu",
    "perf_split_ppu",
    # mapper.c
    "axrom_bank", "bandai_chr", "bandai_counter", "bandai_irq_enabled", "bandai_latch", "bandai_outer",
    "bandai_prg", "bandai_submapper", "camerica_bank", "camerica_block", "eeprom", "fme7_command", "fme7_counter",
    "fme7_irq_control", "mapper_6000_mode", "mapper_6000_reads", "mapper_chr_latch", "mapper_chr_ram_slots",
    "mapper_clock_dot", "mapper_cpu_timer", "mapper_ext_attributes", "mapper_fetch_hooks", "mapper_irq_armed",
    "mapper_irq_flag", "mapper_number", "mapper_write_fn", "mapper_sprite_chr_split", "mapper_timer_cycle", "mmc1_chr0", "mmc1_chr1",
    "mmc1_control", "mmc1_count", "mmc1_last_write_cycle", "mmc1_prg", "mmc1_shift", "mmc2_chr", "mmc2_latch",
    "mmc3_a12", "mmc3_a12_low_cycle", "mmc3_bank_select", "mmc3_irq_counter", "mmc3_irq_enabled", "mmc3_irq_latch",
    "mmc3_irq_reload", "mmc3_mirroring", "mmc3_registers", "mmc5_chr", "mmc5_chr_last_b", "mmc5_chr_mode",
    "mmc5_chr_upper", "mmc5_exram_mode", "mmc5_fill_attribute", "mmc5_fill_tile", "mmc5_in_frame",
    "mmc5_irq_compare", "mmc5_irq_enabled", "mmc5_irq_pending", "mmc5_multiplicand", "mmc5_multiplier",
    "mmc5_nametables", "mmc5_prg", "mmc5_prg_mode", "mmc5_scanline", "mmc5_slot_ram", "mmc5_sprite_pages", "mmc6",
    "mmc6_a001", "mmc6_ram", "namco108_registers", "namco108_select", "rambo_counter", "rambo_cycle_mode",
    "ppu_chr_alias_slots", "rambo_enabled", "rambo_latch", "rambo_prescaler", "rambo_registers", "rambo_reload", "rambo_select",
    "sunsoft3_toggle", "sunsoft4_control", "sunsoft4_nt", "vrc1_chr", "vrc3_control", "vrc3_counter", "vrc3_latch",
    "vrc4_chr", "vrc4_prg", "vrc4_swap", "vrc_irq", "vrc_irq_used",
    # memory.c
    "PALETTE_RAM", "PPU_MEMORY_BUFFER_MMU_HANDLE", "RAM", "REGISTER_BUFFER_MMU_HANDLE", "SPRRAM", "VRAM", "WRAM",
    "WRAM_size", "cpu_registers", "nametable_slot_page", "ppu_memory_buffer", "ppu_nametable_generation",
    "ppu_registers", "prg_ram_written", "register_buffer",
    # ppu.c
    "bg_next_attribute", "bg_next_high", "bg_next_low", "bg_next_table", "bg_next_tile", "bg_shift_attribute_high",
    "bg_shift_attribute_low", "bg_shift_pattern_high", "bg_shift_pattern_low", "cached_dot", "color_line", "colors",
    "key_frame", "key_generation", "key_scanline", "key_setup", "lazy_hit_dot", "lazy_line_rendered",
    "lazy_x_pixel", "lazy_x_switches", "lazy_x_value", "line_start", "next_sprite_count",
    "next_sprite_zero_on_line", "oam2_missed", "oam2_missed_frame", "oam2_missed_line", "oam2_off_dot",
    "oam2_off_frame", "oam2_off_line", "oam_bucket", "oam_bucket_count", "oam_bucket_key", "opaque_line",
    "ppu_bit_spread", "ppu_blank_filled_dot", "ppu_bus_address_hook", "ppu_bus_frame_count", "ppu_copy_buffer",
    "ppu_dot", "ppu_evaluate_end_oamaddr", "ppu_evaluate_end_secondary", "ppu_evaluate_overflow_dot",
    "ppu_frame_count", "ppu_frame_ready", "ppu_line_buffer", "ppu_line_buffer_used", "ppu_line_mode",
    "ppu_mapper_clock_a12_dot", "ppu_mapper_clocked", "ppu_nmi_dots_after", "ppu_nmi_raised", "ppu_oam2_row",
    "ppu_oam_corrupt_rows", "ppu_oam_generation", "ppu_open_bus_frame", "ppu_output", "ppu_output_sq",
    "ppu_palette_dirty", "ppu_pattern_offsets", "ppu_pending_dots", "ppu_run_end_position", "ppu_running",
    "ppu_scanline", "ppu_scroll_resume_dot", "ppu_sprite_fetch_line", "ppu_sprite_fetched", "ppu_sprite_x_line",
    "ppu_sprite_x_origin", "ppu_stat_apu_us", "ppu_stat_fast_renders", "ppu_stat_lazy_lines",
    "ppu_stat_overflow_evaluations", "ppu_stat_overflow_predictions", "ppu_stat_perdot_dots", "ppu_stat_ppu_us",
    "ppu_stat_render_perf", "ppu_stat_render_us", "ppu_stat_replay_dots", "ppu_stat_replays",
    "ppu_stat_replays_early", "ppu_stat_replays_hblank", "ppu_stat_run_calls", "ppu_stat_sprite_perf",
    "ppu_stat_status_syncs", "ppu_stat_tail_perf", "ppu_stat_tail_us", "ppu_vbl_suppress", "row_generation",
    "row_key", "row_pattern", "row_table", "secondary", "sprite_attribute", "sprite_count", "sprite_line",
    "sprite_pattern_high", "sprite_pattern_low", "sprite_tile", "sprite_x", "sprite_y", "sprite_zero_on_line",
    "tile_colors",
    # video.c
    "finished_index", "frame_open", "pvr_params", "textures", "video_crumb_frame", "video_frames_submitted",
    "video_measured_hz", "video_pacing", "video_rate_start_us", "video_translucent_overlay", "video_vblank_count",
    "video_vblank_handle", "video_vblank_waiting", "work_index",
}
KNOWN_COLD = {
    # apu_synth.c
    "chunk", "fds", "fds_reciprocal", "n163", "n163_reciprocal", "opll_attenuation", "opll_sine", "pulse_mix",
    "ring", "s5b", "s5b_volume", "synth_frame_end", "tap_buffer", "tnd_mix", "vrc6", "vrc7", "write_log",
    # mapper.c
    "mapper_save_written", "prg_ram_disabled", "n163_address", "n163_counter", "n163_nametable", "n163_ram", "n163_write_protect",
    "rambo_a12", "rambo_a12_low_cycle", "tc0690_a12", "tc0690_a12_low_cycle", "tc0690_counter", "tc0690_delay", "tc0690_enabled", "tc0690_jetsons",
    "tc0690_latch", "tc0690_reload", "vrc2_latch", "vrc2_latch_mode",
    # memory.c
    "PPU_NAMETABLES_MMU_HANDLES", "RAM_MMU_HANDLES", "chr_slot_page", "nametable_pages_valid", "ROMBANK_MMU_HANDLES", "VROM_MMU_HANDLES", "WRAM_MMU_HANDLES",
    "utlb_shadow_pteh", "utlb_shadow_ptel",
    # ppu.c
    "ppu_framebuffer",
    # video.c
    "codebook",
}
# streamed through or cold: placement doesn't matter, and they're too large for the block
EXCLUDE = {"ppu_framebuffer", "write_log", "ring", "chunk", "codebook", "tap_buffer", "utlb_shadow_ptel", "utlb_shadow_pteh",
           "PPU_NAMETABLES_MMU_HANDLES", "VROM_MMU_HANDLES", "ROMBANK_MMU_HANDLES", "WRAM_MMU_HANDLES",
           "RAM_MMU_HANDLES", "tnd_mix", "pulse_mix", "video_crumbs"}
# state of mapper expansion sound chips (VRC7's FM tables alone are 3 kB; the FDS's sound, and its drive in fds.c, and
# the frame display's expansion timing too): only those cartridges use it, and in the
# block it took operand cache sets that the hot code's literal pools could otherwise have (tools/hot_layout.py)
# and mapper state only its own boards touch (Taito TC0690's IRQ, VRC2's $6000 latch) or that is written when a board
# saves (mapper_save_written): small, but each one in the block moved the tables after it (b4f6f9d -> fedd517: every
# renderer table one operand cache set later, PinBot +3%)
COLD = re.compile(r"^(opll_|vrc7|n163|vrc6|s5b|mmc5_pcm|mmc5_pulse|fme7_tone|fds$|fds_|expansion_|tc0690_|vrc2_|"
                  r"mapper_save_written$)")
# after the small state, in this order (hottest per dot / per tile first)
TABLE_ORDER = ["line_start", "secondary", "colors", "row_table", "row_pattern", "sprite_line", "opaque_line",
               "color_line", "ppu_line_buffer", "oam_bucket_count", "ppu_pattern_offsets", "ppu_bit_spread",
               "oam_bucket", "tile_colors"]
SMALL = 64
# after everything else in the block: hot variables added later that would otherwise sort in among the small state and
# move every variable after them onto other operand cache sets (a pointer added between mapper_number and the MMC3
# state brought back mapper_ppu_clock's literal / data conflicts in the release build)
LAST = ["mapper_write_fn"]
# the tables start at this offset in the block, whatever the small state before them adds up to (the link fails when
# it outgrows the room), so a new small variable doesn't move every table onto other operand cache sets
TABLES_AT = 0x480


def sections(cc, cflags, source, tmp):
    obj = os.path.join(tmp, os.path.basename(source) + ".o")
    subprocess.run([cc, "-c", *cflags, "-fno-lto", "-fdata-sections", "-o", obj, source], check=True)
    out = subprocess.run([os.path.join(os.path.dirname(cc), "sh-elf-objdump"), "-h", obj], check=True,
                         stdout=subprocess.PIPE, text=True).stdout
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 3 and re.match(r"\.(bss|data)\.", parts[1]) and int(parts[2], 16) > 0:
            yield parts[1], int(parts[2], 16)


def main():
    out, cc, cflags, sources = sys.argv[1], sys.argv[2], shlex.split(sys.argv[3]), sys.argv[4:]
    found = {}
    unknown = set()
    with tempfile.TemporaryDirectory() as tmp:
        for source in sources:
            for name, size in sections(cc, cflags, source, tmp):
                if name == ".bss.~frnes_cold":
                    # FRNES_COLD_BSS (memory.h): cold by declaration, last in .bss
                    continue
                # .bss.tile_colors.8 -> tile_colors (static locals get a number that LTO may change)
                base = re.sub(r"^\.(bss|data)\.", "", name)
                base = re.sub(r"\.\d+$", "", base)
                if base in HOT:
                    found[base] = max(size, found.get(base, 0))
                elif base not in KNOWN_COLD and base not in EXCLUDE and not COLD.match(base):
                    unknown.add("%s (%s)" % (base, source))
    for name in sorted(unknown):
        print("\n*** tools/hot_data.py: %s is a new global, placed OUTSIDE .hotdata: add it to HOT (read every scanline or "
              "instruction) or KNOWN_COLD in tools/hot_data.py ***\n" % name, file=sys.stderr)
    small = sorted((n for n in found if found[n] <= SMALL and n not in TABLE_ORDER and n not in LAST), key=lambda n: (found[n], n))
    tables = [n for n in TABLE_ORDER if n in found]
    last = [n for n in LAST if n in found]
    rest = sorted((n for n in found if n not in small and n not in tables and n not in last), key=lambda n: (found[n], n))
    lines = ["/* generated by tools/hot_data.py from %s */" % " ".join(sources)]
    for i, n in enumerate(small + tables + rest + last):
        if i == len(small):
            lines.append('    ASSERT(. <= __frnes_hot_data_start + %#x, "FrNES hot data: the small state outgrew TABLES_AT (tools/hot_data.py)");' % TABLES_AT)
            # (ld stops here first, "cannot move location counter backwards", when the small state outgrew TABLES_AT)
            lines.append("    . = __frnes_hot_data_start + %#x;  /* TABLES_AT (tools/hot_data.py): the tables' fixed offset */" % TABLES_AT)
        pats = " ".join(".%s.%s .%s.%s.*" % (kind, n, kind, n) for kind in ("data", "bss"))
        lines.append("    *(%s)  /* %d */" % (pats, found[n]))
    text = "\n".join(lines) + "\n"
    if not os.path.exists(out) or open(out).read() != text:
        with open(out, "w") as f:
            f.write(text)
    else:
        os.utime(out)


if __name__ == "__main__":
    main()
