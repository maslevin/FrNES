/* tools/simplay/data_ld.py: 325 data units at their offsets modulo 16 kB (cd), each with the sections that share its lines */
. = ALIGN(. - 0x134, 0x4000) + 0x134;
*(.bss.cpu_irq_raise_cycle)
. = ALIGN(. - 0x15c, 0x4000) + 0x15c;
*(.bss.vrc4_prg.lto_priv.0)
. = ALIGN(. - 0x174, 0x4000) + 0x174;
*(.bss.rambo_registers.lto_priv.0)
. = ALIGN(. - 0x27b, 0x4000) + 0x27b;
*(.bss.mmc3_irq_reload.lto_priv.0)
. = ALIGN(. - 0x2a4, 0x4000) + 0x2a4;
*(.bss.n163_span_length.lto_priv.0)
. = ALIGN(. - 0x2a8, 0x4000) + 0x2a8;
*(.bss.ppu_nmi_dots_after)
. = ALIGN(. - 0x300, 0x4000) + 0x300;
*(.bss.line_start.lto_priv.0)
. = ALIGN(. - 0x36d, 0x4000) + 0x36d;
*(.bss.mapper_fetch_hooks)
. = ALIGN(. - 0x380, 0x4000) + 0x380;
*(.bss.n163_span_pos.lto_priv.0)
. = . + 0x8;
*sp_stubs.o(.bss.text.0)
*sp_stubs.o(.bss.renames_count)
*sp_stubs.o(.bss.renames_read)
*sp_stubs.o(.bss.renames)
*sp_stubs.o(.bss.gate_windows)
*sp_stubs.o(.bss.gate_read)
*sp_stubs.o(.bss.gate_scenes)
. = ALIGN(. - 0x13d4, 0x4000) + 0x13d4;
*libc.a:libc_a-impure.o(.data._impure_data)
. = ALIGN(. - 0x15e8, 0x4000) + 0x15e8;
*(.data.oam2_off_line)
. = ALIGN(. - 0x15f8, 0x4000) + 0x15f8;
*(.data.hud_mode)
. = ALIGN(. - 0x1614, 0x4000) + 0x1614;
*(.bss.vrc4_chr.lto_priv.0)
. = ALIGN(. - 0x1674, 0x4000) + 0x1674;
*(.bss.bars_texture)
. = ALIGN(. - 0x167c, 0x4000) + 0x167c;
*(.bss.sequencer_counter.lto_priv.0)
. = ALIGN(. - 0x1764, 0x4000) + 0x1764;
*(.bss.cpu_cycle_total)
. = ALIGN(. - 0x1790, 0x4000) + 0x1790;
*(.bss.apu_cycles_to_event)
. = ALIGN(. - 0x17b8, 0x4000) + 0x17b8;
*(.bss.mapper_irq_armed)
. = ALIGN(. - 0x181f, 0x4000) + 0x181f;
*(.bss.rambo_prescaler.lto_priv.0)
. = ALIGN(. - 0x18b0, 0x4000) + 0x18b0;
*(.bss.ppu_blank_filled_dot.lto_priv.0)
. = ALIGN(. - 0x1924, 0x4000) + 0x1924;
*(.bss.ppu_nametable_generation)
. = ALIGN(. - 0x194c, 0x4000) + 0x194c;
*(.bss.sprite_y.lto_priv.0)
. = ALIGN(. - 0x1960, 0x4000) + 0x1960;
*(.bss.cpu_registers)
. = ALIGN(. - 0x197c, 0x4000) + 0x197c;
*(.data.mmc3_mirroring.lto_priv.0)
. = ALIGN(. - 0x199b, 0x4000) + 0x199b;
*(.bss.cpu_batch_cut)
. = ALIGN(. - 0x19a8, 0x4000) + 0x19a8;
*(.bss.apu_frame_counter_cycles)
. = ALIGN(. - 0x1a4c, 0x4000) + 0x1a4c;
*(.bss.cpu_instruction_end)
. = ALIGN(. - 0x1a60, 0x4000) + 0x1a60;
*(.bss.sprite_line)
. = ALIGN(. - 0x1ba4, 0x4000) + 0x1ba4;
*(.bss.vrc7_span_pos.lto_priv.0)
. = ALIGN(. - 0x1bc0, 0x4000) + 0x1bc0;
*(.bss.ring.lto_priv.0)
. = ALIGN(. - 0x398, 0x4000) + 0x398;
*(.bss.prg_ram_disabled.lto_priv.0)
. = ALIGN(. - 0x3dc, 0x4000) + 0x3dc;
*(.data.n163.lto_priv.0)
*sp_stubs.o(.data.gate_stride)
*sp_stubs.o(.data.gate_frames)
*sp_stubs.o(.data.gate_from)
*sp_stubs.o(.data.vid_mode)
*sp_stubs.o(.data.vram_next)
. = ALIGN(. - 0x620, 0x4000) + 0x620;
*(.bss.num_1k_VRAM_pages)
. = . + 0x4;
*(.bss.num_VROM_bytes_allocated)
*(.bss.cartridge_VRAM)
*(.bss.VROM)
*(.bss.ROM)
*(.bss.cartridge_submapper)
. = ALIGN(. - 0x660, 0x4000) + 0x660;
*(.bss.secondary)
. = ALIGN(. - 0x68c, 0x4000) + 0x68c;
*(.bss.mapper_ext_attributes)
. = ALIGN(. - 0x6a0, 0x4000) + 0x6a0;
*(.bss.color_line)
. = ALIGN(. - 0x7cc, 0x4000) + 0x7cc;
*(.bss.video_vblank_waiting.lto_priv.0)
. = ALIGN(. - 0x7f0, 0x4000) + 0x7f0;
*(.bss.nametable_slot_page.lto_priv.0)
. = ALIGN(. - 0x804, 0x4000) + 0x804;
*(.bss.num_1k_VROM_pages)
. = ALIGN(. - 0x821, 0x4000) + 0x821;
*mapper_boards.o(.bss.n163_channel)
. = ALIGN(. - 0x836, 0x4000) + 0x836;
*(.bss.ex_y)
. = ALIGN(. - 0x840, 0x4000) + 0x840;
*(.bss.mmc5_exram_mode.lto_priv.0)
*(.bss.mmc5_fill_attribute.lto_priv.0)
*(.bss.mmc5_fill_tile.lto_priv.0)
*(.bss.mmc5_in_frame.lto_priv.0)
*(.bss.mmc5_irq_compare.lto_priv.0)
*(.bss.mmc5_irq_enabled.lto_priv.0)
*(.bss.mmc5_irq_pending.lto_priv.0)
*(.bss.mmc5_multiplicand.lto_priv.0)
*(.bss.mmc5_multiplier.lto_priv.0)
*(.bss.mmc5_nametables.lto_priv.0)
*(.bss.mmc5_prg_mode.lto_priv.0)
*(.bss.mmc5_scanline.lto_priv.0)
. = . + 0x1;
*(.bss.mmc6_a001.lto_priv.0)
*(.bss.namco108_select.lto_priv.0)
. = ALIGN(. - 0x850, 0x4000) + 0x850;
*(.bss.oam_bucket)
. = ALIGN(. - 0xfe7, 0x4000) + 0xfe7;
*(.bss.mapper_6000_mode)
. = ALIGN(. - 0x103a, 0x4000) + 0x103a;
*(.bss.ex_pc)
. = ALIGN(. - 0x1069, 0x4000) + 0x1069;
*(.bss.oam_unit.lto_priv.0)
. = ALIGN(. - 0x10b0, 0x4000) + 0x10b0;
*libc.a:libc_a-findfp.o(.bss.__sf)
. = ALIGN(. - 0x121c, 0x4000) + 0x121c;
*(.bss.apu_length_new_halt)
. = ALIGN(. - 0x1226, 0x4000) + 0x1226;
*(.bss.ex_nmi_late)
. = ALIGN(. - 0x1250, 0x4000) + 0x1250;
*(.bss.nametable_pages_valid.lto_priv.0)
. = ALIGN(. - 0x1263, 0x4000) + 0x1263;
*(.bss.sequencer_step.lto_priv.0)
. = ALIGN(. - 0x1273, 0x4000) + 0x1273;
*(.bss.ppu_copy_buffer)
. = ALIGN(. - 0x12fa, 0x4000) + 0x12fa;
*(.bss.prg_ram_written)
. = ALIGN(. - 0x1301, 0x4000) + 0x1301;
*(.bss.apu_delays_pending)
. = ALIGN(. - 0x132a, 0x4000) + 0x132a;
*(.bss.apu_frame_irq_flag)
. = ALIGN(. - 0x1350, 0x4000) + 0x1350;
*(.bss.ppu_pattern_offsets.lto_priv.0)
. = ALIGN(. - 0x1a88, 0x4000) + 0x1a88;
*(.bss.mmc2_chr.lto_priv.0)
. = ALIGN(. - 0x1ac4, 0x4000) + 0x1ac4;
*(.data.cycles_per_sample_fp.lto_priv.0)
. = ALIGN(. - 0x1ad9, 0x4000) + 0x1ad9;
*(.bss.ppu_vbl_suppress.lto_priv.0)
. = ALIGN(. - 0x1aec, 0x4000) + 0x1aec;
*(.bss.dc_previous_out.lto_priv.0)
. = ALIGN(. - 0x1c36, 0x4000) + 0x1c36;
*(.bss.ppu_nmi_raised)
. = ALIGN(. - 0x1c4c, 0x4000) + 0x1c4c;
*(.data.mapper_write_fn)
. = ALIGN(. - 0x1c74, 0x4000) + 0x1c74;
*(.bss.apu_dmc_disable_delay)
. = ALIGN(. - 0x1c8e, 0x4000) + 0x1c8e;
*(.bss.mapper_irq_flag)
. = ALIGN(. - 0x1ca5, 0x4000) + 0x1ca5;
*(.bss.lazy_line_rendered.lto_priv.0)
. = ALIGN(. - 0x1cdc, 0x4000) + 0x1cdc;
*(.bss.ppu_output_sq.lto_priv.0)
. = ALIGN(. - 0x1ce4, 0x4000) + 0x1ce4;
*(.bss.ppu_registers)
. = ALIGN(. - 0x1dc6, 0x4000) + 0x1dc6;
*(.bss.oam_unit_step.lto_priv.0)
. = ALIGN(. - 0x1df4, 0x4000) + 0x1df4;
*(.bss.video_frames_submitted)
. = ALIGN(. - 0x1e22, 0x4000) + 0x1e22;
*(.bss.bg_shift_pattern_high.lto_priv.0)
. = ALIGN(. - 0x1e5c, 0x4000) + 0x1e5c;
*(.bss.row_pattern)
. = ALIGN(. - 0x1f1a, 0x4000) + 0x1f1a;
*(.bss.mmc3_irq_latch.lto_priv.0)
. = ALIGN(. - 0x1f34, 0x4000) + 0x1f34;
*(.bss.frame_open.lto_priv.0)
. = ALIGN(. - 0x1f49, 0x4000) + 0x1f49;
*(.bss.apu_frame_counter_mode)
. = ALIGN(. - 0x1f65, 0x4000) + 0x1f65;
*(.bss.apu_dmc_irq_flag)
. = ALIGN(. - 0x1f9c, 0x4000) + 0x1f9c;
*(.bss.ppu_mapper_clocked.lto_priv.0)
. = ALIGN(. - 0x1fa0, 0x4000) + 0x1fa0;
*(.bss.ppu_dot.lto_priv.0)
. = ALIGN(. - 0x1fc0, 0x4000) + 0x1fc0;
*(.bss.cpu_cycle_now)
. = ALIGN(. - 0x1fe3, 0x4000) + 0x1fe3;
*(.bss.fme7_command.lto_priv.0)
. = ALIGN(. - 0x2058, 0x4000) + 0x2058;
*(.bss.expansion_samples.lto_priv.0)
. = ALIGN(. - 0x2078, 0x4000) + 0x2078;
*(.bss.ex_a.lto_priv.0)
. = ALIGN(. - 0x209f, 0x4000) + 0x209f;
*(.bss.cpu_io_bus_value)
. = ALIGN(. - 0x20e4, 0x4000) + 0x20e4;
*(.bss.mmc1_last_write_cycle.lto_priv.0)
. = ALIGN(. - 0x20e8, 0x4000) + 0x20e8;
*(.bss.ex_prev_need_nmi)
. = ALIGN(. - 0x20f0, 0x4000) + 0x20f0;
*frame_hud_perf.o(.bss.~frnes_cold_hud)
. = ALIGN(. - 0x213c, 0x4000) + 0x213c;
*(.bss.apu_dmc_start_delay)
. = ALIGN(. - 0x215c, 0x4000) + 0x215c;
*(.bss.cpu_dma_need_halt)
. = ALIGN(. - 0x2164, 0x4000) + 0x2164;
*(.bss.oam_unit_page.lto_priv.0)
. = ALIGN(. - 0x2192, 0x4000) + 0x2192;
*(.bss.apu_dmc_rate)
. = ALIGN(. - 0x21a8, 0x4000) + 0x21a8;
*(.bss.cpu_devices_synced_cycle)
. = ALIGN(. - 0x21cf, 0x4000) + 0x21cf;
*(.bss.apu_length_pending)
. = ALIGN(. - 0x2260, 0x4000) + 0x2260;
*(.bss.oam_bucket_count)
. = ALIGN(. - 0x237c, 0x4000) + 0x237c;
*(.bss.rambo_cycle_mode.lto_priv.0)
. = ALIGN(. - 0x2404, 0x4000) + 0x2404;
*(.bss.apu_frame_block_time)
. = ALIGN(. - 0x24a4, 0x4000) + 0x24a4;
*(.bss.sprite_count.lto_priv.0)
. = ALIGN(. - 0x24d4, 0x4000) + 0x24d4;
*(.bss.ppu_oam_generation)
. = ALIGN(. - 0x24ec, 0x4000) + 0x24ec;
*(.bss.ppu_scanline.lto_priv.0)
. = ALIGN(. - 0x2500, 0x4000) + 0x2500;
*(.bss.vrc7_eg_tick.lto_priv.0)
. = ALIGN(. - 0x2504, 0x4000) + 0x2504;
*(.bss.sprite_attribute.lto_priv.0)
. = ALIGN(. - 0x25a5, 0x4000) + 0x25a5;
*(.bss.sprite_zero_on_line.lto_priv.0)
. = ALIGN(. - 0x25e4, 0x4000) + 0x25e4;
*(.bss.sprite_x.lto_priv.0)
. = ALIGN(. - 0x2620, 0x4000) + 0x2620;
*(.data.ppu_sprite_x_origin.lto_priv.0)
*(.bss.register_buffer)
. = ALIGN(. - 0x2650, 0x4000) + 0x2650;
*(.bss.next_sprite_zero_on_line.lto_priv.0)
. = ALIGN(. - 0x267c, 0x4000) + 0x267c;
*(.bss.bg_next_table.lto_priv.0)
. = ALIGN(. - 0x26b4, 0x4000) + 0x26b4;
*(.bss.ppu_line_buffer_used.lto_priv.0)
. = ALIGN(. - 0x26dc, 0x4000) + 0x26dc;
*(.data.ppu_sprite_x_line.lto_priv.0)
. = ALIGN(. - 0x26e0, 0x4000) + 0x26e0;
*(.bss.tile_colors.5)
. = ALIGN(. - 0x36e0, 0x4000) + 0x36e0;
*(.bss.eeprom.lto_priv.0)
. = . + 0x2;
*(.bss.mmc6_ram.lto_priv.0)
. = ALIGN(. - 0x3c00, 0x4000) + 0x3c00;
*(.frnes_hot_rodata)
. = ALIGN(. - 0x3c40, 0x4000) + 0x3c40;
*6502_core.o(.frnes_hot_rodata)
. = ALIGN(. - 0x3ec, 0x4000) + 0x3ec;
*(.bss.ppu_oam2_row)
. = ALIGN(. - 0x430, 0x4000) + 0x430;
*(.bss.apu_frame_write_cycles)
. = ALIGN(. - 0x440, 0x4000) + 0x440;
*(.bss.n163_address.lto_priv.0)
. = ALIGN(. - 0x45e, 0x4000) + 0x45e;
*(.bss.rambo_latch.lto_priv.0)
. = ALIGN(. - 0x480, 0x4000) + 0x480;
*(.bss.utlb_shadow_ptel.lto_priv.0)
. = ALIGN(. - 0x5e4, 0x4000) + 0x5e4;
*(.bss.s5b_volume.lto_priv.0)
. = ALIGN(. - 0x624, 0x4000) + 0x624;
*(.bss.expansion_ticks.lto_priv.0)
. = ALIGN(. - 0x790, 0x4000) + 0x790;
*(.bss.hud_ring)
. = ALIGN(. - 0x924, 0x4000) + 0x924;
*(.bss.fme7_irq_control.lto_priv.0)
. = ALIGN(. - 0x940, 0x4000) + 0x940;
*(.bss.cartridge_mapper_number)
. = ALIGN(. - 0x948, 0x4000) + 0x948;
*(.bss.dmc_unit_stopped.lto_priv.0)
. = ALIGN(. - 0x978, 0x4000) + 0x978;
*(.bss.apu_dmc_request_time)
. = ALIGN(. - 0x988, 0x4000) + 0x988;
*(.bss.WRAM)
. = ALIGN(. - 0x990, 0x4000) + 0x990;
*(.bss.ppu_frame_count)
. = ALIGN(. - 0x9a8, 0x4000) + 0x9a8;
*(.bss.dc_previous_in.lto_priv.0)
. = ALIGN(. - 0x9b4, 0x4000) + 0x9b4;
*(.bss.ppu_frame_ready.lto_priv.0)
. = ALIGN(. - 0x9c0, 0x4000) + 0x9c0;
*(.bss.ppu_memory_buffer)
. = ALIGN(. - 0x9d0, 0x4000) + 0x9d0;
*(.bss.ppu_bit_spread.lto_priv.0)
. = ALIGN(. - 0x11d8, 0x4000) + 0x11d8;
*(.bss.cpu_nmi_raise_cycle)
. = ALIGN(. - 0x1440, 0x4000) + 0x1440;
*(.bss.ppu_line_buffer.lto_priv.0)
. = ALIGN(. - 0x1f36, 0x4000) + 0x1f36;
*(.bss.mmc3_a12.lto_priv.0)
. = ALIGN(. - 0x20f4, 0x4000) + 0x20f4;
*(.bss.video_rate_start_us.lto_priv.0)
. = ALIGN(. - 0x210e, 0x4000) + 0x210e;
*(.bss.apu_dmc_address)
. = ALIGN(. - 0x2110, 0x4000) + 0x2110;
*(.bss.ppu_oam_corrupt_rows)
. = ALIGN(. - 0x2120, 0x4000) + 0x2120;
*(.bss.video_translucent_overlay)
. = . + 0x4;
*(.data.video_vblank_handle)
. = . + 0x8;
*(.bss.mmc5_prg.lto_priv.0)
. = . + 0x3;
*(.bss.bandai_chr.lto_priv.0)
. = . + 0x18;
*(.bss.namco108_registers.lto_priv.0)
. = ALIGN(. - 0x2164, 0x4000) + 0x2164;
*(.bss.apu_length_previous)
. = ALIGN(. - 0x2274, 0x4000) + 0x2274;
*(.bss.cached_dot.0)
. = ALIGN(. - 0x2282, 0x4000) + 0x2282;
*(.bss.sequencer_five_step.lto_priv.0)
. = ALIGN(. - 0x22d0, 0x4000) + 0x22d0;
*(.bss.ring_head)
. = ALIGN(. - 0x26f0, 0x4000) + 0x26f0;
*(.bss.mmc1_chr0.lto_priv.0)
. = ALIGN(. - 0x270b, 0x4000) + 0x270b;
*(.bss.vrc4_swap.lto_priv.0)
. = ALIGN(. - 0x2850, 0x4000) + 0x2850;
*(.bss.log_tail.lto_priv.0)
. = ALIGN(. - 0x2860, 0x4000) + 0x2860;
*(.bss.oam2_off_dot)
. = ALIGN(. - 0x28d5, 0x4000) + 0x28d5;
*(.bss.ex_sp.lto_priv.0)
. = ALIGN(. - 0x2928, 0x4000) + 0x2928;
*(.bss.ppu_evaluate_end_secondary)
. = ALIGN(. - 0x2930, 0x4000) + 0x2930;
*(.bss.VROM_MMU_HANDLES)
*(.bss.WRAM_MMU_HANDLES)
*(.bss.currentCRC32)
. = ALIGN(. - 0x296d, 0x4000) + 0x296d;
*(.bss.apu_frame_step)
. = ALIGN(. - 0x29b0, 0x4000) + 0x29b0;
*(.data.cpu_io_bus_cycle)
. = ALIGN(. - 0x2ad4, 0x4000) + 0x2ad4;
*(.bss.pulse.lto_priv.0)
*(.bss.mmc5.lto_priv.0)
. = ALIGN(. - 0x2b78, 0x4000) + 0x2b78;
*(.bss.key_frame.2)
. = ALIGN(. - 0x2b8c, 0x4000) + 0x2b8c;
*(.bss.apu_frame_irq_visible)
. = ALIGN(. - 0x2bac, 0x4000) + 0x2bac;
*(.bss.ring_write.lto_priv.0)
. = ALIGN(. - 0x2bc4, 0x4000) + 0x2bc4;
*(.bss.vrc7.lto_priv.0)
. = . + 0x1;
*(.bss.vrc2_latch.lto_priv.0)
*(.bss.vrc2_latch_mode.lto_priv.0)
. = ALIGN(. - 0x2f62, 0x4000) + 0x2f62;
*(.bss.expansion.lto_priv.0)
. = ALIGN(. - 0x2f74, 0x4000) + 0x2f74;
*(.bss.mapper_clock_dot)
. = ALIGN(. - 0x2f86, 0x4000) + 0x2f86;
*mapper_boards.o(.bss.board_own_irqs)
. = ALIGN(. - 0x2f8b, 0x4000) + 0x2f8b;
*(.bss.apu_frame_irq_read_pending)
. = ALIGN(. - 0x2f9c, 0x4000) + 0x2f9c;
*(.bss.key_generation.3)
. = ALIGN(. - 0x2fa0, 0x4000) + 0x2fa0;
*(.bss.apu_length_pending_time)
. = ALIGN(. - 0x2fb8, 0x4000) + 0x2fb8;
*(.bss.ppu_bus_frame_count)
. = ALIGN(. - 0x2fe0, 0x4000) + 0x2fe0;
*(.bss.rambo_reload.lto_priv.0)
. = . + 0x7;
*(.bss.sunsoft3_toggle.lto_priv.0)
*(.bss.sunsoft4_control.lto_priv.0)
*(.bss.vrc3_control.lto_priv.0)
. = . + 0xd;
*(.bss.bandai_counter.lto_priv.0)
*(.bss.bandai_latch.lto_priv.0)
. = ALIGN(. - 0x3014, 0x4000) + 0x3014;
*(.bss.expansion_timed.lto_priv.0)
. = ALIGN(. - 0x301c, 0x4000) + 0x301c;
*(.bss.save_indicator_active)
. = ALIGN(. - 0x3021, 0x4000) + 0x3021;
*(.bss.rambo_select.lto_priv.0)
. = ALIGN(. - 0x3066, 0x4000) + 0x3066;
*(.bss.lazy_x_switches.lto_priv.0)
. = ALIGN(. - 0x3097, 0x4000) + 0x3097;
*(.bss.bg_next_low.lto_priv.0)
. = ALIGN(. - 0x3157, 0x4000) + 0x3157;
*(.bss.ex_x.lto_priv.0)
. = ALIGN(. - 0x3178, 0x4000) + 0x3178;
*(.bss.ppu_sprite_fetched.lto_priv.0)
. = ALIGN(. - 0x3180, 0x4000) + 0x3180;
*(.bss.colors)
. = ALIGN(. - 0x31b8, 0x4000) + 0x31b8;
*(.bss.apu_length_halt)
. = ALIGN(. - 0x31fd, 0x4000) + 0x31fd;
*(.bss.rambo_enabled.lto_priv.0)
. = ALIGN(. - 0x3200, 0x4000) + 0x3200;
*(.bss.utlb_shadow_pteh.lto_priv.0)
. = ALIGN(. - 0x3347, 0x4000) + 0x3347;
*(.bss.strobe_saved_bit2.0)
. = ALIGN(. - 0x3354, 0x4000) + 0x3354;
*(.bss.mmc3_registers.lto_priv.0)
. = ALIGN(. - 0x3374, 0x4000) + 0x3374;
*(.bss.mmc1_prg.lto_priv.0)
. = ALIGN(. - 0x33d0, 0x4000) + 0x33d0;
*(.bss.apu_synth_tap)
. = ALIGN(. - 0x33db, 0x4000) + 0x33db;
*(.bss.rambo_counter.lto_priv.0)
. = ALIGN(. - 0x3444, 0x4000) + 0x3444;
*(.bss.ppu_evaluate_end_oamaddr)
. = ALIGN(. - 0x3478, 0x4000) + 0x3478;
*(.bss.sample_fraction.lto_priv.0)
. = ALIGN(. - 0x34b5, 0x4000) + 0x34b5;
*(.bss.mmc1_shift.lto_priv.0)
. = ALIGN(. - 0x34c0, 0x4000) + 0x34c0;
*(.bss.cpu_irq_raise_recent)
. = . + 0x10;
*(.bss.mmc1_chr1.lto_priv.0)
. = . + 0xb;
*(.bss.mmc5_chr_last_b.lto_priv.0)
*(.bss.mmc5_chr_mode.lto_priv.0)
*(.bss.mmc5_chr_upper.lto_priv.0)
. = ALIGN(. - 0x34e0, 0x4000) + 0x34e0;
*(.bss.record_watchdog_frame)
. = ALIGN(. - 0x3504, 0x4000) + 0x3504;
*(.bss.lazy_x_pixel)
. = ALIGN(. - 0x3535, 0x4000) + 0x3535;
*(.bss.bg_next_attribute.lto_priv.0)
. = ALIGN(. - 0x355c, 0x4000) + 0x355c;
*(.bss.mapper_number.lto_priv.0)
. = ALIGN(. - 0x3578, 0x4000) + 0x3578;
*(.bss.ppu_running.lto_priv.0)
. = ALIGN(. - 0x35a8, 0x4000) + 0x35a8;
*(.bss.rambo_a12_low_cycle.lto_priv.0)
. = ALIGN(. - 0x35ac, 0x4000) + 0x35ac;
*(.bss.mmc3_a12_low_cycle.lto_priv.0)
. = ALIGN(. - 0x35c9, 0x4000) + 0x35c9;
*(.bss.mapper_chr_latch)
. = ALIGN(. - 0x35d7, 0x4000) + 0x35d7;
*(.bss.mmc3_bank_select.lto_priv.0)
. = ALIGN(. - 0x35f4, 0x4000) + 0x35f4;
*(.bss.num_8k_ROM_pages)
. = ALIGN(. - 0x360c, 0x4000) + 0x360c;
*(.bss.textures.lto_priv.0)
. = ALIGN(. - 0x3612, 0x4000) + 0x3612;
*(.bss.ppu_chr_alias_slots)
. = ALIGN(. - 0x3616, 0x4000) + 0x3616;
*(.bss.apu_dmc_sample_length)
. = ALIGN(. - 0x361a, 0x4000) + 0x361a;
*(.bss.n163_counter.lto_priv.0)
. = ALIGN(. - 0x3620, 0x4000) + 0x3620;
*(.bss.ppu_pending_dots.lto_priv.0)
. = ALIGN(. - 0x3658, 0x4000) + 0x3658;
*(.bss.bg_next_tile.lto_priv.0)
. = ALIGN(. - 0x366c, 0x4000) + 0x366c;
*(.bss.rambo_a12.lto_priv.0)
. = ALIGN(. - 0x36fd, 0x4000) + 0x36fd;
*(.bss.cpu_batch_resync)
. = ALIGN(. - 0x371c, 0x4000) + 0x371c;
*(.bss.vrc_irq.lto_priv.0)
*(.bss.mmc5_slot_ram)
. = ALIGN(. - 0x375c, 0x4000) + 0x375c;
*(.bss.ROMBANK_MMU_HANDLES)
*(.bss.draw_count)
. = ALIGN(. - 0x3794, 0x4000) + 0x3794;
*(.data.row_key)
. = ALIGN(. - 0x37f0, 0x4000) + 0x37f0;
*libc.a:libc_a-impure.o(.data._impure_ptr)
. = ALIGN(. - 0x381c, 0x4000) + 0x381c;
*(.bss.cpu_batch_mapper_only)
. = ALIGN(. - 0x382c, 0x4000) + 0x382c;
*(.bss.pulse_level.lto_priv.0)
. = ALIGN(. - 0x3838, 0x4000) + 0x3838;
*(.data.ppu_output.lto_priv.0)
. = ALIGN(. - 0x385c, 0x4000) + 0x385c;
*(.bss.sprite_tile.lto_priv.0)
. = ALIGN(. - 0x387c, 0x4000) + 0x387c;
*(.bss.VRAM_pages)
*(.bss.num_8k_WRAM_pages)
. = ALIGN(. - 0x3910, 0x4000) + 0x3910;
*(.bss.ppu_scroll_resume_dot)
. = ALIGN(. - 0x3924, 0x4000) + 0x3924;
*(.bss.bg_shift_pattern_low.lto_priv.0)
. = ALIGN(. - 0x392c, 0x4000) + 0x392c;
*(.bss.apu_synth_muted)
. = ALIGN(. - 0x3940, 0x4000) + 0x3940;
*(.data.message_lines)
*(.data.vmu_screen_saving)
. = . + 0x7;
*(.data.battery_backed_cache)
. = ALIGN(. - 0x3f97, 0x4000) + 0x3f97;
*(.data.ppu_palette_dirty)
. = ALIGN(. - 0x3fb0, 0x4000) + 0x3fb0;
*(.bss.row_generation)
. = ALIGN(. - 0x4a4, 0x4000) + 0x4a4;
*(.bss.VRAM)
. = ALIGN(. - 0x4ef, 0x4000) + 0x4ef;
*(.bss.mapper_sprite_chr_split)
. = ALIGN(. - 0x540, 0x4000) + 0x540;
*(.bss.opaque_line)
. = ALIGN(. - 0x7a0, 0x4000) + 0x7a0;
*(.bss.chunk.0)
. = ALIGN(. - 0x7ac, 0x4000) + 0x7ac;
*(.bss.text_frames)
. = ALIGN(. - 0x874, 0x4000) + 0x874;
*(.bss.ppu_bus_address_hook)
. = ALIGN(. - 0x8a0, 0x4000) + 0x8a0;
*(.bss.RAM)
. = . + 0x8;
*(.bss.WRAM_size)
. = ALIGN(. - 0x8b4, 0x4000) + 0x8b4;
*(.bss.apu_dmc_sample_address)
. = ALIGN(. - 0x8c8, 0x4000) + 0x8c8;
*mapper_boards.o(.bss.board_low)
. = ALIGN(. - 0x8cc, 0x4000) + 0x8cc;
*(.bss.lazy_x_value)
. = ALIGN(. - 0x9dc, 0x4000) + 0x9dc;
*(.bss.cpu_nmi_raise_dots)
. = ALIGN(. - 0xa04, 0x4000) + 0xa04;
*(.bss.NesHeader)
. = ALIGN(. - 0xa1c, 0x4000) + 0xa1c;
*(.bss.~frnes_cold)
*mapper_boards.o(.bss.~frnes_cold)
*fds.o(.bss.~frnes_cold)
. = ALIGN(. - 0x3268, 0x4000) + 0x3268;
*(.bss.apu_length_reload)
. = ALIGN(. - 0x3284, 0x4000) + 0x3284;
*(.bss.n163_ram.lto_priv.0)
. = . + 0xc;
*(.bss.tc0690_a12_low_cycle.lto_priv.0)
*(.bss.tc0690_a12.lto_priv.0)
*(.bss.tc0690_enabled.lto_priv.0)
*(.bss.tc0690_latch.lto_priv.0)
*(.bss.tc0690_reload.lto_priv.0)
*(.bss.tc0690_counter.lto_priv.0)
*(.bss.tc0690_jetsons.lto_priv.0)
. = . + 0x2;
*(.bss.tc0690_delay.lto_priv.0)
. = ALIGN(. - 0x34df, 0x4000) + 0x34df;
*(.bss.n163_write_protect.lto_priv.0)
. = ALIGN(. - 0x38ac, 0x4000) + 0x38ac;
*(.data.oam_bucket_key)
. = ALIGN(. - 0x38cc, 0x4000) + 0x38cc;
*(.bss.apu_frame_irq_read_until)
. = ALIGN(. - 0x38d6, 0x4000) + 0x38d6;
*(.bss.bg_next_high.lto_priv.0)
. = ALIGN(. - 0x38fe, 0x4000) + 0x38fe;
*(.bss.bg_shift_attribute_high.lto_priv.0)
. = ALIGN(. - 0x3911, 0x4000) + 0x3911;
*(.bss.noise_level.lto_priv.0)
. = ALIGN(. - 0x3954, 0x4000) + 0x3954;
*(.data.ppu_sprite_fetch_line.lto_priv.0)
. = ALIGN(. - 0x3974, 0x4000) + 0x3974;
*(.bss.sprite_pattern_low.lto_priv.0)
. = ALIGN(. - 0x398f, 0x4000) + 0x398f;
*(.bss.next_sprite_count.lto_priv.0)
. = ALIGN(. - 0x399c, 0x4000) + 0x399c;
*(.data.ppu_data_read_cycle.0)
. = ALIGN(. - 0x39b8, 0x4000) + 0x39b8;
*(.bss.mmc3_irq_counter.lto_priv.0)
. = ALIGN(. - 0x3adc, 0x4000) + 0x3adc;
*(.bss.message_texture)
. = ALIGN(. - 0x3b80, 0x4000) + 0x3b80;
*(.bss.bg_shift_attribute_low.lto_priv.0)
. = . + 0xe;
*(.bss.sunsoft4_nt.lto_priv.0)
. = . + 0x2;
*(.bss.vrc1_chr.lto_priv.0)
*(.bss.vrc3_counter.lto_priv.0)
*(.bss.vrc3_latch.lto_priv.0)
. = ALIGN(. - 0x3bcc, 0x4000) + 0x3bcc;
*(.bss.sprite_pattern_high.lto_priv.0)
. = ALIGN(. - 0x3c34, 0x4000) + 0x3c34;
*(.bss.s5b.lto_priv.0)
. = ALIGN(. - 0x3ca0, 0x4000) + 0x3ca0;
*(.bss.vrc6.lto_priv.0)
. = ALIGN(. - 0x3cc8, 0x4000) + 0x3cc8;
*(.bss.output_options.lto_priv.0)
. = ALIGN(. - 0x3d40, 0x4000) + 0x3d40;
*(.bss.opll_block.lto_priv.0)
. = ALIGN(. - 0xa26, 0x4000) + 0xa26;
*(.bss.apu_dmc_loop)
. = ALIGN(. - 0xa84, 0x4000) + 0xa84;
*(.bss.noise.lto_priv.0)
. = ALIGN(. - 0xaee, 0x4000) + 0xaee;
*(.bss.apu_frame_write_value)
. = ALIGN(. - 0xb58, 0x4000) + 0xb58;
*(.bss.video_picture)
*(.bss.frnes_settings)
*(.bss.num_VRAM_bytes_allocated)
. = . + 0x4;
*(.bss.table.1.lto_priv.0)
*(.bss.table_built.2.lto_priv.0)
. = ALIGN(. - 0xfbd, 0x4000) + 0xfbd;
*(.bss.cpu_exact_polled)
. = ALIGN(. - 0x10f0, 0x4000) + 0x10f0;
*(.bss.video_crumb_frame)
. = ALIGN(. - 0x1104, 0x4000) + 0x1104;
*(.bss.n163_nametable.lto_priv.0)
. = ALIGN(. - 0x1118, 0x4000) + 0x1118;
*(.bss.text_texture)
. = ALIGN(. - 0x1150, 0x4000) + 0x1150;
*(.data.finished_index.lto_priv.0)
. = ALIGN(. - 0x118a, 0x4000) + 0x118a;
*(.bss.dmc_unit.lto_priv.0)
. = ALIGN(. - 0x1464, 0x4000) + 0x1464;
*(.bss.apu_dmc_irq_enable)
. = ALIGN(. - 0x14a4, 0x4000) + 0x14a4;
*(.bss.key_setup.1)
. = ALIGN(. - 0x2928, 0x4000) + 0x2928;
*(.bss.mmc2_latch.lto_priv.0)
. = ALIGN(. - 0x2953, 0x4000) + 0x2953;
*(.bss.mmc1_count.lto_priv.0)
. = ALIGN(. - 0x2b44, 0x4000) + 0x2b44;
*mapper_boards.o(.bss.n163_cycle)
. = ALIGN(. - 0x2be8, 0x4000) + 0x2be8;
*(.bss.ring_read.lto_priv.0)
. = ALIGN(. - 0x2c07, 0x4000) + 0x2c07;
*(.bss.apu_dmc_request_due)
. = ALIGN(. - 0x2c20, 0x4000) + 0x2c20;
*(.bss.apu_dmc_timer)
. = ALIGN(. - 0x2c40, 0x4000) + 0x2c40;
*mapper_boards.o(.bss.n163_halt)
. = . + 0x7;
*mapper_boards.o(.bss.irem77_prg)
*mapper_boards.o(.bss.a53_mirroring)
. = . + 0x2;
*mapper_boards.o(.bss.a53_regs)
*mapper_boards.o(.bss.a53_select)
. = . + 0x1;
*mapper_boards.o(.bss.ss88006_mask)
*mapper_boards.o(.bss.ss88006_counter)
*mapper_boards.o(.bss.ss88006_enabled)
. = . + 0x1;
*mapper_boards.o(.bss.ss88006_reload)
*mapper_boards.o(.bss.ss88006_chr)
*mapper_boards.o(.bss.ss88006_prg)
*mapper_boards.o(.bss.h3001_enabled)
*mapper_boards.o(.bss.h3001_reload)
*mapper_boards.o(.bss.h3001_counter)
*mapper_boards.o(.bss.g101_mode)
*mapper_boards.o(.bss.g101_prg0)
. = . + 0x2;
*mapper_boards.o(.bss.x1_registers)
. = ALIGN(. - 0x2c8a, 0x4000) + 0x2c8a;
*(.bss.mapper_chr_ram_slots)
. = ALIGN(. - 0x2c8c, 0x4000) + 0x2c8c;
*(.bss.ex_next_event)
. = ALIGN(. - 0x2c96, 0x4000) + 0x2c96;
*(.bss.pulse_mix.lto_priv.0)
. = ALIGN(. - 0x2d61, 0x4000) + 0x2d61;
*(.bss.cpu_poll_early)
. = ALIGN(. - 0x2d82, 0x4000) + 0x2d82;
*mapper_boards.o(.bss.n163_count)
. = ALIGN(. - 0x2d90, 0x4000) + 0x2d90;
*(.bss.apu_dmc_bytes_remaining)
. = ALIGN(. - 0x2d9c, 0x4000) + 0x2d9c;
*(.bss.chr_slot_page)
. = ALIGN(. - 0x2dc3, 0x4000) + 0x2dc3;
*(.bss.apu_dmc_buffer_full)
. = ALIGN(. - 0x328c, 0x4000) + 0x328c;
*(.bss.log_head.lto_priv.0)
. = ALIGN(. - 0x32c0, 0x4000) + 0x32c0;
*(.bss.vrc7_span_length.lto_priv.0)
. = . + 0x4;
*(.bss.fds_reciprocal.lto_priv.0)
*(.bss.n163_reciprocal.lto_priv.0)
. = ALIGN(. - 0x3e20, 0x4000) + 0x3e20;
*(.bss.row_table)
. = ALIGN(. - 0x3ec0, 0x4000) + 0x3ec0;
*(.bss.autosave_main_waiting)
*(.bss.autosave_kthread)
*(.bss.vmu_save_yield)
. = ALIGN(. - 0x3ee4, 0x4000) + 0x3ee4;
*(.bss.triangle.lto_priv.0)
. = ALIGN(. - 0x3f34, 0x4000) + 0x3f34;
*(.bss.message_frames)
. = ALIGN(. - 0x3f48, 0x4000) + 0x3f48;
*(.bss.lazy_hit_dot.lto_priv.0)
. = ALIGN(. - 0xb66, 0x4000) + 0xb66;
*(.bss.strobe_saved_bit1.1)
. = ALIGN(. - 0xb68, 0x4000) + 0xb68;
*(.bss.ppu_open_bus_frame)
*(.data.pvr_params)
. = ALIGN(. - 0xbbc, 0x4000) + 0xbbc;
*(.bss.synth_frame_end.lto_priv.0)
. = ALIGN(. - 0xbc4, 0x4000) + 0xbc4;
*(.bss.ex_run_irq)
. = ALIGN(. - 0xbc8, 0x4000) + 0xbc8;
*(.bss.synth_cycle.lto_priv.0)
. = ALIGN(. - 0xbf0, 0x4000) + 0xbf0;
*(.bss.ex_cycle)
. = ALIGN(. - 0xbf8, 0x4000) + 0xbf8;
*(.bss.dmc.lto_priv.0)
*(.bss.mmc5_chr.lto_priv.0)
*(.bss.mmc5_sprite_pages.lto_priv.0)
. = ALIGN(. - 0xc68, 0x4000) + 0xc68;
*(.bss.apu_frame_blocking)
. = ALIGN(. - 0xc80, 0x4000) + 0xc80;
*(.bss.tnd_mix.lto_priv.0)
. = ALIGN(. - 0xe28, 0x4000) + 0xe28;
*(.bss.mapper_6000_reads)
. = ALIGN(. - 0xe48, 0x4000) + 0xe48;
*(.bss.fpu_spans.lto_priv.0)
. = ALIGN(. - 0xe5c, 0x4000) + 0xe5c;
*(.bss.PPU_NAMETABLES_MMU_HANDLES)
*sp_stubs.o(.bss.tlb_nb_static)
. = ALIGN(. - 0xe82, 0x4000) + 0xe82;
*(.bss.apu_dmc_bits_remaining)
. = ALIGN(. - 0xeac, 0x4000) + 0xeac;
*(.bss.mmc6.lto_priv.0)
. = ALIGN(. - 0xedc, 0x4000) + 0xedc;
*(.data.video_pacing)
. = ALIGN(. - 0xf40, 0x4000) + 0xf40;
*(.bss.apu_channel_enabled)
. = . + 0xf;
*(.bss.axrom_bank.lto_priv.0)
*(.bss.bandai_irq_enabled.lto_priv.0)
*(.bss.bandai_outer.lto_priv.0)
*(.bss.bandai_prg.lto_priv.0)
*(.bss.bandai_submapper.lto_priv.0)
. = . + 0x4;
*(.bss.camerica_bank.lto_priv.0)
*(.bss.camerica_block.lto_priv.0)
. = ALIGN(. - 0xf7c, 0x4000) + 0xf7c;
*(.bss.cpu_batch_end)
. = ALIGN(. - 0xf95, 0x4000) + 0xf95;
*(.bss.ppu_line_mode)
. = ALIGN(. - 0x2c44, 0x4000) + 0x2c44;
*(.bss.oam2_off_frame)
. = ALIGN(. - 0x2c58, 0x4000) + 0x2c58;
*(.bss.ppu_mapper_clock_a12_dot)
. = ALIGN(. - 0x2c7e, 0x4000) + 0x2c7e;
*(.bss.cpu_internal_bus_value)
. = ALIGN(. - 0x2cb0, 0x4000) + 0x2cb0;
*(.bss.work_index.lto_priv.0)
. = ALIGN(. - 0xb84, 0x4000) + 0xb84;
*(.bss.video_vblank_count)
. = ALIGN(. - 0xba4, 0x4000) + 0xba4;
*(.bss.strobe_cycle.2)
. = ALIGN(. - 0xc12, 0x4000) + 0xc12;
*(.bss.mmc1_control.lto_priv.0)
. = ALIGN(. - 0xc2c, 0x4000) + 0xc2c;
*(.bss.ppu_evaluate_overflow_dot)
. = ALIGN(. - 0xc34, 0x4000) + 0xc34;
*(.bss.apu_length)
. = ALIGN(. - 0xc85, 0x4000) + 0xc85;
*(.bss.ex_prev_run_irq)
. = ALIGN(. - 0xc90, 0x4000) + 0xc90;
*libc.a:libc_a-locale.o(.data.__global_locale)
. = ALIGN(. - 0xe07, 0x4000) + 0xe07;
*mapper_boards.o(.bss.board_low_unsynced)
. = ALIGN(. - 0xe4b, 0x4000) + 0xe4b;
*(.bss.mapper_cpu_timer)
. = ALIGN(. - 0xe60, 0x4000) + 0xe60;
*(.data.key_scanline.4)
. = ALIGN(. - 0xe78, 0x4000) + 0xe78;
*(.bss.cpu_batch_device_event)
. = ALIGN(. - 0xf40, 0x4000) + 0xf40;
*(.bss.mapper_timer_cycle.lto_priv.0)
. = . + 0x10;
*(.bss.oam2_missed)
*(.bss.oam2_missed_frame)
*(.data.oam2_missed_line)
. = ALIGN(. - 0x2c46, 0x4000) + 0x2c46;
*(.bss.fme7_counter.lto_priv.0)
. = ALIGN(. - 0xba7, 0x4000) + 0xba7;
*(.bss.ex_need_nmi)
. = ALIGN(. - 0xc98, 0x4000) + 0xc98;
*(.bss.message_length)
. = ALIGN(. - 0xce9, 0x4000) + 0xce9;
*mapper_boards.o(.bss.board_timer)
. = ALIGN(. - 0xcf9, 0x4000) + 0xcf9;
*(.bss.mmc3_irq_enabled.lto_priv.0)
. = ALIGN(. - 0xd00, 0x4000) + 0xd00;
*(.data.stream.lto_priv.0)
. = ALIGN(. - 0xd14, 0x4000) + 0xd14;
*(.bss.ex_p.lto_priv.0)
. = ALIGN(. - 0xd58, 0x4000) + 0xd58;
*(.bss.video_measured_hz.lto_priv.0)
. = ALIGN(. - 0xdac, 0x4000) + 0xdac;
*(.bss.vrc_irq_used.lto_priv.0)
. = ALIGN(. - 0xde8, 0x4000) + 0xde8;
*(.bss.ppu_run_end_position.lto_priv.0)
