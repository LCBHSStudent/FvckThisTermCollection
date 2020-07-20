#pragma once
#include <stdio.h>  
#include <string.h>  
#include <stdlib.h>

#define PRN_ERRMSG_RETURN printf

/*
 * @prarms��        title
 *                      �����ļ���һ�����ݵı�ʶ
 *                  key
 *                      ����������Ҫ������ֵ�ı�ʶ
 *                  filename
 *                      Ҫ��ȡ���ļ�·��
 * @return��        ��Ч�ַ��� / NULL
 */
char* GetIniKeyString(
	const char* title,
	const char* key,
	const char* filename
);

/*
 * @params��        title
 *                      �����ļ���һ�����ݵı�ʶ
 *                  key
 *                      ����������Ҫ������ֵ�ı�ʶ
 *                  filename
 *                      Ҫ��ȡ���ļ�·��
 * @return��         ��ЧINTֵ / 0
 */
int GetIniKeyInt(
	const char* title,
	const char* key,
	const char* filename
);

/*
 * @params��        title
 *                      �����ļ���һ�����ݵı�ʶ
 *                  key
 *                      ����������Ҫ������ֵ�ı�ʶ
 *                  val
 *                      ���ĺ��ֵ
 *                  filename
 *                      Ҫ��ȡ���ļ�·��
 * @return��         success: 0 / failed: -1
 */
int PutIniKeyString(
	const char* title,
	const char* key,
	const char* val,
	const char* filename
);

/*
 * @params��        title
 *                      �����ļ���һ�����ݵı�ʶ
 *                  key
 *                      ����������Ҫ������ֵ�ı�ʶ
 *                  val
 *                      ���ĺ��ֵ
 *                  filename
 *                      Ҫ��ȡ���ļ�·��
 * @return��         success: 0 / failed: -1
 */
int PutIniKeyInt(
	const char* title,
	const char* key,
	int			val,
	const char* filename
);
