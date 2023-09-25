#include <algorithm>
#include "aps6404.hpp"
#include "hardware/dma.h"

#ifndef NO_QSTR
#include "aps6404.pio.h"
#endif

namespace pimoroni {
    void APS6404::write_no_page_crossing(uint32_t addr, uint32_t* data, uint32_t len_in_bytes) {
        int len = len_in_bytes;
        int page_len = PAGE_SIZE - (addr & (PAGE_SIZE - 1));

        if ((page_len & 3) != 0) {
            while (len > page_len) {
                wait_for_finish_blocking();
                hw_set_bits(&dma_hw->ch[dma_channel].al1_ctrl, DMA_CH0_CTRL_TRIG_INCR_READ_BITS);

                pio_sm_put_blocking(pio, pio_sm, (page_len << 1) - 1);
                pio_sm_put_blocking(pio, pio_sm, 0x38000000u | addr);
                pio_sm_put_blocking(pio, pio_sm, pio_offset + sram_offset_do_write);

                dma_channel_transfer_from_buffer_now(dma_channel, data, (page_len >> 2) + 1);

                len -= page_len;
                addr += page_len;
                data += page_len >> 2;
                int bytes_sent_last_word = page_len & 3;
                page_len = std::min(4 - bytes_sent_last_word, len);
                
                dma_channel_wait_for_finish_blocking(dma_channel);
                pio_sm_put_blocking(pio, pio_sm, (page_len << 1) - 1);
                pio_sm_put_blocking(pio, pio_sm, 0x38000000u | addr);
                pio_sm_put_blocking(pio, pio_sm, pio_offset + sram_offset_do_write);
                pio_sm_put_blocking(pio, pio_sm, __builtin_bswap32(*data >> (8 * bytes_sent_last_word)));
                
                addr += page_len;
                len -= page_len;
                ++data;
                page_len = PAGE_SIZE - page_len;
            }
        }

        for (page_len = std::min(page_len, len);
            len > 0; 
            addr += page_len, data += page_len >> 2, len -= page_len, page_len = std::min(PAGE_SIZE, len))
        {
            wait_for_finish_blocking();
            hw_set_bits(&dma_hw->ch[dma_channel].al1_ctrl, DMA_CH0_CTRL_TRIG_INCR_READ_BITS);

            pio_sm_put_blocking(pio, pio_sm, (page_len << 1) - 1);
            pio_sm_put_blocking(pio, pio_sm, 0x38000000u | addr);
            pio_sm_put_blocking(pio, pio_sm, pio_offset + sram_offset_do_write);

            dma_channel_transfer_from_buffer_now(dma_channel, data, (page_len >> 2) + 1);
        }
    }

    void APS6404::write(uint32_t addr, uint32_t* data, uint32_t len_in_bytes) {
        if (!last_cmd_was_write) {
            last_cmd_was_write = true;
            wait_for_finish_blocking();
            dma_channel_set_write_addr(dma_channel, &pio->txf[pio_sm], false);
            dma_hw->ch[dma_channel].al1_ctrl = write_config.ctrl;
        }

        if (!page_smashing_ok) {
            write_no_page_crossing(addr, data, len_in_bytes);
            return;
        }

        for (int len = len_in_bytes, page_len = std::min(PAGE_SIZE, len); 
            len > 0; 
            addr += page_len, data += page_len >> 2, len -= page_len, page_len = std::min(PAGE_SIZE, len))
        {
            wait_for_finish_blocking();
            hw_set_bits(&dma_hw->ch[dma_channel].al1_ctrl, DMA_CH0_CTRL_TRIG_INCR_READ_BITS);

            pio_sm_put_blocking(pio, pio_sm, (page_len << 1) - 1);
            pio_sm_put_blocking(pio, pio_sm, 0x38000000u | addr);
            pio_sm_put_blocking(pio, pio_sm, pio_offset + sram_offset_do_write);

            dma_channel_transfer_from_buffer_now(dma_channel, data, (page_len >> 2) + 1);
        }
    }

