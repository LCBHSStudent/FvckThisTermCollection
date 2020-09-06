#include <config.h>
#include "utils_helper.h"

void SyncTime(SYSTEMTIME* sysTime, TIME* sysTimeLocal) {
	GetLocalTime(sysTime);
	sysTimeLocal->Day			= (byte)sysTime->wDay;
	sysTimeLocal->Hour			= (byte)sysTime->wHour;
	sysTimeLocal->Minute		= (byte)sysTime->wMinute;
	sysTimeLocal->Second		= (byte)sysTime->wSecond;
	sysTimeLocal->Milliseconds	= (byte)sysTime->wMilliseconds;
}

int32_t GetTimeSubValue(TIME* pFormer, TIME* pLater) {
	return (int32_t) (
		(
			(
					(pLater->Day - pFormer->Day) * 24 
				+ pLater->Hour - pFormer->Hour) * 60
			+ pLater->Minute - pFormer->Minute) * 60
		+ pLater->Second - pFormer->Second
	);
}

byte4 ToSecond(TIME* pTime) {
	return (byte4)(
		pTime->Day * 24 * 60 * 60
		+ pTime->Hour * 60 * 60
		+ pTime->Minute * 60
		+ pTime->Second
	);
}

char* ParseUrlFromData(char* baseBuffer, char* buffer, int size) {
	static char result[1024] = { 0 };
	if (size > 1024 || size <= 0) {
		return NULL;
	}

	memset(result, 0, 1024);
	
	//��.��λ��������
	byte url[1024] = { 0 };
	memcpy(url, buffer, size);

	int i = 0,
		j = 0,
		k = 0;
	int len = (int)strlen(url);

	while (i < len) {
		if (url[i] > 0 && url[i] <= 63)
			for (j = url[i], i++; j > 0; j--, i++, k++) {
				result[k] = url[i];
			}
		if (url[i] != 0) {
			if (url[i] >= 192) {
				int offset		= (url[i] - 192) * 256 + url[i+1];
				char* copySrc	= baseBuffer + offset + 1;
				
				result[k++] = '.';

				strcpy_s(&result[k], sizeof(result) - k - 1, copySrc);
				k += (int)strlen(copySrc);
				break;
			}
			else {
				result[k] = '.';
				k++;
			}
		}
	}
	result[k] = '\0';

	return result;
}

void DisplayTime(SYSTEMTIME* sysTime) {
	GetLocalTime(sysTime);
	printf("[systime_info]: ");
	printf("current system time: %d-%02d-%02d %02d:%02d:%02d:%03d\n",
		(byte4)sysTime->wYear,
		(byte4)sysTime->wMonth,
		(byte4)sysTime->wDay,
		(byte4)sysTime->wHour,
		(byte4)sysTime->wMinute,
		(byte4)sysTime->wSecond,
		(byte4)sysTime->wMilliseconds
	);
}

void DisplayIDTransInfo(TransIDRow* info) {
	if (info == NULL)
		return;
	char clientIP[16] = { 0 };
	uv_ip4_name((const struct sockaddr_in*)&info->client, clientIP, 15);
	printf("\t[id_transaction_info]\n");

	printf("\t\t[join_time] %d\n",		info->joinTime);
	printf("\t\t[client_addr] %s\n",	clientIP);
	printf("\t\t[previous_id] %d\n",	info->prevID);
	printf("\t\t[request_url] %s\n",	info->url);
	printf("\t\t[is_finished] %d\n",	info->finished);
	printf("\t\t[request_offset] %d\n", info->offset);
}

void* FindAnswerStart(byte* pData) {
	return pData + strlen(pData) + 5;
}


#define BigLittleSwap16(X) (\
	(((byte2)(X) & 0xFF00) >> 8)	|\
	(((byte2)(X) & 0x00FF) << 8)	\
)

#define BigLittleSwap32(X) (\
	(((byte4)(X) & 0xFF000000) >> 24) |\
	(((byte4)(X) & 0x00FF0000) >> 8)  |\
	(((byte4)(X) & 0x0000FF00) << 8)  |\
	(((byte4)(X) & 0x000000FF) << 24) \
)

// in c++14 or later, use constexpr instead
BOOL checkCPUendian() {
	union {
		unsigned long int i;
		unsigned char s[4];
	} c;
	c.i = 0x12345678;
	return (0x12 == c.s[0]);
}

byte2 nhswap_s(byte2 value) {
	return checkCPUendian() ? value : BigLittleSwap16(value);
}

byte4 nhswap_l(byte4 value) {
	return checkCPUendian() ? value : BigLittleSwap32(value);
}


long atol_t(const char* nptr) {
	long total = 0;
	char sign = '+';
	while (isspace(*nptr)) { ++nptr; }						// �����ո�
	if (*nptr == '-' || *nptr == '+') { sign = *nptr++; }	// ����Ƿ�ָ������
	while (isdigit(*nptr)) {
		total = 10 * total + ((*nptr++) - '0');
	}
	return (sign == '-') ? -total : total;
}

ULONG inet_addr_t(IN char* cp) {
	char ipBytes[4] = { 0 };
	LONG i;
	for (i = 0; i < 4; i++, cp++) {
		ipBytes[i] = (char)atol_t(cp);
		if (!(cp = strchr(cp, '.'))) { break; }
	}
	return *(ULONG*)ipBytes;
}

int Len2RespDataEnd(char* pData) {
	if (pData == NULL)
		return 0;
	int len = 0;
	while (*pData != '\0') {
		len++;
		pData++;
	}
	return len;
}
