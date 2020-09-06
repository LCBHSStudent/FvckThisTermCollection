#pragma once

#ifndef _CONFIG_H
#define _CONFIG_H

#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <stdlib.h>

#include <uv.h>

#define ARR_LENGTH			64				// len of char array 
#define MAX_AMOUNT			1024
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

enum ANS_TYPE {
	A		= 1,				// IPV4��ַ
	NS		= 2,				// ����������
	CNAME	= 5,				// ����
	SOA		= 6,				// ��Ȩ
	WKS		= 11,				// ��ʶ����
	PTR		= 12,				// IPת��Ϊ����
	HINFO	= 13,				// ������Ϣ
	MX		= 15,				// �ʼ�����
	AAAA	= 28,				// IPv6	
	AXFR	= 252,				// ������������
	ANY		= 255				// �����м�¼������
};

// DNS����ͷ
typedef struct DNSHeader_t {
	byte2		Id;				// ��ʶ��
	byte2		Flags;			// ��־
	byte2		QueryNum;		// �������
	byte2		AnswerNum;		// �ش����
	byte2		AuthorNum;		// Ȩ�����
	byte2		AdditionNum;	// ����RR��
} DNSHeader, *pDNSHeader;

// #pragma pack(2)
// DNS������Answer��ͷ�� ע�������ڴ���뵼�µ������޷�ȡ������
typedef struct DNSAnswerHeader_t {
	byte2		Name;			// ��֪����ɶ��name
	byte2		Type;			// ��ѯ�������CNAME & A & AAAA...
	byte2		Class;			// In & Out?
	byte4		TTL;			// Effective Time
	byte2		DataLength;		// Length of response answer
} DNSAnswerHeader, *pDNSAnswerHeader;
// #pragma pack()

// DNS�����������Row
typedef struct TransDNSRow_t {
	byte2		Type;					// Answer Type
	byte4		TTL;					// ��Ч��
	byte		Data[ARR_LENGTH];		// IP��ַ
	byte		Domain[ARR_LENGTH];		// ����
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