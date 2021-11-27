/*

	调用hw_data.h的接口对flash进行管理
	
	垃圾回收部分需要用到动态内存 如果平台有自带动态内存分配的rtos使用rtos
	没做重入处理如果有rtos要加互斥锁

*/

#include "user_storage_v2.h"
#include "co_debug.h"
#include "crc16.h"
#include "co_allocator.h"

#if 1
#define LOGD log_debug
#else
#define LOGD
#endif

#define EMPTH_SECTOR_HEAD {SECTOR_EMPTY,SECTOR_CLEAN,PAGE_CONFIG_MAGIC}
#define SWAP_SECTOR_HEAD {SECTOR_SW_EMPTY,SECTOR_SW_WIRTING,PAGE_CONFIG_MAGIC}

const File_head_t m_empty_file = {0xffffffff,0xffff,0xffff,0xffff,0xffff};

static uint32_t  page_head_check(Page_head_t head);
static void page_init_swap(uint16_t page_index);
static void page_set_status_dirty(uint16_t page_index,uint32_t dirty_stu);
static memory_alloc_cb  storage_mem_alloc 	= NULL;
static memory_free_cb	storage_mem_free	= NULL;
//extern uint8_t alg_max_buf[12000];
/*
		1-根据id key 遍历几页文件头数据 **---1**(只遍历非空的页)

			1-该头文件存在 FILE_FOUND

				记录当前的文件头的位置 和 旧数据的位置

			2-该文件不存在 FILE_NOFOUND
*/

File_exit_t get_file_location(File_t file_info,File_addr_t* local)
{
	Page_stu_t page_stu;
	File_head_t	head_file_temp;	
	uint16_t 	sector_index;
	uint32_t 	offset;
	uint32_t 	write_addr;
	for(sector_index = 0;sector_index < FDS_DATA_PAGE_NUM;sector_index++)
	{
		page_stu = page_check(sector_index);
		if(page_stu != PAGE_USED && page_stu != PAGE_FULL)
		{
			continue;
		}
		write_addr = (USER_STORAGE_ADDR_START + (sector_index*USER_STORAGE_PAGESIZE));//页起始地址
		
		offset = sizeof(Page_head_t);
		
		while(offset < USER_STORAGE_PAGESIZE)//从页头找到页尾
		{
			hw_data_read((write_addr + offset),(uint8_t *)&head_file_temp,sizeof(File_head_t));
			if(head_file_temp.file_status == FLASH_EMPTY_WORDS)//如果头文件的文件头为未使用 那这个块认为是新的 继续往下找
			{
				break;
			}
			//找到了
			else if(head_file_temp.file_status == FILE_WRITED && head_file_temp.record_id == file_info.file_id && head_file_temp.record_key == file_info.file_key)
			{
//				//校验通过
//				if(crc16_compute((uint8_t*)&head_file_temp.data,sizeof(File_head_data_t),NULL) == head_file_temp.head.crc16_data)
//				{
					local->page_index_data 	= 	sector_index;
					local->data_addr 		= 	(write_addr + offset);
					return FILE_FOUND;//找到
//				}
//				else
//				{
//					//crc错误的文件后续决定怎么处理 可能损坏了 先根据地址来处理 如果地址读出来的不对再通过文件id和key来删除
//					LOGD("crc err id 0x%x key 0x%x\n",file_info.file_id,file_info.file_key);
//					
//				}
			}
			offset += sizeof(File_head_t) + head_file_temp.file_length;//偏移整个头
		}
	}
	return FILE_NOFOUND;//找不到
}
/*
	根据数据长度找到空的数据和位置--4

		先找到一个空的头地址 **----2**(只遍历非满的页)

		先找到一个空的数据的头地址**----3**(只遍历非满的页)

		找不到空间给头文件 		返回FILE_ERR_FULL
		找不到空间给数据文件 	返回FILE_ERR_FULL	

*/
uint32_t find_location_for_data(File_addr_t* find_addr,uint32_t file_size)
{
	uint16_t 	sector_index;
	uint32_t 	offset;
	uint32_t 	write_addr;	
	File_head_t data_head;
	
	for(sector_index = 0;sector_index < FDS_DATA_PAGE_NUM;sector_index++)
	{
		//只遍历非满的页
		if(page_check(sector_index) != PAGE_USED && page_check(sector_index) != PAGE_EMPTY )
		{
			continue;
		}
		
		
		write_addr = (USER_STORAGE_ADDR_START + (sector_index*USER_STORAGE_PAGESIZE));//页起始地址
		
		offset = sizeof(Page_head_t);
		
		while(offset < USER_STORAGE_PAGESIZE)//从页头找到页尾
		{
			hw_data_read((write_addr + offset),(uint8_t *)&data_head,sizeof(File_head_t));
			if(data_head.file_status == FLASH_EMPTY_WORDS)
			{
				if((offset + sizeof(File_head_t) + file_size) <= USER_STORAGE_PAGESIZE)
				{
					LOGD("find data %d %d\n",sector_index,offset);
					find_addr->page_index_data 	= sector_index;
					find_addr->data_addr 		= (write_addr + offset);
					return FILE_SUCESS;
				}
				else if(offset +  MIN_DATA_SIZE > USER_STORAGE_PAGESIZE)
				{
					//这扇区认为满了 这个页不找了
					page_set_status(sector_index,PAGE_FULL);
					break;
				}					
			}
			else if(offset + data_head.file_length + sizeof(File_head_t) == USER_STORAGE_PAGESIZE)
			{
				//这个页刚好用满 这扇区认为满了 这个页不找了
				page_set_status(sector_index,PAGE_FULL);
				break;				
			}
			//继续遍历
			offset += data_head.file_length + sizeof(File_head_t);//偏移头和文件长度
		}
		
	}
	return FILE_ERR_FULL;	
}



