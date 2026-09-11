#include "svc_cache.h"
#include "dvc_w25q128.h"
#include "TaskHandle.h"
#include "cmsis_os2.h"
#include <stddef.h>
#include <string.h>

//The first 4 MiB is reserved by the existing log layout.
#define CACHE_FLASH_BASE  0x00400000UL  //Flash缓存起始地址（4MB处）
#define CACHE_FLASH_END   0x01000000UL  //Flash缓存结束地址（16MB处，总12MB空间）
#define CACHE_SECTOR_SIZE 4096UL        //Flash扇区大小（W25Q128的4KB/扇区）
#define CACHE_SLOT_SIZE   ((uint32_t)sizeof(SvcCacheRecord_t))  //单个记录大小
#define CACHE_SLOT_COUNT  ((CACHE_SECTOR_SIZE / CACHE_SLOT_SIZE))  //每扇区可容纳记录数
#define CACHE_SECTOR_COUNT ((CACHE_FLASH_END - CACHE_FLASH_BASE) / CACHE_SECTOR_SIZE)  //总扇区数

//缓存信息结构体（RAM侧）
//初始化时读写指针都指向起始地址，表示空缓存
CacheInfo_t cache_info =
{
    CACHE_FLASH_BASE, CACHE_FLASH_BASE, 0U, 0U, 0U, 0U
};
static uint32_t cache_peek_address = CACHE_FLASH_END;  //Peek操作的专用读指针

//计算CRC32校验值（标准算法）
//参数：data - 要计算的数据，length - 数据长度
//返回：CRC32校验值
static uint32_t SvcCache_Crc32(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFU;  //CRC初始值
    uint32_t i;
    uint8_t bit;

    for (i = 0U; i < length; i++)
    {
        crc ^= data[i];  //当前字节与CRC异或
        for (bit = 0U; bit < 8U; bit++)
        {
            //如果最低位为1，则进行多项式除法，否则右移一位
            crc = ((crc & 1U) != 0U) ?
                  ((crc >> 1U) ^ 0xEDB88320U) : (crc >> 1U);
        }
    }
    return ~crc;  //最终结果取反
}

//计算记录的CRC32校验值（从version字段到data字段）
//参数：record - 要计算的记录指针
//返回：CRC32校验值
static uint32_t SvcCache_RecordCrc(const SvcCacheRecord_t *record)
{
    //计算从version字段到crc32字段之前的数据段的CRC
    //offsetof计算字段偏移量，得出要计算的数据长度
    return SvcCache_Crc32((const uint8_t *)&record->version,
                          (uint32_t)(offsetof(SvcCacheRecord_t, crc32) -
                                     offsetof(SvcCacheRecord_t, version)));
}

//检查记录是否有效（完整且未被破坏）
//参数：record - 要检查的记录指针
//返回：1=有效记录，0=无效记录
static uint8_t SvcCache_RecordValid(const SvcCacheRecord_t *record)
{
    //检查条件：魔数正确、版本匹配、长度正确、提交标志正确、CRC校验通过
    return (record->magic == SVC_CACHE_MAGIC_VALID &&           //魔数必须为0xDEADBEEF
            record->version == SVC_CACHE_VERSION &&              //版本必须为当前版本
            record->payload_length == sizeof(GatewayData_t) &&   //数据长度必须正确
            record->commit == SVC_CACHE_COMMIT_VALID &&           //提交标志必须为0xA5
            record->crc32 == SvcCache_RecordCrc(record)) ? 1U : 0U;  //CRC校验必须通过
}

//检查记录是否为擦除态（全0xFF，表示从未写入）
//参数：record - 要检查的记录指针
//返回：1=擦除态（空记录），0=非擦除态（有数据）
static uint8_t SvcCache_RecordErased(const SvcCacheRecord_t *record)
{
    const uint8_t *bytes = (const uint8_t *)record;  //记录字节数组指针
    uint32_t i;

    //逐个字节检查，如果发现非0xFF字节则说明不是擦除态
    for (i = 0U; i < CACHE_SLOT_SIZE; i++)
    {
        if (bytes[i] != 0xFFU)
        {
            return 0U;  //发现非FF字节，不是擦除态
        }
    }
    return 1U;  //所有字节都是FF，是擦除态
}

