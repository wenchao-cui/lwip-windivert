#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#ifdef _MSC_VER
#include <windows.h>
#include <io.h>
#include <process.h>
#pragma warning(disable:4996)
#else
#include <sys/types.h>
#include <dirent.h>
#include <libgen.h>
#include <unistd.h>
#endif

#include "log.h"

#define LOG_BUFFER_SIZE	4096
#define LOG_SHORT_TIME 0



struct log_file_t {
	char log_level;
	
	unsigned int flush_count;  //达到多少就flush
	unsigned int buffer_count;  //缓冲的消息数量

	unsigned int size;  //当前文件大小
	unsigned long long total_size;//所有文件大小
	unsigned int one_size;	  //单个文件大小限制
	unsigned long long max_size; //总的大小限制
	char print_log;  //是否屏幕打印
	char short_time; //是否使用短时间
	char* filename;
	FILE* fp;
	time_t createtime;
};

//默认的日志文件
log_file g_log_file = {LOG_LEVEL_INFO,0,0,0,0,0,0,1, LOG_SHORT_TIME ,NULL ,NULL ,0 };

void log_set_level(int log_level)
{
	g_log_file.log_level = log_level % 5;
}

void log_set_flush_count(unsigned int flush_count)
{
	g_log_file.flush_count = flush_count;
}

void log_set_one_size(unsigned int one_size)
{
	if (one_size!=0 && one_size < 1024)
		g_log_file.one_size = 1024;
	else
		g_log_file.one_size = one_size;
}

void log_set_max_size(unsigned int max_size)
{
	if (max_size != 0 && max_size < 1024)
		g_log_file.max_size = 1024;
	else
		g_log_file.max_size = max_size;
}

/*
根据日期产生一个日志文件名
*/
static void genenate_default_filename(char filename[20])
{
	time_t now;
	struct tm *t1;
	char tmp[64]= "./";

	time(&now);
	t1 = localtime(&now);
	sprintf(tmp+2, "LOG_%04d%02d%02d.log",
		t1->tm_year + 1900, t1->tm_mon + 1, t1->tm_mday);
 
	strcpy(filename, tmp);
}

void log_file_set_filename(log_file* file, const char* filename)
{
	if (file->filename != NULL)
	{
		//文件名已经设置，且跟要设置的相同
		if (strcmp(file->filename, filename) == 0)
		{
			return;
		}
		else
		{
			free(file->filename);
			file->filename = (char*)malloc(strlen(filename) + 1);
		}
	}
	else
	{
		file->filename = (char*)malloc(strlen(filename) + 1);
	}

	if (file->filename == NULL)
	{
		printf("ERROR: log_set_filename %s malloc file->filename failed, error=%d\n", filename, errno);
		return;
	}
	else
	{
		memcpy(file->filename, filename, strlen(filename) + 1);
	}

	//如果文件已经打开，关闭
	if (file->fp != NULL)
	{
		fclose(file->fp);
		file->fp = NULL;
	}
}

void log_set_filename(const char* filename)
{
	log_file_set_filename(&g_log_file, filename);
}

void get_dir_base_name(char* filename, char* dirname, char* basename)
{
	int i;
	strcpy(dirname, ".");
	strcpy(basename, filename);
	for (i = (int) strlen(filename) - 1; i >= 0; i--)
	{
		if (filename[i] == '/' || filename[i] == '\\')
		{
			//前面是目录，后面是文件名
			if (i == 0)
			{
				strcpy(dirname, "/");
			}
			else
			{
				strncpy(dirname, filename, i);
			}

			strcpy(basename, &filename[i + 1]);

			break;
		}
	}
}

