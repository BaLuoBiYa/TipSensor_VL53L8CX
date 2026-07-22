#include "wrappers/uart.h"
#include <string.h>

/* ── UART_Init ────────────────────────────────────────────────────── */
void UART_Init(UART *uart, UART_HandleTypeDef *huart)
{
    uart->huart      = huart;
    uart->active_idx = 0;
    uart->ready_len  = 0;
    uart->tx_done    = true;
    memset(uart->rx_buf, 0, sizeof(uart->rx_buf));
    memset(uart->dma_tx_buf, 0, UART_DMA_TX_BUF_SIZE);
    HAL_UARTEx_ReceiveToIdle_DMA(huart, uart->rx_buf[0], UART_DMA_RX_BUF_SIZE);
    __HAL_DMA_DISABLE_IT(uart->huart->hdmarx, DMA_IT_HT);
}

/* ── UART_Send (阻塞模式) ────────────────────────────────────────── */
bool UART_Send(UART *uart, const uint8_t *data, uint16_t len)
{
    if (uart->huart->gState != HAL_UART_STATE_READY) {
        return false;
    }
    return HAL_UART_Transmit(uart->huart, (uint8_t *)data, len, HAL_MAX_DELAY) == HAL_OK;
}

/* ── UART_SendDMA (非阻塞) ───────────────────────────────────────── */
bool UART_SendDMA(UART *uart, const uint8_t *data, uint16_t len)
{
    if (uart->huart->gState != HAL_UART_STATE_READY) {
        return false;
    }
    if (len > UART_DMA_TX_BUF_SIZE) {
        return false;
    }

    memcpy(uart->dma_tx_buf, data, len);
    uart->tx_done = false;
    return HAL_UART_Transmit_DMA(uart->huart, uart->dma_tx_buf, len) == HAL_OK;
}

/* ── UART_IsTxDone ────────────────────────────────────────────────── */
bool UART_IsTxDone(UART *uart)
{
    return uart->tx_done;
}

/* ── UART_TxCpltISR (在 HAL_UART_TxCpltCallback 中调用) ─────────── */
void UART_TxCpltISR(UART *uart)
{
    uart->tx_done = true;
}

/* ── UART_GetRxData ───────────────────────────────────────────────── */
bool UART_GetRxData(UART *uart, uint8_t **data, uint16_t *len)
{
    if (uart->ready_len == 0) {
        return false;
    }

    __disable_irq();
    uint8_t ready_idx = uart->active_idx ^ 1; /* 非活跃缓冲即为就绪缓冲 */
    *data             = uart->rx_buf[ready_idx];
    *len              = uart->ready_len;
    uart->ready_len   = 0;
    __enable_irq();

    return true;
}

/* ── UART_RxIdleHandler (可复用, 从 HAL_UARTEx_RxEventCallback 调用) ── */
void UART_RxIdleHandler(UART *uart, uint16_t size)
{
    /* 记录当前 active 缓冲的就绪长度 */
    uart->ready_len = size;

    /* 翻转 active_idx 到另一个缓冲 */
    uart->active_idx ^= 1;

    /* 在新 active 缓冲上重启 DMA+空闲接收 */
    HAL_UARTEx_ReceiveToIdle_DMA(uart->huart,
                                 uart->rx_buf[uart->active_idx],
                                 UART_DMA_RX_BUF_SIZE);
}
