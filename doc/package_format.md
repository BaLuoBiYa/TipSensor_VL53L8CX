# 主机-->传感器
## 传感器配置帧
| 字节范围 | bit | 字段 | 类型 | 长度 | 说明 | 写入原始值 |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| 0-1 | - | header.magic | uint8_t[2] | 2 Byte | 控制帧包头 | 固定为 0xFE 0xEE |
| 2 | 0-3 | header.id | bit field | 4 bit | ID | 0~14；15 表示广播，无返回 |
| 2 | 4-7 | header.mode | bit field | 4 bit | 工作模式 | 0；传感器配置 |
| 3 | 0	| payload.resolution | bit field | 1 bit | 分辨率 | 0，4x4；1，8x8 |
| 3 | 1 | payload.target_order | bit field | 1 bit | 过滤器顺序 | 0，最强回波；1，最近回波 |
| 3 | 2-7 | payload.framerate | uint8_t | 6 bit | 帧率 | 4x4模式下，最大为60hz；8x8模式下，最大为15hz |
| 4-5 | - | payload.sharpener | uint16_t | 2 Byte | 锐化器系数 | 0~99 |
| 6-7 | - | crc16 | uint16_t | 2 Byte | CRC16_CCITT 校验值 | 覆盖前 6 Byte |

## 传感器请求帧
| 字节范围 | bit | 字段 | 类型 | 长度 | 说明 | 写入原始值 |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| 0-1 | - | header.magic | uint8_t[2] | 2 Byte | 控制帧包头 | 固定为 0xFE 0xEE |
| 2 | 0-3 | header.id | bit field | 4 bit | ID | 0~14；15 表示广播，无返回 |
| 2 | 4-7 | header.mode | bit field | 4 bit | 工作模式 | 1；要求返回数据 |
| 3-4 | - | crc16 | uint16_t | 2 Byte | CRC16_CCITT 校验值 | 覆盖前 3 Byte |

# 传感器-->主机
## 4x4精简返回帧
| 字节范围 | bit | 字段 | 类型 | 长度 | 说明 | 写入原始值 |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| 0-1 | - | header.magic | uint8_t[2] | 2 Byte | 控制帧包头 | 固定为 0xFE 0xEE |
| 2 | 0-3 | header.id | bit field | 4 bit | ID | 0~14；15 表示广播，无返回 |
| 2 | 4-7 | header.mode | bit field | 4 bit | 工作模式 | 2；4x4精简返回帧 |
| 3-34 | - | payload.distance | uint16_t[16] | 32 Byte | 深度图 | 单位mm |
| 35-36 | - | crc16 | uint16_t | 2 Byte | CRC16_CCITT 校验值 | 覆盖前 35 Byte |

## 4x4完整返回帧
| 字节范围 | bit | 字段 | 类型 | 长度 | 说明 | 写入原始值 |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| 0-1 | - | header.magic | uint8_t[2] | 2 Byte | 控制帧包头 | 固定为 0xFE 0xEE |
| 2 | 0-3 | header.id | bit field | 4 bit | ID | 0~14；15 表示广播，无返回 |
| 2 | 4-7 | header.mode | bit field | 4 bit | 工作模式 | 3；4x4完整返回帧 |
| 3 | - | payload.silicon_temp_degc | int8_t | 1 Byte | 芯片内部温度 | 单位℃ |
| 4-179 | - | payload.zones[16] | zone_data_t[16] | 176 Byte | 16 个 zone 的完整数据 | 每个 zone 11 Byte，结构见下表 |
| 180-181 | - | crc16 | uint16_t | 2 Byte | CRC16_CCITT 校验值 | 覆盖前 180 Byte |

## 8x8精简返回帧
| 字节范围 | bit | 字段 | 类型 | 长度 | 说明 | 写入原始值 |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| 0-1 | - | header.magic | uint8_t[2] | 2 Byte | 控制帧包头 | 固定为 0xFE 0xEE |
| 2 | 0-3 | header.id | bit field | 4 bit | ID | 0~14；15 表示广播，无返回 |
| 2 | 4-7 | header.mode | bit field | 4 bit | 工作模式 | 4；8x8精简返回帧 |
| 3-130 | - | payload.distance | uint16_t[64] | 128 Byte | 深度图 | 单位mm |
| 131-132 | - | crc16 | uint16_t | 2 Byte | CRC16_CCITT 校验值 | 覆盖前 131 Byte |

## 8x8完整返回帧
| 字节范围 | bit | 字段 | 类型 | 长度 | 说明 | 写入原始值 |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| 0-1 | - | header.magic | uint8_t[2] | 2 Byte | 控制帧包头 | 固定为 0xFE 0xEE |
| 2 | 0-3 | header.id | bit field | 4 bit | ID | 0~14；15 表示广播，无返回 |
| 2 | 4-7 | header.mode | bit field | 4 bit | 工作模式 | 5；8x8完整返回帧 |
| 3 | - | payload.silicon_temp_degc | int8_t | 1 Byte | 芯片内部温度 | 单位℃ |
| 4-707 | - | payload.zones[64] | zone_data_t[64] | 704 Byte | 64 个 zone 的完整数据 | 每个 zone 11 Byte |
| 708-709 | - | crc16 | uint16_t | 2 Byte | CRC16_CCITT 校验值 | 覆盖前 708 Byte |

### zone_data_t 
| zone 内偏移 | 字段 | 类型 | 长度 | 说明 |
| ---- | ---- | ---- | ---- | ---- |
| 0 | nb_target_detected | uint8_t | 1 Byte | 该 zone 检测到的有效目标数 |
| 1-4 | signal_per_spad | uint32_t | 4 Byte | 回波信号强度，单位 kcps/spad |
| 5-6 | range_sigma_mm | uint16_t | 2 Byte | 测距标准差，单位 mm |
| 7-8 | distance_mm | int16_t | 2 Byte | 测量距离，单位 mm |
| 9 | reflectance | uint8_t | 1 Byte | 估计反射率，单位 % |
| 10 | target_status | uint8_t | 1 Byte | 测量有效性状态 |