/*
	根据设置的文件最大字节数，删除多最早的文件，或者滚动写入
*/
static unsigned long long log_file_resize(log_file* file)
{
#ifdef _WIN32
	WIN32_FIND_DATAA findFileData;
	HANDLE hFind;
	char basename[260];
	int remove_size = 0;
#else
	DIR* dir;
	struct dirent *ptr;
	char dirname[255];
	char basename[255];
	//stat只支持2G大小文件
	struct stat file_stat;
#endif // _WIN32
	//long long size = 0;
	char remove_file[255] = { 0 };

	//不分多个文件存储
	if (file->one_size == 0)
	{
		//移动到头部继续写
		fseek(file->fp, 0, SEEK_SET);
		file->size = 0;
		file->total_size = 0;
		return 0;
	}

	//查找最早的一个文件（文件名最小）
#ifdef _WIN32
	strcpy(basename, file->filename);
	strcat(basename, ".????????_??????");
	hFind = FindFirstFileA(basename, &findFileData);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				continue;
			}
			else
			{
				if (remove_file[0] == 0 || strncmp(findFileData.cFileName, remove_file, strlen(findFileData.cFileName)) < 0)
				{
					//windows下 test.log.*会匹配到test.log
					//if (strcmp(findFileData.cFileName,basefile) != 0)
					{
						//FIXME: >4G文件不支持
						remove_size = findFileData.nFileSizeLow;
						strcpy(remove_file, findFileData.cFileName);
					}
				}
			}

		} while (FindNextFileA(hFind, &findFileData));

		FindClose(hFind);
	}
#else
	//打开目录
	get_dir_base_name(file->filename, dirname, basename);
	strcat(basename, ".");
	dir = opendir(dirname);
	if (dir == NULL)
	{
		printf("log_file_size opendir %s failed, errno=%d\n", dirname, errno);
		return -1;
	}

	while ((ptr = readdir(dir)) != NULL)
	{
		//当前目录或上一级目录
		if (strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0)
			continue;
		else if (ptr->d_type != 8) // not file
			continue;

		//文件前面相同,比较后面的序号
		if (strncmp(ptr->d_name, basename, strlen(basename)) == 0)
		{
			if (remove_file[0]==0 || strncmp(ptr->d_name, remove_file, strlen(ptr->d_name)) < 0)
			{
				strcpy(remove_file, ptr->d_name);
			}
		}
	}
#endif

	if (remove_file[0] != 0)
	{
#ifndef _WIN32
		if (stat(remove_file, &file_stat) == 0)
		{
			file->total_size -= file_stat.st_size;
		}
#else
		file->total_size -= remove_size;
#endif

		if (remove(remove_file) < 0)
		{
			printf("log_file_resize remove %s failed, errno=%d\n", remove_file, errno);
		}
	}

	return file->total_size;
}

/*
	创建一个新的文件
*/
static int log_file_new(log_file* file)
{
	struct tm *t1;
	char tmp[256];
	FILE* fp;

	if (file->createtime == 0)
		time(&file->createtime);

	t1 = localtime(&file->createtime);
	sprintf(tmp, "%s.%04d%02d%02d_%02d%02d%02d", file->filename,
		t1->tm_year + 1900, t1->tm_mon + 1, t1->tm_mday,
		t1->tm_hour, t1->tm_min, t1->tm_sec); 

	//FIXME：如果文件已经存在

	//关闭原来文件，建立新文件
	if (file->fp != 0)
	{
		fclose(file->fp);
	}

	rename(file->filename, tmp);

	//如果文件不存在，先创建一个空文件
	fp = fopen(file->filename, "w");
	if (fp == NULL)
	{
		printf("ERROR: log_file_new %s mode w failed, error=%d\n", file->filename, errno);
		return -1;
	}
	fclose(fp);

	fp = fopen(file->filename, "rb+");
	if (fp == NULL)
	{
		printf("ERROR: log_file_new %s mode rb+ failed, error=%d\n", file->filename, errno);
		return -1;
	}

	file->size = 0;
	file->fp = fp;

	//指针跳到结尾
	fseek(file->fp, 0, SEEK_END);

	time(&file->createtime);

	return 0;
}

/*
	获取文件大小
*/
static long long log_file_size(log_file* file)
{
#ifdef _WIN32
	WIN32_FIND_DATAA findFileData;
	HANDLE hFind;
	char basename[260];
#else
	DIR* dir;
	struct dirent *ptr;
	char dirname[255];
	char basename[255];
#endif // _WIN32
	//stat只支持2G大小文件
	struct stat file_stat;
	//long long size = 0;

	if (stat(file->filename, &file_stat) == 0)
	{
		//不分多个文件存储
		if (file->one_size == 0)
		{
			file->size = file_stat.st_size;
			file->total_size = file->size;
		}
		else
		{
			file->size = file_stat.st_size;
			file->total_size = file->size;
		}
	}
	else
	{
		file->size = 0;
		file->total_size = 0;
		printf("log_file_size stat %s failed, errno=%d\n", file->filename, errno);
	}

#ifdef _WIN32
	strcpy(basename, file->filename);
	strcat(basename, ".????????_??????");
	hFind = FindFirstFileA(basename, &findFileData);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				continue;
			}
			else
			{
				//FIXME: >4G文件
				file->total_size += findFileData.nFileSizeLow;
			}

		} while (FindNextFileA(hFind, &findFileData));

		FindClose(hFind);
	}
