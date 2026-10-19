/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "crc.h"
#include "dma.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "drivers/parser.h"
#include "drivers/sensor.h"
#include "drivers/shared_buf.h"
#include "drivers/w25qxx.h"
#include "drivers/ws2812.h"
#include "platform.h"
#include "vl53l8cx_api.h"
#include "wrappers/uart.h"
#include <stdint.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DEVICE_ID 0
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
UART rs485;
W25Qxx w25qxx;
WS2812 led;

VL53L8CX_Configuration tof;
uint8_t isAlive, resolution, dataReady;
int32_t status;

Frame_Config_t savedConfig;
bool hasConfig; /* Flash 中是否有有效配置（供配置回滚使用） */
Sensor_Buffer_t sensorBuf;

static Pixel red = {
    .index = 0,
    .r = 255,
    .g = 0,
    .b = 0};

static Pixel green = {
    .index = 0,
    .r = 0,
    .g = 255,
    .b = 0};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static int32_t Tof_ApplyConfig(const Frame_Config_t *cfg)
{
    int32_t rtn;
    rtn = vl53l8cx_set_resolution(&tof,
                                  cfg->payload.bits.resolution ? VL53L8CX_RESOLUTION_8X8
                                                               : VL53L8CX_RESOLUTION_4X4);
    rtn |= vl53l8cx_set_ranging_frequency_hz(&tof,
                                             cfg->payload.bits.framerate == 0 ? 1 : cfg->payload.bits.framerate);
    rtn |= vl53l8cx_set_target_order(&tof,
                                     cfg->payload.bits.target_order ? VL53L8CX_TARGET_ORDER_CLOSEST
                                                                    : VL53L8CX_TARGET_ORDER_STRONGEST);
    rtn |= vl53l8cx_set_sharpener_percent(&tof, cfg->sharpener);

    return rtn;
}

/**
 * @brief 处理传感器 INT 数据就绪标志，阻塞读取一帧数据到缓冲
 */
static void ProcessSensorData(void)
{
    if (sensorBuf.data_ready)
    {
        sensorBuf.data_ready = false;
        SensorBuffer_Collect(&sensorBuf, &tof);
    }
}

/**
 * @brief 校验接收帧：magic、长度、ID，均匹配才返回 true
 */
static bool HandleFrameFilter(const uint8_t *data, uint16_t len,
                              uint8_t *id, Frame_Mode_t *mode)
{
    if (!Frame_Parse_Header(data, id, mode))
    {
        return false; /* magic 不匹配 */
    }
    if (!Frame_ValidateSize(len, *mode))
    {
        return false; /* 长度不匹配 */
    }
    if (*id != DEVICE_ID)
    {
        return false; /* id 不匹配 */
    }
    return true;
}

/**
 * @brief 回滚到上次保存的配置并重启 ranging（应用/重启失败时调用）
 */
static void RevertAndRestart(void)
{
    if (hasConfig)
    {
        Tof_ApplyConfig(&savedConfig);
    }
    vl53l8cx_start_ranging(&tof);
}

/**
 * @brief 处理配置帧（mode=0）：应用新配置，失败回滚，成功后持久化并回执 echo
 */
static void HandleConfigMode(const uint8_t *data)
{
    Frame_Config_t cfg;

    if (!Frame_Parse_Config(data, &cfg))
    {
        return; /* 配置解析失败，丢弃 */
    }

    /* 先停止测距，再应用新配置 */
    int32_t apply_status = vl53l8cx_stop_ranging(&tof);
    apply_status |= Tof_ApplyConfig(&cfg);
    if (apply_status != VL53L8CX_STATUS_OK)
    {
        /* 应用失败：回滚旧配置并重启 */
        RevertAndRestart();
        return;
    }

    /* 应用成功，重启 ranging 使新配置生效 */
    int32_t start_status = vl53l8cx_start_ranging(&tof);
    if (start_status != VL53L8CX_STATUS_OK)
    {
        /* 重启失败：回滚旧配置再试一次 */
        RevertAndRestart();
        return;
    }

    /* 更新本地保存的配置（供后续回滚使用） */
    savedConfig = cfg;
    hasConfig = true;

    /* 持久化 + 回读 echo（保存失败不回执，主机超时判定） */
    if (Config_SaveToFlash(&cfg, &w25qxx))
    {
        uint8_t echo[sizeof(Frame_Config_t)];
        uint32_t echoLen = Frame_Pack_Config(&cfg, echo, DEVICE_ID);
        UART_SendDMA(&rs485, echo, echoLen);
    }
}