/*
	根据指定的空间写入头文件和数据文件 状态为写入中
*/
uint32_t write_file(File_t file_info,File_addr_t addr,uint8_t* data)
{
	File_head_t head_file;
	uint32_t 	return_size;
	
	if(addr.data_addr < FDS_DATA_START_ADDR && addr.data_addr > FDS_STORAGE_END_ADDR)
	{
		LOGD("write data err 0x%x\n",addr.data_addr);
		return FILE_FAILD;
	}
	//LOGD("write data 0x%x size %d\n",addr.data_addr,file_info.file_size);
	
	//开始写数据文件
	//这里先写入数据的头 因为数据可能比较大 分段写入
	head_file.file_status = FILE_WRITE_READY;
	head_file.record_id   = file_info.file_id;
	head_file.record_key  = file_info.file_key;	
	head_file.file_length = file_info.file_size+((file_info.file_size&0x0003)?(4-(file_info.file_size&0x0003)):0);//四字节对齐
	head_file.crc16_data  = crc16_compute(data,head_file.file_length,NULL);	
	//这里把文件头写入	
	return_size = hw_data_write(addr.data_addr,(uint8_t *)&head_file,sizeof(File_head_t));
	//LOGD("write data head %d crc %x\n",return_size,head_file.crc16_data);
	//到这里数据文件头已经写入完成 写入文件数据 
	hw_data_write((addr.data_addr + return_size),data,head_file.file_length);
	//LOGD("write data data %d\n",head_file.file_length);
	//到这里文件数据已经写入完成
	//前面的文件可能占用时间长 避免断电 写到这里才第二次写入 文件状态为已写入
	uint32_t temp_status = FILE_WRITED;
	hw_data_write(addr.data_addr,(uint8_t *)&temp_status,sizeof(temp_status));
	//到这里写完一个文件
	return FILE_SUCESS;
}

/*
	根据文件地址设置文件的状态
*/
uint32_t set_file_status(File_addr_t addr,uint32_t status)
{
	uint32_t file_status = status;
	
	if(addr.data_addr < FDS_DATA_START_ADDR && addr.data_addr > FDS_STORAGE_END_ADDR)
	{
		LOGD("set data err 0x%x\n",addr.data_addr);
		return FILE_FAILD;
	}	
	hw_data_write(addr.data_addr,(uint8_t *)&file_status,sizeof(file_status));
	
	//有数据要删除 要把当前这块设置为脏
	if(status == FILE_DELTED)
	{
		page_set_status_dirty(addr.page_index_data,SECTOR_DIRTY);
	}
	
	return FILE_SUCESS;
}	
/*
	根据文件id和key 写入一个文件
*/

