

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <linux/kfifo.h>
#include <linux/mutex.h>

#include "ntx_timestamp.h"



static struct kfifo_rec_ptr_1 gtNTX_TimeStampsFIFO;

#define TEMPBUF_SIZE		256
static char gcTempBufA[TEMPBUF_SIZE];
static const char *gszNTX_timestamps_proc_name = "ntx_timestamps";
static uint32_t gdwNTX_TimeStampIdx=0;
static DEFINE_MUTEX(NTX_TimeStampFIFOIn);

static uint32_t getDiffmS(struct timeval *tvEnd,struct timeval *tvBegin) 
{
	uint32_t dwDiffms = (tvEnd->tv_sec-tvBegin->tv_sec)*1000;
	dwDiffms += (tvEnd->tv_usec)/1000;
	dwDiffms -= (tvBegin->tv_usec)/1000;
  return (uint32_t) (dwDiffms); // ms
}

static uint32_t getDiffuS(struct timeval *tvEnd,struct timeval *tvBegin) 
{
	uint32_t dwDiffus = (tvEnd->tv_sec-tvBegin->tv_sec)*1000000;
	dwDiffus += (tvEnd->tv_usec);
	dwDiffus -= (tvBegin->tv_usec);
  return (uint32_t) (dwDiffus); // ms
}

static NTX_TimeStamp gtNTX_LastTimeStamp;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)) //[
// 4.1.15 .
#define SEQ_PRINT	1	
static int ntx_timestamp_proc_show(struct seq_file *seq, void *v)
{
	NTX_TimeStamp tNTX_TimeStamp;
	//uint32_t dwDiffmS ;
	uint32_t dwDiffuS ;

#ifdef SEQ_PRINT//[
	seq_printf(seq,"#,event,val,timestamp,diffUS,advance informations\n");
#else //][!SEQ_PRINT
	printk("#,event,val,timestamp,diffUS,advance informations\n");
#endif //] SEQ_PRINT
	while (NTX_TimeStamp_Out(&tNTX_TimeStamp)>=0) 
	{
		//dwDiffmS = getDiffmS(&tNTX_TimeStamp.tv,&gtNTX_LastTimeStamp.tv);
		dwDiffuS = getDiffuS(&tNTX_TimeStamp.tv,&gtNTX_LastTimeStamp.tv);
		
#ifdef SEQ_PRINT//[
		seq_printf(seq,"%u,%s,%u,%u.%06u,%u,%s\n",
#else //][!SEQ_PRINT
		printk("%u,%s,%u,%u.%06u,%u,%s\n",
#endif //] SEQ_PRINT
			(unsigned)tNTX_TimeStamp.dwIdx,
			tNTX_TimeStamp.szName,
			(unsigned)tNTX_TimeStamp.dwData,
			(unsigned)tNTX_TimeStamp.tv.tv_sec,
			(unsigned)tNTX_TimeStamp.tv.tv_usec,
			dwDiffuS,
			tNTX_TimeStamp.szDescription);
		

		gtNTX_LastTimeStamp=tNTX_TimeStamp;
	}
	return 0;
}
	
static int ntx_timestamp_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ntx_timestamp_proc_show, NULL);
}
	
