/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Master - Reaction Game Controller (2 Players)
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ssd1306.h"
#include "ssd1306_fonts.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
    GAME_IDLE,
    GAME_WAITING,
    GAME_LED_ON,
    GAME_FINISHED,
    GAME_FALSE_START
} GameState_t;

typedef struct {
    uint32_t timestamp;
    uint8_t player_id;
    uint8_t valid;
} ReactionTime_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define UART_RX_BUFFER_SIZE 64
#define LED_START_PIN GPIO_PIN_1  // LED di start su PA1
#define MAX_WAIT_TIME 10000       // 10 secondi massimo
#define MIN_WAIT_TIME 1000        // 1 secondo minimo
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
TIM_HandleTypeDef htim2;
UART_HandleTypeDef huart1;  // Per Slave 1
UART_HandleTypeDef huart6;  // Per Slave 2

/* USER CODE BEGIN PV */
volatile GameState_t game_state = GAME_IDLE;
volatile uint32_t game_start_time = 0;
volatile uint32_t led_on_time = 0;
volatile uint32_t random_delay = 0;

ReactionTime_t player1_time = {0, 1, 0};
ReactionTime_t player2_time = {0, 2, 0};

uint8_t uart1_rx_buffer[UART_RX_BUFFER_SIZE];
uint8_t uart6_rx_buffer[UART_RX_BUFFER_SIZE];
volatile uint8_t uart1_rx_index = 0;
volatile uint8_t uart6_rx_index = 0;