uint32_t fds_write_data(File_t file_info,uint8_t* data)
{
	uint32_t 		fds_ret;
	File_exit_t		file_exit;
	File_local_t 	temp_addr;

	//如果文件存在 将得到旧的地址
	file_exit = get_file_location(file_info,&temp_addr.old_addr);
	LOGD("find exit ? %d\n",(file_exit == FILE_FOUND));
	if(file_exit == FILE_FOUND)
	{
		//先标记原来的数据为准备删除
		LOGD("ready del..\n");
		if(set_file_status(temp_addr.old_addr,FILE_DELTED_READY) != FILE_SUCESS)
		{
			//地址有问题 返回
			return FILE_FAILD;
		}
	}
	//找到位置存储,同时判断该页的使用情况 如果写到结尾了 认为页满了 默认页为已使用
	fds_ret = find_location_for_data(&temp_addr.new_addr,file_info.file_size);
	if(fds_ret != FILE_SUCESS)
	{
		log_file_line();
		LOGD("err %d\n",fds_ret);
		return fds_ret;//可能是找不到位置
	}	
	//找到地方存储了
	write_file(file_info,temp_addr.new_addr,data);
	
	if(file_exit == FILE_FOUND)
	{
		//将标记的数据设置为已经删除
		LOGD("had del..\n");
		set_file_status(temp_addr.old_addr,FILE_DELTED);
	}	
	
	//到这里存储完成 如果页头的为emthy状态要修改为已使用 如果....
	if(page_check(temp_addr.new_addr.page_index_data) != PAGE_USED)//如果判断页的状态和实际上的不一样
	{
		page_set_status(temp_addr.new_addr.page_index_data,PAGE_USED);
	}	
	return FILE_SUCESS;	
}
/*
	先读出来再做校验
*/
static uint32_t read_file(File_addr_t addr,uint8_t* data ,uint32_t buff_size)
{
	File_head_t file_head;
	uint32_t read_size;
	//先读出文件头 然后读出实际的数据
	hw_data_read(addr.data_addr,(uint8_t *)&file_head,sizeof(File_head_t));
	//根据数据头的地址 和 size 读出数据 如果要读的数据大于存的数据size 则根据最短的来
//	read_size = (*size > file_head.file_length)?file_head.file_length:*size;
//	*size = read_size;
	if(file_head.file_length > (USER_STORAGE_PAGESIZE - sizeof(File_head_t) - sizeof(Page_head_t)))
	{
		log_debug("x1 len err %d\n",file_head.file_length);
		return FILE_ERR_LEN;
	}
	//长度只能用存的长度 否则crc会出错 这个后面需要用堆内存来保存数据
    //这里为了避免越界风险 需要使用上层的长度 如果读出来的长度跟目标长度不一样 则爆crc 错误
	hw_data_read((addr.data_addr+sizeof(File_head_t)),data,/*file_head.file_length*/buff_size);
    if(buff_size < file_head.file_length)
    {
		log_debug("x2 len err %d\n",file_head.file_length);
		return FILE_ERR_LEN;        
    }    
	//判断数据的crc
	if(crc16_compute(data,/*file_head.file_length*/buff_size,NULL) != file_head.crc16_data)
	{
		//后续再想想怎么处理
		log_debug("crc err\r\n");
		return FILE_ERR_CRC;
	}
	return FILE_SUCESS;
}

/*
	根据文件id和key 读出一个文件
    buff_size:增加一个读缓存最大的空间 防止异常长度导致越界
*/
uint32_t fds_read_data(File_t file_info,uint8_t* data,uint32_t buff_size)
{
	uint32_t 		fds_ret;
	File_exit_t		file_exit;
	File_local_t 	temp_addr;
	

	
	//如果文件存在 将得到旧的地址
	file_exit = get_file_location(file_info,&temp_addr.old_addr);
	LOGD("read exit? %d\n",(file_exit == FILE_FOUND));
	if(file_exit == FILE_FOUND)
	{	
		fds_ret = read_file(temp_addr.old_addr,data ,buff_size);
		return fds_ret;
	}
	else
	{
		return FILE_FAILD;
	}
			
}

/*
	根据文件id和key 删除一个文件
*/
uint32_t fds_delete_data(File_t file_info)
{
	uint32_t 		fds_ret;
	File_exit_t		file_exit;
	File_local_t 	temp_addr;
	

	
	//如果文件存在 将得到旧的地址
	file_exit = get_file_location(file_info,&temp_addr.old_addr);
	LOGD("read exit? %d\n",(file_exit == FILE_FOUND));
	if(file_exit == FILE_FOUND)
	{	
		LOGD("del file..\n");
		set_file_status(temp_addr.old_addr,FILE_DELTED);	
		return fds_ret;
	}
	else
	{
		return FILE_FAILD;
	}
			
}

/*
	修改一个页的状态
	注意这个只能 高状态向低状态修改
*/
void page_set_status(uint16_t page_index,Page_stu_t page_stu)
{
	uint32_t page_stu_change;
	
	switch(page_stu)
	{
		case PAGE_EMPTY:
			page_stu_change = SECTOR_EMPTY;
		break;
		case PAGE_USED:
			page_stu_change = SECTOR_USED;
		break;
		case PAGE_FULL:
			page_stu_change = SECTOR_FULL;
		break;
		case PAGE_SWAP:
			page_stu_change = SECTOR_SW_EMPTY;
		break;
		case PAGE_SWAP_USED:
			page_stu_change = SECTOR_SW_USED;
		break;		
		default:
			LOGD("err stu 0x%x\n",page_stu);
		break;	
	}
	if(page_stu != PAGE_SWAP)
		hw_data_write((USER_STORAGE_ADDR_START + (page_index*USER_STORAGE_PAGESIZE)),(uint8_t *)&page_stu_change,4);
	else
		page_init_swap(page_index);
	LOGD("change page %d stu 0x%x\r\n",page_index,page_stu_change);
}