struct file_operations ntx_timestamps_proc_fops = {
	.owner = THIS_MODULE,
	.open = ntx_timestamp_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


#else //][!
// 3.0.35 .
static int NTX_TimeStamp_ProcRead(char *page,char **start,off_t off,int count,int *eof,void *data)
{
#if 1

	NTX_TimeStamp tNTX_TimeStamp;
	//uint32_t dwDiffmS ;
	uint32_t dwDiffuS ;


	printk("#,event,val,timestamp,diffUS,advance informations\n");
	while (NTX_TimeStamp_Out(&tNTX_TimeStamp)>=0) 
	{
		//dwDiffmS = getDiffmS(&tNTX_TimeStamp.tv,&gtNTX_LastTimeStamp.tv);
		dwDiffuS = getDiffuS(&tNTX_TimeStamp.tv,&gtNTX_LastTimeStamp.tv);
		
		printk("%u,%s,%u,%u.%06u,%u,%s\n",
			(unsigned)tNTX_TimeStamp.dwIdx,
			tNTX_TimeStamp.szName,
			(unsigned)tNTX_TimeStamp.dwData,
			(unsigned)tNTX_TimeStamp.tv.tv_sec,
			(unsigned)tNTX_TimeStamp.tv.tv_usec,
			dwDiffuS,
			tNTX_TimeStamp.szDescription);
		

		gtNTX_LastTimeStamp=tNTX_TimeStamp;
	}
	return 0;
#else
	int len = 0;
	int iTempLen;
	NTX_TimeStamp tNTX_TimeStamp;
	uint32_t dwDiffmS ;
	volatile static int over_page=0;
	volatile static uint32_t gdwLastDiffmS ;
	int iBeginIdx=-1;



	printk("%s():start=0x%x,off=%u,count=%u,last idx=%u\n",
			__FUNCTION__,(unsigned)(*start),(unsigned)off,(unsigned)count,(unsigned)(gdwNTX_TimeStampIdx-1));

	if(off=0) {
		*page = 0;
	}

	while(1)
	{

		if(over_page) {
			over_page = 0;

			dwDiffmS = gdwLastDiffmS;
			tNTX_TimeStamp=gtNTX_LastTimeStamp;
			printk("restore last timestamp ,name=\"%s\",idx=%u,data=%u\n",
				gtNTX_LastTimeStamp.szName,(unsigned)gtNTX_LastTimeStamp.dwIdx,
				(unsigned)gtNTX_LastTimeStamp.dwData);
			
		}
		else {
			if(NTX_TimeStamp_Out(&tNTX_TimeStamp)<0) {
				*eof = 1;
				// *start = page+off;

				printk("timestamp %d~%u,%s,%u,%u.%u\n",iBeginIdx,
					(unsigned)gtNTX_LastTimeStamp.dwIdx,
					gtNTX_LastTimeStamp.szName,
					(unsigned)gtNTX_LastTimeStamp.dwData,
					(unsigned)gtNTX_LastTimeStamp.tv.tv_sec,
					(unsigned)gtNTX_LastTimeStamp.tv.tv_usec);
				break;
			}
			if(-1==iBeginIdx) {
				iBeginIdx = (int)tNTX_TimeStamp.dwIdx;
			}
			dwDiffmS = getDiffmS(&tNTX_TimeStamp.tv,&gtNTX_LastTimeStamp.tv);
		
		}


		iTempLen = sprintf(gcTempBufA,"%u,%s,%u,%u.%u,%u\n",
			(unsigned)tNTX_TimeStamp.dwIdx,
			tNTX_TimeStamp.szName,(unsigned)tNTX_TimeStamp.dwData,
			(unsigned)tNTX_TimeStamp.tv.tv_sec,
			(unsigned)tNTX_TimeStamp.tv.tv_usec,
			dwDiffmS);

		gdwLastDiffmS = dwDiffmS;
		gtNTX_LastTimeStamp=tNTX_TimeStamp;
		if(len+iTempLen>PAGE_SIZE)
		{
			printk("%s():------over %u bytes page size------\n",__FUNCTION__,(unsigned)PAGE_SIZE);
			printk("last timestamp name=\"%s\",idx=%u,data=%u\n",
				gtNTX_LastTimeStamp.szName,(unsigned)gtNTX_LastTimeStamp.dwIdx,
				(unsigned)gtNTX_LastTimeStamp.dwData);
			over_page = 1;
			break;
		}
		else {
			strcat(page,gcTempBufA);
			len+=iTempLen;
		}
	}

	// *start = gtNTX_LastTimeStamp.dwIdx;

	printk("return %u bytes,start=0x%x\n",(unsigned)len,(unsigned)(*start));
	return len;
#endif
}
#endif //] 

int NTX_TimeStamp_In3(const char *pszTimeStampName,uint32_t dwData,const char *pszDescription)
{
	NTX_TimeStamp tTimeStamp;

	tTimeStamp.szName = (char *)pszTimeStampName;
	tTimeStamp.dwData = dwData;

	tTimeStamp.szDescription[0] = '\0';
	tTimeStamp.szDescription[NTX_TIMESTAMP_DESCRIPTION_LEN] = '\0';
	tTimeStamp.szDescription[NTX_TIMESTAMP_DESCRIPTION_LEN+1] = '.';
	tTimeStamp.szDescription[NTX_TIMESTAMP_DESCRIPTION_LEN+2] = '.';
	tTimeStamp.szDescription[NTX_TIMESTAMP_DESCRIPTION_LEN+3] = '.';
	tTimeStamp.szDescription[NTX_TIMESTAMP_DESCRIPTION_LEN+4] = '\0';

	strncpy(tTimeStamp.szDescription,pszDescription,NTX_TIMESTAMP_DESCRIPTION_LEN);

	return NTX_TimeStamp_InEx(&tTimeStamp);
}

int NTX_TimeStamp_In2(const char *pszTimeStampName,const char *pszDescription)
{
	NTX_TimeStamp tTimeStamp;

	tTimeStamp.szName = (char *)pszTimeStampName;
	tTimeStamp.dwData = 0;

	tTimeStamp.szDescription[0] = '\0';
	tTimeStamp.szDescription[NTX_TIMESTAMP_DESCRIPTION_LEN] = '\0';
	tTimeStamp.szDescription[NTX_TIMESTAMP_DESCRIPTION_LEN+1] = '.';
	tTimeStamp.szDescription[NTX_TIMESTAMP_DESCRIPTION_LEN+2] = '.';
	tTimeStamp.szDescription[NTX_TIMESTAMP_DESCRIPTION_LEN+3] = '.';
	tTimeStamp.szDescription[NTX_TIMESTAMP_DESCRIPTION_LEN+4] = '\0';

	strncpy(tTimeStamp.szDescription,pszDescription,NTX_TIMESTAMP_DESCRIPTION_LEN);

	return NTX_TimeStamp_InEx(&tTimeStamp);
}

int NTX_TimeStamp_In(const char *pszTimeStampName,uint32_t dwData)
{
	NTX_TimeStamp tTimeStamp;

	tTimeStamp.szName = (char *)pszTimeStampName;
	tTimeStamp.dwData = dwData;
	tTimeStamp.szDescription[0] = '\0';

	return NTX_TimeStamp_InEx(&tTimeStamp);
}

int NTX_TimeStamp_InEx(NTX_TimeStamp *I_ptTimeStamp)
{
	unsigned int dwChk;
	NTX_TimeStamp tTimeStamp;
	int iRet = 0;

	mutex_lock(&NTX_TimeStampFIFOIn);
	I_ptTimeStamp->dwIdx = gdwNTX_TimeStampIdx++;
	do_gettimeofday(&I_ptTimeStamp->tv);

	dwChk = kfifo_in(&gtNTX_TimeStampsFIFO,I_ptTimeStamp,sizeof(NTX_TimeStamp));
	if(dwChk!=sizeof(NTX_TimeStamp)) {
		//printk("queue full !?\n");
		if(NTX_TimeStamp_Out(&tTimeStamp)>=0) {
			// drop a timestamp in queue .
			dwChk = kfifo_in(&gtNTX_TimeStampsFIFO,I_ptTimeStamp,sizeof(NTX_TimeStamp));
			if(dwChk!=sizeof(NTX_TimeStamp)) {
				printk(KERN_ERR"%s():enqueue error\n",__FUNCTION__);
				iRet = -2;goto exit;
			}
			else {
				iRet = 1;goto exit;
			}
		}
		iRet = -1;goto exit;
	}

exit:
	mutex_unlock(&NTX_TimeStampFIFOIn);

	return iRet;
}

int NTX_TimeStamp_printf(const char *pszTimeStampName,uint32_t dwData,const char *fmt, ...)
{
	va_list args;
	int i;
	NTX_TimeStamp tTimeStamp;

	tTimeStamp.szName = (char *)pszTimeStampName;
	tTimeStamp.dwData = dwData;

	tTimeStamp.szDescription[0] = '\0';

	va_start(args,fmt);
	i = vsnprintf(tTimeStamp.szDescription,NTX_TIMESTAMP_DESCRIPTION_LEN+6,fmt,args);
	va_end(args);

	return NTX_TimeStamp_InEx(&tTimeStamp);	
}

int NTX_TimeStamp_Out(NTX_TimeStamp *O_ptTimeStamp)
{
	unsigned int dwChk;
	dwChk = kfifo_out(&gtNTX_TimeStampsFIFO,O_ptTimeStamp,sizeof(NTX_TimeStamp));
	if(dwChk!=sizeof(NTX_TimeStamp)) {
		return -1;
	}
	return 0;
}




static int __init ntx_timestamp_init(void)
{
	int iChk;
	if(kfifo_alloc(&gtNTX_TimeStampsFIFO,PAGE_SIZE*4,GFP_KERNEL)) {
		printk(KERN_ERR"%s() kfifo alloc failed !\n",__FUNCTION__);
		return -ENOMEM;
	}
	NTX_TimeStamp_In("ntx_timestamp_init ok",0);


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)) //[
	// 4.1.15 .
	{
		struct proc_dir_entry *pdent;
		pdent = proc_create(gszNTX_timestamps_proc_name, S_IRUGO,NULL,&ntx_timestamps_proc_fops);
	}
#else //][! 
	iChk = create_proc_read_entry (gszNTX_timestamps_proc_name, S_IRUGO, NULL, NTX_TimeStamp_ProcRead, 0);
	if (!iChk) {
		printk("create \"%s\" failed\n",gszNTX_timestamps_proc_name);
	}
#endif //] 

	return 0;
}
module_init(ntx_timestamp_init);

static void __exit ntx_timestamp_exit(void)
{
	remove_proc_entry(gszNTX_timestamps_proc_name, NULL);
	kfifo_free(&gtNTX_TimeStampsFIFO);
}

module_exit(ntx_timestamp_exit);

MODULE_AUTHOR("Netronix, Inc.");
MODULE_DESCRIPTION("Timestamps module");
MODULE_LICENSE("GPL");

