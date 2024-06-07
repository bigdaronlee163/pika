#!/bin/bash

# 定义Redis服务器地址和端口
#REDIS_HOST="127.0.0.1"
#REDIS_PORT="6379"
REDIS_HOST="172.17.0.2"
REDIS_PORT="9221"
REDIS_CLI="/pika/redis/src/redis-cli"

# 检查是否传入至少一个参数
if [ "$#" -lt 1 ]; then
    echo "Usage: $0 {string|list|set|hash|zset|hyperlog|bitmap|geospatial} [...]"
    exit 1
fi

# 定义要写入的数据
CHAR_LIST=("a" "b" "c" "d" "e")
NUM=5
HASH_FIELDS=("field1" "field2" "field3" "field4" "field5")
GEO_LOCATIONS=("13.361389 38.115556" "15.087269 37.502669")

# 函数：向字符串写入数据
write_to_string() {
    REDIS_KEY="mystring"
    for CHAR in "${CHAR_LIST[@]}"; do
        $REDIS_CLI -h $REDIS_HOST -p $REDIS_PORT set ${REDIS_KEY}${CHAR} $CHAR
        echo "$CHAR written to string ${REDIS_KEY}${CHAR}"
    done
    echo "Character list has been written to Redis strings with key prefix $REDIS_KEY"
}

# 函数：向列表写入数据
write_to_list() {
    REDIS_KEY="mylist"
    for CHAR in "${CHAR_LIST[@]}"; do
        $REDIS_CLI -h $REDIS_HOST -p $REDIS_PORT rpush $REDIS_KEY $CHAR
        echo "$CHAR written to list $REDIS_KEY"
    done
    echo "Character list has been written to Redis list $REDIS_KEY"
}

# 函数：向集合写入数据
write_to_set() {
    REDIS_KEY="myset"
    for CHAR in "${CHAR_LIST[@]}"; do
        $REDIS_CLI -h $REDIS_HOST -p $REDIS_PORT sadd $REDIS_KEY $CHAR
        echo "$CHAR written to set $REDIS_KEY"
    done
    echo "Character list has been written to Redis set $REDIS_KEY"
}

# 函数：向散列写入数据
write_to_hash() {
    REDIS_KEY="myhash"
    for i in `seq $NUM`; do
        FIELD=${HASH_FIELDS[$((i-1))]}
        CHAR=${CHAR_LIST[$((i-1))]}
        $REDIS_CLI -h $REDIS_HOST -p $REDIS_PORT hset $REDIS_KEY $FIELD $CHAR
        echo "field $FIELD with value $CHAR written to hash $REDIS_KEY"
    done
    echo "Fields and values have been written to Redis hash $REDIS_KEY"
}

# 函数：向有序集合写入数据
write_to_zset() {
    REDIS_KEY="myzset"
    for i in `seq $NUM`; do
        CHAR=${CHAR_LIST[$((i-1))]}
        $REDIS_CLI -h $REDIS_HOST -p $REDIS_PORT zadd $REDIS_KEY $i $CHAR
        echo "$CHAR with score $i written to sorted set $REDIS_KEY"
    done
    echo "Character list has been written to Redis sorted set $REDIS_KEY"
}

# 函数：向HyperLogLog写入数据
write_to_hyperloglog() {
    REDIS_KEY="myhyperloglog"
    for CHAR in "${CHAR_LIST[@]}"; do
        $REDIS_CLI -h $REDIS_HOST -p $REDIS_PORT pfadd $REDIS_KEY $CHAR
        echo "$CHAR written to HyperLogLog $REDIS_KEY"
    done
    echo "Character list has been written to Redis HyperLogLog $REDIS_KEY"
}

# 函数：向位图写入数据
write_to_bitmap() {
    REDIS_KEY="mybitmap"
    for i in `seq $NUM`; do
        $REDIS_CLI -h $REDIS_HOST -p $REDIS_PORT setbit $REDIS_KEY $i 1
        echo "bit $i set to 1 in bitmap $REDIS_KEY"
    done
    echo "Bits have been set in Redis bitmap $REDIS_KEY"
}

# 函数：向地理位置写入数据
write_to_geospatial() {
    REDIS_KEY="mygeo"
    i=1
    for LOC in "${GEO_LOCATIONS[@]}"; do
        $REDIS_CLI -h $REDIS_HOST -p $REDIS_PORT geoadd $REDIS_KEY $LOC place${i}
        echo "Location $LOC added as place${i} in geospatial $REDIS_KEY"
        i=$((i + 1))
    done
    echo "Geospatial locations have been written to Redis geospatial $REDIS_KEY"
}

# 遍历所有传入的参数并调用相应的函数
for arg in "$@"; do
    case $arg in
        string)
            write_to_string
            ;;
        list)
            write_to_list
            ;;
        set)
            write_to_set
            ;;
        hash)
            write_to_hash
            ;;
        zset)
            write_to_zset
            ;;
        hyperlog)
            write_to_hyperloglog
            ;;
        bitmap)
            write_to_bitmap
            ;;
        geospatial)
            write_to_geospatial
            ;;
        *)
            echo "Invalid option: $arg. Usage: $0 {string|list|set|hash|zset|hyperlog|bitmap|geospatial} [...]"
            exit 1
            ;;
    esac
done
