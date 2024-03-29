SAFE_STACK :=
SAFE_STACK += -mshstk -fstack-protector-all 

ASAN :=
ASAN += #-fsanitize=address

WAVE := -DDEBUG_WAVE

DEBUG :=
DEBUG += -g
DEBUG += -DDEBUG 
DEBUG += $(WAVE)

WNO := 
WNO += -Wno-unused-parameter
WNO += -Wno-unused-variable
