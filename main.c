#include "main.h"
#include <stdio.h>
#include <string.h>

/* ======================== GLOBAL VARIABLES ======================== */
#define MAX_BUFFER_SIZE 30

UART_HandleTypeDef huart2;
ADC_HandleTypeDef hadc1;

/* ---------------- UART BUFFER ---------------- */
uint8_t temp = 0;
uint8_t buffer[MAX_BUFFER_SIZE];
uint8_t index_buffer = 0;
uint8_t buffer_flag = 0;

/* ---------------- FSM FLAGS ---------------- */
uint8_t command_flag = 0;
uint8_t ok_flag = 0;
uint32_t ADC_value = 0;
char uart_str[50];

/* ======================== FUNCTION DECLARATION ======================== */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);

/* ======================== UART INTERRUPT ======================== */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
    if(huart->Instance == USART2){
        buffer[index_buffer++] = temp;
        if(index_buffer >= MAX_BUFFER_SIZE) index_buffer = 0;
        buffer_flag = 1;
        HAL_UART_Receive_IT(&huart2, &temp, 1);
    }
}

/* ======================== COMMAND PARSER FSM ======================== */
void command_parser_fsm(){
    static uint8_t state = 0;

    for(uint8_t i = 0; i < index_buffer; i++){
        uint8_t c = buffer[i];
        switch(state){
            case 0:
                if(c == '!') state = 1;
                break;
            case 1:
                if(c == 'R') state = 2;
                else if(c == 'O') state = 5;
                else state = 0;
                break;
            case 2:
                if(c == 'S') state = 3;
                else state = 0;
                break;
            case 3:
                if(c == 'T') state = 4;
                else state = 0;
                break;
            case 4:
                if(c == '#'){
                    command_flag = 1;   // Detected !RST#
                }
                state = 0;
                break;
            case 5:
                if(c == 'K') state = 6;
                else state = 0;
                break;
            case 6:
                if(c == '#'){
                    ok_flag = 1;        // Detected !OK#
                }
                state = 0;
                break;
            default:
                state = 0;
                break;
        }
    }
    index_buffer = 0;
}

/* ======================== UART COMMUNICATION FSM ======================== */
void uart_communication_fsm(){
    static uint8_t state = 0;
    static uint32_t timer = 0;

    switch(state){
        case 0:
            if(command_flag){
                command_flag = 0;
                ADC_value = HAL_ADC_GetValue(&hadc1);
                sprintf(uart_str, "!ADC=%lu#\r\n", ADC_value);
                HAL_UART_Transmit(&huart2, (uint8_t*)uart_str, strlen(uart_str), 100);
                timer = HAL_GetTick();
                state = 1;
            }
            break;

        case 1: // Waiting for !OK#
            if(ok_flag){
                ok_flag = 0;
                state = 0;
            }
            if(HAL_GetTick() - timer >= 3000){  // resend after 3s
                sprintf(uart_str, "!ADC=%lu#\r\n", ADC_value);
                HAL_UART_Transmit(&huart2, (uint8_t*)uart_str, strlen(uart_str), 100);
                timer = HAL_GetTick();
            }
            break;
    }
}

/* ======================== MAIN FUNCTION ======================== */
int main(void){
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_ADC1_Init();

    HAL_UART_Receive_IT(&huart2, &temp, 1);
    HAL_ADC_Start(&hadc1);

    while (1){
        if(buffer_flag){
            command_parser_fsm();
            buffer_flag = 0;
        }
        uart_communication_fsm();

        // Blink LED mỗi 0.5s
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        HAL_Delay(500);
    }
}

/* ======================== PERIPHERAL INITIALIZATION ======================== */
static void MX_USART2_UART_Init(void){
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 9600;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if(HAL_UART_Init(&huart2) != HAL_OK) Error_Handler();
}

static void MX_ADC1_Init(void){
    ADC_ChannelConfTypeDef sConfig = {0};
    hadc1.Instance = ADC1;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode = ENABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    if(HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler();

    sConfig.Channel = ADC_CHANNEL_0;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

static void MX_GPIO_Init(void){
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/* ======================== SYSTEM CLOCK ======================== */
void SystemClock_Config(void){
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();


    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

void Error_Handler(void){
    __disable_irq();
    while (1){}
}

