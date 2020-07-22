#pragma once

#ifndef _CONFIG_H
#define _CONFIG_H

#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <stdlib.h>

#include <uv.h>

#define ARR_LENGTH			64				// len of char array 
#define MAX_AMOUNT			300
#define BUFFER_SIZE			1024
#define ANY_HOST			"0.0.0.0"
#define INI_PATH			"config.ini"

#define DEBUG_FLAG

typedef unsigned short	byte2;
typedef unsigned int	byte4;
typedef unsigned char	byte;

typedef enum BOOL_t {
	false = 0,
	true
} bool;

// DNS����ͷ
typedef struct DNSHeader_t {
	byte2		Id;				// ��ʶ��
	byte2		Flags;			// ��־
	byte2		QueryNum;		// �������
	byte2		AnswerNum;		// �ش����
	byte2		AuthorNum;		// Ȩ�����
	byte2		AdditionNum;	// ����RR��
} DNSHeader, *pDNSHeader;

// DNS�����������Row
typedef struct TransDNSRow_t {
	char		IP[ARR_LENGTH];			// IP��ַ
	char		Domain[ARR_LENGTH];		// ����
} TransDNSRow;

// DNS������
typedef TransDNSRow DNSTable[MAX_AMOUNT];

// IDת�����Row
typedef struct TransIDRow_t {
	byte2				prevID;				// ԭID
	bool				finished;			// �Ƿ���ɽ���
	struct sockaddr_in	client;				// �ͻ��׽��ֵ�ַ
	int					joinTime;			// ����ת�����ʱ��
	int					offset;				// �ͻ����ͱ��ĵ��ֽ���
	char				url[ARR_LENGTH];	// �ͻ�Ҫ��ѯ��url
} TransIDRow;

// IDת����
typedef TransIDRow IDTable[MAX_AMOUNT];

typedef struct TIME_t {
	byte	Day;
	byte    Hour;
	byte	Minute;
	byte	Second;
	byte4   Milliseconds;
} TIME;

#endif