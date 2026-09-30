#include "sys.h"

void WFI_SET(void)
{
	__ASM volatile("wfi");
}
//关闭所有中断
void INTX_DISABLE(void)
{
	__ASM volatile("cpsid i");
}
//开启所有中断
void INTX_ENABLE(void)
{
	__ASM volatile("cpsie i");
}
//设置栈顶地址
//addr:栈顶地址
//AC6兼容: 使用 __attribute__((naked)) + GCC风格内联汇编替代 AC5 的 __asm 嵌入式汇编
__attribute__((naked)) void MSR_MSP(u32 addr)
{
	__asm volatile(
		"msr msp, r0 \n"	//set Main Stack value (r0 = addr, 第一个参数)
		"bx lr       \n"	//返回
	);
}