/**
 * @brief 处理请求帧（mode=1）：打包最新一帧传感器数据回传
 */
static void HandleRequestMode(const uint8_t *data)
{
    Frame_Request_t req;

    if (!Frame_Parse_Request(data, &req))
    {
        return; /* 请求解析失败，丢弃 */
    }

    uint32_t txLen = SensorBuffer_PackResponse(&sensorBuf, g_shared.pack.raw, DEVICE_ID);
    if (txLen > 0)
    {
        UART_SendDMA(&rs485, g_shared.pack.raw, txLen);
    }
}

/**
 * @brief UART 帧主分发：校验帧头后按 mode 分派到各处理函数
 */
static void ProcessUartFrame(void)
{
    uint8_t *data;
    uint16_t len;
    uint8_t id;
    Frame_Mode_t mode;

    if (!UART_GetRxData(&rs485, &data, &len))
    {
        return; /* 无新数据 */
    }
    if (!HandleFrameFilter(data, len, &id, &mode))
    {
        return; /* magic/长度/ID 校验失败，丢弃 */
    }

    switch (mode)
    {
    case FRAME_MODE_CONFIG:
        HandleConfigMode(data);
        break;
    case FRAME_MODE_REQUEST:
        HandleRequestMode(data);
        break;
    default:
        break; /* 未知 mode，丢弃 */
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI1_Init();
  MX_USART2_UART_Init();
  MX_CRC_Init();
  MX_TIM16_Init();
  /* USER CODE BEGIN 2 */

    UART_Init(&rs485, &huart2);
    W25Qxx_Init(&w25qxx, &hspi1, FLASH_CS_GPIO_Port, FLASH_CS_Pin);
    SensorBuffer_Init(&sensorBuf);
    WS2812_Init(&led, &htim16);
    WS2812_SetPixel(&led, &red);
    WS2812_Show(&led);

    /* 从 W25Q Flash 加载持久化配置 */
    hasConfig = Config_LoadFromFlash(&savedConfig, &w25qxx);

    /* Flash 配置参数越界（旧固件/CRC 撞车写入）时回退默认值，避免上电即死 */
    if (hasConfig && !Config_Validate(&savedConfig))
    {
        hasConfig = false;
    }

    /* Register external firmware read callback */
    tof.platform.read_fw = VL53L8CX_FW_Read;
    /* Reset sensor */
    VL53L8CX_Reset_Sensor(&(tof.platform));
    /* Check if sensor is alive */
    status = vl53l8cx_is_alive(&tof, &isAlive);
    if (!isAlive || status)
    {
        Error_Handler();
    }

    /* Initialize sensor (loads FW from external flash via callback) */
    status = vl53l8cx_init(&tof);
    if (status != VL53L8CX_STATUS_OK)
    {
        Error_Handler();
    }

    /* 应用配置：Flash 有有效配置则用 Flash，否则用默认值 */
    if (hasConfig)
    {
        status = Tof_ApplyConfig(&savedConfig);
    }
    else
    {
        Frame_Config_t defaults = {
            .payload.raw = 0,
            .sharpener = 10,
        };
        defaults.payload.bits.framerate = 60;
        status = Tof_ApplyConfig(&defaults);
    }

    status |= vl53l8cx_set_ranging_mode(&tof,
                                        VL53L8CX_RANGING_MODE_CONTINUOUS);

    if (status == VL53L8CX_STATUS_OK)
    {
        status = vl53l8cx_start_ranging(&tof);
    }
    else
    {
        Error_Handler();
    }

    WS2812_SetPixel(&led, &green);
    WS2812_Show(&led);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    while (1)
    {
        __WFI();

        /* 传感器 INT 触发 → 主循环完成 SPI 读取 */
        ProcessSensorData();

        /* UART 帧接收 → 校验并分发处理 */
        ProcessUartFrame();

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV2;
  RCC_OscInitStruct.PLL.PLLN = 16;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV3;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
    /* 红灯指示错误状态；WS2812 初始化完成前 led.htim==NULL，跳过亮灯 */
    if (led.htim != NULL)
    {
        WS2812_SetPixel(&led, &red);
        WS2812_Show(&led);
    }
    __disable_irq();
    while (1)
    {
    }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
