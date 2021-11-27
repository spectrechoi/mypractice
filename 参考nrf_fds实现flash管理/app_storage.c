/*

	添加1层专门给应用层用 不同工程使用不同的app_mfs.c

	使用前先注册需要写的文件
*/

#include "co_debug.h"
#include "app_mfs.h"
#include "global.h"
#include "co_allocator.h"
#if 1
#define LOGD log_debug
#else
#define LOGD
#endif

/*
	血压存储逻辑:
	1-先找到存储的相关信息：新数据位置 原来的数据位置 数据原来是否存在 
	2-然后血压测量 正常测量完再根据刚刚找到的地址将数据保存
	
	心电存储逻辑：
	1-由于心电分为31个文件来保存  由于资源问题只能复用 
	由于心电数据是分批写入多个
	
	全部文件公用一个全局变量
*/


#define		BIGGEST_FILE_SIZE		40
extern uint8_t alg_max_buf[12000];
//uint8_t* m_mfs_crc_buf = alg_max_buf;//这里没有操作系统不担心重入问题

typedef struct
{
	File_head_t head;
	uint32_t	tick;
}HeadWithTick_t;

static uint32_t read_time(File_addr_t addr,uint32_t* tick)
{
	HeadWithTick_t file_head;
	uint32_t read_size;
	//先读出文件头 然后读出实际的数据
	hw_data_read(addr.data_addr,(uint8_t *)&file_head,sizeof(HeadWithTick_t));
	*tick = file_head.tick;
	
	return FILE_SUCESS;
}

//有时候需要用到分配一段内存来临时缓存读出来的flash数据
//6621未测试能否像st那样操作读flash
//co malloc存在问题这里使用一段不会同时使用的内存替代 由于一个扇区最大4096个字节 alg_max_buf足够大
void* mfs_memory_alloc(uint32_t size)
{
	void *pvReturn = NULL;
	
	return 	(void*)alg_max_buf;
}	

void mfs_memory_free(void *pv)
{

}
/*
	时间戳放数据段前4个字节  这个没做crc校验
*/
uint32_t mfs_read_times(File_t* file,uint32_t* time)
{
	uint32_t 		fds_ret;
	File_exit_t		file_exit;

	
	//如果文件存在 将得到旧的地址
	file_exit = get_file_location(*file,&file->addr.old_addr);
	LOGD("get time exit? %d\n",(file_exit == FILE_FOUND));
	if(file_exit == FILE_FOUND)
	{	if(time != NULL)
		{
			fds_ret = read_time(file->addr.old_addr,time);
			//log_debug("0x%x\n",time);
			return fds_ret;			
		}
		else
		{
			return FILE_SUCESS;
		}

	}
	else
	{
		return FILE_FAILD;
	}
}

uint32_t file_checksum(File_addr_t addr,uint32_t* tick)
{
	HeadWithTick_t 	file_head;
	uint32_t 		read_size;
	uint8_t* 		crc_buf = NULL;
	
	
	crc_buf = (uint8_t*)mfs_memory_alloc(USER_STORAGE_PAGESIZE);
	//先读出文件头 然后读出实际的数据
	hw_data_read(addr.data_addr,(uint8_t *)&file_head,sizeof(HeadWithTick_t));
	if(file_head.head.file_length > BIGGEST_FILE_SIZE || !crc_buf)
	{
		log_file_line();
		log_debug("please change BIGGEST_FILE_SIZE size\n");
		return FILE_ERR_LEN;
	}
	
	hw_data_read((addr.data_addr+sizeof(File_head_t)),crc_buf,file_head.head.file_length);
	//判断数据的crc
	if(crc16_compute(crc_buf,file_head.head.file_length,NULL) != file_head.head.crc16_data)
	{
		//后续再想想怎么处理
		log_debug("crc err\r\n");
		return FILE_ERR_CRC;
	}
	*tick = file_head.tick;
	mfs_memory_free(crc_buf);
	crc_buf = NULL;
	return FILE_SUCESS;	
}

