#include <common.h>

#ifdef CONFIG_CONSOLE_QUIET
#define PUTS dmesg_puts
#else
#define PUTS printf
#endif

static int do_puts(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
  int i;
  if(argc>1) {
    for(i=1;i<argc;i++) {
      dmesg_puts(argv[i]);
      if(i!=argc-1)
	dmesg_puts(" ");
      else
	dmesg_puts("\n");
    };
    return 0;
  }

  cmd_usage(cmdtp);
  return 1;
}

U_BOOT_CMD(
	puts,	CONFIG_SYS_MAXARGS,	1,	do_puts,
#ifdef CONFIG_CONSOLE_QUIET
	"puts string to console (override the quiet option)",
	"- puts string to console (override the quiet option)"
#else
	"puts string to console",
	"- puts string to console"
#endif
);