static void page_set_status_dirty(uint16_t page_index,uint32_t dirty_stu)
{
	uint32_t page_dirty_change = dirty_stu;
	uint16_t offset = sizeof(uint32_t);

	hw_data_write((USER_STORAGE_ADDR_START + (page_index*USER_STORAGE_PAGESIZE)+offset),(uint8_t *)&page_dirty_change,4);

	LOGD("change page %d dirty 0x%x\r\n",page_index,page_dirty_change);
}

/*
	检测一个扇区的头是否合法
*/
static uint32_t  page_head_check(Page_head_t head)
{
	if(head.magic_num != PAGE_CONFIG_MAGIC)
		return 0;
	if(head.store_status != SECTOR_EMPTY && head.store_status != SECTOR_USED && head.store_status != SECTOR_FULL && head.store_status != SECTOR_SW_EMPTY  && head.store_status != SECTOR_SW_USED)
		return 0;
	if(head.dirty_status != SECTOR_CLEAN && head.dirty_status != SECTOR_DIRTY && head.dirty_status != SECTOR_CLEAR &&  head.dirty_status != SECTOR_SW_WIRTING)
		return 0;
	return 1;
}
/*
	返回一个扇区头的状态
*/
Page_stu_t page_check(uint16_t page_index)
{
	Page_head_t page_head;
	
	hw_data_read((USER_STORAGE_ADDR_START + (page_index*USER_STORAGE_PAGESIZE)),(uint8_t *)&page_head,sizeof(Page_head_t));
	if(!page_head_check(page_head))
	{
		return PAGE_UNDEFINE;
	}
	if(page_head.store_status == SECTOR_EMPTY)
		return PAGE_EMPTY;
	if(page_head.store_status == SECTOR_USED)
		return PAGE_USED;	
	if(page_head.store_status == SECTOR_FULL)
		return PAGE_FULL;	
	if(page_head.store_status == SECTOR_SW_EMPTY)
		return PAGE_SWAP;	
	if(page_head.store_status == SECTOR_SW_USED)
		return PAGE_SWAP_USED;	
	return PAGE_UNDEFINE;	
}

static uint32_t page_check_dirty(uint16_t page_index)
{
	Page_head_t page_head;
	
	hw_data_read((USER_STORAGE_ADDR_START + (page_index*USER_STORAGE_PAGESIZE)),(uint8_t *)&page_head,sizeof(Page_head_t));

	return  page_head.dirty_status;	
}

File_head_stu_t file_head_check(File_head_t file_head)
{
	if(memcmp((uint8_t *)&file_head,(uint8_t *)&m_empty_file,sizeof(File_head_t)) == 0)
	{
		return HEAD_UNUSED;
	}		
	if(file_head.file_status != FILE_WRITE_READY && file_head.file_status != FILE_WRITED \
		&&  file_head.file_status != FILE_DELTED_READY && file_head.file_status != FILE_DELTED)
	{
		return HEAD_UNVAILD;
	}
	if(file_head.file_length > MAX_DATA_SIZE)
	{
		return HEAD_OVER_LEN;
	}
	return 	HEAD_NORMAL;
}
/*
	初始化一个扇区：
	先擦除该扇区 然后往扇区头写入初始扇区头
*/
static void page_init(uint16_t page_index)
{
	Page_head_t page_head = EMPTH_SECTOR_HEAD;
	
	hw_data_erase((USER_STORAGE_ADDR_START + (page_index*USER_STORAGE_PAGESIZE)),USER_STORAGE_PAGESIZE);		
	hw_data_write((USER_STORAGE_ADDR_START + (page_index*USER_STORAGE_PAGESIZE)),(uint8_t *)&page_head,sizeof(Page_head_t));
}

static void page_init_swap(uint16_t page_index)
{
	Page_head_t page_head = SWAP_SECTOR_HEAD;
	
	hw_data_erase((USER_STORAGE_ADDR_START + (page_index*USER_STORAGE_PAGESIZE)),USER_STORAGE_PAGESIZE);		
	hw_data_write((USER_STORAGE_ADDR_START + (page_index*USER_STORAGE_PAGESIZE)),(uint8_t *)&page_head,sizeof(Page_head_t));
}
/*
	
*/
static uint32_t write_for_swap(uint16_t swap_index,uint32_t offset,File_head_t head,uint8_t* data)
{
	uint32_t 	write_addr;	
	uint32_t	return_size = 0;

	write_addr = (USER_STORAGE_ADDR_START + (swap_index*USER_STORAGE_PAGESIZE) + offset);//页起始地址
	
	return_size += hw_data_write(write_addr,(uint8_t *)&head,sizeof(File_head_t));
	return_size += hw_data_write((write_addr + return_size),data,head.file_length);	
	
	return return_size;	//返回的是实际上写了多少
}

