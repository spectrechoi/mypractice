#ifndef __HW_DATA_H_
#define __HW_DATA_H_

#ifdef __cplusplus
extern "C"
{
#endif
#include <stdint.h>
#include "rwip_config.h" 
#include "rwapp_config.h" 	
	
#define USER_STORAGE_PAGESIZE	4096												//扇区大小
#if BLE_APP_NORDIC_DFU
#include "nordic_dfu.h"
#define USER_STORAGE_ADDR_START	 	(NEW_APP_BASE_ADDR2+MAX_APP_SIZE)				//可用于存储的起始地址
#define USER_STORAGE_SIZE	 		(USER_STORAGE_ADDR_END - USER_STORAGE_ADDR_START - USER_STORAGE_PAGESIZE)		//减去一个扇区 最后一个扇区先预留 后面可能拿来打log用	
#if (((NEW_APP_BASE_ADDR1+MAX_APP_SIZE) != NEW_APP_BASE_ADDR2)||((USER_STORAGE_ADDR_START + USER_STORAGE_SIZE)>=NEW_PATCH_BASE_ADDR2))
//如果设置的地址有问题要报错	
#error "application addr error!"
#endif	

#define USER_STORAGE_ADDR_END NEW_PATCH_BASE_ADDR1
	
#else		
//还没加onmi ota的方式地址

	
#endif
	
	
extern void hw_data_init(void);
extern uint32_t hw_data_erase(uint32_t addr,uint32_t size);
extern uint32_t hw_data_write(uint32_t addr, const uint8_t *data, uint32_t length);
extern uint32_t hw_data_read(uint32_t addr, uint8_t *data, uint32_t length);
	
	
#ifdef __cplusplus
}
#endif


#endif

