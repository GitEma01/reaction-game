/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Slave - Player Controller for Reaction Game with OLED
  *                   STM32F303 Discovery Board
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdio.h>
#include <string.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ssd1306.h"
#include "ssd1306_fonts.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
    SLAVE_IDLE,
    SLAVE_READY,
    SLAVE_BUTTON_PRESSED,
    SLAVE_SHOW_RESULT
} SlaveState_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define UART_RX_BUFFER_SIZE 64
#define LED_PIN GPIO_PIN_5          // LED esterno su PA5
#define BUZZER_PIN GPIO_PIN_6       // Buzzer su PA6
#define USER_BUTTON_PIN GPIO_PIN_0  // User button su PA0
#define BUZZER_DURATION 500         // Durata suono buzzer in ms
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
TIM_HandleTypeDef htim2;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
volatile SlaveState_t slave_state = SLAVE_IDLE;
volatile uint8_t button_pressed = 0;
volatile uint32_t button_press_time = 0;
volatile uint8_t game_active = 0;
volatile uint32_t buzzer_start_time = 0;
volatile uint8_t buzzer_active = 0;
volatile uint32_t go_received_time = 0;
volatile uint8_t show_result = 0;
volatile uint8_t player_won = 0;  // 1 = vittoria, 0 = sconfitta
volatile uint32_t result_start_time = 0;  // ✅ NUOVO: tempo inizio visualizzazione risultato

uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE];
volatile uint8_t uart_rx_index = 0;

