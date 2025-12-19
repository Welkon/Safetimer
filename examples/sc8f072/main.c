/*-------------------------------------------
Project: MultiTimer Demo
Version: V1.0
Date: 2025.12.11
-------------------------------------------*/

#include "safetimer_single.h"
extern void init_timer0(void);
extern void init_system(void);
int main(void) {
  asm("nop");
  asm("clrwdt");

  init_system();
  init_timer0();

  while (1) {
    asm("clrwdt");
    safetimer_process();
  }
}