#ifndef __mapreduce_h__
#define __mapreduce_h__

// Different function pointer types used by MR
// MapReduce使用的不同的函数指针类型
typedef char *(*Getter)(char *key, int partition_number);					// 从分区用键获取值
typedef void (*Mapper)(char *file_name);									// map 函数
typedef void (*Reducer)(char *key, Getter get_func, int partition_number);	// reduce 函数
typedef unsigned long (*Partitioner)(char *key, int num_partitions);		// 分区函数

// External functions: these are what you must define
// 需要实现的外部函数

// MapReduce 键值发射
void MR_Emit(char *key, char *value);

// MapReduce 默认哈希分区
unsigned long MR_DefaultHashPartition(char *key, int num_partitions);

/**
 * 接收:
 * - 给定程序的命令行参数
 * - 指向Map函数的指针
 * - MapReduce库创建的线程数量
 * - 指向Reduce函数的指针
 * - reducer数量
 * - 指向分区函数的指针
*/
void MR_Run(int argc, char *argv[], 
	    Mapper map, int num_mappers, 
	    Reducer reduce, int num_reducers, 
	    Partitioner partition);

#endif // __mapreduce_h__