char display_buffer[32];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART6_UART_Init(void);
/* USER CODE BEGIN PFP */
void StartNewGame(void);
void ProcessUARTMessage(uint8_t *buffer, uint8_t len, uint8_t player_id);
void UpdateDisplay(void);
void GenerateRandomDelay(void);
uint32_t GetCurrentTime(void);
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
  MX_USART1_UART_Init();
  MX_USART6_UART_Init();
  /* USER CODE BEGIN 2 */

  // Inizializza timer per timestamp
  HAL_TIM_Base_Start(&htim2);

  // Test LED integrato per indicare avvio
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
  HAL_Delay(500);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  // Inizializza display OLED
  HAL_Delay(100);
  ssd1306_Init();
  ssd1306_Fill(Black);
  ssd1306_SetCursor(0, 0);
  ssd1306_WriteString("REACTION", Font_11x18, White);     // ✅ CORRETTO
  ssd1306_SetCursor(15, 20);
  ssd1306_WriteString("GAME", Font_11x18, White);         // ✅ NUOVA RIGA
  ssd1306_SetCursor(0, 40);
  ssd1306_WriteString("Press USER button", Font_6x8, White);
  ssd1306_SetCursor(0, 55);
  ssd1306_WriteString("to start", Font_6x8, White);
  ssd1306_UpdateScreen();

  // Inizializza LED di start spento
  HAL_GPIO_WritePin(GPIOA, LED_START_PIN, GPIO_PIN_RESET);

  // Abilita ricezione UART interrupt per entrambi i slave
  HAL_UART_Receive_IT(&huart1, &uart1_rx_buffer[uart1_rx_index], 1);
  HAL_UART_Receive_IT(&huart6, &uart6_rx_buffer[uart6_rx_index], 1);

  // Seed per random (usa timer)
  srand(HAL_GetTick());

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t current_time = GetCurrentTime();

    switch(game_state) {
        case GAME_IDLE:
            // Aspetta pressione user button per iniziare
            if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) {
                StartNewGame();
                HAL_Delay(300); // Debounce
            }
            break;

        case GAME_WAITING:
            // Aspetta il delay casuale
            if (current_time - game_start_time >= random_delay) {
                // Accendi LED di start
                HAL_GPIO_WritePin(GPIOA, LED_START_PIN, GPIO_PIN_SET);
                led_on_time = current_time;  // Salva il momento di accensione LED
                game_state = GAME_LED_ON;

                // Invia comando GO a entrambi i slave
                uint8_t go_cmd[] = "GO\r\n";
                HAL_UART_Transmit(&huart1, go_cmd, strlen((char*)go_cmd), 100);
                HAL_UART_Transmit(&huart6, go_cmd, strlen((char*)go_cmd), 100);

                // Aggiorna display
                ssd1306_Fill(Black);
                ssd1306_SetCursor(30, 20);
                ssd1306_WriteString("GO!", Font_11x18, White);
                ssd1306_UpdateScreen();
            }
            break;

        case GAME_LED_ON:
            // Aspetta reazioni dei giocatori o timeout
            if (player1_time.valid && player2_time.valid) {
                // Entrambi hanno reagito
                game_state = GAME_FINISHED;
            } else if (current_time - led_on_time > 5000) {
                // Timeout 5 secondi - finisce anche se solo uno ha reagito
                game_state = GAME_FINISHED;
            }
            break;

        case GAME_FINISHED:
            // Mostra risultati
            UpdateDisplay();

            // Aspetta 5 secondi poi torna al menu
            HAL_Delay(5000);
            game_state = GAME_IDLE;

            // Reset display
            ssd1306_Fill(Black);
            ssd1306_SetCursor(0, 0);
            ssd1306_WriteString("REACTION", Font_11x18, White);     // ✅ CORRETTO
            ssd1306_SetCursor(15, 20);
            ssd1306_WriteString("GAME", Font_11x18, White);         // ✅ NUOVA RIGA
            ssd1306_SetCursor(0, 40);
            ssd1306_WriteString("Press USER button", Font_6x8, White);
            ssd1306_SetCursor(0, 55);
            ssd1306_WriteString("to start", Font_6x8, White);
            ssd1306_UpdateScreen();
            break;

        case GAME_FALSE_START:
            // Gestisce partenza falsa
            HAL_Delay(3000);
            game_state = GAME_IDLE;
            break;
    }

    HAL_Delay(10);
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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

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
  hi2c1.Init.ClockSpeed = 400000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
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
  htim2.Init.Prescaler = 15999;
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
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART6_UART_Init(void)
{

  /* USER CODE BEGIN USART6_Init 0 */

  /* USER CODE END USART6_Init 0 */

  /* USER CODE BEGIN USART6_Init 1 */

  /* USER CODE END USART6_Init 1 */
  huart6.Instance = USART6;
  huart6.Init.BaudRate = 115200;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart6) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART6_Init 2 */

  /* USER CODE END USART6_Init 2 */

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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1|GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA1 PA5 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */

/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void StartNewGame(void) {
    // Reset variabili di gioco per entrambi i giocatori
    player1_time.valid = 0;
    player1_time.timestamp = 0;
    player2_time.valid = 0;
    player2_time.timestamp = 0;

    // Spegni LED
    HAL_GPIO_WritePin(GPIOA, LED_START_PIN, GPIO_PIN_RESET);

    // Genera delay casuale
    GenerateRandomDelay();

    game_start_time = GetCurrentTime();
    game_state = GAME_WAITING;

    // Aggiorna display
    ssd1306_Fill(Black);
    ssd1306_SetCursor(0, 10);
    ssd1306_WriteString("GET", Font_11x18, White);
    ssd1306_SetCursor(0, 35);
    ssd1306_WriteString("READY...", Font_11x18, White);
    sprintf(display_buffer, "Wait %lu sec", random_delay/1000);
    ssd1306_SetCursor(0, 55);
    ssd1306_WriteString(display_buffer, Font_6x8, White);
    ssd1306_UpdateScreen();

    // Invia comando di reset a entrambi i slave
    uint8_t reset_cmd[] = "RESET\r\n";
    HAL_UART_Transmit(&huart1, reset_cmd, strlen((char*)reset_cmd), 100);
    HAL_UART_Transmit(&huart6, reset_cmd, strlen((char*)reset_cmd), 100);
}