/*
	时间戳放数据段前4个字节  这个没做了crc校验
*/
uint32_t mfs_read_times_ex(File_t* file,uint32_t* tick)
{
	uint32_t 		fds_ret;
	File_exit_t		file_exit;

	
	//如果文件存在 将得到旧的地址
	//这个地方没校验
	file_exit = get_file_location(*file,&file->addr.old_addr);
	//LOGD("get time ex exit? %d\n",(file_exit == FILE_FOUND));
	if(file_exit == FILE_FOUND/* && m_mfs_crc_buf*/)
	{	
		fds_ret = file_checksum(file->addr.old_addr,tick);
		return fds_ret;
	}
	else
	{
		return FILE_FAILD;
	}
}


/*
	血压需要存1个结果文件 所以是需要 1个id 1个key
	存之前需要找到一个未使用的id或者最老的id  因为是轮询 所以这里需要遍历血压的存储数据 然后记录下次要操作的id
	1次1个
	
	
	心电1个结果文件 30个波形文件 1个id 31个key
	31个
*/

/*
	获得结果文件的 id key 旧文件地址 新文件地址
*/

uint32_t mfs_file_open(File_t *file,uint8_t file_type,char *file_name,char mode)
{
	File_t		  	temp_file;	
	unsigned char 	file_num_end = 0;
	unsigned char 	file_num_start = 0;
	uint32_t		file_tick = 0xffffffff;			
	uint32_t 		temp_tick = 0xffffffff;
	char 			buff[16];
	uint32_t		ret;
    
	switch(file_type){
		case MFS_BP_FILE: //血压数据文件
			file_num_end = BP_FILE_NUM;//BP_FILE_NUM;
			file_num_start = 0;
			break;
		case MFS_ECG_FILE: //心电数据文件
			file_num_end = FILE_TOTAL_NUM;
			file_num_start = BP_FILE_NUM;
			break;
		case MFS_ALL_FILE:
			file_num_end = FILE_TOTAL_NUM;
			file_num_start = 0;			
			break;
		default :
			break;
	}	
	temp_file.file_key = RECORD_RESULT_KEY;
	
	if(mode == 'w')
	{
		if((file->file_size == 0) || (file->file_size > (USER_STORAGE_PAGESIZE - sizeof(Page_head_t))))
		{
			LOGD("err length\n");
			return FILE_ERR_LEN;
		}
		
		//开始遍历历史文件要使用的id 和 key
		for(uint8_t i = file_num_start; i < file_num_end; i++)
		{
			
			temp_file.file_id = i;
			if(FILE_SUCESS == mfs_read_times(&temp_file,&temp_tick))
			{
				//找到一个更小的时间戳
				if(temp_tick < file_tick)
				{
					file_tick = temp_tick;
					file->file_id  		= i;
					file->file_key 		= temp_file.file_key;
					file->addr.old_addr = temp_file.addr.old_addr;
					file->exit	   		= FILE_FOUND;
				}
			}
			//找到空的id号
			else
			{
				file->exit		= FILE_NOFOUND;
				file->file_id  	= i;
				file->file_key 	= temp_file.file_key;			
				break;
			}		
		}
		
		//到这里找到了要使用的id 和 key
		uint32_t fds_ret = find_location_for_data(&temp_file.addr.new_addr,file->file_size);
		if(fds_ret != FILE_SUCESS)
		{
			log_file_line();
			LOGD("err %d\n",fds_ret);
			return fds_ret;//可能是找不到位置
		}	
		file->addr.new_addr = temp_file.addr.new_addr;
		
		return FILE_SUCESS;		
	}
	else
	{
		uint8_t 		find_file = 0;
		uint32_t		file_size = 0;
		uint16_t		offset = 0;
		TimeStruct_t 	tm;
		
		
		if(file_name == NULL)
		{
			return FILE_FAILD;
		}
		//开始遍历历史文件要使用的id 和 key
		for(uint8_t i = file_num_start; i < file_num_end; i++)
		{	
			temp_file.file_id = i;
			
			if(FILE_SUCESS == mfs_read_times_ex(&temp_file,&file_tick))
			{
				//结果文件数据校验OK
				memset(buff ,0, 16);
				get_time(&tm,&file_tick);
				sprintf(buff, "%04d%02d%02d%02d%02d%02d", tm.year, tm.mon, tm.day, tm.hour, tm.min, tm.sec);			
				if (strcmp(file_name, buff) == 0) 
				{
					//找到目标文件 且结果文件校验通过
					find_file = 1;
					break;
				}
			}
			
		}
		//文件已经找到
		if(find_file)
		{
			log_debug("f2:%d %d\n",temp_file.file_id,temp_file.file_key);
			if(temp_file.file_id < BP_FILE_NUM)//血压文件
			{	
				file_size = sizeof(BP_File_head);
			}
			else
			{									//心电文件对波形也要校验
				file_size = sizeof(ECG_File_head);
                
                 for(uint8_t i = 0;i < 15;i++)
                {
                    temp_file.file_key = i;
                    ret = fds_read_data(temp_file,((uint8_t *)&m_ecg_data_buff)+offset,ECG_WAVE_BUFF_SEC_SIZE);
                    if(ret == FILE_SUCESS)
                    {
                        file_size 	+= ECG_WAVE_BUFF_SEC_SIZE;
                        offset 		+= ECG_WAVE_BUFF_SEC_SIZE;
                    }
                    else
                    {
                        log_debug("fail:%d\n",ret);
                        break;                        
                    }        
                }                				
			}			
			file->file_id 	= temp_file.file_id;
			file->file_key 	= RECORD_RESULT_KEY;
			file->file_size	= file_size;	
		}
		
		return file_size;
	}		
}