/*
	辣鸡回收

	为了提升速率这里不分段写入 因为无论是否分段只要出现异常例如断电或者复位都会出现问题 
	这里只分段写页头
*/
static uint8_t page_garbage_collect(uint16_t gc_index,uint16_t swap_index)
{
	uint32_t 	offset_gc;
	uint32_t 	offset_swap;
	uint32_t 	write_addr;
	uint16_t	length_write;	
	uint32_t*	pTemp_data = NULL; 	//保证分配的内存是对齐	
	File_head_t data_head;	
	//在初始化一次交换区
	page_init_swap(swap_index);
	//先把交换页设置为使用中
	page_set_status(swap_index,PAGE_SWAP_USED);
	//遍历处理区
	write_addr = (USER_STORAGE_ADDR_START + (gc_index*USER_STORAGE_PAGESIZE));//页起始地址
	offset_gc 		= sizeof(Page_head_t);	
	offset_swap		= sizeof(Page_head_t);

	//pTemp_data = (uint32_t*)&alg_max_buf;//(uint32_t*)co_malloc(MAX_DATA_SIZE);
	pTemp_data = (uint32_t*)storage_mem_alloc(USER_STORAGE_PAGESIZE);
	if(!pTemp_data)
	{
		log_file_line();
		LOGD("malloc err for gc\n");
		return 0;	
	}	
	log_debug("gc %d %d\n",gc_index,swap_index);
	while(offset_gc < USER_STORAGE_PAGESIZE && offset_swap < USER_STORAGE_PAGESIZE)//从页头找到页尾
	{
		hw_data_read((write_addr + offset_gc),(uint8_t *)&data_head,sizeof(File_head_t));
		//扇区处理到尾部了 退出	
		if(data_head.file_status == FLASH_EMPTY_WORDS)
		{
			break;			
		}
		//找到了有效的数据
		if(data_head.file_status == FILE_WRITED)
		{
			if(((data_head.file_length+sizeof(File_head_t)+sizeof(offset_swap))  > USER_STORAGE_PAGESIZE) || (data_head.file_length == 0))
			{
				log_file_line();
				page_init_swap(swap_index);
				LOGD("gc err l:%d p:%d oc:%d\n",data_head.file_length,gc_index,offset_gc);
				return 0;
			}

			hw_data_read((write_addr + offset_gc + sizeof(File_head_t)),(uint8_t *)pTemp_data,data_head.file_length);
			length_write = write_for_swap(swap_index,offset_swap,data_head,(uint8_t *)pTemp_data);
			offset_swap += length_write;
			
		}
		//log_file_line();
		//继续遍历
		offset_gc += data_head.file_length + sizeof(File_head_t);//偏移头和文件长度
	}
	//设置为删除完成
	page_set_status_dirty(gc_index,SECTOR_CLEAR);
	//转移完成 设置为干净
	page_set_status_dirty(swap_index,SECTOR_CLEAN);
	//处理区成为新的交换区
	page_init_swap(gc_index);//这1步无论成功失败最后这个扇区都会被干掉 SECTOR_CLEAR的扇区都干掉
	//到这里已经处理完成
	page_set_status(swap_index,PAGE_USED);
	
	storage_mem_free(pTemp_data);
    pTemp_data = NULL;
    
	return 1;
}	

