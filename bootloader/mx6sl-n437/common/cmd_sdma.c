#include <common.h>
#include <command.h>
#include <sdma.h>
#include <asm/arch/mx6.h>

#if defined(CONFIG_CMD_SDMA)

static char env_buffer[SDMA_ENV_BUF_SIZE];
static sdma_bd_t bd[1];

int do_sdma ( cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
  int src, dst, size;
  int idx, channel;
  sdma_chan_desc_t chan_desc;

  switch (argc) {
  case 4:
    dst  = simple_strtoul(argv[1], NULL, 16);
    src  = simple_strtoul(argv[2], NULL, 16);
    size = simple_strtoul(argv[3], NULL, 16);
    break;
  default:
    cmd_usage(cmdtp);
    return 1;
  }

  if (SDMA_RETV_SUCCESS != sdma_init((unsigned int *)env_buffer, DMA_REQ_PORT_HOST_BASE_ADDR)) {
      puts("SDMA initialization failed.\n");
      return -1;
  }

    unsigned int script_addr;
    if (SDMA_RETV_SUCCESS != sdma_lookup_script(SDMA_AP_2_AP_SIMP, &script_addr)) {
        puts("Invalid script.\n");
        return -1;
    }
    chan_desc.script_addr = script_addr;
    chan_desc.dma_mask[0] = chan_desc.dma_mask[1] = 0;
    chan_desc.priority = SDMA_CHANNEL_PRIORITY_LOW;
    for (idx = 0; idx < 8; idx++) {
        chan_desc.gpr[idx] = 0;
    }

    /* Setup buffer descriptors */
    if ((size << 2) > 0xffff){
	bd[0].mode = (((size << 2)&0xff0000)<<8) | SDMA_FLAGS_BUSY | SDMA_FLAGS_WRAP | ((size << 2)&0xffff);
//	printf("BD[0].mode = 0x%x \n",bd[0].mode);
    } else {
//	puts("Normal.\n");   	
	bd[0].mode = SDMA_FLAGS_BUSY | SDMA_FLAGS_WRAP | (size << 2);
    }
    bd[0].buf_addr = (void *)src;
    bd[0].ext_buf_addr = (void *)dst;

    /* Open channel */
    puts("Open SDMA channel for transfer.\n");
    channel = sdma_channel_request(&chan_desc, (sdma_bd_p) bd);
    if (channel < 0) {
        puts("Channel open failed.\n");
        return -1;
    }

    /* Start channel */
    puts("Channel opened, starting transfer...\n");
    sdma_channel_start(channel);

    /* Wait channel stop */
    unsigned int status;
    do {
        sdma_channel_status(channel, &status);
    } while (!(status & SDMA_CHANNEL_STATUS_DONE));
    
    /* Stop channel */
    puts("Transfer completed. Stop channel.\n");
    sdma_channel_stop(channel);
    sdma_channel_release(channel);
    sdma_deinit();

    return 0;

}

U_BOOT_CMD(
	sdma,   4,   1, do_sdma,
	"memory transfer using the i.MX SDMA Engine",
	"<dst> <src> <size>\n"
);

#endif