uint32_t mfs_wave_file_open(FileMini2_t file_mini,File_localmini_t* addr,uint16_t size)
{
	File_t 			temp_file;
	File_exit_t		file_exit;
	File_local_t 	temp_addr;	
	uint32_t 		fds_ret;
	
	memset(&temp_addr,0,sizeof(File_local_t));
	if((file_mini.file_id >= FILE_TOTAL_NUM) && (file_mini.file_id < BP_FILE_NUM))
	{
		LOGD("err id\n");
		return FILE_FAILD;
	}
	
	temp_file.file_id 	= file_mini.file_id;
	temp_file.file_key 	= file_mini.file_key;
	
	file_exit = get_file_location(temp_file,&temp_addr.old_addr);
	LOGD("find exit ? %d\n",(file_exit == FILE_FOUND));

	addr->old_page_index_data 	= temp_addr.old_addr.page_index_data;
	addr->old_data_addr			= temp_addr.old_addr.data_addr;

	
	//找到位置存储,同时判断该页的使用情况 如果写到结尾了 认为页满了 默认页为已使用
	fds_ret = find_location_for_data(&temp_addr.new_addr,size);
	
	if(fds_ret != FILE_SUCESS)
	{
		log_file_line();
		LOGD("err %d\n",fds_ret);
		return fds_ret;//可能是找不到位置
	}
	addr->new_page_index_data	= temp_addr.new_addr.page_index_data;
	addr->new_data_addr			= temp_addr.new_addr.data_addr;
	
	return FILE_SUCESS;	
}



uint32_t mfs_file_write(File_t *file,uint8_t* data)
{
	if(file->exit == FILE_FOUND)
	{
		//先标记原来的数据为准备删除
		LOGD("ready del..\n");
		if(set_file_status(file->addr.old_addr,FILE_DELTED_READY) != FILE_SUCESS)
		{
			//地址有问题 返回
			return FILE_FAILD;
		}		
	}
	write_file(*file,file->addr.new_addr,data);
	
	if(file->exit == FILE_FOUND)
	{
		//将标记的数据设置为已经删除
		LOGD("had del..\n");
		set_file_status(file->addr.old_addr,FILE_DELTED);
	}	
	
	//到这里存储完成 如果页头的为emthy状态要修改为已使用 如果....
	if(page_check(file->addr.new_addr.page_index_data) != PAGE_USED)//如果判断页的状态和实际上的不一样
	{
		page_set_status(file->addr.new_addr.page_index_data,PAGE_USED);
	}	
	return FILE_SUCESS;		
}