static uint8_t page_garbage_collect_crc_err(uint16_t gc_index,uint16_t swap_index)
{
	uint32_t 	offset_gc;
	uint32_t 	offset_swap;
	uint32_t 	write_addr;
	uint16_t	length_write;	
	uint32_t*	pTemp_data = NULL; 	//保证分配的内存是对齐	
	File_head_t data_head;	
	//在初始化一次交换区
	page_init_swap(swap_index);
	//先把交换页设置为使用中
	page_set_status(swap_index,PAGE_SWAP_USED);
	//遍历处理区
	write_addr = (USER_STORAGE_ADDR_START + (gc_index*USER_STORAGE_PAGESIZE));//页起始地址
	offset_gc 		= sizeof(Page_head_t);	
	offset_swap		= sizeof(Page_head_t);

	//pTemp_data = (uint32_t*)&alg_max_buf;//(uint32_t*)co_malloc(MAX_DATA_SIZE);
	pTemp_data = (uint32_t*)storage_mem_alloc(USER_STORAGE_PAGESIZE);
	if(!pTemp_data)
	{
		log_file_line();
		LOGD("malloc err for gc\n");
		return 0;	
	}	
	log_debug("crc gc %d %d\n",gc_index,swap_index);
	while(offset_gc < USER_STORAGE_PAGESIZE && offset_swap < USER_STORAGE_PAGESIZE)//从页头找到页尾
	{
		hw_data_read((write_addr + offset_gc),(uint8_t *)&data_head,sizeof(File_head_t));
		//扇区处理到尾部了 退出	
		if(data_head.file_status == FLASH_EMPTY_WORDS)
		{
			break;			
		}
		//找到了有效的数据
		if(data_head.file_status == FILE_WRITED)
		{
			if(((data_head.file_length+sizeof(File_head_t)+sizeof(offset_swap))  > USER_STORAGE_PAGESIZE) || (data_head.file_length == 0))
			{
				log_file_line();
				page_init_swap(swap_index);
				LOGD("gc err l:%d p:%d oc:%d\n",data_head.file_length,gc_index,offset_gc);
				return 0;
			}

			hw_data_read((write_addr + offset_gc + sizeof(File_head_t)),(uint8_t *)pTemp_data,data_head.file_length);
			if(crc16_compute((uint8_t *)pTemp_data,data_head.file_length,NULL) == data_head.crc16_data)
			{
				length_write = write_for_swap(swap_index,offset_swap,data_head,(uint8_t *)pTemp_data);
				offset_swap += length_write;
			}
			else
			{
				//crc出错的数据后面没法要了 可能是写入的时候掉电导致没写完整 只转移前面部分有效的数据
				LOGD("get crc err data %d %d\n",gc_index,offset_gc);
				break;
			}
			
		}
		//log_file_line();
		//继续遍历
		offset_gc += data_head.file_length + sizeof(File_head_t);//偏移头和文件长度
	}
	//设置为删除完成
	page_set_status_dirty(gc_index,SECTOR_CLEAR);
	//转移完成 设置为干净
	page_set_status_dirty(swap_index,SECTOR_CLEAN);
	//处理区成为新的交换区
	page_init_swap(gc_index);//这1步无论成功失败最后这个扇区都会被干掉 SECTOR_CLEAR的扇区都干掉
	//到这里已经处理完成
	page_set_status(swap_index,PAGE_USED);
	
	storage_mem_free(pTemp_data);
    pTemp_data = NULL; 
    
	return 1;
}	

/*
	将数据打出来看看发生了什么
*/
#if 1

//__align(4) uint8_t temp_string[200];

void test_temp(void)
{
	uint16_t 	sector_index;
	uint32_t 	offset;
	uint32_t 	write_addr;	
	File_head_t data_head;
	
	for(sector_index = 0;sector_index < FDS_DATA_PAGE_NUM;sector_index++)
	{
		//只遍历非满的页
		if(page_check(sector_index) != PAGE_USED && page_check(sector_index) != PAGE_FULL)
		{
			continue;
		}
		LOGD("page %d:\n",sector_index);
		
		write_addr = (USER_STORAGE_ADDR_START + (sector_index*USER_STORAGE_PAGESIZE));//页起始地址
		
		offset = sizeof(Page_head_t);
		
		while(offset < USER_STORAGE_PAGESIZE)//从页头找到页尾
		{
			hw_data_read((write_addr + offset),(uint8_t *)&data_head,sizeof(File_head_t));
			if(data_head.file_status == FLASH_EMPTY_WORDS)//如果头文件的文件头为未使用 那这个块认为是新的 继续往下找
			{
				LOGD("find useless data %d\n",offset);
				break;				
			}
			else if(offset + data_head.file_length + sizeof(File_head_t) == USER_STORAGE_PAGESIZE)
			{
				LOGD("full %d\n",offset);
				break;				
			}
			//到这里遍历完了1个数据
			LOGD("id:%d key:%d stu:0x%x size:%d offset%d\n",data_head.record_id,data_head.record_key,data_head.file_status,data_head.file_length,offset);
			//hw_data_read((write_addr + offset),(uint8_t *)&data_head,sizeof(File_data_t));
			//继续遍历
			offset += data_head.file_length + sizeof(File_head_t);//偏移头和文件长度
		}
		
	}	
}

#endif