char tx_buffer[64];
char display_buffer[32];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
void ProcessUARTCommand(uint8_t *buffer, uint8_t len);
void SendReactionTime(uint32_t timestamp);
uint32_t GetCurrentTime(void);
void ActivateBuzzer(void);
void DeactivateBuzzer(void);
void ResetSlave(void);
void ShowGameResult(uint8_t won);
void DisplayIdleScreen(void);
void DisplayReadyScreen(void);
void DisplayGoScreen(void);
void DisplayPressedScreen(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  // Inizializza timer per timestamp
  HAL_TIM_Base_Start(&htim2);

  // Inizializza LED e buzzer spenti
  HAL_GPIO_WritePin(GPIOA, LED_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, BUZZER_PIN, GPIO_PIN_RESET);

  // Inizializza display OLED
  HAL_Delay(100);
  ssd1306_Init();
  DisplayIdleScreen();

  // Abilita ricezione UART interrupt
  HAL_UART_Receive_IT(&huart2, &uart_rx_buffer[uart_rx_index], 1);

  // Stato iniziale
  slave_state = SLAVE_IDLE;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t current_time = GetCurrentTime();

    // Gestione buzzer con timeout
    if (buzzer_active && (current_time - buzzer_start_time >= BUZZER_DURATION)) {
        DeactivateBuzzer();
    }

    switch(slave_state) {
        case SLAVE_IDLE:
            // Aspetta comando di reset/start dal master
            break;

        case SLAVE_READY:
            // Aspetta pressione del user button
            if (HAL_GPIO_ReadPin(GPIOA, USER_BUTTON_PIN) == GPIO_PIN_SET && !button_pressed) {
                button_pressed = 1;
                button_press_time = current_time;
                slave_state = SLAVE_BUTTON_PRESSED;

                // Attiva LED e buzzer
                HAL_GPIO_WritePin(GPIOA, LED_PIN, GPIO_PIN_SET);
                ActivateBuzzer();

                // Mostra messaggio di pressione
                DisplayPressedScreen();

                // Invia tempo di reazione al master
                SendReactionTime(button_press_time);
            }
            break;

        case SLAVE_BUTTON_PRESSED:
            // Aspetta rilascio del pulsante e comando di reset
            if (HAL_GPIO_ReadPin(GPIOA, USER_BUTTON_PIN) == GPIO_PIN_RESET) {
                button_pressed = 0;
            }
            break;

        case SLAVE_SHOW_RESULT:
            // Mostra risultato per 5 secondi poi torna a idle
            // ✅ CORREZIONE: Usa result_start_time invece di HAL_Delay
            if (current_time - result_start_time >= 5000) {
                slave_state = SLAVE_IDLE;
                DisplayIdleScreen();
            }
            break;
    }

    HAL_Delay(5); // Piccolo delay per ridurre carico CPU
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
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x2000090E;  // 400kHz @ 8MHz per STM32F3
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

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
  htim2.Init.Prescaler = 7999;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4294967295;
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
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

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
  HAL_GPIO_WritePin(GPIOA, LED_PIN|BUZZER_PIN, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA0 - User Button */
  GPIO_InitStruct.Pin = USER_BUTTON_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PA5 PA6 - LED e Buzzer */
  GPIO_InitStruct.Pin = LED_PIN|BUZZER_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB6 PB7 - I2C1 SCL/SDA per OLED */
  GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;      // Open-drain per I2C
  GPIO_InitStruct.Pull = GPIO_PULLUP;          // Pull-up interno
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;   // AF4 per I2C1 su F303
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */

/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void ProcessUARTCommand(uint8_t *buffer, uint8_t len) {
    // Comando di reset per iniziare nuovo gioco
    if (strstr((char*)buffer, "RESET") != NULL) {
        ResetSlave();
        slave_state = SLAVE_READY;
        game_active = 1;

        // Mostra schermo di gioco pronto
        DisplayReadyScreen();
    }

    // Comando GO - il LED si è acceso
    if (strstr((char*)buffer, "GO") != NULL) {
        go_received_time = GetCurrentTime();

        // Mostra schermo GO
        DisplayGoScreen();
    }

    // Comando WIN - questo slave ha vinto
    if (strstr((char*)buffer, "WIN") != NULL) {
        ShowGameResult(1);  // 1 = vittoria
        slave_state = SLAVE_SHOW_RESULT;
    }

    // Comando LOSE - questo slave ha perso
    if (strstr((char*)buffer, "LOSE") != NULL) {
        ShowGameResult(0);  // 0 = sconfitta
        slave_state = SLAVE_SHOW_RESULT;
    }
}

void SendReactionTime(uint32_t button_timestamp) {
    // Calcola tempo di reazione dal comando GO
    uint32_t reaction_time = button_timestamp - go_received_time;
    sprintf(tx_buffer, "TIME:%lu\r\n", reaction_time);
    HAL_UART_Transmit(&huart2, (uint8_t*)tx_buffer, strlen(tx_buffer), 100);
}

uint32_t GetCurrentTime(void) {
    return __HAL_TIM_GET_COUNTER(&htim2);
}

void ActivateBuzzer(void) {
    HAL_GPIO_WritePin(GPIOA, BUZZER_PIN, GPIO_PIN_SET);
    buzzer_start_time = GetCurrentTime();
    buzzer_active = 1;
}

void DeactivateBuzzer(void) {
    HAL_GPIO_WritePin(GPIOA, BUZZER_PIN, GPIO_PIN_RESET);
    buzzer_active = 0;
}

void ResetSlave(void) {
    // Reset variabili di stato
    button_pressed = 0;
    button_press_time = 0;
    game_active = 0;
    go_received_time = 0;
    show_result = 0;
    player_won = 0;
    result_start_time = 0; // ✅ NUOVO: reset tempo risultato

    // Spegni LED e buzzer
    HAL_GPIO_WritePin(GPIOA, LED_PIN, GPIO_PIN_RESET);
    DeactivateBuzzer();

    slave_state = SLAVE_IDLE;
}

// Funzione per mostrare risultato del gioco
void ShowGameResult(uint8_t won) {
    ssd1306_Fill(Black);

    if (won) {
        // Vittoria
        ssd1306_SetCursor(15, 15);
        ssd1306_WriteString("HAI", Font_11x18, White);
        ssd1306_SetCursor(10, 35);
        ssd1306_WriteString("VINTO!", Font_11x18, White);

        // ✅ CORREZIONE: Buzzer breve per vittoria (già gestito dal timeout nel main loop)
        ActivateBuzzer();
    } else {
        // Sconfitta
        ssd1306_SetCursor(15, 15);
        ssd1306_WriteString("HAI", Font_11x18, White);
        ssd1306_SetCursor(10, 35);
        ssd1306_WriteString("PERSO!", Font_11x18, White);
    }

    ssd1306_UpdateScreen();
    show_result = 1;
    player_won = won;
    result_start_time = GetCurrentTime(); // ✅ NUOVO: salva momento inizio risultato
}

// Schermo idle
void DisplayIdleScreen(void) {
    ssd1306_Fill(Black);
    ssd1306_SetCursor(0, 15);
    ssd1306_WriteString("REACTION", Font_11x18, White);
    ssd1306_SetCursor(0, 35);
    ssd1306_WriteString("GAME", Font_11x18, White);
    ssd1306_SetCursor(0, 55);
    ssd1306_WriteString("Waiting...", Font_7x10, White);
    ssd1306_UpdateScreen();
}

// Schermo pronto
void DisplayReadyScreen(void) {
    ssd1306_Fill(Black);
    ssd1306_SetCursor(10, 15);
    ssd1306_WriteString("READY!", Font_11x18, White);
    ssd1306_SetCursor(0, 40);
    ssd1306_WriteString("Wait for LED...", Font_7x10, White);
    ssd1306_UpdateScreen();
}

// Schermo GO
void DisplayGoScreen(void) {
    ssd1306_Fill(Black);
    ssd1306_SetCursor(30, 25);
    ssd1306_WriteString("GO!", Font_11x18, White);
    ssd1306_UpdateScreen();
}

// Schermo pulsante premuto
void DisplayPressedScreen(void) {
    ssd1306_Fill(Black);
    ssd1306_SetCursor(20, 25);
    ssd1306_WriteString("PRESSED!", Font_11x18, White);
    ssd1306_UpdateScreen();
}

// Callback UART per ricezione comandi dal master
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        uart_rx_index++;
        if (uart_rx_index >= UART_RX_BUFFER_SIZE - 1 ||
            uart_rx_buffer[uart_rx_index-1] == '\n') {
            uart_rx_buffer[uart_rx_index] = '\0';
            ProcessUARTCommand(uart_rx_buffer, uart_rx_index);
            uart_rx_index = 0;
        }
        HAL_UART_Receive_IT(&huart2, &uart_rx_buffer[uart_rx_index], 1);
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
    // Lampeggia LED in caso di errore
    HAL_GPIO_WritePin(GPIOA, LED_PIN, GPIO_PIN_SET);
    HAL_Delay(200);
    HAL_GPIO_WritePin(GPIOA, LED_PIN, GPIO_PIN_RESET);
    HAL_Delay(200);
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
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
