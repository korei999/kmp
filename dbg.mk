SAFE_STACK :=
SAFE_STACK += -mshstk -fstack-protector-all 

ASAN :=
ASAN += -fsanitize=address

DEBUG :=
DEBUG += -g

WNO := 
WNO += -Wno-unused-parameter
WNO += -Wno-unused-variable