#else
	//打开目录
	get_dir_base_name(file->filename, dirname, basename);
	dir = opendir(dirname);
	if (dir == NULL)
	{
		printf("log_file_size opendir %s failed, errno=%d\n", dirname, errno);
		return -1;
	}

	//查找 test.log.开头的文件
	strcat(basename, ".");
	while ((ptr = readdir(dir)) != NULL)
	{
		//当前目录或上一级目录
		if (strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0)
			continue;
		else if (ptr->d_type != 8) // not file
			continue;

		//printf("log_file_size readdir file: %s\n", ptr->d_name);

		//文件前面相同
		if (strncmp(ptr->d_name, basename, strlen(basename)) == 0)
		{
			if (stat(ptr->d_name, &file_stat) == 0)
			{
				file->total_size += file_stat.st_size;
			}
			else
			{
				printf("log_file_size stat %s failed, errno=%d\n", ptr->d_name, errno);
			}
		}
	}
#endif

	return file->total_size;
}

//打开文件，返回文件大小，错误返回-1
static long long do_open_log_file(log_file* file)
{
	FILE* fp;

	if (file->filename == NULL)
		return -1;

	if(file->createtime==0)
		time(&file->createtime);

	//重新打开
	fp = fopen(file->filename, "rb+");

	if (fp == NULL)
	{
		//如果文件不存在，先创建一个空文件
		if (errno == 2)
		{
			fp = fopen(file->filename, "w");
			if (fp == NULL)
			{
				printf("ERROR: do_open_log_file %s mode w failed, error=%d\n", file->filename, errno);
				return -1;
			}
		}
		else
		{
			printf("ERROR: do_open_log_file %s mode rb+ failed, error=%d\n", file->filename, errno);
			return -1;
		}
	}
 
	file->fp = fp;

	//指针跳到结尾
	fseek(file->fp, 0, SEEK_END);

	//获取文件大小
	log_file_size(file);

	return file->total_size;
}

log_file* log_file_open(const char* filename, int log_level, int print_log, unsigned int one_size, unsigned int max_size)
{
	log_file *file;
 
	file = (log_file*)malloc(sizeof(log_file));
	if (file == NULL)
	{
		printf("ERROR: open log file %s malloc() failed, error=%d\n", filename, errno);
		return NULL;
	}

	//设置成默认值
	file->print_log = (char)print_log;
	file->short_time = g_log_file.short_time;

	file->filename = (char*)malloc(strlen(filename)+1);
	if (file->filename == NULL)
	{
		printf("ERROR: open log file %s malloc file->filename failed, error=%d\n", filename, errno);
		free(file);
		return NULL;
	}

	memcpy(file->filename, filename, strlen(filename) + 1);
	file->log_level = log_level % 5;

	if (one_size != 0 && one_size < 1024)
		file->one_size = 1024;
	else
		file->one_size = one_size;

	if (max_size != 0 && max_size < 1024)
		file->max_size = 1024;
	else
		file->max_size = max_size;

	if (do_open_log_file(file) == -1)
	{
		free(file->filename);
		free(file);
		return NULL;
	}

	return file;
}

void log_file_close(log_file* file)
{
	if (file != NULL)
	{
		if (file->fp != NULL)
		{
			fclose(file->fp);
			file->fp = NULL;
		}

		if (file->filename != NULL)
		{
			free(file->filename);
			file->filename = NULL;
		}
	}
}

void log_file_flush(log_file* file)
{
	if (file != NULL)
	{
		if (file->fp != NULL)
		{
			fflush(file->fp);

			//是否需要刷新缓冲
			if (file->flush_count != 0)
			{
				file->buffer_count = 0;
			}
		}
	}
}

static __inline int format_time_level(int short_time, char* buffer, int log_level)
{
	struct tm *localtime1;
	const char* level_name[5] = { "DEBUG", "INFO ", "WARN ", "ERROR", "FATAL" };
	//时间，级别

	time_t now;
	time(&now);

	localtime1 = localtime(&now);

	if (short_time)
	{
		sprintf(buffer, "%02d:%02d:%02d %s ",
			localtime1->tm_hour, localtime1->tm_min, localtime1->tm_sec,
			level_name[log_level % 5]);
	}
	else
	{
		sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d %s ",
			localtime1->tm_year + 1900, localtime1->tm_mon + 1, localtime1->tm_mday,
			localtime1->tm_hour, localtime1->tm_min, localtime1->tm_sec,
			level_name[log_level % 5]);
	}

	return (int)strlen(buffer);
}