//比较两个序列号的大小（处理32位回绕）
//参数：a, b - 要比较的两个序列号
//返回：1=a<b，0=a>=b
static uint8_t SvcCache_SequenceBefore(uint32_t a, uint32_t b)
{
    //将无符号比较转换为有符号比较，处理32位回绕问题
    //当a接近0xFFFFFFFF，b接近0时，a-b会溢出，但int32_t会正确处理
    return ((int32_t)(a - b) < 0) ? 1U : 0U;
}

static uint32_t SvcCache_SectorBase(uint32_t address)
{
    return CACHE_FLASH_BASE +
           (((address - CACHE_FLASH_BASE) / CACHE_SECTOR_SIZE) *
            CACHE_SECTOR_SIZE);
}

static uint32_t SvcCache_NextSlot(uint32_t address)
{
    uint32_t sector_offset = (address - CACHE_FLASH_BASE) % CACHE_SECTOR_SIZE;
    uint32_t next = address + CACHE_SLOT_SIZE;

    if ((sector_offset + CACHE_SLOT_SIZE) > CACHE_SECTOR_SIZE)
    {
        next = SvcCache_SectorBase(address) + CACHE_SECTOR_SIZE;
    }
    return (next >= CACHE_FLASH_END) ? CACHE_FLASH_BASE : next;
}

static void SvcCache_Lock(void)
{
    if (flashMutexHandle != NULL)
    {
        (void)osMutexAcquire(flashMutexHandle, osWaitForever);
    }
}

static void SvcCache_Unlock(void)
{
    if (flashMutexHandle != NULL)
    {
        (void)osMutexRelease(flashMutexHandle);
    }
}

static uint8_t SvcCache_FindEmpty(uint32_t start, uint32_t *address)
{
    SvcCacheRecord_t record;
    uint32_t current = start;
    uint32_t i;

    for (i = 0U; i < CACHE_SLOT_COUNT * CACHE_SECTOR_COUNT; i++)
    {
        SPI_FLASH_BufferRead((uint8_t *)&record, current, sizeof(record));
        if (SvcCache_RecordErased(&record) != 0U)
        {
            *address = current;
            return 1U;
        }
        current = SvcCache_NextSlot(current);
    }
    return 0U;
}

void SvcCache_Init(void)
{
    SvcCacheRecord_t record;
    uint32_t address = CACHE_FLASH_BASE;
    uint32_t oldest_address = CACHE_FLASH_END;
    uint32_t newest_address = CACHE_FLASH_END;
    uint32_t oldest_sequence = 0U;
    uint32_t newest_sequence = 0U;
    uint32_t empty_address = CACHE_FLASH_END;
    uint32_t i;

    SvcCache_Lock();
    memset(&cache_info, 0, sizeof(cache_info));
    cache_info.cache_read_address = CACHE_FLASH_BASE;
    cache_info.cache_write_address = CACHE_FLASH_BASE;
    cache_peek_address = CACHE_FLASH_END;

    for (i = 0U; i < CACHE_SLOT_COUNT * CACHE_SECTOR_COUNT; i++)
    {
        SPI_FLASH_BufferRead((uint8_t *)&record, address, sizeof(record));
        if (SvcCache_RecordValid(&record) != 0U)
        {
            cache_info.cache_count++;
            if ((oldest_address == CACHE_FLASH_END) ||
                SvcCache_SequenceBefore(record.sequence, oldest_sequence) != 0U)
            {
                oldest_sequence = record.sequence;
                oldest_address = address;
            }
            if ((newest_address == CACHE_FLASH_END) ||
                SvcCache_SequenceBefore(newest_sequence, record.sequence) != 0U)
            {
                newest_sequence = record.sequence;
                newest_address = address;
            }
        }
        else if (SvcCache_RecordErased(&record) != 0U)
        {
            /* 有效记录在 Flash 内连续追加，遇到第一个空槽即可停 */
            empty_address = address;
            break;
        }
        address = SvcCache_NextSlot(address);
    }

    if (oldest_address != CACHE_FLASH_END)
    {
        cache_info.cache_read_address = oldest_address;
        cache_info.next_sequence = newest_sequence + 1U;
    }
    if (empty_address != CACHE_FLASH_END)
    {
        cache_info.cache_write_address = empty_address;
    }
    else
    {
        cache_info.cache_write_address = CACHE_FLASH_END;
    }
    cache_info.initialized = 1U;
    SvcCache_Unlock();
}

