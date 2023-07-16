#ifndef __ntx_timestamp_h //[
#define __ntx_timestamp_h 

#define NTX_TIMESTAMP_DESCRIPTION_LEN		64
typedef struct tagTimeStamp {
// public : 
	char *szName;
	uint32_t dwData;
	char szDescription[NTX_TIMESTAMP_DESCRIPTION_LEN+6];
// private :
	uint32_t dwIdx;
	struct timeval tv;
} NTX_TimeStamp;

#ifdef CONFIG_NTX_TIMESTAMPS //[
int NTX_TimeStamp_InEx(NTX_TimeStamp *I_ptTimeStamp);
int NTX_TimeStamp_In(const char *pszTimeStampName,uint32_t dwData);
int NTX_TimeStamp_In2(const char *pszTimeStampName,const char *pszDescription);
int NTX_TimeStamp_In3(const char *pszTimeStampName,uint32_t dwData,const char *pszDescription);
int NTX_TimeStamp_Out(NTX_TimeStamp *O_ptTimeStamp);

int NTX_TimeStamp_printf(const char *pszTimeStampName,uint32_t dwData,const char *fmt, ...);

#else //][!CONFIG_NTX_TIMESTAMPS
#define NTX_TimeStamp_InEx(ptTimeStamp) 0
#define NTX_TimeStamp_In(pszTimeStampName,dwData) 0
#define NTX_TimeStamp_In2(pszTimeStampName,pszDescription) 0
#define NTX_TimeStamp_In3(pszTimeStampName,dwData,pszDescription) 0
#define NTX_TimeStamp_Out(O_ptTimeStamp) 0
#define NTX_TimeStamp_printf(pszTimeStampName,dwData,fmt,args...) 0
#endif //] CONFIG_NTX_TIMESTAMPS



#endif //] __ntx_timestamp_h