static int do_write_file(log_file* file, char* data, int data_len)
{
	int len = 0;

	if (file->fp == NULL)
	{
		if (file->filename == NULL)
		{
			char filename[256] = { 0 };
			genenate_default_filename(filename);

			log_file_set_filename(file, filename);
		}

		if (do_open_log_file(file) == -1)
		{
			return -1;
		}
	}

	//如果已经超过文件最大值,产生新文件，或者从头开始
	if (file->max_size != 0)
	{
		//单文件
		if (file->one_size == 0)
		{
			if (file->total_size > file->max_size - data_len)
			{
				if (log_file_resize(file) > file->max_size)
					return -1;
			}
		}
		else
		{
			//如果单个文件超出大小，新创建一个文件
			if (file->size > file->one_size - data_len)
			{
				if (log_file_new(file) < 0)
				{
					return -1;
				}
			}

			//如果总大小超出，删除旧的文件
			if (file->total_size > file->max_size - data_len)
			{
				if (log_file_resize(file) > file->max_size)
					return -1;
			}
		}
	}

	while (len < data_len)
	{
		int write_bytes = 0;
		write_bytes = (int)fwrite(data+len, 1, data_len-len, file->fp);
		if (write_bytes < 0)
		{
			printf("ERROR: fwrite %s failed, error=%d\n", file->filename, errno);
			break;
		}

		len += write_bytes;
	}

	//是否需要刷新缓冲
	if (file->flush_count != 0)
	{
		if (file->buffer_count >= file->flush_count)
		{
			fflush(file->fp);
			file->buffer_count = 0;
		}
		else
		{
			file->buffer_count++;
		}
	}

	file->size += len;
	file->total_size += len;
	return len;
}

/*
自动把数据加上时间和日志级别，写十六进制数据
*/
static int do_write_hex_data(log_file* file, char* data, int data_len)
{
	char seperator[] = "  ";
	char line_offset[32];
	char line_hex[64];
	char line_ascii[32];

	int offset = 0;

	while (offset < data_len)
	{
		int j;
		int blank = 0;
		int line_chars = 16;

		memset(line_hex, ' ', sizeof(line_hex));

		if (data_len - offset < 16)
		{
			line_chars = data_len - offset;
		}

		//地址偏移
		sprintf(line_offset, "%04X:  ", offset);

		//内容十六进制
		for (j = 0; j < line_chars; j++)
		{
			unsigned char bytevalue=(unsigned char)data[offset + j];
			
			//1字节变成2个十六进制数ASCII
			line_hex[blank + j * 3] =  ((bytevalue>>4) < 10) ? ((bytevalue>>4)+'0') : ((bytevalue>>4)+'A'-10) ;
			line_hex[blank + j * 3 + 1 ] = ((bytevalue&0x0F) < 10) ? ((bytevalue&0x0F)+'0') : ((bytevalue&0x0F)+'A'-10) ;
			
			//可打印字符
			if ( bytevalue >= 0x20 && bytevalue <= 0x7e)
			{
				line_ascii[blank + j] = bytevalue;
			}
			else
			{
				line_ascii[blank + j] = '.';
			}

			if (j == 7)
			{
				//8字节，开始加空格
				blank = 1;
				line_hex[8 * 3] = ' ';
				line_ascii[8] = ' ';
			}
		}

		line_hex[16 * 3+1] = '\0';

		line_ascii[blank + line_chars] = '\n';
		line_ascii[blank + line_chars + 1] = '\0';

		offset += 16;

		do_write_file(file, line_offset, (int)strlen(line_offset));
		do_write_file(file, line_hex, (int)strlen(line_hex));
		do_write_file(file, seperator, 2);
		do_write_file(file, line_ascii, (int)strlen(line_ascii));
		//do_write_file(file, "\r\n", 2);
	}

	//写入文件
	return data_len;
}