uint8_t SvcCache_Add(const GatewayData_t *data)
{
    SvcCacheRecord_t record;
    SvcCacheRecord_t verify;
    uint32_t address;

    if ((data == NULL) || (cache_info.initialized == 0U))
    {
        return 0U;
    }

    SvcCache_Lock();
    if (SvcCache_FindEmpty(cache_info.cache_write_address, &address) == 0U)
    {
        cache_info.error_count++;
        SvcCache_Unlock();
        return 0U;
    }

    /* Only erase a sector after proving it has no valid record. */
    if ((address % CACHE_SECTOR_SIZE) == 0U)
    {
        uint32_t scan = address;
        uint8_t has_live = 0U;
        uint32_t slot;
        for (slot = 0U; slot < CACHE_SLOT_COUNT; slot++)
        {
            SPI_FLASH_BufferRead((uint8_t *)&verify, scan, sizeof(verify));
            if (SvcCache_RecordValid(&verify) != 0U)
            {
                has_live = 1U;
                break;
            }
            scan += CACHE_SLOT_SIZE;
        }
        if (has_live == 0U)
        {
            SPI_FLASH_SectorErase(address);
            SPI_FLASH_WaitForWriteEnd();
        }
    }

    memset(&record, 0xFF, sizeof(record));
    record.magic = SVC_CACHE_MAGIC_EMPTY;
    record.version = SVC_CACHE_VERSION;
    record.payload_length = sizeof(GatewayData_t);
    record.sequence = cache_info.next_sequence++;
    record.timestamp = data->timestamp;
    memcpy(&record.data, data, sizeof(record.data));
    record.crc32 = SvcCache_RecordCrc(&record);
    record.commit = SVC_CACHE_COMMIT_EMPTY;

    SPI_FLASH_BufferWrite((uint8_t *)&record, address, sizeof(record));
    SPI_FLASH_WaitForWriteEnd();
    /* Program the validity marker and commit byte only after the payload. */
    record.magic = SVC_CACHE_MAGIC_VALID;
    SPI_FLASH_BufferWrite((uint8_t *)&record.magic, address, sizeof(record.magic));
    record.commit = SVC_CACHE_COMMIT_VALID;
    SPI_FLASH_BufferWrite(&record.commit,
                          address + offsetof(SvcCacheRecord_t, commit), 1U);
    SPI_FLASH_WaitForWriteEnd();

    SPI_FLASH_BufferRead((uint8_t *)&verify, address, sizeof(verify));
    if (SvcCache_RecordValid(&verify) == 0U)
    {
        cache_info.error_count++;
        SvcCache_Unlock();
        return 0U;
    }
    cache_info.cache_count++;
    cache_info.cache_write_address = SvcCache_NextSlot(address);
    SvcCache_Unlock();
    return 1U;
}

