################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (10.3-2021.10)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/drumidy/drumidy.c 

OBJS += \
./Drivers/drumidy/drumidy.o 

C_DEPS += \
./Drivers/drumidy/drumidy.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/drumidy/%.o Drivers/drumidy/%.su: ../Drivers/drumidy/%.c Drivers/drumidy/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DSTM32G431xx -DUSE_HAL_DRIVER -DDEBUG -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../USB_Device/App -I../USB_Device/Target -I../Middlewares/ST/STM32_USB_Device_Library/Core/Inc -I../Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc -I"A:/stm32/workspace/Drumidy/Drivers/drumidy" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-drumidy

clean-Drivers-2f-drumidy:
	-$(RM) ./Drivers/drumidy/drumidy.d ./Drivers/drumidy/drumidy.o ./Drivers/drumidy/drumidy.su

.PHONY: clean-Drivers-2f-drumidy