/*
自动把数据加上时间和日志级别，写字符串
*/
static int do_write_time_level_msg(log_file* file, int log_level, const char* msg)
{
	char buffer[LOG_BUFFER_SIZE] = { 0 };
	int len = 0;

	//时间,日志级别
	len = format_time_level(file->short_time, buffer, log_level);

	if (msg != NULL)
	{
		//加上消息内容
		strcat(buffer, msg);
		len = (int)strlen(buffer);
	}

	//如果消息没有回车，加上回车
	if (buffer[len - 1] != '\n')
	{
		strcat(buffer, "\n");
		len += 1;
	}
	
	//写入文件
	do_write_file(file, buffer, (int)strlen(buffer));

	//屏幕显示
	if (file->print_log)
	{
		if (g_log_file.log_level == LOG_LEVEL_WARN)
		{
			printf("\033[1;34m%s\033[0m", buffer);
		}
		else if (g_log_file.log_level == LOG_LEVEL_ERROR)
		{
			printf("\033[1;31m%s\033[0m", buffer);
		}
		else
		{
			printf(buffer);
		}

		// notify_gui_log(log_level, buffer);
	}

	return len;
}

/*
自动把数据加上时间和日志级别，写二进制数据
*/
static int do_write_time_level_msg_data(log_file* file, int log_level, char* msg, char* data, int data_len)
{
	int len;

	if ( (data_len == 0 || data == NULL) && msg==NULL)
		return 0;

	//先写时间、日志级别、字符内容
	len = do_write_time_level_msg(file, log_level, msg);

	//数据以十六进制格式写入文件
	len += do_write_hex_data(file, data, data_len);

	return len;
}

int log_file_debug(log_file* file, const char* format, ...)
{
	va_list p;
	char msg[LOG_BUFFER_SIZE - 26];

	if (file == NULL)
		return -1;

	if (file->log_level > LOG_LEVEL_DEBUG)
		return 0;

	va_start(p, format);
	vsnprintf(msg, LOG_BUFFER_SIZE - 27, format, p);
	va_end(p);
 
	return do_write_time_level_msg(file, LOG_LEVEL_DEBUG, msg);
}

int log_file_info(log_file* file, const char* format, ...)
{
	va_list p;
	char msg[LOG_BUFFER_SIZE - 26];

	if (file == NULL)
		return -1;

	if (file->log_level > LOG_LEVEL_INFO)
		return 0;

	va_start(p, format);
	vsnprintf(msg, LOG_BUFFER_SIZE - 27, format, p);
	va_end(p);

	return do_write_time_level_msg(file, LOG_LEVEL_INFO, msg);
}

int log_file_warn(log_file* file, const char* format, ...)
{
	va_list p;
	char msg[LOG_BUFFER_SIZE - 26];

	if (file == NULL)
		return -1;

	if (file->log_level > LOG_LEVEL_WARN)
		return 0;

	va_start(p, format);
	vsnprintf(msg, LOG_BUFFER_SIZE - 27, format, p);
	va_end(p);

	return do_write_time_level_msg(file, LOG_LEVEL_WARN, msg);
}

int log_file_error(log_file* file, const char* format, ...)
{
	va_list p;
	char msg[LOG_BUFFER_SIZE - 26];

	if (file == NULL)
		return -1;

	if (file->log_level > LOG_LEVEL_ERROR)
		return 0;

	va_start(p, format);
	vsnprintf(msg, LOG_BUFFER_SIZE - 27, format, p);
	va_end(p);

	return do_write_time_level_msg(file, LOG_LEVEL_ERROR, msg);
}

int log_file_fatal(log_file* file, const char* format, ...)
{
	va_list p;
	char msg[LOG_BUFFER_SIZE - 26];

	if (file == NULL)
		return -1;

	if (file->log_level > LOG_LEVEL_FATAL)
		return 0;

	va_start(p, format);
	vsnprintf(msg, LOG_BUFFER_SIZE - 27, format, p);
	va_end(p);

	return do_write_time_level_msg(file, LOG_LEVEL_FATAL, msg);
}

int log_file_data(log_file* file, char* data, int data_len)
{
	char msg[20];

	if (file->log_level > LOG_LEVEL_DEBUG)
		return 0;

	sprintf(msg, "[DATA] %d bytes", data_len);
	return do_write_time_level_msg_data(file, LOG_LEVEL_DEBUG, msg,data,data_len);
}

int log_file_raw_data(log_file* file, char* data, int data_len)
{
	return do_write_file(file, data, data_len);
}