/*
	传入的file数组 读的个数 文件类型 

	//！！！不可在心电测量的时候调用！！！！
*/
uint32_t mfs_file_list_id(FileMini1_t *file,uint16_t file_cnt,uint8_t file_type)
{
	uint32_t 		file_tick;
	unsigned char 	file_num_end = 0;
	unsigned char 	file_num_start = 0;	
	File_t		  	temp_file;
	
	
	switch(file_type)
	{
		case MFS_BP_FILE: //血压数据文件
			file_num_end = BP_FILE_NUM;//BP_FILE_NUM;
			file_num_start = 0;
			break;
		case MFS_ECG_FILE: //心电数据文件
			file_num_end = FILE_TOTAL_NUM;
			file_num_start = BP_FILE_NUM;
			break;
		case MFS_ALL_FILE:
			file_num_start = 0;
			file_num_end = FILE_TOTAL_NUM;
		default :
			break;
	}

	uint8_t temp_cnt = 0;
	temp_file.file_key = RECORD_RESULT_KEY;
	for(uint8_t i = file_num_start; i < file_num_end; i++)
	{
		
		temp_file.file_id = i;
		
		if(temp_cnt == file_cnt)
		{
			break;
		}
		
		if(FILE_SUCESS == mfs_read_times_ex(&temp_file,&file_tick))
		{
			//结果文件数据校验OK
			 (file + temp_cnt)->measure_tick	= file_tick;
			 (file + temp_cnt)->file_id 		= i;
			 temp_cnt++;
		}		
	}

	return temp_cnt;
}

/*
	根据文件类型得到文件名字列表
*/
uint32_t mfs_file_list(char *list_buff,uint8_t file_type,uint8_t file_num)
{
	uint32_t 		file_tick;
	unsigned char 	file_num_end = 0;
	unsigned char 	file_num_start = 0;	
	File_t		  	temp_file;
	char 			buff[16];
	TimeStruct_t 	tm;
	
	switch(file_type)
	{
		case MFS_BP_FILE: //血压数据文件
			file_num_end = BP_FILE_NUM;//BP_FILE_NUM;
			file_num_start = 0;
			break;
		case MFS_ECG_FILE: //心电数据文件
			file_num_end = FILE_TOTAL_NUM;
			file_num_start = BP_FILE_NUM;
			break;
		case MFS_ALL_FILE://全部文件
			file_num_start = 0;
			file_num_end = FILE_TOTAL_NUM;
            break;
        case MFS_SPEC_FILE://指定的一个文件
			file_num_start = 0;
			file_num_end = FILE_TOTAL_NUM;            
            break;
		default :
			break;
	}
	
	uint8_t temp_cnt = 0;
	temp_file.file_key = RECORD_RESULT_KEY;
	for(uint8_t i = file_num_start; i < file_num_end; i++)
	{	
		temp_file.file_id = i;
		
		if(FILE_SUCESS == mfs_read_times_ex(&temp_file,&file_tick))
		{
			//结果文件数据校验OK
			memset(buff ,0, 16);
			get_time(&tm,&file_tick);
			sprintf(buff, "%04d%02d%02d%02d%02d%02d", tm.year, tm.mon, tm.day, tm.hour, tm.min, tm.sec);
			memcpy(&list_buff[temp_cnt*16], buff,16);			
			temp_cnt++;
            if(file_type == MFS_SPEC_FILE && temp_cnt == file_num)
            {
                break;
            }    
		}		
	}

	return temp_cnt;
}

/*
	读取文件列表并且根据时间顺序降序排序得到id表
*/
unsigned char file_list_sort(uint16_t *list)
{
	FileMini1_t *list_temp = NULL;
	unsigned char file_list_len = 0;
	list_temp = (FileMini1_t *)co_malloc(sizeof(FileMini1_t) * FILE_TOTAL_NUM);
	memset(list_temp,0,sizeof(FileMini1_t) * FILE_TOTAL_NUM);
	//统计有效文件
	file_list_len = mfs_file_list_id(list_temp,FILE_TOTAL_NUM,MFS_ALL_FILE);
	//排序
	FileMini1_t file_temp;
	for(int i=0;i<file_list_len;i++){                                           
		for(int j=0;j<file_list_len-i;j++){
			if(list_temp[j].measure_tick < list_temp[j+1].measure_tick){
				file_temp = list_temp[j];
				list_temp[j] = list_temp[j+1];
				list_temp[j+1] = file_temp;
			}
		}
	}
	for(int i = 0;i<file_list_len;i++){
		list[i] = list_temp[i].file_id;
	}	
	
	if(list_temp) {
		co_free(list_temp);
		list_temp = NULL;
	}
	return file_list_len;
}

