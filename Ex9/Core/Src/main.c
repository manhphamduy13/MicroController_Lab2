/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Lab 2 - Ex7, Ex8, Ex9 (digital clock + LED matrix)
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ================= Dinh thoi quet ================= */
#define SEG_SCAN_TICKS       5U    // 5 x 2 ms = 10 ms
#define CLOCK_TICKS        500U    // 500 x 2 ms = 1 s
#define MATRIX_SCAN_TICKS   1U   // Số nhịp ngắt để chuyển cột matrix (ví dụ: 1 hoặc 2 ms)
static volatile uint16_t matrix_tick_count = 0;

static volatile uint8_t matrix_scan_flag = 0;
static volatile uint8_t segment_scan_flag = 0;
static volatile uint8_t clock_tick_flag = 0;
static volatile uint16_t segment_tick_count = 0;
static volatile uint16_t clock_tick_count = 0;

/* ================= LED 7 doan ================= */
// Common anode: bit = 0 -> sang. Bit0 = a ... bit6 = g
const uint8_t SEG_CODE[10] = {
    0x40, 0x79, 0x24, 0x30, 0x19,   // 0 1 2 3 4
    0x12, 0x02, 0x78, 0x00, 0x10    // 5 6 7 8 9
};

// PA6..PA9 dieu khien PNP high-side: base LOW = bat, base HIGH = tat
#define EN_ON   GPIO_PIN_RESET
#define EN_OFF  GPIO_PIN_SET

void display7SEG(int num) {
    GPIOB->ODR = (GPIOB->ODR & ~0x007F) | SEG_CODE[num];
}

const int MAX_LED = 4;
int index_led = 0;
int led_buffer[4] = {1, 2, 3, 4};

void update7SEG(int index) {
    // Tat ca 4 LED truoc khi doi du lieu de tranh bong mo
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9, EN_OFF);
    switch (index) {
        case 0:
            display7SEG(led_buffer[0]);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, EN_ON);
            break;
        case 1:
            display7SEG(led_buffer[1]);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, EN_ON);
            break;
        case 2:
            display7SEG(led_buffer[2]);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, EN_ON);
            break;
        case 3:
            display7SEG(led_buffer[3]);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, EN_ON);
            break;
        default:
            break;
    }
}

/* ================= Dong ho ================= */
// Bat dau tai 15:08:00. Neu bat dau o giay 50 thi chi sau 10 giay
// man hinh se doi tu 15:08 sang 15:09.
int hour = 15, minute = 8, second = 0;

void updateClockBuffer(void) {
    led_buffer[0] = hour / 10;
    led_buffer[1] = hour % 10;
    led_buffer[2] = minute / 10;
    led_buffer[3] = minute % 10;
}

/* ================= LED matrix ================= */
// Giu nguyen ket noi tren mach Proteus hien tai:
// PB8..PB15 -> ROW0..ROW7; ULN2803 -> COL0..COL7.
// PA2, PA3, PA10..PA15 chon tung COL thong qua ULN2803.
#define COL_ON   GPIO_PIN_SET
#define COL_OFF  GPIO_PIN_RESET

#define ENM_PORT GPIOA
#define ENM_ALL (GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_10 | GPIO_PIN_11 | \
                 GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15)

// Chu A chi co 3 mau hang khac nhau, nen gom cac hang giong nhau
// thanh mot pha quet. Duty cycle tang tu 1/8 len 1/3 va khung hinh
// Voi timer 2 ms: mot khung mat 6 ms, tuong duong 166.7 Hz.
const int MAX_LED_MATRIX = 3;
int index_led_matrix = 0;

// Tren symbol MATRIX-8X8-RED nay, moi ENM chon mot hang hien thi.
// matrix_buffer[row] chua 8 diem cua hang do; bit 0..7 la cac cot.
const uint8_t matrix_buffer[8] = {
    0x3C, // ..####..
    0x42, // .#....#.
    0x42, // .#....#.
    0x42, // .#....#.
    0x7E, // .######.
    0x42, // .#....#.
    0x42, // .#....#.
    0x42  // .#....#.
};

// Xuat du lieu ROW ma khong anh huong PB0..PB6 cua LED 7 doan.
static void setRowData(uint8_t data) {
    // Ghi truc tiep 8 bit cao, giu nguyen PB0..PB7 cua LED 7 doan.
    GPIOB->ODR = (GPIOB->ODR & 0x00FFU) | ((uint16_t)data << 8);
}