void ProcessUARTMessage(uint8_t *buffer, uint8_t len, uint8_t player_id) {
    // Cerca timestamp nel messaggio formato "TIME:12345"
    char *time_str = strstr((char*)buffer, "TIME:");
    if (time_str != NULL) {
        uint32_t reaction_time_ms = atoi(time_str + 5);  // Tempo in millisecondi dallo Slave

        // Verifica se è partenza falsa
        if (game_state != GAME_LED_ON) {
            game_state = GAME_FALSE_START;
            ssd1306_Fill(Black);
            ssd1306_SetCursor(0, 10);
            ssd1306_WriteString("FALSE", Font_11x18, White);
            ssd1306_SetCursor(0, 35);
            sprintf(display_buffer, "START P%d!", player_id);
            ssd1306_WriteString(display_buffer, Font_11x18, White);
            ssd1306_UpdateScreen();
            return;
        }

        // Il tempo di reazione è già corretto dallo Slave
        if (player_id == 1 && !player1_time.valid) {
            player1_time.timestamp = reaction_time_ms;  // Usa direttamente il tempo dello Slave
            player1_time.valid = 1;
        } else if (player_id == 2 && !player2_time.valid) {
            player2_time.timestamp = reaction_time_ms;  // Usa direttamente il tempo dello Slave
            player2_time.valid = 1;
        }
    }
}

void UpdateDisplay(void) {
    // Spegni LED
    HAL_GPIO_WritePin(GPIOA, LED_START_PIN, GPIO_PIN_RESET);

    ssd1306_Fill(Black);

    if (player1_time.valid || player2_time.valid) {
        ssd1306_SetCursor(0, 0);
        ssd1306_WriteString("RESULTS:", Font_7x10, White);

        // Player 1
        if (player1_time.valid) {
            uint32_t seconds = player1_time.timestamp / 1000;
            uint32_t milliseconds = player1_time.timestamp % 1000;
            sprintf(display_buffer, "P1: %lu.%03lu sec", seconds, milliseconds);
            ssd1306_SetCursor(0, 15);
            ssd1306_WriteString(display_buffer, Font_7x10, White);
        } else {
            ssd1306_SetCursor(0, 15);
            ssd1306_WriteString("P1: NO PRESS", Font_7x10, White);
        }

        // Player 2
        if (player2_time.valid) {
            uint32_t seconds = player2_time.timestamp / 1000;
            uint32_t milliseconds = player2_time.timestamp % 1000;
            sprintf(display_buffer, "P2: %lu.%03lu sec", seconds, milliseconds);
            ssd1306_SetCursor(0, 30);
            ssd1306_WriteString(display_buffer, Font_7x10, White);
        } else {
            ssd1306_SetCursor(0, 30);
            ssd1306_WriteString("P2: NO PRESS", Font_7x10, White);
        }

        // Determina vincitore e invia comandi agli slave
        if (player1_time.valid && player2_time.valid) {
            if (player1_time.timestamp < player2_time.timestamp) {
                // Player 1 vince
                ssd1306_SetCursor(0, 50);
                ssd1306_WriteString("WINNER: P1!", Font_7x10, White);

                // Invia risultati agli slave
                uint8_t win_cmd[] = "WIN\r\n";
                uint8_t lose_cmd[] = "LOSE\r\n";
                HAL_UART_Transmit(&huart1, win_cmd, strlen((char*)win_cmd), 100);   // Slave 1 vince
                HAL_UART_Transmit(&huart6, lose_cmd, strlen((char*)lose_cmd), 100); // Slave 2 perde

            } else if (player2_time.timestamp < player1_time.timestamp) {
                // Player 2 vince
                ssd1306_SetCursor(0, 50);
                ssd1306_WriteString("WINNER: P2!", Font_7x10, White);

                // Invia risultati agli slave
                uint8_t win_cmd[] = "WIN\r\n";
                uint8_t lose_cmd[] = "LOSE\r\n";
                HAL_UART_Transmit(&huart1, lose_cmd, strlen((char*)lose_cmd), 100); // Slave 1 perde
                HAL_UART_Transmit(&huart6, win_cmd, strlen((char*)win_cmd), 100);   // Slave 2 vince

            } else {
                // Pareggio - entrambi perdono tecnicamente
                ssd1306_SetCursor(0, 50);
                ssd1306_WriteString("TIE!", Font_7x10, White);

                // In caso di pareggio, entrambi "perdono"
                uint8_t lose_cmd[] = "LOSE\r\n";
                HAL_UART_Transmit(&huart1, lose_cmd, strlen((char*)lose_cmd), 100);
                HAL_UART_Transmit(&huart6, lose_cmd, strlen((char*)lose_cmd), 100);
            }
        } else if (player1_time.valid) {
            // Solo Player 1 ha premuto
            ssd1306_SetCursor(0, 50);
            ssd1306_WriteString("WINNER: P1!", Font_7x10, White);

            uint8_t win_cmd[] = "WIN\r\n";
            uint8_t lose_cmd[] = "LOSE\r\n";
            HAL_UART_Transmit(&huart1, win_cmd, strlen((char*)win_cmd), 100);   // Slave 1 vince
            HAL_UART_Transmit(&huart6, lose_cmd, strlen((char*)lose_cmd), 100); // Slave 2 perde

        } else if (player2_time.valid) {
            // Solo Player 2 ha premuto
            ssd1306_SetCursor(0, 50);
            ssd1306_WriteString("WINNER: P2!", Font_7x10, White);

            uint8_t win_cmd[] = "WIN\r\n";
            uint8_t lose_cmd[] = "LOSE\r\n";
            HAL_UART_Transmit(&huart1, lose_cmd, strlen((char*)lose_cmd), 100); // Slave 1 perde
            HAL_UART_Transmit(&huart6, win_cmd, strlen((char*)win_cmd), 100);   // Slave 2 vince
        }
    } else {
        // Nessuno ha premuto
        ssd1306_SetCursor(0, 20);
        ssd1306_WriteString("NO RESPONSE", Font_11x18, White);
        ssd1306_SetCursor(0, 45);
        ssd1306_WriteString("FROM BOTH!", Font_7x10, White);

        // Entrambi perdono
        uint8_t lose_cmd[] = "LOSE\r\n";
        HAL_UART_Transmit(&huart1, lose_cmd, strlen((char*)lose_cmd), 100);
        HAL_UART_Transmit(&huart6, lose_cmd, strlen((char*)lose_cmd), 100);
    }

    ssd1306_UpdateScreen();
}

