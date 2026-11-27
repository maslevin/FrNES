/* tools/simplay/joint_opt.py: 156 code sections at their offsets modulo 16 kB (from simplay.map, 5400 s, /Users/maslevin/Documents/Projects/numechanix/perf_runs/simlay/v2/traces/smb_1_1/t, /Users/maslevin/Documents/Projects/numechanix/perf_runs/simlay/v2/traces/1943_stage1/t, /Users/maslevin/Documents/Projects/numechanix/perf_runs/simlay/v2/traces/mm3_needle/t, /Users/maslevin/Documents/Projects/numechanix/perf_runs/simlay/v2/traces/g2b/t, /Users/maslevin/Documents/Projects/numechanix/perf_runs/simlay/v2/traces/pinbot/t, /Users/maslevin/Documents/Projects/numechanix/perf_runs/simlay/v2/traces/cv3intro/t, /Users/maslevin/Documents/Projects/numechanix/perf_runs/simlay/v2/traces/lagrange/t, /Users/maslevin/Documents/Projects/numechanix/perf_runs/simlay/v2/traces/mappykids/t, /Users/maslevin/Documents/Projects/numechanix/perf_runs/simlay/v2/traces/feg/t, /Users/maslevin/Documents/Projects/numechanix/perf_runs/simlay/v2/traces/punchout/t, /Users/maslevin/Documents/Projects/numechanix/perf_runs/simlay/v2/traces/radracer2/t, /Users/maslevin/Documents/Projects/numechanix/perf_runs/simlay/v2/traces/radracer/t, /Users/maslevin/Documents/Projects/numechanix/perf_runs/simlay/v2/traces/rm6/t, /Users/maslevin/Documents/Projects/numechanix/perf_runs/simlay/v2/traces/zelda2/t, /Users/maslevin/Documents/Projects/numechanix/perf_runs/simlay/v2/traces/bttf/t, /Users/maslevin/Documents/Projects/numechanix/perf_runs/simlay/v2/traces/alien/t, /Users/maslevin/Documents/Projects/numechanix/perf_runs/simlay/v2/traces/cod/t, /Users/maslevin/Documents/Projects/numechanix/perf_runs/simlay/v2/traces/kirby/t, /Users/maslevin/Documents/Projects/numechanix/perf_runs/simlay/v2/traces/ng2/t, /Users/maslevin/Documents/Projects/numechanix/perf_runs/simlay/v2/traces/nwc/t, /Users/maslevin/Documents/Projects/numechanix/perf_runs/simlay/v2/traces/gimmick/t, /Users/maslevin/Documents/Projects/numechanix/perf_runs/simlay/v2/traces/rollingthunder/t) */
FRNES_HOT_SLOTS_LOOSE = DEFINED(FRNES_HOT_SLOTS_LOOSE) ? FRNES_HOT_SLOTS_LOOSE : 0;
. = ALIGN(. - 0xe8, 0x4000) + 0xe8;
*(.text.unlikely.cpu_boundary_poll)
. = ALIGN(. - 0x19c, 0x4000) + 0x19c;
*(.text.cpu_cycles_to_next_event)
. = ALIGN(. - 0x304, 0x4000) + 0x304;
*(.text.hooked_expand_tiles)
. = ALIGN(. - 0x5cc, 0x4000) + 0x5cc;
*(.text.unlikely.ex_bus_write)
. = ALIGN(. - 0x68c, 0x4000) + 0x68c;
*mapper_boards.o(.text.board_n163_halted)
. = ALIGN(. - 0x6ac, 0x4000) + 0x6ac;
*libc.a:libc_a-mprec.o(.text._Bfree)
. = ALIGN(. - 0x6cc, 0x4000) + 0x6cc;
*(.text.set_ppu_nametable_mirroring)
. = ALIGN(. - 0x740, 0x4000) + 0x740;
*(.text.hot.timer_cpu_run)
. = ALIGN(. - 0xf38, 0x4000) + 0xf38;
*libc.a:libc_a-strtoul.o(.text._strtoul_r)
. = ALIGN(. - 0xf78, 0x4000) + 0xf78;
*(.text.mmc1_apply_prg)
. = ALIGN(. - 0x1230, 0x4000) + 0x1230;
*(.text.hot.8_read)
. = ALIGN(. - 0x19b8, 0x4000) + 0x19b8;
*(.text.map_nametables.lto_priv.0)
. = ALIGN(. - 0x1b54, 0x4000) + 0x1b54;
*mapper_mmc3_sync.o(.text.hot.c_write_sync_mmc3)
. = ALIGN(. - 0x1be0, 0x4000) + 0x1be0;
*6502_core.o(.text.hot.1_core)
. = ALIGN(. - 0x3270, 0x4000) + 0x3270;
__frnes_hud_0 = .;
*(.text.hot.5_mmc3_cycles_to_clock)
__frnes_hud_0_end = .;
. = ALIGN(. - 0x3380, 0x4000) + 0x3380;
*(.text.unlikely.ex_interrupt)
. = ALIGN(. - 0x3520, 0x4000) + 0x3520;
*(.text.unlikely.n163_write)
. = ALIGN(. - 0x36e8, 0x4000) + 0x36e8;
*libc.a:libc_a-printf.o(.text.printf)
. = ALIGN(. - 0x3784, 0x4000) + 0x3784;
*libc.a:libc_a-mprec.o(.text.__d2b)
. = ALIGN(. - 0x3958, 0x4000) + 0x3958;
*(.text.hot.exact_process_dma)
. = ALIGN(. - 0xf4, 0x4000) + 0xf4;
*mapper_boards.o(.text.board_cycles_to_fire)
. = ALIGN(. - 0xd38, 0x4000) + 0xd38;
*(.text.~frnes_board)
. = ALIGN(. - 0x120, 0x4000) + 0x120;
__frnes_hud_1 = .;
*(.text.hot.3_sync_ppu_status)
__frnes_hud_1_end = .;
. = ALIGN(. - 0x2c0, 0x4000) + 0x2c0;
*mapper_boards.o(.text.board_n163_halt)
. = ALIGN(. - 0x34c, 0x4000) + 0x34c;
*libgcc.a:unorddf2.o(.text)
. = ALIGN(. - 0x3b0, 0x4000) + 0x3b0;
*(.text.apu_update_next_event.lto_priv.0)
. = ALIGN(. - 0x6e0, 0x4000) + 0x6e0;
*libgcc.a:floatundidf.o(.text)
. = ALIGN(. - 0x8f4, 0x4000) + 0x8f4;
*(.text.unlikely.ppu_status_stable_update)
. = ALIGN(. - 0xa80, 0x4000) + 0xa80;
*(.text.dmc_clock.lto_priv.0)
. = ALIGN(. - 0xb4c, 0x4000) + 0xb4c;
*(.text.video_vblank.lto_priv.0)
. = ALIGN(. - 0xc94, 0x4000) + 0xc94;
*(.text.unlikely.ppu_rendering_bus_address)
. = ALIGN(. - 0xfac, 0x4000) + 0xfac;
__frnes_hud_2 = .;
*(.text.ppu_restore_line_start)
__frnes_hud_2_end = .;
. = ALIGN(. - 0x12a8, 0x4000) + 0x12a8;
*(.text.cnrom_write)
. = ALIGN(. - 0x13fc, 0x4000) + 0x13fc;
*(.text.unlikely.ex_abs_indexed.constprop.0)
. = ALIGN(. - 0x15d4, 0x4000) + 0x15d4;
*(.text.unlikely.mapper_clocks_to_irq)
. = ALIGN(. - 0x16c0, 0x4000) + 0x16c0;
__frnes_hud_3 = .;
*(.text.hot.6_fetch)
__frnes_hud_3_end = .;
. = ALIGN(. - 0x18e0, 0x4000) + 0x18e0;
*(.text.vrc4_write)
. = ALIGN(. - 0x1c6c, 0x4000) + 0x1c6c;
*(.text.half_frame)
. = ALIGN(. - 0x1dcc, 0x4000) + 0x1dcc;
*(.text.apply_write.lto_priv.0)
. = ALIGN(. - 0x2574, 0x4000) + 0x2574;
*(.text.unlikely.ex_ind_y.constprop.0)
. = ALIGN(. - 0x2638, 0x4000) + 0x2638;
__frnes_hud_4 = .;
*(.text.ppu_evaluate_to)
__frnes_hud_4_end = .;
. = ALIGN(. - 0x29bc, 0x4000) + 0x29bc;
*(.text.chr_slot_map.lto_priv.0)
. = ALIGN(. - 0x2c3c, 0x4000) + 0x2c3c;
*(.text.video_frame_end)
. = ALIGN(. - 0x31b8, 0x4000) + 0x31b8;
*(.text.unlikely.vrc7_write)
. = ALIGN(. - 0x3524, 0x4000) + 0x3524;
*(.text.apu_run_events)
. = ALIGN(. - 0x134, 0x4000) + 0x134;
*(.text.vrc_irq_acknowledge)
. = ALIGN(. - 0x218, 0x4000) + 0x218;
*libc.a:libc_a-strlen.o(.text)
. = ALIGN(. - 0x270, 0x4000) + 0x270;
*(.text.hot.exact_plan)
. = ALIGN(. - 0x760, 0x4000) + 0x760;
*libc.a:libc_a-memcpy.o(.text)
. = ALIGN(. - 0x83c, 0x4000) + 0x83c;
*(.text.apu_dmc_dma_done)
. = ALIGN(. - 0xa98, 0x4000) + 0xa98;
*libc.a:libc_a-localeconv.o(.text._localeconv_r)
. = ALIGN(. - 0xaf8, 0x4000) + 0xaf8;
*mapper_boards.o(.text.board_n163_sync)
. = ALIGN(. - 0x10a4, 0x4000) + 0x10a4;
__frnes_hud_5 = .;
*(.text.ppu_dot_cpu_cycle)
__frnes_hud_5_end = .;
. = ALIGN(. - 0x1778, 0x4000) + 0x1778;
*(.text.hooked_build_row.constprop.0)
. = ALIGN(. - 0x1a20, 0x4000) + 0x1a20;
*libgcc.a:_udivdi3.o(.text)
. = ALIGN(. - 0x1c9c, 0x4000) + 0x1c9c;
__frnes_hud_6 = .;
*(.text.hot.timer_vrc7_envelope)
__frnes_hud_6_end = .;
. = ALIGN(. - 0x1f44, 0x4000) + 0x1f44;
*libgcc.a:floatundisf.o(.text)
. = ALIGN(. - 0x20ec, 0x4000) + 0x20ec;
__frnes_hud_7 = .;
*(.text.hot.3_sync_ppu_sync)
__frnes_hud_7_end = .;
. = ALIGN(. - 0x23a4, 0x4000) + 0x23a4;
*(.text.unlikely.ppu_mapper_clocks_resumed)
. = ALIGN(. - 0x2408, 0x4000) + 0x2408;
*libgcc.a:_ctzsi2.o(.text)
. = ALIGN(. - 0x24d8, 0x4000) + 0x24d8;
*libc.a:libc_a-dtoa.o(.text._dtoa_r)
. = ALIGN(. - 0x39a4, 0x4000) + 0x39a4;
*(.text.unlikely.ex_zp_indexed)
. = ALIGN(. - 0x3a00, 0x4000) + 0x3a00;
__frnes_hud_8 = .;
*(.text.hot.5_evaluate)
__frnes_hud_8_end = .;
. = ALIGN(. - 0x3cf4, 0x4000) + 0x3cf4;
*libc.a:libc_a-stdio.o(.text.__swrite)
. = ALIGN(. - 0x3d9c, 0x4000) + 0x3d9c;
*(.text.unlikely.ppu_poll_reject_reset.isra.0)
. = ALIGN(. - 0x3e28, 0x4000) + 0x3e28;
*(.text.vrc7_channel_update)
. = ALIGN(. - 0x134, 0x4000) + 0x134;
*mapper_boards.o(.text.board_write)
. = ALIGN(. - 0x2f4c, 0x4000) + 0x2f4c;
*(.text.unlikely.ppu_rendering_switched)
. = ALIGN(. - 0x31f4, 0x4000) + 0x31f4;
*(.text.mmc1_apply)
. = ALIGN(. - 0x3400, 0x4000) + 0x3400;
*libgcc.a:_udiv_qrnnd_16.o(.text)
. = ALIGN(. - 0x35c4, 0x4000) + 0x35c4;
*libc.a:libc_a-memchr.o(.text.memchr)
. = ALIGN(. - 0x3b18, 0x4000) + 0x3b18;
*libc.a:libc_a-memset.o(.text)
. = ALIGN(. - 0x3d00, 0x4000) + 0x3d00;
*(.text.hot.zz_main)
. = ALIGN(. - 0x180, 0x4000) + 0x180;
*(.text.sequencer_clock.lto_priv.0)
. = ALIGN(. - 0x38c, 0x4000) + 0x38c;
__frnes_hud_9 = .;
*(.text.hot.3_sync_ppu_status_race)
__frnes_hud_9_end = .;
. = ALIGN(. - 0x760, 0x4000) + 0x760;
__frnes_hud_10 = .;
*(.text.hot.5_mmc3_zz_cycles_to_far_clock)
__frnes_hud_10_end = .;
. = ALIGN(. - 0x980, 0x4000) + 0x980;
__frnes_hud_11 = .;
*(.text.ppu_sync_lazy_write)
__frnes_hud_11_end = .;
. = ALIGN(. - 0xb30, 0x4000) + 0xb30;
*(.text.dma_read)
. = ALIGN(. - 0xfe0, 0x4000) + 0xfe0;
*(.text.mmc1_write)
. = ALIGN(. - 0x1798, 0x4000) + 0x1798;
*(.text.video_crumb.lto_priv.0)
. = ALIGN(. - 0x1800, 0x4000) + 0x1800;
__frnes_hud_12 = .;
*(.text.hot.3_sync)
__frnes_hud_12_end = .;
. = ALIGN(. - 0x269c, 0x4000) + 0x269c;
*mapper_boards.o(.text.board_cpu_run)
. = ALIGN(. - 0x3b48, 0x4000) + 0x3b48;
__frnes_hud_13 = .;
*(.text.hot.timer_expansion_run)
__frnes_hud_13_end = .;
. = ALIGN(. - 0x19c, 0x4000) + 0x19c;
*mapper_boards.o(.text.nwc_apply)
. = ALIGN(. - 0x7d0, 0x4000) + 0x7d0;
*frame_hud_perf.o(.text.~frnes_cold_hud)
. = ALIGN(. - 0xca0, 0x4000) + 0xca0;
*(.text.stream_callback)
. = ALIGN(. - 0xe40, 0x4000) + 0xe40;
*(.text.hot.9_write)
. = ALIGN(. - 0x2128, 0x4000) + 0x2128;
*(.text.unlikely.ppu_output_line.constprop.0)
. = ALIGN(. - 0x2190, 0x4000) + 0x2190;
*(.text.unlikely.ppu_poll_skip)
. = ALIGN(. - 0x2464, 0x4000) + 0x2464;
*libc.a:libc_a-sprint_r.o(.text.__sprint_r)
. = ALIGN(. - 0x2720, 0x4000) + 0x2720;
*libc.a:libc_a-mprec.o(.text.__lo0bits)
. = ALIGN(. - 0x28e4, 0x4000) + 0x28e4;
*(.text.unlikely.ex_rmw)
. = ALIGN(. - 0x2b60, 0x4000) + 0x2b60;
__frnes_hud_14 = .;
*(.text.hot.5_mmc3_apply_chr)
__frnes_hud_14_end = .;
. = ALIGN(. - 0x2ce0, 0x4000) + 0x2ce0;
*(.text.hot.2_run)
. = ALIGN(. - 0x2ec4, 0x4000) + 0x2ec4;
*(.text.apu_synth_silence)
. = ALIGN(. - 0x2f80, 0x4000) + 0x2f80;
*(.text.unlikely.apu_read_status)
. = ALIGN(. - 0x30e8, 0x4000) + 0x30e8;
*(.text.hooked_prefetch_tile.lto_priv.0)
. = ALIGN(. - 0x3210, 0x4000) + 0x3210;
*(.text.vrc_irq_write_control)
. = ALIGN(. - 0x32f4, 0x4000) + 0x32f4;
*libc.a:libc_a-mprec.o(.text._Balloc)
. = ALIGN(. - 0x3d9c, 0x4000) + 0x3d9c;
*(.text.unlikely.ex_read_op)
. = ALIGN(. - 0x38c, 0x4000) + 0x38c;
*(.text.hot.c_write_sync)
. = ALIGN(. - 0x7d8, 0x4000) + 0x7d8;
*libc.a:libc_a-fflush.o(.text.__sflush_r)
. = ALIGN(. - 0x9e0, 0x4000) + 0x9e0;
*libgcc.a:_clzsi2.o(.text)
. = ALIGN(. - 0xa5c, 0x4000) + 0xa5c;
*libc.a:libc_a-fvwrite.o(.text.__sfvwrite_r)
. = ALIGN(. - 0xe6c, 0x4000) + 0xe6c;
*(.text.ppu_render_line_switches)
. = ALIGN(. - 0x1a80, 0x4000) + 0x1a80;
*(.text.unlikely.ex_fetch_word)
. = ALIGN(. - 0x1b7c, 0x4000) + 0x1b7c;
*(.text.startup.main)
. = ALIGN(. - 0x1d04, 0x4000) + 0x1d04;
*(.text.hot.timer_cycles_to_event)
. = ALIGN(. - 0x2154, 0x4000) + 0x2154;
*(.text.mmc3_write)
. = ALIGN(. - 0x275c, 0x4000) + 0x275c;
*(.text.hooked_fetch_sprite)
. = ALIGN(. - 0x293c, 0x4000) + 0x293c;
*(.text.update_levels.lto_priv.0)
. = ALIGN(. - 0x2aa0, 0x4000) + 0x2aa0;
__frnes_hud_15 = .;
*(.text.hot.4_render)
__frnes_hud_15_end = .;
. = ALIGN(. - 0x3e44, 0x4000) + 0x3e44;
*(.text.quarter_frame)
. = ALIGN(. - 0x7e0, 0x4000) + 0x7e0;
*(.text.ppu_replay_switches)
. = ALIGN(. - 0xb54, 0x4000) + 0xb54;
__frnes_hud_16 = .;
*(.text.hot.5_mmc3_bus_address)
__frnes_hud_16_end = .;
. = ALIGN(. - 0x1024, 0x4000) + 0x1024;
*(.text.unlikely.ex_interrupt_tail)
. = ALIGN(. - 0x10e4, 0x4000) + 0x10e4;
__frnes_hud_17 = .;
*(.text.hot.5_mmc3_apply_prg)
__frnes_hud_17_end = .;
. = ALIGN(. - 0x1d20, 0x4000) + 0x1d20;
__frnes_hud_18 = .;
*(.text.hot.7_row)
__frnes_hud_18_end = .;
. = ALIGN(. - 0x2168, 0x4000) + 0x2168;
*libgcc.a:_div_table.o(.text)
. = ALIGN(. - 0x29e4, 0x4000) + 0x29e4;
*(.text.hooked_rendering_dot)
. = ALIGN(. - 0x3e80, 0x4000) + 0x3e80;
*(.text.~frnes_cold_hud)
. = ALIGN(. - 0x83c, 0x4000) + 0x83c;
*(.text.apu_synth_pace)
. = ALIGN(. - 0xa14, 0x4000) + 0xa14;
*(.text.unlikely.ex_end_cycle)
. = ALIGN(. - 0xb98, 0x4000) + 0xb98;
*(.text.video_frame_begin)
. = ALIGN(. - 0x1038, 0x4000) + 0x1038;
*(.text.frame_hud_draw)
. = ALIGN(. - 0x1d24, 0x4000) + 0x1d24;
*libc.a:libc_a-fflush.o(.text._fflush_r)
. = ALIGN(. - 0x21e0, 0x4000) + 0x21e0;
__frnes_hud_19 = .;
*(.text.hot.timer_vrc7_span_fill)
__frnes_hud_19_end = .;
. = ALIGN(. - 0x2d7c, 0x4000) + 0x2d7c;
*(.text.apu_synth_log_write)
. = ALIGN(. - 0x2e20, 0x4000) + 0x2e20;
*6502_core.o(.text.cpu_cold)
. = ALIGN(. - 0x3eb0, 0x4000) + 0x3eb0;
*(.text.apu_synth_poll)
. = ALIGN(. - 0x860, 0x4000) + 0x860;
*(.text.hot.8_read_status)
. = ALIGN(. - 0xa14, 0x4000) + 0xa14;
*(.text.cpu_irq_update_line)
. = ALIGN(. - 0xbbc, 0x4000) + 0xbbc;
*(.text.hooked_prerender_fetches.lto_priv.0)
. = ALIGN(. - 0x1038, 0x4000) + 0x1038;
__frnes_hud_20 = .;
*(.text.ppu_data_increment)
__frnes_hud_20_end = .;
. = ALIGN(. - 0x1100, 0x4000) + 0x1100;
*(.text.mmc3_update_armed)
. = ALIGN(. - 0x21e4, 0x4000) + 0x21e4;
*mapper_boards.o(.text.board_n163_reset)
. = ALIGN(. - 0x22dc, 0x4000) + 0x22dc;
*(.text.cpu_sync_devices_io)
. = ALIGN(. - 0x2304, 0x4000) + 0x2304;
*(.text.unlikely.apu_synth_frame_options.lto_priv.0)
. = ALIGN(. - 0x8d0, 0x4000) + 0x8d0;
*(.text.~frnes_cold)
. = ALIGN(. - 0x10c4, 0x4000) + 0x10c4;
__frnes_hud_21 = .;
*(.text.ppu_prerender_oam.lto_priv.0)
__frnes_hud_21_end = .;
. = ALIGN(. - 0x21e8, 0x4000) + 0x21e8;
*(.text.vrc4_apply_prg)
. = ALIGN(. - 0x22e0, 0x4000) + 0x22e0;
*libc.a:libc_a-vfprintf.o(.text._vfprintf_r)
. = ALIGN(. - 0x10c8, 0x4000) + 0x10c8;
*(.text.unlikely.ex_read)
. = ALIGN(. - 0x2ee0, 0x4000) + 0x2ee0;
*(.text.hot.exact_fallback)
. = ALIGN(. - 0x2ee0, 0x4000) + 0x2ee0;
*(.text.unlikely.n163_output_fpu)
. = ALIGN(. - 0x31c0, 0x4000) + 0x31c0;
*libgcc.a:_umoddi3.o(.text)
. = ALIGN(. - 0x2f8c, 0x4000) + 0x2f8c;
__frnes_hud_22 = .;
*(.text.ppu_rendering_dot.lto_priv.0)
__frnes_hud_22_end = .;
. = ALIGN(. - 0x2fac, 0x4000) + 0x2fac;
*(.text.uxrom_write)
. = ALIGN(. - 0x3030, 0x4000) + 0x3030;
*(.text.unlikely.ex_adc)
. = ALIGN(. - 0x31e8, 0x4000) + 0x31e8;
__frnes_hud_23 = .;
*(.text.hot.5_mmc3_ppu_clock)
__frnes_hud_23_end = .;
. = ALIGN(. - 0x3008, 0x4000) + 0x3008;
*libc.a:libc_a-memmove.o(.text.memmove)
. = ALIGN(. - 0x31f8, 0x4000) + 0x31f8;
*(.text.vrc6_write)
. = ALIGN(. - 0x3020, 0x4000) + 0x3020;
*(.text.unlikely.ex_branch)
. = ALIGN(. - 0x3320, 0x4000) + 0x3320;
*(.text.hot.c_bank)
. = ALIGN(. - 0x3074, 0x4000) + 0x3074;
*(.text.unlikely.ppu_mapper_prerender_8x16.lto_priv.0)
. = ALIGN(. - 0x3330, 0x4000) + 0x3330;
*(.text.mmc3_set_chr)
*(SORT_BY_NAME(.text.hot.*))