uint8_t SvcCache_Peek(GatewayData_t *data, uint32_t *sequence)
{
    SvcCacheRecord_t record;
    uint32_t address;
    uint32_t i;
    uint8_t found = 0U;

    if ((data == NULL) || (cache_info.initialized == 0U))
    {
        return 0U;
    }

    /* 记录在 Flash 中按写入顺序连续排列，队首即最小序号。
       从 cache_read_address 顺序向前找到第一条有效记录即可，
       避免每次整片扫描 12MB（原实现导致回放 ~10s/条）。 */
    SvcCache_Lock();
    address = cache_info.cache_read_address;
    if (cache_info.cache_count != 0U)
    {
        for (i = 0U; i < CACHE_SLOT_COUNT * CACHE_SECTOR_COUNT; i++)
        {
            if (address >= CACHE_FLASH_END)
            {
                address = CACHE_FLASH_BASE; /* 环形兜底，正常不会走到 */
            }
            SPI_FLASH_BufferRead((uint8_t *)&record, address, sizeof(record));
            if (SvcCache_RecordValid(&record) != 0U)
            {
                memcpy(data, &record.data, sizeof(*data));
                cache_peek_address = address;
                if (sequence != NULL)
                {
                    *sequence = record.sequence;
                }
                found = 1U;
                break;
            }
            if (SvcCache_RecordErased(&record) != 0U)
            {
                break; /* 后面不可能再有有效记录 */
            }
            address = SvcCache_NextSlot(address);
        }
    }
    SvcCache_Unlock();
    return found;
}

uint8_t SvcCache_CommitPeek(void)
{
    uint8_t consumed = SVC_CACHE_COMMIT_CONSUMED;
    uint8_t result = 0U;

    SvcCache_Lock();
    if ((cache_info.cache_count != 0U) &&
        (cache_peek_address < CACHE_FLASH_END))
    {
        SPI_FLASH_BufferWrite(&consumed,
                              cache_peek_address + offsetof(SvcCacheRecord_t, commit), 1U);
        SPI_FLASH_WaitForWriteEnd();
        cache_info.cache_count--;
        cache_info.cache_read_address = SvcCache_NextSlot(cache_peek_address);
        if (cache_info.cache_count == 0U)
        {
            cache_info.cache_read_address = cache_info.cache_write_address;
        }
        cache_peek_address = CACHE_FLASH_END;
        result = 1U;
    }
    SvcCache_Unlock();
    return result;
}

uint8_t SvcCache_Get(GatewayData_t *data) { return SvcCache_Peek(data, NULL); }
uint8_t SvcCache_Remove(void) { return SvcCache_CommitPeek(); }

uint32_t SvcCache_Count(void)
{
    uint32_t count;
    SvcCache_Lock(); count = cache_info.cache_count; SvcCache_Unlock();
    return count;
}

uint8_t SvcCache_Clear(void)
{
    uint32_t address;
    SvcCache_Lock();
    for (address = CACHE_FLASH_BASE; address < CACHE_FLASH_END; address += CACHE_SECTOR_SIZE)
    {
        SPI_FLASH_SectorErase(address);
        SPI_FLASH_WaitForWriteEnd();
    }
    cache_info.cache_read_address = CACHE_FLASH_BASE;
    cache_info.cache_write_address = CACHE_FLASH_BASE;
    cache_info.cache_count = 0U;
    cache_info.next_sequence = 0U;
    cache_peek_address = CACHE_FLASH_END;
    SvcCache_Unlock();
    return 1U;
}

uint8_t SvcCache_IsEmpty(void) { return (SvcCache_Count() == 0U) ? 1U : 0U; }
void SvcCache_FlushToTransport(void) { }

uint32_t SvcCache_GetReadAddress(void) { return SvcCache_GetSnapshot().cache_read_address; }
uint32_t SvcCache_GetWriteAddress(void) { return SvcCache_GetSnapshot().cache_write_address; }

CacheInfo_t SvcCache_GetSnapshot(void)
{
    CacheInfo_t snapshot;
    SvcCache_Lock(); snapshot = cache_info; SvcCache_Unlock();
    return snapshot;
}
