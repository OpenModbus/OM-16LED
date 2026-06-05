################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Libs/libConfig/src/config.c 

OBJS += \
./Libs/libConfig/src/config.o 

C_DEPS += \
./Libs/libConfig/src/config.d 


# Each subdirectory must supply rules for building sources it contributes
Libs/libConfig/src/%.o Libs/libConfig/src/%.su Libs/libConfig/src/%.cyclo: ../Libs/libConfig/src/%.c Libs/libConfig/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -DUSE_HAL_DRIVER -DMODBUS_DOUBLE_BUFFER -DSTM32G474xx -c -I../Core/Inc -IC:/Users/Sebastian/STM32Cube/Repository/STM32Cube_FW_G4_V1.6.2/Drivers/STM32G4xx_HAL_Driver/Inc -IC:/Users/Sebastian/STM32Cube/Repository/STM32Cube_FW_G4_V1.6.2/Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -IC:/Users/Sebastian/STM32Cube/Repository/STM32Cube_FW_G4_V1.6.2/Drivers/CMSIS/Device/ST/STM32G4xx/Include -IC:/Users/Sebastian/STM32Cube/Repository/STM32Cube_FW_G4_V1.6.2/Drivers/CMSIS/Include -I"C:/Users/Sebastian/Git/OM-16LED/stm32/Libs/libConfig/src" -I"C:/Users/Sebastian/Git/OM-16LED/stm32/Libs/libModbus/src" -I"C:/Users/Sebastian/Git/OM-16LED/stm32/Libs/libBrightness/src" -I"C:/Users/Sebastian/Git/OM-16LED/stm32/Libs/libBrightness" -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Libs-2f-libConfig-2f-src

clean-Libs-2f-libConfig-2f-src:
	-$(RM) ./Libs/libConfig/src/config.cyclo ./Libs/libConfig/src/config.d ./Libs/libConfig/src/config.o ./Libs/libConfig/src/config.su

.PHONY: clean-Libs-2f-libConfig-2f-src