static void displayMatrixRows(uint16_t enPins, uint8_t data) {
    // Tat het cac hang truoc khi doi du lieu de tranh bong mo.
    HAL_GPIO_WritePin(ENM_PORT, ENM_ALL, COL_OFF);

    // Cap du lieu ROW.
    setRowData(data);

    // Cac hang co cung mau duoc bat dong thoi qua ULN2803.
    HAL_GPIO_WritePin(ENM_PORT, enPins, COL_ON);
}

void updateLEDMatrix(int index) {
    switch (index) {
        // Hang tren.
        case 0:
            displayMatrixRows(GPIO_PIN_2, matrix_buffer[0]);
            break;

        // ROW1, ROW2, ROW3, ROW5, ROW6, ROW7 deu co mau .#....#.
        case 1:
            displayMatrixRows(GPIO_PIN_3  | GPIO_PIN_10 | GPIO_PIN_11 |
                              GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15,
                              matrix_buffer[1]);
            break;

        // Thanh ngang giua.
        case 2:
            displayMatrixRows(GPIO_PIN_12, matrix_buffer[4]);
            break;

        default: break;
    }
}

// Cau hinh them cac chan cua LED matrix (khong can chinh trong CubeMX)
static void MX_Matrix_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    HAL_GPIO_WritePin(GPIOA, ENM_ALL, COL_OFF);
    HAL_GPIO_WritePin(GPIOB, 0xFF00, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = ENM_ALL;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 |
                          GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

static void clockTickOneSecond(void) {
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_4);

    second++;
    if (second >= 60) {
        second = 0;
        minute++;
    }
    if (minute >= 60) {
        minute = 0;
        hour++;
    }
    if (hour >= 24) {
        hour = 0;
    }
    updateClockBuffer();
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
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  MX_Matrix_GPIO_Init();
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9, EN_OFF);
  updateClockBuffer();
  update7SEG(index_led++);
  updateLEDMatrix(index_led_matrix++);
  HAL_TIM_Base_Start_IT(&htim2);
//  HAL_GPIO_WritePin(ENM_PORT, ENM_ALL, COL_OFF);       // tắt hết cột
//  HAL_GPIO_WritePin(ENM_PORT, GPIO_PIN_2, COL_ON);     // chỉ bật COL0 (PA2)
//  GPIOB->ODR |= 0xFF00;       // nếu bạn đang đảo ROW (dùng ~data) thì đổi thành: GPIOB->ODR &= ~0xFF00;
//  while (1);
  // TEST CỰC TÍNH - xóa sau khi test xong
//  while (1) {
//      for (int p = 0; p < 4; p++) {
//          HAL_GPIO_WritePin(ENM_PORT, ENM_ALL, (p < 2) ? GPIO_PIN_SET : GPIO_PIN_RESET);
//          if (p % 2 == 0) GPIOB->ODR |= 0xFF00;
//          else            GPIOB->ODR &= ~0xFF00;
//          HAL_Delay(3000);
//      }
//  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    if (matrix_scan_flag) {
        matrix_scan_flag = 0;
        updateLEDMatrix(index_led_matrix++);
        if (index_led_matrix >= MAX_LED_MATRIX) index_led_matrix = 0;
    }

    if (segment_scan_flag) {
        segment_scan_flag = 0;
        update7SEG(index_led++);
        if (index_led >= MAX_LED) index_led = 0;
    }

    if (clock_tick_flag) {
        clock_tick_flag = 0;
        clockTickOneSecond();
    }
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 799;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 19;        // 10 kHz / 20 = 500 Hz -> 2 ms
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_5|GPIO_PIN_10
                          |GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14
                          |GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_10
                          |GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14
                          |GPIO_PIN_15|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5
                          |GPIO_PIN_6|GPIO_PIN_8|GPIO_PIN_9, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA2 PA3 PA5 PA6
                           PA7 PA8 PA9 PA10
                           PA11 PA12 PA13 PA14
                           PA15 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_5|GPIO_PIN_6
                          |GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10
                          |GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14
                          |GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB1 PB2 PB10
                           PB11 PB12 PB13 PB14
                           PB15 PB3 PB4 PB5
                           PB6 PB8 PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_10
                          |GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14
                          |GPIO_PIN_15|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5
                          |GPIO_PIN_6|GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        // Thay vì bật cờ liên tục mỗi ms, ta cho phép quét matrix nhanh hơn tùy ý
        if (++matrix_tick_count >= MATRIX_SCAN_TICKS) {
            matrix_tick_count = 0;
            matrix_scan_flag = 1;
        }

        if (++segment_tick_count >= SEG_SCAN_TICKS) {
            segment_tick_count = 0;
            segment_scan_flag = 1;
        }

        if (++clock_tick_count >= CLOCK_TICKS) {
            clock_tick_count = 0;
            clock_tick_flag = 1;
        }
    }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
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
