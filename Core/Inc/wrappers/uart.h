#ifndef UART_H
#define UART_H

#include "main.h"
#include <stdbool.h>

#define UART_DMA_RX_BUF_SIZE 64
#define UART_DMA_TX_BUF_SIZE 720

typedef struct UART UART;

struct UART {
    UART_HandleTypeDef *huart;
    uint8_t rx_buf[2][UART_DMA_RX_BUF_SIZE]; /* 双缓冲: [0] 和 [1] */
    volatile uint8_t active_idx;             /* DMA 当前写入的缓冲索引 (ISR 写，用户读) */
    volatile uint16_t ready_len;             /* 就绪缓冲区的有效字节数 (ISR 写非零，用户清零) */
    uint8_t dma_tx_buf[UART_DMA_TX_BUF_SIZE];
    volatile bool tx_done; /* ISR 写 true，用户读/写 false */
};

void UART_Init(UART *uart, UART_HandleTypeDef *huart);
bool UART_Send(UART *uart, const uint8_t *data, uint16_t len);
bool UART_SendDMA(UART *uart, const uint8_t *data, uint16_t len);
bool UART_IsTxDone(UART *uart);
void UART_TxCpltISR(UART *uart);

/**
 * UART_GetRxData: 原子性获取就绪缓冲区指针和长度
 * @return true:  data/len 有效，调用者使用后缓冲区由下次调用覆盖
 *         false: 当前无新数据
 * @note   调用后 ready_len 清零，允许 ISR 覆盖该缓冲
 */
bool UART_GetRxData(UART *uart, uint8_t **data, uint16_t *len);

/**
 * UART_RxIdleHandler: 空闲中断处理（从 HAL_UARTEx_RxEventCallback 调用）
 * @param uart: UART 实例指针（持有 rx_event / rx_event_flag）
 * @param size: 本次接收的字节数
 * @note  在 ISR 上下文中调用，记录就绪数据、翻转缓冲、重启 DMA、发送事件
 */
void UART_RxIdleHandler(UART *uart, uint16_t size);

#endif /* UART_H */
