#define STM32F030x6
#define F_OSC   8000000UL
#define F_CPU   48000000UL
#define ADR(reg) GET_ADR(In_regs, reg)

#include "init_clock.h"
#include "periph_rcc.h"
#include "modbus_slave.h"

extern "C" void init_clock() { init_clock<F_OSC, F_CPU>(); }

using DI1  = mcu::PA9 ;  using DO1  = mcu::PB0 ;
using DI2  = mcu::PA8 ;  using DO2  = mcu::PA7 ;
using DI3  = mcu::PB15;  using DO3  = mcu::PA6 ;
using DI4  = mcu::PB14;  using DO4  = mcu::PA5 ;


using TX_slave  = mcu::PA2;
using RX_slave  = mcu::PA3;
using RTS_slave = mcu::PB1;


int main()
{
   auto[do1,do2,do3,do4]
     = make_pins<mcu::PinMode::Output, DO1,DO2,DO3,DO4>();
   auto[di1,di2,di3,di4] 
      = make_pins<mcu::PinMode::Input, DI1,DI2,DI3,DI4>();
   
   struct Flash_data {
      uint16_t factory_number = 0;
      UART::Settings uart_set = {
         .parity_enable  = false,
         .parity         = USART::Parity::even,
         .data_bits      = USART::DataBits::_8,
         .stop_bits      = USART::StopBits::_1,
         .baudrate       = USART::Baudrate::BR9600,
         .res            = 0
      };
      uint8_t  modbus_address = 1;
   } flash;
   
   struct Sense {
      bool DI1  :1;  // Bit 0
      bool DI2  :1;  // Bit 1
      bool DI3  :1;  // Bit 2
      bool DI4  :1;  // Bit 3 
      uint16_t  :12; // Bits 15:10 res: Reserved, must be kept cleared
   };

   struct Control {
      bool DO1  :1;  // Bit 0
      bool DO2  :1;  // Bit 1
      bool DO3  :1;  // Bit 2
      bool DO4  :1;  // Bit 3 
      uint16_t  :12; // Bits 15:8 res: Reserved, must be kept cleared
   };
   
   
   struct In_regs {
   
      UART::Settings uart_set;   // 0
      uint16_t modbus_address;   // 1
      uint16_t password;         // 2
      uint16_t factory_number;   // 3
      Control   output;          // 4 
   
   }__attribute__((packed));


   struct Out_regs {
   
      uint16_t device_code;      // 0
      uint16_t factory_number;   // 1
      UART::Settings uart_set;   // 2
      uint16_t modbus_address;   // 3
      Sense    input;            // 4
      uint16_t qty_input;        // 5
      uint16_t qty_output;       // 6

   }__attribute__((packed));

   auto& slave = Modbus_slave<In_regs, Out_regs>::
      make<mcu::Periph::USART1, TX_slave, RX_slave, RTS_slave>
         (flash.modbus_address, flash.uart_set);


   slave.outRegs.device_code        = 101; // test_version
   slave.outRegs.factory_number     = flash.factory_number;
   slave.outRegs.uart_set           = flash.uart_set;
   slave.outRegs.modbus_address     = flash.modbus_address;
   slave.outRegs.qty_input          = 4;
   slave.outRegs.qty_output         = 4;

   while(1){

      slave ([&](uint16_t registrAddress) {
            static bool unblock = false;
         switch (registrAddress) {
            case ADR(uart_set):
               flash.uart_set
                  = slave.outRegs.uart_set
                  = slave.inRegs.uart_set;
            break;
            case ADR(modbus_address):
               flash.modbus_address 
                  = slave.outRegs.modbus_address
                  = slave.inRegs.modbus_address;
            break;
            case ADR(password):
               unblock = slave.inRegs.password == 208;
            break;
            case ADR(factory_number):
               if (unblock) {
                  unblock = false;
                  flash.factory_number 
                     = slave.outRegs.factory_number
                     = slave.inRegs.factory_number;
               }
               unblock = true;
            break;
            case ADR(output):
               do1 = slave.inRegs.output.DO1;
               do2 = slave.inRegs.output.DO2;
               do3 = slave.inRegs.output.DO3;
               do4 = slave.inRegs.output.DO4;
            break;
         }

      });

      slave.outRegs.input.DI1 = di1;
      slave.outRegs.input.DI2 = di2;
      slave.outRegs.input.DI3 = di3;
      slave.outRegs.input.DI4 = di4;

      __WFI();

   } //while
}