uint32_t check_sector(uint16_t sector_index)
{
	File_head_t	head_file_temp;	
	uint32_t 	offset;
	uint32_t 	write_addr;
    uint32_t    ret = FILE_SUCESS;
    uint32_t*	pTemp_data = NULL; 	//保证分配的内存是对齐	
    uint32_t    file_stu;
    uint32_t    temp_stu;
    uint8_t     get_file = 0;
    
	write_addr = (USER_STORAGE_ADDR_START + (sector_index*USER_STORAGE_PAGESIZE));//页起始地址
		
	offset = sizeof(Page_head_t);
    //log_debug("check sec %d ",sector_index);
    //pTemp_data = (uint32_t*)&alg_max_buf;//(uint32_t*)co_malloc(MAX_DATA_SIZE);//分配一段可能使用的最大内存用来缓存数据做crc用
	pTemp_data = (uint32_t*)storage_mem_alloc(USER_STORAGE_PAGESIZE);

	while(offset < USER_STORAGE_PAGESIZE)//从页头找到页尾
	{
		hw_data_read((write_addr + offset),(uint8_t *)&head_file_temp,sizeof(File_head_t));

        //先判断数据头是不是合法：判断文件状态、数据长度、后面第4个字节数据是否存在
        file_stu = file_head_check(head_file_temp);

		if(file_stu == HEAD_UNUSED)//如果头文件的文件头为未使用 那这个块认为是新的 继续往下找
		{
            //log_debug("file unsed %d\n",offset);
			if(offset == sizeof(Page_head_t))
			{
				//这个扇区没有数据
				ret = FILE_FAILD;
			}
			break;
		}
        else
        {   
            if(head_file_temp.file_status != FILE_DELTED)
            {
                get_file = 1;
            }    
                
        }
        if(file_stu == HEAD_OVER_LEN)
        {
            log_debug("file over %d\n",offset);
            temp_stu = FILE_DELTED;
            hw_data_write((write_addr + offset),(uint8_t *)&temp_stu,4);//删除这个数据
            ret = FILE_ERR_LEN;
            break;
        }
        if(file_stu == HEAD_UNVAILD)
        {
            log_debug("file unvaild %d\n",offset);
			temp_stu = FILE_DELTED;
            hw_data_write((write_addr + offset),(uint8_t *)&temp_stu,4);//删除这个数据
        }
		//对数据进行crc判断
		
		if(head_file_temp.file_status == FILE_DELTED_READY || head_file_temp.file_status == FILE_WRITE_READY)
		{
            temp_stu = FILE_DELTED;
            hw_data_write((write_addr + offset),(uint8_t *)&temp_stu,4);//删除这个数据	
		}
		
        hw_data_read((write_addr + offset + sizeof(File_head_t)),(uint8_t *)pTemp_data,head_file_temp.file_length);
        if(!head_file_temp.file_length || crc16_compute((uint8_t *)pTemp_data,head_file_temp.file_length,NULL) != head_file_temp.crc16_data)
        {
            log_debug("crc err\r\n");
            ret = FILE_ERR_CRC;
            break;
        }
		offset = offset + sizeof(File_head_t) + head_file_temp.file_length;
		
	}
    
    if(!get_file)//使用中的扇区出现没有 有用数据的扇区 这个扇区需要马上回收
        ret = FILE_ERR_USELESS;
    
	storage_mem_free(pTemp_data);
    pTemp_data = NULL;
    
    return ret;
}


void flash_use_by_log(void)
{
    uint16_t sector_index = USER_STORAGE_SIZE/USER_STORAGE_PAGESIZE;
    uint32_t data = 0;
    hw_data_erase((USER_STORAGE_ADDR_START + (sector_index*USER_STORAGE_PAGESIZE)),USER_STORAGE_PAGESIZE);  
    hw_data_write((USER_STORAGE_ADDR_START + (sector_index*USER_STORAGE_PAGESIZE)),(uint8_t*)&data,4);
} 
void flash_put_log(uint16_t offset,uint8_t* data,uint8_t length)
{
   uint16_t sector_index = USER_STORAGE_SIZE/USER_STORAGE_PAGESIZE;
   uint32_t write_addr =  (USER_STORAGE_ADDR_START + (sector_index*USER_STORAGE_PAGESIZE))  +  offset;
        
   hw_data_write(write_addr,data,length); 
}    

void flash_log_dump(uint8_t* global_buf)
{
   uint32_t fault_happen;
   uint16_t sector_index = USER_STORAGE_SIZE/USER_STORAGE_PAGESIZE;
   uint32_t write_addr =  (USER_STORAGE_ADDR_START + (sector_index*USER_STORAGE_PAGESIZE)); 
    
    
   #ifdef CMBTRACE 
   hw_data_read(write_addr,(uint8_t*)&fault_happen,4); 
   if(fault_happen != 0xffffffff)
   {
      //这一段需要保存直到测量 这里复用心电的这断内存
      hw_data_read(write_addr+4,global_buf,2048); 
      for(uint16_t i = 0;i<2048;i++)
      {
          if(global_buf[i] != 0 && global_buf[i] != 0xff)
            log_debug("%c",global_buf[i]);
      }  
   }  
   log_debug("\r\n"); 
   #endif
}    


