#ifndef __STORAGE_V2_H_
#define __STORAGE_V2_H_

#ifdef __cplusplus
extern "C"
{
#endif
#include "hw_data.h"
#include "user_driver.h"
#include <stdint.h>


//存储相关的宏定义	
#define			FDS_DATA_PAGE_NUM				47
	
//文件数据存储起始地址
#define			FDS_DATA_START_ADDR				(USER_STORAGE_ADDR_START)
#define			FDS_STORAGE_END_ADDR			(FDS_DATA_START_ADDR+(FDS_DATA_PAGE_NUM)*USER_STORAGE_PAGESIZE)		

#if (FDS_STORAGE_END_ADDR > (USER_STORAGE_ADDR_START+USER_STORAGE_SIZE))
#error "storage config error!"	
#endif	

#ifndef MIN_DATA_SIZE
#define			MIN_DATA_SIZE		500//辣鸡清理的扇区剩下的最小空间 根据实际的情况调整
#endif	
#define			MAX_DATA_SIZE		512//辣鸡清理的文件转移的最大size 根据实际的情况调整
//#define			FDS_GC_FULL_NUM		10	//满的扇区超过多少会执行回收

	
//数据结构相关	
typedef   uint16_t     crc16_t;

#define	FLASH_EMPTY_WORDS   0xffffffff	//未使用words

#define	SECTOR_SW_EMPTY		0x00ffffff	//交换区标志
#define	SECTOR_SW_USED		0x000fffff	//交换区标志
#define	SECTOR_EMPTY		0x0000ffff	//扇区空但是已写入扇区头
#define	SECTOR_USED			0x000000ff	//扇区正在使用	
#define	SECTOR_FULL			0x00000000	//扇区满	

#define	SECTOR_SW_WIRTING	0x00ffffff	//交换页用 代表交换页已使用 如果开机交换页是这个状态证明交换页在使用的时候断电过
#define	SECTOR_CLEAN		0x0000ffff	//干净 不存在无效数据
#define	SECTOR_DIRTY		0x000000ff	//脏 存在无效数据	
#define	SECTOR_CLEAR		0x00000000	//已清理 该扇区可以被重置为空页




#define	FILE_WRITE_READY	0x00ffffff //准备写入	
#define	FILE_WRITED			0x0000ffff //已写入
#define	FILE_DELTED_READY	0x000000ff //准备删除
#define	FILE_DELTED			0x00000000 //已经删除

#define  FILE_SUCESS		0
#define  FILE_FAILD			1
#define  FILE_ERR_LEN		2
#define  FILE_ERR_CRC		3
#define  FILE_ERR_FULL		4
#define  FILE_ERR_USELESS	5

#define  FILE_ID_VAILD		0xffff	

#define	 FILE_EXIT		0	
#define	 FILE_NO_EXIT	1	

#define	 FULL_NOCHECK	0
#define	 FULL_CHECK		1


typedef enum
{
	PAGE_UNDEFINE = 0,
	PAGE_EMPTY,
	PAGE_USED,
	PAGE_FULL,
	PAGE_SWAP,
	PAGE_SWAP_USED,
}Page_stu_t;


typedef enum
{
	FILE_FOUND,
	FILE_NOFOUND,
}File_exit_t;

typedef enum
{
	HEAD_NORMAL,	//数据头正常
	HEAD_UNVAILD,	//数据头非法
	HEAD_OVER_LEN,	//数据长度过长
	HEAD_UNUSED,	//数据头未使用
}File_head_stu_t;

/*
	页头：
	
	4字节 空、正在使用、满

	4字节 干净、可删除、已删除

	4字节 magic 幻数	
*/
typedef struct
{
	uint32_t store_status;		
	uint32_t dirty_status;
	uint32_t magic_num;
}Page_head_t;




/*
	一个文件头的数据
	
	头文件的大小固定 当前的是16字节 2021.6.3 
*/
typedef struct
{
	uint32_t file_status;
	uint16_t record_id;			//文件id
	uint16_t record_key;		//文件key
	uint16_t file_length;	
	crc16_t  crc16_data;
}File_head_t;


/*
	这个类用于保存文件的原始地址 和 新地址
*/
typedef struct
{
	uint16_t 	page_index_data;
	uint32_t 	data_addr;	
}File_addr_t;

typedef struct
{
	File_addr_t old_addr;
	File_addr_t new_addr;	
}File_local_t;

typedef struct
{
	uint16_t 	old_page_index_data;
	uint16_t 	new_page_index_data;
	uint32_t 	old_data_addr;
	uint32_t 	new_data_addr;	
}File_localmini_t;


/*
	保存的每个数据有唯一的一个 id和key
	如果文件分段保存 那用key值区分
*/

typedef struct
{
	uint16_t				file_size;
	uint16_t 				file_id;
	uint16_t				file_key;
	File_local_t			addr;
	File_exit_t				exit;	
}File_t;

typedef struct
{
	uint16_t 				file_id;
	uint32_t				measure_tick;
}FileMini1_t;

typedef struct
{
	uint16_t 				file_id;
	uint16_t				file_key;
}FileMini2_t;

typedef void* (*memory_alloc_cb)(uint32_t size);
typedef void (*memory_free_cb)(void* pv);

uint32_t storage_init(void);
uint32_t fds_write_data(File_t file_info,uint8_t* data);
uint32_t fds_read_data(File_t file_info,uint8_t* data,uint32_t buff_size);
File_exit_t get_file_location(File_t file_info,File_addr_t* local);
uint32_t find_location_for_data(File_addr_t* find_addr,uint32_t file_size);
uint32_t set_file_status(File_addr_t addr,uint32_t status);
uint32_t write_file(File_t file_info,File_addr_t addr,uint8_t* data);
Page_stu_t page_check(uint16_t page_index);
void page_set_status(uint16_t page_index,Page_stu_t page_stu);
uint32_t fds_delete_data(File_t file_info);
void storage_reset(void);
#ifdef __cplusplus
}
#endif


#endif