void GenerateRandomDelay(void) {
    random_delay = MIN_WAIT_TIME + (rand() % (MAX_WAIT_TIME - MIN_WAIT_TIME));
}

uint32_t GetCurrentTime(void) {
    return __HAL_TIM_GET_COUNTER(&htim2);
}

// Callback UART per ricezione dati dai slave
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        // Player 1
        uart1_rx_index++;
        if (uart1_rx_index >= UART_RX_BUFFER_SIZE - 1 ||
            uart1_rx_buffer[uart1_rx_index-1] == '\n') {
            uart1_rx_buffer[uart1_rx_index] = '\0';
            ProcessUARTMessage(uart1_rx_buffer, uart1_rx_index, 1);
            uart1_rx_index = 0;
        }
        HAL_UART_Receive_IT(&huart1, &uart1_rx_buffer[uart1_rx_index], 1);
    }

    if (huart->Instance == USART6) {
        // Player 2
        uart6_rx_index++;
        if (uart6_rx_index >= UART_RX_BUFFER_SIZE - 1 ||
            uart6_rx_buffer[uart6_rx_index-1] == '\n') {
            uart6_rx_buffer[uart6_rx_index] = '\0';
            ProcessUARTMessage(uart6_rx_buffer, uart6_rx_index, 2);
            uart6_rx_index = 0;
        }
        HAL_UART_Receive_IT(&huart6, &uart6_rx_buffer[uart6_rx_index], 1);
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
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
    HAL_Delay(200);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
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