void check_all_sector(uint16_t swap_index)
{
	uint8_t		empty_index = 0;
	uint16_t 	sector_index;
	Page_stu_t	page_stu;
	uint16_t	swap_index_temp = swap_index;
	uint32_t	ret;
	log_debug("check_all_sector\n");
    for(sector_index = 0; sector_index < USER_STORAGE_SIZE/USER_STORAGE_PAGESIZE; sector_index++)
    {
		page_stu = page_check(sector_index);
		if(page_stu == PAGE_USED || page_stu == PAGE_FULL)	
		{
			ret = check_sector(sector_index);
			if(ret == FILE_ERR_CRC)
			{
				if(page_garbage_collect_crc_err(sector_index,swap_index_temp) == 1)//crc错误的扇区后面部分没法用 这里要改
				{
					swap_index_temp = sector_index;
				}
			}
			else if(ret == FILE_FAILD || ret == FILE_ERR_USELESS)
			{
				page_init(sector_index);
			}         
		}		
    }	
}
//erase all sector
void storage_reset(void)
{
	uint16_t 	sector_index;
    for(sector_index = 0; sector_index < USER_STORAGE_SIZE/USER_STORAGE_PAGESIZE; sector_index++)
    {	
		page_init(sector_index);
	}
	page_init_swap(sector_index - 1);	
}

void storage_alloc_fun_init(memory_alloc_cb fun_alloc,memory_free_cb fun_free)
{
	if(storage_mem_alloc == NULL)
	{
		storage_mem_alloc = fun_alloc;
	}
	if(storage_mem_free == NULL)
	{
		storage_mem_free = fun_free;
	}				
}	

uint32_t storage_init(void)
{
	static 		uint8_t check_once = 0;
	uint8_t		empty_index = 0;
	uint8_t		swap_cnt = 0;
	uint16_t 	sector_index;
	Page_stu_t	page_stu;
	uint32_t 	dirty_stu;
	uint8_t 	garbage_flag = 0;	
	uint16_t 	gc_index = 0xff;//辣鸡回收索引
	uint16_t 	swap_index = 0xff;//索引页索引
	
	uint8_t		full_cnt = 0;	//满扇区的个数
	uint8_t		gc_flag = 0;
    
	LOGD("storage init..\n");
	
    for(sector_index = 0; sector_index < USER_STORAGE_SIZE/USER_STORAGE_PAGESIZE; sector_index++)
    {
		page_stu = page_check(sector_index);
		switch(page_stu)
		{
			case PAGE_UNDEFINE:
				LOGD("udf %d ",sector_index);	
				page_init(sector_index);		
				empty_index = sector_index;
			break;	
			
			case PAGE_EMPTY:
				empty_index = sector_index;
				LOGD("emp %d ",sector_index);
			break;
			
			case PAGE_USED:
				LOGD("used %d ",sector_index);
			break;
			
			case PAGE_FULL:
				LOGD("full %d ",sector_index);
			break;
			
			case PAGE_SWAP:
				swap_cnt++;
				swap_index = sector_index;
				LOGD("swap %d ",sector_index);
			break;
			
			case PAGE_SWAP_USED:
				//swap_cnt++;
				LOGD("swapused %d ",sector_index);
				if(check_sector(swap_index) != FILE_SUCESS)
				{
					page_init_swap(swap_index);
					swap_cnt++;
				}			
				else
				{
					//log_debug("resume a sector\n");
					page_set_status_dirty(swap_index,SECTOR_CLEAN);
					page_set_status(swap_index,PAGE_USED);
				}
			break;			
		}
		dirty_stu = page_check_dirty(sector_index);
		
		
		LOGD("%x  ",dirty_stu);
		if(dirty_stu == SECTOR_DIRTY && page_stu == PAGE_FULL)
		{
			garbage_flag = 1;
			gc_index = sector_index;
			full_cnt++;
		}
		else if(dirty_stu == SECTOR_CLEAR)
		{
			log_debug("clear a sector %d\n",sector_index);
			page_init(sector_index);		
			empty_index = sector_index;
		}
			
    }
	
	if(swap_cnt == 0)
	{
		//LOGD("exception,add a swap page\n");
		swap_index = empty_index;
		page_set_status(empty_index,PAGE_SWAP);
	}
	if(garbage_flag)
	{
		//LOGD("garbage collection excute %d %d....\n",gc_index,swap_index);
		gc_flag = page_garbage_collect(gc_index,swap_index);
	}
	
	LOGD("\n");
	if(garbage_flag && gc_flag)
	{
		check_all_sector(gc_index);
	}	
	else if(swap_index != 0xff && !gc_flag && !check_once) //gc成功的话 交换页就不是swap_index了
	{
		check_once = 1;
		check_all_sector(swap_index);
	}
	
    
    return gc_flag;
	//test_temp();
}


