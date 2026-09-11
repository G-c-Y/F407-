#ifndef SVC_CACHE_H
#define SVC_CACHE_H

#include <stdint.h>
#include "svc_data.h"

#ifdef __cplusplus
extern "C" {
#endif

//记录有效性标记：
//   VALID    正常可回放记录（magic=0xDEADBEEF，commit=0xA5）
//   EMPTY    槽位从未写入/整片擦除态（可写入，magic=0xFFFFFFFF）
//   CONSUMED 已回放成功（仅commit字段使用，commit=0x00）
#define SVC_CACHE_MAGIC_VALID    0xDEADBEEFU    //有效记录魔数
#define SVC_CACHE_MAGIC_EMPTY    0xFFFFFFFFU    //空记录标志（全FF）
#define SVC_CACHE_VERSION        1U             //记录格式版本，升级不兼容时靠它区分
#define SVC_CACHE_COMMIT_VALID   0xA5U          //数据写完整后再把commit置为该值
#define SVC_CACHE_COMMIT_EMPTY   0xFFU          //commit初始值（未写入）
#define SVC_CACHE_COMMIT_CONSUMED 0x00U         //回放成功后把commit清0表示已消费

//每条离线数据的Flash记录格式（固定大小，写入不超过一页）
//magic/commit只在整条payload+CRC写好后才补写，用于掉电一致性：
//  上电扫描只接受magic+version+长度+CRC+commit全部正确的记录，
//  因此半写记录不会被误当成有效数据
typedef struct
{
    uint32_t magic;           //有效记录魔数（0xDEADBEEF表示有效）
    uint16_t version;         //格式版本（用于升级兼容性）
    uint16_t payload_length;  //实际数据长度（应等于sizeof(GatewayData_t)）
    uint32_t sequence;        //采集序号，供接收端去重/排序
    uint32_t timestamp;       //采集时刻（UTC时间戳）
    GatewayData_t data;       //传感器数据快照（温度、湿度、光照）
    uint32_t crc32;           //对version..data段的CRC32校验
    uint8_t commit;           //提交标志：FF→A5表示写完整，→00表示已消费
    uint8_t reserved[3];      //对齐填充，保持记录大小固定（便于地址计算）
} SvcCacheRecord_t;

//缓存运行时元数据（RAM侧；真实队列状态靠上电扫描Flash重建）
typedef struct
{
    uint32_t cache_read_address;   //队首读指针（下一条待回放槽位地址）
    uint32_t cache_write_address;  //写指针（下一条可写入的擦除槽位地址）
    uint32_t cache_count;          //当前有效记录数（Peek可读的记录数）
    uint32_t next_sequence;        //下一条写入的序号（自动递增）
    uint32_t error_count;          //累计错误计数（满、校验失败等）
    uint8_t initialized;           //是否已完成上电扫描（初始化标志）
} CacheInfo_t;

//缓存初始化：扫描Flash并重建缓存元数据
void SvcCache_Init(void);
//添加数据：将新数据写入缓存（成功返回1，失败返回0）
uint8_t SvcCache_Add(const GatewayData_t *data);
//预读数据：读取下一条待回放的数据但不删除（不改变读指针）
uint8_t SvcCache_Peek(GatewayData_t *data, uint32_t *sequence);
//提交预读：删除Peek读取的最后一条数据（Peek后调用，确认发送成功）
uint8_t SvcCache_CommitPeek(void);
//获取并删除：读取并删除队首数据（返回数据并移动读指针）
uint8_t SvcCache_Get(GatewayData_t *data);
//删除队首：只删除数据不返回（兼容接口，实际使用CommitPeek）
uint8_t SvcCache_Remove(void);
//获取当前缓存记录数
uint32_t SvcCache_Count(void);
//清空缓存：擦除整个Flash缓存区域（耗时操作）
uint8_t SvcCache_Clear(void);
//检查缓存是否为空
uint8_t SvcCache_IsEmpty(void);
//批量发送（已弃用，保留接口）
void SvcCache_FlushToTransport(void);
//获取当前读指针地址（用于调试和状态监控）
uint32_t SvcCache_GetReadAddress(void);
//获取当前写指针地址（用于调试和状态监控）
uint32_t SvcCache_GetWriteAddress(void);
//获取缓存快照：返回完整的缓存元数据（供UI等模块使用）
CacheInfo_t SvcCache_GetSnapshot(void);

#ifdef __cplusplus
}
#endif

#endif /* SVC_CACHE_H */
