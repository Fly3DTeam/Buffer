#include <Arduino.h>
#include "buffer.h"

extern "C"
{
    static void remap_sram(void)
    {
        __DSB();
        /* 先清零 MEM_MODE 位，再设置为 0x03 (SRAM mapped at 0x00000000) */
        SYSCFG->CFGR1 = (SYSCFG->CFGR1 & ~SYSCFG_CFGR1_MEM_MODE_Msk) |
                        (3U << SYSCFG_CFGR1_MEM_MODE_Pos);
        __DSB();
        __ISB();
    }

    static void copy_vector_table(void)
    {
        memcpy((void *)0x20000000, (void *)0x08002000, 192);
    }

    void CustomInit(void)
    {
        RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
        __DSB();

        // //   /* 复制向量表到 SRAM */
        copy_vector_table();

        // //   /* SRAM 重映射到 0x00000000 */
        remap_sram();
        // #endif

        //   /* 向量表重映射完成后再启用中断 */
        __enable_irq();
    }
}

void setup() {
    Serial.begin(115200);
    Serial.dtr(false);
    buffer_init();

}

void loop() {
    buffer_loop();
    // Serial.println("loop() is running");
    // delay(1000);
}

