#ifndef BEIDOU_LOG_H_
#define BEIDOU_LOG_H_

#include <stdio.h>
#include <fcntl.h>

#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>

#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_WARN 2
#define LOG_LEVEL_ERROR 3
#define LOG_LEVEL_FATAL 4

typedef struct log_file_t log_file;

void log_set_filename(const char* filename);
void log_set_level(int log_level);
void log_set_one_size(unsigned int one_size);
void log_set_max_size(unsigned int max_size);

/*
	以十六进制显示方式记录数据, 只有日志等级 = DEBUG级别时才有效
	参数msg为\0结尾的字符串， data为二进制数据 
*/
int log_data(char* data, int data_len);

/*
	记录原始数据
*/
int log_raw_data(char* data, int data_len);
int log_hex_data(char* data, int data_len);

/*
	记录二进制数据, 只有日志等级 = DEBUG级别时才有效
	参数msg为\0结尾的字符串， data为二进制数据
*/
int log_data_msg(char* data, int data_len, const char* format, ...);

//只打印在屏幕上，不记录到日志文件中，添加了时间，不管日志级别，一律显示
void log_print(const char* format, ...);

/*
	记录,格式为 printf 格式
*/
int log_debug(const char* format, ...);
int log_info(const char* format, ...);
int log_warn(const char* format, ...);
int log_error(const char* format, ...);
int log_fatal(const char* format, ...);

void log_flush();

/*
新建或者打开日志文件，如果失败返回NULL
*/
log_file* log_file_open(const char* filename, int log_level, int print_log, unsigned int one_size, unsigned int max_size);

int log_file_debug(log_file* log_file, const char* format, ...);
int log_file_info(log_file* log_file, const char* format, ...);
int log_file_warn(log_file* log_file, const char* format, ...);
int log_file_error(log_file* log_file, const char* format, ...);
int log_file_fatal(log_file* log_file, const char* format, ...);

int log_file_data(log_file* log_file, char* data, int data_len);
int log_file_data_msg(log_file* log_file, char* data, int data_len, const char* format, ...);

int log_file_raw_data(log_file* log_file, char* data, int data_len);
int log_file_hex_data(log_file* file, char* data, int data_len);

void log_file_flush(log_file* log_file);
void log_file_close(log_file* log_file);

#endif /* BEIDOU_LOG_H_ */
