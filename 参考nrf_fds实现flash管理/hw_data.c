/*


调用sdk的读写内部flash的接口


*/

#include "rwip_config.h" // RW SW configuration
#include "arch.h"      // architectural platform definitions
#include "rwip.h"      // RW SW initialization
#include "peripheral.h"
#include "sysdump.h"

#include "hw_data.h"
#include "co_debug.h"

#if 1
#define LOGD log_debug
#else
#define LOGD
#endif
void hw_data_init(void)
{
	
}

uint32_t hw_data_erase(uint32_t addr,uint32_t size)
{
	if((addr+size) > USER_STORAGE_ADDR_END)
		return 0;
	sf_enable(HS_SF, 0);
	sf_erase(HS_SF, 0, addr, size);
	
	return 1;
}

uint32_t hw_data_write(uint32_t addr, const uint8_t *data, uint32_t size)
{
	if((addr+size) > USER_STORAGE_ADDR_END)
	{
		LOGD("write over %d %d\r\n",addr,size);
		return 0;
	}
			
	//addr = addr + (addr%4);
	//size = size + (size%4);//写地址4字节向后对齐 保证每次写入的长度
	//LOGD("aw:0x%x s:%d\r\n",addr,size);
	sf_enable(HS_SF, 0);
	sf_write(HS_SF, 0, addr, data, size);
	
	return size;
}

uint32_t hw_data_read(uint32_t addr, uint8_t *data, uint32_t size)
{
	//log_debug("r ar:0x%x s:0x%x\r\n",addr,size);
	if((addr+size) > USER_STORAGE_ADDR_END || !size)
		return 0;	
	
	//addr = addr + (addr%4);//写地址4字节向后对齐的如果读的起始地址没对齐可能读到偏移过去的字节
	
	sf_enable(HS_SF, 0);
	sf_read(HS_SF, 0, addr, data, size);
	
	return size;
}