    void APS6404::write_repeat(uint32_t addr, uint32_t data, uint32_t len_in_bytes) {
        if (!last_cmd_was_write) {
            last_cmd_was_write = true;
            dma_channel_set_write_addr(dma_channel, &pio->txf[pio_sm], false);
            dma_hw->ch[dma_channel].al1_ctrl = write_config.ctrl;
        }

        int first_page_len = PAGE_SIZE;
        if (!page_smashing_ok) {
            first_page_len -= (addr & (PAGE_SIZE - 1));
            if ((first_page_len & 3) != 0 && (int)len_in_bytes > first_page_len) {
                wait_for_finish_blocking();
                hw_clear_bits(&dma_hw->ch[dma_channel].al1_ctrl, DMA_CH0_CTRL_TRIG_INCR_READ_BITS);

                pio_sm_put_blocking(pio, pio_sm, (first_page_len << 1) - 1);
                pio_sm_put_blocking(pio, pio_sm, 0x38000000u | addr);
                pio_sm_put_blocking(pio, pio_sm, pio_offset + sram_offset_do_write);

                dma_channel_transfer_from_buffer_now(dma_channel, &data, (first_page_len >> 2) + 1);

                len_in_bytes -= first_page_len;
                addr += first_page_len;
                int bytes_sent_last_word = first_page_len & 3;
                first_page_len = PAGE_SIZE;
                data = (data >> (8 * bytes_sent_last_word)) | (data << (32 - (8 * bytes_sent_last_word)));
            }
        }

        for (int len = len_in_bytes, page_len = std::min(first_page_len, len); 
             len > 0; 
             addr += page_len, len -= page_len, page_len = std::min(PAGE_SIZE, len))
        {
            wait_for_finish_blocking();
            hw_clear_bits(&dma_hw->ch[dma_channel].al1_ctrl, DMA_CH0_CTRL_TRIG_INCR_READ_BITS);
            repeat_data = data;

            pio_sm_put_blocking(pio, pio_sm, (page_len << 1) - 1);
            pio_sm_put_blocking(pio, pio_sm, 0x38000000u | addr);
            pio_sm_put_blocking(pio, pio_sm, pio_offset + sram_offset_do_write);

            dma_channel_transfer_from_buffer_now(dma_channel, &repeat_data, (page_len >> 2) + 1);
        }
    }

    void APS6404::read(uint32_t addr, uint32_t* read_buf, uint32_t len_in_words) {
        start_read(read_buf, len_in_words);

        uint32_t first_page_len = PAGE_SIZE;
        if (!page_smashing_ok) {
            first_page_len -= (addr & (PAGE_SIZE - 1));
        }

        if (first_page_len >= len_in_words << 2) {
            pio_sm_put_blocking(pio, pio_sm, (len_in_words * 8) - 4);
            pio_sm_put_blocking(pio, pio_sm, 0xeb000000u | addr);
            pio_sm_put_blocking(pio, pio_sm, pio_offset + sram_offset_do_read);
        }
        else {
            uint32_t* cmd_buf = add_read_to_cmd_buffer(multi_read_cmd_buffer, addr, len_in_words);
            dma_channel_transfer_from_buffer_now(read_cmd_dma_channel, multi_read_cmd_buffer, cmd_buf - multi_read_cmd_buffer);
        }
    }

    void APS6404::multi_read(uint32_t* addresses, uint32_t* lengths, uint32_t num_reads, uint32_t* read_buf, int chain_channel) {
        uint32_t total_len = 0;
        uint32_t* cmd_buf = multi_read_cmd_buffer;
        for (uint32_t i = 0; i < num_reads; ++i) {
            total_len += lengths[i];
            cmd_buf = add_read_to_cmd_buffer(cmd_buf, addresses[i], lengths[i]);
        }

        start_read(read_buf, total_len, chain_channel);

        dma_channel_transfer_from_buffer_now(read_cmd_dma_channel, multi_read_cmd_buffer, cmd_buf - multi_read_cmd_buffer);
    }

    void APS6404::start_read(uint32_t* read_buf, uint32_t total_len_in_words, int chain_channel) {
        dma_channel_config c = read_config;
        if (chain_channel >= 0) {
            channel_config_set_chain_to(&c, chain_channel);
        }
        
        last_cmd_was_write = false;
        wait_for_finish_blocking();

        dma_channel_configure(
            dma_channel, &c,
            read_buf,
            &pio->rxf[pio_sm],
            total_len_in_words,
            true
        );
    }

    uint32_t* APS6404::add_read_to_cmd_buffer(uint32_t* cmd_buf, uint32_t addr, uint32_t len_in_words) {
        int32_t len_remaining = len_in_words << 2;
        uint32_t align = page_smashing_ok ? 0 : (addr & (PAGE_SIZE - 1));
        uint32_t len = std::min((PAGE_SIZE - align), (uint32_t)len_remaining);

        while (true) {
            if (len < 2) {
                *cmd_buf++ = 0;
                *cmd_buf++ = 0xeb000000u | addr;
                *cmd_buf++ = pio_offset + sram_offset_do_read_one;
            }
            else {
                *cmd_buf++ = (len * 2) - 4;
                *cmd_buf++ = 0xeb000000u | addr;
                *cmd_buf++ = pio_offset + sram_offset_do_read;
            }
            len_remaining -= len;
            addr += len;

            if (len_remaining <= 0) break;

            len = len_remaining;
            if (len > PAGE_SIZE) len = PAGE_SIZE;
        }

        return cmd_buf;
    }
}