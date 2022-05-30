#ifndef	__RAWDATATABLE_H__
#define	__RAWDATATABLE_H__

#include <linux/mmc/card.h>

void rawdatatable_load(struct mmc_card *card, const char* devname);
bool is_rawinit(void);
bool is_rawdatadev(const char* devname);
unsigned long	rawdata_index(const char* label, unsigned long* size);
int	rawdata_read(unsigned long index, unsigned long offset, char* buffer, unsigned long size);
int	rawdata_write(unsigned long index, unsigned long offset, char* buffer, unsigned long size);

#define RAWLABEL_BOOTIMAGE	"Boot Image"
#define RAWLABEL_WAVEFORM	"Waveform"
#define RAWLABEL_INFO		"Info"
#define RAWLABEL_ID			"Id"
#define RAWLABEL_LOG		"LOG"

#endif	/* __RAWDATATABLE_H__ */