int log_file_hex_data(log_file* file, char* data, int data_len)
{
	return do_write_hex_data(file, data, data_len);
}


int log_file_data_msg(log_file* file, char* data, int data_len, const char* format, ...)
{
	va_list p;
	char msg[LOG_BUFFER_SIZE - 26];

	if (file->log_level > LOG_LEVEL_DEBUG)
		return 0;

	va_start(p, format);
	vsnprintf(msg, LOG_BUFFER_SIZE - 27, format, p);
	va_end(p);

	return do_write_time_level_msg_data(file, LOG_LEVEL_DEBUG, msg, data, data_len);
}

int log_debug(const char* format, ...)
{
	va_list p;
	char msg[LOG_BUFFER_SIZE - 26];

	if (g_log_file.log_level > LOG_LEVEL_DEBUG)
		return 0;

	va_start(p, format);
	vsnprintf(msg, LOG_BUFFER_SIZE - 27, format, p);
	va_end(p);

	return do_write_time_level_msg(&g_log_file, LOG_LEVEL_DEBUG, msg);
}

int log_info(const char* format, ...)
{
	va_list p;
	char msg[LOG_BUFFER_SIZE - 26];

	if (g_log_file.log_level > LOG_LEVEL_INFO)
		return 0;

	va_start(p, format);
	vsnprintf(msg, LOG_BUFFER_SIZE - 27, format, p);
	va_end(p);

	return do_write_time_level_msg(&g_log_file, LOG_LEVEL_INFO, msg);
}

int log_warn(const char* format, ...)
{
	va_list p;
	char msg[LOG_BUFFER_SIZE - 26];

	if (g_log_file.log_level > LOG_LEVEL_WARN)
		return 0;

	va_start(p, format);
	vsnprintf(msg, LOG_BUFFER_SIZE - 27, format, p);
	va_end(p);

	return do_write_time_level_msg(&g_log_file, LOG_LEVEL_WARN, msg);
}

int log_error(const char* format, ...)
{
	va_list p;
	char msg[LOG_BUFFER_SIZE - 26];

	if (g_log_file.log_level > LOG_LEVEL_ERROR)
		return 0;

	va_start(p, format);
	vsnprintf(msg, LOG_BUFFER_SIZE - 27, format, p);
	va_end(p);

	return do_write_time_level_msg(&g_log_file, LOG_LEVEL_ERROR, msg);
}

int log_fatal(const char* format, ...)
{
	va_list p;
	char msg[LOG_BUFFER_SIZE - 26];

	if (g_log_file.log_level > LOG_LEVEL_FATAL)
		return 0;

	va_start(p, format);
	vsnprintf(msg, LOG_BUFFER_SIZE - 27, format, p);
	va_end(p);

	return do_write_time_level_msg(&g_log_file, LOG_LEVEL_FATAL, msg);
}

int log_data(char* data, int data_len)
{
	if (g_log_file.log_level > LOG_LEVEL_DEBUG)
		return 0;

	return do_write_time_level_msg_data(&g_log_file, LOG_LEVEL_DEBUG, NULL, data, data_len);
}

int log_raw_data(char* data, int data_len)
{
	return do_write_file(&g_log_file, data, data_len);
}

int log_hex_data(char* data, int data_len)
{
	return do_write_hex_data(&g_log_file, data, data_len);
}

int log_data_msg(char* data, int data_len, const char* format, ...)
{
	va_list p;
	char msg[LOG_BUFFER_SIZE - 26];

	if (g_log_file.log_level > LOG_LEVEL_DEBUG)
		return 0;

	va_start(p, format);
	vsnprintf(msg, LOG_BUFFER_SIZE - 27, format, p);
	va_end(p);

	return do_write_time_level_msg_data(&g_log_file, LOG_LEVEL_DEBUG, msg, data, data_len);
}

void log_print(const char* format, ...)
{
	int len;
	va_list p;
	char msg[LOG_BUFFER_SIZE];

	//时间,日志级别
	len = format_time_level(LOG_SHORT_TIME , msg, LOG_LEVEL_INFO);

	va_start(p, format);
	vsnprintf(msg+ len, LOG_BUFFER_SIZE - len - 2, format, p);
	va_end(p);

	if (format[strlen(format) - 1] != '\n')
	{
		strcat(msg, "\n");
	}

	printf(msg);
}

void log_flush()
{
	log_file_flush(&g_log_file);
}