/*根据接口协议读取血压结果文件*/
uint32_t read_bp_file_head(File_t fp ,BP_File_head *flie_head)
{
	uint32_t ret;
	BloodPressureResultSave flie_res;
	ret = fds_read_data(fp,(uint8_t *)&flie_res,sizeof(BloodPressureResultSave));
	flie_head->file_version = 1;
	flie_head->file_type = 1;
	flie_head->measuring_timestamp = flie_res.start_time;
	
	flie_head->status_code = flie_res.state_code;
	flie_head->systolic_pressure = flie_res.systolic_pressure;
	flie_head->diastolic_pressure = flie_res.diastolic_pressure;
	flie_head->mean_pressure = flie_res.mean_pressure;
	flie_head->medical_result = flie_res.state_code;
	flie_head->pulse_rate = flie_res.pulse_rate;
	memset(flie_head->reserved1, 0,sizeof(flie_head->reserved1));
	memset(flie_head->reserved2, 0,sizeof(flie_head->reserved2));
	
	return ret;
}

/*根据接口协议读取心电结果文件*/
uint32_t read_ecg_file_head(File_t fp ,ECG_File_head *flie_head)
{
	uint32_t ret;
	AnalysisResultSave flie_res;
	ret = fds_read_data(fp,(uint8_t *)&flie_res,sizeof(AnalysisResultSave));
	flie_head->file_version = 1;
	flie_head->file_type = 2;
	flie_head->measuring_timestamp = flie_res.start_time;
	
	flie_head->recording_time = 30;
	flie_head->result = flie_res.result;
	flie_head->hr = flie_res.hr;
	flie_head->qrs = flie_res.qrs;
	flie_head->pvcs = flie_res.pvcs;
	flie_head->qtc = flie_res.qtc;
	flie_head->is_connect_cable = flie_res.is_connect_cable;
	memset(flie_head->reserved1, 0,sizeof(flie_head->reserved1));
	memset(flie_head->reserved2, 0,sizeof(flie_head->reserved2));
	
	return ret;
}

/*
	读取指定波形文件的某一段,参考自原bp2工程
	cur_length：	当前需要读取的波形下标
	*length：		想要读取的波形长度
	
*/
uint32_t mfs_file_read_wave(File_t fp, uint8_t *data,uint32_t cur_length, uint32_t *length)
{
	uint16_t copy_len;
	uint8_t *pTemp = NULL;
    
	if(cur_length + *length > (WAVE_LEN*2))
	{
		copy_len = (WAVE_LEN*2) - cur_length;//剩下的长度
	}
	else
	{
		copy_len = *length;
	}
	if(copy_len <= 0)
	{
		*length = 0;
        pTemp = NULL;
		log_debug("err len %d\n",cur_length);
		return 0;
	}
    pTemp = (((uint8_t*)m_ecg_data_buff)+cur_length);
	memcpy(data,pTemp,copy_len);
	*length = copy_len;
    pTemp = NULL;
	log_debug("wave:%d %d\n",cur_length,copy_len);
}

/*
	擦除全部文件相关的flash扇区并且初始化flash头部
*/
void mfs_history_del_all(void)
{
	storage_reset();
}	

uint32_t mfs_file_read(File_t file_info,uint8_t* data,uint32_t buff_size)
{
	fds_read_data(file_info,data,buff_size);
}	

void mfs_init(void)
{
    uint32_t ret;
    uint32_t try = 0;
    //test_temp();
	storage_alloc_fun_init(mfs_memory_alloc,mfs_memory_free);
    do{
       ret = storage_init();
       try++;
    }while(ret && try < 10);
    log_debug("gc %d\n",try);    
}

