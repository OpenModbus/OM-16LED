################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Libs/libModbus/src/modbus_crc16.c \
../Libs/libModbus/src/modbus_slave.c \
../Libs/libModbus/src/modbus_slave_handlers.c 

OBJS += \
./Libs/libModbus/src/modbus_crc16.o \
./Libs/libModbus/src/modbus_slave.o \
./Libs/libModbus/src/modbus_slave_handlers.o 

C_DEPS += \
./Libs/libModbus/src/modbus_crc16.d \
./Libs/libModbus/src/modbus_slave.d \
./Libs/libModbus/src/modbus_slave_handlers.d 


# Each subdirectory must supply rules for building sources it contributes
Libs/libModbus/src/%.o Libs/libModbus/src/%.su Libs/libModbus/src/%.cyclo: ../Libs/libModbus/src/%.c Libs/libModbus/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DMODBUS_DOUBLE_BUFFER -DUSE_HAL_DRIVER -DSTM32G474xx -c -I../Core/Inc -IC:/Users/Sebastian/STM32Cube/Repository/STM32Cube_FW_G4_V1.6.2/Drivers/STM32G4xx_HAL_Driver/Inc -IC:/Users/Sebastian/STM32Cube/Repository/STM32Cube_FW_G4_V1.6.2/Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -IC:/Users/Sebastian/STM32Cube/Repository/STM32Cube_FW_G4_V1.6.2/Drivers/CMSIS/Device/ST/STM32G4xx/Include -IC:/Users/Sebastian/STM32Cube/Repository/STM32Cube_FW_G4_V1.6.2/Drivers/CMSIS/Include -I"C:/Users/Sebastian/Git/OM-16LED/stm32/Libs/libConfig/src" -I"C:/Users/Sebastian/Git/OM-16LED/stm32/Libs/libModbus/src" -I"C:/Users/Sebastian/Git/OM-16LED/stm32/Libs/libBrightness/src" -I"C:/Users/Sebastian/Git/OM-16LED/stm32/Libs/libBrightness" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Libs-2f-libModbus-2f-src

clean-Libs-2f-libModbus-2f-src:
	-$(RM) ./Libs/libModbus/src/modbus_crc16.cyclo ./Libs/libModbus/src/modbus_crc16.d ./Libs/libModbus/src/modbus_crc16.o ./Libs/libModbus/src/modbus_crc16.su ./Libs/libModbus/src/modbus_slave.cyclo ./Libs/libModbus/src/modbus_slave.d ./Libs/libModbus/src/modbus_slave.o ./Libs/libModbus/src/modbus_slave.su ./Libs/libModbus/src/modbus_slave_handlers.cyclo ./Libs/libModbus/src/modbus_slave_handlers.d ./Libs/libModbus/src/modbus_slave_handlers.o ./Libs/libModbus/src/modbus_slave_handlers.su

.PHONY: clean-Libs-2f-libModbus-2f-src

